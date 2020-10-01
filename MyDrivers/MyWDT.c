//------------------------------------------------------------------------------
//  Watchdog kezelo driver
//
//    File: MyWDT.c
//------------------------------------------------------------------------------
#include "MyWDT.h"
#include <string.h>


//------------------------------------------------------------------------------
// kezdeti inicializalasa
void MyWDT_init(const MyWDT_config_t* cfg)
{
    Wdt* hw=WDT;

    //A programozas elott a watchdogot ki kell kapcsolni
    MyWDT_disable();

    //Ellenorzes, hogy sikerult-e kikapcsolni. Ha nem, akkor az azt jelenti,
    //hogy a watchdog irasvedettre van allitva. Ezt megtehette egy esetleges
    //bootloader, de johet az NVM-bol is, melyet hardveres reset-kor tolt be.
    if (hw->CTRLA.bit.ENABLE) return;

    //Early Warning mod beallitasa. (IT engedelyezese vagy tiltasa)
    if (cfg->earlyWarning) hw->INTENSET.reg=WDT_INTENSET_EW;
                    else hw->INTENCLR.reg=WDT_INTENCLR_EW;
    __DMB();
    while(WDT->SYNCBUSY.reg);
    //Esetleges korabban beallt flag torlese
    hw->INTFLAG.reg=WDT_INTFLAG_EW;

    hw->EWCTRL.reg=cfg->earlyWarning;

    //Window/periodu beallitasa
    hw->CONFIG.reg=WDT_CONFIG_PER(cfg->period) | WDT_CONFIG_WINDOW(cfg->window);
    __DMB();
    while(WDT->SYNCBUSY.reg);

    //Control regiszter beallitasa...
    uint8_t regVal=0;
    if (cfg->alwaysOn) regVal|=WDT_CTRLA_ALWAYSON;
    if (cfg->windowMode) regVal|=WDT_CTRLA_WEN;
    hw->CTRLA.reg=regVal;
    __DMB();
    while(WDT->SYNCBUSY.reg);

    MyWDT_clear();

    //Ha konfiguracio szerint engedelyezni kell a watchdogot
    if (cfg->enable) MyWDT_enable();
}
//------------------------------------------------------------------------------
//Watchdog engedelyezese
void MyWDT_enable(void)
{
    WDT->CTRLA.bit.ENABLE=1;
    __DMB();
    while(WDT->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//Watchdog tiltasa.
//Ha a konfiguracio szerint alaways-on modban van, akkor ez a funkcio hatastalan
void MyWDT_disable(void)
{
    WDT->CTRLA.bit.ENABLE=0;
    __DMB();
    while(WDT->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
