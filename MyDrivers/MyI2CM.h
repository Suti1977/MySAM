//------------------------------------------------------------------------------
//  I2C Master driver
//
//    File: MyI2CM.h
//------------------------------------------------------------------------------
#ifndef MY_I2CM_H_
#define MY_I2CM_H_



#include "MySercom.h"

struct MyI2CM_t_;
//------------------------------------------------------------------------------
//I2C driver hibaja eseten hivott callback funkcio definicioja
typedef void MyI2CM_errorFunc_t(status_t errorCode, void* callbackdata);
//I2C busz hiba eseten hivott callback, melyben a beragadt busz feloldasa
//megoldhato.
typedef void MyI2CM_busErrorResolverFunc_t(struct MyI2CM_t_* i2cm,
                                           void* callbackdata);
//------------------------------------------------------------------------------
//I2C periferia konfiguracios parameterei, melyet az MyI2CM_Init() fuggvenynek
//adunk at initkor.
typedef struct
{
    //Busz frekvencia/sebesseg
    uint32_t bitRate;

    //Konfiguracios attributumok (bitmaszk mezo)
    //Az ertelmezett attributumok definicioit lasd lejebb, a "MYI2CM_ATTR_"
    //definicioknal
    uint32_t    attribs;

    //A periferia IRQ prioritasa
    uint32_t    irqPriorities;

} MyI2CM_Config_t;
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
//Adatblokk atvitel iranyai, egyeb flagek
typedef enum
{
    //Adat kiiras a buszon
    MYI2CM_FLAG_TX=0,
    //Adat olvasas a buszrol. (A logika kihasznalja, hogy az RX=1!!!)
    MYI2CM_FLAG_RX=1,
    //10 bites cimzes eloirasa
    MYI2CM_FLAG_TENBIT=2,
} MyI2CM_flag_t;


//I2C- buszon torteno adatblokk tranzakcios leiro. Ilyenekbol tobb felsorolhato,
//melyet a driver egymas utan hajt vegre. Egy-egy ilyen leiro adat irast, vagy
//olvasast is eloirhat.
typedef struct
{
    //A blokk iranya, es egyeb flagek
    MyI2CM_flag_t  flags;
    //Az adatok helyere/celteruletere mutat
    uint8_t*       buffer;
    //A blokkban mozgatni kivant adatbyteok szama
    uint32_t       length;
} MyI2CM_xfer_t;
#define MYI2CM_DIR_MASK     1
//------------------------------------------------------------------------------
//MyI2CM valtozoi
typedef struct MyI2CM_t_
{
    //A driverhez tartozo konfiguraciora mutat
    const MyI2CM_Config_t* config;

    //Az I2C interface alapszintu sercom driver-enek leiroja
    MySercom_t   sercom;

    #ifdef USE_FREERTOS
    //A taszkok kozotti busz hozzaferest kizaro mutex.
    SemaphoreHandle_t   busMutex;
    //Az intarrupt alat futo folyamatok vegere varo szemafor
    SemaphoreHandle_t   semaphore;
    #endif //USE_FREERTOS


    //Az aktualisan vegrehajtott transzfer leirora mutat
    const MyI2CM_xfer_t* actualBlock;

    //Ennyi blokk van meg hatra amit vegre kell hajtani
    uint32_t leftBlockCnt;
    //A kommunikaltatott I2C eszkoz slave cime.
    uint32_t  slaveAddress;

    //A blokkon beluli adatbyteokat cimzi
    uint8_t*    dataPtr;
    //A hatralevo adatbyteok szama
    uint32_t    leftByteCnt;

    //Az I2C folyamatok alatt beallitott statusz, melyet az applikacio fele
    //visszaad.
    int     asyncStatus;

    //A megszakitasban kiolvasott STATUS regiszter erteke.
    uint32_t statusRegValue;

    //A busz sebessege
    uint32_t    bitRate;

    //Hiba eseten hivodo callback. (ha be van regisztralva.)
    MyI2CM_errorFunc_t* errorFunc;
    //A hiba callback szamara atadott tetszoleges valtozo.
    void* errorFuncCallbackdata;

    //I2C busz hiba eseten hivott callback, melyben a beragadt busz feloldasa
    //megoldhato.
    MyI2CM_busErrorResolverFunc_t* busErrorResolverFunc;
    void* busErrorResolverFuncCallbackData;

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
    //Low timeout
    kMyI2CMStatus_LowTimeout,
    //periferia LEN error
    kMyI2CMStatus_LenError,
    //SCL Low Timeout
    kMyI2CMStatus_SclLowTimeout,
    //Master SCL Low Extend Timeout
    kMyI2CMStatus_MasterSclExtendTimeout,
    //Slave SCL Low Extend Timeout
    kMyI2CMStatus_SlaveSclLowExtendTimeout,
    //Hibas leiro
    kMyI2CMStatus_InvalidTransferDescriptor,
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
//I2C master driver letrehozasa es konfiguralasa.
//Fontos! A config altal mutatott konfiguracionak permanensen a memoriaban
//kell maradnia!
void MyI2CM_create(MyI2CM_t* i2cm,
                   const MyI2CM_Config_t* config,
                   const MySercom_Config_t* sercomCfg);

//I2C driver es eroforrasok felaszabditasa
void MyI2CM_destory(MyI2CM_t* i2cm);

//I2C Periferia inicializalasa/engedelyezese
void MyI2CM_init(MyI2CM_t* i2cm);
//I2C Periferia tiltasa. HW eroforrasok tiltasa.
void MyI2CM_deinit(MyI2CM_t* i2cm);

//I2C periferia resetelese
void MyI2CM_reset(MyI2CM_t* i2cm);

//Az I2C interfacehez tartozo sercom interrupt service rutinokbol hivando.
//Parameterkent at kell adni a kezelt interface leirojat.
void MyI2CM_service(MyI2CM_t* i2cm);

//Hiba eseten meghivodo callback beregisztralasa
void MyI2CM_registerErrorFunc(MyI2CM_t* i2cm,
                              MyI2CM_errorFunc_t* errorFunc,
                              void* errorFuncCallbackdata);

//Busz hiba eseten meghivodo callback beregisztralasa, melyben a beragadt busz
//feloldhato
void MyI2CM_registerBusErrorResolverFunc(MyI2CM_t* i2cm,
                              MyI2CM_busErrorResolverFunc_t* func,
                              void* funcCallbackdata);

//I2C eszkoz letrehozasa
//i2c_device: A letrehozando eszkoz leiroja
//i2c: Annak a busznak az I2CM driverenek handlere, melyre az eszkoz csatlakozik
//slave_address: Eszkoz I2C slave cime a buszon
//handler: Az I2C-s eszkozhoz tartozo driver handlere
void MyI2CM_createDevice(MyI2CM_Device_t* i2cDevice,
                         MyI2CM_t* i2c,
                         uint8_t slaveAddress,
                         void* handler);

//Atviteli leirok listaja alapjan atvitel vegrehajtasa
status_t MYI2CM_transfer(MyI2CM_Device_t* i2cDevice,
                        const MyI2CM_xfer_t* xferBlocks, uint32_t blockCount);

//Eszkoz elerhetoseg tesztelese. Nincs adattartalom.
status_t MyI2CM_ackTest(MyI2CM_Device_t* i2cDevice);

//I2C busz sebesseg modositasa/beallitasa
status_t MyI2CM_setBitRate(MyI2CM_t* i2cDevice, uint32_t bitRate);


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
