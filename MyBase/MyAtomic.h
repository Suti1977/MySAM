//------------------------------------------------------------------------------
//  .
//
//    File: MyAtomic.h
//------------------------------------------------------------------------------
#ifndef MY_ATOMIC_H_
#define MY_ATOMIC_H_

#include <stdint.h>

extern uint32_t MyAtomic_savedPrimask;

void MyAtomic_enterCritical(uint32_t volatile *atomic);
void MyAtomic_leaveCritical(uint32_t volatile *atomic);
//------------------------------------------------------------------------------
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#define MY_ENTER_CRITICAL()     portENTER_CRITICAL()
#define MY_LEAVE_CRITICAL()     portEXIT_CRITICAL()
#else
#define MY_ENTER_CRITICAL()     MyAtomic_enterCritical(&MyAtomic_savedPrimask)
#define MY_LEAVE_CRITICAL()     MyAtomic_leaveCritical(&MyAtomic_savedPrimask)
#endif
//------------------------------------------------------------------------------
#endif //MY_ATOMIC_H_
