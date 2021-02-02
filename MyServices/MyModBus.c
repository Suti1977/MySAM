//------------------------------------------------------------------------------
//  MyModBus service
//
//    File: MyModBus.c
//------------------------------------------------------------------------------
//Stuff:
//  https://www.modbustools.com/modbus.html
//  https://en.wikipedia.org/wiki/Modbus
//  https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf
#include "MyModBus.h"
#include <string.h>

//#define MYMODBUS_TRACE 1
#ifdef MYMODBUS_TRACE
#include "MyDump.h"
#endif

static void __attribute__((noreturn)) MyModBus_task(void* taskParam);
static void MyModBus_uartInit(MyModBus_t* this, const MyModbus_Config_t* cfg);
static void MyModBus_uartRx_cb(SercomUsart* hw, void* callbackData);
static void MyModBus_uartError_cb(SercomUsart* hw, void* callbackData);
static status_t MyModBus_send(MyModBus_t* this, uint8_t* data, uint32_t length);
static void MyModBus_feeding(MyModBus_t* this, uint8_t rxByte);
static void MyModBus_rxTimeout(MyModBus_t* this);
static uint16_t MyModBus_CRC16(const uint8_t *nData, uint16_t wLength);
static void MyModBus_parseFunction(MyModBus_t* this);
static void MyModBus_sendResponse(MyModBus_t* this);
static void MyModBus_uart_txDone_cb(void* callbackData);

//Uj adat a bemeneti RxFifo-ban. fel kell dolgozni
#define NOTIFY__RX                      (1 << 2)

//------------------------------------------------------------------------------
//service kezdeti inicializalasa
void MyModBus_init(MyModBus_t* this, const MyModbus_Config_t* cfg)
{
    //Modul valtozoinak kezdeti torlese.
    memset(this, 0, sizeof(MyModBus_t));

    //parameterek atvetele
    this->regTable=cfg->regTable;
    this->rxTimeout=cfg->rxTimeout;

    //Modul valtozoit vedo mutex letrehozasa
    this->mutex=xSemaphoreCreateMutex();
    ASSERT(this->mutex);

    //Sorosportos kuldes veget jelzo szemafor letrehozasa
    this->sendDoneSemaphore=xSemaphoreCreateBinary();
    ASSERT(this->sendDoneSemaphore);

    //Uart driver letrehozas
    MyUart_create(&this->uart, cfg->uartConfig, cfg->sercomConfig);


    //kommunikaciot futtato taszk letrehozasa
    if (xTaskCreate(MyModBus_task,
                    cfg->taskName,
                    (const configSTACK_DEPTH_TYPE)cfg->taskStackSize,
                    this,
                    cfg->taskPriority,
                    &this->taskHandler)!=pdPASS)
    {
        ASSERT(0);
    }


    //A veteli fifo letrehozasa. (Ebbe tortenik a bemenetrol erkezo
    //stream bufferelese.)
    MyFIFO_init(&this->rxFifo, &cfg->rxFifoCfg);

    //Uart periferie inicializalasa
    MyModBus_uartInit(this, cfg);

    //Ha nem broadcast (0) a cim, akkor arra beallitja a cimet.
    //Ez kesobb is modosithato.
    MyModBus_setSlaveAddress(this, cfg->slaveAddress);
}
//------------------------------------------------------------------------------
//Uart periferia inicializalasa
static void MyModBus_uartInit(MyModBus_t* this, const MyModbus_Config_t* cfg)
{
    //Uart konfiguralasa...
    if (cfg->peripheriaInitFunc)
    {
        cfg->peripheriaInitFunc(&this->uart, cfg->callbackData);
    }

    //Callback funkciok beregisztralasa
    MyUart_registreCallbacks(&this->uart,
                             MyModBus_uartRx_cb,
                             MyModBus_uartError_cb,
                             this);
    //Uart engedelyezese
    MyUart_enable(&this->uart);
}
//------------------------------------------------------------------------------
//Uj karakter vetelekor felojovo callback fuggveny
//[INTERRUPTBAN FUT!]
static void MyModBus_uartRx_cb(SercomUsart* hw, void* callbackData)
{
    MyModBus_t* this=(MyModBus_t*) callbackData;

    //True-lesz, ha a kilepes elott kontextust kell valtani, mivel egy
    //magasabb prioritasu taszk jelzett.
    BaseType_t xHigherPriorityTaskWoken=pdFALSE;

    //Az uartrol olvashato adatok betolasa a bemeneti streambufferbe...
    while(MyUart_receivedDataAvailable(hw))
    {
        //Adat olvasasa az uart veteli fifojabol, majd atadas a modulhoz tartozo
        //konzol kezelonek
        uint8_t RxData=MyUart_readRxReg(hw);

        //A vett adatokkal pedig toltjuk a bemeneti Fifo-ba...
        MyFIFO_putBytesFromIsr(&this->rxFifo, RxData);
    }

    //Jelzes a taszknak, hogy ebredjen fel, mert vannak ujabb karakterek
    xTaskNotifyFromISR( this->taskHandler,
                        (uint32_t) NOTIFY__RX,
                        eSetBits,
                        &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
//------------------------------------------------------------------------------
//UART hiba eseten feljovo callback fuggveny
static void MyModBus_uartError_cb(SercomUsart* hw, void* callbackData)
{
    MyModBus_t* this=(MyModBus_t*) callbackData;
    (void) hw;
    (void) this;
    //TODO: sorosporti hibat itt lehetne kezelni
}
//------------------------------------------------------------------------------
//Akkor jon fel, ha az uarton egy adathalmaz kuldesevel elkeszult
//[MEGSZAKITASBAN FUT!]
static void MyModBus_uart_txDone_cb(void* callbackData)
{
    MyModBus_t* this=(MyModBus_t*) callbackData;

    //True-lesz, ha a kilepes elott kontextust kell valtani,
    //mivel egymagasabb prioritasu taszk jelzett.
    BaseType_t xHigherPriorityTaskWoken=pdFALSE;
    xSemaphoreGiveFromISR(this->sendDoneSemaphore,
                          &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}
//------------------------------------------------------------------------------
//Adattartalom kiirasa az uarton.
//A rutin bevarja, amig az uarton az osszes adat kimegy
static status_t MyModBus_send(MyModBus_t* this, uint8_t* data, uint32_t length)
{

    status_t status;
    //Uzenet kuldese a sorosporton...
    status=MyUart_sendNonBlocking(  &this->uart,
                                    data,
                                    length,
                                    MyModBus_uart_txDone_cb,
                                    this);
    if (status) return status;

    //varakozas arra, hogy a tartalom kimenjen az uarton
    xSemaphoreTake(this->sendDoneSemaphore, portMAX_DELAY);

    return kStatus_Success;
}
//------------------------------------------------------------------------------
static uint16_t MyModBus_CRC16(const uint8_t *nData, uint16_t wLength)
{
    static const uint16_t wCRCTable[] =
    {
       0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
       0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
       0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
       0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
       0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
       0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
       0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
       0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
       0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
       0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
       0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
       0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
       0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
       0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
       0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
       0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
       0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
       0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
       0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
       0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
       0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
       0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
       0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
       0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
       0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
       0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
       0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
       0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
       0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
       0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
       0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
       0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
    };

    uint8_t nTemp;
    uint16_t wCRCWord = 0xFFFF;

    while (wLength--)
    {
        nTemp = *nData++ ^ (uint8_t)wCRCWord;
        wCRCWord >>= 8;
        wCRCWord  ^= wCRCTable[nTemp];
    }
    return wCRCWord;
}
//------------------------------------------------------------------------------
//kommunikaciot futtato taszk
static void __attribute__((noreturn)) MyModBus_task(void* taskParam)
{
    MyModBus_t* this=(MyModBus_t*) taskParam;

    uint32_t waitTime=portMAX_DELAY;
    while(1)
    {


        //Varakozas ujabb karakterre a sorosporton, mellyel a protokoll
        //ertelmezo etetheto.
        //Az xTaskNotifyWait() fuggvenyben a FreeRTOS varakozni fog egy
        //TaskNotify esemenyre, melyet a vevo megszakitasban generalunk,
        //miutan a vett karaktereket a RxFifoba helyeztuk.
        //Akkor is ebresztjuk a taszkot, ha adott ideig nem erkezett ujabb
        //karakter, egy megkezdett csomagban.
        uint32_t events=0;
        if (xTaskNotifyWait(0, 0xffffffff, &events, waitTime)==pdFALSE)
        {   //TIMEOUT
            MyModBus_rxTimeout(this);

            //A tovabbiakban nem kell idoziteni
            waitTime=portMAX_DELAY;
        } else
        {
            if (events & NOTIFY__RX)
            {   //Uj byte erekezett.

                //Bemeneti stream olvasasa az RxFifo-bol...
                //(Ciklus, amig van a fifo-ban adat)
                uint8_t rx_byte;
                while(MyFIFO_getByte(&this->rxFifo, &rx_byte)==kStatus_Success)
                {
                    MyModBus_feeding(this, rx_byte);
                }

                waitTime=this->rxTimeout;
            }
        }
    }
}
//------------------------------------------------------------------------------
//Sajat cim beallitasa
void MyModBus_setSlaveAddress(MyModBus_t* this, uint8_t slaveAddr)
{
    //broadcast cimet nem lehet beallitani
    if (slaveAddr == MODBUS_BROADCAST_ADDRESS) return;

    this->slaveAddress=slaveAddr;
    this->slaveAddressActive=true;
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha ujabb karaktert kapott
static void MyModBus_feeding(MyModBus_t* this, uint8_t rxByte)
{
    if (this->inFrame==false)
    {   //Meg nem volt keret kezdet. Ez egy csomag elso byteja.

        //Jelezzuk, hogy egy keretet veszunk...
        this->inFrame=true;
        this->frameLength=0;
    }

    if (this->inFrame)
    {   //egy megkezdett keretben vagyunk. A vett karaktert buffereljuk.
        if (this->frameLength<=MAX_MODBUS_FRAME_LENGTH)
        {   //van meg hely a bemeneti fifoban. Buffereljuk
            this->rxFrame.buff[this->frameLength]=rxByte;
            this->frameLength++;
        } else
        {   //Hosazabb uzenet akar erkezni, mint amekkorat fogdni tudunk.
            //Az uzenet el lesz dobva.
            this->drop=true;
        }
    }
}
//------------------------------------------------------------------------------
//akkor hivodik, ha egy ideig nem kapott uj karaktert.
static void MyModBus_rxTimeout(MyModBus_t* this)
{
    if (this->inFrame)
    {   //egy keret vetele volt

        //Jelzes torolehto. Ujabb keret vetele kezdodhet.
        this->inFrame=false;

        //Ellenorizzuk az uzenetet hosszra
        if ((this->drop) || (this->frameLength<MODBUS_MIN_FRAME_LENGTH))
        {   //Az uzenetet el kell dobni, mert valami hiba volt vele.
            this->drop=false;

            #ifdef MYMODBUS_TRACE
              printf("Mod bus frame dropped.\n");
            #endif

            return;
        }

        bool broadcast=false;

        //ellenorzes, hogy az uzenet nekunk szol-e....
        //Egyezik-e a cimunkel, vagy broadcast cimet kaptunk. A cim a bufferben
        //a legelso byte.
        uint8_t frameAddress=this->rxFrame.slaveAddr;
        if ((frameAddress == this->slaveAddress) && (this->slaveAddressActive))
        {   //ez a mi uzenetunk!

        } else
        if (frameAddress==MODBUS_BROADCAST_ADDRESS)
        {   //broadcast uzenet
            //erre nem szabad valaszt kuldeni!
            broadcast=true;
        } else
        {
            //az uzenet nem nekunk szol.

            //az uzenetet eldobjuk
            return;
        }
        //Az uzenet nekunk (is) szol.

        uint16_t frameLength=this->frameLength;



        //CRC ellenorzes...
        uint16_t frameCrc= (uint16_t)
                ((uint16_t)this->rxFrame.buff[frameLength-1] << 8) |
                ((uint16_t)this->rxFrame.buff[frameLength-2]);
        uint16_t calcCrc=MyModBus_CRC16(this->rxFrame.buff, frameLength-2);

        #ifdef MYMODBUS_TRACE
          printf("Mod bus rx. Length=%d\n", frameLength);
          MyDump_memorySpec(this->rxFrame.buff, frameLength, 0);
          printf("\nFrameCRC: %04X  CalcCrc: %04X\n", frameCrc, calcCrc);
        #endif

        if (frameCrc!=calcCrc)
        {   //A CRC hibas. Az uzenetet eldobjuk.
            return;
        }


        //Az uzenet ok.
        //Funkciokod alapjan tovabbi uzenet feldolgozas
        MyModBus_parseFunction(this);

        //valasz kuldese, de csak, ha nem broadcast volt a parancs
        if (broadcast==false)
        {
            MyModBus_sendResponse(this);
        }
    }
}
//------------------------------------------------------------------------------
//Tobb 16 bites regiszter irasa
static void MyModBus_writeMultipleRegisters(MyModBus_t* this)
{
    MyModBus_Req_WriteMultipleRegisters_t* req=
            &this->rxFrame.req.writeMultipleRegisters;


    MyModBus_Resp_WriteMultipleRegisters_t* resp=
            &this->txFrame.resp.writeMultipleRegisters;

    status_t status=kStatus_Success;

    uint16_t addr=__builtin_bswap16(req->startingAddress);
    //A hatralevo elemeket szamolja 0-ig.
    uint16_t leftCnt=__builtin_bswap16(req->quntityOfRegisters);

    #ifdef MYMODBUS_TRACE
      printf("Write multiple registers...   Start: %d   quantity:%d\n", addr, leftCnt);
    #endif

    //forrason vegighalado pointer
    uint16_t* src=req->registerValues;

    //Az atadott byteokon endiant forditunk, mivel big endianban ertelmezik.
    for(uint8_t i=0; i<leftCnt; i++)
    {
        *src=__builtin_bswap16(*src);
        src++;
    }
    src=req->registerValues;

    //cim tabalazat elso elemere allas
    const MyModBus_AddrTable_t* tablePtr=this->regTable;

    //ciklus, amig a kivant mennyisegu regiszteren vegig nem ert...
    while(leftCnt)
    {
        uint16_t skipLen;

        //A tablazatban azt ez elemet keressuk, aminek cim tartomanyaba beleesik
        //a soron levo cim (addr), vagy amelyik az utan all. Azt is figyeljuk,
        //hogy van-e definialva iro fuggveny az adott tartomanyhoz....
        const MyModBus_AddrTable_t* foundTable=NULL;
        while(1)
        {
            //Az elemszam, ha 0, akkor az jelzi a tablazat veget.
            if (tablePtr->qantity==0) break;

            if (((tablePtr->startAddress >= addr) ||
                ((tablePtr->startAddress+tablePtr->qantity) > addr)) &&
                  (tablePtr->setFunc))
            {
                foundTable=tablePtr;
                break;
            }
            tablePtr++;
        }

        if (foundTable==NULL)
        {   //nem talalt olyan elemet a tablazatban, mely megfelelne.
            //muvelet befejezese
            break;
        } else
        {   //A soron levo cim beleesik egy regiszter tartomanyba, vagy
            //a soron kovetjezo bejegyzes elott talalhato.

            if (addr < tablePtr->startAddress)
            {
                skipLen=tablePtr->startAddress - addr;

                if (skipLen>leftCnt)
                {   //ha tobbet kellene ugrani, mint amenyit egyeltalan irni
                    //kell meg, akkor az azt jelenti, hogy vegeztunk.
                    skipLen=leftCnt;
                }

                //A hatralevo elemszamot csokkentjuk az ugras hosszaval...
                leftCnt -= skipLen;
                //A soron levo cim is annyival odebb lep
                addr += skipLen;
                src += skipLen;
            }

            //ellenorzes, hogy kell-e meg valamit csinalni...
            if (leftCnt==0)
            {   //Nincs hatra tobb olvasando byte.
                break;
            }

            //Az iro fuggvenynek atadando regiszter mennyiseg.
            //(Az iro fuggveny ebben adja majd vissza tenylegesen kiirt
            //regiszter szamot.)
            uint16_t funcQuantity=leftCnt;
            //A tartomanybol hatralevo regiszterek szama
            uint16_t leftInRange;
            //A tartomanybon beluli elso regiszter
            uint16_t offset=addr-tablePtr->startAddress;

            if (offset>0)
            {   //a regisztertartalom koztes elemetol indul a lekerdezes.
                leftInRange = tablePtr->qantity-offset;
            } else
            {
                leftInRange=tablePtr->qantity;
            }

            if (funcQuantity>leftInRange) funcQuantity=leftInRange;

            uint16_t x=funcQuantity;
            //olvado fuggveny meghivasa.
            //(A ciklus elejen az ellenorzes miatt, csak akkor kerulhet ide a
            //vezerles, ha van definialva olvaso fuggveny.)
            status=tablePtr->setFunc(offset,
                                     src,
                                     &funcQuantity,
                                     tablePtr->callbackData);
            if (status) goto error;

            src+=funcQuantity;
            //Amennyivel kevesebbet tett a sterambe a fuggevny, annyi dumy
            //ertekkel kel feltolteni. fillCnt-be lesz beallitva...
            skipLen=funcQuantity-x;
            addr+=(funcQuantity+skipLen);
            leftCnt-=(funcQuantity+skipLen);
            src+=skipLen;
        }
    }
error:
    if (status)
    {   //volt valami hiba a parancs vegrehajtasakor

        //TODO: statuszkod alapjan hibat beallitani. Esetleg erre kulon
        //      lookup fuggvenyt.
        this->exceptionCode=MODBUS_EXCEPTION_NOT_DEFINED;
        return;
    }


    //Valaszban beallitjuk a beallitott regiszter mennyisegnek megfelelo
    //byte szamot.
    resp->quntity= req->quntityOfRegisters;
    resp->startingAddress=req->startingAddress;

    //Valsz hosszanak beallitasa
    this->responseLength= sizeof(MyModBus_Resp_WriteMultipleRegisters_t);
}
//------------------------------------------------------------------------------
//Egyetlen 16 bites regiszter irasa
static void MyModBus_writeSingleRegister(MyModBus_t* this)
{
    status_t status=kStatus_Success;

    MyModBus_Req_WriteSingleRegister_t* req=
            &this->rxFrame.req.writeSingleRegister;

    uint16_t regAddress    =__builtin_bswap16(req->registerAddress);
    uint16_t regValue      =__builtin_bswap16(req->registerValue);

    #ifdef MYMODBUS_TRACE
      printf("Write single registers...   Addr: %d   Value:%d\n", regAddress, regValue);
    #endif

    MyModBus_Resp_WriteSingleRegister_t* resp=
            &this->txFrame.resp.writeSingleRegister;



    //A cimnek megfelelo tablazat bejegyzes megkeresese...
    const MyModBus_AddrTable_t* table=NULL;
    const MyModBus_AddrTable_t* ptr=this->regTable;
    while(1)
    {
        //quantity mezo 0 erteke jelzi a tablazat veget.
        if (ptr->qantity==0) break;

        if ((ptr->startAddress <= regAddress) &&
            ((ptr->startAddress + ptr->qantity)>regAddress))
        {   //A cim a soron levo tartomanyba esik. Visszaadjuk a bejegyzest.
            table=ptr;
            break;
        }
        ptr++;
    }

    if (table==NULL)
    {   //Nincs a cimhez tartoz regiszter
        this->exceptionCode=MODBUS_EXCEPTION_NOT_DEFINED;
        return;
    }

    if (table->setFunc==NULL)
    {   //A cimtartromanyhoz nem tartozik iro fuggveny. Ez csak olvashato
        //regiszter.
        this->exceptionCode=MODBUS_EXCEPTION_NOT_DEFINED;
        return;
    }

    uint16_t quantity=1;
    status=table->setFunc( (regAddress - table->startAddress),
                            &regValue,
                            &quantity,
                            table->callbackData);

    if (status)
    {   //volt valami hiba a parancs vegrehajtasakor

        //TODO: statuszkod alapjan hibat beallitani. Esetleg erre kulon
        //      lookup fuggvenyt.
        this->exceptionCode=MODBUS_EXCEPTION_NOT_DEFINED;
        return;
    }

    resp->registerValue=  req->registerValue;
    resp->registerAddress=req->registerAddress;

    //Valsz hosszanak beallitasa
    this->responseLength= sizeof(MyModBus_Resp_WriteMultipleRegisters_t);
}
//------------------------------------------------------------------------------
//Tobb 16 bites regiszter olvasasa
static void MyModBus_readHoldingRegister(MyModBus_t* this)
{
    MyModBus_Req_ReadHoldingRegisters_t* req=
            &this->rxFrame.req.readHoldingRegisters;

    MyModBus_Resp_ReadHoldingRegisters_t* resp=
            &this->txFrame.resp.readHoldingRegisters;

    status_t status=kStatus_Success;

    uint16_t addr=__builtin_bswap16(req->startingAddress);
    //A hatralevo elemeket szamolja 0-ig.
    uint16_t leftCnt=__builtin_bswap16(req->quantityOfRegisters);
    uint16_t quantityOfRegisters=leftCnt;

    #ifdef MYMODBUS_TRACE
      printf("ReadHoldingRegister...   Start: %d   quantity:%d\n", addr, leftCnt);
    #endif

    //Ha tobbet akarnanak olvasni, mint amennyit egy keret elbir, az hiba.
    //Szaturalunk a maximumra.
    if (leftCnt>MODBUS_MAX_READABLE_REGISTZER)
    {
        leftCnt=MODBUS_MAX_READABLE_REGISTZER;
    }

    //kimeneti steramet iro pointer
    uint16_t* dest=resp->registerValue;

    //cim tabalazat elso elemere allas
    const MyModBus_AddrTable_t* tablePtr=this->regTable;

    //ciklus, amig a kivant mennyisegu regiszter ertek a sterambe nem irodik...
    while(leftCnt)
    {
        uint16_t fillCnt;

        //A tablazatban azt ez elemet keressuk, aminek cim tartomanyaba beleesik
        //a soron levo cim (addr), vagy amelyik az utan all. Azt is figyeljuk,
        //hogy van-e definialva olvaso fuggveny az adott tartomanyhoz....
        const MyModBus_AddrTable_t* foundTable=NULL;
        while(1)
        {
            //Az elemszam, ha 0, akkor az jelzi a tablazat veget.
            if (tablePtr->qantity==0) break;

            if (((tablePtr->startAddress >= addr) ||
                ((tablePtr->startAddress+tablePtr->qantity) > addr)) &&
                  (tablePtr->getFunc))
            {
                foundTable=tablePtr;
                break;
            }
            tablePtr++;
        }

        if (foundTable==NULL)
        {   //nem talalt olyan elemet a tablazatban, mely megfelelne.

            //A leftCnt-nek megfelelo szamu dumy adatot tesz a streambe,
            //majd befejezi a mukodest.
            for(; leftCnt; leftCnt--)
            {
                *dest++=MYMODBUS_FILL_VALUE;
            }
            break;
        } else
        {   //A soron levo cim beleesik egy regiszter tartomanyba, vagy
            //a soron kovetjezo bejegyzes elott talalhato.

            fillCnt=0;
            //kell a tablazat start-jaig kitolto byte?
            if (addr < tablePtr->startAddress)
            {   //A soron kovetkezo elso valos cimig kell kitoltes. Kiszamoljuk
                //a tolto byteok szamat.
                fillCnt=tablePtr->startAddress - addr;

                //Ha kevesebb lenne a hatralevo igeny, akkor arra szaturalunk.
                if (fillCnt>leftCnt) fillCnt=leftCnt;

                //A hatralevo elemszamot csokkentjuk a feltoltes hosszaval...
                leftCnt -= fillCnt;
                //A soron levo cim is annyival odebb lep
                addr += fillCnt;

                //feltoltes...
                for(;fillCnt; fillCnt--)
                {
                    *dest++ = MYMODBUS_FILL_VALUE;
                }
            }

            //ellenorzes, hogy kell-e meg valamit csinalni...
            if (leftCnt==0)
            {   //Nincs hatra tobb olvasando byte.
                break;
            }

            //Az olvaso fuggvenynek atadando regiszter mennyiseg.
            //(Az olvaso fuggveny ebben adja majd vissza tenylegesen olvasott
            //regiszter szamot.)
            uint16_t funcQuantity=leftCnt;
            //A tartomanybol hatralevo regiszterek szama
            uint16_t leftInRange;
            //A tartomanybon beluli elso regiszter
            uint16_t offset=addr-tablePtr->startAddress;

            if (offset>0)
            {   //a regisztertartalom koztes elemetol indul a lekerdezes.
                leftInRange = tablePtr->qantity-offset;
            } else
            {
                leftInRange=tablePtr->qantity;
            }

            if (funcQuantity>leftInRange) funcQuantity=leftInRange;

            uint16_t x=funcQuantity;
            //olvado fuggveny meghivasa.
            //(A ciklus elejen az ellenorzes miatt, csak akkor kerulhet ide a
            //vezerles, ha van definialva olvaso fuggveny.)
            status=tablePtr->getFunc(offset,
                                     dest,
                                     &funcQuantity,
                                     tablePtr->callbackData);
            if (status) goto error;

            dest+=funcQuantity;
            //Amennyivel kevesebbet tett a sterambe a fuggevny, annyi dumy
            //ertekkel kel feltolteni. fillCnt-be lesz beallitva...
            fillCnt=funcQuantity-x;
            addr+=(funcQuantity+fillCnt);
            leftCnt-=(funcQuantity+fillCnt);

            for(; fillCnt; fillCnt--)
            {
                *dest++=MYMODBUS_FILL_VALUE;
            }
        }
    }

error:
    if (status)
    {   //volt valami hiba a parancs vegrehajtasakor

        //TODO: statuszkod alapjan hibat beallitani. Esetleg erre kulon
        //      lookup fuggvenyt.
        this->exceptionCode=MODBUS_EXCEPTION_NOT_DEFINED;
        return;
    }

    //Valaszban beallitjuk az atadott regiszter mennyisegnek megfelelo
    //byte szamot.
    resp->byteCount= (uint8_t) quantityOfRegisters * 2;

    //Az atadott byteokon endiant forditunk, mivel big endianban ertelmezik.
    for(uint8_t i=0; i<quantityOfRegisters; i++)
    {
        resp->registerValue[i]=__builtin_bswap16(resp->registerValue[i]);
    }

    //Valsz hossza az atadott byteok szama + maga a byteszam mezo
    this->responseLength= resp->byteCount + 1;
}
//------------------------------------------------------------------------------
//Funkciokod alapjan uzenet feldolgozas
static void MyModBus_parseFunction(MyModBus_t* this)
{
    //A funkcio vegrehajtasa elott nincs hiba
    this->exceptionCode=0;
    this->responseLength=0;

    //Az 1. indexen a funkcio kod.
    uint8_t functionCode=this->rxFrame.req.functionCode;

    #ifdef MYMODBUS_TRACE
      printf("FunctionCode: %d\n", functionCode);
    #endif

    switch(functionCode)
    {
        //case MODBUS_FC_READ_COILS:
        //    break;
        //case MODBUS_FC_READ_DISCRETE_INPUTS:
        //    break;
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            MyModBus_readHoldingRegister(this);
            break;
        //case MODBUS_FC_READ_INPUT_REGISTERS:
        //    break;
        //case MODBUS_FC_WRITE_SINGLE_COIL:
        //    break;

        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            MyModBus_writeSingleRegister(this);
            break;
        //case MODBUS_FC_READ_EXCEPTION_STATUS:
        //    break;
        //case MODBUS_FC_WRITE_MULTIPLE_COILS:
        //    break;

        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            MyModBus_writeMultipleRegisters(this);
            break;

        //case MODBUS_FC_REPORT_SLAVE_ID:
        //    break;
        //case MODBUS_FC_MASK_WRITE_REGISTER:
        //    break;
        //case MODBUS_FC_WRITE_AND_READ_REGISTERS:
        //    break;

        default:
            #ifdef MYMODBUS_TRACE
              printf("Unknown MODBUS Function. Code: %d\n", functionCode);
            #endif
            this->exceptionCode=MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
            break;
    }

}
//------------------------------------------------------------------------------
//Valasz kuldese
static void MyModBus_sendResponse(MyModBus_t* this)
{
    uint16_t responseLength;

    //0. byte a slave cime
    this->txFrame.slaveAddr=this->rxFrame.slaveAddr;

    //1. funkcio kod, vagy 0x80-al vagyolva hiba eseten   
    volatile uint8_t funcCode=this->rxFrame.req.functionCode;

    if (this->exceptionCode!=0)
    {   //van hibakod. Exception kerul kuldesre.
        funcCode |= 128;
        this->txFrame.resp.exception.exceptionCode=this->exceptionCode;
        responseLength=1;
    } else
    {   //nincs hiba. Normal response kerul elkuldesre.
        responseLength=this->responseLength;
    }
    this->txFrame.resp.functionCode=funcCode;

    //A cimet, es a funkcio koddal hoszabb a teljes uzenet.
    responseLength+=2;

    //uzenetre CRC szamitas...
    uint16_t calcCrc=MyModBus_CRC16(this->txFrame.buff, responseLength);
    this->txFrame.buff[responseLength  ]=calcCrc & 0xff;
    this->txFrame.buff[responseLength+1]=calcCrc >> 8;

    //A CRC is hozzaadodik a teljes uzenet hosszhoz
    responseLength+=2;

    #ifdef MYMODBUS_TRACE
      printf("tx:  length:%d\n", responseLength);
      MyDump_memorySpec(this->txFrame.buff, responseLength, 0);
      printf("\n");
    #endif

    //Uzenet kuldese a sorosporton...
    //A hivott rutin addig nem ter vissza, amig az ossze byte ki nem ment a
    //soros vonalon...
    MyModBus_send(this, this->txFrame.buff, responseLength);
}
//------------------------------------------------------------------------------
