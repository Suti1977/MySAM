//------------------------------------------------------------------------------
//  SPI Master driver
//
//    File: MySPIM.c
//------------------------------------------------------------------------------
#include "MySPIM.h"
#include <string.h>

static void MySPIM_initSercom(MySPIM_t* spim, const MySPIM_Config_t* config);
//------------------------------------------------------------------------------
//driver kezdeti inicializalasa
void MySPIM_init(MySPIM_t* spim, MySPIM_Config_t* config)
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
