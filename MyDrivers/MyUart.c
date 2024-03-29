//------------------------------------------------------------------------------
//  Sajat UART kezelo driver. A MySercom driverre epul. 
//
//    File: MyUart.c
//------------------------------------------------------------------------------
#include "MyUart.h"
#include <string.h>
#include "MyAtomic.h"

static void MyUart_initPeri(MyUart_t* uart);
static void MyUart_deinitPeri(MyUart_t* uart);

//------------------------------------------------------------------------------
//Driver instancia inicializalasa
//Az inicializacio alatt egy standard "8 bit, 1 stop bit, nincs paritas"
//aszinkron uart jon letre, mely a belso orajelrol mukodik. Ha ettol eltero
//mukodes szukseges, akkor azt az applikacioban az init utan lehet modositani,
//az uart engedelyezese elott.
//Figyelem! Az init hatasara nem kerul engedelyezesre az UART,
//          azt a MyUart_Enable() fuggvennyel kell elvegezni, a hasznalat elott.
void MyUart_create(MyUart_t* uart,
                   const MyUart_Config_t* cfg,
                   const MySercom_Config_t* sercomCfg)
{
    //Modul valtozoinak kezdeti torlese.
    memset(uart, 0, sizeof(MyUart_t));

    //Konfiguracio mentese
    uart->config=cfg;    

    //Alap sercom periferia inicializalasa
    MySercom_create(&uart->sercom, sercomCfg);

    //UART-hoz tartozo megszakitasok prioritasana beallitasa
    MySercom_setIrqPriorites(&uart->sercom, cfg->irqPriorities);


    //Sercom megszakitasok engedelyezese
    MySercom_enableIrqs(&uart->sercom);
}
//------------------------------------------------------------------------------
//UART driver es eroforrasok felaszabditasa
void MyUart_destory(MyUart_t* uart)
{
    (void) uart;
}
//------------------------------------------------------------------------------
//UART Periferia inicializalasa/engedelyezese
void MyUart_init(MyUart_t* uart)
{
    //UART-nak konfiguralja a sercomot, tovabba beallitja azt a config szerint.
    MyUart_initPeri(uart);
    uart->inited=true;
}
//------------------------------------------------------------------------------
//UART Periferia tiltasa. HW eroforrasok tiltasa.
void MyUart_deinit(MyUart_t* uart)
{
    uart->inited=false;
    MyUart_deinitPeri(uart);
}
//------------------------------------------------------------------------------
static void MyUart_waitingForSync(SercomUsart* hw)
{
    while(hw->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//UART-nak konfiguralja a sercomot, tovabba beallitja azt a config szerint...
static void MyUart_initPeri(MyUart_t* uart)
{
    SercomUsart* hw=&uart->sercom.hw->USART;
    const MyUart_Config_t* cfg=uart->config;

    //Periferia orajelek engedelyezese
    MySercom_enableMclkClock(&uart->sercom);
    MySercom_enableCoreClock(&uart->sercom);
    MySercom_enableSlowClock(&uart->sercom);

    //USART Periferia reset
    //hw->CTRLA.reg = SERCOM_USART_CTRLA_SWRST;
    //varakozas, amig a reset befejedodik

    MyUart_waitingForSync(hw);

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

    //Adatatviteli sebesseg beallitasa.
    MyUart_setBitRate(uart, (uart->bitRate==0) ? cfg->bitRate : uart->bitRate);

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
            //Hiba megszakitasa
            SERCOM_USART_INTENSET_ERROR |
            0;

    //Sercom-hoz tartozo interruptok engedelyezese az NVIC-ben.
    //Feltetelezzuk, hogy egy SERCOM-hoz tartozo interruptok egymas utan
    //sorban kovetkeznek, es egy sercom-hoz 4 darab tartozik.
    MySercom_enableIrqs(&uart->sercom);
}
//------------------------------------------------------------------------------
static void MyUart_deinitPeri(MyUart_t* uart)
{
    //SPI periferia tiltasa
    MyUart_disable(uart);

    //Minden msz tiltasa az NVIC-ben
    MySercom_disableIrqs(&uart->sercom);

    //Periferia orajelek tiltasa
    MySercom_disableSlowClock(&uart->sercom);
    MySercom_disableCoreClock(&uart->sercom);
    MySercom_disableMclkClock(&uart->sercom);
}
//------------------------------------------------------------------------------
//Uart periferia resetelese
void MyUart_reset(MyUart_t* uart)
{
    SercomUsart* hw=&uart->sercom.hw->USART;
    MY_ENTER_CRITICAL();
    hw->CTRLA.reg=SERCOM_USART_CTRLA_SWRST;
    MyUart_waitingForSync(hw);
    uart->bitRate=0;
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Uart mukodes engedelyezese
void MyUart_enable(MyUart_t* uart)
{
    SercomUsart* hw=&uart->sercom.hw->USART;
    MY_ENTER_CRITICAL();
    hw->CTRLA.bit.ENABLE=1;
    MyUart_waitingForSync(hw);
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Uart mukodes tiltaasa
void MyUart_disable(MyUart_t* uart)
{
    SercomUsart* hw=&uart->sercom.hw->USART;
    MY_ENTER_CRITICAL();
    hw->CTRLA.bit.ENABLE=0;
    MyUart_waitingForSync(hw);
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Vetel atmeneti tiltasa.
void MyUart_disableRx(MyUart_t* uart)
{
    SercomUsart* hw=&uart->sercom.hw->USART;
    MY_ENTER_CRITICAL();
    hw->CTRLB.bit.RXEN=0;
    MyUart_waitingForSync(hw);
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Vetel visszaengedelyezese
void MyUart_enableRx(MyUart_t* uart)
{
    SercomUsart* hw=&uart->sercom.hw->USART;
    MY_ENTER_CRITICAL();
    hw->CTRLB.bit.RXEN=1;
    MyUart_waitingForSync(hw);
    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Adatatviteli sebesseg beallitasa/modositasa
void MyUart_setBitRate(MyUart_t* uart, uint32_t bitRate)
{
    if (uart->bitRate==bitRate) return;
    uart->bitRate=bitRate;

    if (bitRate==0) return;

    uint32_t coreFreq=uart->sercom.config->coreFreq;
    if (coreFreq==0) return;
    uint16_t baudValeu=(uint16_t)
                        (65536 - (((uint64_t)65536 * 16 * bitRate) / coreFreq));

    SercomUsart* hw=&uart->sercom.hw->USART;

    //Ha be van kapcsolva a sercom, akkor azt elobb tiltani kell.
    if (hw->CTRLA.bit.ENABLE)
    {   //A periferia be van kapcsolva. Azt elobb tiltani kell.
        hw->CTRLA.bit.ENABLE=0;
        MyUart_waitingForSync(hw);

        //baud beallitasa
        hw->BAUD.reg= baudValeu;

        //periferia engedelyezese. (Ez utan mar bizonyos config bitek nem
        //modosithatok!)
        hw->CTRLA.bit.ENABLE=1;
        MyUart_waitingForSync(hw);
    }
    else
    {
        //baud beallitasa
        hw->BAUD.reg= baudValeu;
    }
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
//Kimeneti fifo beregisztralasa
void MyUart_registerTxFifo(MyUart_t* uart, MyFIFO_t* txFifo)
{
    MY_ENTER_CRITICAL();
    uart->tx.txFifo=txFifo;
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
    if (uart->tx.blockSending)
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
    uart->tx.blockSending=1;

    //IT engedelyezese. A kuldes az IT-ben fog elkezdodni...
    hw->INTENSET.reg=SERCOM_USART_INTENSET_DRE;

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Uartra adat kuldese a kimeneti fifo-n keresztul.
//FONTOS! a kimeneti fifot elotte regisztralni kell!!!
status_t MyUart_sendByte(MyUart_t* uart, uint8_t data)
{
    ASSERT(uart->tx.txFifo);

    SercomUsart* hw=&uart->sercom.hw->USART;
    status_t status;

    //IT tiltasa a vizsgalatok idejere
    //hw->INTENCLR.reg=SERCOM_USART_INTENCLR_DRE;

    //A karakter elhelyezese a kimenetei fifoban
    status=MyFIFO_putByte(uart->tx.txFifo, data);

    //IT engedelyezese. A kuldes az IT-ben fog elkezdodni/folytatodni...
    hw->INTENSET.reg=SERCOM_USART_INTENSET_DRE;

    return status;
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

        if ((hw->INTFLAG.bit.ERROR))
        {   //Valamilyen hiba van a sorosporton


            //Hiba jelzo callback meghivasa
            if (uart->errorFunc)
            {
                uart->errorFunc(hw, uart->callbackData);
            }

            //Hiba jelzo msz flag torlese
            hw->INTFLAG.reg=SERCOM_USART_INTFLAG_ERROR;

            wasIT=1;
        }

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
            }
            else
            {   //Minden adatbyte el lett kuldve a blockbol, vagy nem blokk
                //kuldes van...
                if (uart->tx.blockSending)
                {   //blokk kuldes volt.
                    uart->tx.blockSending=0;

                    //Tovabbi Tx interruptok tiltasa
                    hw->INTENCLR.reg = SERCOM_USART_INTENCLR_DRE;
                    __asm("dmb");
                    __asm("isb");

                    //Ha van bereisztralva callback a Tx vegere, akkor meghivjuk...
                    if (uart->tx.doneFunc)
                    {
                        uart->tx.doneFunc(uart->tx.doneFunc_callbackData);
                    }
                }

                if (uart->tx.txFifo)
                {   //Van beregisztralva tx fifo. Ha ababn van tartalom, akkor
                    //abbol tortenik a kovetkezo adatbyte portra helyezese...
                    uint8_t rxData;
                    if (MyFIFO_getByteFromIsr(uart->tx.txFifo, &rxData)==
                                                                kStatus_Success)
                    {   //A fifobol sikerult adatot olvasni, melyet az uartra
                        //helyezett.                        
                        hw->DATA.reg=rxData;
                    } else
                    {   //A tx fifo ures.
                        //Tovabbi Tx interruptok tiltasa
                        hw->INTENCLR.reg = SERCOM_USART_INTENCLR_DRE;
                        __asm("dmb");
                        __asm("isb");
                    }
                }
            }

            wasIT=1;
        }

    }while (wasIT);
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
