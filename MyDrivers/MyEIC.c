//------------------------------------------------------------------------------
//  Kulso interrupt (EXTINT) kezelest segito driver 
//
//    File: MyEIC.c
//------------------------------------------------------------------------------
#include "MyEIC.h"
#include "MyGCLK.h"
#include <string.h>
//------------------------------------------------------------------------------
//EIC kezdeti inicializalasa
//Az inicializacio nem engedelyezi az EIC-et. Azt a csatornak beallitasa utan
//kell az applikacioban megoldani, a MyEIC_enable() segitsegevel.
void MyEIC_init(const MyEIC_Config_t* cfg)
{
    //EIC orajel engedelyezese
    MCLK->APBAMASK.bit.EIC_=1;

    Eic* eic=EIC;
    //EIC resetelese...
    eic->CTRLA.bit.SWRST=1; __DMB();
    while(eic->SYNCBUSY.reg);

    //EIC-hez orajelforras beallitasa. Lehet GCLK, vagy CLK_ULP32K. GCLK- csak
    //akkor kerul beallitasra, ha a az van eloirva.
    if (cfg->clkSelect==1)
    {   //CLK_ULP32K-t hasznalunk
        eic->CTRLA.bit.CKSEL=1;
    } else
    {   //GCLK kerul hasznalatra.
        //Engedelyezzuk es hozzarendeljuk a GCLK orajelet
        MyGclk_enablePeripherialClock(EIC_GCLK_ID, cfg->gclkNr);
    }

    //pergesmentesitesi orajel forras beallitasa
    eic->DPRESCALER.bit.TICKON=cfg->bounceSamplerClkSource;
    //A mintavetelek szama 3 vagy 7
    eic->DPRESCALER.bit.STATES0 =
    eic->DPRESCALER.bit.STATES1 = cfg->bounceSampleCount;

    //A mintavetelezes orajelenek eloosztasa
    eic->DPRESCALER.bit.PRESCALER0 =
    eic->DPRESCALER.bit.PRESCALER1 = cfg->bouncePrescaler;
}
//------------------------------------------------------------------------------
//Egy kulso interrupt vonal konfiguralasa
//Csak akkor lehetseges, ha az NVIC tiltott allapotu!
void MyEIC_configChannel(uint8_t channel, const MyEIC_ChannelConfig_t* cfg)
{
    Eic* eic=EIC;

    uint32_t mask= 1 << channel;

    //Aszinkron bemenet engedelyezese/tiltasa...
    if (cfg->asyncEnable)
    {
        eic->ASYNCH.reg |= mask;
    } else
    {
        eic->ASYNCH.reg &= ~mask;
    }

    //Event kimenet engedelyezese/tiltasa...
    if (cfg->eventOutEnable)
    {
        eic->EVCTRL.reg |= mask;
    } else
    {
        eic->EVCTRL.reg &= ~mask;
    }

    //csatornahoz pergesmentesites engedelyezese/tiltasa...
    if (cfg->debounceEnable)
    {
        eic->DEBOUNCEN.reg |= mask;
    } else
    {
        eic->DEBOUNCEN.reg &= ~mask;
    }

    //Filterezes es bemenet kezeles beallitasa...
    uint32_t temp;
    temp=cfg->inputSense;
    if (cfg->filterEnable) temp |= 0x80;
    temp <<= (4 * (channel & 7));

    mask= 0xfUL << (4 * (channel & 7));
    uint32_t group=channel / 8;
    eic->CONFIG[ group ].reg &= ~mask;
    eic->CONFIG[ group ].reg |= temp;

    //Ha az NVIC-be is be kell regisztralni es engedelyezni...
    if (cfg->enableInNVIC)
    {
        uint8_t irqN=EIC_0_IRQn + channel;
        NVIC_SetPriority(irqN, cfg->interruptPriority);
        NVIC_EnableIRQ(irqN);
    }
}
//------------------------------------------------------------------------------
//Alapertelemezett csatorna beallitasokkal feltolti az atadott strukturat
void MyEIC_getDefaultChannelConfig(MyEIC_ChannelConfig_t* cfg)
{
    memset(cfg, 0, sizeof(MyEIC_ChannelConfig_t));
}
//------------------------------------------------------------------------------
