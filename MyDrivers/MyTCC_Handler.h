//------------------------------------------------------------------------------
//  Sajat TCC timer/counter modul kezeleshez irq handler definicios makro
//
//    File: MyTCC_Handler.h
//------------------------------------------------------------------------------
#ifndef MY_TCC_HANDLER_H_
#define MY_TCC_HANDLER_H_

#include "MyCommon.h"

//A TCC definicioit undefine-olnom kellet, mert kulonben az alabbi makroban
//a "TCC" osszeveszett a forditoval.
#undef TCC0
#undef TCC1
#undef TCC2
#undef TCC3
#undef TCC4
#undef TCC5
#undef TCC6
#undef TCC7

//TCCn
#define MYTCC_H1(n)                 MY_EVALUATOR(TCC, n)
//_k_Handler(void)
#define MYTCC_H2(k)                 _##k##_Handler(void)

//Seged makro, mellyel egyszeruen definialhato a TCC-hez hasznalando interrupt
//rutin.
//Ez kerul a kodba peldaul:
//  void TCCn_k_Handler(void)
//          ^n^k
//n: A TCC sorszama
//k: A TCC-hez tartozo IRQ vektor sorszama
#define MYTCC_HANDLER(n, k) void MY_EVALUATOR(MYTCC_H1(n), MYTCC_H2(k))

//------------------------------------------------------------------------------
#endif //MY_TCC_HANDLER_H_
