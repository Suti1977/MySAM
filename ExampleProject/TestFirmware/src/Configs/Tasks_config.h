//------------------------------------------------------------------------------
//A taszkok es interruptok parameterei
//------------------------------------------------------------------------------
#ifndef TASKS_CONFIG_H
#define TASKS_CONFIG_H

#include "MyCommon.h"
//------------------------------------------------------------------------------
//Megj: minel nagyobb szamerteket allitunk be egy taszkhoz, annal magasabb a
//      prioritasa.


#define TEST_TASK_STACK_SIZE                (1024 / sizeof(portSTACK_TYPE))
#define TEST_TASK_PRIORITY                  (tskIDLE_PRIORITY + 3)

//Sorosportos konzolt futtato taszk parameterei
#define SERIAL_CONSOLE_TASK_STACKS_SIZE     (1024 / sizeof(portSTACK_TYPE))
#define SERIAL_CONSOLE_TASK_PRIORITY        (tskIDLE_PRIORITY + 1)

//RTT konzolt futtato taszk parameterei
#define RTT_CONSOLE_TASK_STACKS_SIZE        (1024 / sizeof(portSTACK_TYPE))
#define RTT_CONSOLE_TASK_PRIORITY           (tskIDLE_PRIORITY + 1)

//A konzolok azonos prioritason legyenek, es lehetoleg a legmagasabb prioritast
//kapjak a taszkok kozul!
#define RTT_POLL_TASK_STACKS_SIZE           (256 / sizeof(portSTACK_TYPE))
#define RTT_POLL_TASK_PRIORITY              (tskIDLE_PRIORITY + 2)

//------------------------------------------------------------------------------
//Periferia megszakiatsaok (IRQ) definialasa.
//
//Megj: minel kisebb szamerteket allitunk be egy periferiahoz, annal magasabb a
//      prioritasa.
//ATSAME mikrovezerlon 0..7 kozott lehetnek a megszakitasok. (3 prioritas bit)
// 0 : Fenntartjuk nagyon magas prioritasu periferiakezeleshez, melyet az OS
//     sem tud blockolni a Critical section-okkel.
//
// 1-6: ezeket hasznalhatjuk a periferiakhoz.
//
// 7 : ezen fut a kernel, ennek kell a legalacsonyabbnak lennie. Ezt a szintet
//     nem hasznalhatja az applikacio.
#define IRQ_PRIORITY__MAX   1
#define IRQ_PRIORITY__MIN   6

//sorosportos konzolhoz tartozo sercom prioritasa
#define IRQ_PRIORITY__CONSOLE_UART      (IRQ_PRIORITY__MIN-0)

#endif //TASKS_CONFIG_H
