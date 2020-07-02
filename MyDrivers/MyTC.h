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
