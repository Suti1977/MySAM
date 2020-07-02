//------------------------------------------------------------------------------
//  SAM tipusu mikrovezerlok alap Sercom drivere.
//
//    File: MySercom.c
//------------------------------------------------------------------------------
#include <string.h>
#include "MySercom.h"
#include "sam.h"
#include "assert.h"
#include "MyHelpers.h"
#include "MyAtomic.h"
#include "MyGCLK.h"

//Az alkalmazott SAM MCU-n egy-egy sercomhoz ennyi IRQ vonal tartozik
#define SERCOM_IRQ_COUNT    4
//------------------------------------------------------------------------------
//A lehetseges 8 sercom-ra vonatkozo informacios tablazat
//Ebben a tablazatban kerulnek leirasra az alkalmazott mcu sercomjainak hardver
//jellemzoi (baziscimek, orajek ID-k es maszkok, irq indexek, stb...
#define NO_SERCOM  {NULL, 0, 0, NULL, 0, 0}
const MySercom_Info_t g_MySercom_infos[]=
{

    #ifdef SERCOM0
        {SERCOM0, SERCOM0_GCLK_ID_CORE, SERCOM0_GCLK_ID_SLOW, &MCLK->APBAMASK.reg, MCLK_APBAMASK_SERCOM0, SERCOM0_0_IRQn},
    #else
        NO_SERCOM,
    #endif

    #ifdef SERCOM1
        {SERCOM1, SERCOM1_GCLK_ID_CORE, SERCOM1_GCLK_ID_SLOW, &MCLK->APBAMASK.reg, MCLK_APBAMASK_SERCOM1, SERCOM1_0_IRQn},
    #else
        NO_SERCOM,
    #endif

    #ifdef SERCOM2
        {SERCOM2, SERCOM2_GCLK_ID_CORE, SERCOM2_GCLK_ID_SLOW, &MCLK->APBBMASK.reg, MCLK_APBBMASK_SERCOM2, SERCOM2_0_IRQn},
    #else
        NO_SERCOM,
    #endif

    #ifdef SERCOM3
        {SERCOM3, SERCOM3_GCLK_ID_CORE, SERCOM3_GCLK_ID_SLOW, &MCLK->APBBMASK.reg, MCLK_APBBMASK_SERCOM3, SERCOM3_0_IRQn},
    #else
        NO_SERCOM,
    #endif

    #ifdef SERCOM4
        {SERCOM4, SERCOM4_GCLK_ID_CORE, SERCOM4_GCLK_ID_SLOW, &MCLK->APBDMASK.reg, MCLK_APBDMASK_SERCOM4, SERCOM4_0_IRQn},
    #else
        NO_SERCOM,
    #endif

    #ifdef SERCOM5
        {SERCOM5, SERCOM5_GCLK_ID_CORE, SERCOM5_GCLK_ID_SLOW, &MCLK->APBDMASK.reg, MCLK_APBDMASK_SERCOM5, SERCOM5_0_IRQn},
    #else
        NO_SERCOM,
    #endif

    #ifdef SERCOM6
        {SERCOM6, SERCOM6_GCLK_ID_CORE, SERCOM6_GCLK_ID_SLOW, &MCLK->APBDMASK.reg, MCLK_APBDMASK_SERCOM6, SERCOM6_0_IRQn},
    #else
        NO_SERCOM,
    #endif

    #ifdef SERCOM7
        {SERCOM7, SERCOM7_GCLK_ID_CORE, SERCOM7_GCLK_ID_SLOW, &MCLK->APBDMASK.reg, MCLK_APBDMASK_SERCOM7, SERCOM7_0_IRQn},
    #else
        NO_SERCOM,
    #endif
};
//------------------------------------------------------------------------------
//Sercom driver kezdeti inicializalasa
void MySercom_init(MySercom_t* sercom, const MySercom_Config_t* config)
{
    //Modul valtozoinak kezdeti torlese.
    memset(sercom, 0, sizeof(MySercom_t));

    uint8_t index=config->sercomNr;

    //Ha ez az assert feljon, akkor a hivatkozott sercom nem letezik
    assert(index<ARRAY_SIZE(g_MySercom_infos));


    const MySercom_Info_t* sercomInfo=&g_MySercom_infos[index];
    //Ha ez az assert feljon, akkor a hivatkozott sercom nem letezik
    assert(sercomInfo->hw);

    //A leiroban beallitja az alkalmazott sercom baziscimet.
    //A kesobbiekben a driverek (SPI, UART, I2C,...) ezen keresztul erik el.
    sercom->hw=sercomInfo->hw;

    sercom->info=sercomInfo;

    //A Sercom MCLK orajelenek engedelyezese
    MY_ENTER_CRITICAL();
    *sercomInfo->mclkApbMaskReg |= sercomInfo->mclkApbMask;
    MY_LEAVE_CRITICAL();

    //A Sercom Core es Slow orajeleinek bekotese a GCLK modulok valamelyikehez,
    //a configok alapjan.
    MyGclk_setPeripherialClock(sercomInfo->gclkId_Core, config->gclkCore);
    MyGclk_setPeripherialClock(sercomInfo->gclkId_Slow, config->gclkSlow);

    //Sercomhoz tartozo IRQ vonalak prioritasanak beallitasa
    MySercom_setPriorites(sercom, config->irqPriorites);
}
//------------------------------------------------------------------------------
//Sercom-hoz tartozo interruptok engedelyezese az NVIC-ben.
void MySercom_enableIrqs(MySercom_t* sercom)
{
    uint32_t irqn;

    //Az adaott sercomhoz tartozo elso IRQ vonal sorszama
    irqn=sercom->info->IRQn;

    //Feltetelezzuk, hogy egy SERCOM-hoz tartozo interruptok egymas utan
    //sorban kovetkeznek, es egy sercom-hoz 4 darab tartozik.
    for(uint8_t i=0; i<SERCOM_IRQ_COUNT; i++)
    {
        NVIC_ClearPendingIRQ(irqn);
        NVIC_EnableIRQ(irqn);
        irqn++;
    }
}
//------------------------------------------------------------------------------
//Sercom-hoz tartozo interruptok tiltasa az NVIC-ben.
void MySercom_disableIrqs(MySercom_t* sercom)
{
    uint32_t irqn;

    //Az adaott sercomhoz tartozo elso IRQ vonal sorszama
    irqn=sercom->info->IRQn;

    //Feltetelezzuk, hogy egy SERCOM-hoz tartozo interruptok egymas utan
    //sorban kovetkeznek, es egy sercom-hoz 4 darab tartozik.
    for(uint8_t i=0; i<SERCOM_IRQ_COUNT; i++)
    {
        NVIC_DisableIRQ(irqn);
        irqn++;
    }
}
//------------------------------------------------------------------------------
//Sercom-hoz tartozo interruptok prioritasanak beallitasa.
//A rutin az osszes sercom IRQ-t a megadott szintre allitja be.
void MySercom_setPriorites(MySercom_t* sercom, uint32_t level)
{
    uint32_t irqn;

    //Az adaott sercomhoz tartozo elso IRQ vonal sorszama
    irqn=sercom->info->IRQn;

    //Feltetelezzuk, hogy egy SERCOM-hoz tartozo interruptok egymas utan
    //sorban kovetkeznek, es egy sercom-hoz 4 darab tartozik.
    for(uint8_t i=0; i<SERCOM_IRQ_COUNT; i++)
    {
        NVIC_SetPriority(irqn, level);
        irqn++;
    }
}
//------------------------------------------------------------------------------
