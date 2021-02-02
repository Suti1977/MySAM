//------------------------------------------------------------------------------
//  SPI Master driver
//
//    File: MySPIM.c
//------------------------------------------------------------------------------
#include "MySPIM.h"
#include <string.h>

static void MySPIM_initPeri(MySPIM_t* spim);
static void MySPIM_deinitPeri(MySPIM_t* spim);
static void MySPIM_getNextBlock(MySPIM_t* spim);
//------------------------------------------------------------------------------
//SPI eszkoz letrehozasa
//spiDevice: A letrehozando eszkoz leiroja
//spi: Annak a busznak a MySPIM driverenek handlere,melyre az eszkoz csatlakozik
//slaveSelectFunc: A slave select (chip select) vonal vezerleset megvalosito
//                 callback funkcio
//handler: Az SPI-s eszkozhoz tartozo driver handlere
void MySPIM_createDevice(MySPIM_Device_t *spiDevice,
                         MySPIM_t *spi,
                         MySPIM_slaveSelectFunc_t* slaveSelectFunc,
                         void *handler)
{
    spiDevice->spi=spi;
    spiDevice->handler=handler;
    spiDevice->slaveSelectFunc=slaveSelectFunc;
}
//------------------------------------------------------------------------------
//driver kezdeti inicializalasa, letrehozasa
void MySPIM_create(MySPIM_t* spim,
                   const MySPIM_Config_t* config,
                   const MySercom_Config_t* sercomCfg)
{
    ASSERT(spim);
    ASSERT(config);

    //Modul valtozoinak kezdeti torlese.
    memset(spim, 0, sizeof(MySPIM_t));

    //Konfiguraciot hordozo strukturara mutato pointer mentese
    spim->config=config;

    //Sercom driver letrehozasa
    MySercom_create(&spim->sercom, sercomCfg);

    //IRQ prioritasok beallitasa
    MySercom_setIrqPriorites(&spim->sercom, config->irqPriorities);


  #ifdef USE_FREERTOS
    //Az egyideju busz hozzaferest tobb taszk kozott kizaro mutex letrehozasa
    spim->busMutex=xSemaphoreCreateMutex();
    ASSERT(spim->busMutex);
    //Az intarrupt alat futo folyamatok vegere varo szemafor letrehozasa
    spim->semaphore=xSemaphoreCreateBinary();
    ASSERT(spim->semaphore);
  #endif //USE_FREERTOS
}
//------------------------------------------------------------------------------
//SPI driver es eroforrasok felaszabditasa
void MySPIM_destory(MySPIM_t* spim)
{
    MySPIM_deinit(spim);

  #ifdef USE_FREERTOS
    if (spim->busMutex) vSemaphoreDelete(spim->busMutex);
    if (spim->semaphore) vSemaphoreDelete(spim->semaphore);
  #endif //USE_FREERTOS
}
//------------------------------------------------------------------------------
//SPI Periferia inicializalasa/engedelyezese
void MySPIM_init(MySPIM_t* spim)
{
    //Sercom beallitasa SPI master interfacenek megfeleloen, a kapott config
    //alapjan.
    MySPIM_initPeri(spim);
}
//------------------------------------------------------------------------------
//SPI Periferia tiltasa. HW eroforrasok tiltasa.
void MySPIM_deinit(MySPIM_t* spim)
{
    //Sercom beallitasa SPI master interfacenek megfeleloen, a kapott config
    //alapjan.
    MySPIM_deinitPeri(spim);
}
//------------------------------------------------------------------------------
//SPI interfacehez tartozo sercom felkonfiguralasa
static void MySPIM_initPeri(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;
    const MySPIM_Config_t* cfg=spim->config;

    //Mclk es periferia orajelek engedelyezese
    MySercom_enableMclkClock(&spim->sercom);
    MySercom_enableCoreClock(&spim->sercom);
    //MySercom_enableSlowClock(&spim->sercom);

    //Periferia resetelese
    //hw->CTRLA.reg=SERCOM_SPI_CTRLA_SWRST;

    while(hw->SYNCBUSY.reg);

    //Sercom uzemmod beallitas (0x03-->SPI master)
    hw->CTRLA.reg=SERCOM_SPI_CTRLA_MODE(0x03); __DMB();

    //A konfiguraciokor megadott attributum mezok alapjan a periferia mukodese-
    //nek beallitasa.
    //Az attributumok bitmaszkja igazodik a CTRLA regiszter bit pozicioihoz,
    //igy a beallitasok nagy resze konnyen elvegezheto. A nem a CTRLA regisz-
    //terre vonatkozo bitek egy maszkolassal kerulnek eltuntetesre.
    hw->CTRLA.reg |=
            (cfg->attribs & MYSPIM_CTRLA_CONFIG_MASK) |
            0;
    __DMB();

    hw->CTRLB.reg =
            //Vetel engedelyezett
            SERCOM_SPI_CTRLB_RXEN |
            //Hardver kezelje-e az SS vonalat? Config alapjan.
            (cfg->hardwareSSControl ? SERCOM_SPI_CTRLB_MSSEN : 0) |
            0;
    __DMB();
    while(hw->SYNCBUSY.reg);

    //Adatatviteli sebesseg beallitasa
    MySPIM_setBitRate(spim, (spim->bitRate==0) ? cfg->bitRate : spim->bitRate);

    //Sercom-hoz tartozo interruptok engedelyezese az NVIC-ben.
    //Feltetelezzuk, hogy egy SERCOM-hoz tartozo interruptok egymas utan
    //sorban kovetkeznek, es egy sercom-hoz 4 darab tartozik.
    MySercom_enableIrqs(&spim->sercom);
}
//------------------------------------------------------------------------------
static void MySPIM_deinitPeri(MySPIM_t* spim)
{
    //SPI periferia tiltasa
    MySPIM_disable(spim);

    //Minden msz tiltasa az NVIC-ben
    MySercom_disableIrqs(&spim->sercom);

    //Periferia orajelek tiltasa
    //MySercom_disableSlowClock(&spim->sercom);
    MySercom_disableCoreClock(&spim->sercom);
    MySercom_disableMclkClock(&spim->sercom);
}
//------------------------------------------------------------------------------
//SPI periferia resetelese
void MySPIM_reset(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;
    MY_ENTER_CRITICAL();
    hw->CTRLA.reg=SERCOM_SPI_CTRLA_SWRST;
    while(hw->SYNCBUSY.reg);
    spim->bitRate=0;
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//SPI mukodes engedelyezese
void MySPIM_enable(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;
    MY_ENTER_CRITICAL();
    hw->CTRLA.bit.ENABLE=1;
    __DMB();
    while(hw->SYNCBUSY.reg);
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//SPI mukodes tiltaasa
void MySPIM_disable(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;
    MY_ENTER_CRITICAL();
    hw->CTRLA.bit.ENABLE=0;
    __DMB();
    while(hw->SYNCBUSY.reg);
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//SPI busz sebesseg modositasa/beallitasa
void MySPIM_setBitRate(MySPIM_t* spim, uint32_t bitRate)
{
    if (spim->bitRate==bitRate) return;
    spim->bitRate=bitRate;

    SercomSpi* hw=&spim->sercom.hw->SPI;

    if (bitRate==0) return;
    uint32_t coreFreq=spim->sercom.config->coreFreq;
    uint8_t baudValue= (uint8_t)((uint64_t)coreFreq / (2*bitRate))-1;

    //Ha be van kapcsolva a sercom, akkor azt elobb tiltani kell.
    if (hw->CTRLA.bit.ENABLE)
    {   //A periferia be van kapcsolva. Azt elobb tiltani kell.
        hw->CTRLA.bit.ENABLE=0;
        while(hw->SYNCBUSY.reg);

        //baud beallitasa
        hw->BAUD.reg=baudValue;

        //SPI periferia engedelyezese. (Ez utan mar bizonyos config bitek nem
        //modosithatok!)
        hw->CTRLA.bit.ENABLE=1;
        while(hw->SYNCBUSY.reg);
    }
    else
    {
        //baud beallitasa
        hw->BAUD.reg= baudValue;
    }
}
//------------------------------------------------------------------------------
//Egyetlen byte kuldese. A rutin bevarja, mig a byte kimegy.
void MySPIM_sendByte(MySPIM_t* spim, uint8_t data)
{

    SercomSpi* hw=&spim->sercom.hw->SPI;
    hw->DATA.reg=data;
    __DMB();
    while(hw->INTFLAG.bit.DRE==0);
}
//------------------------------------------------------------------------------
//SPI-s adat transzfer, leirok alapjan.
//A leiroknak a muvelet vegeig a memoriaban kell maradnia!
void MySPIM_transfer(MySPIM_Device_t *spiDevice,
                     const MySPIM_xfer_t* transferBlocks,
                     uint32_t transferBlockCount)
{
    //Eszkozhoz tartozo busz driver kijelolese
    MySPIM_t* spim=spiDevice->spi;

    //SPI busz lezarasa a tranzakcio idejere a tobbi taszk elol
    #ifdef USE_FREERTOS
    xSemaphoreTake(spim->busMutex, portMAX_DELAY);
    #endif

    //slave select vonallal eszkoz kijelolese.
    //A slave select vonalat egy, a letrehozaskor beallitott callback funkcio
    //vegzi.
    if (spiDevice->slaveSelectFunc) spiDevice->slaveSelectFunc(true);

    //Az elso vegrehajtando transzfer leirot allitjuk be
    spim->xferBlock=transferBlocks;
    spim->leftXferBlockCount=transferBlockCount;

    //Elso blockra allas
    MySPIM_getNextBlock(spim);

    //Varakozas arra, hogy a transzfer lefusson...
    #ifdef USE_FREERTOS
    xSemaphoreTake(spim->semaphore, portMAX_DELAY);
    #endif

    //Slave select vonal deaktivalasa
    if (spiDevice->slaveSelectFunc) spiDevice->slaveSelectFunc(false);

    //Busz foglals felszabaditasa
    #ifdef USE_FREERTOS
    xSemaphoreGive(spim->busMutex);
    #endif
}
//------------------------------------------------------------------------------
static void MySPIM_getNextBlock(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;


    //Soron kovetkezo block beallitasa, mely tartalmaz valos adatmennyiseget...
    const MySPIM_xfer_t* xferBlock=spim->xferBlock;

    while(spim->leftXferBlockCount)
    {
        //A soron kovekezo blokk tartalmaz kiirando mennyiseget. kilephetunk
        if (xferBlock->length) break;

        //Csokkentjuk a hatralevo
        spim->leftXferBlockCount--;

        //Ez a block el lesz dobva. Ugras a kovetkezore...
        xferBlock++;
    }

    //ellenorzse, hogy van-e meg hatra blokk...
    if (spim->leftXferBlockCount==0)
    {   //minden block el lett kudve.
        portBASE_TYPE higherPriorityTaskWoken;
        //Jelzes az applikacionak, hogy vegeztunk. A blokkolt taszk elindul.
        xSemaphoreGiveFromISR(spim->semaphore, &higherPriorityTaskWoken);

        //megszakitasok tiltasa
        hw->INTENCLR.reg = SERCOM_SPI_INTENCLR_RXC;

        portYIELD_FROM_ISR(higherPriorityTaskWoken)

        return;
    }


    //A kiirando byteok szamanak beallitasa az uj block szerint...
    //De egyel kevesebbet, mivel ebben a fuggevnyben el lesz inditva az elso.
    spim->leftByteCount=xferBlock->length-1;

    //pointerek beallitasa
    spim->rxPtr=xferBlock->rxData;
    spim->txPtr=xferBlock->txData;

    //Uj block kijelolese
    xferBlock++;
    spim->xferBlock=xferBlock;
    //Hatralevo blockok szamanak csokkentese
    spim->leftXferBlockCount--;


    if (spim->txPtr)
    {   //van mit kiirni
        hw->DATA.reg=*spim->txPtr++;
    } else
    {   //nincs mit kiirni. Dumy byteot kell irni az adat helyere
        hw->DATA.reg=0xff;
    }

    //megszakitas engedelyezese
    hw->INTENSET.reg = SERCOM_SPI_INTENSET_RXC;
}
//------------------------------------------------------------------------------
//Az SPI interfacehez tartozo sercom interrupt service rutinokbol hivando.
//Parameterkent at kell adni a kezelt interface leirojat.
//[Megszakitasban fut]
void MySPIM_service(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;

    //if (hw->INTFLAG.bit.TXC)
    //{   //Transmit complete. Egy adatbyte kiment, vagy egy adatbyte beerkezett.

        //msz flag torlese
        hw->INTFLAG.reg= SERCOM_SPI_INTFLAG_RXC;

        //volt eloirva olvasas?
        if (spim->rxPtr)
        {   //Beerkezett adatbyteot menteni kell.
            *spim->rxPtr++= (uint8_t) hw->DATA.reg ;
        } else
        {   //Dumy olvasas
            volatile uint8_t Dumy=(uint8_t) hw->DATA.reg;
            (void) Dumy;
        }



        //Van meg hatra byte?
        if (spim->leftByteCount==0)
        {   //Az adott adatblockbol minden adat ki/be ment.

            //Uj blockra allas, es annak az elso bytejanak elinditasa a buszon..
            MySPIM_getNextBlock(spim);
            return;
        }


        if (spim->txPtr)
        {   //Van kiirando adathalmaz. Kiirjuk. -->elindul a busz tarnszfer
            hw->DATA.reg=*spim->txPtr++;
        } else
        {   //Nincs mit kiirni. Dumy byteot irunk...
            hw->DATA.reg=0xff;
        }

        //Hatralevo byteok szamanak csokkentese
        spim->leftByteCount--;
    //}
}
//------------------------------------------------------------------------------
