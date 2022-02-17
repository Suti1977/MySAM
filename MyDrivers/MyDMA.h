//------------------------------------------------------------------------------
//  Sajat DMA kezelo rutinok
//------------------------------------------------------------------------------
#ifndef MY_DMA_H
#define MY_DMA_H

#include "MyCommon.h"
//------------------------------------------------------------------------------
//A legmagasabb hasznalt DMA csatorna szama.
//Ez befolyasolja az allokalt memoriateruletet, mivel a deszkriptoroknak az
//SRAM-bol ki kell szakitani egy reszt.
//[Globalisan feluldefinialhato.]
#ifndef MAX_USEABLE_DMA_CHANNEL
#define MAX_USEABLE_DMA_CHANNEL     8
#endif
//------------------------------------------------------------------------------
//DMA kezeles hibakodok
enum
{
    kMyDMAStatus_= MAKE_STATUS(kStatusGroup_MyDMA, 0),
    //Nem kezelt DMA csatorna. (>=MAX_USEABLE_DMA_CHANNEL)
    kMyDMAStatus_InvalidChannelIndex,
};
//------------------------------------------------------------------------------
//Az SRAM-bol a DMA kezeles deszkriptorainak memoria foglalasa. Erre mutat a DMA
//vezerloben a BASEADDR mezo is.
extern DmacDescriptor  MyDMA_firstDescriptors[MAX_USEABLE_DMA_CHANNEL];

//DMA vezerlo ide irja vissza a statusz informaciot. (Erre mutat a DMAC-ben a
//WRBADDR regiszter.
extern DmacDescriptor MyDMA_writeBackMemory[];
//------------------------------------------------------------------------------
#define MYDMA_ENTER_CRITICAL()  MY_ENTER_CRITICAL()
#define MYDMA_LEAVE_CRITICAL()  MY_LEAVE_CRITICAL()

//DMA csatornan szoftveres trigger kivaltasa. Specialis esetekben szabad csak
//hasznalni, mivel nincs megoldva a muvelet alatti interruptok tiltasa.
#define MYDMA_SWTRIGGER(ch) DMAC->SWTRIGCTRL.reg |= (1<<ch)

//DMA csatorna deszkriptoranak beallitasara szolgalo makrok.
//A makrok kozvetlen a csatornak elso deszkriptorait irjak. Nem vegeznek elle-
//norzest!
#define MYDMA_SET_BTCTRL(ch, val)   MyDMA_firstDescriptors[ch].BTCTRL.reg=(uint16_t)val
#define MYDMA_SET_BTCNT(ch, val)    MyDMA_firstDescriptors[ch].BTCNT.reg=(uint16_t)val
#define MYDMA_SET_SRCADDR(ch, val)  MyDMA_firstDescriptors[ch].SRCADDR.reg=(uint32_t)val
#define MYDMA_SET_DSTADDR(ch, val)  MyDMA_firstDescriptors[ch].DSTADDR.reg=(uint32_t)val
#define MYDMA_SET_DESCADDR(ch, val) MyDMA_firstDescriptors[ch].DESCADDR.reg=(uint32_t)val

//WriteBack DMA teruleten talalhato mezok lekerdezeset lehetove tevo makrok
#define MYDMA_GET_WB_BTCTRL(ch)     MyDMA_writeBackMemory[ch].BTCTRL.reg
#define MYDMA_GET_WB_BTCNT(ch)      MyDMA_writeBackMemory[ch].BTCNT.reg
#define MYDMA_GET_WB_SRCADDR(ch)    MyDMA_writeBackMemory[ch].SRCADDR.reg
#define MYDMA_GET_WB_DSTADDR(ch)    MyDMA_writeBackMemory[ch].DSTADDR.reg
#define MYDMA_GET_WB_DESCADDR(ch)   MyDMA_writeBackMemory[ch].DESCADDR.reg


//Deszkriptor kozvetlen beallitasa
#define MYDMA_SET_DESCRIPTOR(ch, descriptor) MyDMA_firstDescriptors[ch]=descriptor
//------------------------------------------------------------------------------
//DMA vezerlo inicializalasanal megadando struktura.
typedef struct
{
    //Round-Robin arbitalasi modok engedelyezese a 4 szintre
    bool    roundRobinEn0;
    bool    roundRobinEn1;
    bool    roundRobinEn2;
    bool    roundRobinEn3;
    //QOS Level Quality of Service QoS Name Description
    //0x0 DISABLE Background (no sensitive operation)
    //0x1 LOW Sensitive to bandwidth
    //0x2 MEDIUM Sensitive to latency
    //0x3 Critical Latency Critical Latency
    uint32_t qos0;
    uint32_t qos1;
    uint32_t qos2;
    uint32_t qos3;
    //Level Channel Priority Number (lasd adatlap)
    uint32_t priorityLevel0;
    uint32_t priorityLevel1;
    uint32_t priorityLevel2;
    uint32_t priorityLevel3;

    //A DMA vezerlohoz tartozo IRQ-k kezdeti prioritasanak bealitasa
    uint32_t defaultIrqPriority;
} MyDMA_Config_t;
//------------------------------------------------------------------------------
//DMA csatorna beallitasanal megadando struktura.
typedef struct
{
    //A DMA atvitelt kezdemenyezo hardveres triggerforras azonositoja
    uint32_t triggerSource;
    //TriggerAction beallitasa. (0-block, 1-x, 2-Burst, 3-Transaction)
    //  DMAC_CHCTRLA_TRIGACT_BLOCK_Val
    //  DMAC_CHCTRLA_TRIGACT_BURST_Val
    //  DMAC_CHCTRLA_TRIGACT_TRANSACTION_Val
    uint32_t triggerAction;
    //Burst hossza
    //  DMAC_CHCTRLA_BURSTLEN_SINGLE_Val
    //  DMAC_CHCTRLA_BURSTLEN_2_Val
    //  DMAC_CHCTRLA_BURSTLEN_3_Val
    //  ...
    //  DMAC_CHCTRLA_BURSTLEN_16_Val
    uint32_t burstLen;
    //FIFO Tresshold
    //  DMAC_CHCTRLA_THRESHOLD_1BEAT_Val
    //  DMAC_CHCTRLA_THRESHOLD_2BEATS_Val
    //  DMAC_CHCTRLA_THRESHOLD_4BEATS_Val
    //  DMAC_CHCTRLA_THRESHOLD_8BEATS_Val
    uint32_t threshold;

    //Eventcontrol regiszter erteke. DMAC_CHEVCTRL_ definiciokkal osszeallitando
    DMAC_CHEVCTRL_Type eventControl;

    //Az adott DMA csatorna melyik prioritasi szinten mukodik (0..3)
    uint8_t priorityLevel;
} MyDMA_ChannelConfig_t;
//------------------------------------------------------------------------------
//DMA csatorna adatatvitelere vonatkozo beallitasok. Ezek a satorna transzfer
//deszkriptoraba irodnak.
typedef struct
{
    //Block Event Output Selection
    //  0-   DMAC_BTCTRL_EVOSEL_DISABLE_Val
    //  1-   DMAC_BTCTRL_EVOSEL_BLOCK_Val
    //  3-   DMAC_BTCTRL_EVOSEL_BURST_Val
    uint16_t eventOutputSelectoin;

    //Block Action
    //  0-  DMAC_BTCTRL_BLOCKACT_NOACT_Val
    //  1-  DMAC_BTCTRL_BLOCKACT_INT_Val
    //  2-  DMAC_BTCTRL_BLOCKACT_SUSPEND_Val
    //  3-  DMAC_BTCTRL_BLOCKACT_BOTH_Val
    uint16_t blockAction;

    //Beat Size
    //  0-  DMAC_BTCTRL_BEATSIZE_BYTE_Val   8 bit
    //  1-  DMAC_BTCTRL_BEATSIZE_HWORD_Val  16 bit
    //  2-  DMAC_BTCTRL_BEATSIZE_WORD_Val   32 bit
    uint16_t beatSize;

    //Source Address Increment Enable
    bool    srcAddrIncrementEnable;

    ///Destination Address Increment Enable
    bool    dstAddrIncrementEnable;

    //Step Selection
    //  0-  DMAC_BTCTRL_STEPSEL_DST_Val
    //          Step size settings apply to the destination addres
    //  1-  DMAC_BTCTRL_STEPSEL_SRC_Val
    //          Step size settings apply to the source address
    uint16_t stepSlect;

    //Address Increment Step Size
    //  0-  DMAC_BTCTRL_STEPSIZE_X1_Val     Next ADDR = ADDR+(1<<BEATSIZE)*1
    //  1-  DMAC_BTCTRL_STEPSIZE_X2_Val     Next ADDR = ADDR+(1<<BEATSIZE)*2
    //  2-  DMAC_BTCTRL_STEPSIZE_X4_Val     Next ADDR = ADDR+(1<<BEATSIZE)*4
    //  3-  DMAC_BTCTRL_STEPSIZE_X8_Val     Next ADDR = ADDR+(1<<BEATSIZE)*8
    //  4-  DMAC_BTCTRL_STEPSIZE_X16_Val    Next ADDR = ADDR+(1<<BEATSIZE)*16
    //  5-  DMAC_BTCTRL_STEPSIZE_X32_Val    Next ADDR = ADDR+(1<<BEATSIZE)*32
    //  6-  DMAC_BTCTRL_STEPSIZE_X64_Val    Next ADDR = ADDR+(1<<BEATSIZE)*64
    //  7-  DMAC_BTCTRL_STEPSIZE_X128_Val   Next ADDR = ADDR+(1<<BEATSIZE)*128
    uint16_t stepSize;

    //Block Transfer Count
    uint16_t blockTransferCount;
    //Block Transfer Source Address
    void*   src;
    //Block Transfer Destination Address
    void*   dst;
    //Next Descriptor Address
    void*   nextDescriptor;
} MyDMA_TransferConfig_t;
//------------------------------------------------------------------------------
//Megszakitas eseten hivott callback rutinok definicio. Ilyen tipusokat lehet
//beregisztralni a MyDMA_registerCallbacks() fuggvennyel...
typedef void MyDMA_doneFunc_t(void* callbackData);
typedef void MyDMA_errorFunc_t(void* callbackData);
//------------------------------------------------------------------------------
//DMA csatornak kezelesehez tartozo valtozok.
typedef struct
{
    //Interrupt eseten feljovo callback fuggvenypointerek
    MyDMA_doneFunc_t*    doneFunc;
    MyDMA_errorFunc_t*   errorFunc;
    //A callbackeknek atadott tetszoleges valtozo
    void*   callbackData;
} MyDMA_Channel_t;
//------------------------------------------------------------------------------
//DMA kezeles kezdeti initje.
void MyDMA_init(MyDMA_Config_t* cfg);
//Default beallitasokkal struktrura feltoltese
void MyDMA_getDefaultConfig(MyDMA_Config_t* cfg);

//csatorna beallitasa, es engedelyezese.
//A CHCTRLB_Value ertekben be kell allitgatni a csatorna jellemzoit
// pl prioritas, esemeny kimenet dolgai, trigger forrast, stb...
status_t MyDMA_configureChannel(uint8_t channel, MyDMA_ChannelConfig_t* cfg);
//Default beallitasokkal csatorna config struktrura feltoltese
void MyDMA_getDefaultChannelConfig(MyDMA_ChannelConfig_t* cfg);

//DMA deszkriptor beallitasa.
//Ezt tudjuk hasznalni sajat linkelt deszkriptorok epitesere is
void MyDMA_buildDescriptor(DmacDescriptor* desc,
                           const MyDMA_TransferConfig_t* cfg);

//Csatorna atviteli parameterek beallitasa az elso deszkriptorba.
//A rutin egyben validra is allitja a deszkriptort
status_t MyDMA_configureTransfer(uint8_t channel,
                                 const MyDMA_TransferConfig_t* cfg);

//Defalt beallitasokkal csatorna transzfer konfiguracio feltoltese
void MyDMA_getDefaultTransferConfig(MyDMA_TransferConfig_t* cfg);

//Elore osszeallitott deszkriptor beallitasa az adott csatornara
void MyDMA_setDescriptor(uint8_t channel, DmacDescriptor* desc);

//Egy DMA csatorna engedelyezese megszakitasbol --> Elindul az atvitel, ha van
//trigger
void inline MyDMA_enableChannelFromIsr(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];

    //Engedelyezes
    //#undef ENABLE
    chRegs->CHCTRLA.bit.ENABLE = 1;
}


//Egy DMA csatorna engedelyezese -->Elindul az atvitel, ha van trigger
void MyDMA_enableChannel(uint8_t channel);

//Egy DMA csatorna tiltasa megszakitasbol
void inline MyDMA_disableChannelFromIsr(uint8_t channel)
{
    DmacChannel* chRegs=&DMAC->Channel[channel];
    //tiltas
    //#undef ENABLE
    chRegs->CHCTRLA.bit.ENABLE = 0;
}

//Egy DMA csatorna tiltasa
void MyDMA_disableChannel(uint8_t channel);

//Egy DMA csatorna suspendolasa
void MyDMA_suspendChannel(uint8_t channel);

//Egy DMA csatorna mukodes folytatasa szoftveresen
void MyDMA_resumeChannel(uint8_t channel);

//Egy DMA csatorna ujrainditasa
void MyDMA_restartChannel(uint8_t channel);

//Csatornahoz cel cim beallitasa.
void MyDMA_setDestAddress(uint8_t channel, const void *const dst);

//Csatornahoz forras cim beallitasa.
void MyDMA_setSrcAddress(uint8_t channel, const void *const src);

//A csatornahoz tartozo kovetkezo DMA leiro beallitasa
void MyDMA_setNextDescriptor(uint8_t channel,
                             DmacDescriptor* nextDescriptor);

//Forrascim noveles engedelyezese/tiltasa
void MyDMA_setSrcAddressIncrement(uint8_t channel, bool enable);

//Celcim noveles engedelyezese/tiltasa
void MyDMA_setDstAddressIncrement(uint8_t channel, bool enable);

//A DMA atvitelkor atviendo elemek szama
//Fontos! Elotte az SRC/DST cimeket, es azok novelesenek engedelyezeset
//be kell allitani! A rutin a beallitott bitek alapjan szamolja ki az SRC es
//DST cimeket, mivel azokhoz az eredeti poziciohoz kepest hozza kell adni!
void MyDMA_setDataAmount(DmacDescriptor* desc, const uint32_t amount);

//Tranzakcio engedelyezese a csatornan
//void MyDMA_EnableTransaction(uint8_t Channel);

//Szoftveres trigger kiadasa
void MyDMA_swTrigger(uint8_t channel);

//Statusz callbackek beregisztralasa a megadott csatornahoz.
//Ha valamelyik callback nincs hasznalva, akkor helyette NULL-t kell megadni.
void MyDMA_registreCallbacks(   uint8_t channelNr,
                                MyDMA_doneFunc_t* doneFunc,
                                MyDMA_errorFunc_t* errorFunc,
                                void* callbackData);

//DMA adatok kiirsa a konzolra
void MyDMA_printDebugInfo(void);
//------------------------------------------------------------------------------
#endif // MY_DMA_H
