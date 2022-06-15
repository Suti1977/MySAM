//------------------------------------------------------------------------------
//  Processzor belso Flash kezelest segito fuggvenek
//
//    File: MyNVM.h
//------------------------------------------------------------------------------
#ifndef MYNVM_H_
#define MYNVM_H_

#include "MyCommon.h"
//------------------------------------------------------------------------------
//NVM driver kezdeti inicializalasa
void MyNVM_init(void);

//NVM driver deinicializalasa
void MyNVM_deinit(void);

//Varakozas, hogy az NVM vezerlo elkeszuljon egy korabbi parancsal.
status_t MyNVM_waitingForReady(void);

//NVM parancs vegrehajtasa
status_t MyNVM_execCmd(uint16_t cmd);

//BOOTPROT bitek programozasat megvalosito fuggveny.
//Ha a BOOTPROT bitek nem azonosak a bemeno parameterben megadottal, akkor
//atprogramozza azokat. Ha azonosak, akkor nem tortenik muvelet.
//FONTOS! Biztositani kell, hogy a hivas kozben a megszakitasok tiltva legyenek!
status_t MyNVM_setBootProtBits(uint32_t value);

//Security bit beallitasa --> kodvedelem
status_t MyNVM_setSecurityBit(void);

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

//Tetszoleges szamu byte irasa. Az irast a PAGE_BUFFER-en keresztul hajtja
//vegre.
status_t MyNVM_write(void *dst, const void *src, uint32_t size);

//Erase/write ellen vedett regiokat jelolo bitek lekerdezese.
uint32_t MyNVM_getLockBits(void);

//Flash terulet lezarasa
status_t MyNVM_lockBlock(uint32_t address);

//Flash terulet feloldasa
status_t MyNVM_unlockBlock(uint32_t address);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYNVM_H_
