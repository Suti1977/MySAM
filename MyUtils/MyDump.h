//------------------------------------------------------------------------------
//  Memoriatartalmat konzolra dump-olo rutinok
//
//    File: MyDump.h
//------------------------------------------------------------------------------
#ifndef MY_DUMP_H_
#define MY_DUMP_H_

#include "MyCommon.h"

//memoriatartalom kilistazasa a konzolra
void MyDump_memory(void* src, unsigned int length);
//memoriatartalom kilistazasa a konzolra, de ugy, hogy a cimet mi specifikaljuk
void MyDump_memorySpec(void* src,
                       unsigned int length,
                       unsigned int firstPrintedAddress);
//------------------------------------------------------------------------------
#endif //MY_DUMP_H_
