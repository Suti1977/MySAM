//------------------------------------------------------------------------------
//  SAM tipusu mikrovezerlok alap Sercom drivere.
//
//    File: MySercom.h
//------------------------------------------------------------------------------
#ifndef MY_SERCOM_H_
#define MY_SERCOM_H_

#include <MyCommon.h>

//------------------------------------------------------------------------------
//Sercom info tablazat egyes sorainak felepitese.
//Ebben a tablazatban kerulnek leirasra az alkalmazott mcu sercomjainak hardver
//jellemzoi (baziscimek, orajek ID-k es maszkok, irq indexek, stb...
typedef struct
{
    //Az adott sercom periferia valtozoira mutat. (bazis cim)
    Sercom* hw;
    //Core orajel azonositoja
    uint16_t gclkId_Core;
    //Slow orajel azonositoja
    uint16_t gclkId_Slow;
    //Periferia orajelenek engedelyezo regiszterenek cime
    volatile uint32_t* mclkApbMaskReg;
    //Periferia orajelenek a regiszteren beluli maszkja
    uint32_t mclkApbMask;
    //A sercomhoz tartozo interruptok elso elemenek indexe.
    //Feltetelezzuk, hogy az egy sercomhoz tartozo interrupt vektorok egymas
    //utan kovetkeznek.
    uint16_t IRQn;
} MySercom_Info_t;
extern const MySercom_Info_t g_MySercom_infos[];
//------------------------------------------------------------------------------
typedef struct
{
    //A beallitando sercom sorszama. Processzoronkent mas lehet.
    uint8_t sercomNr;
    //A sercom Core orajelehez rendelt GCLK modul sorszama
    uint8_t gclkCore;
    //Core orajel frekvenciaja
    uint32_t coreFreq;
    //A sercom Slow orajelehez rendelt GCLK modul sorszama
    uint8_t gclkSlow;
    //Slow orajel frekvenciaja
    uint32_t slowFreq;
    //A sercomhoz tartozo IRQ vonalak prioritasa
    uint32_t irqPriorites;
} MySercom_Config_t;

//------------------------------------------------------------------------------
//MySercom valtozoi
typedef struct
{
    //A sercom periferiara mutat
    Sercom*     hw;

    //A sercom informacios blokkjara mutat
    const MySercom_Info_t*   info;

    //A sercom Core orajelehez rendelt GCLK modul sorszama
    uint8_t gclkCore;
    //Core orajel frekvenciaja
    uint32_t coreFreq;
    //A sercom Slow orajelehez rendelt GCLK modul sorszama
    uint8_t gclkSlow;
    //Slow orajel frekvenciaja
    uint32_t slowFreq;
} MySercom_t;
//------------------------------------------------------------------------------
//Sercom driver kezdeti inicializalasa
void MySercom_init(MySercom_t* sercom, const MySercom_Config_t* config);

//Sercom-hoz tartozo interruptok engedelyezese az NVIC-ben.
void MySercom_enableIrqs(MySercom_t* sercom);
//Sercom-hoz tartozo interruptok tiltasa az NVIC-ben.
void MySercom_disableIrqs(MySercom_t* sercom);
//Sercom-hoz tartozo interruptok prioritasanak beallitasa.
//A rutin az osszes sercom IRQ-t a megadott szintre allitja be.
void MySercom_setPriorites(MySercom_t* sercom, uint32_t level);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_SERCOM_H_
