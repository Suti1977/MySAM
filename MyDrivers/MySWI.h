//------------------------------------------------------------------------------
//  Single-Wire (SWI) driver
//  A rendszerben tobb Wire is lehet. Az eroforrasok optimalizalasa erdekeben a
//  Wire-okhoz egyetlen kozos kezelo modul tartozik, kozos hardver peri-
//  feriakkal. Az egyes Wireo-ok kezelése viszont egy idoben nem lehetseges.
//  Ezt a kizarast mutexelessel oldjuk meg.
//
//    File: MySWI.h
//------------------------------------------------------------------------------
#ifndef MY_SWI_H_
#define MY_SWI_H_

#include "MyCommon.h"
#include "MyTC.h"

struct MySWI_Driver_t;
//------------------------------------------------------------------------------
//MySWI driver status/hibakodok
enum
{
    kMySWI_Status_= MAKE_STATUS(kStatusGroup_MySWI, 0),
    //A slave nem ACK-zott
    kMySWI_Status_NACK,
    //A felderiteskor (Discovery) nem kapott visszajelzest.
    kMySWI_Status_discoveryNACK,
};
//------------------------------------------------------------------------------
//          MySWI idozitesi konstansok
//High Speed mode az alapertelmezett mindegyik eszkoznnel. Standard speed modba
//ugy lehetne atkapcsolni. --> HS modban erjuk el az eszkozoket. Az idoziteseket
//HS modhoz hatarozzuk meg.

//Bit olvasasanal ennyi idore huzza le a vonalat, mielott elengedne [us]
#define MySWI_READBIT_DRIVE_LOW_TIME          0.1
//Bit olvasasa ebben az idopontban tortenik meg a vonal elengedese utan [us]
#define MySWI_READBIT_SAMPLING_TIME           1
//A mintavetel utan ennyit var, mielott az uj bit olvasasaba kezdene [us]
#define MySWI_READBIT_HIGH_TIME               20
//Bit irasnal az 1-hez tartozo alacsony es magas szint idok [us]
#define MySWI_WRITEBIT_1_LOW_TIME             1
#define MySWI_WRITEBIT_1_HIGH_TIME            19
//Bit irasnal a 0-hoz tartozo alacsony es magas szint idok [us]
#define MySWI_WRITEBIT_0_LOW_TIME             10
#define MySWI_WRITEBIT_0_HIGH_TIME            10
//start/stop feltetelhez tartozo ido [us]
#define MySWI_START_STOP_TIME                 200
//Reset feltetelnel alacsonyba huzas, majd magasban tartas ideje [us]
#define MySWI_RESET_LOW_TIME                  500
#define MySWI_RESET_HIGH_TIME                 200
//------------------------------------------------------------------------------
//Vonalat alacsony szintbe huzo callback fuggveny
typedef void MySWI_driveWireFunc_t(void);
//Vonalat magas szintre engedo callback fuggveny. (Vonal meghajtas tiltasa)
typedef void MySWI_releaseWireFunc_t(void);
//Vonal allapotanak lekerdezese. true-t ad vissza, ha a vonal magasban van.
typedef bool MySWI_readWireFunc_t(void);
//------------------------------------------------------------------------------
//MySWI egyetlen Wire-hoz tartozo konfiguracios struktura.
typedef struct
{
    //Vonalat alacsony szintbe huzo callback fuggveny
    MySWI_driveWireFunc_t*     driveWireFunc;
    //Vonalat magas szintre engedo callback fuggveny. (Vonal meghajtas tiltasa)
    MySWI_releaseWireFunc_t*   releaseWireFunc;
    //Vonal allapotanak lekerdezese. true-t ad vissza, ha a vonal magasban van.
    MySWI_readWireFunc_t*      readWireFunc;
} MySWI_WireConfig_t;
//------------------------------------------------------------------------------
//Egyetlen Single-Wire vezetekhez tartozo handler valtozoi
typedef struct
{
    //Konfiguraciokor megadott parameterek
    MySWI_WireConfig_t         cfg;
    //A wire-hoz rendelt driverre mutat. (Create-kor beallitva.)
    struct MySWI_Driver_t*     driver;
} MySWI_Wire_t;
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//Elore kalkulalt idozitesi ertekek, melyek a TC modul CC regiszterebe irunk.
typedef struct
{
    //Bit olvasasanal ennyi idore huzza le a vonalat, mielott elengedne
    uint16_t readBit_lowTime;
    //Bit olvasasa ebben az idopontban tortenik meg a vonal elengedese utan
    uint16_t readBit_samplingTime;
    //A mintavetel utan ennyit var magasban, az ujabb bit olvasasa elott
    uint16_t readBit_highTime;
    //Bit irasnal az 1-hez tartozo alacsony es magas szint idok
    uint16_t writeBit_1_lowTime;
    uint16_t writeBit_1_highTime;
    //Bit irasnal a 0-hoz tartozo alacsony es magas szint idok
    uint16_t writeBit_0_lowTime;
    uint16_t writeBit_0_highTime;
    //Start es stop felteteleknel hasznalt kitartas ideje
    uint16_t startStopTime;
    //Reset feltetel magas es alacsony idozitesei
    uint16_t reset_lowTime;
    uint16_t reset_highTime;

    //Megj: ha a lista bovitesre kerul, ugy a MySWI_TIME_TABLE makrot is
    //      boviteni kell!
} MySWI_Times_t;
//------------------------------------------------------------------------------
//A latenciak miatt bevezetett korrekcios tenyezo, mely segit pontositani az
//idoziteseket. (Az volt a tapasztalat, hogy amig feljott az interrupt, addig
//nagyon sok idot kepes volt elvacakolni az MCU, igy kb 48MHz esetén 1uS-el
//hoszabbak voltak az idozitesek.)
#ifndef MySWI_TIMING_CORRECTION
#define MySWI_TIMING_CORRECTION    (40)
#endif
//A TC/t hajto GCLK frekvencia, es a kivant idozitesbol kiszamolja a CC
//regiszterbe irando erteket.
//f: a TC-t hajto orajel frekvenciaja (Hz)
//f: a kivant idozites (us)
#define MySWI_CALC_CC(f, t) (uint16_t) \
    (( (double)t / (1000000.0 / (double)f) )- MySWI_TIMING_CORRECTION)

//Seged makro, mely egy konstans MySWI_Times_t tipusu struktura elemeit
//kepes generalni. Segitsegevel forditasi idoben meghatarozhatok az idozitesi
//konstansok, a driverhez rendelt TC periferiat hajto GCLK orajel freki ismere-
//teben.
//A makrot egy strukturaba kell irni.
//f: a TC-t hajto orajel frekvenciaja (Hz)
#define MySWI_TIME_TABLE(f)                                              \
    .readBit_lowTime     =MySWI_CALC_CC(f, MySWI_READBIT_DRIVE_LOW_TIME),\
    .readBit_samplingTime=MySWI_CALC_CC(f, MySWI_READBIT_SAMPLING_TIME), \
    .readBit_highTime    =MySWI_CALC_CC(f, MySWI_READBIT_HIGH_TIME),     \
    .writeBit_1_lowTime  =MySWI_CALC_CC(f, MySWI_WRITEBIT_1_LOW_TIME),   \
    .writeBit_1_highTime =MySWI_CALC_CC(f, MySWI_WRITEBIT_1_HIGH_TIME),  \
    .writeBit_0_lowTime  =MySWI_CALC_CC(f, MySWI_WRITEBIT_0_LOW_TIME),   \
    .writeBit_0_highTime =MySWI_CALC_CC(f, MySWI_WRITEBIT_0_HIGH_TIME),  \
    .startStopTime       =MySWI_CALC_CC(f, MySWI_START_STOP_TIME),       \
    .reset_lowTime       =MySWI_CALC_CC(f, MySWI_RESET_LOW_TIME),        \
    .reset_highTime      =MySWI_CALC_CC(f, MySWI_RESET_HIGH_TIME)

//------------------------------------------------------------------------------
//MySWI driver konfiguracios struktura.
typedef struct
{
    //A driverhez rendelt TC periferia konfiguracioja
    MyTC_Config_t           tcConfig;
    //Elore kalkulalt mukodesi idozitesi konstansok.
    MySWI_Times_t      times;
} MySWI_DriverConfig_t;
//------------------------------------------------------------------------------
//A megszakitasban hivott, a megszakitas bekovetkezesekor vegrehajtando allapot
//callback fuggvenyenek felepitese.
typedef void MySWI_stateFunc_t(struct MySWI_Driver_t* driver);
//------------------------------------------------------------------------------
//MySWI driver handlere
typedef struct
{
    //A modulhoz rendelt TC driver valtozoi
    MyTC_t          tc;

    //A TC-t hajto orajel frekvencia fuggvenyeben elore kalkulalt idozitesi
    //parameterek.
    MySWI_Times_t times;

    //Az aktualisan kezelt wire parametereinek masolata. Ezt azert csinaljuk,
    //hogy a leheto leggyorsabban elerhessuk megszakitasban az aktualis Wire-re
    //vonatkozo informaciokat.    
    MySWI_WireConfig_t wireConfig;

    //a vonalrol olvasott adatbiteket ebben allitja ossze.
    //ACK vagy Discovery eseten is hasznaljuk. Ilyenkor az also bit hordozza a
    //vonalrol beolvasott allapotot.
    uint8_t rxShiftReg;
    //a vonalra irando adatbiteket ebbol lepteti ki
    uint8_t txShiftReg;

    //a beolvasando/kiirando biteket szamolja. 0-ig csokken
    uint8_t bitCnt;

    //olvasasnal az utolso byteot jelzi. Ilyenkor NACK-t kuld a slave fele.
    uint8_t lastByte;


    //A soron kovetkezo kiirando byteot cimzi.
    uint8_t* txPtr;
    //a hatralevo byteokat szamolja
    uint16_t txCnt;
    //Masodik korben kiirando byteokat cimzi
    uint8_t* txPtr2;
    //Masodik korben kiirando byteok szama
    uint16_t txCnt2;
    //ha ez 1, akkor restrat feltetlet kell beiktatni az elso kikuldott
    //adathalmaz utan. Ez van peldaul memoriak random-read modja eseten.
    uint8_t insertRestart;

    //beolvasando byteok celteruletet cimci
    uint8_t* rxPtr;
    //beolvasoando byteokat szamolja. 0-ig csokken.
    uint16_t rxCnt;

    //A megszakitaskor hivando, az aktualis allapotnak megfelelo kiszolgalo
    //fuggvenyre mutat.
    //Ez lehet a vonalon torteno alacsonyszintu allapot: MySWI_wState_xxx
    //De lehet a magasabb szintu allapotget fuggvenye is: MySWI_mState_xxx
    MySWI_stateFunc_t* stateFunc;
    //Alacsony szintu (wire) muveletek utan a fo allapotgep azon fuggvenyere
    //mutat, ahova at kell adni a vezerlest.
    MySWI_stateFunc_t* returnFunc;

    #ifdef USE_FREERTOS
    //Az egyes Wire-ok elerese kozotti kizarast biztosito mutex
    SemaphoreHandle_t mutex;

    //megszakitasban vegrehajtott folyamat vegetvaro szemafor
    SemaphoreHandle_t doneSemaphore;
    #endif //USE_FREERTOS

    //A megszakitasban keletkezo hibakodot orzi meg. Ez kerul visszaadasra
    //a birtoklo taszk szamara.
    status_t asyncStatus;
} MySWI_Driver_t;
//------------------------------------------------------------------------------
//Tranzakcios leiro.
//A leirot a MySWI_transaction() fuffvenynek kell atadni.
typedef struct
{
    //Az elso kuldendo adatcsomag (opcode, mem address, ...) Kotelezo!
    uint8_t*    tx1Buff;
    //Az elso kuldendo adatcsomag hossza. Kotelezo!
    uint16_t    tx1Length;

    //A masodik kuldendo adatcsomag (adatok, vagy random read eseten opcode)
    uint8_t*    tx2Buff;
    //A masodik kuldendo adatcsomag hossza. Ha ilyen nincs, akkor ez 0.
    uint16_t    tx2Length;

    //ha ez 1, akkor restrat feltetlet kell beiktatni az elso kikuldott
    //adathalmaz utan. Ez van peldaul memoriak random-read modja eseten.
    uint8_t     insertRestart;

    //olvasando adatblokk cel buffere
    uint8_t*    rxBuff;
    //Az olvasando byteok szama. Ha nincs olvasas, akkor ez 0.
    uint16_t    rxLength;
} MySWI_transaction_t;
//------------------------------------------------------------------------------
//Single-Wire (SWI) driver kezdeti inicializalasa
void MySWI_driverInit(MySWI_Driver_t* this,
                           const MySWI_DriverConfig_t* cfg);

//Wire letrehozasa, a megadott konfiguracio alapjan
//A fuggveny hivasat meg kell, hoyg elozze a MySWI_driverInit() !
void MySWI_createWire(MySWI_Wire_t* wire,
                           MySWI_Driver_t* driver,
                           const MySWI_WireConfig_t* cfg);

//A hozza rendelt TC-hez tartozo interrupt service rutin
void MySWI_service(MySWI_Driver_t* driverv);

//Single-Wire (SWI) tranzakcio vegzese.
//A fuggveny blokkolja a hivo oldalt, mig a tranzakcioval nem vegez.
status_t MySWI_transaction(MySWI_Wire_t* wire,
                                const MySWI_transaction_t* transaction);
//------------------------------------------------------------------------------
//A TC-k definicioit undefine-olnom kellet, mert kulonben az alabbi makroban
//a "TC" osszeveszett a forditoval.
#undef TC0
#undef TC1
#undef TC2
#undef TC3
#undef TC4
#undef TC5
#undef TC6
#undef TC7
#undef TC8
#define MySWI_PASTER( a, b )       a ## b
#define MySWI_EVALUATOR( a, b )    MySWI_PASTER(a, b)
#define MySWI_H1(...)  _Handler(void) {MySWI_service(  __VA_ARGS__ );}
#define MySWI_H2(n)    MySWI_EVALUATOR(TC, n)
#define MySWI_TC_HANDLER(n, ...)   \
      void MySWI_EVALUATOR(MySWI_H2(n), MySWI_H1( __VA_ARGS__))

//Seged makro, mellyel egyszeruen definialhato az SWI-hez hasznalando TC
//interrupt rutinok.
//Ez kerul a kodba peldaul:
//  void TCn_Handler(void){ MySWI_service( xxxx ); }
//         ^n                              ^...
//n: A driverhez hasznalt TC modul  sorszama (0..4)
//xxxx: A MySWI_Driver_t* tipusu leiro
#define MySWI_HANDLERS( n, ...) MySWI_TC_HANDLER(n, __VA_ARGS__)
//------------------------------------------------------------------------------

#endif //MY_SWI_H_
