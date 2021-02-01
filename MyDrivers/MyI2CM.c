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
            //SERCOM_I2CM_CTRLA_SDAHOLD(3) |
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

    //Megszakitasi flagek olvasasa.
    uint8_t intFlags=hw->INTFLAG.reg;
    //A megszakitasi flageket itt nem szabad meg torolni, mivel a CMD mezo csak
    //akkor irhato, ha valamelyik INTFLAG 1 (SB/MB). Ha nem igy tennenk, akkor
    //a buszt folyamatosan fogna a periferia.

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

        //A statusz regiszter allapotanak megjegyzese. Ezt majd a szalban, a
        //transfer funkcioban ertekeli ki.
        i2cm->statusRegValue=hw->STATUS.reg;

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

                if ((actualBlock->flags & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
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
                    if (actualBlock->flags & MYI2CM_FLAG_TENBIT)
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
        //(RX eseten mindenkeppen olvas az MCU egy byteot a START es CIM utan.)
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

                if ((actualBlock->flags & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
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
                    if (actualBlock->flags & MYI2CM_FLAG_TENBIT)
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

        i2cm->asyncStatus=kStatus_Success;
        i2cm->statusRegValue=0;

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

        if (actualBlock->flags & MYI2CM_FLAG_TENBIT)
        {   //10 bites cimzes eloirva. Beallitjuk a regiszterbe ezt is.
            addressReg |= SERCOM_I2CM_ADDR_TENBITEN;
        }

        if ((actualBlock->flags & MYI2CM_DIR_MASK)==MYI2CM_DIR_RX)
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
        if ((actualBlock->flags & MYI2CM_DIR_MASK) == MYI2CM_DIR_RX)
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
            status=i2cm->asyncStatus;
            if ((status==kStatus_Success) && (i2cm->statusRegValue))
            {
                //A megszakitasi rutinban kiolvasott, es letarolt STATUS
                //regiszter egyes bitjei alapjan hibakod beallitasa.
                if (i2cm->statusRegValue & SERCOM_I2CM_STATUS_ARBLOST)
                {   //Arbitacio vesztes
                    status=kMyI2CMStatus_ArbitationLost;
                } else
                if (i2cm->statusRegValue & SERCOM_I2CM_STATUS_BUSERR)
                {   //Busz hiba
                    status=kMyI2CMStatus_BusError;
                } else
                if (i2cm->statusRegValue & SERCOM_I2CM_STATUS_RXNACK)
                {   //NACK.
                    status=kMyI2CMStatus_NACK;
                } else
                if (i2cm->statusRegValue & SERCOM_I2CM_STATUS_SEXTTOUT)
                {   //NACK.
                    status=kMyI2CMStatus_SlaveSclLowExtendTimeout;
                } else
                if (i2cm->statusRegValue & SERCOM_I2CM_STATUS_MEXTTOUT)
                {   //NACK.
                    status=kMyI2CMStatus_MasterSclExtendTimeout;
                } else
                if (i2cm->statusRegValue & SERCOM_I2CM_STATUS_LOWTOUT)
                {   //NACK.
                    status=kMyI2CMStatus_SclLowTimeout;
                } else
                {   //A nem kezelt esetekben busz hiba
                    status=kMyI2CMStatus_BusError;
                }
            }
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
