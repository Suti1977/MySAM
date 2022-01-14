//------------------------------------------------------------------------------
//  Sajat timer/counter modul kezeles
//
//    File: MyTC.c
// Created: 2020.2.3 11:19:54
//Modified: 2020.2.3 11:19:54
//------------------------------------------------------------------------------
#include "MyTC.h"
#include <string.h>
#include "MyGCLK.h"

//------------------------------------------------------------------------------
//A lehetseges 4 TC-ra vonatkozo informacios tablazat
//Ebben a tablazatban kerulnek leirasra az alkalmazott mcu TC hardver
//jellemzoi (baziscimek, orajek ID-k es maszkok, irq indexek, stb...
#define NO_TC  {NULL, 0, NULL, 0, 0}
const MyTC_Info_t g_MyTC_infos[]=
{

    #ifdef TC0
        {TC0, TC0_GCLK_ID, &MCLK->APBAMASK.reg, MCLK_APBAMASK_TC0, TC0_IRQn, TC0_CC_NUM, EVSYS_ID_USER_TC0_EVU, EVSYS_ID_GEN_TC0_OVF},
    #else
        NO_TC,
    #endif

    #ifdef TC1
        {TC1, TC1_GCLK_ID, &MCLK->APBAMASK.reg, MCLK_APBAMASK_TC1, TC1_IRQn, TC1_CC_NUM, EVSYS_ID_USER_TC1_EVU, EVSYS_ID_GEN_TC1_OVF},
    #else
        NO_TC,
    #endif

    #ifdef TC2
        {TC2, TC2_GCLK_ID, &MCLK->APBBMASK.reg, MCLK_APBBMASK_TC2, TC2_IRQn, TC2_CC_NUM, EVSYS_ID_USER_TC2_EVU, EVSYS_ID_GEN_TC2_OVF},
    #else
        NO_TC,
    #endif

    #ifdef TC3
        {TC3, TC3_GCLK_ID, &MCLK->APBBMASK.reg, MCLK_APBBMASK_TC3, TC3_IRQn, TC3_CC_NUM, EVSYS_ID_USER_TC3_EVU, EVSYS_ID_GEN_TC3_OVF},
    #else
        NO_TC,
    #endif

    #ifdef TC4
        {TC4, TC4_GCLK_ID, &MCLK->APBCMASK.reg, MCLK_APBCMASK_TC4, TC4_IRQn, TC4_CC_NUM, EVSYS_ID_USER_TC4_EVU, EVSYS_ID_GEN_TC4_OVF},
    #else
        NO_TC,
    #endif

    #ifdef TC5
        {TC5, TC5_GCLK_ID, &MCLK->APBCMASK.reg, MCLK_APBCMASK_TC5, TC5_IRQn, TC5_CC_NUM, EVSYS_ID_USER_TC5_EVU, EVSYS_ID_GEN_TC5_OVF},
    #else
        NO_TC,
    #endif

    #ifdef TC6
        {TC6, TC6_GCLK_ID, &MCLK->APBDMASK.reg, MCLK_APBDMASK_TC6, TC6_IRQn, TC6_CC_NUM, EVSYS_ID_USER_TC6_EVU, EVSYS_ID_GEN_TC6_OVF},
    #else
        NO_TC,
    #endif

    #ifdef TC7
        {TC7, TC7_GCLK_ID, &MCLK->APBDMASK.reg, MCLK_APBDMASK_TC7, TC7_IRQn, TC7_CC_NUM, EVSYS_ID_USER_TC7_EVU, EVSYS_ID_GEN_TC7_OVF},
    #else
        NO_TC,
    #endif

};
//------------------------------------------------------------------------------
//TC modul kezdeti inicializalasa
void MyTC_init(MyTC_t* tc, const MyTC_Config_t* config)
{
    //Modul valtozoinak kezdeti torlese.
    memset(tc, 0, sizeof(MyTC_t));

    uint8_t index=config->tcNr;

    //Ha ez az assert feljon, akkor a hivatkozott TC nem letezik
    ASSERT(index<ARRAY_SIZE(g_MyTC_infos));

    const MyTC_Info_t* tcInfo=&g_MyTC_infos[index];
    //Ha ez az assert feljon, akkor a hivatkozott TC nem letezik
    ASSERT(tcInfo->hw);

    //A leiroban beallitja az alkalmazott TC baziscimet.
    //A kesobbiekben a driverek (cc, pwm, ...) ezen keresztul erik el.
    tc->hw=tcInfo->hw;

    tc->info=tcInfo;

    //A TC modul MCLK orajelenek engedelyezese
    MY_ENTER_CRITICAL();
    *tcInfo->mclkApbMaskReg |= tcInfo->mclkApbMask;
    MY_LEAVE_CRITICAL();

    //A TC modul orajeleinek bekotese a GCLK modulok valamelyikehez,
    //a configok alapjan.
    MyGclk_enablePeripherialClock(tcInfo->gclkId, config->gclkId);

    //Sercomhoz tartozo IRQ vonalak prioritasanak beallitasa
    MyTC_setIrqPriority(tc, config->irqPriority);
}
//------------------------------------------------------------------------------
//TC-hoz tartozo interruptok engedelyezese az NVIC-ben.
void MyTC_enableIrq(MyTC_t* tc)
{
    NVIC_ClearPendingIRQ(tc->info->irqn);
    NVIC_EnableIRQ(tc->info->irqn);
}
//------------------------------------------------------------------------------
//TC-hoz tartozo interruptok tiltasa az NVIC-ben.
void MyTC_disableIrq(MyTC_t* tc)
{
    NVIC_DisableIRQ(tc->info->irqn);
}
//------------------------------------------------------------------------------
//TC-hoz tartozo interruptok prioritasanak beallitasa.
//A rutin az osszes TC IRQ-t a megadott szintre allitja be.
void MyTC_setIrqPriority(MyTC_t* tc, uint32_t level)
{
    NVIC_SetPriority(tc->info->irqn, level);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
