//------------------------------------------------------------------------------
//  RTC driver
//
//    File: MyRTC.h
//------------------------------------------------------------------------------
#ifndef MYRTC_H_
#define MYRTC_H_

#include "MyCommon.h"
//------------------------------------------------------------------------------
//RTC inializalasanal hasznalt konfiguracios struktura
typedef struct
{
    //ha true, akkor initkor az RTC engedelyezve is lesz
    bool enable;

    //Eloosztas merteke
    //RTC_MODE0_CTRLA_PRESCALER_OFF_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV1_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV2_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV4_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV8_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV16_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV32_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV64_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV128_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV256_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV512_Val
    //RTC_MODE0_CTRLA_PRESCALER_DIV1024_Val
    uint32_t prescaler;

    //RTC uzemmod (0, 1, 2)
    //RTC_MODE0_CTRLA_MODE_COUNT32_Val  Mode 0: 32-bit Counter
    //RTC_MODE0_CTRLA_MODE_COUNT16_Val  Mode 1: 16-bit Counter
    //RTC_MODE0_CTRLA_MODE_CLOCK_Val    Mode 2: Clock/Calendar
    uint32_t mode;

    //The COUNT register requires synchronization when reading.
    //Disabling the synchronization will prevent reading valid
    //values from the COUNT register.
    bool countSync;
    //Ha true, akkor match eseten reseteli a countert
    bool matchClear;
    //Ha true, akkor tamper eseten a backup (BK) regisztereket torli
    bool bktRst;
    //Ha true, akkor tamper eseten az altalanos regisztereket (GP) torli
    bool gptRst;


    //EVCTRL regiszterbe irando bitek.
    RTC_MODE0_EVCTRL_Type evctrl;
} MyRTC_config_t;
//------------------------------------------------------------------------------
//APB busz orajel engedelyezese
static inline void MyRTC_enableClock(void)
{
    MCLK->APBAMASK.bit.RTC_=1;
}
//APB busz orajel tiltasa
static inline void MyRTC_disbaleClock(void)
{
    MCLK->APBAMASK.bit.RTC_=0;
}

//RTC periferia inicializalasa
bool MyRTC_init(const MyRTC_config_t* cfg, bool skipIfInited);

//RTC engedelyezese
void MyRTC_enable(Rtc* hw);

//RTC tiltasa
void MyRTC_disable(Rtc* hw);

//32 bites masoderc szamlalo lekerdezese
uint32_t MyRTC_mode0_getCounter(void);

//32 bites masodperc szamlalo modositasa
void MyRTC_mode0_setCounter(uint32_t newCounterValue);

//32 bites compare0 regiszter lekerdezese
uint32_t MyRTC_mode0_getCompare0(void);

//32 bites compare0 regiszter beallitasa
void MyRTC_mode0_setCompare0(uint32_t newCompareValue);

//32 bites compare1 regiszter lekerdezese
uint32_t MyRTC_mode0_getCompare1(void);

//32 bites compare1 regiszter beallitasa
void MyRTC_mode0_setCompare1(uint32_t newCompareValue);
//------------------------------------------------------------------------------
#endif //MYRTC_H_
