//------------------------------------------------------------------------------
//  Sorosportos konzol modul
//
//    File: SerialConsole.h
//------------------------------------------------------------------------------
#ifndef SERIALCONSOLE_H_
#define SERIALCONSOLE_H_

#include "MyConsole.h"
#include "MyUart.h"

//A sorosportos parancsok max ilyen hosszuak lehetnek. Ekkora sorbuffert foglal
#define SERIAL_CONSOLE_CMD_LINE_LENGTH 128

//------------------------------------------------------------------------------
//SerialConsole valtozoi
typedef struct
{
    //Konzol kezelo alap modul
    MyConsole_t console;

    //A konzolhoz hasznalt uart handlere
    MyUart_t uart;

    //Parancssor buffer. Ebbe rakja ossze a parancsort.
    char lineBuffer[SERIAL_CONSOLE_CMD_LINE_LENGTH];

    //Ha ez true, akkor egy parancssoros kuldes van, melynek a vegere varni
    //kell, de meg nem fut a scheduler, igy flageken keresztul tudunk csak
    //atjelezni a varakozo rutinnak.
    bool sendInProgress;
  #ifdef USE_FREERTOS
    //Uartra kiiras veget jelzo szemafor
    SemaphoreHandle_t  sendDoneSemaphore;

    //Az egyidoben tobb taszk feloli meghivas kizarasa
    SemaphoreHandle_t   mutex;
  #endif

} SerialConsole_t;
extern SerialConsole_t serialConsole;
//------------------------------------------------------------------------------
//Sorosportos konzol kezdeti inicializalasa
void SerialConsole_init(void);

//Fociklusbol hivogatott rutin bare metal eseten
void SerialConsole_poll(void);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //SERIALCONSOLE_H_
