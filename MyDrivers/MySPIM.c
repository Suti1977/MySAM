//------------------------------------------------------------------------------
//  SPI Master driver
//
//    File: MySPIM.c
//------------------------------------------------------------------------------
#include "MySPIM.h"
#include <string.h>

static void MySPIM_initSercom(MySPIM_t* spim, const MySPIM_Config_t* config);
static void MySPIM_getNextBlock(MySPIM_t* spim);
//------------------------------------------------------------------------------
//driver kezdeti inicializalasa
void MySPIM_init(MySPIM_t* spim, const MySPIM_Config_t* config)
{
    ASSERT(spim);
    ASSERT(config);

    //Modul valtozoinak kezdeti torlese.
    memset(spim, 0, sizeof(MySPIM_t));

  #ifdef USE_FREERTOS
    //Az egyideju busz hozzaferest tobb taszk kozott kizaro mutex letrehozasa
    spim->busMutex=xSemaphoreCreateMutex();
    ASSERT(spim->busMutex);
    //Az intarrupt alat futo folyamatok vegere varo szemafor letrehozasa
    spim->semaphore=xSemaphoreCreateBinary();
    ASSERT(spim->semaphore);
  #endif //USE_FREERTOS

    //Alap sercom periferia driver initje.
    //Letrejon a sercom leiro, beallitja es engedelyezi a Sercom orajeleket
    MySercom_init(&spim->sercom, &config->sercomCfg);

    //Sercom beallitasa SPI master interfacenek megfeleloen, a kapott config
    //alapjan.
    MySPIM_initSercom(spim, config);
}
//------------------------------------------------------------------------------
//I2C interfacehez tartozo sercom felkonfiguralasa
static void MySPIM_initSercom(MySPIM_t* spim, const MySPIM_Config_t* config)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;

    //Periferia resetelese
    hw->CTRLA.reg=SERCOM_SPI_CTRLA_SWRST;
    while(hw->SYNCBUSY.reg);

    //Sercom uzemmod beallitas (0x03-->SPI master)
    hw->CTRLA.reg=SERCOM_SPI_CTRLA_MODE(0x03); __DSB();

    //A konfiguraciokor megadott attributum mezok alapjan a periferia mukodese-
    //nek beallitasa.
    //Az attributumok bitmaszkja igazodik a CTRLA regiszter bit pozicioihoz,
    //igy a beallitasok nagy resze konnyen elvegezheto. A nem a CTRLA regisz-
    //terre vonatkozo bitek egy maszkolassal kerulnek eltuntetesre.
    hw->CTRLA.reg |=
            (config->attribs & MYSPIMM_CTRLA_CONFIG_MASK) |
            0;
    __DSB();

    hw->CTRLB.reg =
            //Vetel engedelyezett
            SERCOM_SPI_CTRLB_RXEN |
            //Hardver kezelje-e az SS vonalat? Config alapjan.
            (config->hardwareSSControl ? SERCOM_SPI_CTRLB_MSSEN : 0) |
            0;
    __DSB();
    while(hw->SYNCBUSY.reg);

    //Adatatviteli sebesseg beallitasa
    //A beallitando ertek kialakitasaban a MYSPIM_CALC_BAUDVALUE makro segiti
    //a felhasznalot.
    hw->BAUD.reg=config->baudValue;
    __DSB();

    //Sercom-hoz tartozo interruptok engedelyezese az NVIC-ben.
    //Feltetelezzuk, hogy egy SERCOM-hoz tartozo interruptok egymas utan
    //sorban kovetkeznek, es egy sercom-hoz 4 darab tartozik.
    MySercom_enableIrqs(&spim->sercom);
}
//------------------------------------------------------------------------------
//SPI mukodes engedelyezese
void MySPIM_enable(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;

    hw->CTRLA.bit.ENABLE=1;
    __DSB();
    while(hw->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//SPI mukodes tiltaasa
void MySPIM_disable(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;

    hw->CTRLA.bit.ENABLE=0;
    __DSB();
    while(hw->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//Egyetlen byte kuldese. A rutin bevarja, mig a byte kimegy.
void MySPIM_sendByte(MySPIM_t* spim, uint8_t data)
{

    SercomSpi* hw=&spim->sercom.hw->SPI;
    hw->DATA.reg=data;
    while(hw->INTFLAG.bit.DRE==0);
}
//------------------------------------------------------------------------------
//SPI-s adat transzfer, leirok alapjan.
//A leiroknak a muvelet vegeig a memoriaban kell maradnia!
void MySPIM_transfer(MySPIM_t* spim,
                     const MySPIM_xfer_t* transferBlocks,
                     uint32_t transferBlockCount)
{
    //SPI busz lezarasa a tranzakcio idejere a tobbi taszk elol
    #ifdef USE_FREERTOS
    xSemaphoreTake(spim->busMutex, portMAX_DELAY);
    #endif

    //Az elso vegrehajtando transzfer leirot allitjuk be
    spim->xferBlock=transferBlocks;
    spim->leftXferBlockCount=transferBlockCount;

    //Elso blockra allas
    MySPIM_getNextBlock(spim);

    //Varakozas arra, hogy a transzfer lefusson...
    #ifdef USE_FREERTOS
    xSemaphoreTake(spim->semaphore, portMAX_DELAY);
    #endif


    //Busz foglals felszabaditasa
    #ifdef USE_FREERTOS
    xSemaphoreGive(spim->busMutex);
    #endif
}
//------------------------------------------------------------------------------
static void MySPIM_getNextBlock(MySPIM_t* spim)
{
    SercomSpi* hw=&spim->sercom.hw->SPI;


    //Soron kovetkezo block beallitasa, mely tartalmaz valos adatmenyniseget...
    const MySPIM_xfer_t* xferBlock=spim->xferBlock;

    while(spim->leftXferBlockCount)
    {
        //A soron kovekezo blokk tartalmaz kiirando menyniseget. kilephetunk
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

        portYIELD_FROM_ISR(higherPriorityTaskWoken);

        return;
    }


    //A kiirando byteok szamanak beallitasa az uj block szerint...
    //De egyel kevesebbet, mivel ebebn a fuggevnyben el lesz inditva az elso.
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
        hw->DATA.reg=(uint8_t)0xff;
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
            hw->DATA.reg=(uint8_t)0xff;
        }

        //Hatralevo byteok szamanak csokkentese
        spim->leftByteCount--;
    //}
}
//------------------------------------------------------------------------------
