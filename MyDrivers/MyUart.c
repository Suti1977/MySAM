//------------------------------------------------------------------------------
//  Sajat UART kezelo driver. A MySercom driverre epul. 
//
//    File: MyUart.c
//------------------------------------------------------------------------------
#include "MyUart.h"
#include <string.h>
#include "MyAtomic.h"

static status_t MyUart_initSercom(MyUart_t* uart, const MyUart_Config_t* cfg);

//------------------------------------------------------------------------------
//Driver instancia inicializalasa
//Az inicializacio alatt egy standard "8 bit, 1 stop bit, nincs paritas"
//aszinkron uart jon letre, mely a belso orajelrol mukodik. Ha ettol eltero
//mukodes szukseges, akkor azt az applikacioban az init utan lehet modositani,
//az uart engedelyezese elott.
//Figyelem! Az init hatasara nem kerul engedelyezesre az UART,
//          azt a MyUart_Enable() fuggvennyel kell elvegezni, a hasznalat elott.
status_t MyUart_init(MyUart_t* uart, const MyUart_Config_t* cfg)
{
    status_t status=kStatus_Success;

    //Modul valtozoinak kezdeti torlese.
    memset(uart, 0, sizeof(MyUart_t));

    //Alap sercom periferia inicializalasa
    MySercom_init(&uart->sercom, &cfg->sercomConfig);

    //UART-nak konfiguralja a sercomot, tovabba beallitja azt a config szerint.
    MyUart_initSercom(uart, cfg);

    //Sercom megszakitasok engedelyezese
    MySercom_enableIrqs(&uart->sercom);

    //Jelzes, hogy a driver initje megtortent.
    uart->inited=1;

    return status;
}
//------------------------------------------------------------------------------
//Alapertelmezett uart konfiguracios struktura visszaadasa
void MyUart_getDefaultConfig(MyUart_Config_t* cfg)
{
    memset(cfg, 0, sizeof(MyUart_Config_t));
}
//------------------------------------------------------------------------------
static void MyUart_waitingForSync(SercomUsart* hw)
{
    while(hw->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//UART-nak konfiguralja a sercomot, tovabba beallitja azt a config szerint...
static status_t MyUart_initSercom(MyUart_t* uart, const MyUart_Config_t* cfg)
{
    status_t status=kStatus_Success;
    SercomUsart* hw=&uart->sercom.hw->USART;

    //USART Periferia reset
    hw->CTRLA.reg = SERCOM_USART_CTRLA_SWRST;
    //varakozas, amig a reset befejedodik
    MyUart_waitingForSync(hw);

    //Adatatviteli sebesseg beallitasa. Ezt initkor kellet kiszamolni, a
    //MyUART_CALC_BAUDVALUE() makroval
    hw->BAUD.reg= cfg->baudRegValue;

    hw->CTRLA.reg =
                //Usart uzemmod kivalasztasa, belso orajelrol, aszinkron
                SERCOM_USART_CTRLA_MODE(0x01) |
                //Mintaveteli rata beallitasa (0--> 16x, aritmetikus mod)
                //Ez befolyasolja a BAUD generatort is! (Adatlap!)
                //Ha nem ez kell, akkor a MyUART_CALC_BAUDVALUE() makro nem
                //hasznalhato!
                SERCOM_USART_CTRLA_SAMPR(0) |
                //Kimenetek pinoutjanak beallitasa. PAD[0] --> TXD
                //RS485 eseten TXPO-t 3-ra kell beallitani
                (cfg->rs485Mode ? SERCOM_USART_CTRLA_TXPO(3) :
                                 SERCOM_USART_CTRLA_TXPO(0)) |
                //Adat bemenet (RXD) kijelolese  PAD[1] <-- RXD
                SERCOM_USART_CTRLA_RXPO(1) |
                //Mintavetelezesi pontok beallitasa.
                //SERCOM_USART_CTRLA_SAMPA(0) |
                //Formatum beallitasa. Egyszeru frame. Nincs paritas.
                //Ha kellene paritas,akkor '1' formatumot kellene itt kijelolni.
                //SERCOM_USART_CTRLA_FORM(0) |
                //Adat irany: ha nincs benne akkor MSB megy elol.
                SERCOM_USART_CTRLA_DORD |
                //Ha ez benne van, akkor overflow eseten azonnal jelez.
                //ha nincs, akkor ez is egyutt mozog a vett adatokkal
                //(tehat a fifon keresztul jon a jelzes.)
                //SERCOM_USART_CTRLA_IBON |
                0;

    hw->CTRLB.reg=
            //Soros vetel engedelyezese
            SERCOM_USART_CTRLB_RXEN |
            //ado oldal engedelyezese
            SERCOM_USART_CTRLB_TXEN |
            //Paritas beallitasa (ha hasznalnank)
            //SERCOM_USART_CTRLB_PMODE |
            //Stopbitek szamanak beallitasa. Ha benne van, akkor 2 stopbit van
            //SERCOM_USART_CTRLB_SBMODE |
            //Adatbitek szamanak beallitasa. 8 bites frameket hasznalunk
            SERCOM_USART_CTRLB_CHSIZE(0);
    MyUart_waitingForSync(hw);

    //Megszakitasok tiltasa
    hw->INTENCLR.reg=0xff;

    //Veteli megszakitasok engedelyezese. (Az ado majd csak ha van mit kudleni)
    hw->INTENSET.reg=
            //Veteli msz. (ujabb adatbyte erkezett)
            SERCOM_USART_INTENSET_RXC |
            0;

    return status;
}
//------------------------------------------------------------------------------
//Uart mukodes engedelyezese
void MyUart_enable(MyUart_t* uart)
{
    //Sercomhoz tartozo IRQ vonalak engedelyezese az NVIC-ben
    //MySercom_EnableIRQs(&uart->Sercom);

    MY_ENTER_CRITICAL();
    uart->sercom.hw->USART.CTRLA.bit.ENABLE=1;
    MyUart_waitingForSync(&uart->sercom.hw->USART);
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Uart mukodes tiltaasa
void MyUart_disable(MyUart_t* uart)
{
    MY_ENTER_CRITICAL();
    uart->sercom.hw->USART.CTRLA.bit.ENABLE=0;
    MyUart_waitingForSync(&uart->sercom.hw->USART);
    MY_LEAVE_CRITICAL();

    //Sercomhoz tartozo IRQ vonalak tiltasa az NVIC-ben
    MySercom_disableIrqs(&uart->sercom);
}
//------------------------------------------------------------------------------
//Callback funkciok beregisztralasa
void MyUart_registreCallbacks(MyUart_t* uart,
                              MyUart_rxFunc_t* rxFunc,
                              MyUart_errorFunc_t* errorFunc,
                              void* callbackData)
{
    MY_ENTER_CRITICAL();
    uart->rx.rxFunc=rxFunc;
    uart->errorFunc=errorFunc;
    uart->callbackData=callbackData;
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//UART-ra kozvetlen karakter kiirasa
void MyUart_putCharDirect(MyUart_t* uart, char txByte)
{
    SercomUsart *hw=&uart->sercom.hw->USART;
    //Varakozunk, amig az uart kepes uj adatot fogadni a kimeneti regiszterebe
    while(hw->INTFLAG.bit.DRE==0);
    //afat kiirasa az uartra
    hw->DATA.reg=txByte;
}
//------------------------------------------------------------------------------
//Adatcsomag kiirasanak kezdemenyezese. A rutin nem varja meg, amig az adatok
//kiirodnak az uartra.
//A rutinnak atadhato egy callback rutin, mely meghivodik, ha az osszes adatot
//kiirta az uartra. Figyelem: A beregisztralt callback rutin interrupt alol
//hivodik!
//uart: Az uarthoz tartozo valtozok
//data: A kikuldendo adatokra mutat
//lengt: A kikuldendo adatblokk hossza (byteban)
//done_func: megadhato egy callback funkcio, melyet ha vegez, akkor hiv
//done_callback_data: A callback szamara atadott valtozo
status_t MyUart_sendNonBlocking(MyUart_t* uart,
                                const uint8_t* data,
                                uint32_t lengt,
                                MyUart_txDoneFunc_t* doneFunc,
                                void* doneCallbackData)
{
    //Ha ugy hivnank a rutint, hogy az uart nincs inicializalva, akkor hiba!
    if (uart->inited==false) return kMyUartStatus_DriverIsNotInited;

    SercomUsart* hw=&uart->sercom.hw->USART;

    //IT tiltasa a vizsgalatok idejere
    hw->INTENCLR.reg=SERCOM_USART_INTENCLR_DRE;

    //Ha ugy hivnank a rutint, hogy az uarton meg fut egy korabbi csomag kuldese
    if (uart->tx.sending)
    {
        //IT visszaengedelyezese.
        hw->INTENSET.reg=SERCOM_USART_INTENSET_DRE;
        __asm("dmb");
        __asm("isb");

        return kMyUartStatus_TxBusy;
    }

    //A kikuldendo adattartalomra allas...
    uart->tx.dataPtr=data;
    uart->tx.leftByteCount=lengt;
    uart->tx.doneFunc=doneFunc;
    uart->tx.doneFunc_callbackData=doneCallbackData;
    //Jelzes, hogy fut a kuldes
    uart->tx.sending=1;

    //IT engedelyezese. A kuldes az IT-ben fog elkezdodni...
    hw->INTENSET.reg=SERCOM_USART_INTENSET_DRE;

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Uart driver kozos interrupt kiszolgalo rutinja. Az applikacioban elhelyezett
//IRQ handlerekbol kell meghivni.
void MyUart_service(MyUart_t* uart)
{
    SercomUsart* hw=&uart->sercom.hw->USART;

    //(Megj: Az INTENSET regiszter visszaolvasva mutatja, hogy mely IT-k
    //       vannak beengedelyezve.

    //Ez a valtozo jelzi, hogy volt interrupt kezeles, ezert a kilepes elott
    //megegszer le kell futtatni a kiertekelest.
    //Gyakorlatilag addig marad igy az interrupt rutinban, amig vannnak
    //feldolgozhato jelzesek. Igy minimalizaljuk az IT-be/ki lepes tobblet
    //muveleteit
    uint8_t wasIT;
    do
    {
        wasIT=0;
        if ((hw->INTFLAG.bit.RXC) && (hw->INTENSET.bit.RXC))
        {   //uj adatbyte van az uarton

            //Ha van beregiszralva hozza callback funkcio, akkor meghivja azt
            if (uart->rx.rxFunc)
            {
                //Az uarton levo adatbyteokat a veteli callbackban kell olvasni.
                //Ezt segitik a MyUart_receivedDataAvailable() es
                //MyUart_readRxReg() fuggvenyek.
                uart->rx.rxFunc(hw, uart->callbackData);
            } else
            {   //Ha nincs megadva veteli callback, ami az adatokat olvasna,
                //akkor dummy olvasast kell vegrehajtani
                volatile uint8_t dummy=hw->DATA.reg & 0xff;
                (void) dummy;
            }

            wasIT=1;

            //Amig van a bemeneten adat, addig nem megy tovabb a Tx-re.
            //igy minimalizaljuk az esetleges adatvesztest.
            continue;
        }


        if ((hw->INTFLAG.bit.DRE) && (hw->INTENSET.bit.DRE))
        {   //Lehet ujabb adatbyteot helyezni az uartra.

            if (uart->tx.leftByteCount)
            {   //Van meg adat a bufferben. Uartra helyezzuk...
                hw->DATA.reg=(uint8_t) *uart->tx.dataPtr++;
                //Hatralevo elemszam csokkentese
                uart->tx.leftByteCount--;
            } else
            {   //Minden adatbyte el lett kuldve.
                //Tovabbi Tx interruptok tiltasa
                hw->INTENCLR.reg = SERCOM_USART_INTENCLR_DRE;
                __asm("dmb");
                __asm("isb");

                uart->tx.sending=0;

                //Ha van bereisztralva callback a Tx vegere, akkor meghivjuk...
                if (uart->tx.doneFunc)
                {
                    uart->tx.doneFunc(uart->tx.doneFunc_callbackData);
                }
            }

            wasIT=1;
        }

    }while (wasIT);
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
