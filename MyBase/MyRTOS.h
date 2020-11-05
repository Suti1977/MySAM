//------------------------------------------------------------------------------
//  RTOS kezelest segito modul (FreeRTOS-hez)
//
//    File: MyRTOS.h
//------------------------------------------------------------------------------
#ifndef MY_RTOS_H_
#define MY_RTOS_H_


#ifdef USE_FREERTOS

//FreeRTOS-bol hasznalt gyakori inculde-ok
#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"
#include "task.h"
#include "stream_buffer.h"

//Megadott notify esemeny bitekre varakozas, timeout kezelessel.
//(A FreeRTOS xEventGroupWaitBits() helyett alkalmazzuk, ahol a taszkoknak event
//bit jellegu jelzeseket kell kuldeni.)
//waitedEvents: esemenybitek, melyekre varni kell.
//waitTime: a varakozas ideje, mig a waitedEvents-ben megadott valamelyik notify
//          bitje be nem all a taszknak.
//Visszateresi erteke: 0 - timeout/nincs esemeny
//                     nem 0 eseten, a taszknak kuldott es altalunk vart esemeny
//                     bitek es kapcsolata.
uint32_t MyRTOS_waitForNotifyEvents(uint32_t waitedEvents,
                                    TickType_t waitTime);

//64 bites sajat tick szamlalo lekerdezese. A rutinban a lekerdezes idejere
//critical section kerul nyitasra.
uint64_t MyRTOS_getTick(void);

//A megadott taszkrol irogat ki informaciokat a termianlra
void MyRTOS_printTaskInfo(TaskHandle_t TaskHandle);
//Kiirja konzolra a taszkokat, es azok informacioit
void MyRTOS_printTasks(void);

#endif //USE_FREERTOS
#endif //MY_RTOS_H_
