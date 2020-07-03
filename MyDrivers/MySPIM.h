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
    //Az SPI interfacet biztosito Sercom beallitasaira mutat
    MySercom_Config_t  sercomCfg;

    //A (kezdeti) adatatvitei sebesseghez tartozo BAUD ertek.
    //Ez fugg a Sercomhoz rendelt GCLK altal eloallitott orajel frekitol.
    //Kiszamitasa a MYSPIM_CALC_BAUDVALUE() makro segitsegevel egyszerubb.
    uint8_t    baudValue;

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
//Ezek fuggenek a CPOL alalpotatatol
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
#define MYSPIMM_CTRLA_CONFIG_MASK     (SERCOM_SPI_CTRLA_RUNSTDBY            | \
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

#define MYSPIM_CALC_BAUDVALUE(gclkfreq, baud) \
                            ((uint8_t)((gclkfreq / (2*baud))-1))
//------------------------------------------------------------------------------
//MySPIM driver valtozoi
typedef struct
{
    //Az SPI interface alapszintu sercom driver-enek leiroja
    MySercom_t   sercom;

  #ifdef USE_FREERTOS
    //A taszkok kozotti busz hozzaferest kizaro mutex.
    SemaphoreHandle_t   busMutex;
    //Az intarrupt alat futo folyamatok vegere varo szemafor
    SemaphoreHandle_t   semaphore;
  #endif //USE_FREERTOS


} MySPIM_t;
//------------------------------------------------------------------------------
//driver kezdeti inicializalasa
void MySPIM_init(MySPIM_t* spim, MySPIM_Config_t* config);

//SPI mukodes engedelyezese
void MySPIM_enable(MySPIM_t* spim);
//SPI mukodes tiltaasa
void MySPIM_disable(MySPIM_t* spim);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYSPI_H_
