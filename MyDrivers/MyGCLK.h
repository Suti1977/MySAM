//------------------------------------------------------------------------------
//	GCLK kezeles sajat rutinjai
//------------------------------------------------------------------------------
#ifndef MY_GCLK_H
#define MY_GCLK_H

#include "sam.h"

//------------------------------------------------------------------------------
//A megadott periferiahoz tartozo orajelforras (GCLK) beallitasa, es annak enge-
//delyezese.
void MyGclk_enablePeripherialClock(uint32_t peripherialId, uint32_t generator);

//A megadott priferiara vonatkozo orajel tiltasa
void MyGclk_disablePeripherialClock(uint32_t peripherialId);
//------------------------------------------------------------------------------
#endif //MY_GCLK_H

