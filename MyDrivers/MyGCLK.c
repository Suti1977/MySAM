//------------------------------------------------------------------------------
#include "MyGCLK.h"
#include "MyCommon.h"

//------------------------------------------------------------------------------
//A megadott periferiahoz tartozo orajelforras (GCLK) beallitasa, es annak enge-
//delyezese.
void MyGclk_enablePeripherialClock(uint32_t peripherialId, uint32_t generator)
{
    volatile Gclk* gclk=GCLK;

    MY_ENTER_CRITICAL();
    gclk->PCHCTRL[peripherialId].reg =
            //generator beallitasa
            GCLK_PCHCTRL_GEN(generator) |
            //Orajel engedelyezese
            GCLK_PCHCTRL_CHEN;
    __DMB();
    //Varakozas, hogy az orajel forrasra valo atallas befejezodjon...
    while (0 == (gclk->PCHCTRL[peripherialId].reg & GCLK_PCHCTRL_CHEN));
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//A megadott priferiara vonatkozo orajel tiltasa
void MyGclk_disablePeripherialClock(uint32_t peripherialId)
{
    Gclk* gclk=GCLK;

    MY_ENTER_CRITICAL();
    gclk->PCHCTRL[peripherialId].reg =  0;
    __DMB();
    //Varakozas, hogy az orajel forras tiltodjon
    while (gclk->PCHCTRL[peripherialId].reg & GCLK_PCHCTRL_CHEN);
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
