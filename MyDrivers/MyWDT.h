//------------------------------------------------------------------------------
//  Watchdog kezelo driver
//
//    File: MyWDT.h
//------------------------------------------------------------------------------
#ifndef MYWDT_H_
#define MYWDT_H_

#include "MyCommon.h"
//------------------------------------------------------------------------------
//Watchdog inicializalasanal hasznalando konfiguracios struktura
typedef struct
{
    //ha true, akkor a konfiguracio utan azonnal engedelyezi is a watchdogot
    bool enable;

    //Watchdog mindig fut. Nem lehet tobbet leallitani. A konfiguracioja
    //nem modosithato. A regiszterei csak olvashatok.
    bool alwaysOn;
    //Ablakos mukodesi mod engedelyezese
    bool windowMode;

    //Normal modban a watchdog idozites hossza
    //Ablakos modban a nyitott ablak hossza
    //These bits determine the watchdog time-out period as a number of 1.024kHz
    //CLK_WDTOSC clock cycles. In Window mode operation, these bits define the
    //open window period.
    //WDT_CONFIG_PER_CYC8_Val     8 clock cycles
    //WDT_CONFIG_PER_CYC16_Val    16 clock cycles
    //WDT_CONFIG_PER_CYC32_Val    32 clock cycles
    //WDT_CONFIG_PER_CYC64_Val    64 clock cycles
    //WDT_CONFIG_PER_CYC128_Val   128 clock cycles
    //WDT_CONFIG_PER_CYC256_Val   256 clock cycles
    //WDT_CONFIG_PER_CYC512_Val   512 clock cycles
    //WDT_CONFIG_PER_CYC1024_Val  1024 clock cycles
    //WDT_CONFIG_PER_CYC2048_Val  2048 clock cycles
    //WDT_CONFIG_PER_CYC4096_Val  4096 clock cycles
    //WDT_CONFIG_PER_CYC8192_Val  8192 clock cycles
    //WDT_CONFIG_PER_CYC16384_Va  16384 clock cycles
    uint32_t period;

    //Ablakos modban a zart ablak hossza
    //In Window mode, these bits determine the watchdog closed window period as
    //a number of cycles of the 1.024kHz
    //WDT_CONFIG_WINDOW_CYC8_Val      8 clock cycles
    //WDT_CONFIG_WINDOW_CYC16_Val     16 clock cycles
    //WDT_CONFIG_WINDOW_CYC32_Val     32 clock cycles
    //WDT_CONFIG_WINDOW_CYC64_Val     64 clock cycles
    //WDT_CONFIG_WINDOW_CYC128_Val    128 clock cycles
    //WDT_CONFIG_WINDOW_CYC256_Val    256 clock cycles
    //WDT_CONFIG_WINDOW_CYC512_Val    512 clock cycles
    //WDT_CONFIG_WINDOW_CYC1024_Val   1024 clock cycles
    //WDT_CONFIG_WINDOW_CYC2048_Val   2048 clock cycles
    //WDT_CONFIG_WINDOW_CYC4096_Val   4096 clock cycles
    //WDT_CONFIG_WINDOW_CYC8192_Val   8192 clock cycles
    //WDT_CONFIG_WINDOW_CYC16384_Val  16384 clock cycles
    uint32_t window;


    //korai jelzes megszakitas engedelyezese.
    bool earlyWarning;
    //korai jelzes idopontja
    //WDT_EWCTRL_EWOFFSET_CYC8_Val      8 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC16_Val     16 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC32_Val     32 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC64_Val     64 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC128_Val    128 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC256_Val    256 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC512_Val    512 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC1024_Val   1024 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC2048_Val   2048 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC4096_Val   4096 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC8192_Val   8192 clock cycles
    //WDT_EWCTRL_EWOFFSET_CYC16384_Val  16384 clock cycles
    uint32_t earlyWarningOffset;

} MyWDT_config_t;
//------------------------------------------------------------------------------
// kezdeti inicializalasa
void MyWDT_init(const MyWDT_config_t* cfg);

//Watchdog engedelyezese
void MyWDT_enable(void);

//Watchdog tiltasa.
//Ha a konfiguracio szerint alaways-on modban van, akkor ez a funkcio hatastalan
void MyWDT_disable(void);
//------------------------------------------------------------------------------
//Watchdog torlese
static inline MyWDT_clear(void)
{
    WDT->CLEAR.reg=0xA5;
    __DMB();
    while(WDT->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
#endif //MYWDT_H_
