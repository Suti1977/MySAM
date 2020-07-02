//------------------------------------------------------------------------------
//  RTOS kezelest segito modul (FreeRTOS-hez)
//
//    File: MyRTOS.c
//------------------------------------------------------------------------------
#ifdef USE_FREERTOS
#include "MyCommon.h"
#include "MyRTOS.h"
#include <stdio.h>

//FreeRTOS STUFF:
// http://ww1.microchip.com/downloads/en/appnotes/atmel-42382-getting-started-with-freertos-on-atmel-sam-flash-mcus_applicationnote_at04056.pdf

//Tickless-Idle mukodes:
//
//Ha a varakozasi ido nagyobb, mint a configEXPECTED_IDLE_TIME_BEFORE_SLEEP,
//akkor tickless idle modba valt a kernel. A Task.c-ben kerul meghivasra a
//prvIdleTask()-ban a portSUPPRESS_TICKS_AND_SLEEP() definicio, mely mogott a
//vPortSuppressTicksAndSleep() funkcio talalhto. Ennek bemeno parametere az a
//tick ido, ameddig a proci aluhat.
//Az alapertelmezett esetben a port.c-ben van ez weak attributummal definialva,
//melyben a SysTick timerhez van implementalva a mukodes. Ebben egyszeruen csak
//a SysTick idozitore allit be egy interruptot, ami majd ebreszti a procit.
//A kvarc orajele ilyenkor is aktiv.
//Ha tenyleg nagyon alacsony energiafelvetelre torekszunk, akkor az abban
//talalhato rutinokat implementalnunk kell...
//A FreeRTOS initkor hivja a vPortSetupTimerInterrupt() fuggvenyt, melyben
//Meg kell valositanunk SysTick timer es a tickless-idle alatt mukodo idozito
//initjeit. Ez egy week-elt fuggveny, es a port.c-ben talalhato eredetileg.
//A sajat 64 bites ido kezelesehez a vPortSuppressTicksAndSleep() fuggvenyt
//ujra implementaljuk, hogy a tickless idle miatti adjusztokat vegre tudjuk
//hajtani a sajat 64 bites MyRTOS_TickCnt szamlalonkon. A fuggvenyek eredetiet a
//port.c-bol vesszuk at.

//Sajat tick szamlalo, melyet a tick hookban novelunk. Ezt azert vezettuk be,
//hogy szemben a FreeRTOS 32 bites tickjevel, segeitsegevel atfordulas kezeles
//nelkul tudjunk idot merni.
//A valtozo a tick hook-ban novekszik. Tickless idle eseten viszont adjusztalni
//kell ennek is az erteket!
static uint64_t  __attribute__((aligned(8))) MyRTOS_tickCnt=0;


//Ebbe a bufferbe rakja ossze a taszkok informacios tablazatat, melyet mint
//szoveg, a kesobbiekben a terminalon megjelenithetunk.
static char MyRTOS_taskList[1024];


/* Constants required to manipulate the core.  Registers first... */
#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SYSPRI2_REG				( * ( ( volatile uint32_t * ) 0xe000ed20 ) )
/* ...then bits in the registers. */
#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define portNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )
#define portNVIC_PENDSVCLEAR_BIT 			( 1UL << 27UL )
#define portNVIC_PEND_SYSTICK_CLEAR_BIT		( 1UL << 25UL )

#ifndef configSYSTICK_CLOCK_HZ
    #define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
    /* Ensure the SysTick is clocked at the same frequency as the core. */
    #define portNVIC_SYSTICK_CLK_BIT	( 1UL << 2UL )
#else
    /* The way the SysTick is clocked is not modified in case it is not the same
    as the core. */
    #define portNVIC_SYSTICK_CLK_BIT	( 0 )
#endif

//------------------------------------------------------------------------------
//64 bites sajat tick szamlalo lekerdezese. A rutinban a lekerdezes idejere
//critical section kerul nyitasra.
uint64_t MyRTOS_getTick(void)
{
    register uint64_t Res;

    //Lekerdezesbe nem csaphat bele IT
    taskENTER_CRITICAL();

    Res=MyRTOS_tickCnt;

    taskEXIT_CRITICAL();

    return Res;
}
//------------------------------------------------------------------------------
//Tickless IDLE eseten hasznaljuk. Csak ugy mint az RTOS tick szamlalojat,
//ugy a sajat tick szamlalot is adjusztalni kell, az idle-ben toltott idovel.
static void inline MyRTOS_adjustMyTickCnt(uint32_t Adjust)
{
    MyRTOS_tickCnt+=Adjust;
}
//------------------------------------------------------------------------------
//FreeRTOS tick-enkent meghivodo hook rutin. Ebben valositjuk meg a sajat
//64 bites tick szamlalonkat.
//[INTERUPT alatt fut]
void vApplicationTickHook( void )
{
    //Sajat tick szamlalo novelese.
    MyRTOS_tickCnt++;
}
//------------------------------------------------------------------------------
//A megadott taszkrol irogat ki informaciokat a termianlra
#if( configUSE_TRACE_FACILITY == 1 )
void MyRTOS_printTaskInfo(TaskHandle_t TaskHandle)
{
    static TaskStatus_t xTaskDetails;

    vTaskGetInfo(TaskHandle, &xTaskDetails, pdTRUE, eInvalid);

    printf("..Task name: %s\n",       xTaskDetails.pcTaskName);
    printf("..Task num: %d\n",        (int)xTaskDetails.xTaskNumber);
    printf("..Task handle: %08X\n",   (int)xTaskDetails.xHandle);
    printf("..Stack base: %08X\n",    (int)xTaskDetails.pxStackBase);
    printf("..Current state: %d\n",   (int)xTaskDetails.eCurrentState);
    printf("..Current priority: %d\n",(int)xTaskDetails.uxCurrentPriority);
    printf("..Base priority: %d\n",   (int)xTaskDetails.uxBasePriority);
    printf("..Runtime counter: %d\n", (int)xTaskDetails.ulRunTimeCounter);
    printf("..Stack Watermak: %d\n",  (int)xTaskDetails.usStackHighWaterMark);
}
#endif
//------------------------------------------------------------------------------
//Kiirja konzolra a taszkokat, es azok informacioit
void MyRTOS_printTasks(void)
{
    vTaskList(MyRTOS_taskList);
    printf(("*******************************************\n"));
    printf(("Task          State    Prio    Stack    Num\n"));
    printf(("*******************************************\n"));
    printf("%s", MyRTOS_taskList);
    printf(("*******************************************\n"));
}
//------------------------------------------------------------------------------
//Akkor jon fel, ha a stack elfogyott valamelyik tasknal.
void __attribute__((noreturn)) vApplicationStackOverflowHook(TaskHandle_t pxCurrentTCB)
{
    printf("\n\nSTACK OVERFLOW!\n");

    MyRTOS_printTaskInfo(pxCurrentTCB);

    while(1);
}
//------------------------------------------------------------------------------
//Akkor jon fel, ha nem sikerult memoriat allokalnia
void __attribute__((noreturn)) vApplicationMallocFailedHook(void)
{
    printf("\n\nOUT OF MEMORY!\n");
    while(1);
}
//------------------------------------------------------------------------------
#if( configUSE_TICKLESS_IDLE == 1 )

//The number of SysTick increments that make up one tick period.
static uint32_t ulTimerCountsForOneTick = 0;
//The maximum number of tick periods that can be suppressed is limited by the
//24 bit resolution of the SysTick timer.
static uint32_t xMaximumPossibleSuppressedTicks = 0;
//Compensate for the CPU cycles that pass while the SysTick is stopped (low
//power functionality only.
static uint32_t ulStoppedTimerCompensation = 0;
//The systick is a 24-bit counter.
#define portMAX_24_BIT_NUMBER				( 0xffffffUL )
//A fiddle factor to estimate the number of SysTick counts that would have
//occurred while the SysTick counter is stopped during tickless idle
//calculations.
#define portMISSED_COUNTS_FACTOR			( 45UL )
//..............................................................................
//Tickless idle idozitest megvalosito fuggveny, melyet az eredeti port.c-bol
//masoltunk at, mivel abba boviteseket kell eszkozolni. (Az eredeti fuggveny
//weak-elve van.)
//[WEAK]
void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
{
    uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements;
    TickType_t xModifiableIdleTime;

    /* Make sure the SysTick reload value does not overflow the counter. */
    if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
    {
        xExpectedIdleTime = xMaximumPossibleSuppressedTicks;
    }

    /* Stop the SysTick momentarily.  The time the SysTick is stopped for
    is accounted for as best it can be, but using the tickless mode will
    inevitably result in some tiny drift of the time maintained by the
    kernel with respect to calendar time. */
    portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;

    /* Calculate the reload value required to wait xExpectedIdleTime
    tick periods.  -1 is used because this code will execute part way
    through one of the tick periods. */
    ulReloadValue = portNVIC_SYSTICK_CURRENT_VALUE_REG +
                    ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );
    if( ulReloadValue > ulStoppedTimerCompensation )
    {
        ulReloadValue -= ulStoppedTimerCompensation;
    }

    /* Enter a critical section but don't use the taskENTER_CRITICAL()
    method as that will mask interrupts that should exit sleep mode. */
    __asm volatile( "cpsid i" ::: "memory" );
    __asm volatile( "dsb" );
    __asm volatile( "isb" );

    /* If a context switch is pending or a task is waiting for the scheduler
    to be unsuspended then abandon the low power entry. */
    if( eTaskConfirmSleepModeStatus() == eAbortSleep )
    {
        /* Restart from whatever is left in the count register to complete
        this tick period. */
        portNVIC_SYSTICK_LOAD_REG = portNVIC_SYSTICK_CURRENT_VALUE_REG;

        /* Restart SysTick. */
        portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

        /* Reset the reload register to the value required for normal tick
        periods. */
        portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;

        /* Re-enable interrupts - see comments above the cpsid instruction()
        above. */
        __asm volatile( "cpsie i" ::: "memory" );
    }
    else
    {
        /* Set the new reload value. */
        portNVIC_SYSTICK_LOAD_REG = ulReloadValue;

        /* Clear the SysTick count flag and set the count value back to
        zero. */
        portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

        /* Restart SysTick. */
        portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

        /* Sleep until something happens.  configPRE_SLEEP_PROCESSING() can
        set its parameter to 0 to indicate that its implementation contains
        its own wait for interrupt or wait for event instruction, and so wfi
        should not be executed again.  However, the original expected idle
        time variable must remain unmodified, so a copy is taken. */
        xModifiableIdleTime = xExpectedIdleTime;
        configPRE_SLEEP_PROCESSING( xModifiableIdleTime );
        if( xModifiableIdleTime > 0 )
        {
            __asm volatile( "dsb" ::: "memory" );
            __asm volatile( "wfi" );
            __asm volatile( "isb" );
        }
        configPOST_SLEEP_PROCESSING( xExpectedIdleTime );

        /* Re-enable interrupts to allow the interrupt that brought the MCU
        out of sleep mode to execute immediately.  see comments above
        __disable_interrupt() call above. */
        __asm volatile( "cpsie i" ::: "memory" );
        __asm volatile( "dsb" );
        __asm volatile( "isb" );

        /* Disable interrupts again because the clock is about to be stopped
        and interrupts that execute while the clock is stopped will increase
        any slippage between the time maintained by the RTOS and calendar
        time. */
        __asm volatile( "cpsid i" ::: "memory" );
        __asm volatile( "dsb" );
        __asm volatile( "isb" );

        /* Disable the SysTick clock without reading the
        portNVIC_SYSTICK_CTRL_REG register to ensure the
        portNVIC_SYSTICK_COUNT_FLAG_BIT is not cleared if it is set.  Again,
        the time the SysTick is stopped for is accounted for as best it can
        be, but using the tickless mode will inevitably result in some tiny
        drift of the time maintained by the kernel with respect to calendar
        time*/
        portNVIC_SYSTICK_CTRL_REG =
                    ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT );

        /* Determine if the SysTick clock has already counted to zero and
        been set back to the current reload value (the reload back being
        correct for the entire expected idle time) or if the SysTick is yet
        to count to zero (in which case an interrupt other than the SysTick
        must have brought the system out of sleep mode). */
        if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT )!=0)
        {
            uint32_t ulCalculatedLoadValue;

            /* The tick interrupt is already pending, and the SysTick count
            reloaded with ulReloadValue.  Reset the
            portNVIC_SYSTICK_LOAD_REG with whatever remains of this tick
            period. */
            ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL ) -
                        ( ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG );

            /* Don't allow a tiny value, or values that have somehow
            underflowed because the post sleep hook did something
            that took too long. */
            if( ( ulCalculatedLoadValue < ulStoppedTimerCompensation ) ||
                ( ulCalculatedLoadValue > ulTimerCountsForOneTick ) )
            {
                ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL );
            }

            portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;

            /* As the pending tick will be processed as soon as this
            function exits, the tick value maintained by the tick is stepped
            forward by one less than the time spent waiting. */
            ulCompleteTickPeriods = xExpectedIdleTime - 1UL;
        }
        else
        {
            /* Something other than the tick interrupt ended the sleep.
            Work out how long the sleep lasted rounded to complete tick
            periods (not the ulReload value which accounted for part
            ticks). */
            ulCompletedSysTickDecrements =
                    ( xExpectedIdleTime * ulTimerCountsForOneTick ) -
                    portNVIC_SYSTICK_CURRENT_VALUE_REG;

            /* How many complete tick periods passed while the processor
            was waiting? */
            ulCompleteTickPeriods =
                    ulCompletedSysTickDecrements / ulTimerCountsForOneTick;

            /* The reload value is set to whatever fraction of a single tick
            period remains. */
            portNVIC_SYSTICK_LOAD_REG =
                    (( ulCompleteTickPeriods + 1UL ) * ulTimerCountsForOneTick)-
                    ulCompletedSysTickDecrements;
        }

        /* Restart SysTick so it runs from portNVIC_SYSTICK_LOAD_REG
        again, then set portNVIC_SYSTICK_LOAD_REG back to its standard
        value. */
        portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
        portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;
        vTaskStepTick( ulCompleteTickPeriods );

        //Sajat 64 bites tick idozito adjusztalasa.
        MyRTOS_adjustMyTickCnt( ulCompleteTickPeriods );

        portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;

        /* Exit with interrupts enabled. */
        __asm volatile( "cpsie i" ::: "memory" );
    }
}
#endif //configUSE_TICKLESS_IDLE
//..............................................................................
//Setup the systick timer to generate the tick interrupts at the required
//frequency.
//port.c-bol atveve
//[WEAK]
void vPortSetupTimerInterrupt( void )
{
    /* Calculate the constants required to configure the tick interrupt. */
    #if( configUSE_TICKLESS_IDLE == 1 )
    {
        ulTimerCountsForOneTick =
                ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );

        xMaximumPossibleSuppressedTicks =
                portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;

        ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR /
                                ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );
    }
    #endif /* configUSE_TICKLESS_IDLE */

    /* Stop and clear the SysTick. */
    portNVIC_SYSTICK_CTRL_REG = 0UL;
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

    /* Configure SysTick to interrupt at the requested rate. */
    portNVIC_SYSTICK_LOAD_REG=(configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ)-1UL;

    portNVIC_SYSTICK_CTRL_REG=(portNVIC_SYSTICK_CLK_BIT |
                               portNVIC_SYSTICK_INT_BIT |
                               portNVIC_SYSTICK_ENABLE_BIT );
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha az RTOS idle taszkja fut, nincsenek folyamatok.
void vApplicationIdleHook( void )
{

}
//------------------------------------------------------------------------------
#endif //USE_FREERTOS
