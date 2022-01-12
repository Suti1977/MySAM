//------------------------------------------------------------------------------
//	Sajat esemeny alrendszer driver rutinok
//------------------------------------------------------------------------------
#include "MyEvent.h"
#include <stdio.h>

//------------------------------------------------------------------------------
//Esemenykezeles kezdeti initje
//ELOTTE EROSEN AJANLOTT LEALLITANI A FUTO ESEMENY GENERATOROKAT!
void MyEvent_init(void)
{
    //esemenyrendszer orajelenek engedelyezese
    MCLK->APBBMASK.bit.EVSYS_=1;

    //Esemeny alrendszer szoftveres resetje
    EVSYS->CTRLA.bit.SWRST=1;


}
//------------------------------------------------------------------------------
//Esemeny alrendszerrol debug informaciok kiirasa a konzolra
void MyEvent_printDebugInfo(void)
{
    Evsys* hw=EVSYS;
    printf("============EVENT SYS INFO==============\n");
    printf("    CTRLA: %08x\n", (int)hw->CTRLA.reg);
    printf("  INTPEND: %08x\n", (int)hw->INTPEND.reg);
    printf("INTSTATUS: %08x\n", (int)hw->INTSTATUS.reg);
    printf("    SWEVT: %08x\n", (int)hw->SWEVT.reg);
    printf("\n");
    for(uint16_t i=0; i<ARRAY_SIZE(hw->Channel); i++)
    {
       printf("  CHANNEL%02d: %08x -->",i, (int)hw->Channel[i].CHANNEL.reg);
       printf("  EVGEN=%02d",   (int)hw->Channel[i].CHANNEL.bit.EVGEN);
       printf("  EDGESEL=%02x", (int)hw->Channel[i].CHANNEL.bit.EDGSEL);

       switch(hw->Channel[i].CHANNEL.bit.PATH)
       {
        case EVSYS_CHANNEL_PATH_ASYNCHRONOUS_Val:   printf(" Async"); break;
        case EVSYS_CHANNEL_PATH_RESYNCHRONIZED_Val: printf(" Resync");break;
        case EVSYS_CHANNEL_PATH_SYNCHRONOUS_Val:    printf(" Sync");  break;
       }

       printf("\n");


    }
    printf("\nUsers:\n");
    uint16_t m=0;
    for(uint16_t i=0; i<10; i++)
    {
        printf("  %02d-%02d:   ",m, m+9);
        for(uint16_t j=0; j<10; j++)
        {
            printf("%02x ", (int)EVSYS->USER[m].reg);
            m++;
            if (m >= ARRAY_SIZE(EVSYS->USER))
            {
                printf("\n");
                goto exit;
            }
        }
        printf("\n");
    }
exit:
    printf("========================================\n");
}
