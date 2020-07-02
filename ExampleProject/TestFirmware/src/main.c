#include "MyCommon.h"
#include <stdio.h>
#include "MyGPIO.h"
#include "Console.h"
#include "resources.h"
#include "Tasks_config.h"
#include "board.h"
#include "SEGGER_SYSVIEW.h"

static TaskHandle_t      main_TestTaskHandle;

static void main_printChipInfo(void);
//------------------------------------------------------------------------------
static const uint32_t main_pinConfigs[]=
{
    PIN_STATUS_LED1_CONFIG,
    PINCONFIG_END
};
//------------------------------------------------------------------------------
static void __attribute__((noreturn)) main_test_task(void *p)
{
    (void)p;

    while (1)
    {     
        MyGPIO_toggle(PIN_STATUS_LED1);
        vTaskDelay(500);
    }
}
//------------------------------------------------------------------------------
int main(void)
{
    SEGGER_SYSVIEW_Conf();

    //ASF kornyezet kezdeti initje. A WEB feluleten osszekattogtatott
    //beallitasok alkalmazasa a periferiakra es a szoftver komponensekre.
    atmel_start_init();

    //Portok inicializalasa. Engedelyezi minden portra a a bemeneti sampling-et.
    MyGPIO_initPorts();

    //Nehany alapveto PIN beallitsa
    MyGPIO_pinGroupConfig(main_pinConfigs);

    printf("START...\n");
    main_printChipInfo();

    //Sororsportos es RTT alapu konzolok inicializalasa
    Console_init();
	
    //Teszt taszk letrehozasa...
    if (xTaskCreate(main_test_task,
                    "TestTask",
                    TEST_TASK_STACK_SIZE,
                    NULL,
                    TEST_TASK_PRIORITY,
                    &main_TestTaskHandle)!= pdPASS)
    {
        goto error;
    }


    //scheduler inditasa...
    vTaskStartScheduler();

    //<--Ide mar nem terhet vissza a vezerles
error:


    while(1);
}

//------------------------------------------------------------------------------
//A hasznalt MCU azonositoinak kiirasa a konzolra
static void main_printChipInfo(void)
{
    printf("===================CHIP INFO==============\n");
    printf("  Device ID:%08x\n", (int)DSU->DID.reg);
    printf("     DEVSEL:%02x\n", DSU->DID.bit.DEVSEL);
    printf("        DIE:%02x\n", DSU->DID.bit.DIE);
    printf("     FAMILY:%02x\n", DSU->DID.bit.FAMILY);
    printf("  PROCESSOR:%02x\n", DSU->DID.bit.PROCESSOR);
    printf("   REVISION:%02x\n", DSU->DID.bit.REVISION);
    printf("     SERIES:%02x\n", DSU->DID.bit.SERIES);
    printf("==========================================\n");
}
//------------------------------------------------------------------------------
void __attribute__((noreturn)) MemManagement_Handler(void)
{
    while(1);
}
void __attribute__((noreturn)) BusFault_Handler(void)
{
    while(1);
}
void __attribute__((noreturn)) UsageFault_Handler(void)
{
    while(1);
}
//------------------------------------------------------------------------------
