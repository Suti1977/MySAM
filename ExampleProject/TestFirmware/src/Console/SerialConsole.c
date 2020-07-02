//------------------------------------------------------------------------------
//  Sorosportos konzol modul
//
//    File: SerialConsole.c
//------------------------------------------------------------------------------
#include "SerialConsole.h"
#include <string.h>
#include "resources.h"
#include <stdio.h>
#ifdef USE_FREERTOS
#include "Tasks_config.h"
#endif
#include "board.h"

//A sorosport es a veteli taszk kozotti buffer/fifo merete
#define SERIAL_CONSOLE_RX_FIFO_SIZE   64

//Cmd.c-ben talalhato parancs tablazat
extern const MyCmdLine_CmdTable_t g_Cmd_cmdTable[];

//modul sajat valtozoi
SerialConsole_t serialConsole;


static status_t SerialConsole_uartInit_cb(struct MyConsole_t* console,
                                          void* callbackData);
static void SerialConsole_send_cb( const uint8_t* data,
                                   uint32_t length,
                                   void* callbackData);
static void SerialConsole_uart_rx_cb(SercomUsart* hw, void* callbackdata);
static void SerialConsole_uartError_cb(SercomUsart* hw, void* callbackData);
static void SerialConsole_uart_txDone_cb(void* callbackData);

//A bemeneti stream fifozasahoz buffer allokacio
static uint8_t SerialConsole_RxFifoBuffer[SERIAL_CONSOLE_RX_FIFO_SIZE];
//------------------------------------------------------------------------------
//Sorosportos konzol kezdeti inicializalasa
void SerialConsole_init(void)
{
    SerialConsole_t* this=&serialConsole;
    //Modul valtozoinak kezdeti torlese.
    memset(this, 0, sizeof(SerialConsole_t));

  #ifdef USE_FREERTOS
    //Sorosportos kuldes veget jelzo szemafor letrehozasa
    this->sendDoneSemaphore=xSemaphoreCreateBinary();
    ASSERT(this->sendDoneSemaphore);

    //Valtozokat vedo mutex letrehozasa
    this->mutex=xSemaphoreCreateMutex();
    ASSERT(this->mutex);
  #endif

    //Konzol modul konfiguralasa es initje...
    static const MyConsole_Config_t consoleCfg=
    {
        .name                   ="SerialConsole",
        .peripheriaInitFunc     =SerialConsole_uartInit_cb,
        .peripheriaDeinitFunc   =NULL,
        .sendFunc               =SerialConsole_send_cb,
        .callbackData           =(void*) &serialConsole,

        .rxFifoCfg.bufferSize   =SERIAL_CONSOLE_RX_FIFO_SIZE,
        .rxFifoCfg.buffer       =SerialConsole_RxFifoBuffer,

        .lineBuffer             =serialConsole.lineBuffer,
        .lineBufferSize         =sizeof(serialConsole.lineBuffer),
      #ifdef USE_FREERTOS
        .taskStackSize          =SERIAL_CONSOLE_TASK_STACKS_SIZE,
        .taskPriority           =SERIAL_CONSOLE_TASK_PRIORITY,
      #endif
        .cmdTable        =(const struct MyCmdLine_CmdTable_t*) g_Cmd_cmdTable,
    };
    MyConsole_init(&this->console, &consoleCfg);
}
//------------------------------------------------------------------------------
//Fociklusbol hivogatott rutin bare metal eseten
void SerialConsole_poll(void)
{
    //KOnzol futtatasa...
    MyConsole_poll(&serialConsole.console);
}
//------------------------------------------------------------------------------
//Sorosport periferia inicializalasa
//Callback funkcio, melyet a MyConsole modul hiv meg.
static status_t SerialConsole_uartInit_cb(struct MyConsole_t* console,
                                          void* callbackData)
{
    (void) console;
    SerialConsole_t* this=(SerialConsole_t*) callbackData;

    //Uart konfiguralasa...
    //A hozza tartozo GPIO vonalak az ASF-es wizardal kerultek definialasra

    MyUart_Config_t cfg;
    MyUart_getDefaultConfig(&cfg);
    cfg.sercomConfig.sercomNr=CONSOLE_SERCOM_NUM;
    cfg.sercomConfig.gclkCore=CONSOLE_SERCOM_CORE_GCLK;
    cfg.sercomConfig.gclkSlow=CONSOLE_SERCOM_SLOW_GCLK;
    cfg.sercomConfig.irqPriorites=IRQ_PRIORITY__CONSOLE_UART;
    cfg.baudRegValue=
            MYUART_CALC_BAUDVALUE(CONSOLE_BAUDRATE, CONSOLE_SERCOM_CORE_FREQ);

    MyUart_init(&this->uart, &cfg);

    //Az alapertelmezestol elter az RX vonal PAD szama. Azt be kell allitani.
    this->uart.sercom.hw->USART.CTRLA.bit.RXPO=CONSOLE_SERCOM_RX_PAD;

    //RX, TX GPIO vonalaj beallitasa
    MyGPIO_pinConfig(PIN_CONSOLE_SERCOM_RX_CONFIG);
    MyGPIO_pinConfig(PIN_CONSOLE_SERCOM_TX_CONFIG);

    //Callback funkciok beregisztralasa
    MyUart_registreCallbacks(&this->uart,
                             SerialConsole_uart_rx_cb,
                             SerialConsole_uartError_cb,
                             this);
    //Uart engedelyezese
    MyUart_enable(&this->uart);

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Akkor jon fel, ha az uarton egy adathalmaz kuldesevel elkeszult
//[MEGSZAKITASBAN FUT!]
static void SerialConsole_uart_txDone_cb(void* callbackData)
{
    SerialConsole_t* this=(SerialConsole_t*) callbackData;

    if (this->sendInProgress)
    {   //Nem fut a scheduler, ezert egy kerulo uton kell jelezni a varakozo
        //oldalnak, hogy kesz a tx. Avval, hogy torlom ezt a flaget, ezt meg
        //is oldja. (nincsenek szinkronizacios objektumok.)
        this->sendInProgress=false;
    } else
    {   //A scheduler fut. Szemaforon keresztul jelzunk a varakozo taszknak,
        //hogy kesz a kuldes. A taszk felebred, es fut tovabb...
       #ifdef USE_FREERTOS
        //True-lesz, ha a kilepes elott kontextust kell valtani,
        //mivel egymagasabb prioritasu taszk jelzett.
        BaseType_t xHigherPriorityTaskWoken=pdFALSE;
        xSemaphoreGiveFromISR(this->sendDoneSemaphore,
                              &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
      #endif
    }
}
//------------------------------------------------------------------------------
//A periferiara torteno adatkuldest megvalosito callback funkcio.
//A fuggevny addig nem terhet vissza, amig a periferian ki nem mentek az adatok.
static void SerialConsole_send_cb( const uint8_t* data,
                                uint32_t length,
                                void* userData)
{
    SerialConsole_t* this=(SerialConsole_t*) userData;
  #ifdef USE_FREERTOS
    if (xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED)
  #endif
    {   //A scheduler le van allitva.  Nem lehet hasznalni szinkronizacios
        //objektumokat. :-(
        //Vagy Bare meteal modban vagyunk.

        //Ez a flag segit jelezni majd a callback-bol, ha vegzett a kuldessel.
        this->sendInProgress=true;

        MyUart_sendNonBlocking(  &this->uart,
                                    data,
                                    length,
                                    SerialConsole_uart_txDone_cb,
                                    this);

        //Varakozas, hogy az IT-ben vegezzen
        while(this->sendInProgress!=0)
        {
            __NOP();
        }

    }
   #ifdef USE_FREERTOS
    else
    {   //A scheduler fut. A szinkronizacios objektumok hasznalhatok.
        xSemaphoreTake(this->mutex, portMAX_DELAY);

        MyUart_sendNonBlocking(&this->uart,
                               data,
                               length,
                               SerialConsole_uart_txDone_cb,
                               this);

        //varakozas arra, hogy a tartalom kimenjen az uarton
        xSemaphoreTake(this->sendDoneSemaphore, portMAX_DELAY);

        xSemaphoreGive(this->mutex);
    }
   #endif
}
//------------------------------------------------------------------------------
//console uart-on olvashatosag eseten meghivodo callback.
//A rutinon keresztul kerul toltesre a bemeneti stream buffer, melyen keresztul
//csorog le az adat a taszkok fele.
//[INTERRUPT ALATT FUT!]
static void SerialConsole_uart_rx_cb(SercomUsart* hw, void* callbackdata)
{
    SerialConsole_t* this=(SerialConsole_t*) callbackdata;

    //ciklsus, amig az uartrol lehet olvasni...
    while(MyUart_receivedDataAvailable(hw))
    {
        //Adat olvasasa az uart veteli fifojabol, majd atadas a modulhoz tartozo
        //konzol kezelonek
        uint8_t rx_byte=MyUart_readRxReg(hw);
        MyConsole_feedFromIsr(&this->console, rx_byte);
    }
}
//------------------------------------------------------------------------------
//UART hiba eseten feljovo callback fuggveny prototipusa
//[MEGSZAKIATSBAN FUT!]
static void SerialConsole_uartError_cb(SercomUsart* hw, void* callbackdata)
{
    (void) hw;
    (void) callbackdata;
    printf("__UART ERROR__\n");
}
//------------------------------------------------------------------------------
//Uarthoz tartozo IRQ handlerek letrehozasa
MYUART_HANDLERS(CONSOLE_SERCOM_NUM, &serialConsole.uart)
//------------------------------------------------------------------------------

