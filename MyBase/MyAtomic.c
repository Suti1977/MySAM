//------------------------------------------------------------------------------
//  .
//
//    File: MyAtomic.c
//------------------------------------------------------------------------------
#include "MyAtomic.h"
#include "cmsis_gcc.h"

uint32_t MyAtomic_savedPrimask=0;

//
void MyAtomic_enterCritical(uint32_t volatile *atomic)
{
    *atomic = __get_PRIMASK();
    __disable_irq();
    __DMB();
}

//
void  MyAtomic_leaveCritical(uint32_t volatile *atomic)
{
    __DMB();
    __set_PRIMASK(*atomic);
}
