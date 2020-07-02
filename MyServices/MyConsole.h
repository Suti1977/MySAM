//------------------------------------------------------------------------------
//  Absztrakt konzol kezelo modul
//
//    File: MyConsole.h
//------------------------------------------------------------------------------
#ifndef MY_CONSOLE_H_
#define MY_CONSOLE_H_

#include "MyCommon.h"
#include "MyCmdLine.h"
#include "MyFIFO.h"

struct MyConsole_t;
//------------------------------------------------------------------------------
//Olyan callback, melyben a kommunikacios periferiat (pl uart) inicializalni kell
typedef
status_t MyConsoleinitPeripheriaFunc_t(struct MyConsole_t* console,
                                       void* userData);

//Alacsony fogyasztasu modba menet elotti periferia deinit callback
typedef
status_t MyConsoledeinitPeripheriaFunc_t(struct MyConsole_t* console,
                                         void* userData);

//A periferiara torteno adatkuldest megvalosito callback funkcio.
//A fuggevny addig nem terhet vissza, amig a periferian ki nem mentek az adatok.
typedef void MyConsole_sendFunc_t(const uint8_t* data,
                                  uint32_t length,
                                  void* userData);

typedef struct
{
    //Olyan callback, melyben a kommunikacios periferiat inicializalni kell
    MyConsoleinitPeripheriaFunc_t*   peripheriaInitFunc;
    //Alacsony fogyasztasu modba menet elotti periferia deinit callback
    MyConsoledeinitPeripheriaFunc_t* peripheriaDeinitFunc;
    //A periferiara torteno adatkuldest megvalosito callback funkcio.
    MyConsole_sendFunc_t* sendFunc;
    //A Callbackek szamara atadott tetszolesges valtozo (user_data)
    void* callbackData;

    //Bemeneti streambuffer/fifo merete
    MyFIFO_Config_t rxFifoCfg;

    //Parancssor buffere, es hossza. (konfiguraciokort megadva)
    char*       lineBuffer;
    uint32_t    lineBufferSize;

    //A parancsertelmezohoz tartozo parancs tablazatra mutat
    const struct MyCmdLine_CmdTable_t* cmdTable;

    //A konzolnak a neve. Ilyen neven jon letre hozza tasz, vagy ilyen neven
    //hivatkozunk ra a logban
    const char* name;

  #ifdef USE_FREERTOS
    //A konzolt futtato taszkhoz foglalando stack meret
    uint32_t    taskStackSize;
    //A konzol taszk prioritasa
    uint32_t    taskPriority;
    //Konzolt futtato taszk neve
  #endif

} MyConsole_Config_t;
//------------------------------------------------------------------------------
//MyConsole valtozoi
typedef struct
{
    //A konzolnak a neve. Ilyen neven jon letre hozza tasz, vagy ilyen neven
    //hivatkozunk ra a logban
    const char* name;

    //Olyan callback, melyben a kommunikacios periferiat inicializalni kell
    MyConsoleinitPeripheriaFunc_t*   peripheriaInitFunc;
    //Alacsony fogyasztasu modba menet elotti periferia deinit callback
    MyConsoledeinitPeripheriaFunc_t* peripheriaDeinitFunc;
    //A periferiara torteno adatkuldest megvalosito callback funkcio.
    //A fuggevny addig nem terhet vissza, amig a periferian ki nem mentek az
    //adatok!
    MyConsole_sendFunc_t* sendFunc;
    //A Callbackek szamara atadott tetszolesges valtozo
    void* callbackData;

    //Parancsertelmezo valtozoi
    MyCmdLine_t cmdLine;

    //Ezen a bufferen keresztul tortenik a sorosporton/RTT kapott karakterek
    //fifozasa a veteli megszakitas rutin es a veteli taszkok kozott.
    MyFIFO_t    rxFifo;

    //true, ha mar a start prompt ki lett irva
    bool startPromptPrinted;

  #ifdef USE_FREERTOS
    //A konzolt futtato taszk handlere
    TaskHandle_t    taskHandler;
  #endif //USE_FREERTOS
} MyConsole_t;
//------------------------------------------------------------------------------
//Konzol kezdeti inicializalasa
status_t MyConsole_init(MyConsole_t* console, const MyConsole_Config_t* cfg);

//A konzol etetese megszakitasbol.
void MyConsole_feedFromIsr(MyConsole_t* console, uint8_t rxByte);

//A konzol etetese taszkbol/normal futasbol
void MyConsole_feed(MyConsole_t* console, uint8_t rxByte);

//Konzolra karakter kiirsa
void MyConsole_putChar(MyConsole_t* console, const char txByte);

//Konzolra string kiirsa
void MyConsole_putString(MyConsole_t* console, const char* Str);

void MyConsole_poll(MyConsole_t* console);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_CONSOLE_H_
