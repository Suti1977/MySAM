//------------------------------------------------------------------------------
//  Sajat TC timer/counter modul kezeleshez irq handler definicios makro
//
//    File: MyTC_Handler.h
//------------------------------------------------------------------------------
#ifndef MY_TC_HANDLER_H_
#define MY_TC_HANDLER_H_

#include "MyCommon.h"

//A TC definicioit undefine-olnom kellet, mert kulonben az alabbi makroban
//a "TC" osszeveszett a forditoval.
#undef TC0
#undef TC1
#undef TC2
#undef TC3
#undef TC4
#undef TC5
#undef TC6
#undef TC7

//TCn
#define MYTC_H1(n)                 MY_EVALUATOR(TC, n)
//_Handler(void)
#define MYTC_H2                    _Handler(void)

//Seged makro, mellyel egyszeruen definialhato a TC-hez hasznalando interrupt
//rutin.
//Ez kerul a kodba peldaul:
//  void TCn_Handler(void)
//         ^n
//n: A TC sorszama
#define MYTC_HANDLER(n) void MY_EVALUATOR(MYTC_H1(n), MYTC_H2)
//------------------------------------------------------------------------------
#endif //MY_TC_HANDLER_H_
