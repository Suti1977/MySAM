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
void MyNVM_init(void);

//Varakozas, hogy az NVM vezerlo elkeszuljon egy korabbi parancsal.
status_t MyNVM_waitingForReady(void);

//NVM parancs vegrehajtasa
status_t MyNVM_execCmd(uint16_t cmd);

//BOOTPROT bitek programozasat megvalosito fuggveny.
//Ha a BOOTPROT bitek nem azonosak a bemeno parameterben megadottal, akkor
//atprogramozza azokat. Ha azonosak, akkor nem tortenik muvelet.
//FONTOS! Biztositani kell, hogy a hivas kozben a megszakitasok tiltva legyenek!
status_t MyNVM_setBootProtBits(uint32_t value);

//BOOTPROT bitek kiolvasasa
uint32_t MyNVM_readBootProtBits(void);

//Egy megadott blokk torlese.  (NVMCTRL_BLOCK_SIZE)
status_t MyNVM_eraseBlock(uint32_t *dst);

//Egy adott cimtol kezdve a Flash vegeig torli a blokkokat
status_t MyNVM_eraseToFlashEnd(uint32_t *dst);

//User page torlese
status_t MyNVM_eraseUserPage(uint32_t *dst);

//32 bites szavak irasa
status_t MyNVM_writeWords(uint32_t *dst,
                          const uint32_t *src,
                          uint32_t numWords);

//Egy teljes lap irasa (FLASH_PAGE_SIZE)
status_t MyNVM_writePage(void *dst,
                         const void *src,
                         uint32_t size);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYNVM_H_
