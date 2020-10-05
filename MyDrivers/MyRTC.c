//------------------------------------------------------------------------------
//  RTC driver
//
//    File: MyRTC.c
//------------------------------------------------------------------------------
#include "MyRTC.h"
#include <string.h>


//------------------------------------------------------------------------------
static void MyRTC_waitForSync(Rtc* hw);
//------------------------------------------------------------------------------
//RTC periferia inicializalasa
bool MyRTC_init(const MyRTC_config_t* cfg, bool skipIfInited)
{
    Rtc* hw=RTC;

    //RTC-nek APB busz orajel engedelyezese.
    MyRTC_enableClock();

    //Ellenorzes, hogy mindenkepen inicializalni kell-e...
    //Csak akkor inicializalunk, ha meg nem fut az rtc
    bool enabled=(hw->MODE0.CTRLA.bit.ENABLE==1);

    if (enabled && (skipIfInited))
    {   //Az RTC mar mukodik, es ebben az esetben nem kell modositani
        return enabled;
    }


    //RTC tiltasa a konfiguracio elott
    MyRTC_disable(hw);
    //RTC szoftveres reset
    hw->MODE0.CTRLA.bit.SWRST=1;
    __DMB();
    while(hw->MODE0.CTRLA.bit.SWRST);
    MyRTC_waitForSync(hw);

    //Uzemmod kijelolese. (Mindegyik modban azonosak a bitek)
    hw->MODE0.CTRLA.reg=RTC_MODE0_CTRLA_MODE(cfg->mode) |
                        RTC_MODE0_CTRLA_PRESCALER(cfg->prescaler) |
                        (cfg->bktRst ? RTC_MODE0_CTRLA_BKTRST : 0) |
                        (cfg->gptRst ? RTC_MODE0_CTRLA_GPTRST : 0) |
                        (cfg->gptRst ? RTC_MODE0_CTRLA_GPTRST : 0) |
                        (cfg->countSync ? RTC_MODE0_CTRLA_COUNTSYNC : 0) |
                        (cfg->matchClear ? RTC_MODE0_CTRLA_MATCHCLR : 0) |
                        0;
    MyRTC_waitForSync(hw);

    //Event control regiszter beallitasa
    hw->MODE0.EVCTRL=cfg->evctrl;
    MyRTC_waitForSync(hw);

    if (cfg->enable) MyRTC_enable(hw);

    return true;
}
//------------------------------------------------------------------------------
static void MyRTC_waitForSync(Rtc* hw)
{
    __DMB();
    while(hw->MODE0.SYNCBUSY.bit.COUNT);
}
//------------------------------------------------------------------------------
//32 bites masoderc szamlalo lekerdezese
uint32_t MyRTC_mode0_getCounter(void)
{
    Rtc* hw=RTC;
    while(hw->MODE0.SYNCBUSY.bit.COUNT);
    return hw->MODE0.COUNT.reg;
}
//------------------------------------------------------------------------------
//32 bites masodperc szamlalo modositasa
void MyRTC_mode0_setCounter(uint32_t newCounterValue)
{
    Rtc* hw=RTC;
    hw->MODE0.COUNT.reg=newCounterValue;
    __DMB();
    while(hw->MODE0.SYNCBUSY.bit.COUNT);
}
//------------------------------------------------------------------------------
//32 bites compare0 regiszter lekerdezese
uint32_t MyRTC_mode0_getCompare0(void)
{
    Rtc* hw=RTC;
    while(hw->MODE0.SYNCBUSY.bit.COMP0);
    return hw->MODE0.COMP[0].reg;
}
//------------------------------------------------------------------------------
//32 bites compare0 regiszter beallitasa
void MyRTC_mode0_setCompare0(uint32_t newCompareValue)
{
    Rtc* hw=RTC;
    hw->MODE0.COMP[0].reg=newCompareValue;
    __DMB();
    while(hw->MODE0.SYNCBUSY.bit.COMP0);
}
//------------------------------------------------------------------------------
//32 bites compare1 regiszter lekerdezese
uint32_t MyRTC_mode0_getCompare1(void)
{
    Rtc* hw=RTC;
    while(hw->MODE0.SYNCBUSY.bit.COMP1);
    return hw->MODE0.COMP[1].reg;
}
//------------------------------------------------------------------------------
//32 bites compare1 regiszter beallitasa
void MyRTC_mode0_setCompare1(uint32_t newCompareValue)
{
    Rtc* hw=RTC;
    hw->MODE0.COMP[1].reg=newCompareValue;
    __DMB();
    while(hw->MODE0.SYNCBUSY.bit.COMP1);
}
//------------------------------------------------------------------------------

//RTC engedelyezese
void MyRTC_enable(Rtc* hw)
{
    hw->MODE0.CTRLA.bit.ENABLE=1;
    __DMB();
    while(hw->MODE0.SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//RTC tiltasa
void MyRTC_disable(Rtc* hw)
{
    hw->MODE0.CTRLA.bit.ENABLE=0;
    __DMB();
    while(hw->MODE0.SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
