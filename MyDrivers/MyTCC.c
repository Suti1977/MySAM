//------------------------------------------------------------------------------
//  Sajat TCC timer/counter modul kezeles
//
//    File: MyTCC.c
//------------------------------------------------------------------------------
#include "MyTCC.h"
#include <string.h>
#include "MyGCLK.h"
//------------------------------------------------------------------------------
//A lehetseges 5 TCC-ra vonatkozo informacios tablazat
//Ebben a tablazatban kerulnek leirasra az alkalmazott mcu TCC hardver
//jellemzoi (baziscimek, orajek ID-k es maszkok, irq indexek, stb...
#define NO_TCC {NULL, 0, NULL, 0, 0, 0, 0, 0}
const MyTCC_Info_t g_MyTCC_infos[]=
{
    #ifdef TCC0
        {TCC0, TCC0_GCLK_ID, &MCLK->APBBMASK.reg, MCLK_APBBMASK_TCC0, TCC0_0_IRQn, TCC0_CC_NUM, EVSYS_ID_USER_TCC0_EV_0, EVSYS_ID_GEN_TCC0_OVF},
    #else
        NO_TCC,
    #endif

    #ifdef TCC1
        {TCC1, TCC1_GCLK_ID, &MCLK->APBBMASK.reg, MCLK_APBBMASK_TCC1, TCC1_0_IRQn, TCC1_CC_NUM, EVSYS_ID_USER_TCC1_EV_0, EVSYS_ID_GEN_TCC1_OVF},
    #else
        NO_TCC,
    #endif

    #ifdef TCC2
        {TCC2, TCC2_GCLK_ID, &MCLK->APBCMASK.reg, MCLK_APBCMASK_TCC2, TCC2_0_IRQn, TCC2_CC_NUM, EVSYS_ID_USER_TCC2_EV_0, EVSYS_ID_GEN_TCC2_OVF},
    #else
        NO_TCC,
    #endif

    #ifdef TCC3
        {TCC3, TCC3_GCLK_ID, &MCLK->APBCMASK.reg, MCLK_APBCMASK_TCC3, TCC3_0_IRQn, TCC3_CC_NUM, EVSYS_ID_USER_TCC3_EV_0, EVSYS_ID_GEN_TCC3_OVF},
    #else
        NO_TCC,
    #endif

    #ifdef TCC4
        {TCC4, TCC4_GCLK_ID, &MCLK->APBDMASK.reg, MCLK_APBDMASK_TCC4, TCC4_0_IRQn, TCC4_CC_NUM, EVSYS_ID_USER_TCC4_EV_0, EVSYS_ID_GEN_TCC4_OVF},
    #else
        NO_TCC,
    #endif
};
//------------------------------------------------------------------------------
//TCC modul kezdeti inicializalasa
void MyTCC_init(MyTCC_t* tcc, const MyTCC_Config_t* config)
{
    //Modul valtozoinak kezdeti torlese.
    memset(tcc, 0, sizeof(MyTCC_t));

    uint8_t index=config->tccNr;

    //Ha ez az assert feljon, akkor a hivatkozott tccnem letezik
    ASSERT(index<ARRAY_SIZE(g_MyTCC_infos));

    const MyTCC_Info_t* tccInfo=&g_MyTCC_infos[index];
    //Ha ez az assert feljon, akkor a hivatkozott tccnem letezik
    ASSERT(tccInfo->hw);

    //A leiroban beallitja az alkalmazott tccbaziscimet.
    //A kesobbiekben a driverek (cc, pwm, ...) ezen keresztul erik el.
    tcc->hw=tccInfo->hw;

    tcc->info=tccInfo;

    //A tccmodul MCLK orajelenek engedelyezese
    MY_ENTER_CRITICAL();
    *tccInfo->mclkApbMaskReg |= tccInfo->mclkApbMask;
    MY_LEAVE_CRITICAL();

    //A tccmodul orajeleinek bekotese a GCLK modulok valamelyikehez,
    //a configok alapjan.
    MyGclk_setPeripherialClock(tccInfo->gclkId, config->gclkId);

    //Sercomhoz tartozo IRQ vonalak prioritasanak beallitasa
    //(1-el tobb vonala van, mint amennyi Compare/match modulja.
    for (uint8_t i=0; i<=tccInfo->ccNum; i++)
    {
        MyTCC_setIrqPriority(tcc, i, config->irqPriority);
    }

    //modul resetelese...
    Tcc* hw=tcc->hw;
    hw->CTRLA.reg=TCC_CTRLA_SWRST;
    while(hw->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//TCC-hoz tartozo interruptok engedelyezese az NVIC-ben.
void MyTCC_enableIrq(MyTCC_t* tcc, uint8_t irqIndex)
{
    NVIC_ClearPendingIRQ(tcc->info->irqn+irqIndex);
    NVIC_EnableIRQ(tcc->info->irqn+irqIndex);
}
//------------------------------------------------------------------------------
//TCC-hoz tartozo interruptok tiltasa az NVIC-ben.
void MyTCC_disableIrq(MyTCC_t* tcc, uint8_t irqIndex)
{
    NVIC_DisableIRQ(tcc->info->irqn+irqIndex);
}
//------------------------------------------------------------------------------
//TCC-hoz tartozo interruptok prioritasanak beallitasa.
//A rutin az osszes tccIRQ-t a megadott szintre allitja be.
void MyTCC_setIrqPriority(MyTCC_t* tcc, uint8_t irqIndex, uint32_t level)
{
    NVIC_SetPriority(tcc->info->irqn+irqIndex, level);
}
//------------------------------------------------------------------------------
//TCC szamlalo ertekenek lekerdezese
uint32_t MyTCC_getCounter(MyTCC_t* tcc)
{
    Tcc* hw=tcc->hw;
    hw->CTRLBSET.reg=TCC_CTRLBSET_CMD_READSYNC;
    while(hw->SYNCBUSY.reg);
    return hw->COUNT.reg;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
