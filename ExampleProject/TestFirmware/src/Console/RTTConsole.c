//------------------------------------------------------------------------------
//  Segger RTT-n keresztul kommunikalo konzol modul
//
//    File: RTTConsole.c
//------------------------------------------------------------------------------
#include "RTTConsole.h"
#include <string.h>
#include <stdio.h>
#include "resources.h"
#include "SEGGER_RTT.h"

#ifdef USE_FREERTOS
  #include "Tasks_config.h"
#endif

//Az RTT-taszk kozotti buffer/fifo merete
#define RTT_CONSOLE_RX_FIFO_SIZE   64

//Cmd.c-ben talalhato parancs tablazat
extern const MyCmdLine_CmdTable_t g_Cmd_cmdTable[];

//RTTConsole sajat valtozoi
RTTconsole_t RTTconsole;

static void RTTconsole_send_cb( const uint8_t* Data,
                                uint32_t Length,
                                void* UserData);
#ifdef USE_FREERTOS
static void RTTconsole_poll_task(void* TaskParam);
#endif

//A bemeneti stream fifozasahoz buffer allokacio
static uint8_t RTTConsole_RxFifoBuffer[RTT_CONSOLE_RX_FIFO_SIZE];
//------------------------------------------------------------------------------
//RTT konzol kezdeti inicializalasa
void RTTconsole_init(void)
{
    RTTconsole_t* this=&RTTconsole;
    //Modul valtozoinak kezdeti torlese.
    memset(this, 0, sizeof(RTTconsole_t));

    //Konzol modul konfiguralasa es initje...
    static const MyConsole_Config_t consoleCfg=
    {
        .name                   ="RTTConsole",
        .peripheriaInitFunc     =NULL,
        .peripheriaDeinitFunc   =NULL,
        .sendFunc               =RTTconsole_send_cb,
        .callbackData           =(void*) &RTTconsole,

        .rxFifoCfg.bufferSize   =RTT_CONSOLE_RX_FIFO_SIZE,
        .rxFifoCfg.buffer       =RTTConsole_RxFifoBuffer,

        .lineBuffer             =RTTconsole.lineBuffer,
        .lineBufferSize         =sizeof(RTTconsole.lineBuffer),
       #ifdef USE_FREERTOS
        .taskStackSize          =RTT_CONSOLE_TASK_STACKS_SIZE,
        .taskPriority           =RTT_CONSOLE_TASK_PRIORITY,
       #endif
        .cmdTable      =(const struct MyCmdLine_CmdTable_t*) g_Cmd_cmdTable,
    };
    MyConsole_init(&this->console, &consoleCfg);

  #ifdef USE_FREERTOS
    //Segger RTT-t monitorozo taszk letrehozasa
    if (xTaskCreate(RTTconsole_poll_task,
                    "RTT poll",
                    RTT_POLL_TASK_STACKS_SIZE,
                    this,
                    RTT_POLL_TASK_PRIORITY,
                    &this->taskHandler)!=pdPASS)
    {
        ASSERT(0);
    }
  #endif
}
//------------------------------------------------------------------------------
//A periferiara torteno adatkuldest megvalosito callback funkcio.
//A fuggevny addig nem terhet vissza, amig a periferian ki nem mentek az adatok.
static void RTTconsole_send_cb( const uint8_t* data,
                                uint32_t length,
                                void* userData)
{
    (void) userData;

    for(;length;length--)
    {
        char ch=*data++;
        if (ch=='\n')

        {
            SEGGER_RTT_PutCharSkip(0, '\r');
        }

        SEGGER_RTT_PutCharSkip(0, ch);
    }
}
//------------------------------------------------------------------------------
#ifdef USE_FREERTOS
static void __attribute__((noreturn)) RTTconsole_poll_task(void* taskParam)
{
    RTTconsole_t* this=(RTTconsole_t*) taskParam;

    while(1)
    {
        if (RTTconsole_poll(this)==false)
        {
            //Ha nincs adat, akkor kis varakozas, mielott ujra ranezne
            //TODO: mivel ez gyakran felebreszti az MCU-t ticklessIDLE-bol,
            //      lehet, hogy valami mas megoldast kellene kidolgozni!
            vTaskDelay(100);
            continue;
        }
    }
}
#endif
//------------------------------------------------------------------------------
//Baremetal eseten a fociklusbol hivogatott fuggveny.
//true-val ter vissza, ha volt a konzolon adat.
bool RTTconsole_poll(RTTconsole_t* this)
{
    #ifndef USE_FREERTOS
      MyConsole_poll(&this->console);
    #endif


    unsigned Len=sizeof(this->buffer);
    volatile unsigned readLen=SEGGER_RTT_Read(0, this->buffer, Len);
    if (readLen==0)
    {
        //Nincs adat
        return false;
    }

    //Van adat az RTT konzolon, amit a bufferben talalunk..
    uint8_t* Ptr=(uint8_t*)this->buffer;
    for(;readLen; readLen--)
    {
        char ch=*Ptr++;
        //Az RTT konzolon a visszatorleskor 127 megy. Ha ezt kapjuk, akkor
        //azt transzformaljuk a CmdLine szamara ertelmes backspace-re.
        if (ch==127) ch=CHAR_BACKSPACE;

        MyConsole_feed(&this->console, ch);
    }


    return true;
}
//------------------------------------------------------------------------------
