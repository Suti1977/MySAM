//------------------------------------------------------------------------------
//  Sajat timer/counter modul kezeles
//
//    File: MyTC.h
//------------------------------------------------------------------------------
#ifndef MY_TC_H_
#define MY_TC_H_

#include "MyCommon.h"

//------------------------------------------------------------------------------
//TC info tablazat egyes sorainak felepitese.
//Ebben a tablazatban kerulnek leirasra az alkalmazott mcu TC hardver
//jellemzoi (baziscimek, orajek ID-k es maszkok, irq indexek, stb...
typedef struct
{
    //Az adott TC periferie valtozoira mutat. (bazis cim)
    Tc*         hw;
    //A TC modul GCLK orajel azonositoja
    uint16_t    gclkId;
    //Periferia orajelenek engedelyezo regiszterenek cime
    volatile uint32_t* mclkApbMaskReg;
    //Periferia orajelenek a regiszteren beluli maszkja
    uint32_t    mclkApbMask;
    //A TC modulhoz tartozo interruptok elso elemenek indexe.
    //Feltetelezzuk, hogy az egy TC-hez tartozo interrupt vektorok egymas
    //utan kovetkeznek.
    uint16_t    irqn;

    //Compare/capture modulok szama (A kulonbozo TC-knel ez mas lehet.)
    uint8_t     ccNum;

    //Event bemenet azonosito
    uint8_t     eventUserID;

    //Az elso event kimenet azonosito
    //   EVSYS_ID_GEN_TCCn_OVF  +0
    //   EVSYS_ID_GEN_TCCn_MC_0 +1
    //   EVSYS_ID_GEN_TCCn_MC_1 +2
    uint8_t     firstEventGeneratorID;
} MyTC_Info_t;
extern const MyTC_Info_t g_MyTC_infos[];
//------------------------------------------------------------------------------
//TC modul inicializalasahoz hasznalt konfiguracios struktura
typedef struct
{
    //A beallitando TC sorszama. Processzoronkent mas lehet.
    uint8_t tcNr;
    //A TC orajelehez rendelt GCLK modul sorszama
    uint8_t gclkId;
    //A TC-hez tartozo IRQ vonalak prioritasa
    uint32_t irqPriority;
} MyTC_Config_t;

//------------------------------------------------------------------------------
//MyTC valtozoi
typedef struct
{
    //A TC periferiara mutat
    Tc*                     hw;
    //A TC informacios blokkjara mutat
    const MyTC_Info_t*      info;   
} MyTC_t;
//------------------------------------------------------------------------------
//A TC-hez tartozo event bemenet azonosito lekerdezese
static inline uint8_t MyTC_getEventUserID(MyTC_t* tc)
{
     return tc->info->eventUserID;
}
//------------------------------------------------------------------------------
//A TCC-hez tartozo event generator azonosito lekerdezese
//   EVSYS_ID_GEN_TCn_OVF  +0
//   EVSYS_ID_GEN_TCn_MC_0 +1
//   EVSYS_ID_GEN_TCn_MC_1 +2
#define MYTC_EVENT_GEN_OVF  0
#define MYTC_EVENT_GEN_MC_0 1
#define MYTC_EVENT_GEN_MC_1 2
static inline uint8_t MyTC_getEventGeneratorID(MyTC_t* tc, uint8_t eventIdx)
{
     return tc->info->firstEventGeneratorID + eventIdx;
}
//------------------------------------------------------------------------------
//TC modul kezdeti inicializalasa
void MyTC_init(MyTC_t* tc, const MyTC_Config_t* config);

//TC-hoz tartozo interruptok engedelyezese az NVIC-ben.
void MyTC_enableIrq(MyTC_t* tc);
//TC-hoz tartozo interruptok tiltasa az NVIC-ben.
void MyTC_disableIrq(MyTC_t* tc);
//TC-hoz tartozo interruptok prioritasanak beallitasa.
//A rutin az osszes TC IRQ-t a megadott szintre allitja be.
void MyTC_setIrqPriority(MyTC_t* tc, uint32_t level);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_TC_H_
