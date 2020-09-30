//------------------------------------------------------------------------------
//  Kulso interrupt (EXTINT) kezelest segito driver 
//
//    File: MyEIC.h
//------------------------------------------------------------------------------
#ifndef MYEIC_H_
#define MYEIC_H_

#include "MyCommon.h"

//------------------------------------------------------------------------------
//EIC konfiguracios struktura, melyet init-kor hasznalunk
typedef struct
{
    //Az NVIC periferiat hajto orajel forras kijelolese
    // 0 - GCLK_EIC
    // 1 - CLK_ULP32K
    uint8_t clkSelect;

    //Az EIC-hez hasznalt GCLK periferia sorszama. Csak akkor, ha a clkSelect
    //nem 0.
    uint8_t gclkNr;

    //DPRESCALER.TICKON
    //Ha 0, akkor a GCLK-rol megy a bemenetek pergesmentesitese.
    //Ha 1, akkor az LP32kHz orajelrol
    uint8_t bounceSamplerClkSource;

    //DPRESCALER.PRESCALERx
    //A pergesmentesites orajelenek elooszatasa.
    // 0- F/2
    // 1- F/4
    // 2- F/8
    // 3- F/16
    // 4- F/32
    // 5- F/64
    // 6- F/128
    // 7- F/256
    uint8_t bouncePrescaler;

    //DPRESCALER.STATESx
    //A pergesmentesitesnel a mintazott allapotok szama. Ennyi alapjan keszit
    //tobbsegi dontest.
    //Ha 0, akkor 3 minta alapjan
    //Ha 1, akkor 7 minta alapjan
    uint8_t bounceSampleCount;
}MyEIC_Config_t;
//------------------------------------------------------------------------------
//EIC csatornakat inicializalo konfiguracios struktura
typedef struct
{
    //Ha true, akkor az EVENT kimenet engedelyezett lesz a csatornahoz
    bool eventOutEnable;

    //Async bemeneti mod engedelyezese. (Peldaul sleep-bol valo ebresztes
    //lehetseges ilyenkor a csatornarol.)
    bool asyncEnable;

    //Az adott csatornan a pergesmentesites engedelyezese
    bool debounceEnable;

    //filter engedelyezese a csatornan
    bool filterEnable;

    //Bemenet kezelesi mod beallitasa
    // EIC_CONFIG_SENSE0_NONE_Val      _U_(0x0)   No detection
    // EIC_CONFIG_SENSE0_RISE_Val      _U_(0x1)   Rising edge detection
    // EIC_CONFIG_SENSE0_FALL_Val      _U_(0x2)   Falling edge detection
    // EIC_CONFIG_SENSE0_BOTH_Val      _U_(0x3)   Both edges detection
    // EIC_CONFIG_SENSE0_HIGH_Val      _U_(0x4)   High level detection
    // EIC_CONFIG_SENSE0_LOW_Val       _U_(0x5)   Low level detection
    uint8_t inputSense;

    //ha true, akkor engedelyezni fogja az NVIC-ben a csatornat
    bool enableInNVIC;
    //Ha engedelyezesre kerul az NVIC-ben, akkor az IRQ prioritas
    uint32_t interruptPriority;

} MyEIC_ChannelConfig_t;

//------------------------------------------------------------------------------
//EIC kezdeti inicializalasa
//Az inicializacio nem engedelyezi az EIC-et. Azt a csatornak beallitasa utan
//kell az applikacioban megoldani, a MyEIC_enable() segitsegevel.
void MyEIC_init(const MyEIC_Config_t* cfg);

//Egy kulso interrupt vonal konfiguralasa
//Csak akkor lehetseges, ha az NVIC tiltott allapotu!
void MyEIC_configChannel(uint8_t channel, const MyEIC_ChannelConfig_t* cfg);

//Alapertelemezett csatorna beallitasokkal feltolti az atadott strukturat
void MyEIC_getDefaultChannelConfig(MyEIC_ChannelConfig_t* cfg);

//megszakitasi  vonal engedelyezese
static inline void MyEIC_enableExtInt(uint8_t channel)
{
    EIC->INTENSET.reg = 1 << channel;
}

//megszakitasi vonal tiltasa
static inline void MyEIC_disableExtInt(uint8_t channel)
{
    EIC->INTENCLR.reg = 1 << channel;
}

//megszakitasi flag torlese
static inline void MyEIC_clearExtIntFlag(uint8_t channel)
{
    EIC->INTFLAG.reg = 1 << channel;
}

//------------------------------------------------------------------------------
//EIC engedelyezese
static inline void MyEIC_enable(void)
{
    Eic* eic=EIC;
    eic->CTRLA.bit.ENABLE=1; __DSB();
    while(eic->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//EIC tiltasa
static inline void MyEIC_disable(void)
{
    Eic* eic=EIC;
    eic->CTRLA.bit.ENABLE=0; __DSB();
    while(eic->SYNCBUSY.reg);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYEIC_H_
