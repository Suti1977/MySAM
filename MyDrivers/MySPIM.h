//------------------------------------------------------------------------------
//  SPI Master driver
//
//    File: MySPIM.h
//------------------------------------------------------------------------------
#ifndef MYSPI_H_
#define MYSPI_H_

#include "MySercom.h"
//------------------------------------------------------------------------------
//SPI periferia konfiguracios parameterei, melyet az MySPIM_init() fuggvenynek
//adunk at initkor.
typedef struct
{
    //A periferia IRQ prioritasa
    uint32_t irqPriorities;

    //A (kezdeti) adatatvitei sebesseghez tartozo BAUD ertek.
    //Ez fugg a Sercomhoz rendelt GCLK altal eloallitott orajel frekitol.
    //Kiszamitasa a MYSPIM_CALC_BAUDVALUE() makro segitsegevel egyszerubb.
    uint32_t    bitRate;

    //Konfiguracios attributumok (bitmaszk mezo)
    //Az ertelmezett attributumok definicioit lasd lejebb, a "MYSPIM_ATTR_"
    //definicioknal
    uint32_t    attribs;

    //Ha ez true, akkor a hardver kezeli az SS vonalat (slave select)
    bool hardwareSSControl;

} MySPIM_Config_t;
//------------------------------------------------------------------------------
//SPI master modot befolyasolo konfiguracios attributumok, melyeket a periferia
//konfiguraciojanal hasznalunk az Attribs mezo feltoltesere.
//Az egyes attributumok bitmaszkot kepeznek, igy azokat vagy kapcsolattal
//hasznalhatjuk.
//A SAM mikrovezerlok SPI  periferiajanak a CTRLA regiszter bitjeinek
//a definicioi kerulnek felhasznalasra, igy a config beallitasa egyszerusodik,
//mert csak az osszerakott attributum mezot kell a CTRLA regiszterbe irni.
//Mivel a regiszternek vannak nem hasznalt bitjei, igy azokat a tobbi konfi-
//guracios attributumhoz hasznaljuk, termeszetesen a CTRLA regiszterebe azok
//beirasat egy maszkolassal tiltjuk.
//FONTOS: Ha CTRLA regisztert erinto beallitas valtozik vagy kerul be, akkor a
//        MYSPIM_CTRLA_CONFIG_MASK definiciot utana kell allitani!

//Adatbit sorrend   (Lasd: CTRLA.DORD)
//MSB megy elobb
#define MYSPIM_ATTR_MSB_FIRST           0
//LSB megy elobb
#define MYSPIM_ATTR_LSB_FIRST           SERCOM_SPI_CTRLA_DORD

//SP transfer modok
//Ezek fuggenek a CPOL allapotatatol
// Mode  Leadin Edge      Trailing Edge    Clk idle state
// 0:   Rising, sample    Falling, change         low
// 1:   Rising, change    Falling, sample         low
// 2:   Falling, sample   Rising, change          high
// 3:   Falling, change   Rising, sample          high
#define MYSPIM_ATTR_MODE0    (0                      | 0                    )
#define MYSPIM_ATTR_MODE1    (0                      | SERCOM_SPI_CTRLA_CPHA)
#define MYSPIM_ATTR_MODE2    (SERCOM_SPI_CTRLA_CPOL  | 0                    )
#define MYSPIM_ATTR_MODE3    (SERCOM_SPI_CTRLA_CPOL  | SERCOM_SPI_CTRLA_CPHA)

//MISO pad megadasa (lasd: CTRLA.DIPO)
//Lehetseges ertekek: 0-3. A sercom PAD0-PAD3 kozul valaszt.
#define MYSPIM_ATTR_DIPO(pad)           SERCOM_SPI_CTRLA_DIPO(pad)
//MOSI. SCK, SS padek megadasa (lasd: CTRLA.DOPO)
//(2 lehetoseg van)
//MOSI: PAD0  SCK: PAD1  SS: PAD2
#define MYSPIM_ATTR_0MOSI_1SCK_2SS      0
//MOSI: PAD3  SCK: PAD1  SS: PAD2
#define MYSPIM_ATTR_3MOSI_1SCK_2SS      SERCOM_SPI_CTRLA_DOPO(2)

//Immediate Buffer Overflow Notification
#define MYSPIM_ATTR_IBON                SERCOM_SPI_CTRLA_IBON

//Run In Standby
#define MYSPIM_ATTR_RUNSTDBY            SERCOM_SPI_CTRLA_RUNSTDBY
//..............................................................................
//CTRLA-ba valo beirashoz a konfigracios attributumok maszkja.
#define MYSPIM_CTRLA_CONFIG_MASK      (SERCOM_SPI_CTRLA_RUNSTDBY            | \
                                       SERCOM_SPI_CTRLA_IBON                | \
                                       SERCOM_SPI_CTRLA_DOPO_Msk            | \
                                       SERCOM_SPI_CTRLA_DIPO_Msk            | \
                                       SERCOM_SPI_CTRLA_DORD)
//------------------------------------------------------------------------------
//Seged makro, az SPI periferia BAUD regiszter ertekenek kiszamitasahoz.
//
//                  gclkfreq
// BAUDREG = -----------------------
//                  2 * baud
//
// gclkfreq: A sercom Core orajelehez rendelt GCLK modul kimeneti frekvenciaja
// baud:     Az SPI busz sebesseg
// trise:    ride time ido [ns-ban]. 215-300 us kozott lehet!
//
//#define MYSPIM_CALC_BAUDVALUE(gclkfreq, baud)
//                            ((uint8_t)((gclkfreq / (2*baud))-1))
//------------------------------------------------------------------------------
//Kuldes/fogadas leiroja. Ha txData es vagy rxData NULL, akkor az adott
//transzfert nem hajtja vegre. Ha mindegyik be van allitva, akkor egyidoben
//kuldes es fogadas is tortenik.
typedef struct
{
    //Kikuldendo adatok.
    uint8_t*   txData;
    //olvasando adatok celterulete.
    uint8_t*   rxData;
    //kuldott es vagy olvasott byteok szama.
    uint32_t length;
} MySPIM_xfer_t;
//------------------------------------------------------------------------------
//MySPIM driver valtozoi
typedef struct
{
    //Driver konfiguracio
    const MySPIM_Config_t*  config;

    //Az SPI interface alapszintu sercom driver-enek leiroja
    MySercom_t   sercom;

  #ifdef USE_FREERTOS
    //A taszkok kozotti busz hozzaferest kizaro mutex.
    SemaphoreHandle_t   busMutex;
    //Az intarrupt alat futo folyamatok vegere varo szemafor
    SemaphoreHandle_t   semaphore;
  #endif //USE_FREERTOS

    //A feldolgozas alatt allo tranzakcios blokk leirojara mutat
    const MySPIM_xfer_t*  xferBlock;
    //A meg hatralevo tranzakcios blokkok szama
    uint32_t        leftXferBlockCount;

    //A blokkbol hatralevo byteok szama
    uint32_t        leftByteCount;
    //Kiirando adatbyteokat cimzi...
    uint8_t*        txPtr;
    //Beolvasott adatbyteokat cimzi
    uint8_t*        rxPtr;

    //A busz sebessege
    uint32_t    bitRate;
} MySPIM_t;
//------------------------------------------------------------------------------
//SS (slave select/chip select) vonal vezerleset vegzo callback fuggvenyek
//prototipusa.
//select: true, eseten a slavet ki kell valasztani. (Altalanosan alacsonyba kell
//helyezni a kivalaszto vonalat.)
typedef void MySPIM_slaveSelectFunc_t(bool select);
//------------------------------------------------------------------------------
//Altalanos SPI buszra kotott eszkozok elerese
//Egy-egy ilyen strukturaban tarolodnak azok az informaciok, melyek egy eszkoz
//eleresehez szuksegesek.
typedef struct
{
    //A hozza tartozo SPI driver handlerere mutat
    MySPIM_t*   spi;
    //A slave select vonalat vezerlo callback fuggveny
    MySPIM_slaveSelectFunc_t* slaveSelectFunc;
    //Az eszkozhoz tartozo driver valtozoira mutat. Ezt minden driver eseten
    //sajat tipusanak megfeleloen kell kasztolni.
    void*       handler;
} MySPIM_Device_t;
//------------------------------------------------------------------------------
//SPI eszkoz letrehozasa
//spiDevice: A letrehozando eszkoz leiroja
//spi: Annak a busznak a MySPIM driverenek handlere,melyre az eszkoz csatlakozik
//slaveSelectFunc: A slave select (chip select) vonal vezerleset megvalosito
//                 callback funkcio
//handler: Az SPI-s eszkozhoz tartozo driver handlere
void MySPIM_createDevice(MySPIM_Device_t *spiDevice,
                         MySPIM_t *spi,
                         MySPIM_slaveSelectFunc_t* slaveSelectFunc,
                         void *handler);


//driver kezdeti inicializalasa, letrehozasa
void MySPIM_create(MySPIM_t* spim,
                   const MySPIM_Config_t* config,
                   const MySercom_Config_t* sercomCfg);

//SPI driver es eroforrasok felaszabditasa
void MySPIM_destory(MySPIM_t* spim);

//SPI Periferia inicializalasa/engedelyezese
void MySPIM_init(MySPIM_t* spim);
//SPI Periferia tiltasa. HW eroforrasok tiltasa.
void MySPIM_deinit(MySPIM_t* spim);

//SPI periferia resetelese
void MySPIM_reset(MySPIM_t* spim);
//SPI mukodes engedelyezese
void MySPIM_enable(MySPIM_t* spim);
//SPI mukodes tiltaasa
void MySPIM_disable(MySPIM_t* spim);

//SPI busz sebesseg modositasa/beallitasa
void MySPIM_setBitRate(MySPIM_t* i2cDevice, uint32_t bitRate);

//Egyetlen byte kuldese. A rutin bevarja, mig a byte kimegy.
void MySPIM_sendByte(MySPIM_t* spim, uint8_t data);

//SPI-s adat transzfer, leirok alapjan.
//A leiroknak a muvelet vegeig a memoriaban kell maradnia!
void MySPIM_transfer(MySPIM_Device_t *spiDevice,
                     const MySPIM_xfer_t* transferBlocks,
                     uint32_t transferBlockCount);

//Az SPI interfacehez tartozo sercom interrupt service rutinokbol hivando.
//Parameterkent at kell adni a kezelt interface leirojat.
void MySPIM_service(MySPIM_t* spim);
//------------------------------------------------------------------------------
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
#define MYSPIM_PASTER( a, b )       a ## b
#define MYSPIM_EVALUATOR( a, b )    MYSPIM_PASTER(a, b)
#define MYSPIM_H1(k, ...)           _##k##_Handler(void)\
                                    {MySPIM_service(  __VA_ARGS__ );}
#define MYSPIM_H2(n)                MYSPIM_EVALUATOR(SERCOM, n)
#define MYSPIM_SERCOM_HANDLER(n, k, ...)   \
            void MYSPIM_EVALUATOR(MYSPIM_H2(n), MYSPIM_H1(k, __VA_ARGS__))

//Seged makro, mellyel egyszeruen definialhato az SPI-hez hasznalando interrupt
//rutinok.
//Ez kerul a kodba peldaul:
//  void SERCOMn_0_Handler(void){ MYSPIM_service( xxxx ); }
//  void SERCOMn_1_Handler(void){ MYSPIM_service( xxxx ); }
//  void SERCOMn_2_Handler(void){ MYSPIM_service( xxxx ); }
//  void SERCOMn_3_Handler(void){ MYSPIM_service( xxxx ); }
//             ^n                                 ^...
//n: A sercom sorszama (0..7)
//xxxx: A MYSPIM_t* tipusu leiro
#define MYSPIM_HANDLERS( n, ...) \
        MYSPIM_SERCOM_HANDLER(n, 0, __VA_ARGS__) \
        MYSPIM_SERCOM_HANDLER(n, 1, __VA_ARGS__) \
        MYSPIM_SERCOM_HANDLER(n, 2, __VA_ARGS__) \
        MYSPIM_SERCOM_HANDLER(n, 3, __VA_ARGS__)

//------------------------------------------------------------------------------
#endif //MYSPI_H_
