//------------------------------------------------------------------------------
//  Sajat DMA kezelo rutinok
//------------------------------------------------------------------------------
#include "MyDMA.h"

//DMA csatornakhoz tartozo leirok, melyeket a driver hasznal
static MyDMA_Channel_t MyDMA_Channels[MAX_USEABLE_DMA_CHANNEL];

//Az SRAM-bol a DMA kezeles deszkriptorainak memoria foglalasa. Erre mutat a DMA
//vezerloben a BASEADDR mezo is.
DmacDescriptor
MyDMA_firstDescriptors[MAX_USEABLE_DMA_CHANNEL] __attribute__ ((aligned (16)));
//DMA vezerlo ide irja vissza a statusz informaciot. (Erre mutat a DMAC-ben a
//WRBADDR regiszter.
DmacDescriptor
MyDMA_writeBackMemory [MAX_USEABLE_DMA_CHANNEL] __attribute__ ((aligned (16)));

//------------------------------------------------------------------------------
//DMA kezeles kezdeti initje.
void MyDMA_init(MyDMA_Config_t* cfg)
{
    //DMA-zashoz allokalt RAM teruletek torlese
    memset(MyDMA_firstDescriptors, 0, sizeof(MyDMA_firstDescriptors));
    memset(MyDMA_writeBackMemory,  0, sizeof(MyDMA_writeBackMemory));
    memset(MyDMA_Channels,         0, sizeof(MyDMA_Channels));

    //DMA vezerlo orajelenek engedelyezese az MCLK modulban
    MCLK->AHBMASK.bit.DMAC_=1;

    Dmac* hw=DMAC;

    //DMA vezerlo szoftveres resetelese. Ez egyben ki is kapcsolja azt, es lehet
    //irni az enable-protect vedelemmel rendelkezo regisztereket is.
    hw->CTRL.reg &= ~DMAC_CTRL_DMAENABLE;
    hw->CTRL.reg  =  DMAC_CTRL_SWRST;
    while(hw->CTRL.bit.SWRST);

    //Deszkriptorok beallitasa
    hw->BASEADDR.reg=(uint32_t) MyDMA_firstDescriptors;
    //Writeback cim beallitasa. Ide irogat vissza a DMA vezerlo
    hw->WRBADDR.reg=(uint32_t) MyDMA_writeBackMemory;

    //DMA vezerlo alapertelemzesek beallitasa..
    hw->CTRL.reg=
            //mind a 4 prioritasi szintet engedelyezem, hatha kesobb majd kell.
            DMAC_CTRL_LVLEN(0x0f) |
            //TODO: itt lehetne engedelyezni a CRC szamito modult
            //DMAC_CTRL_CRCENABLE |
            0;

    //A 4 prioritasi szint, Round-Robin mod, QOS beallitasa a 4 prioritasi
    //szinthez.
    uint32_t reg=
    (cfg->qos0 << DMAC_PRICTRL0_QOS0_Pos) |
    (cfg->qos1 << DMAC_PRICTRL0_QOS1_Pos) |
    (cfg->qos2 << DMAC_PRICTRL0_QOS2_Pos) |
    (cfg->qos3 << DMAC_PRICTRL0_QOS3_Pos) |
    (cfg->priorityLevel0 << DMAC_PRICTRL0_LVLPRI0_Pos) |
    (cfg->priorityLevel1 << DMAC_PRICTRL0_LVLPRI0_Pos) |
    (cfg->priorityLevel2 << DMAC_PRICTRL0_LVLPRI0_Pos) |
    (cfg->priorityLevel3 << DMAC_PRICTRL0_LVLPRI0_Pos);

    if (cfg->roundRobinEn0) reg |=DMAC_PRICTRL0_RRLVLEN0;
    if (cfg->roundRobinEn1) reg |=DMAC_PRICTRL0_RRLVLEN1;
    if (cfg->roundRobinEn2) reg |=DMAC_PRICTRL0_RRLVLEN2;
    if (cfg->roundRobinEn3) reg |=DMAC_PRICTRL0_RRLVLEN3;

    hw->PRICTRL0.reg = reg;

    //DMA IRQ vonalak kezdeti prioritasanak beallitasa, majd engedelyezese
    uint32_t irqn=DMAC_0_IRQn;
    for (int i = 0; i < 5; i++)
    {
        NVIC_DisableIRQ(irqn);
        NVIC_SetPriority(irqn, cfg->defaultIrqPriority);
        NVIC_ClearPendingIRQ(irqn);
        NVIC_EnableIRQ(irqn);
        irqn++;
    }

    //DMA vezerlo engedelyezese (tobb regiszter innentol nem lesz modosithato!)
    hw->CTRL.bit.DMAENABLE=1;
}
//------------------------------------------------------------------------------
//Default beallitasokkal struktrura feltoltese
void MyDMA_getDefaultConfig(MyDMA_Config_t* cfg)
{
    memset(cfg, 0, sizeof(MyDMA_Config_t));

    //Alapertelmezesben minden szinthez Statikus prioritas kerul beallitasra.
    //QOS: background (0x00)
    //Prioritasi szint: 0x00
}
//------------------------------------------------------------------------------
//DMA csatorna beallitasa.
status_t MyDMA_configureChannel(uint8_t channel, MyDMA_ChannelConfig_t* cfg)
{
    if (channel>=MAX_USEABLE_DMA_CHANNEL)
        return kMyDMAStatus_InvalidChannelIndex;

    DmacChannel* chRegs=&DMAC->Channel[channel];

    uint32_t ctrlAValue=
            DMAC_CHCTRLA_BURSTLEN(cfg->burstLen) |
            DMAC_CHCTRLA_TRIGACT(cfg->triggerAction) |
            DMAC_CHCTRLA_TRIGSRC(cfg->triggerSource) |
            DMAC_CHCTRLA_THRESHOLD(cfg->threshold);


    MYDMA_ENTER_CRITICAL();

    //A csatornahoz tartozo regiszterek szoftver reszetes torlese
    chRegs->CHCTRLA.reg &= ~DMAC_CHCTRLA_ENABLE;
    chRegs->CHCTRLA.reg  = DMAC_CHCTRLA_SWRST;
    while(chRegs->CHCTRLA.bit.SWRST);

    chRegs->CHCTRLA.reg |=ctrlAValue;
    chRegs->CHPRILVL.bit.PRILVL=cfg->priorityLevel;
    chRegs->CHEVCTRL.reg =cfg->eventControl.reg;

    MYDMA_LEAVE_CRITICAL();

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Default beallitasokkal csatorna config struktrura feltoltese
void MyDMA_getDefaultChannelConfig(MyDMA_ChannelConfig_t* cfg)
{
    memset(cfg, 0, sizeof(MyDMA_ChannelConfig_t));
}
//------------------------------------------------------------------------------
//DMA deszkriptor beallitasa.
//Ezt tudjuk hasznalni sajat linkelt deszkriptorok epitesere is
void MyDMA_buildDescriptor(DmacDescriptor* desc,
                             const MyDMA_TransferConfig_t* cfg)
{
    DMAC_BTCTRL_Type btCtrl;
    btCtrl.reg=
        (uint16_t)(cfg->eventOutputSelectoin << DMAC_BTCTRL_EVOSEL_Pos) |
        (uint16_t)(cfg->blockAction << DMAC_BTCTRL_BLOCKACT_Pos) |
        (uint16_t)(cfg->beatSize    << DMAC_BTCTRL_BEATSIZE_Pos) |
        (uint16_t)(cfg->stepSize    << DMAC_BTCTRL_STEPSIZE_Pos);
    btCtrl.bit.STEPSEL=cfg->stepSlect;
    btCtrl.bit.SRCINC =cfg->srcAddrIncrementEnable;
    btCtrl.bit.DSTINC =cfg->dstAddrIncrementEnable;
    desc->BTCTRL.reg=btCtrl.reg;

    desc->DSTADDR.reg=(uint32_t) cfg->dst;
    desc->SRCADDR.reg=(uint32_t) cfg->src;
    desc->DESCADDR.reg=(uint32_t) cfg->nextDescriptor;

    //Forras es vagy cel cimek adjusztalasa a BEAT hossznak megfeloen, majd
    //BTCNT mezo beallitasa.
    //desc->BTCNT.reg=Cfg->BlockTransferCount;
    MyDMA_setDataAmount(desc, cfg->blockTransferCount);

    //A beallitasok utan validra tesszuk a deszkriptort.
    desc->BTCTRL.bit.VALID=1;

    __asm("dmb");
    __asm("dsb");
    __asm("isb");
}
//------------------------------------------------------------------------------
//Csatorna atviteli parameterek beallitasa az elso deszkriptorba.
//A rutin egyben validra is allitja a deszkriptort
status_t MyDMA_configureTransfer(uint8_t channel,
                                   const MyDMA_TransferConfig_t* cfg)
{
    if (channel>=MAX_USEABLE_DMA_CHANNEL)
        return kMyDMAStatus_InvalidChannelIndex;

    DmacDescriptor* desc=&MyDMA_firstDescriptors[channel];

    MyDMA_buildDescriptor(desc, cfg);

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Defalt beallitasokkal csatorna transzfer konfiguracio feltoltese
void MyDMA_getDefaultTransferConfig(MyDMA_TransferConfig_t* cfg)
{
    memset(cfg, 0, sizeof(MyDMA_TransferConfig_t));
}
//------------------------------------------------------------------------------
//Elore osszeallitott deszkriptor beallitasa az adott csatornara
void MyDMA_setDescriptor(uint8_t channel, DmacDescriptor* desc)
{
    ASSERT(channel<MAX_USEABLE_DMA_CHANNEL);

    //Bemasolas az allokalt memoria teruletre
    memcpy(&MyDMA_firstDescriptors[channel], desc, sizeof(DmacDescriptor));
}
//------------------------------------------------------------------------------
//Egy DMA csatorna engedelyezese megszakitasbol --> Elindul az atvitel, ha van
//trigger
void MyDMA_enableChannelFromIsr(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];

    //Engedelyezes
    #undef ENABLE
    chRegs->CHCTRLA.bit.ENABLE = 1;
}
//------------------------------------------------------------------------------
//Egy DMA csatorna engedelyezese -->Elindul az atvitel, ha van trigger
void MyDMA_enableChannel(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];

    MYDMA_ENTER_CRITICAL();

    //Engedelyezes
    #undef ENABLE
    chRegs->CHCTRLA.bit.ENABLE = 1;

    MYDMA_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Egy DMA csatorna tiltasa megszakitasbol
void MyDMA_disableChannelFromIsr(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];

    //tiltas
    #undef ENABLE
    chRegs->CHCTRLA.bit.ENABLE = 0;
}
//------------------------------------------------------------------------------
//Egy DMA csatorna tiltasa
void MyDMA_disableChannel(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];

    MYDMA_ENTER_CRITICAL();

    //tiltas
    #undef ENABLE
    chRegs->CHCTRLA.bit.ENABLE = 0;

    MYDMA_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Egy DMA csatorna suspendolasa
void MyDMA_suspendChannel(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];

    MYDMA_ENTER_CRITICAL();

    chRegs->CHCTRLB.bit.CMD=DMAC_CHCTRLB_CMD_SUSPEND_Val;

    MYDMA_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Egy DMA csatorna mukodes folytatasa szoftveresen
void MyDMA_resumeChannel(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];

    MYDMA_ENTER_CRITICAL();

    chRegs->CHCTRLB.bit.CMD=DMAC_CHCTRLB_CMD_RESUME_Val;

    MYDMA_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Egy DMA csatorna ujrainditasa
void MyDMA_restartChannel(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];

    //Avval, hogy egy DMA csatornat tiltunk, majd visszaengedelyezunk, az adott
    //csatorna befejezi a korabbi tranzakciot, majd az ujraengedelyezes hatasara
    //a friss deszkriptort kezdi feldolgozni.
    MYDMA_ENTER_CRITICAL();

    #undef ENABLE
    chRegs->CHCTRLA.bit.ENABLE = 0;
    chRegs->CHCTRLA.bit.ENABLE = 1;

    MYDMA_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Csatornahoz cel cim beallitasa.
void MyDMA_setDestAddress(uint8_t channel, const void *const dst)
{
    MyDMA_firstDescriptors[channel].DSTADDR.reg=(uint32_t)dst;
}
//------------------------------------------------------------------------------
//Csatornahoz forras cim beallitasa.
void MyDMA_setSrcAddress(uint8_t channel, const void *const src)
{
    MyDMA_firstDescriptors[channel].SRCADDR.reg=(uint32_t)src;
}
//------------------------------------------------------------------------------
//A csatornahoz tartozo kovetkezo DMA leiro beallitasa
void MyDMA_setNextDescriptor(uint8_t channel,
                                DmacDescriptor* nextDescriptor)
{
    MyDMA_firstDescriptors[channel].DESCADDR.reg=(uint32_t)nextDescriptor;
}
//------------------------------------------------------------------------------
//Forrascim noveles engedelyezese/tiltasa
void MyDMA_setSrcAddressIncrement(uint8_t channel, bool enable)
{
    MyDMA_firstDescriptors[channel].BTCTRL.bit.SRCINC=enable;
}
//------------------------------------------------------------------------------
//Celcim noveles engedelyezese/tiltasa
void MyDMA_setDstAddressIncrement(uint8_t channel, bool enable)
{
    MyDMA_firstDescriptors[channel].BTCTRL.bit.DSTINC=enable;
}
//------------------------------------------------------------------------------
//A DMA atvitelkor atviendo elemek szamanak beallitasa
//Fontos! Elotte az SRC/DST cimeket, es azok novelesenek engedelyezeset
//be kell allitani!
void MyDMA_setDataAmount(DmacDescriptor* desc, const uint32_t amount)
{
    //A SAM-ek novekmenyes cimkezelese eseten, a baziscimhez hozza kell adni
    //az atviendo adatmennyisegnyi byteot:
    //      SRCADDR=SRCADDR + BTCNT*(BEATSIZE+1)*2^STEPSIZE.
    //      DSTADDR=DSTADDR + BTCNT*(BEATSIZE+1)*2^STEPSIZE.

    uint32_t add=amount * (1 << desc->BTCTRL.bit.BEATSIZE);

    if (desc->BTCTRL.bit.DSTINC)
    {   //Celcimet novelni kell
        desc->DSTADDR.reg += add;
    }

    if (desc->BTCTRL.bit.SRCINC)
    {   //forras cimet novelni kell
        desc->SRCADDR.reg += add;
    }

    desc->BTCNT.reg=(uint16_t)amount;
}
//------------------------------------------------------------------------------
//Tranzakcio engedelyezese a csatornan
//void MyDMA_EnableTransaction(uint8_t Channel)
//{
//    MYDMA_ENTER_CRITICAL();
//    MyDMA_firstDescriptors[Channel].BTCTRL.bit.VALID=1;
//    DMAC->Channel[Channel].CHCTRLA.bit.ENABLE = 1;
//    MYDMA_LEAVE_CRITICAL();
//}
//------------------------------------------------------------------------------
void MyDMA_swTrigger(uint8_t channel)
{
    MYDMA_ENTER_CRITICAL();

    MYDMA_SWTRIGGER(channel);

    MYDMA_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Statusz callbackek beregisztralasa a megadott csatornahoz.
//Ha valamelyik callback nincs hasznalva, akkor helyette NULL-t kell megadni.
void MyDMA_registreCallbacks(uint8_t channelNr,
                             MyDMA_doneFunc_t* doneFunc,
                             MyDMA_errorFunc_t* errorFunc,
                             void* callback_data)
{
    ASSERT(channelNr<MAX_USEABLE_DMA_CHANNEL);
    MyDMA_Channel_t* channel=&MyDMA_Channels[channelNr];
    DmacChannel* chRegs=&DMAC->Channel[channelNr];
    channel->callbackData=callback_data;
    channel->errorFunc=errorFunc;
    channel->doneFunc=doneFunc;

    if (doneFunc)
    {   //Van megadva a transzfer veget jelzo callback. Hozza tartozo IT enge-
        //delyezese.
        chRegs->CHINTENSET.reg=DMAC_CHINTENSET_TCMPL;
    } else
    {
        chRegs->CHINTENCLR.reg=DMAC_CHINTENCLR_TCMPL;
    }

    if (errorFunc)
    {   //Van megadva a transzfer veget jelzo callback. Hozza tartozo IT enge-
        //delyezese.
        chRegs->CHINTENSET.reg=DMAC_CHINTENSET_TERR;
    } else
    {
        chRegs->CHINTENCLR.reg=DMAC_CHINTENCLR_TERR;
    }
}
//------------------------------------------------------------------------------
//Kozos IT rutin
void MyDMAC_Handler(void)
{
    Dmac* hw=DMAC;

    //Az interruptot kivalto csatorna sorsazamanak lekerdezese
    uint8_t channelID=hw->INTPEND.bit.ID;
    MyDMA_Channel_t* channel=&MyDMA_Channels[channelID];

    //Callback funkciok hivasa...
    if (hw->INTPEND.bit.TERR)
    {
        //IT flag torlese
        hw->Channel[channelID].CHINTFLAG.reg = DMAC_CHINTFLAG_TERR;

        if (channel->errorFunc)
        {
            channel->errorFunc(channel->callbackData);
        }
    }

    if (hw->INTPEND.bit.TCMPL)
    {
        //IT flag torlese
        hw->Channel[channelID].CHINTFLAG.reg = DMAC_CHINTFLAG_TCMPL;

        if (channel->doneFunc)
        {
            channel->doneFunc(channel->callbackData);
        }
    }

}
//------------------------------------------------------------------------------
//Interrupt rutinok
void DMAC_0_Handler(void)
{
    MyDMAC_Handler();
}
void DMAC_1_Handler(void)
{
    MyDMAC_Handler();
}
void DMAC_2_Handler(void)
{
    MyDMAC_Handler();
}
void DMAC_3_Handler(void)
{
    MyDMAC_Handler();
}
void DMAC_4_Handler(void)
{
    MyDMAC_Handler();
}
//------------------------------------------------------------------------------

#include <stdio.h>
//DMA adatok kiirsa a konzolra
void MyDMA_printDebugInfo(void)
{
    printf("=====DMA INFO=====\n");
    printf("       CTRL: %04X\n", (int)DMAC->CTRL.reg);
    printf("    CRCCTRL: %04X\n", (int)DMAC->CRCCTRL.reg);
    printf("  CRCDATAIN: %08X\n", (int)DMAC->CRCDATAIN.reg);
    printf("  CRCCHKSUM: %08X\n", (int)DMAC->CRCCHKSUM.reg);
    printf("  CRCSTATUS: %02X\n", (int)DMAC->CRCSTATUS.reg);
    printf("    DBGCTRL: %02X\n", (int)DMAC->DBGCTRL.reg);
    printf(" SWTRIGCTRL: %08X\n", (int)DMAC->SWTRIGCTRL.reg);
    printf("   PRICTRL0: %08X\n", (int)DMAC->PRICTRL0.reg);
    printf("    INTPEND: %04X\n", (int)DMAC->INTPEND.reg);
    printf("  INTSTATUS: %08X\n", (int)DMAC->INTSTATUS.reg);
    printf("     BUSYCH: %08X\n", (int)DMAC->BUSYCH.reg);
    printf("     PENDCH: %08X\n", (int)DMAC->PENDCH.reg);
    printf("     ACTIVE: %08X\n", (int)DMAC->ACTIVE.reg);
    printf("   BASEADDR: %08X\n", (int)DMAC->BASEADDR.reg);
    printf("    WRBADDR: %08X\n", (int)DMAC->WRBADDR.reg);
    printf("-----------------CHANNEL REGISTRES----------------\n");
    printf(" CH  CHCTRLA    CHCTRLB   CHINTENCLR CHINTENSET  CHINTFLAG  CHSTATUS  EVENTCNTRL\n");
    for(int i=0; i< MAX_USEABLE_DMA_CHANNEL; i++)
    {
        //csatorna kijelolese
        DmacChannel* ChRegs=&DMAC->Channel[i];

        printf(" %02d. %08x     %02x          %02x         %02x         %02x         %02x       %02X",
               i,
               (int)ChRegs->CHCTRLA.reg,
               (int)ChRegs->CHCTRLB.reg,
               (int)ChRegs->CHINTENCLR.reg,
               (int)ChRegs->CHINTENSET.reg,
               (int)ChRegs->CHINTFLAG.reg,
               (int)ChRegs->CHSTATUS.reg,
               (int)ChRegs->CHEVCTRL.reg
               );
        if (ChRegs->CHCTRLA.bit.ENABLE) printf(" ENABLED");

        if (ChRegs->CHINTFLAG.bit.SUSP) printf(" *SUSP");
        if (ChRegs->CHINTFLAG.bit.TCMPL)printf(" *TCMPL");
        if (ChRegs->CHINTFLAG.bit.TERR) printf(" *TERR");

        if (ChRegs->CHSTATUS.bit.BUSY) printf(" -BUSY");
        if (ChRegs->CHSTATUS.bit.PEND) printf(" -PEND");
        if (ChRegs->CHSTATUS.bit.FERR) printf(" -FERR");
        printf("\n");
    } //for

    printf("---------------CHANNEL DESCRIPTORS----------------\n");
    printf(" CH  BTCTRL  BTCNT   SRCADDR    DSTADDR    DESCADDR\n");
    for(int i=0; i<MAX_USEABLE_DMA_CHANNEL; i++)
    {
        printf(" %02d.  %04x    %04x   %08x   %08x   %08x",
               i,
               (int)MyDMA_firstDescriptors[i].BTCTRL.reg,
               (int)MyDMA_firstDescriptors[i].BTCNT.reg,
               (int)MyDMA_firstDescriptors[i].SRCADDR.reg,
               (int)MyDMA_firstDescriptors[i].DSTADDR.reg,
               (int)MyDMA_firstDescriptors[i].DESCADDR.reg);
        printf(" \n");
    }
    printf("---------------WRITE BACK DESCRIPTORS----------------\n");
    printf(" CH  BTCTRL  BTCNT   SRCADDR    DSTADDR    DESCADDR\n");
    for(int i=0; i<MAX_USEABLE_DMA_CHANNEL; i++)
    {
        printf(" %02d.  %04x    %04x   %08x   %08x   %08x",
               i,
               (int)MyDMA_writeBackMemory[i].BTCTRL.reg,
               (int)MyDMA_writeBackMemory[i].BTCNT.reg,
               (int)MyDMA_writeBackMemory[i].SRCADDR.reg,
               (int)MyDMA_writeBackMemory[i].DSTADDR.reg,
               (int)MyDMA_writeBackMemory[i].DESCADDR.reg);
        printf(" \n");
    }
    printf("-----------------------------------------------------\n");
    printf(" \n");

}
//------------------------------------------------------------------------------
