//------------------------------------------------------------------------------
#include "MyGCLK.h"

//------------------------------------------------------------------------------
//A megadott periferiahoz tartozo orajelforras (GCLK) beallitasa, es annak enge-
//delyezese.
void MyGclk_setPeripherialClock(uint32_t peripherialId, uint32_t generator)
{
    volatile Gclk* gclk=GCLK;

    gclk->PCHCTRL[peripherialId].reg =
            //generator beallitasa
            GCLK_PCHCTRL_GEN(generator) |
            //Orajel engedelyezese
            GCLK_PCHCTRL_CHEN;
    __DSB();
    //Varakozas, hogy az orajel forrasra valo atallas befejezodjon...
    while (0 == (gclk->PCHCTRL[peripherialId].reg & GCLK_PCHCTRL_CHEN));
}
//------------------------------------------------------------------------------
//A megadott priferiara vonatkozo orajel tiltasa
void MyGclk_disablePeripherialClock(uint32_t peripherialId)
{
    Gclk* gclk=GCLK;

    gclk->PCHCTRL[peripherialId].reg =  0;
    __DSB();
    //Varakozas, hogy az orajel forras tiltodjon
    while (gclk->PCHCTRL[peripherialId].reg & GCLK_PCHCTRL_CHEN);
}
//------------------------------------------------------------------------------
