//------------------------------------------------------------------------------
//  I2C kezelest segito wrapper. FreeRTOS-hez illeszkedik.
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
//Ha ennyi ido alatt sem sikerul egy I2C folyamatot vegrehajtani, akkor hibaval
//kilep.    [TICK]
#define I2C_TRANSFER_TIMEOUT    1000

static void MyI2CM_initSercom(MyI2CM_t* i2cm, const MyI2CM_Config_t* config);
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
    MyI2CM_initSercom(i2cm, config);
}
//------------------------------------------------------------------------------
//I2C interfacehez tartozo sercom felkonfiguralasa
static void MyI2CM_initSercom(MyI2CM_t* i2cm, const MyI2CM_Config_t* config)
{
    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;

    //Periferia resetelese
    hw->CTRLA.reg=SERCOM_I2CM_CTRLA_SWRST;
    while(hw->SYNCBUSY.reg);

    //Sercom uzemmod beallitas (0x05-->I2C master)
    hw->CTRLA.reg=SERCOM_I2CM_CTRLA_MODE(0x05);

    //A konfiguraciokor megadott attributum mezok alapjan a periferia mukodese-
    //nek beallitasa.
    //Az attributumok bitmaszkja igazodik a CTRLA regiszter bit pozicioihoz,
    //igy a beallitasok nagy resze konnyen elvegezheto. A nem a CTRLA regisz-
    //terre vonatkozo bitek egy maszkolassal kerulnek eltuntetesre.    
    hw->CTRLA.reg |=
            (config->attribs & MYI2CM_CTRLA_CONFIG_MASK) |
            //A driver ugy lett megirva, hogy az SCL vonalat az ACK bit
            //kiadasa utan tartsa az MCU.
            SERCOM_I2CM_CTRLA_SCLSM |
            0;

    hw->CTRLB.reg =
            //Smart uzemmod beallitasa. (Kevesebb szoftveres interrupcio kell)
            //Ez a driver a SMART modra epitkezik.
            SERCOM_I2CM_CTRLB_SMEN |
            //Quick command-ok engedelyezese.
            //SERCOM_I2CM_CTRLB_QCEN |
            0;
    while(hw->SYNCBUSY.reg);

    //Adatatviteli sebesseg beallitasa
    //A beallitando ertek kialakitasaban a MYI2CM_CALC_BAUDVALUE makro segiti
    //a felhasznalot.
    hw->BAUD.reg=config->baudValue;

    //I2C periferia engedelyezese. (Ez utan mar bizonyos config bitek nem
    //modosithatok!)
    hw->CTRLA.bit.ENABLE=1;
    while(hw->SYNCBUSY.reg);

    //A periferiat "UNKNOWN" allapotbol ki kell mozditani, mert addig nem tudunk
    //erdemben mukodni. Ez megteheto ugy, ha irjuk a statusz regiszterenek
    //a BUSSTATE mezojet.
    //Ekkor IDLE modba kerulunk.
    hw->STATUS.reg=SERCOM_I2CM_STATUS_BUSSTATE(1);
    while(hw->SYNCBUSY.reg);

    //Minden interrupt engedelyezese a periferian...
    hw->INTENSET.reg=   SERCOM_I2CM_INTENSET_ERROR |
                        SERCOM_I2CM_INTENSET_MB |
                        SERCOM_I2CM_INTENSET_SB;

    //Sercom-hoz tartozo interruptok engedelyezese az NVIC-ben.
    //Feltetelezzuk, hogy egy SERCOM-hoz tartozo interruptok egymas utan
    //sorban kovetkeznek, es egy sercom-hoz 4 darab tartozik.
    MySercom_enableIrqs(&i2cm->sercom);
}
//------------------------------------------------------------------------------
//Stop feltetel generalasa a buszon
static void MyI2CM_sendStop(SercomI2cm* hw)
{
    /*
    __DMB();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();

    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
*/
    //0x03 irasa a parancs regiszterbe STOP-ot general az eszkoz.
    uint32_t tmp;
    while(hw->SYNCBUSY.reg !=0 );
    tmp = hw->CTRLB.reg;
    tmp &= ~(SERCOM_I2CM_CTRLB_CMD_Msk /*| SERCOM_I2CM_CTRLB_ACKACT*/);
    tmp |= SERCOM_I2CM_CTRLB_CMD(0x03);
    hw->CTRLB.reg =tmp;
    __DMB();
    //Varakozas a szinkronra.
    while(hw->SYNCBUSY.reg);
/*
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();

    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __DMB();
    */

}
//------------------------------------------------------------------------------
//I2C folyamatok vegen hivodo rutin.
//Ebben tortenik meg a buszciklust lezaro stop feltetel kiadas, majd jelzes
//az applikacio fele, hogy vegzett az interruptban futo folyamat.
static void MyI2CM_end(MyI2CM_t* i2cm)
{

    /*
    //-TX eseten STOP
    //-RX eseten NACK, STOP
    if (i2cm->transferDir==MYI2CM_DIR_RX)
    {   //RX van, ezert NACK-zni kell!
        //Ezt beallitjuk a regiszterbe, melyet majd a stop parancsal
        //egyutt fog kikuldeni a sercom. Az I2C slave eszkoz inenn
        //tudja, hogy ez volt az utolso olvasas tole.
        i2cm->sercom.hw->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_ACKACT;
    }

    //Stop feltetel generalasa
    MyI2CM_sendStop(i2cm);


    //Ha elo van irva foglaltsagra varas, akkor varakozni kell a slave-re.
    //Ez a jelzes torolve lesz akkor is, ha megjott a vart ACK, es igy a
    //varakozasi folyamat befejezodik. Ugorhat a ciklus lezarasara...
    if (i2cm->waitingWhileBusy)
    {   //Ellenorizni kell, hogy az eszkoz foglalt.

        //Jelezzuk, hogy foglaltsag tesztelesi allapotba lepunk. A
        //tovabbiakban NACK-kat nem veszi hibanak. Amig nem kap ACK-t, addig
        //folyik a slave pingelese.
        i2cm->busyTest=true;

        //Restart feltetelt generalunk, irasi irannyal
        SercomI2cm* hw=&i2cm->sercom.hw->I2CM;
        hw->ADDR.reg=i2cm->slaveAddress;
        while(hw->SYNCBUSY.reg);

        //Folytatas majd az interrupt rutinban...
        return;
    }

    //<--ide jut, ha vegzett az osszes leiroval, es (mar) nem kell az eszkoz
    //   foglaltsagara varni.
    */


    //Jelzes az applikacionak
  #ifdef USE_FREERTOS
    portBASE_TYPE higherPriorityTaskWoken=pdFALSE;
    xSemaphoreGiveFromISR(i2cm->semaphore, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
  #endif //USE_FREERTOS
}
//------------------------------------------------------------------------------
//Kovetkezo vegrehajtando transzfer blokk keresese.
//A jelenleg futo blokk iranya ha nem valtozik, viszont a soron kovetkezo leiro
//0 hosszusagot mutat, akkor az a leiro kihagyasra kerul.
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
    //Aalapertelmezesben azt jelezzuk, hogy egy iranyhoz tratozo transzfernek
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
            //Elore csokkentjuka  beolavsando byteok szamat, mivel a start
            //kiadasa azonnal egy olvasast is general majd.
            if (i2cm->leftByteCnt)
            {
                i2cm->leftByteCnt--;
            }

            //olvasas eseten az ACK/NACK-t be kell allitani, ha csak 1 byte lenne,
            //mivel annak olvasasa a cim utan automatikusan meg fog tortenni.
            //De mindez csak akkor ervenyes, ha ez az utolso transzfer az adott
            //iranynak megfeleloen.
            if ((i2cm->leftByteCnt==0) && (i2cm->last))
            {   //NACK-t kell adni.
                hw->CTRLB.bit.ACKACT=1; __DMB();
            } else
            {   //ACK-t adunk a tovabbi byteokra.
                hw->CTRLB.bit.ACKACT=0; __DMB();
            }
            while(hw->SYNCBUSY.reg);
        }


        //Elso startfeltetel generalasa...
        //A start feltetel az iranytol fugg.
        hw->ADDR.reg=i2cm->slaveAddress | i2cm->transferDir;
        __DMB();
        while(hw->SYNCBUSY.reg);

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
            while(hw->SYNCBUSY.reg !=0 );
            while(hw->SYNCBUSY.reg !=0 );
            if (i2cm->dataPtr)
            {   //Van buffer az adatoknak
                *i2cm->dataPtr=hw->DATA.reg & 0xff;
            } else
            {   //nincs buffer. Csak olvassuk a regisztert.
                volatile uint8_t dummyRead=hw->DATA.reg & 0xff;
                (void)dummyRead;
            }
        }


        //jelzes az applikacionak, hogy kesz a leiroval
        MyI2CM_end(i2cm);

    } else
    {   //Vam meg hatra block.

        //A jelenlegi iranynak megfelelo foylamatotot kell folytatni?
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
                if ((i2cm->leftByteCnt==0) && (i2cm->last))
                {   //NACK-t kell adni.
                    hw->CTRLB.bit.ACKACT=1; __DMB();
                } else
                {   //ACK-t adunk a tovabbi byteokra.
                    hw->CTRLB.bit.ACKACT=0; __DMB();
                }
                while(hw->SYNCBUSY.reg);
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
                }
            }


            //Start/restart feltetel generalasa...
            //A start feltetel az iranytol fugg.
            hw->ADDR.reg=i2cm->slaveAddress | i2cm->transferDir;
            __DMB();
            while(hw->SYNCBUSY.reg);

        }
    } //if (i2cm->nextBlock==NULL) else
}
//------------------------------------------------------------------------------
//Az I2C interfacehez tartozo sercom interrupt service rutinokbol hivando.
//Parameterkent at kell adni a kezelt interface leirojat.
//[INTERRUPTBAN FUT]
void MyI2CM_service(MyI2CM_t* i2cm)
{
    SercomI2cm* hw=&i2cm->sercom.hw->I2CM;
    volatile SERCOM_I2CM_STATUS_Type    status;

    //Statusz regiszter kiolvasasa. A tovabbiakban ezt hasznaljuk az elemzeshez
    status.reg=hw->STATUS.reg;

    //..........................................................................
    if (hw->INTFLAG.bit.ERROR)
    {   //Valam hiba volt a buszon
        //TODO: kezelni a hibat
        //A hibakat a status register irja le.

        //Toroljuk a hiba IT jelzest
        __NOP();
        hw->INTFLAG.reg=SERCOM_I2CM_INTFLAG_ERROR;
    }
    //..........................................................................

    if (hw->INTFLAG.bit.MB)
    {   //A masterhez tartozo IT-t kaptunk
        //A buszon arbitaltunk vagy irtunk, es egy adatbyte/cim kiirasa
        //befejezodott, vagy busz/arbitacios hiba miatt meghiusult.


        if (status.bit.ARBLOST)
        {   //Elvesztettuk az arbitaciot a buszon, vagy mi birtokoljuk ugyan a
            //buszt, de azon valami hibat detektalt a periferia.

            //IT flag torlese.
            hw->INTFLAG.reg=SERCOM_I2CM_INTFLAG_MB;

            if (status.bit.BUSERR)
            {   //Busz hiba volt                
                i2cm->asyncStatus=kMyI2CMStatus_BusError;
            } else
            {   //Arbitacios hiba volt.               
                i2cm->asyncStatus=kMyI2CMStatus_ArbitationLost;
            }
            goto error;
        }

        //Normal esetben minden iras eseten kell, hogy kapjunk ACK-t.
        //Ha az elmarad, akkor a slave nem vette az uzenetunket.
        //De ha egy slave foglalt, akkor is adhat NACk-t!
        if (status.bit.RXNACK==1)
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
            }
        }

        //<--a slave ack-zott, vagy NACK volt, de foglaltsag veget varjuk.
        //Adatok kuldese amig van hatra...

        if (i2cm->leftByteCnt)
        {   //van meg mit kuldeni a bufferbol. A soron kovetkezo adatbyte
            //kuldesenek inditasa, es egyben az uj elem kijelolese...
            hw->DATA.reg = *i2cm->dataPtr++; __DMB();
            //Hatralevo byteok szama csokken
            i2cm->leftByteCnt--;
            while(hw->SYNCBUSY.reg);

        } else
        {   //Nincs mar mit kuldeni. Ez volt az utolso adatbyte a blokkban.
            //Uj blokk nyitasa, ha van meg, vagy a transzfer lezarasa.
            //A hivott rutin vagy restart feltetelt general, vagy a buszra
            //helyezi a kovetkezo adatbyteot, vagy jelez az applikacionak,
            //hogy kesz.
            //Abban az esetbben, ha varni kell az eszkozre, mert az foglalt,
            //ugy egy pingeles indul.

            if (i2cm->busyTest)
            {   //Foglaltsagi teszt van.

                //ACK-ra varunk, mely ha megjon, akkor mar nem foglalt a slave
                if (status.bit.RXNACK==0)
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
            hw->CTRLB.bit.ACKACT=1; __DMB();
            while(hw->SYNCBUSY.reg);
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
            while(hw->SYNCBUSY.reg);
            *i2cm->dataPtr++ = (uint8_t) hw->DATA.reg;

            //Hatralevo byteok szamanak csokkenetese...
            i2cm->leftByteCnt--;
        }

    } //if (Hw->INTFLAG.bit.SB)
    return;
    //..........................................................................
error:
    //<--ide ugrunk hiba eseten.
    //A hibat a korabban beallitott ->AsyncStatus valtozo orzi

    //Stop feltetel generalasa
    MyI2CM_sendStop(hw);

    //jelzes az applikacionak, hogy kesz a leiroval
    MyI2CM_end(i2cm);
    return;
}
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa
//Device: Az eszkoz leiroja
//I2C: Annak a busznak az I2CM driverenek handlere, melyre az eszkoz csatlakozik
//SlaveAddress: Eszkoz I2C slave cime a buszon
//Handler: Az I2C-s eszkozhoz tartozo driver handlere
void MyI2CM_CreateDevice(MyI2CM_Device_t* i2cDevice,
                         MyI2CM_t* i2c,
                         uint8_t slaveAddress,
                         void* handler)
{
    i2cDevice->i2c=i2c;
    i2cDevice->slaveAddress=slaveAddress;
    i2cDevice->handler=handler;
}
//------------------------------------------------------------------------------
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
            printf("I2C bus error.\n");
            status=kMyI2CMStatus_BusError;
        } else
        {
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

    //Interfeszt fogo mutex feloldasa
  #ifdef USE_FREERTOS
    xSemaphoreGive(i2cm->busMutex);
  #endif //USE_FREERTOS

    return status;
}
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
//I2C iras/olvasas rutin.
//Blokkolos mukodes. Megvarja amig a transzfer befejezodik.
//FIGYELEM!  a bemenetnek megadott buffereknek permanensen a memoriaban kell
//          maradni, amig az I2C folyamat le nem zarul, mivel a rutin nem keszit
//          masolatot a bemeno adatokrol.
//i2cDevice: I2C eszkoz leiroja
//txData1: Elso korben kiirando adatokra mutat
//txLength1: Az elso korben kiirando adatbyteok szama
//txData2: Masodik korben kiirando adatokra mutat
//txLength2: A masodik korben kiirando adatbyteok szama
//rxData:  Ide pakolja a buszrol olvasott adatbyteokat
//         (Az olvasaskor erre a memoriateruletre pakolja a vett byteokat.)
//rxLength: Ennyi byteot olvas be.
//Visszaterse: hibakoddal, ha van.

//Ha TxLength valtozok valamelyike 0, akkor azt kihagyja.
//Ha RxLength nulla, akkor nem hajt vegre olvasast.
//     Ha nem nulla, akkor az irasok utan restartot general, es utanna olvas.
#if 0
status_t MyI2CM_writeRead(MyI2CM_Device_t* i2cDevice,
                            uint8_t *txData1, uint32_t txLength1,
                            uint8_t *txData2, uint32_t txLength2,
                            uint8_t *rxData,  uint32_t rxLength)
{
    status_t status;

    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, txData1, txLength1},
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, txData2, txLength2},
        (MyI2CM_xfer_t){MYI2CM_DIR_RX, rxData,  rxLength }
    };

    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
#endif
//------------------------------------------------------------------------------
