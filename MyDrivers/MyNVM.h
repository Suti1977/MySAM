//------------------------------------------------------------------------------
//  Processzor belso Flash kezelest segito fuggvenek
//
//    File: MyNVM.h
//------------------------------------------------------------------------------
#ifndef MYNVM_H_
#define MYNVM_H_

#include "MyCommon.h"
//------------------------------------------------------------------------------
//MyNVM valtozoi
typedef struct
{
    int x;
} sMyNVM;
extern sMyNVM MyNVM;
//------------------------------------------------------------------------------
// kezdeti inicializalasa
void MyNVM_Init(void);

//Varakozas, hogy az NVM vezerlo elkeszuljon egy korabbi parancsal.
void MyNVM_waitingForReady(void);

//NVM parancs vegrehajtasa
status_t MyNVM_execCmd(uint32_t cmd);

//BOOTPROT bitek programozasat megvalosito fuggveny.
//Ha a BOOTPROT bitek nem azonosak a bemeno parameterben megadottal, akkor
//atprogramozza azokat. Ha azonosak, akkor nem tortenik muvelet.
//FONTOS! Biztositani kell, hogy a hivas kozben a megszakitasok tiltva legyenek!
status_t MyNVM_setBootProtBits(uint32_t value);

//BOOTPROT bitek kiolvasasa
uint32_t MyNVM_readBootProtBits(void);

//Egy megadott blokk torlese.
void MyNVM_eraseBlock(uint32_t *dst);

//Egy adott cimtol kezdve a Flash vegeig torli a blokkokat
void MyNVM_eraseToFlashEnd(uint32_t *dst);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYNVM_H_
