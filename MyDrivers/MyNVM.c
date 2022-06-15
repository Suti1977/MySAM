//------------------------------------------------------------------------------
//  Processzor belso Flash kezelest segito fuggvenek
//
//    File: MyNVM.c
//------------------------------------------------------------------------------
//Flash stuffs:
//  https://community.atmel.com/forum/fuses-samd21-xplained
//  https://github.com/mattytrog/Switchboot_Part_1_src/blob/master/src/selfmain.c
//  https://roamingthings.de/posts/use-j-link-to-change-the-boot-loader-protection-of-a-sam-d21/
//  https://www.avrfreaks.net/sites/default/files/forum_attachments/Flash.cpp
//definiciok:
// FLASH_SIZE           a teljes flash merete byte-ban
// FLASH_PAGE_SIZE      flash lapok merete
// FLASH_NB_OF_PAGES    flash lapok szama
// FLASH_USER_PAGE_SIZE
// FLASH_ADDR           flash kezdocime (0)
// NVMCTRL_PAGE_SIZE    a main teruleten a legkisebb irhato egyseg (512)
// NVMCTRL_BLOCK_SIZE   a torolheto blokk merete (8192)
//
//Az NVMCTRL 4 reszt hataroz meg a flash kezelessel kapcsolatban
// 1. NVM (Auxilary)
// 2. PAGE BUFFER
// 3. BANK A \_____A main (fo) teruelet. (FLASH_SIZE) meretu
// 4. BANK B /
//
//- BANK A/B-bol kepes kodot futtatni. Amig az egyikbol futtat, addig a masik
//  irhato.
//- BANK A/B swappolhato
//- A main resz (BANK A/B) 32 regiora (lapra) oszlik. (Meretuk a Chiptol fugg)
//   o  tehat bankonkent 16 lapra.
//   o  1M chip --> 32k-s  512k chip --> 16k   256k chip -->8k  ...
//   o  regionkent beallithato a write/erase/runlock vedelem
//   o  RUNLOCK regiszter 32 bitje azonositja a 32 regiot. A regiszter resetkor
//      toltodik be az AUX teruletrol (region lock user fuse data), melyet
//      az LR parancsal lehet allitgatni? ??
//- A main resz elso fele lehet bootloader. A bootloader meretet a BOOTPROT[3:0]
//  bitek hatarozzak meg. A bootloader terulet lezarhato erase/write ellen.
//   o  A boot terulet nem torlodik chip erase eseten, csak ha a CBPDIS parancs
//      ki van adva.
//- A felso reszbol lehet allokalni EEPROM emulacionak terultet.
//- A main blokkoknkent torolheto (NVMCTRL_BLOCK_SIZE mutatja a meretet.)
//   o  pl: SamE54: 128 block, 1 block 8192byte
//   o  Tehat ez a legkisebb erase meret!
//- A main blokkjai, 4 szavanként (4x32bit) irhato, ez a legkisebb irhato egyseg
//- PAGEBUFFER-en  keresztul egy teljes lap is irhato.
//- ha nem hasznaljuk a PAGEBUFFERT-t, akkor szabad RAM teruletkent hasznalhato.
//   o A PAGE BUFFER csak 4 szavankent irhato (4x32bit!)
//   o A PAGE BUFFER akkora meretu, mint egy lap.
//   o A PAGE BUFFER-be akkor is  lehet irni, amikor egy ERASE van folyamatban.
//
//Az NVM (Auxilary) modul tovabbi 3 reszre tagolodik:
// 1. USER PAGE                                                 1 page (R/W)
// 2. CB (Calibration page)                                     1 page (R)
// 3. FS (Factory and signature page)                           4 page (R)
//- Az AUX teruletek 4 szavas (4x32bit) egysegekben irhatok.
//- Az AUX teruletek laponkent torolhetok.
//------------------------------------------------------------------------------


#include "MyNVM.h"
#include <string.h>

//------------------------------------------------------------------------------
//NVM driver kezdeti inicializalasa
void MyNVM_init(void)
{
    //NVM orajel engedelyezese
    MCLK->AHBMASK.reg |= MCLK_AHBMASK_NVMCTRL;
}
//------------------------------------------------------------------------------
//NVM driver deinicializalasa
void MyNVM_deinit(void)
{
    //NVM orajel tiltasa
    MCLK->AHBMASK.reg &= ~MCLK_AHBMASK_NVMCTRL;
}
//------------------------------------------------------------------------------
//Varakozas, hogy az NVM vezerlo elkeszuljon egy korabbi parancsal.
status_t MyNVM_waitingForReady(void)
{
    while (NVMCTRL->STATUS.bit.READY == 0) {}
    //TODO: lehetne valami timeoutot impelmentalni
    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Varakozas, hogy az NVM vezerlo elkeszuljon egy korabbi parancsal.
status_t MyNVM_waitingForDone(void)
{
    while (NVMCTRL->INTFLAG.bit.DONE == 0) {}
    //TODO: lehetne valami timeoutot impelmentalni
    return kStatus_Success;
}
//------------------------------------------------------------------------------
//NVM parancs vegrehajtasa
status_t MyNVM_execCmd(uint16_t cmd)
{
    status_t status;
    NVMCTRL->ADDR.reg = (uint32_t)NVMCTRL_USER;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | cmd;
    status=MyNVM_waitingForReady();
    return status;
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
    uint32_t bootprot =
        (fuses[0] & NVMCTRL_FUSES_BOOTPROT_Msk) >> NVMCTRL_FUSES_BOOTPROT_Pos;

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
    //Page buffer torlese
    MyNVM_execCmd(NVMCTRL_CTRLB_CMD_PBC);

    *((uint32_t *) NVMCTRL_FUSES_BOOTPROT_ADDR)      = fuses[0];
    *(((uint32_t *)NVMCTRL_FUSES_BOOTPROT_ADDR) + 1) = fuses[1];

    //QWORD iras
    MyNVM_execCmd(NVMCTRL_CTRLB_CMD_WQW);

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Egy megadott blokk torlese.  (NVMCTRL_BLOCK_SIZE)
status_t MyNVM_eraseBlock(uint32_t *dst)
{
    status_t status;
    status=MyNVM_waitingForReady();
    if (status) goto error;

    MY_ENTER_CRITICAL();
    NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
    NVMCTRL->ADDR.reg = (uint32_t)dst;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_EB;
    MY_LEAVE_CRITICAL();

    status=MyNVM_waitingForDone();
    if (status) goto error;
    status=MyNVM_waitingForReady();
    if (status) goto error;

error:
    //Cache uritese
    MY_ENTER_CRITICAL();
    CMCC->CTRL.bit.CEN=0;
    CMCC->MAINT0.bit.INVALL=1;
    CMCC->CTRL.bit.CEN=1;
    MY_LEAVE_CRITICAL();

    return status;
}
//------------------------------------------------------------------------------
//User page torlese
status_t MyNVM_eraseUserPage(uint32_t *dst)
{
    (void) dst;
    status_t status;
    status=MyNVM_waitingForReady();
    if (status) goto error;

    MY_ENTER_CRITICAL();
    NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
    NVMCTRL->ADDR.reg = (uint32_t)NVMCTRL_USER;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_EP;
    MY_LEAVE_CRITICAL();

    status=MyNVM_waitingForDone();
    if (status) goto error;
    status=MyNVM_waitingForReady();
    if (status) goto error;

error:
    //Cache uritese
    MY_ENTER_CRITICAL();
    CMCC->CTRL.bit.CEN=0;
    CMCC->MAINT0.bit.INVALL=1;
    CMCC->CTRL.bit.CEN=1;
    MY_LEAVE_CRITICAL();

    return status;
}
//------------------------------------------------------------------------------
//32 bites szavak irasa
status_t MyNVM_writeWords(uint32_t *dst,
                          const uint32_t *src,
                          uint32_t numWords)
{
    status_t status=kStatus_Success;

    //Az iras csak 16 bajtos hatarokon kezdodhet
    if (((uint32_t)dst & 0x0F) !=0)
    {
        //TODO: hibakodot adni!
        status=kStatus_Fail;
        goto error;
    }

    MY_ENTER_CRITICAL();

    //CACHE tiltasa. Errata alapjan, az "A" sorozatnal a cache kepes bezavarni.
    NVMCTRL->CTRLA.bit.CACHEDIS0=1;
    NVMCTRL->CTRLA.bit.CACHEDIS1=1;

    //Manual page write
    NVMCTRL->CTRLA.bit.WMODE=NVMCTRL_CTRLA_WMODE_MAN;

    //Page buffer torlese. Minden bit 1-be all...
    MyNVM_waitingForReady();
    NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_PBC;
    while (NVMCTRL->INTFLAG.bit.DONE == 0) { }
    MyNVM_waitingForDone();

    //wordok irasa. 4 szavankent...
    const uint32_t *src_addr=src;
    uint32_t *dst_addr=(uint32_t *)dst;
    while(numWords>0)
    {
        uint32_t addr=(uint32_t)dst_addr;

        status=MyNVM_waitingForReady();
        if (status)
        {
            break;
        }

        if (numWords >= 4)
        {   //Tobb mint 4 szo van meg hatra...
            *dst_addr++ = *src_addr++;
            *dst_addr++ = *src_addr++;
            *dst_addr++ = *src_addr++;
            *dst_addr++ = *src_addr++;
            numWords -= 4;
        } else
        {   //kevesebb mint 4 szo van hatra
            uint32_t i;
            for(i=0; i<numWords; i++)
            {
                *dst_addr++ = *src_addr++;
            }

            //A maradekot csupa 1-el toltjuk fel
            for(; i<4; i++)
            {
                *dst_addr++ = 0xFFFFFFFF;
            }
            numWords=0;
        }

        NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
        NVMCTRL->ADDR.reg = ((uint32_t)addr);
        NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_WQW;

        status=MyNVM_waitingForDone();
        if (status) break;
        status=MyNVM_waitingForReady();
        if (status) break;

    } //while(numWords>0)

    //Cache vezerles visszakapcsolasa
    NVMCTRL->CTRLA.bit.CACHEDIS0=0;
    NVMCTRL->CTRLA.bit.CACHEDIS1=0;

    MY_LEAVE_CRITICAL();

error:
    return status;
}
//------------------------------------------------------------------------------
//Egy teljes lap irasa (FLASH_PAGE_SIZE)
status_t MyNVM_writePage(void *dst,
                         const void *src,
                         uint32_t size)
{
    status_t status=kStatus_Success;

    // Calculate data boundaries
    size = (size + 3) / 4; //convert bytes to words with rounding

    volatile uint32_t *dst_addr = (volatile uint32_t *)dst;
    const uint8_t *src_addr = (const uint8_t *)src;

    //A lap iras csak hataron kezdodhet!
    if (((uint32_t)dst) % FLASH_PAGE_SIZE)
    {   //TODO: hibakodot adni
        status=kStatus_Fail;
        goto error;
    }

    status=MyNVM_waitingForReady();
    if (status) goto error;

    //Automatikus lap iras tiltasa
    NVMCTRL->CTRLA.bit.WMODE=0x00;

    while (size)
    {
        //Page buffer torlese. Minden bit 1-be all...
        MY_ENTER_CRITICAL();
        MyNVM_waitingForReady();
        NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
        NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_PBC;
        while (NVMCTRL->INTFLAG.bit.DONE == 0) { }
        MyNVM_waitingForDone();
        MY_LEAVE_CRITICAL();

        //Page buffer toltese. Csak 4 bajtonkent szabad tolteni egy idoben!
        for (uint32_t i=0; i<(FLASH_PAGE_SIZE/4) && size; i++)
        {
            //A nem illesztett bemeneti adatok olvasasa...
            union
            {
                uint32_t u32;
                uint8_t u8[4];
            } res;

            const uint8_t *d = (const uint8_t *)src_addr;
            res.u8[0] = d[0];
            res.u8[1] = d[1];
            res.u8[2] = d[2];
            res.u8[3] = d[3];

            *dst_addr = res.u32;
            src_addr += 4;
            dst_addr++;
            size--;
        }
        MyNVM_waitingForReady();

        MY_ENTER_CRITICAL();
        NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
        NVMCTRL->ADDR.reg=(uint32_t)dst;
        NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_WP;
        MyNVM_waitingForDone();
        MY_LEAVE_CRITICAL();


        //Cache uritese
        CMCC->CTRL.bit.CEN=0;
        CMCC->MAINT0.bit.INVALL=1;
        CMCC->CTRL.bit.CEN=1;


        status=MyNVM_waitingForReady();
        if (status) break;
        status=MyNVM_waitingForDone();
        if (status) break;
    }

error:
    return status;
}
//------------------------------------------------------------------------------
//Tetszoleges szamu byte irasa. Az irast a PAGE_BUFFER-en keresztul hajtja
//vegre.
status_t MyNVM_write(void *dst, const void *src, uint32_t size)
{
    status_t status=kStatus_Success;
    volatile uint32_t *dst_addr = (volatile uint32_t *)dst;
    const uint8_t *src_addr = (const uint8_t *)src;

    status=MyNVM_waitingForReady();
    if (status) goto error;

    //Automatikus lap iras tiltasa
    NVMCTRL->CTRLA.bit.WMODE=0x00;

    union
    {
        uint32_t u32;
        uint8_t u8[4];
    } res;

    //A PAGE buffer csak 4 bajtonkent irhato. Kiszamoljuk, hogy a szavon
    //belul honnantol kell kezdeni.
    //volatile uint32_t rem=4-((uint32_t)dst_addr & 0x03);
    volatile uint32_t rem=4-(uint32_t)dst_addr & 0x03;

    //Az elso szo cimere maszkolunk. (Also 2 bit torlese)
    dst_addr=(uint32_t*)((uint32_t) dst_addr & ~((uint32_t) 3ul));

    if (rem)
    {   //Nem teljes szoval kezd.

        //Page buffer torlese. Minden bit 1-be all...
        MY_ENTER_CRITICAL();
        MyNVM_waitingForReady();
        NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
        NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_PBC;
        MyNVM_waitingForDone();
        MY_LEAVE_CRITICAL();

        //A nem irt bitekre '1'-et kell helyezni.
        res.u32=~(uint32_t) 0;

        for(uint32_t i=0; i<rem; i++)
        {
            res.u8[4-rem+i]= *src_addr++;
        }
        size-=rem;
        //elso szo kiirasa.
        *dst_addr++ = res.u32;
        MyNVM_waitingForReady();

        MY_ENTER_CRITICAL();
        NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
        NVMCTRL->ADDR.reg=(uint32_t)dst;
        NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_WP;
        MyNVM_waitingForDone();
        MY_LEAVE_CRITICAL();


        //Cache uritese
        CMCC->CTRL.bit.CEN=0;
        CMCC->MAINT0.bit.INVALL=1;
        CMCC->CTRL.bit.CEN=1;

        MyNVM_waitingForReady();
        MyNVM_waitingForDone();
    }


    while (size)
    {   //Van meg mit kiirni. Ciklusm amig tud irni.

        //Page buffer torlese. Minden bit 1-be all...
        MY_ENTER_CRITICAL();
        MyNVM_waitingForReady();
        NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
        NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_PBC;
        MyNVM_waitingForDone();
        MY_LEAVE_CRITICAL();

        //Page buffer toltese... Csak 4 bajtonkent szabad tolteni egy idoben!

        //Flash iro lapon beluli pozicio meghatarozasa. (32 bites szohatarok)
        uint32_t pageOffs;
        pageOffs=(uint32_t)dst_addr & (FLASH_PAGE_SIZE-1);

        //Az elso irt cim megjegyzese. Majd az iro parancsnal ez kerul megadasra
        uint32_t pageStartAddr=(uint32_t)dst_addr;

        while((pageOffs < FLASH_PAGE_SIZE) && size)
        {
            if (size>4)
            {   //Egy szot biztosan ki lehet irni
                for(uint32_t x=0; x<4; x++)
                {
                    res.u8[x]= *src_addr++;
                }
                size-=4;
            } else
            {   //Egy teljes szot mar nem lehet megtolteni tartalommal.

                //A nem irt bitekre '1'-et kell helyezni.
                res.u32=~(uint32_t) 0;

                for(uint32_t x=0; x<size; x++)
                {
                    res.u8[x]= *src_addr++;
                }
                size=0;
            }
            //32 bites iras
            *dst_addr++ = res.u32;

            //uj szora allt. Annak megfelelo offset mutatasa.
            pageOffs+=4;
        }

        //A lap tartalma elkeszult. Iras...

        MyNVM_waitingForReady();

        MY_ENTER_CRITICAL();
        NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
        NVMCTRL->ADDR.reg=pageStartAddr;
        NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_WP;
        MyNVM_waitingForDone();
        MY_LEAVE_CRITICAL();

        //Cache uritese
        CMCC->CTRL.bit.CEN=0;
        CMCC->MAINT0.bit.INVALL=1;
        CMCC->CTRL.bit.CEN=1;

        status=MyNVM_waitingForReady();
        if (status) break;
        status=MyNVM_waitingForDone();
        if (status) break;
    }

error:
    return status;
}
//------------------------------------------------------------------------------
//Egy adott cimtol kezdve a Flash vegeig torli a blokkokat
status_t MyNVM_eraseToFlashEnd(uint32_t *dst)
{
    status_t status=kStatus_Success;
    for (uint32_t i = ((uint32_t) dst); i < FLASH_SIZE; i += NVMCTRL_BLOCK_SIZE)
    {
        status=MyNVM_eraseBlock((uint32_t *)i);
        if (status) break;
    }

    return status;
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
//Security bit beallitasa --> kodvedelem
status_t MyNVM_setSecurityBit(void)
{
    status_t status;
    status=MyNVM_waitingForReady();
    if (status) goto error;

    MY_ENTER_CRITICAL();
    NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_SSB;
    MY_LEAVE_CRITICAL();

    status=MyNVM_waitingForDone();
    if (status) goto error;
    status=MyNVM_waitingForReady();
    if (status) goto error;

error:
    return status;
}
//------------------------------------------------------------------------------
//Erase/write ellen vedett regiokat jelolo bitek lekerdezese.
uint32_t MyNVM_getLockBits(void)
{
    return NVMCTRL->RUNLOCK.reg;
}
//------------------------------------------------------------------------------
//Flash terulet lezarasa
status_t MyNVM_lockBlock(uint32_t address)
{
    status_t status;

    //TODO: ellenorizni, hogy a cim az blokk hatarra esik e!

    status=MyNVM_waitingForReady();
    if (status) goto error;

    MY_ENTER_CRITICAL();
    NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
    NVMCTRL->ADDR.reg=address;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_LR;
    MY_LEAVE_CRITICAL();

    status=MyNVM_waitingForDone();
    if (status) goto error;
    status=MyNVM_waitingForReady();
    if (status) goto error;

error:
    return status;
}
//------------------------------------------------------------------------------
//Flash terulet feloldasa
status_t MyNVM_unlockBlock(uint32_t address)
{
    status_t status;

    //TODO: ellenorizni, hogy a cim az blokk hatarra esik e!

    status=MyNVM_waitingForReady();
    if (status) goto error;

    MY_ENTER_CRITICAL();
    NVMCTRL->INTFLAG.reg=NVMCTRL_INTFLAG_DONE;
    NVMCTRL->ADDR.reg=address;
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_UR;
    MY_LEAVE_CRITICAL();

    status=MyNVM_waitingForDone();
    if (status) goto error;
    status=MyNVM_waitingForReady();
    if (status) goto error;

error:
    return status;
}
//------------------------------------------------------------------------------
