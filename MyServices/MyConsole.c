//------------------------------------------------------------------------------
//  Absztrakt konzol kezelo modul
//
//    File: MyConsole.c
//------------------------------------------------------------------------------
#include "MyConsole.h"
#include <string.h>
#include <stdio.h>

#ifdef USE_FREERTOS
//Konzolon kuldes befejezeset jelzi (Freertos eseten)
static void MyConsole_task(void* TaskParam);
#endif

#define DEFAULT_CONSOLE_PROMPT_STRING    "[]>"
static void MyConsole_cmdLine_putChar_cb(char c, void* userData);
static void MyConsole_cmdLine_putString_cb(const char* str, void* userData);
static void MyConsole_startPropmpt(MyConsole_t* console);
//------------------------------------------------------------------------------
//Konzol kezdeti inicializalasa
status_t MyConsole_init(MyConsole_t* console, const MyConsole_Config_t* cfg)
{
    status_t status=kStatus_Success;

    ASSERT(cfg->sendFunc);

    //Modul valtozoinak kezdeti torlese.
    memset(console, 0, sizeof(MyConsole_t));

    //Config atvetele...
    console->name=cfg->name;
    console->peripheriaInitFunc=cfg->peripheriaInitFunc;
    console->peripheriaDeinitFunc=cfg->peripheriaDeinitFunc;
    console->sendFunc=cfg->sendFunc;
    console->callbackData=cfg->callbackData;

    //Parancsertelmezo initje...
    MyCmdLine_Config_t cmdLineCfg;
    cmdLineCfg.lineBuffer = cfg->lineBuffer;
    cmdLineCfg.lineBufferSize=cfg->lineBufferSize;
    cmdLineCfg.putCharFunc  =MyConsole_cmdLine_putChar_cb;
    cmdLineCfg.putStringFunc=MyConsole_cmdLine_putString_cb;
    cmdLineCfg.promptString=DEFAULT_CONSOLE_PROMPT_STRING;
    cmdLineCfg.cmdTable=cfg->cmdTable;
    cmdLineCfg.userData=console;
    MyCmdLine_init(&console->cmdLine, &cmdLineCfg);


    //A konzolhoz kapcsolt kommunikacios periferia inicializalasa a beallitott
    //callback funkcion keresztul
    if (console->peripheriaInitFunc)
    {
        status=console->peripheriaInitFunc((struct MyConsole_t*) console,
                                             console->callbackData);
        if (status) goto error;
    }

    //A veteli fifo letrehozasa. (Ebbe tortenik a bemenetrol erkezo
    //stream bufferelese.)
    MyFIFO_init(&console->rxFifo, &cfg->rxFifoCfg);

  #ifdef USE_FREERTOS
    //Konzolt futtato taszk letrehozasa
    if (xTaskCreate(MyConsole_task,
                    cfg->name,
                    (configSTACK_DEPTH_TYPE)cfg->taskStackSize,
                    console,
                    cfg->taskPriority,
                    &console->taskHandler)!=pdPASS)
    {
        ASSERT(0);
    }
  #endif

error:
    return status;
}
//------------------------------------------------------------------------------
//Konzol kezdo prompt szoveg kiirasa
static void MyConsole_startPropmpt(MyConsole_t* console)
{    
    //Kezdo Promptot kiirja a konzolra
    MyCmdLine_putString(&console->cmdLine, "\n\"");
    MyCmdLine_putString(&console->cmdLine, console->name);
    MyCmdLine_putString(&console->cmdLine, "\" console is running.\n");
    MyCmdLine_putPrompt(&console->cmdLine);
}
//------------------------------------------------------------------------------
//A taszkbol, vagy bare metal eseten a fociklusbol hivogatott rutin, mely a
//bemeno karaktereket dolgozza fe.
void MyConsole_poll(MyConsole_t* console)
{
    uint8_t rxChar;

    //Kezdo Promptot kiirja a konzolra, ha meg nem volt
    if (console->startPromptPrinted==false)
    {
        console->startPromptPrinted=true;
        MyConsole_startPropmpt(console);
    }

    //Bemeno fifobol karakter olvasasa (ha van)
    while(MyFIFO_getByte(&console->rxFifo, &rxChar)==kStatus_Success)
    {   //van karakter
        MyCmdLine_feeding(&console->cmdLine, rxChar);
    }
}
//------------------------------------------------------------------------------
#ifdef USE_FREERTOS
static void __attribute__((noreturn)) MyConsole_task(void* task_param)
{
    MyConsole_t* console=(MyConsole_t*)task_param;

    while(1)
    {
        //Varakozas ujabb karakterre, mellyel a parancsertelmezo etetheto.
        //A vegtelen varakozasbol a leallasi kerelem eseten egy TaskNotify
        //esemeny ugrathatja ki.
        uint32_t notifyEvents;
        xTaskNotifyWait(0, 0xffffffff, &notifyEvents, portMAX_DELAY);
        MyConsole_poll(console);

    } //while
}
#endif
//------------------------------------------------------------------------------
//A konzol etetese megszakitasbol.
//[MEGSZAKITASBOL HIVVA!]
void MyConsole_feedFromIsr(MyConsole_t* console, uint8_t rxByte)
{
    MyFIFO_putBytesFromIsr(&console->rxFifo, rxByte);

  #ifdef USE_FREERTOS
    //True-lesz, ha a kilepes elott kontextust kell valtani, mivel egy
    //magasabb prioritasu taszk jelzett.
    BaseType_t xHigherPriorityTaskWoken=pdFALSE;

    //Jelezes a taszknak, hogy ebredjen fel, es nezzen ra a FIFO-ra
    vTaskNotifyGiveFromISR(console->taskHandler, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  #endif
}
//------------------------------------------------------------------------------
//A konzol etetese taszkbol
void MyConsole_feed(MyConsole_t* console, uint8_t rxByte)
{
    MyFIFO_putByte(&console->rxFifo, rxByte);

  #ifdef USE_FREERTOS
    //Jelezes a konzol taszknak, hogy van ujabb karakter. Nezzen ra a fifora.
    xTaskNotify(console->taskHandler, 0, eIncrement);
  #endif
}
//------------------------------------------------------------------------------

//karakter kiirasat megvalosito callback funkcio. A parancssori modul hivja.
static void MyConsole_cmdLine_putChar_cb(char c, void* userData)
{
    MyConsole_t* console=(MyConsole_t*)userData;
    //A konfiguraciokor a konzolhoz beregisztralt kiiro rutint hivjuk
    console->sendFunc((const uint8_t*)&c, 1, console->callbackData);
}
//------------------------------------------------------------------------------
//0 vegu string kiirasat megvalosito callback funkcio.A parancssori modul hivja.
static void MyConsole_cmdLine_putString_cb(const char* str, void* userData)
{
    MyConsole_t* console=(MyConsole_t*)userData;
    //A konfiguraciokor a konzolhoz beregisztralt kiiro rutint hivjuk
    console->sendFunc((const uint8_t*)str,
                       strlen(str),
                       console->callbackData);
}
//------------------------------------------------------------------------------
//Konzolra karakter kiirsa
void MyConsole_putChar(MyConsole_t* console, const char c)
{
    //A konfiguraciokor a konzolhoz beregisztralt kiiro rutint hivjuk
    console->sendFunc((const uint8_t*)&c, 1, console->callbackData);
}
//------------------------------------------------------------------------------
//Konzolra string kiirsa
void MyConsole_putString(MyConsole_t* console, const char* str)
{
    //A konfiguraciokor a konzolhoz beregisztralt kiiro rutint hivjuk
    console->sendFunc((const uint8_t*)str,
                       strlen(str),
                       console->callbackData);
}
//------------------------------------------------------------------------------
