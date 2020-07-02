//------------------------------------------------------------------------------
//  A kulombozo hibak lekezelesenek/naplozasanak modulja
//------------------------------------------------------------------------------
#include <string.h>
#include <stdio.h>
#include "MyCommon.h"

void __attribute__((noreturn)) HardFault_Handler_C(unsigned long * hardfault_args, unsigned int lr_value);
//------------------------------------------------------------------------------
void __attribute__((noreturn)) HardFault_Handler_C  (unsigned long * hardfault_args, unsigned int lr_value)
{
    unsigned long stacked_r0;
    unsigned long stacked_r1;
    unsigned long stacked_r2;
    unsigned long stacked_r3;
    unsigned long stacked_r12;
    unsigned long stacked_lr;
    unsigned long stacked_pc;
    unsigned long stacked_psr;
    unsigned long cfsr;
    unsigned long bus_fault_address;
    unsigned long memmanage_fault_address;


    bus_fault_address = SCB->BFAR;
    memmanage_fault_address = SCB->MMFAR;
    cfsr = SCB->CFSR;
    stacked_r0 = ((unsigned long) hardfault_args[0]);
    stacked_r1 = ((unsigned long) hardfault_args[1]);
    stacked_r2 = ((unsigned long) hardfault_args[2]);
    stacked_r3 = ((unsigned long) hardfault_args[3]);
    stacked_r12 = ((unsigned long) hardfault_args[4]);
    stacked_lr = ((unsigned long) hardfault_args[5]);
    stacked_pc = ((unsigned long) hardfault_args[6]);
    stacked_psr = ((unsigned long) hardfault_args[7]);


    printf ("\n\n\n...Hard Fault!\n");
    printf (".- Stack frame:\n");
    printf (". PC = %08X\n", (int)stacked_pc);
    printf (". LR = %08X\n", (int)stacked_lr);
    printf (". R0 = %08X\n", (int)stacked_r0);
    printf (". R1 = %08X\n", (int)stacked_r1);
    printf (". R2 = %08X\n", (int)stacked_r2);
    printf (". R3 = %08X\n", (int)stacked_r3);
    printf (". R12= %08X\n", (int)stacked_r12);
    printf (". PSR= %08X\n", (int)stacked_psr);
    printf (".- FSR/FAR:\n");
    printf (". CFSR = %08X\n", (int)cfsr);
    printf (". HFSR = %08X\n", (int)SCB->HFSR);
    printf (". DFSR = %08X\n", (int)SCB->DFSR);
    printf (". AFSR = %08X\n", (int)SCB->AFSR);
    if (cfsr & 0x0080) printf (". MMFAR = %08X\n", (int)memmanage_fault_address);
    if (cfsr & 0x8000) printf (". BFAR = %08X\n" , (int)bus_fault_address);
    printf (".- Misc\n");
    printf (". LR/EXC_RETURN= %08X\n", lr_value);

    while(1)
    {
        __NOP();
    }
}
//------------------------------------------------------------------------------
//Hard fault kivetel eseten feljovo handler. WRAPPER
//A rutin eldonti, hogy melyik stack van hasznalva.
//A rutin meghivja a C-s implementaciot
void __attribute__((naked)) HardFault_Handler(void)
{
    __asm__ __volatile__
    (
        "   tst     lr, #0x4            \n\t"
        "   ite     eq                  \n\t"
        "   mrseq   r0, msp             \n\t"
        "   mrsne   r0, psp             \n\t"
        "   b       HardFault_Handler_C \n\t"
    );
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
