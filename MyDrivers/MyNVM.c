//------------------------------------------------------------------------------
//  Processzor belso Flash kezelest segito fuggvenek
//
//    File: MyNVM.c
//------------------------------------------------------------------------------
//Flash stuffs:
//  https://community.atmel.com/forum/fuses-samd21-xplained
//  https://github.com/mattytrog/Switchboot_Part_1_src/blob/master/src/selfmain.c
//  https://roamingthings.de/posts/use-j-link-to-change-the-boot-loader-protection-of-a-sam-d21/

#include "MyNVM.h"
#include <string.h>


//MyNVM sajat valtozoi
sMyNVM MyNVM;

//------------------------------------------------------------------------------
// kezdeti inicializalasa
void MyNVM_Init(void)
{
    //Modul valtozoinak kezdeti torlese.
    memset(&MyNVM, 0, sizeof(sMyNVM));
}
//------------------------------------------------------------------------------
//Varakozas, hogy az NVM vezerlo elkeszuljon egy korabbi parancsal.
void MyNVM_waitingForReady(void)
{
    while (NVMCTRL->STATUS.bit.READY == 0) {}
}
//------------------------------------------------------------------------------
//NVM parancs vegrehajtasa
status_t MyNVM_execCmd(uint32_t cmd)
{
    NVMCTRL->ADDR.reg = (uint32_t)NVMCTRL_USER;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | cmd;
    MyNVM_waitingForReady();
    return kStatus_Success;
}
//------------------------------------------------------------------------------
//BOOTPROT bitek kiolvasasa
uint32_t MyNVM_readBootProtBits(void)
{
    return NVMCTRL->STATUS.bit.BOOTPROT;
}
//------------------------------------------------------------------------------
//BOOTPROT bitek programozasat megvalosito fuggveny.
//Ha a BOOTPROT bitek nem azonosak a bemeno parameterben megadottal, akkor
//atprogramozza azokat. Ha azonosak, akkor nem tortenik muvelet.
//FONTOS! Biztositani kell, hogy a hivas kozben a megszakitasok tiltva legyenek!
status_t MyNVM_setBootProtBits(uint32_t value)
{
    MyNVM_waitingForReady();

    //A biztositek mezok kiolvasasa. (2 szo!)
    uint32_t fuses[2];

    fuses[0] = *((uint32_t *) NVMCTRL_FUSES_BOOTPROT_ADDR);
    fuses[1] = *(((uint32_t *)NVMCTRL_FUSES_BOOTPROT_ADDR) + 1);

    //A jelenlegi bootprot allapot kiolvasasa
    uint32_t bootprot = (fuses[0] & NVMCTRL_FUSES_BOOTPROT_Msk) >> NVMCTRL_FUSES_BOOTPROT_Pos;

    //Ellenorzes, hogy kell-e programozni
    if (bootprot == value)
    {   //Mar be van allitva. Nincs mit tenni.
        return kStatus_Success;
    }

    //Programozas...
    fuses[0] = (fuses[0] & ~NVMCTRL_FUSES_BOOTPROT_Msk) |
               (value << NVMCTRL_FUSES_BOOTPROT_Pos);

    NVMCTRL->CTRLA.bit.WMODE = NVMCTRL_CTRLA_WMODE_MAN;

    //LAP torlese (erase page)
    MyNVM_execCmd(NVMCTRL_CTRLB_CMD_EP );
    //Oage buffer torlese
    MyNVM_execCmd(NVMCTRL_CTRLB_CMD_PBC);

    *((uint32_t *) NVMCTRL_FUSES_BOOTPROT_ADDR)      = fuses[0];
    *(((uint32_t *)NVMCTRL_FUSES_BOOTPROT_ADDR) + 1) = fuses[1];

    //QWORD iras
    MyNVM_execCmd(NVMCTRL_CTRLB_CMD_WQW);

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Egy megadott blokk torlese.
void MyNVM_eraseBlock(uint32_t *dst)
{
    MyNVM_waitingForReady();

    NVMCTRL->ADDR.reg = (uint32_t)dst;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_EB;

    MyNVM_waitingForReady();
}
//------------------------------------------------------------------------------
//Egy adott cimtol kezdve a Flash vegeig torli a blokkokat
void MyNVM_eraseToFlashEnd(uint32_t *dst)
{
    for (uint32_t i = ((uint32_t) dst); i < FLASH_SIZE; i += NVMCTRL_BLOCK_SIZE)
    {
        MyNVM_eraseBlock((uint32_t *)i);
    }
}
//------------------------------------------------------------------------------
//32 bites szavak masolasa
void MyNVM_copyWords(uint32_t *dst, uint32_t *src, uint32_t n_words)
{
    while (n_words--)
    {
        *dst++ = *src++;
    }
}
//------------------------------------------------------------------------------
