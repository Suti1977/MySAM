//------------------------------------------------------------------------------
//  I2C kezelest segito wrapper. FreeRTOS-hez illeszkedik.
//
//    File: MyI2CM.h
//------------------------------------------------------------------------------
#ifndef MY_I2CM_H_
#define MY_I2CM_H_



#include "MySercom.h"
//------------------------------------------------------------------------------
//I2C periferia konfiguracios parameterei, melyet az MyI2CM_Init() fuggvenynek
//adunk at initkor.
typedef struct
{
    //Az I2C interfacet biztosito Sercom beallitasaira mutat
    MySercom_Config_t  sercomCfg;

    //A (kezdeti) adatatvitei sebesseghez tartozo BAUD ertek.
    //Ez fugg a Sercomhoz rendelt GCLK altal eloallitott orajel frekitol.
    //Kiszamitasa a MYI2CM_CALC_BAUDVALUE() makro segitsegevel egyszerubb.
    uint32_t    baudValue;

    //Konfiguracios attributumok (bitmaszk mezo)
    //Az ertelmezett attributumok definicioit lasd lejebb, a "MYI2CM_ATTR_"
    //definicioknal
    uint32_t    attribs;

} MyI2CM_Config_t;
//------------------------------------------------------------------------------
//Seged makro, az I2C periferia BAUDLOW ertekenek kiszamitasahoz.
//  (ASF-bol vett megoldas)
//
//                  gclk_freq - (i2c_scl_freq * 10) - (gclk_freq * i2c_scl_freq * Trise)
// BAUD + BAUDLOW = --------------------------------------------------------------------
//                  i2c_scl_freq
//
// BAUD:    register value low  [7:0]
// BAUDLOW: register value high [15:8], only used for odd BAUD + BAUDLOW
//
//
// gclkfreq: A sercom Core orajelehez rendelt GCLK modul kimeneti frekvenciaja
// baud:     Az I2C busz sebesseg
// trise:    ride time ido [ns-ban]. 215-300 us kozott lehet!

#define MYI2CM_CALC_BAUDLOW(gclkfreq, baud, trise)                           \
                            (((gclkfreq - (baud * 10U)                       \
                               - (trise * (baud / 100U) * (gclkfreq / 10000U)\
                                  / 1000U))                                  \
                                  * 10U                                      \
                                  + 5U)                                      \
                                  / (baud * 10U))

#define MYI2CM_CALC_BAUDVALUE(gclkfreq, baud, trise)                        \
        ((  MYI2CM_CALC_BAUDLOW(gclkfreq, baud, trise) & 0x1)               \
         ? (MYI2CM_CALC_BAUDLOW(gclkfreq, baud, trise) / 2) +               \
          ((MYI2CM_CALC_BAUDLOW(gclkfreq, baud, trise) / 2 + 1) << 8)       \
         : (MYI2CM_CALC_BAUDLOW(gclkfreq, baud, trise) / 2))


//------------------------------------------------------------------------------
//I2C master modot befolyasolo konfiguracios attributumok, melyeket a periferia
//konfiguraciojanal hasznalunk az Attribs mezo feltoltesere.
//Az egyes attributumok bitmaszkot kepeznek, igy azokat vagy kapcsolattal
//hasznalhatjuk.
//A SAM mikrovezerlok I2C master periferiajanak a CTRLA regiszter bitjeinek
//a definicioi kerulnek felhasznalasra, igy a config beallitasa egyszerusodik,
//mert csak az osszerakott attributum mezot kell a CTRLA regiszterbe irni.
//Mivel a regiszternek vannak nem hasznalt bitjei, igy azokat a tobbi konfi-
//guracios attributumhoz hasznaljuk, termeszetesen a CTRLA regiszterebe azok
//beirasat egy maszkolassal tiltjuk.
//FONTOS: Ha CTRLA regisztert erinto beallitas valtozik vagy kerul be, akkor a
//        MYI2CM_CTRLA_CONFIG_MASK definiciot utana kell allitani!

//SCL low timeout engedelyezese (Lasd: CTRLA.LOWTOT)
#define MYI2CM_ATTR_LOWTOUT            SERCOM_I2CM_CTRLA_LOWTOUTEN

//Inactive Time-Out  (Lasd: CTRLA.INACTOUT)
//If the inactive bus time-out is enabled and the bus is inactive for longer
//than the time-out setting, the bus state logic will be set to idle.
//An inactive bus arise when either an I2C master or slave is holding the SCL
//low.
//Enabling i2cm option is necessary for SMBus compatibility, but can also be
//used in a non-SMBus set-up.
//Calculated time-out periods are based on a 100kHz baud rate.
#define MYI2CM_ATTR_INACTOUT_DIS       SERCOM_I2CM_CTRLA_INACTOUT(0)
#define MYI2CM_ATTR_INACTOUT_55US      SERCOM_I2CM_CTRLA_INACTOUT(1)
#define MYI2CM_ATTR_INACTOUT_105US     SERCOM_I2CM_CTRLA_INACTOUT(2)
#define MYI2CM_ATTR_INACTOUT_205US     SERCOM_I2CM_CTRLA_INACTOUT(3)

//Busz sebesseg beallitasa (Lasd: CTRLA.SPEED)
// 100 kHz-ig, vagy FastMode esete 400 kHz-ig
#define MYI2CM_ATTR_STANDARD_MODE      SERCOM_I2CM_CTRLA_SPEED(0)
// Fast-mode Plus (Fm+) max 1 MHz
#define MYI2CM_ATTR_FAST_PLUS_MODE     SERCOM_I2CM_CTRLA_SPEED(1)
//High-speed mode (Hs-mode) max 3.4 MHz
// High-speed mode jelenleg nem tamogatott a driverben!
//#define MYI2CM_ATTR_HIG_SPEED_MODE      SERCOM_I2CM_CTRLA_SPEED(2)

//Slave SCL Low Extended Timeout engedelyezese (Lasd: CTRLA.SEXTOEN)
//i2cm bit enables the slave SCL low extend time-out. If SCL is cumulatively
//held low for greater than 25ms from the initial START to a STOP, the slave
//will release its clock hold if enabled and reset the internal
//state machine.Any interrupt flags set at the time of time-out will remain set.
//If the address was recognized, PREC will be set when a STOP is received.
#define MYI2CM_ATTR_SEXTOEN            SERCOM_I2CM_CTRLA_SEXTTOEN

//Busz sebesseg beallitasa (Lasd: CTRLA.SDAHOLD)
//These bits define the SDA hold time with respect to the negative edge of SCL.
#define MYI2CM_ATTR_SDAHOLD_DIS        SERCOM_I2CM_CTRLA_SDAHOLD(0)
#define MYI2CM_ATTR_SDAHOLD_75NS       SERCOM_I2CM_CTRLA_SDAHOLD(1)
#define MYI2CM_ATTR_SDAHOLD_450NS      SERCOM_I2CM_CTRLA_SDAHOLD(2)
#define MYI2CM_ATTR_SDAHOLD_600NS      SERCOM_I2CM_CTRLA_SDAHOLD(3)

//Standby modban valo mukodes engedelyezese (Lasd: CTRLA.RUNSTDBY)
#define MYI2CM_ATTR_RUNSTDBY           SERCOM_I2CM_CTRLA_RUNSTDBY
//------------------------------------------------------------------------------
//CTRLA-ba valo beirashoz a konfigracios attributumok maszkja.
#define MYI2CM_CTRLA_CONFIG_MASK      (SERCOM_I2CM_CTRLA_RUNSTDBY          | \
                                       SERCOM_I2CM_CTRLA_SDAHOLD_Msk       | \
                                       SERCOM_I2CM_CTRLA_SEXTTOEN          | \
                                       SERCOM_I2CM_CTRLA_SPEED_Msk         | \
                                       SERCOM_I2CM_CTRLA_INACTOUT_Msk      | \
                                       SERCOM_I2CM_CTRLA_LOWTOUTEN)
//------------------------------------------------------------------------------
//Adatblokk atvitel iranyai
typedef enum
{
    //Adat kiiras a buszon
    MYI2CM_DIR_TX=0,
    //Adat olvasas a buszrol. (A logika kihasznalja, hogy az RX=1!!!)
    MYI2CM_DIR_RX=1,
} MyI2CM_dir_t;

//I2C- buszon torteno adatblokk tranzakcios leiro. Ilyenekbol tobb felsorolhato,
//melyet a driver egymas utan hajt vegre. Egy-egy ilyen leiro adat irast, vagy
//olvasast is eloirhat.
typedef struct
{
    //A blokk iranya
    MyI2CM_dir_t   dir;
    //Az adatok helyere/celteruletere mutat
    uint8_t*       buffer;
    //A blokkban mozgatni kivant adatbyteok szama
    uint32_t       length;
} MyI2CM_xfer_t;
//------------------------------------------------------------------------------
//MyI2CM valtozoi
typedef struct
{
    //Az I2C interface alapszintu sercom driver-enek leiroja
    MySercom_t   sercom;

    #ifdef USE_FREERTOS
    //A taszkok kozotti busz hozzaferest kizaro mutex.
    SemaphoreHandle_t   busMutex;
    //Az intarrupt alat futo folyamatok vegere varo szemafor
    SemaphoreHandle_t   semaphore;
    #endif //USE_FREERTOS

    //A kovetkezo vegrehajtando adatatatviteli leirora mutat
    const MyI2CM_xfer_t* nextBlock;
    //Ennyi blokk van meg hatra amit vegre kell hajtani
    uint32_t leftBlockCnt;
    //A kommunikaltatott I2C eszkoz slave cime
    uint8_t  slaveAddress;

    //A blokkon beluli adatbyteokat cimzi
    uint8_t*    dataPtr;
    //A hatralevo adatbyteok szama
    uint32_t    leftByteCnt;

    //Az aktualisan kuldes alatt allo adatblokk iranyat orzi.
    //Ezt hasznalja arra, hogy tudja, hogy TX-bol RX-be valtunk, vagy forditva,
    //es ezert restartot kell inditani.
    MyI2CM_dir_t    transferDir;

    //true, ha egy eszkozon be kell varni, amig vegez, es csak utana szabad
    //lezarni a buszt a STOP kondicioval.
    //Ez ott eredekes, ahol peldaul egy eszkoz NACK-val jelzi, ha egy muvelet
    //miatt foglalt, es azt be kell varnunk, meg ugyan abban az I2C keretben.
    bool waitingWhileBusy;
    //true, ha a foglaltsagra tesztelesi allapotban vagyunk
    bool busyTest;

    //Az I2C folyamatok alatt beallitott statusz, melyet az applikacio fele
    //visszaad.
    int     asyncStatus;
} MyI2CM_t;
//------------------------------------------------------------------------------
//Statusz kodok
enum
{
    kMyI2CMStatus_= MAKE_STATUS(kStatusGroup_MyI2CM, 0),
    //A slave nem ACK-zott
    kMyI2CMStatus_NACK,
    //Hiba a buszon. Peldaul GND-re huzza valami.
    kMyI2CMStatus_BusError,
    //Arbitacio elvesztese
    kMyI2CMStatus_ArbitationLost,
};
//------------------------------------------------------------------------------
//  Altalanos I2C buszra kotott eszkozok elerese
//  Egy-egy ilyen strukturaban tarolodnak azok az informaciok, melyek egy eszkoz
//  eleresehez szuksegesek. Ilyen peldaul, hogy melyik I2C buszon talalhato,
//  illetve, hogy mi az I2C cime...
typedef struct
{
    //A hozza tartozo I2C driver handlerere mutat
    MyI2CM_t*   i2c;
    //Az eszkoz slave cime a buszon 7 bites
    uint8_t     slaveAddress;
    //Az eszkozhoz tartozo driver valtozoira mutat. Ezt minden driver eseten
    //sajat tipusanak megfeleloen kell kasztolni.
    void*       handler;
} MyI2CM_Device_t;
//------------------------------------------------------------------------------
//I2C master driver inicializalasa
void MyI2CM_init(MyI2CM_t* i2cm, const MyI2CM_Config_t* config);

//Az I2C interfacehez tartozo sercom interrupt service rutinokbol hivando.
//Parameterkent at kell adni a kezelt interface leirojat.
void MyI2CM_service(MyI2CM_t* i2cm);

//I2C eszkoz letrehozasa
//i2c_device: A letrehozando eszkoz leiroja
//i2c: Annak a busznak az I2CM driverenek handlere, melyre az eszkoz csatlakozik
//slave_address: Eszkoz I2C slave cime a buszon
//handler: Az I2C-s eszkozhoz tartozo driver handlere
void MyI2CM_CreateDevice(MyI2CM_Device_t* i2cDevice,
                         MyI2CM_t* i2c,
                         uint8_t slaveAddress,
                         void* handler);

//Atviteli leirok listaja alapjan mukodes inditasa
status_t MYI2CM_transfer(MyI2CM_Device_t* i2cDevice,
                        const MyI2CM_xfer_t* xferBlocks, uint32_t blockCount);

//Eszkoz elerhetoseg tesztelese. Nincs adattartalom.
status_t MyI2CM_ackTest(MyI2CM_Device_t* i2cDevice);

//I2C iras/olvasas rutin.
//Blokkolos mukodes. Megvarja amig a transzfer befejezodik.
//FIGYELEM!  a bemenetnek megadott buffereknek permanensen a memoriaban kell
//          maradni, amig az I2C folyamat le nem zarul, mivel a rutin nem keszit
//          masolatot a bemeno adatokrol.
//i2cDevice: I2C eszkoz leiroja
//txData1: Elso korben kiirando adatokra mutat
//txLength1: Az elso korben kiirando adatbyteok szama
//txData2: Masodik korben kiirando adatokra mutat
//txLength2: A masodik korben kiirando adatbyteok szama
//rxData:  Ide pakolja a buszrol olvasott adatbyteokat
//         (Az olvasaskor erre a memoriateruletre pakolja a vett byteokat.)
//rxLength: Ennyi byteot olvas be.
//Visszaterse: hibakoddal, ha van.

//Ha TxLength valtozok valamelyike 0, akkor azt kihagyja.
//Ha RxLength nulla, akkor nem hajt vegre olvasast.
//     Ha nem nulla, akkor az irasok utan restartot general, es utanna olvas.
#if 0
status_t MyI2CM_writeRead(  MyI2CM_Device_t* i2cDevice,
                            uint8_t *txData1, uint32_t txLength1,
                            uint8_t *txData2, uint32_t txLength2,
                            uint8_t *rxData,  uint32_t rxLength);
#endif


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
#define MYI2CM_PASTER( a, b )       a ## b
#define MYI2CM_EVALUATOR( a, b )    MYI2CM_PASTER(a, b)
#define MYI2CM_H1(k, ...)           _##k##_Handler(void)\
                                    {MyI2CM_service(  __VA_ARGS__ );}
#define MYI2CM_H2(n)                MYI2CM_EVALUATOR(SERCOM, n)
#define MYI2CM_SERCOM_HANDLER(n, k, ...)   \
            void MYI2CM_EVALUATOR(MYI2CM_H2(n), MYI2CM_H1(k, __VA_ARGS__))

//Seged makro, mellyel egyszeruen definialhato az I2C-hez hasznalando interrupt
//rutinok.
//Ez kerul a kodba peldaul:
//  void SERCOMn_0_Handler(void){ MyI2CM_service( xxxx ); }
//  void SERCOMn_1_Handler(void){ MyI2CM_service( xxxx ); }
//  void SERCOMn_2_Handler(void){ MyI2CM_service( xxxx ); }
//  void SERCOMn_3_Handler(void){ MyI2CM_service( xxxx ); }
//             ^n                                 ^...
//n: A sercom sorszama (0..7)
//xxxx: A MYI2CM_t* tipusu leiro
#define MYI2CM_HANDLERS( n, ...) \
        MYI2CM_SERCOM_HANDLER(n, 0, __VA_ARGS__) \
        MYI2CM_SERCOM_HANDLER(n, 1, __VA_ARGS__) \
        MYI2CM_SERCOM_HANDLER(n, 2, __VA_ARGS__) \
        MYI2CM_SERCOM_HANDLER(n, 3, __VA_ARGS__)
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_I2CM_H_
