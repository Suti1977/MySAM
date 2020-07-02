//------------------------------------------------------------------------------
//	GCLK kezeles sajat rutinjai
//------------------------------------------------------------------------------
#ifndef MY_GCLK_H
#define MY_GCLK_H

#include "sam.h"

//------------------------------------------------------------------------------
//A megadott periferiahoz tartozo orajelforras (GCLK) beallitasa, es annak enge-
//delyezese.
void MyGclk_setPeripherialClock(uint32_t peripherialId, uint32_t generator);

//------------------------------------------------------------------------------
#endif //MY_GCLK_H

