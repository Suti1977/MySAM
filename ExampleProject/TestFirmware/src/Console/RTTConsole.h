//------------------------------------------------------------------------------
//  Segger RTT-n keresztul kommunikalo konzol modul
//
//    File: RTTConsole.h
//------------------------------------------------------------------------------
#ifndef RTTCONSOLE_H_
#define RTTCONSOLE_H_

#include "MyConsole.h"

//A sorosportos parancsok max ilyen hosszuak lehetnek. Ekkora sorbuffert foglal
#define RTT_CONSOLE_CMD_LINE_LENGTH 128
//RTT-t olvaso buffer merete. Segitsegevel megoldjuk, hogy ne csak
//karakterenkent, hanem maximum ekkora blokkokban legyen olvasva az RTT.
#define RTT_CONSOLE_BUFFER_SIZE     64
//------------------------------------------------------------------------------
//RTTConsole valtozoi
typedef struct
{
    //Konzol kezelo alap modul
    MyConsole_t console;

    //Parancssor buffer. Ebbe rakja ossze a parancsort.
    char lineBuffer[RTT_CONSOLE_CMD_LINE_LENGTH];

    //RTT-t olvaso buffer. Segitsegevel megoldjuk, hogy ne csak karakterenkent
    //legyen ranezve az RTT-re.
    char buffer[RTT_CONSOLE_BUFFER_SIZE];

  #ifdef USE_FREERTOS
    //Segger RTT-t monitorozo taszk
    TaskHandle_t    taskHandler;
  #endif

} RTTconsole_t;
extern RTTconsole_t RTTconsole;
//------------------------------------------------------------------------------
//RTT-s konzol kezdeti inicializalasa
void RTTconsole_init(void);

//Baremetal eseten a fociklusbol hivogatott fuggveny.
bool RTTconsole_poll(RTTconsole_t* this);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //RTTCONSOLE_H_
