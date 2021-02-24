//------------------------------------------------------------------------------
//  Sajat UART kezelo driver. A MySercom driverre epul. 
//
//    File: MyUart.h
//------------------------------------------------------------------------------
#ifndef MY_UART_H_
#define MY_UART_H_

#include "MySercom.h"
#include "MyFIFO.h"

//Statusz kodok
enum
{
    kMyUartStatus_= MAKE_STATUS(kStatusGroup_MyUart, 0),
    //A driver nincs inicializalva, megis meg lett hivva valami fuggvenye
    kMyUartStatus_DriverIsNotInited,
    //A driver meg nem fejezett be egy korabban elinditott kuldest
    kMyUartStatus_TxBusy,
};

//------------------------------------------------------------------------------
//UART inicializacios struktura
typedef struct
{
    //Adatatviteli sebesseg.
    uint32_t    bitRate;
    //RS485 mod kijelolese
    bool        rs485Mode;
    //A periferia IRQ prioritasa
    uint32_t    irqPriorities;
} MyUart_Config_t;
//------------------------------------------------------------------------------
//Uj karakter vetelekor felojovo callback fuggveny prototipusa
typedef void MyUart_rxFunc_t(SercomUsart* hw, void* callbackData);

//UART hiba eseten feljovo callback fuggveny prototipusa
typedef void MyUart_errorFunc_t(SercomUsart* hw, void* callbackData);

//Olyan callback funkcio, mely akkor hivodik, ha egy atadott adathalmaz kiirasa
//megtortent az uarton.
typedef void MyUart_txDoneFunc_t(void* callbackData);
//------------------------------------------------------------------------------
//Az adatok kuldesehez tartozo valtozok halmaza
typedef struct
{
    //A kikuldendo adatokat cimzi a memoriaban
    const uint8_t*  dataPtr;
    //A meg kiirasra varo byteok szamat tartja nyilvan. A kuldes folyaman
    //csokken.
    uint32_t        leftByteCount;
    //Egy olyan callback rutin, melyet meghiv, ha az osszes byte kiirodott az
    //uartra
    MyUart_txDoneFunc_t*  doneFunc;
    //A callbacknak atadando adatokra mutat
    void*   doneFunc_callbackData;

    //1, ha egy blokk kuldesi folyamat aktiv
    uint8_t blockSending;

    //kimeneti fifo
    MyFIFO_t* txFifo;
} MyUart_Tx_t;
//------------------------------------------------------------------------------
//Az adatok vetelehez tartozo valtozok halmaza
typedef struct
{
    //Uj karakter vetelekor felojovo callback fuggveny (beregisztralhato)
    MyUart_rxFunc_t*     rxFunc;
} MyUart_Rx_t;
//------------------------------------------------------------------------------

//MyUart instancia valtozoi
typedef struct
{
    //True, ha sikeresen inicializalva van a driver
    bool    inited;

    //Az uarthoz tartozo sercom driver valtozoi
    MySercom_t sercom;

    //UART konfiguracio
    const MyUart_Config_t* config;

    //UART hiba eseten feljovo callback fuggveny (beregisztralhato)
    MyUart_errorFunc_t*  errorFunc;
    //A callback fuggvenyeknek atadott parameter (regisztarciokor beallitva)
    void* callbackData;

    //Az adatok kuldesehez tartozo valtozok halmaza
    MyUart_Tx_t  tx;

    //Vetelhez tartozo valtozok halmaza
    MyUart_Rx_t  rx;

    //Utoljara beallitott baud ertek.
    uint32_t bitRate;

} MyUart_t;
//------------------------------------------------------------------------------
//Driver instancia letrahozasa
//Az inicializacio alatt egy standard "8 bit, 1 stop bit, nincs paritas"
//aszinkron uart jon letre, mely a belso orajelrol mukodik. Ha ettol eltero
//mukodes szukseges, akkor azt az applikacioban az init utan lehet modositani,
//az uart engedelyezese elott.
//Figyelem! Az init hatasara nem kerul engedelyezesre az UART,
//          azt a MyUart_Enable() fuggvennyel kell elvegezni, a hasznalat elott.
void MyUart_create(MyUart_t* uart,
                   const MyUart_Config_t* cfg,
                   const MySercom_Config_t* sercomCfg);

//UART driver es eroforrasok felaszabditasa
void MyUart_destory(MyUart_t* uart);

//UART Periferia inicializalasa/engedelyezese
void MyUart_init(MyUart_t* uart);
//UART Periferia tiltasa. HW eroforrasok tiltasa.
void MyUart_deinit(MyUart_t* uart);

//Uart periferia resetelese
void MyUart_reset(MyUart_t* uart);
//Uart mukodes engedelyezese
void MyUart_enable(MyUart_t* uart);
//Uart mukodes tiltaasa
void MyUart_disable(MyUart_t* uart);

//Vetel atmeneti tiltasa.
void MyUart_disableRx(MyUart_t* uart);
//Vetel visszaengedelyezese
void MyUart_enableRx(MyUart_t* uart);

//UART-ra kozvetlen karakter kiirasa
void MyUart_putCharDirect(MyUart_t* uart, char txByte);

//Callback funkciok beregisztralasa
void MyUart_registreCallbacks(MyUart_t* uart,
                              MyUart_rxFunc_t* rxFunc,
                              MyUart_errorFunc_t* errorFunc,
                              void* callbackData);
//Kimeneti fifo beregisztralasa
void MyUart_registerTxFifo(MyUart_t* uart, MyFIFO_t* txFifo);


//Adatatviteli sebesseg beallitasa/modositasa
void MyUart_setBitRate(MyUart_t* uart, uint32_t bitRate);

//Adatcsomag kiirasanak kezdemenyezese. A rutin nem varja meg, amig az adatok
//kiirodnak az uartra.
//A rutinnak atadhato egy callback rutin, mely meghivodik, ha az osszes adatot
//kiirta az uartra. Figyelem: A beregisztralt callback rutin interrupt alol
//hivodik!
//uart: Az uarthoz tartozo valtozok
//data: A kikuldendo adatokra mutat
//lengt: A kikuldendo adatblokk hossza (byteban)
//doneFunc: megadhato egy callback funkcio, melyet ha vegez, akkor hiv
//doneCallbackData: A callback szamara atadott valtozo
status_t MyUart_sendNonBlocking(MyUart_t* uart,
                                const uint8_t* data,
                                uint32_t lengt,
                                MyUart_txDoneFunc_t* doneFunc,
                                void* doneCallbackData);

//Uartra adat kuldese a kimeneti fifo-n keresztul.
//FONTOS! a kimeneti fifot elotte regisztralni kell!!!
status_t MyUart_sendByte(MyUart_t* uart, uint8_t data);


//nem 0-at ad vissza, ha az uartrol lehet olvasni
//A veteli callbackban lehet hasznalni, es minaddig olvasni az uartot, mig ez
//nem 0-at ad visza.
static inline uint32_t MyUart_receivedDataAvailable(SercomUsart* hw)
{
    return (hw->INTFLAG.bit.RXC);
}

//Uart veteli adatregiszterenek olvasasa
//A veteli callbackban lehet hasznalni.
static inline uint8_t MyUart_readRxReg(SercomUsart* hw)
{
    return (hw->DATA.reg & 0xff);
}


//Uart driver kozos interrupt kiszolgalo rutinja. Az applikacioban elhelyezett
//IRQ handlerekbol kell meghivni.
void MyUart_service(MyUart_t* uart);

//A sercomok definicioit undefine-olnom kellet, mert kulonben az alabbi makroban
//a "SERCOM" osszeveszett a forditoval.
#undef SERCOM0
#undef SERCOM1
#undef SERCOM2
#undef SERCOM3
#undef SERCOM4
#undef SERCOM5
#undef SERCOM6
#undef SERCOM7
#undef SERCOM8
#define MYUART_PASTER( a, b )       a ## b
#define MYUART_EVALUATOR( a, b )    MYUART_PASTER(a, b)
#define MYUART_H1(k, ...)                   \
                           _##k##_Handler(void){MyUart_service(  __VA_ARGS__ );}
#define MYUART_H2(n)                        MYUART_EVALUATOR(SERCOM, n)
#define MYUART_SERCOM_HANDLER(n, k, ...)    \
            void MYUART_EVALUATOR(MYUART_H2(n), MYUART_H1(k, __VA_ARGS__))

//Seged makro, mellyel egyszeruen definialhato az I2C-hez hasznalando interrupt
//rutinok.
//Ez kerul a kodba peldaul:
//  void SERCOMn_0_Handler(void){ MyUart_service(  xxxx ); }
//  void SERCOMn_1_Handler(void){ MyUart_service(  xxxx ); }
//  void SERCOMn_2_Handler(void){ MyUart_service(  xxxx ); }
//  void SERCOMn_3_Handler(void){ MyUart_service(  xxxx ); }
//             ^n                                  ^...
//n: A sercom sorszama (0..7)
//xxxx: A MyUart_t* tipusu leiro
#define MYUART_HANDLERS( n, ...) \
        MYUART_SERCOM_HANDLER(n, 0, __VA_ARGS__) \
        MYUART_SERCOM_HANDLER(n, 1, __VA_ARGS__) \
        MYUART_SERCOM_HANDLER(n, 2, __VA_ARGS__) \
        MYUART_SERCOM_HANDLER(n, 3, __VA_ARGS__)

//------------------------------------------------------------------------------
//Seged makro, mellyel egyedi megszakitas handler definialhato a SERCOM-hoz.
//A letrejovo kodban, a a sercom osszes interrupt handlere a megadott
//service rutint fogja hivni.
//SERCOMn
#define MYUART_CUSTOM_HANDLER_H1(n)    MY_EVALUATOR(SERCOM, n)
//_k_Handler(void){MstrComm_uart_handler();}
#define MYUART_CUSTOM_HANDLER_H2(k, ...)    _##k##_Handler(void){__VA_ARGS__();}
#define  MYUART_CUSTOM_HANDLER(n, k, ...) \
    void MY_EVALUATOR(MYUART_CUSTOM_HANDLER_H1(n), MYUART_CUSTOM_HANDLER_H2(k,  __VA_ARGS__))
#define MYUART_CUSTOM_HANDLERS(n, ...)   \
        MYUART_CUSTOM_HANDLER(n, 0, __VA_ARGS__) \
        MYUART_CUSTOM_HANDLER(n, 1, __VA_ARGS__) \
        MYUART_CUSTOM_HANDLER(n, 2, __VA_ARGS__) \
        MYUART_CUSTOM_HANDLER(n, 3, __VA_ARGS__)
//------------------------------------------------------------------------------
#endif //MY_UART_H_
