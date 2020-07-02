//------------------------------------------------------------------------------
//	Sajat esemeny alrendszer driver rutinok
//------------------------------------------------------------------------------
#include "MyEvent.h"
#include <stdio.h>

//------------------------------------------------------------------------------
//Esemenykezeles kezdeti initje
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
    printf("    CTRLA: %08x\n", hw->CTRLA.reg);
    printf("  INTPEND: %08x\n", hw->INTPEND.reg);
    printf("INTSTATUS: %08x\n", hw->INTSTATUS.reg);
    printf("    SWEVT: %08x\n", hw->SWEVT.reg);
    printf("\n");
    for(int i=0; i<12; i++)
    {
       printf("  CHANNEL%02d: %08x -->",i, hw->Channel[i].CHANNEL.reg);
       printf("  EVGEN=%002d",   hw->Channel[i].CHANNEL.bit.EVGEN);
       printf("  EDGESEL=%02x", hw->Channel[i].CHANNEL.bit.EDGSEL);

       switch(hw->Channel[i].CHANNEL.bit.PATH)
       {
        case EVSYS_CHANNEL_PATH_ASYNCHRONOUS_Val:   printf(" Async"); break;
        case EVSYS_CHANNEL_PATH_RESYNCHRONIZED_Val: printf(" Resync");break;
        case EVSYS_CHANNEL_PATH_SYNCHRONOUS_Val:    printf(" Sync");  break;
       }

       printf("\n");


    }
    printf("\nUsers:\n");
    int m=0;
    for(int i=0; i<5; i++)
    {
        printf("  %02d-%02d:   ",m, m+9);
        for(int j=0; j<10; j++)
        {
            printf("%02x ", EVSYS->USER[m].reg);
            m++;
        }
        printf("\n");
    }

    printf("========================================\n");
}
