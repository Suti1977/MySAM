//------------------------------------------------------------------------------
//  I2C Master driver
//
//    File: MyI2CM.c
//------------------------------------------------------------------------------
//Stuff:
// https://www.avrfreaks.net/sites/default/files/forum_attachments/Atmel-42631-SAM-D21-SERCOM-I2C-Configura.pdf
#include "MyI2CM.h"
#include <string.h>
#include "compiler.h"
#include "MyHelpers.h"
#include <stdio.h>

//#include "I2C.h"                //TODO: torolni!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//#include "board.h"              //TODO: torolni!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
//#define TESTOUT0  PIN__LED_GREENn
//#define TESTOUT1  PIN__LED_REDn

//Ha ennyi ido alatt sem sikerul egy I2C folyamatot vegrehajtani, akkor hibaval
//kilep.    [TICK]
#ifndef I2C_TRANSFER_TIMEOUT
#define I2C_TRANSFER_TIMEOUT    1000
#endif

static void MyI2CM_initI2CPeri(MyI2CM_t* i2cm, const MyI2CM_Config_t* config);
static void MyI2CM_sync(SercomI2cm* hw);
//------------------------------------------------------------------------------
//I2C master modul inicializalasa
void MyI2CM_init(MyI2CM_t* i2cm, const MyI2CM_Config_t* config)
{
    ASSERT(i2cm);
    ASSERT(config);

    //Modul valtozoinak kezdeti torlese.
    memset(i2cm, 0, sizeof(MyI2CM_t));

  #ifdef USE_FREERTOS
    //Az egyideju busz hozzaferest tobb taszk kozott kizaro mutex letrehozasa
    i2cm->busMutex=xSemaphoreCreateMutex();
    ASSERT(i2cm->busMutex);
    //Az intarrupt alat futo folyamatok vegere varo szemafor letrehozasa
    i2cm->semaphore=xSemaphoreCreateBinary();
    ASSERT(i2cm->semaphore);
  #endif //USE_FREERTOS

    //Alap sercom periferia driver initje.
    //Letrejon a sercom leiro, beallitja es engedelyezi a Sercom orajeleket
    MySercom_init(&i2cm->sercom, &config->sercomCfg);

    //Sercom beallitasa I2C interfacenek megfeleloen, a kapott config alapjan.
    MyI2CM_initI2CPeri(i2cm, config);
}
//------------------------------------------------------------------------------
//I2C interfacehez tartozo sercom felkonfiguralasa
static void MyI2CM_initI2CPeri(MyI2CM_t* i2cm, const MyI2CM_Config_t* config)
{
    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;

    //Periferia resetelese
    hw->CTRLA.reg=SERCOM_I2CM_CTRLA_SWRST;
    MyI2CM_sync(hw);

    hw->CTRLA.reg=

    //A konfiguraciokor megadott attributum mezok alapjan a periferia mukodese-
    //nek beallitasa.
    //Az attributumok bitmaszkja igazodik a CTRLA regiszter bit pozicioihoz,
    //igy a beallitasok nagy resze konnyen elvegezheto. A nem a CTRLA regisz-
    //terre vonatkozo bitek egy maszkolassal kerulnek eltuntetesre.    
    hw->CTRLA.reg =
            //Sercom uzemmod beallitas (0x05-->I2C master)
            SERCOM_I2CM_CTRLA_MODE(0x05) |
            (config->attribs & MYI2CM_CTRLA_CONFIG_MASK) |
            //SERCOM_I2CM_CTRLA_LOWTOUTEN |
            //SERCOM_I2CM_CTRLA_INACTOUT(0x3) |
            SERCOM_I2CM_CTRLA_SDAHOLD(3) |
            0;
    MyI2CM_sync(hw);

    hw->CTRLB.reg =
            //Smart uzemmod beallitasa. (Kevesebb szoftveres interrupcio kell)
            //Ez a driver a SMART modra epitkezik. Az ACK-kat automatikusan
            //generalja a periferia.
            SERCOM_I2CM_CTRLB_SMEN |
            0;
    MyI2CM_sync(hw);

    //Adatatviteli sebesseg beallitasa
    MyI2CM_setFrequency(i2cm, config->busFreq);

    //Minden interrupt tiltasa a periferian...
    hw->INTENCLR.reg=SERCOM_I2CM_INTENCLR_MASK;

    //Sercom-hoz tartozo interruptok engedelyezese az NVIC-ben.
    //Feltetelezzuk, hogy egy SERCOM-hoz tartozo interruptok egymas utan
    //sorban kovetkeznek, es egy sercom-hoz 4 darab tartozik.
    MySercom_enableIrqs(&i2cm->sercom);

    //I2C periferia engedelyezese. (Ez utan mar bizonyos config bitek nem
    //modosithatok!)
    hw->CTRLA.bit.ENABLE=1;
    MyI2CM_sync(hw);

    //A periferiat "UNKNOWN" allapotbol ki kell mozditani, mert addig nem tudunk
    //erdemben mukodni. Ez megteheto ugy, ha irjuk a statusz regiszterenek
    //a BUSSTATE mezojet.
    //Ekkor IDLE modba kerulunk.
    hw->STATUS.reg=SERCOM_I2CM_STATUS_BUSSTATE(1);
    MyI2CM_sync(hw);
}
//------------------------------------------------------------------------------
//I2C busz sebesseg modositasa/beallitasa
status_t MyI2CM_setFrequency(MyI2CM_t* i2cm, uint32_t freq)
{
    //Szaturalas maximum frekvenciara, mely a core orajel fele lehet.
    uint32_t maxFreq = (i2cm->sercom.coreFreq >> 1);
    if (freq > maxFreq)
    {
        freq=maxFreq;
    }

    //Baud szamitasa
    uint32_t baud = ((i2cm->sercom.coreFreq * 10) / (24 * freq)) - 4;
    uint32_t baud_hs =0;
    if (baud > 255)
    {
        baud = 255;
    }
    else
    {
        baud_hs = ((i2cm->sercom.coreFreq * 10) / (478 * freq)) - 1;
        if (baud_hs > 255)
        {
            baud_hs = 255;
        }
    }

    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;

    //Ha be van kapcsolva a sercom, akkor azt elobb tiltani kell.
    if (hw->CTRLA.bit.ENABLE)
    {   //A periferia be van kapcsolva. Azt elobb tiltani kell.
        hw->CTRLA.bit.ENABLE=0;
        MyI2CM_sync(hw);

        //baud beallitasa
        hw->BAUD.reg= ((baud_hs << 16) | baud);

        //I2C periferia engedelyezese. (Ez utan mar bizonyos config bitek nem
        //modosithatok!)
        hw->CTRLA.bit.ENABLE=1;
        MyI2CM_sync(hw);

        //A periferiat "UNKNOWN" allapotbol ki kell mozditani, mert addig nem tudunk
        //erdemben mukodni. Ez megteheto ugy, ha irjuk a statusz regiszterenek
        //a BUSSTATE mezojet.
        //Ekkor IDLE modba kerulunk.
        hw->STATUS.reg=SERCOM_I2CM_STATUS_BUSSTATE(1);
        MyI2CM_sync(hw);
    }
    else
    {
        //baud beallitasa
        hw->BAUD.reg= ((baud_hs << 16) | baud);
    }

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//I2C periferia szinkronizacio. Addig var, amig barmely busy bit is 1.
static void MyI2CM_sync(SercomI2cm* hw)
{
    while((hw->SYNCBUSY.reg & SERCOM_I2CM_SYNCBUSY_MASK) !=0 );
}
//------------------------------------------------------------------------------
//Stop feltetel generalasa a buszon
#if 0
static void MyI2CM_sendStop(SercomI2cm* hw)
{
    //for(int i=0; i<300; i++) __NOP();

    //0x03 irasa a parancs regiszterbe STOP-ot general az eszkoz.
    uint32_t regVal;
    regVal = hw->CTRLB.reg;
    regVal &= ~SERCOM_I2CM_CTRLB_CMD_Msk;
    regVal |= SERCOM_I2CM_CTRLB_CMD(0x03);
if (hw==&i2c.pmBus.driver.sercom.hw->I2CM)
{
    MyGPIO_set(TESTOUT0);
}
    hw->CTRLB.reg =regVal;
    MyI2CM_sync(hw);
if (hw==&i2c.pmBus.driver.sercom.hw->I2CM) MyGPIO_clr(TESTOUT0);
}
#endif
//------------------------------------------------------------------------------
//I2C folyamatok vegen hivodo rutin.
//Ebben tortenik meg a buszciklust lezaro stop feltetel kiadas, majd jelzes
//az applikacio fele, hogy vegzett az interruptban futo folyamat.
static void MyI2CM_end(MyI2CM_t* i2cm)
{
    //Minden tovabbi megszakitast letiltunk
    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;
    hw->INTENCLR.reg = SERCOM_I2CM_INTENCLR_MASK;

    //Jelzes az applikacionak
  #ifdef USE_FREERTOS
    portBASE_TYPE higherPriorityTaskWoken=pdFALSE;
    xSemaphoreGiveFromISR(i2cm->semaphore, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken)
  #endif //USE_FREERTOS
}
//------------------------------------------------------------------------------
//Kovetkezo vegrehajtando transzfer blokk keresese.
//A jelenleg futo blokk iranya ha nem valtozik, viszont a soron kovetkezo leiro
//0 hosszusagot mutat, akkor az a leiro kihagyasra kerul.
#if 0
static void MyI2CM_selectNextXferBlock(MyI2CM_t* i2cm,
                                       const MyI2CM_xfer_t* firstBlock)
{
    //Soron kovetkezo blokk leiro kerese, melynek van tartalma, vagy amelyik
    //masik iranyba megy, mint az utoljara vegrehajtott...
    //Addig lepked,amig olyan leirora nem talal, melyben van valos tartalom,
    //vagy az iranya valtozott a legutoljara kuldotthoz kepest.

    //kezdetben a kovetkezo block ismeretlen
    const MyI2CM_xfer_t* nextBlock;

    if (firstBlock)
    {   //Ez az elso vegrehajtando block.


        //Az elso blokk vegrehajtasaval kezdunk
        i2cm->actualBlock=firstBlock;
        nextBlock=firstBlock;

        if (i2cm->leftBlockCnt) i2cm->leftBlockCnt--;

    } else
    {   //ez nem az elso leiro az atvitel soran

        //Az elozoleg beallitott lesz most vegrehajtva, es a kovetkezo keresese
        //is attol fog folytatodni.
        nextBlock=
        i2cm->actualBlock=i2cm->nextBlock;
    }

    //A kereses elott ismeretlen lesz a kovetkezo blokk. Ez csak akkor kaphat
    //ujra erteket, ha talalt vegrehajthatot a listaban.
    i2cm->nextBlock=NULL;
    //Aalapertelmezesben azt jelezzuk, hogy egy iranyhoz tartozo transzfernek
    //vege van. Ha megis folytatni kelelne akkor felul lesz biralva.
    i2cm->last=true;


    while(i2cm->leftBlockCnt)
    {
        nextBlock++;
        i2cm->leftBlockCnt--;

        if (nextBlock->dir == i2cm->actualBlock->dir)
        {   //A vizsgalt blokknak ugyan az az iranya, mint amit vegrehajtunk
            if (nextBlock->length == 0)
            {   //...viszont nincsen hossza. Ebbbn az esetben ezt a leirot
                //ki kell hagyni. ugrunk a kovetkezore.
                continue;
            } else
            {   //ugyan az az irany, viszont van tartalom.
                //Folytatni kell a megkezdett muveletet.
                i2cm->last=false;
            }
        }

        //A vizsgalt leirot hasznalni kell.
        i2cm->nextBlock=nextBlock;
        break;
    } //while(i2cm->leftBlockCnt)

    //<--ide ugy jut, hogy az i2cm->nextBlock a kovetkezo leiron all, vagy
    //   NULL, ha nincs mas leiro.

    if (i2cm->actualBlock==NULL)
    {   //GEBA.
        ASSERT(NULL);
    }

    //Az aktualis block kijelolese
    const MyI2CM_xfer_t* block=i2cm->actualBlock;
    i2cm->transferDir=block->dir;
    i2cm->leftByteCnt=block->length;
    if (block->length)
    {   //Van adattartalom
        i2cm->dataPtr=(uint8_t*) block->buffer;
    }
    else
    {   //nincs adattartalom
        i2cm->dataPtr=NULL;
    }
}
//------------------------------------------------------------------------------
//Kovetkezo adatblokk kuldesenek inditasa.
static void MyI2CM_startNextXferBlock(MyI2CM_t* i2cm, bool first)
{
    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;

    if (first)
    {   //Ez az elso adatblokk. Evvel indul az I2C folyamat

        i2cm->nextBlock=NULL;

        if (i2cm->leftBlockCnt)
        {   //Van transzfer block leiro.
            //Az elso akar TX, akar RX, vegre kell hajtani, ha van elemszama,
            //ha nincs.

            //legelso blokknak megfelelo beallitasok elvegzese, illetve az
            //azt koveto blokk megkeresese.
            //A rutin beallitja a "last" flaget annak megfeleloen, hogy egy
            //iranyhoz tartozolag ez-e az utolso vegrehajtando atvitel.
            MyI2CM_selectNextXferBlock(i2cm, i2cm->actualBlock);

        } else
        {   //Nincs transzfer blokk leiro. Ez csak egy eszkoz pingeles lesz.
            //Alapertelmezesben Write-ot kuldunk.
            i2cm->transferDir=MYI2CM_DIR_TX;
            i2cm->leftByteCnt=0;
        }

        if (i2cm->transferDir==MYI2CM_DIR_RX)
        {
            //Elore csokkentjuk a beolavsando byteok szamat, mivel a start
            //kiadasa azonnal egy olvasast is general majd.
            if (i2cm->leftByteCnt)
            {
                i2cm->leftByteCnt--;
            }

            //olvasas eseten az ACK/NACK-t be kell allitani, ha csak 1 byte lenne,
            //mivel annak olvasasa a cim utan automatikusan meg fog tortenni.
            //De mindez csak akkor ervenyes, ha ez az utolso transzfer az adott
            //iranynak megfeleloen.
            uint32_t regVal=hw->CTRLB.reg;
            if ((i2cm->leftByteCnt==0) && (i2cm->last))
            {   //NACK-t kell adni.                
                regVal |=SERCOM_I2CM_CTRLB_ACKACT;
            } else
            {   //ACK-t adunk a tovabbi byteokra.                
                regVal &=~SERCOM_I2CM_CTRLB_ACKACT;
            }
            hw->CTRLB.reg=regVal;
            MyI2CM_sync(hw);
        }

        hw->INTENCLR.reg=   SERCOM_I2CM_INTENCLR_ERROR |
                            SERCOM_I2CM_INTENCLR_MB |
                            SERCOM_I2CM_INTENCLR_SB;

        //Elso startfeltetel generalasa...
        //A start feltetel az iranytol fugg.
        uint32_t regVal=(i2cm->slaveAddress | i2cm->transferDir);        
        hw->ADDR.reg=regVal;
        MyI2CM_sync(hw);
        //Megszakitasok engedelyezese
        hw->INTENSET.reg=   SERCOM_I2CM_INTENSET_ERROR |
                            SERCOM_I2CM_INTENSET_MB |
                            SERCOM_I2CM_INTENSET_SB;


        //VIGYAZAT! Ez utan feljohet IT azonnal, pl arbitacio vesztes miatt!
        //Folytatas majd az interrupt rutinban...
        return;
    }

    //<-- I2C folyamat kozben hivva. Uj transzfer blokk vegrehajtasba kezdes...

    //RX eseten az utoljara fogadott adatbyte a DATA regiszterben van.
    //Azt a STOP vagy restart utan kell kiolvasni, kulonben ha elotte
    //tesszuk, akkor elinditana egy ujabb adat vetelt.

    if (i2cm->nextBlock==NULL)
    {   //nincs tobb block.

        //Stop feltetel generalasa
        MyI2CM_sendStop(hw);

        //RX eseten az utolso byte-ot most lehet kiolvasni.
        if (i2cm->transferDir==MYI2CM_DIR_RX)
        {                        
            if (i2cm->dataPtr)
            {   //Van buffer az adatoknak
                *i2cm->dataPtr=hw->DATA.reg & 0xff;
            } else
            {   //nincs buffer. Csak olvassuk a regisztert.
                uint8_t dummyRead=hw->DATA.reg & 0xff;
                (void)dummyRead;
            }
            MyI2CM_sync(hw);
        }


        //jelzes az applikacionak, hogy kesz a leiroval
        MyI2CM_end(i2cm);

    } else
    {   //Van meg hatra block.

        //A jelenlegi iranynak megfelelo folyamatotot kell folytatni?
        if (i2cm->last==false)
        {   //muvelet folytatasa, csak az uj blokkal

            //Soron kovetkezo blokknak megfelelo beallitasok elvegzese, illetve
            //az azt koveto blokk megkeresese.
            //A rutin beallitja a "last" flaget annak megfeleloen, hogy egy
            //iranyhoz tartozolag ez-e az utolso vegrehajtando atvitel.
            MyI2CM_selectNextXferBlock(i2cm, NULL);

        } else
        {   //Uj irany

            //Megjegyezzuk az olvaso pointert, hogy az utolso byteot ki tudjuk
            //majd irni a bufferbe...
            //Ha a stop utan nem kell olvasni, akkor erteke NULL lesz.
            uint8_t* savedReadPtr;
            if (i2cm->transferDir==MYI2CM_DIR_RX)
            {
                savedReadPtr=i2cm->dataPtr;
            } else
            {
                savedReadPtr=NULL;
            }

            //Soron kovetkezo blokknak megfelelo beallitasok elvegzese, illetve
            //az azt koveto blokk megkeresese.
            //A rutin beallitja a "last" flaget annak megfeleloen, hogy egy
            //iranyhoz tartozolag ez-e az utolso vegrehajtando atvitel.
            MyI2CM_selectNextXferBlock(i2cm, NULL);

            //Folytatjuk a mukodest az uj  blokk, uj iranya szerint...

            if (i2cm->transferDir==MYI2CM_DIR_RX)
            {   //Az uj blokk iranya olvasas. (Elotte iras volt.)

                //Elore csokkentjuka  beolavsando byteok szamat, mivel a start
                //kiadasa azonnal egy olvasast is general majd.
                if (i2cm->leftByteCnt)
                {
                    i2cm->leftByteCnt--;
                }

                //olvasas eseten az ACK/NACK-t be kell allitani, ha csak 1 byte
                //lenne, mivel annak olvasasa a cim utan automatikusan meg fog
                //tortenni.
                //De mindez csak akkor ervenyes, ha ez az utolso transzfer az
                //adott iranynak megfeleloen.
                uint32_t regVal=hw->CTRLB.reg;
                if ((i2cm->leftByteCnt==0) && (i2cm->last))
                {   //NACK-t kell adni.
                    regVal |= SERCOM_I2CM_CTRLB_ACKACT;
                } else
                {   //ACK-t adunk a tovabbi byteokra.
                    regVal &= ~SERCOM_I2CM_CTRLB_ACKACT;
                }
                hw->CTRLB.reg=regVal;
                MyI2CM_sync(hw);

            } else
            {   //az uj blokk iranya iras.

                //Stop feltetel generalasa
                MyI2CM_sendStop(hw);

                if (savedReadPtr)
                {   //A korabbi blokkban olvasas volt, melynek az utolso
                    //eleme meg a DATA regiszterben varakozik.
                    //Ezt csak a stop feltetel utan olvashatjuk ki.

                    //utolso byte mentese a bufferbe
                    *savedReadPtr=hw->DATA.reg & 0xff;
                    MyI2CM_sync(hw);
                    savedReadPtr=NULL;
                }
            }


            //Start/restart feltetel generalasa...
            //A start feltetel az iranytol fugg.
            hw->ADDR.reg=(i2cm->slaveAddress | i2cm->transferDir);
            MyI2CM_sync(hw);

        }
    } //if (i2cm->nextBlock==NULL) else
}
#endif
//------------------------------------------------------------------------------
//Az I2C interfacehez tartozo sercom interrupt service rutinokbol hivando.
//Parameterkent at kell adni a kezelt interface leirojat.
//[INTERRUPTBAN FUT]
#if 0
void MyI2CM_service(MyI2CM_t* i2cm)
{
    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;
    //volatile SERCOM_I2CM_STATUS_Type    status;
    //Statusz regiszter kiolvasasa. A tovabbiakban ezt hasznaljuk az elemzeshez
    //status.reg=hw->STATUS.reg;

if (hw==&i2c.pmBus.driver.sercom.hw->I2CM) MyGPIO_set(TESTOUT1);

    if (hw->STATUS.bit.ARBLOST)
    {   //Elvesztettuk az arbitaciot a buszon, vagy mi birtokoljuk ugyan a
        //buszt, de azon valami hibat detektalt a periferia

        if (hw->STATUS.bit.BUSERR)
        {   //Busz hiba volt
            i2cm->asyncStatus=kMyI2CMStatus_BusError;
        } else
        {   //Arbitacios hiba volt.
            i2cm->asyncStatus=kMyI2CMStatus_ArbitationLost;
        }

        hw->INTFLAG.reg=SERCOM_I2CM_INTFLAG_ERROR;

        goto error;
    }
    //..........................................................................
    if (hw->INTFLAG.bit.ERROR)
    {   //Valam hiba volt a buszon
        //TODO: kezelni a hibat
        //A hibakat a status register irja le.

        //Toroljuk a hiba IT jelzest
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        hw->INTFLAG.reg=SERCOM_I2CM_INTFLAG_ERROR;
    }
    //..........................................................................

    if (hw->INTFLAG.bit.MB)
    {   //A masterhez tartozo IT-t kaptunk
        //A buszon arbitaltunk vagy irtunk, es egy adatbyte/cim kiirasa
        //befejezodott, vagy busz/arbitacios hiba miatt meghiusult.


        //if (status.bit.ARBLOST)
        //{   //Elvesztettuk az arbitaciot a buszon, vagy mi birtokoljuk ugyan a
        //    //buszt, de azon valami hibat detektalt a periferia.
        //
        //    //IT flag torlese.
        //    hw->INTFLAG.reg=SERCOM_I2CM_INTFLAG_MB;
        //
        //    if (status.bit.BUSERR)
        //    {   //Busz hiba volt
        //        i2cm->asyncStatus=kMyI2CMStatus_BusError;
        //    } else
        //    {   //Arbitacios hiba volt.
        //        i2cm->asyncStatus=kMyI2CMStatus_ArbitationLost;
        //    }
        //    goto error;
        //}

        //Normal esetben minden iras eseten kell, hogy kapjunk ACK-t.
        //Ha az elmarad, akkor a slave nem vette az uzenetunket.
        //De ha egy slave foglalt, akkor is adhat NACk-t!
        if (hw->STATUS.bit.RXNACK==1)
        {   //A slave nem adott ACK-t. (Nem letezik, vagy foglalt.)

            if (i2cm->busyTest==false)
            {
                //Normal mukodes eseten ez hiba.
                i2cm->asyncStatus=kMyI2CMStatus_NACK;
                goto error;
            } else
            {   //A slave NACK-jat nem szabad hibanak venni, mert foglalt.
                //Ujra fog pingelni...
                //Az uj ping kerelem a MyI2CM_startNextXferBlock()-ban indul.
                ASSERT(0);
            }
        }

        //<--a slave ack-zott, vagy NACK volt, de foglaltsag veget varjuk.
        //Adatok kuldese amig van hatra...

        if (i2cm->leftByteCnt)
        {   //van meg mit kuldeni a bufferbol. A soron kovetkezo adatbyte
            //kuldesenek inditasa, es egyben az uj elem kijelolese...
            hw->DATA.reg = *i2cm->dataPtr++;
            MyI2CM_sync(hw);
            //Hatralevo byteok szama csokken
            i2cm->leftByteCnt--;

        } else
        {   //Nincs mar mit kuldeni. Ez volt az utolso adatbyte a blokkban.
            //Uj blokk nyitasa, ha van meg, vagy a transzfer lezarasa.
            //A hivott rutin vagy restart feltetelt general, vagy a buszra
            //helyezi a kovetkezo adatbyteot, vagy jelez az applikacionak,
            //hogy kesz.
            //Abban az esetben, ha varni kell az eszkozre, mert az foglalt,
            //ugy egy pingeles indul.

            if (i2cm->busyTest)
            {   //Foglaltsagi teszt van.

                //ACK-ra varunk, mely ha megjon, akkor mar nem foglalt a slave
                if (hw->STATUS.bit.RXNACK==0)
                {   //A slave mar nem foglalt. Megvan a vart ACK.
                    //Tovabbi varakozasi folyamat lezarasa
                    i2cm->busyTest=false;
                    i2cm->waitingWhileBusy=false;
                }
            }

            MyI2CM_startNextXferBlock(i2cm, 0);
        }

    } //if (Hw->INTFLAG.bit.MB)
    //..........................................................................
    if (hw->INTFLAG.bit.SB)
    {   //A slavehez tartozo IT-t kaptunk. Ez akkor jon fel, ha a slavetol
        //beerkezett egy adatbyte.       
        //(RX eseten mindenkepen olvas az MCU egy byteot a START es CIM utan.)

        if ((i2cm->last) && (i2cm->leftByteCnt<=1))
        {   //Az utolso adatbyte olvasasa fog indulni, ha a DATA regisztert
            //kiolvassuk. Ezert meg elotte be kell allitani, hogy NACK-t adjon
            //majd arra a periferia.
            uint32_t regVal;
            regVal=hw->CTRLB.reg;
            regVal |= SERCOM_I2CM_CTRLB_ACKACT;
            hw->CTRLB.reg=regVal;
            MyI2CM_sync(hw);
        }

        if (i2cm->leftByteCnt==0)
        {   //a transzfer blokk minden eleme beerkezett. Az utolso viszont a
            //DATA regiszterben varakozik. Ha azt kiolvasnank, akkor a SMART
            //mod miatt egy ujabb atvitel indulna.

            //Uj blokkra allas.
            MyI2CM_startNextXferBlock(i2cm, false);

        } else
        {   //A beolvasott adatbyteot a helyere kell tenni

            //beerkezett adatbyte olvasasa a periferiarol. A SMART mode miatt
            //azonanl elindul a kovetkezo adatbyte olvasasa is.
            *i2cm->dataPtr++ = (uint8_t) hw->DATA.reg;
            MyI2CM_sync(hw);

            //Hatralevo byteok szamanak csokkenetese...
            i2cm->leftByteCnt--;
        }

    } //if (Hw->INTFLAG.bit.SB)
    goto exit;

    //..........................................................................
error:
    //<--ide ugrunk hiba eseten.
    //A hibat a korabban beallitott ->AsyncStatus valtozo orzi
    __NOP();
    __NOP();
    __NOP();
    //Stop feltetel generalasa
    MyI2CM_sendStop(hw);

    //jelzes az applikacionak, hogy kesz a leiroval
    MyI2CM_end(i2cm);

exit:
if (hw==&i2c.pmBus.driver.sercom.hw->I2CM) MyGPIO_clr(TESTOUT1);
    return;
}
//------------------------------------------------------------------------------
#endif
//I2C eszkoz letrehozasa
//Device: Az eszkoz leiroja
//I2C: Annak a busznak az I2CM driverenek handlere, melyre az eszkoz csatlakozik
//SlaveAddress: Eszkoz I2C slave cime a buszon
//Handler: Az I2C-s eszkozhoz tartozo driver handlere
void MyI2CM_createDevice(MyI2CM_Device_t* i2cDevice,
                         MyI2CM_t* i2c,
                         uint8_t slaveAddress,
                         void* handler)
{
    i2cDevice->i2c=i2c;
    i2cDevice->slaveAddress=slaveAddress;
    i2cDevice->handler=handler;
}
//------------------------------------------------------------------------------
#if 0
//Atviteli leirok listaja alapjan mukodes inditasa
status_t MYI2CM_transfer(MyI2CM_Device_t* i2cDevice,
                        const MyI2CM_xfer_t* xferBlocks, uint32_t blockCount)
{
    status_t status;
    MyI2CM_t* i2cm=i2cDevice->i2c;

  #ifdef USE_FREERTOS
    //Interfesz lefoglalasa a hivo taszk szamara
    xSemaphoreTake(i2cm->busMutex, portMAX_DELAY);
  #endif //USE_FREERTOS

    //Arbitacio vesztes eseten ujra probalkozik. (Addig ciklus....)
    do
    {
        i2cm->asyncStatus=kStatus_Success;

        //Az elso kiirando adatblokkra allunk
        i2cm->actualBlock=xferBlocks;
        i2cm->leftBlockCnt=blockCount;

        //Megjegyezzuk a slave eszkoz cimet, mivel IT alol is kell majd
        //restartnal a cimet ismerni.
        //A 7 bits slave cimet shifteljuk 1-el balra, mert a 0. bit az R/W bit
        //helye
        i2cm->slaveAddress= (uint8_t)(i2cDevice->slaveAddress << 1);
        //Folyamat inditasa. (Jelezzuk parameterben, hogy ez az elso kuldendo
        //blokk)
        MyI2CM_startNextXferBlock(i2cm, true);

      #ifdef USE_FREERTOS
        //Varakozas arra, hogy a folyamat befejezodjon...
        if (xSemaphoreTake(i2cm->semaphore, I2C_TRANSFER_TIMEOUT)==pdFALSE)
        {   //Hiba a buszon.

            //Minden interrupt tiltasa a periferian...
            SercomI2cm* hw=&i2cm->sercom.hw->I2CM;
            hw->INTENCLR.reg=   SERCOM_I2CM_INTENCLR_ERROR |
                                SERCOM_I2CM_INTENCLR_MB |
                                SERCOM_I2CM_INTENCLR_SB;

            printf("I2C bus error.\n");

            //TODO: itt stopot generalni, vagy valahogy feloldani a hibat!      !!!!

            status=kMyI2CMStatus_BusError;
        } else
        {
            //Minden interrupt tiltasa a periferian...
            SercomI2cm* hw=&i2cm->sercom.hw->I2CM;
            hw->INTENCLR.reg=   SERCOM_I2CM_INTENCLR_ERROR |
                                SERCOM_I2CM_INTENCLR_MB |
                                SERCOM_I2CM_INTENCLR_SB;

            status=i2cm->asyncStatus;
        }
      #endif //USE_FREERTOS
        if (status==kMyI2CMStatus_ArbitationLost)
        {   //Arbitacio veszetes. Ujra probalkozik...
            printf("I2C arbitation lost.\n");
            continue;
        }

        //Ha nincs hiba, akkor kilepes...
        //if (Status==kStatus_Success) break;
        break;

    } while(1);
    //vTaskDelay(1);

    //Interfeszt fogo mutex feloldasa
  #ifdef USE_FREERTOS
    xSemaphoreGive(i2cm->busMutex);
  #endif //USE_FREERTOS

    return status;
}
#endif
//------------------------------------------------------------------------------
//Eszkoz elerhetoseg tesztelese. Nincs adattartalom.
status_t MyI2CM_ackTest(MyI2CM_Device_t* i2cDevice)
{
    status_t status;

    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, NULL, 0}
    };

    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Az I2C interfacehez tartozo sercom interrupt service rutinokbol hivando.
//Parameterkent at kell adni a kezelt interface leirojat.
//[INTERRUPTBAN FUT]
void MyI2CM_service(MyI2CM_t* i2cm)
{
    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;
//if (hw==&i2c.pmBus.driver.sercom.hw->I2CM) MyGPIO_set(TESTOUT1);
//IRQ tiltas!

    //Megszakitasi flagek olvasasa.
    uint8_t intFlags=hw->INTFLAG.reg;
    //A megszakitasi flageket itt nem szabad meg torolni!
    //hw->INTFLAG.reg=intFlags;
//if (hw==&i2c.pmBus.driver.sercom.hw->I2CM) MyGPIO_clr(TESTOUT1);
    //hibak detektalasa...
    #define MYI2CM_ERROR_MASK               \
    (       0                               \
            | SERCOM_I2CM_STATUS_RXNACK     \
            | SERCOM_I2CM_STATUS_LENERR     \
            | SERCOM_I2CM_STATUS_SEXTTOUT   \
            | SERCOM_I2CM_STATUS_MEXTTOUT   \
            | SERCOM_I2CM_STATUS_LOWTOUT    \
            | SERCOM_I2CM_STATUS_BUSERR     \
    )

    if (hw->STATUS.reg & MYI2CM_ERROR_MASK)
    {   //Hiba a buszon!
        //TODO: a hiba biteket megjegyezni, es a kiertekelest a taszkban megoldani!
        if (hw->STATUS.bit.ARBLOST)
        {   //Arbitacio vesztes
            i2cm->asyncStatus=kMyI2CMStatus_ArbitationLost;
        } else
        if (hw->STATUS.bit.BUSERR)
        {   //Busz hiba
            i2cm->asyncStatus=kMyI2CMStatus_BusError;
        } else
        if (hw->STATUS.bit.RXNACK)
        {   //NACK.
            i2cm->asyncStatus=kMyI2CMStatus_NACK;
        } else
        if (hw->STATUS.bit.SEXTTOUT)
        {   //NACK.
            i2cm->asyncStatus=kMyI2CMStatus_SlaveSclLowExtendTimeout;
        } else
        if (hw->STATUS.bit.MEXTTOUT)
        {   //NACK.
            i2cm->asyncStatus=kMyI2CMStatus_MasterSclExtendTimeout;
        } else
        if (hw->STATUS.bit.LOWTOUT)
        {   //NACK.
            i2cm->asyncStatus=kMyI2CMStatus_SclLowTimeout;
        } else
        {   //A nem kezelt esetekben busz hiba
            i2cm->asyncStatus=kMyI2CMStatus_BusError;
        }


        if (hw->STATUS.reg & SERCOM_I2CM_STATUS_RXNACK)
        {   //RXNACK eseten stop feltetel generalasa
            hw->CTRLB.bit.CMD=3;
        }

        //Hibak torlese...
        hw->STATUS.reg=SERCOM_I2CM_STATUS_ARBLOST |
                      SERCOM_I2CM_STATUS_LENERR   |
                      SERCOM_I2CM_STATUS_LOWTOUT  |
                      SERCOM_I2CM_STATUS_BUSERR;

        MyI2CM_sync(hw);

        //Minden msz flag torlese
        hw->INTFLAG.reg=SERCOM_I2CM_INTFLAG_MASK;
//irq vissza!
        //Transzfer befejezese
        MyI2CM_end(i2cm);
        return;
    }

    //..........................................................................
    if (intFlags & SERCOM_I2CM_INTFLAG_MB)
    {   //A masterhez tartozo IT-t kaptunk
        if (i2cm->leftByteCnt==0)
        {   //Minden adat el lett kuldve az adott leirobol.

mb_leftByteCntZero:
            if (i2cm->leftBlockCnt==0)
            {   //Minden leiron vegigert. Transzfer lezarasa.

                //Stop feltetel
                hw->CTRLB.bit.CMD=3;
                MyI2CM_sync(hw);

                //Jelzes a taszknak. A hivott rutin tiltja a megszakitasokat is.
                MyI2CM_end(i2cm);
                return;

            } else
            {   //Uj transzfer leirora allas...
                i2cm->actualBlock++;
                const MyI2CM_xfer_t* actualBlock=i2cm->actualBlock;
                i2cm->leftByteCnt=actualBlock->length;
                i2cm->dataPtr    =actualBlock->buffer;

                //Hatralevo leirok szamanak csokkentese
                i2cm->leftBlockCnt--;

                if ((actualBlock->dir & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
                {   //RX
                    //Eddig TX volt. Most RX-re valtunk.

                    //Ez minenkeppen restratot jelent.
                    if (actualBlock->length==0)
                    {   //Hibas transzfer leiro!
                        //Olvasas eseten a transzfer hossza nem lehet 0!
                        i2cm->asyncStatus=kMyI2CMStatus_InvalidTransferDescriptor;

                        //Stop.
                        hw->CTRLB.bit.CMD=3;
                        MyI2CM_sync(hw);
                        //teljes transzfer befejezese.
                        MyI2CM_end(i2cm);
                        return;
                    }

                    //RW bit = 1 --> Read
                    uint32_t addressReg= (uint32_t)i2cm->slaveAddress | 1;
                    if (actualBlock->dir & MYI2CM_FLAG_TENBIT)
                    {   //10 bites cimzes eloirva.
                        addressReg |= SERCOM_I2CM_ADDR_TENBITEN;
                    }

                    //Alapertelmezesben ACK-t adunk minden olvasott adatbyte-ra.
                    //Az utolso elott majd NACK-ra valtunk a megszakitasban.
                    hw->CTRLB.bit.ACKACT=0;
                    MyI2CM_sync(hw);

                    //A slave msz. engedelyezese. A hiba tetektacio miatt
                    //szukseges a master msz. engedelyezese is.
                    hw->INTENSET.reg=SERCOM_I2CM_INTENSET_MB |
                                     SERCOM_I2CM_INTENSET_SB;

                    //Transzfer inditasa, az ADDR regiszter irasaval. Hatasara
                    //start/restart feltetel fog generalodni. A periferia
                    //azonnal beolvassa az elso adatbyteot is, mely utan msz
                    //fog generalodni.
                    hw->ADDR.reg=addressReg;
                    MyI2CM_sync(hw);

                    //A blokkbol hatralevo byteok szamat csokkenti 1-el, mivel
                    //a start kiadasara automatikusan beolvasodik az elso,
                    //i2cm->leftByteCnt--;
                } else
                {   //TX
                    if (i2cm->leftByteCnt==0)
                    {   //A soronlevo leiro nem tartalmaz adatokat.
                        //Ugras a kovetkezore.
                        goto mb_leftByteCntZero;
                    }

                    //A leiro szerinti elso adatbyte irasa
                    hw->DATA.reg = *i2cm->dataPtr;
                    i2cm->dataPtr++;
                    //Hatralevo byteok szama csokken
                    i2cm->leftByteCnt--;
                    MyI2CM_sync(hw);
                }
            }
        } else
        {   //Meg van kuldendo adat az aktualis transzfer leiro szerint.
            //buszra helyezi...
            hw->DATA.reg = *i2cm->dataPtr;
            i2cm->dataPtr++;
            //Hatralevo byteok szama csokken
            i2cm->leftByteCnt--;
            MyI2CM_sync(hw);
        }
    } //if (Hw->INTFLAG.bit.MB)


    //..........................................................................
    if (intFlags & SERCOM_I2CM_INTFLAG_SB)
    {   //A slavehez tartozo IT-t kaptunk. Ez akkor jon fel, ha a slavetol
        //beerkezett egy adatbyte.
        //(RX eseten mindenkepen olvas az MCU egy byteot a START es CIM utan.)
        if (i2cm->leftByteCnt==1)
        {   //Az utolso adatbyte a leiro alapjan beerkezett.
            //Ellenorizni kell, hogy ez-e az utolso leiro, vagy ha van meg
            //utanna, annak az iranya nem valtozik-e. Ha kovetkezo leiro is
            //RX, akkor egyszeruen folytatodik az olvasas. Mas esetekben
            //restart, es tx.

            if (i2cm->leftBlockCnt==0)
            {   //Minden leiron vegigert. Transzfer lezarasa.

                uint32_t regValue;
                regValue=hw->CTRLB.reg;
                regValue |= SERCOM_I2CM_CTRLB_CMD(3) | SERCOM_I2CM_CTRLB_ACKACT;
                hw->CTRLB.reg=regValue;

                //HA ITT BENT LENNE EZ A SZINKRONIZALAS, AKKOR A STOP FELTETEL
                //KIADASA MEGZAVARODNA, ES A BUSZT FOGNA A PERIFERIA.
                //EZ VALOSZINULEG AZ IP HIBAJA.
                //MyI2CM_sync(hw);

                //Az utolso adatbyte meg a DATA regiszterben van. Azt a stop
                //kiadasa utan szabad kiolvasni a SMART mod miatt.
                *i2cm->dataPtr=(uint8_t) hw->DATA.reg;
                MyI2CM_sync(hw);

                //trasnszfer lezarasa
                //Jelzes a taszknak. A hivott rutin tiltja a megszakitasokat is.
                MyI2CM_end(i2cm);
                return;

            } else
            {   //Uj transzfer leirora allas...
                i2cm->actualBlock++;
                const MyI2CM_xfer_t* actualBlock=i2cm->actualBlock;
                i2cm->leftByteCnt=actualBlock->length;

                //Hatralevo leirok szamanak csokkentese
                i2cm->leftBlockCnt--;

                if ((actualBlock->dir & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
                {   //RX
                    //Eddig is RX volt. Folytatodik a stream olvasasa...
                    i2cm->dataPtr    =actualBlock->buffer;

                    *i2cm->dataPtr=(uint8_t) hw->DATA.reg;
                    i2cm->dataPtr++;
                    //Hatralevo byteok szama csokken
                    i2cm->leftByteCnt--;
                    MyI2CM_sync(hw);

                } else
                {   //TX
                    //Az elobb RX volt, most TX-re valtunk. Ez mindenkeppen
                    //rastartot jelent.

                    uint32_t addressReg= (uint32_t)i2cm->slaveAddress;
                    if (actualBlock->dir & MYI2CM_FLAG_TENBIT)
                    {   //10 bites cimzes eloirva.
                        addressReg |= SERCOM_I2CM_ADDR_TENBITEN;
                    }

                    //NACK-val zarul az olvasasi szekvencia.
                    hw->CTRLB.bit.ACKACT=1;
                    MyI2CM_sync(hw);

                    //Transzfer inditasa, az ADDR regiszter irasaval. Hatasara
                    //start/restart feltetel fog generalodni.
                    hw->ADDR.reg=addressReg;
                    MyI2CM_sync(hw);

                    //Az utoljara olvasott adatbyte meg a DATA regiszterben van.
                    //A restart utan olvashato ki.
                    *i2cm->dataPtr=(uint8_t)hw->DATA.reg;

                    //Az uj leiro szerinti adattartalomra mutatunk
                    i2cm->dataPtr    =actualBlock->buffer;

                    //A slave msz. tiltasa. master msz engedelyezese.
                    hw->INTENSET.reg=SERCOM_I2CM_INTENSET_MB;
                    hw->INTENCLR.reg=SERCOM_I2CM_INTENCLR_SB;
                }
            }
        } else
        {   //meg van mit olvasni az adott blokkbol.
            //Az adatbyte bufferbe helyezese.
            *i2cm->dataPtr=(uint8_t) hw->DATA.reg;
            i2cm->dataPtr++;
            //Hatralevo byteok szama csokken
            i2cm->leftByteCnt--;
            MyI2CM_sync(hw);
        }

        return;
    } //if (Hw->INTFLAG.bit.SB)
}
//------------------------------------------------------------------------------
//Atviteli leirok listaja alapjan atvitel vegrehajtasa
status_t MYI2CM_transfer(MyI2CM_Device_t* i2cDevice,
                        const MyI2CM_xfer_t* xferBlocks, uint32_t blockCount)
{
    status_t status=kStatus_Success;
    MyI2CM_t* i2cm=i2cDevice->i2c;
    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;

    //Interfesz lefoglalasa a hivo taszk szamara
    xSemaphoreTake(i2cm->busMutex, portMAX_DELAY);

    //Arbitacio vesztes eseten ujra probalkozik. (Addig ciklus....)
    while(1)
    {
        //Varakozas, hogy a busz felszabaduljon...
        MyI2CM_sync(hw);
        uint32_t toCnt=0;
        while (hw->STATUS.bit.BUSSTATE != 1)
        {
            if (toCnt > 1000)
            {   //Timeout!
                status=kMyI2CMStatus_BusError;
                goto error;
            }
            vTaskDelay(1);
            toCnt++;
        }


        //Az elso kiirando transzfer leirora allunk...
        i2cm->actualBlock =xferBlocks;
        i2cm->leftBlockCnt=blockCount;
        i2cm->dataPtr     =xferBlocks->buffer;
        i2cm->leftByteCnt =xferBlocks->length;
        const MyI2CM_xfer_t* actualBlock=xferBlocks;

        //A hatralevo blokkok szamat csokkentjuk.
        i2cm->leftBlockCnt--;

        //Megjegyezzuk a slave eszkoz cimet, mivel IT alol is kell majd
        //restartnal a cimet ismerni.
        //A slave cimet shifteljuk 1-el balra, mert a 0. bit az R/W bit helye.
        i2cm->slaveAddress= (uint8_t)(i2cDevice->slaveAddress << 1);

        //Megszakitasok tiltasa es torlese...
        hw->INTENCLR.reg=SERCOM_I2CM_INTENCLR_MASK;
        hw->INTFLAG.reg=SERCOM_I2CM_INTFLAG_MASK;

        //I2C allapotok torlese
        hw->STATUS.reg=
                    SERCOM_I2CM_STATUS_ARBLOST |
                    SERCOM_I2CM_STATUS_LOWTOUT |
                    SERCOM_I2CM_STATUS_BUSERR  |
                    SERCOM_I2CM_STATUS_LENERR  |
                    0;
        MyI2CM_sync(hw);


        //Az elso adatblokk iranya szerinti busz ciklus inditas...
        uint32_t addressReg= (uint32_t)i2cm->slaveAddress;

        if (actualBlock->dir & MYI2CM_FLAG_TENBIT)
        {   //10 bites cimzes eloirva. Beallitjuk a regiszterbe ezt is.
            addressReg |= SERCOM_I2CM_ADDR_TENBITEN;
        }

        if ((actualBlock->dir & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
        {   //RX

            if (actualBlock->length==0)
            {   //Hibas transzfer leiro!
                //Olvasas eseten a transzfer hossza nem lehet 0!
                status=kMyI2CMStatus_InvalidTransferDescriptor;
                goto error;
            }

            //RW bit = 1 --> Read
            addressReg |= 1;

            //Alapertelmezesben ACK-t adunk minden olvasott adatbyte-ra.
            //Az utolso elott majd NACK-ra valtunk a megszakitasban.
            hw->CTRLB.bit.ACKACT=0;
            MyI2CM_sync(hw);
        } else
        {   //TX
            //Az elso adatbyteot majd a megszakitasban helyezi a buszra,
            //ha van.
        }

        MY_ENTER_CRITICAL();

        //Transzfer inditasa, az ADDR regiszter irasaval. Hatasara start
        //feltetel fog generalodni. A folytatas mar a megszakitasban tortenik.
        hw->ADDR.reg=addressReg;
        MyI2CM_sync(hw);

        //Iranynak megfelelo megszakitas engedelyezese...
        if ((actualBlock->dir & MYI2CM_DIR_MASK) == MYI2CM_DIR_RX)
        {   //RX
            hw->INTENSET.reg=SERCOM_I2CM_INTENSET_MB |
                             SERCOM_I2CM_INTENSET_SB;
        } else
        {   //TX
            hw->INTENSET.reg = SERCOM_I2CM_INTENSET_MB;
        }
        MY_LEAVE_CRITICAL();


        //Varakozas, hogy a transzfer befejezodjon...
        if (xSemaphoreTake(i2cm->semaphore, I2C_TRANSFER_TIMEOUT)==pdFALSE)
        {   //Hiba a buszon. Timeout.

            //Minden interrupt tiltasa a periferian...
            hw->INTENCLR.reg=SERCOM_I2CM_INTENCLR_MASK;

            printf("I2C bus error.\n");

            if (i2cm->asyncStatus==kStatus_Success)
            {
                status=kMyI2CMStatus_BusError;
            }
        } else
        {
            //Minden interrupt tiltasa a periferian...
            status=i2cm->asyncStatus;
        }

{
#if 0
        i2cm->asyncStatus=kStatus_Success;
        i2cm->last=false;

        //Az elso vegrehajtando adatblokkra allunk.

        const MyI2CM_xfer_t* prevBlock=NULL;

        //Vegig a transzfer block leirokon...
        for(uint32_t blockCnt=blockCount; blockCnt; blockCnt--, actualBlock++)
        {
            const MyI2CM_xfer_t* nextBlock=NULL;
            if (blockCnt>1) nextBlock=(actualBlock+1);

            //Megszakitasok tiltasa es torlese...
            hw->INTENCLR.reg=SERCOM_I2CM_INTENCLR_MASK;
            hw->INTFLAG.reg=SERCOM_I2CM_INTFLAG_MASK;

            //I2C allapotok torlese
            hw->STATUS.reg=
                        SERCOM_I2CM_STATUS_ARBLOST |
                        SERCOM_I2CM_STATUS_LOWTOUT |
                        SERCOM_I2CM_STATUS_BUSERR  |
                        SERCOM_I2CM_STATUS_LENERR  |
                        0;
            MyI2CM_sync(hw);

            //Az aktualis blokk bufferere allunk...
            i2cm->dataPtr    =actualBlock->buffer;
            i2cm->leftByteCnt=actualBlock->length;


            if ((prevBlock) &&
                ((prevBlock->dir & MYI2CM_DIR_MASK)==(actualBlock->dir & MYI2CM_DIR_MASK)))
            {   //Ez a blokk ugyanolyan tipusu, mint az elozo. Ezek szerint
                //folytatjuk a megkezdett streamet.
                if ((actualBlock->dir & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
                {   //RX

                } else
                {   //TX.
                    //A tovabbi kiirast, mely utan feljon a megszakitas, az
                    //adatregiszter irasaval kezdemenyezi
                    if ((i2cm->leftByteCnt==0) || (i2cm->dataPtr==NULL))
                    {   //nincs adattartalom! ez hiba!
                        status=kMyI2CMStatus_InvalidTransferDescriptor;
                        break;
                    }

                    hw->DATA.reg=*i2cm->dataPtr;
                    i2cm->leftByteCnt--;
                    MyI2CM_sync(hw);
                }
            } else
            {   //Ez az elso blokk, vagy valtozott az iranya.
                //Ez mindenkeppen uj start/restart feltetelt jelent.

                //START feltetel...

                //A 7 bits slave cimet shifteljuk 1-el balra, mert a 0. bit az
                //R/W bit helye
                uint32_t addressReg= (uint32_t)(i2cDevice->slaveAddress << 1);

                if (actualBlock->dir & MYI2CM_FLAG_TENBIT)
                {   //10 bites cimzes eloirva. Beallitjuk a regiszterbe ezt is.
                    addressReg |= SERCOM_I2CM_ADDR_TENBITEN;
                }

                if ((actualBlock->dir & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
                {   //RX

                    if (actualBlock->length==0)
                    {   //Hibas transzfer leiro!
                        //Olvasas eseten a transzfer hossza nem lehet 0!
                        status=kMyI2CMStatus_InvalidTransferDescriptor;
                        break;
                    }

                    //RW bit = 1 --> Read
                    addressReg |= 1;

                    //Alapertelmezesben ACK-t adunk minden olvasott adatbyte-ra.
                    hw->CTRLB.bit.ACKACT=0;
                    MyI2CM_sync(hw);
                } else
                {   //TX
                    //Az elso adatbyteot majd a megszakitasban helyezi a buszra,
                    //ha van.
                }

                //Transzfer inditasa, az ADDR regiszter irasaval. Hatasara start
                //vagy repeated start feltetel fog generalodni.
                hw->ADDR.reg=addressReg;
                MyI2CM_sync(hw);

                //if (i2cm->last)
                //{   //Az utolso read utolso byte-jat csak a stop/restart kiadasa
                //    //utan szabad olvasni, igy az akkor kezdett olvasas mar a
                //    //smart mode miatt nem fog automatikusan ujabb (felesleges)
                //    //buszciklust kezdemenyezni.
                //    *i2cm->dataPtr=(uint8_t) hw->DATA.reg;
                //    MyI2CM_sync(hw);
                //}
            }

            i2cm->last=false;
            if ((nextBlock) &&
               ((nextBlock->dir & MYI2CM_DIR_MASK)==(actualBlock->dir & MYI2CM_DIR_MASK)))
            {   //Van utana blokk, es annak iranya ugyan az.
            } else
            {   //Nincs utana blokk, vagy anank iranya valtozik.
                //RX eseten a stop utan ki kell olvasni az utolso adatbyteot.
                if ((actualBlock->dir & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
                {
                    i2cm->last=true;
                }
            }



            //Iranynak megfelelo megszakitas engedelyezese...
            MY_ENTER_CRITICAL();
            if ((actualBlock->dir & MYI2CM_DIR_MASK) == MYI2CM_DIR_RX)
            {   //RX
                hw->INTENSET.reg=SERCOM_I2CM_INTENSET_MB |
                                 SERCOM_I2CM_INTENSET_SB;
            } else
            {   //TX
                hw->INTENSET.reg = SERCOM_I2CM_INTENSET_MB;
            }
            MyI2CM_sync(hw);
            MY_LEAVE_CRITICAL();



            //Varakozas, hogy a transzfer befejezodjon...
            if (xSemaphoreTake(i2cm->semaphore, I2C_TRANSFER_TIMEOUT)==pdFALSE)
            {   //Hiba a buszon. Timeout.

                //Minden interrupt tiltasa a periferian...
                hw->INTENCLR.reg=SERCOM_I2CM_INTENCLR_MASK;

                printf("I2C bus error.\n");

                //TODO: itt stopot generalni, vagy valahogy feloldani a hibat!      !!!!
                if (i2cm->asyncStatus==kStatus_Success)
                {
                    status=kMyI2CMStatus_BusError;
                }
            } else
            {
                //Minden interrupt tiltasa a periferian...
                hw->INTENCLR.reg=SERCOM_I2CM_INTENCLR_MASK;
                status=i2cm->asyncStatus;
            }
            MyI2CM_sync(hw);


            //STOP feltetel generalasa...
            //if (actualBlock->dir & MYI2CM_FLAG_STOP)
            //{   //Stop feltetel eloirva.
            //    hw->CTRLB.bit.CMD=3;
            //} else
            //{
            //    if ((actualBlock->dir & MYI2CM_FLAG_RESTART) && (blockCnt>1))
            //    {   //Restart feltetel eloirva, es meg van hatra blokk.
            //        //Nem kell tenni semmit. A kovetkezo leiro ADDR regiszter
            //        //irasa restartot fog generalni.
            //    } else
            //    {   //Busz lezarasa (hiba, vagy ha nincs eloirva)
            //        hw->CTRLB.bit.CMD=3;
            //    }
            //}
            //MyI2CM_sync(hw);


            //Hiba eseten kilepes.
            if (status)
            {
                //Ha olvasaskor volt a hiba, akkro NACK-t allitunk be. A lezaro
                //STOP-ra azt kuldjon. TX eseten ez lenyegtelen.
                hw->CTRLB.bit.ACKACT=1;

                //Ha meg elo lenne irva az olvasas, akkor az ne hajtodjon vegre!
                i2cm->last=false;
                break;
            }


            if ((actualBlock->dir & MYI2CM_DIR_MASK) == MYI2CM_DIR_RX)
            {   //Az utolso atvitel olvasas volt.

            }


            prevBlock=actualBlock;
        } //for(uint32_t blockCnt=blockCount; blockCnt; blockCnt--)


        //STOP feltetel generalasa...

        if (i2cm->last)
        {   //Az utolso read utolso byte-jat csak a stop kiadasa utan szabad
            //olvasni, Igy az akkor kezdett olvasas mar a smart mode miatt
            //nem fog automatikusan ujabb (felesleges) buszciklust kezde-
            //menyezni.
            hw->CTRLB.bit.ACKACT=1;
            MyI2CM_sync(hw);

            hw->CTRLB.bit.CMD=3;
            MyI2CM_sync(hw);

            //vTaskDelay(1);

            *i2cm->dataPtr=(uint8_t) hw->DATA.reg;
            MyI2CM_sync(hw);
        } else
        {
            hw->CTRLB.bit.CMD=3;
            MyI2CM_sync(hw);
        }
#endif
}

        if (status==kMyI2CMStatus_ArbitationLost)
        {   //Arbitacio veszetes. Ujra probalkozik...
            printf("I2C arbitation lost.\n");
            continue;
        }

        break;
    } //while(1);

error:
    //Interfeszt fogo mutex feloldasa
    xSemaphoreGive(i2cm->busMutex);

    if (status)
    {
        __NOP();
        __NOP();
        __NOP();
        __NOP();
    }

    return status;
}
//------------------------------------------------------------------------------
