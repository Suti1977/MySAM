//------------------------------------------------------------------------------
//  Sajat TCC timer/counter modul kezeles
//
//    File: MyTCC.h
//------------------------------------------------------------------------------
#ifndef MY_TCC_H_
#define MY_TCC_H_

#include "MyCommon.h"

//------------------------------------------------------------------------------
//TCC info tablazat egyes sorainak felepitese.
//Ebben a tablazatban kerulnek leirasra az alkalmazott mcu TCC hardver
//jellemzoi (baziscimek, orajek ID-k es maszkok, irq indexek, stb...
typedef struct
{
    //Az adott TCC periferie valtozoira mutat. (bazis cim)
    Tcc*         hw;
    //A TCC modul GCLK orajel azonositoja
    uint16_t    gclkId;
    //Periferia orajelenek engedelyezo regiszterenek cime
    volatile uint32_t* mclkApbMaskReg;
    //Periferia orajelenek a regiszteren beluli maszkja
    uint32_t    mclkApbMask;
    //A TCC modulhoz tartozo interruptok elso elemenek indexe.
    //Feltetelezzuk, hogy az egy TCC-hez tartozo interrupt vektorok egymas
    //utan kovetkeznek.
    uint16_t    irqn;
    //Compare/capture modulok szama (A kulonbozo TCC-knel ez mas.)
    uint8_t     ccNum;

    //Event bemenet azonosito
    //  EVSYS_ID_USER_TCCn_EV_0 +0
    //  EVSYS_ID_USER_TCCn_EV_1 +1
    //  EVSYS_ID_USER_TCCn_MC_0 +2
    //  EVSYS_ID_USER_TCCn_MC_1 +3
    //  EVSYS_ID_USER_TCCn_MC_2 +4
    //  EVSYS_ID_USER_TCCn_MC_3 +5
    //  EVSYS_ID_USER_TCCn_MC_4 +6
    //  EVSYS_ID_USER_TCCn_MC_5 +7
    uint8_t     firstEventUserID;

    //Az elso event kimenet azonosito
    //   EVSYS_ID_GEN_TCCn_OVF  +0
    //   EVSYS_ID_GEN_TCCn_TRG  +1
    //   EVSYS_ID_GEN_TCCn_CNT  +2
    //   EVSYS_ID_GEN_TCCn_MC_0 +3
    //   EVSYS_ID_GEN_TCCn_MC_1 +4
    //   EVSYS_ID_GEN_TCCn_MC_2 +5
    //   EVSYS_ID_GEN_TCCn_MC_3 +6
    //   EVSYS_ID_GEN_TCCn_MC_4 +7
    //   EVSYS_ID_GEN_TCCn_MC_5 +8
    uint8_t     firstEventGeneratorID;

} MyTCC_Info_t;
extern const MyTCC_Info_t g_MyTCC_infos[];
//------------------------------------------------------------------------------
//TCC modul inicializalasahoz hasznalt konfiguracios struktura
typedef struct
{
    //A beallitando TCC sorszama. Processzoronkent mas lehet.
    uint8_t tccNr;
    //A TCC orajelehez rendelt GCLK modul sorszama
    uint8_t gclkId;
    //A TCC-hez tartozo IRQ vonalak prioritasa
    uint32_t irqPriority;
} MyTCC_Config_t;

//------------------------------------------------------------------------------
//MyTCC valtozoi
typedef struct
{
    //A TCC periferiara mutat
    Tcc*                     hw;
    //A TCC informacios blokkjara mutat
    const MyTCC_Info_t*      info;
} MyTCC_t;
//------------------------------------------------------------------------------
//A TCC-hez tartozo event azonosito lekerdezese
//eventNr 0, vagy 1 lehet, a TCC valamelyik event bemenete
//  EVSYS_ID_USER_TCCn_EV_0 +0
//  EVSYS_ID_USER_TCCn_EV_1 +1
//  EVSYS_ID_USER_TCCn_MC_0 +2
//  EVSYS_ID_USER_TCCn_MC_1 +3
//  EVSYS_ID_USER_TCCn_MC_2 +4
//  EVSYS_ID_USER_TCCn_MC_3 +5
//  EVSYS_ID_USER_TCCn_MC_4 +6
//  EVSYS_ID_USER_TCCn_MC_5 +7
static inline uint8_t MyTCC_getEventUserID(MyTCC_t* tcc, uint8_t eventIdx)
{
     return tcc->info->firstEventUserID + eventIdx;
}
#define MYTCC_EVENT_USER_EV_0 0
#define MYTCC_EVENT_USER_EV_1 1
#define MYTCC_EVENT_USER_MC_0 2
#define MYTCC_EVENT_USER_MC_1 3
#define MYTCC_EVENT_USER_MC_2 4
#define MYTCC_EVENT_USER_MC_3 5
#define MYTCC_EVENT_USER_MC_4 6
#define MYTCC_EVENT_USER_MC_5 7
//------------------------------------------------------------------------------
//A TCC-hez tartozo elso event kimenet azonosito lekerdezese
//   EVSYS_ID_GEN_TCCn_OVF  +0
//   EVSYS_ID_GEN_TCCn_TRG  +1
//   EVSYS_ID_GEN_TCCn_CNT  +2
//   EVSYS_ID_GEN_TCCn_MC_0 +3
//   EVSYS_ID_GEN_TCCn_MC_1 +4
//   EVSYS_ID_GEN_TCCn_MC_2 +5
//   EVSYS_ID_GEN_TCCn_MC_3 +6
//   EVSYS_ID_GEN_TCCn_MC_4 +7
//   EVSYS_ID_GEN_TCCn_MC_5 +8
static inline uint8_t MyTCC_getEventGeneratorID(MyTCC_t* tcc, uint8_t eventIdx)
{
     return tcc->info->firstEventGeneratorID + eventIdx;
}
#define MYTCC_EVENT_GEN_OVF  0
#define MYTCC_EVENT_GEN_TRG  1
#define MYTCC_EVENT_GEN_CNT  2
#define MYTCC_EVENT_GEN_MC_0 3
#define MYTCC_EVENT_GEN_MC_1 4
#define MYTCC_EVENT_GEN_MC_2 5
#define MYTCC_EVENT_GEN_MC_3 6
#define MYTCC_EVENT_GEN_MC_4 7
#define MYTCC_EVENT_GEN_MC_5 8
//------------------------------------------------------------------------------
//TCC modul kezdeti inicializalasa
void MyTCC_init(MyTCC_t* tcc, const MyTCC_Config_t* config);

//TCC-hez tartozo interruptok engedelyezese az NVIC-ben.
void MyTCC_enableIrq(MyTCC_t* tcc, uint8_t irqIndex);
//TCC-hez tartozo interruptok tiltasa az NVIC-ben.
void MyTCC_disableIrq(MyTCC_t* tcc, uint8_t irqIndex);
//TCC-hez tartozo interruptok prioritasanak beallitasa.
//A rutin az osszes TCC IRQ-t a megadott szintre allitja be.
void MyTCC_setIrqPriority(MyTCC_t* tcc, uint8_t irqIndex, uint32_t level);

//TCC szamlalo ertekenek lekerdezese
uint32_t MyTCC_getCounter(MyTCC_t* tcc);
//------------------------------------------------------------------------------
////A TCC definicioit undefine-olnom kellet, mert kulonben az alabbi makroban
////a "TCC" osszeveszett a forditoval.
////#undef TCC0
////#undef TCC1
////#undef TCC2
////#undef TCC3
////#undef TCC4
////#undef TCC5
//#define XXX TCC
////TCCn
//#define MYTCC_H1(n)                 MY_EVALUATOR(XXX, n)
////_k_Handler(void)
//#define MYTCC_H2(k)                 _##k##_Handler(void)
//
////Seged makro, mellyel egyszeruen definialhato a TCC-hez hasznalando interrupt
////rutin.
////Ez kerul a kodba peldaul:
////  void TCCn_k_Handler(void)
////          ^n^k
////n: A TCC sorszama
////k: A TCC-hez tartozo IRQ vektor sorszama
//#define MYTCC_HANDLER(n, k) void MY_EVALUATOR(MYTCC_H1(n), MYTCC_H2(k))
//
//------------------------------------------------------------------------------
#endif //MY_TCC_H_
