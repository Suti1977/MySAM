//------------------------------------------------------------------------------
//  One-Wire (OWI) driver
//  A rendszerben tobb Wire is lehet. Az eroforrasok optimalizalasa erdekeben a
//  Wire-okhoz egyetlen kozos kezelo modul tartozik, kozos hardver peri-
//  feriakkal. Az egyes Wireo-ok kezelése viszont egy idoben nem lehetseges.
//  Ezt a kizarast mutexelessel oldjuk meg.
//
//    File: MyOWI.h
//------------------------------------------------------------------------------
#ifndef MY_OWI_H_
#define MY_OWI_H_

#include "MyCommon.h"
#include "MyTC.h"

struct MyOWI_Driver_t;
//------------------------------------------------------------------------------
//MyOWI driver status/hibakodok
enum
{
    kMyOWI_Status_= MAKE_STATUS(kStatusGroup_MyOWI, 0),
    //A slave nem ACK-zott
    //kMyOWI_Status_NACK,
    //A resetkor nem erzekelte a presence bitet
    kMyOWI_Status_noPresence,
};
//------------------------------------------------------------------------------
//          MyOWI idozitesi konstansok

//Bit olvasasanal ennyi idore huzza le a vonalat, mielott elengedne [us]
#define MyOWI_READBIT_DRIVE_LOW_TIME          0.1
//Bit olvasasa ebben az idopontban tortenik meg a vonal elengedese utan [us]
#define MyOWI_READBIT_SAMPLING_TIME           10
//A mintavetel utan ennyit var, mielott az uj bit olvasasaba kezdene [us]
#define MyOWI_READBIT_HIGH_TIME               70
//Bit irasnal az 1-hez tartozo alacsony es magas szint idok [us]
#define MyOWI_WRITEBIT_1_LOW_TIME             1
#define MyOWI_WRITEBIT_1_HIGH_TIME            74
//Bit irasnal a 0-hoz tartozo alacsony es magas szint idok [us]
#define MyOWI_WRITEBIT_0_LOW_TIME             70
#define MyOWI_WRITEBIT_0_HIGH_TIME            5

//Reset feltetelnel alacsonyba huzas, majd magasban tartas ideje [us]
#define MyOWI_RESET_LOW_TIME                  500
#define MyOWI_RESET_HIGH_TIME                 500
//A presence jelet enyni ido mulva olvassa be, miutan elengedte a reset utan
//a vonalat. [us]
#define MyOWI_PRESENCE_SAMPLING_TIME          100
//------------------------------------------------------------------------------
//Vonalat alacsony szintbe huzo callback fuggveny
typedef void MyOWI_driveWireFunc_t(void);
//Vonalat magas szintre engedo callback fuggveny. (Vonal meghajtas tiltasa)
typedef void MyOWI_releaseWireFunc_t(void);
//Vonal allapotanak lekerdezese. true-t ad vissza, ha a vonal magasban van.
typedef bool MyOWI_readWireFunc_t(void);
//------------------------------------------------------------------------------
//MyOWI egyetlen Wire-hoz tartozo konfiguracios struktura.
typedef struct
{
    //Vonalat alacsony szintbe huzo callback fuggveny
    MyOWI_driveWireFunc_t*     driveWireFunc;
    //Vonalat magas szintre engedo callback fuggveny. (Vonal meghajtas tiltasa)
    MyOWI_releaseWireFunc_t*   releaseWireFunc;
    //Vonal allapotanak lekerdezese. true-t ad vissza, ha a vonal magasban van.
    MyOWI_readWireFunc_t*      readWireFunc;
} MyOWI_WireConfig_t;
//------------------------------------------------------------------------------
//Egyetlen Single-Wire vezetekhez tartozo handler valtozoi
typedef struct
{
    //Konfiguraciokor megadott parameterek
    MyOWI_WireConfig_t         cfg;
    //A wire-hoz rendelt driverre mutat. (Create-kor beallitva.)
    struct MyOWI_Driver_t*     driver;
} MyOWI_Wire_t;
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

    //Reset feltetel alacsony idozitese
    uint16_t reset_lowTime;
    uint16_t reset_highTime;
    //A reset utan, ennyi ido mulva olvassa be a presence bitet
    uint16_t presence_samplingTime;

    //Megj: ha a lista bovitesre kerul, ugy a MyOWI_TIME_TABLE makrot is
    //      boviteni kell!
} MyOWI_Times_t;
//------------------------------------------------------------------------------
//A latenciak miatt bevezetett korrekcios tenyezo, mely segit pontositani az
//idoziteseket. (Az volt a tapasztalat, hogy amig feljott az interrupt, addig
//nagyon sok idot kepes volt elvacakolni az MCU, igy kb 48MHz esetén 1uS-el
//hoszabbak voltak az idozitesek.)
#ifndef MyOWI_TIMING_CORRECTION
#define MyOWI_TIMING_CORRECTION    (40)
#endif
//A TC/t hajto GCLK frekvencia, es a kivant idozitesbol kiszamolja a CC
//regiszterbe irando erteket.
//f: a TC-t hajto orajel frekvenciaja (Hz)
//f: a kivant idozites (us)
#define MyOWI_CALC_CC(f, t) (uint16_t) \
    (( (double)t / (1000000.0 / (double)f) )- MyOWI_TIMING_CORRECTION)

//Seged makro, mely egy konstans MyOWI_Times_t tipusu struktura elemeit
//kepes generalni. Segitsegevel forditasi idoben meghatarozhatok az idozitesi
//konstansok, a driverhez rendelt TC periferiat hajto GCLK orajel freki ismere-
//teben.
//A makrot egy strukturaba kell irni.
//f: a TC-t hajto orajel frekvenciaja (Hz)
#define MyOWI_TIME_TABLE(f)                                              \
    .readBit_lowTime     =MyOWI_CALC_CC(f, MyOWI_READBIT_DRIVE_LOW_TIME),\
    .readBit_samplingTime=MyOWI_CALC_CC(f, MyOWI_READBIT_SAMPLING_TIME), \
    .readBit_highTime    =MyOWI_CALC_CC(f, MyOWI_READBIT_HIGH_TIME),     \
    .writeBit_1_lowTime  =MyOWI_CALC_CC(f, MyOWI_WRITEBIT_1_LOW_TIME),   \
    .writeBit_1_highTime =MyOWI_CALC_CC(f, MyOWI_WRITEBIT_1_HIGH_TIME),  \
    .writeBit_0_lowTime  =MyOWI_CALC_CC(f, MyOWI_WRITEBIT_0_LOW_TIME),   \
    .writeBit_0_highTime =MyOWI_CALC_CC(f, MyOWI_WRITEBIT_0_HIGH_TIME),  \
    .reset_lowTime       =MyOWI_CALC_CC(f, MyOWI_RESET_LOW_TIME),        \
    .reset_highTime      =MyOWI_CALC_CC(f, MyOWI_RESET_HIGH_TIME),       \
    .presence_samplingTime=MyOWI_CALC_CC(f, MyOWI_PRESENCE_SAMPLING_TIME)

//------------------------------------------------------------------------------
//MyOWI driver konfiguracios struktura.
typedef struct
{
    //A driverhez rendelt TC periferia konfiguracioja
    MyTC_Config_t           tcConfig;
    //Elore kalkulalt mukodesi idozitesi konstansok.
    MyOWI_Times_t      times;
} MyOWI_DriverConfig_t;
//------------------------------------------------------------------------------
//A megszakitasban hivott, a megszakitas bekovetkezesekor vegrehajtando allapot
//callback fuggvenyenek felepitese.
typedef void MyOWI_stateFunc_t(struct MyOWI_Driver_t* driver);
//------------------------------------------------------------------------------
//MyOWI driver handlere
typedef struct
{
    //A modulhoz rendelt TC driver valtozoi
    MyTC_t          tc;

    //A TC-t hajto orajel frekvencia fuggvenyeben elore kalkulalt idozitesi
    //parameterek.
    MyOWI_Times_t times;

    //Az aktualisan kezelt wire parametereinek masolata. Ezt azert csinaljuk,
    //hogy a leheto leggyorsabban elerhessuk megszakitasban az aktualis Wire-re
    //vonatkozo informaciokat.    
    MyOWI_WireConfig_t wireConfig;

    //a vonalrol olvasott adatbiteket ebben allitja ossze.
    //Presence bit olvasasa eseten is hasznaljuk. Ilyenkor az also bit hordozza
    //a vonalrol beolvasott allapotot.
    uint8_t rxShiftReg;
    //a vonalra irando adatbiteket ebbol lepteti ki
    uint8_t txShiftReg;

    //a beolvasando/kiirando biteket szamolja. 0-ig csokken
    uint8_t bitCnt;


    //A soron kovetkezo kiirando byteot cimzi.
    uint8_t* txPtr;
    //a hatralevo byteokat szamolja
    uint16_t txCnt;
    //Masodik korben kiirando byteokat cimzi
    uint8_t* txPtr2;
    //Masodik korben kiirando byteok szama
    uint16_t txCnt2;

    //beolvasando byteok celteruletet cimci
    uint8_t* rxPtr;
    //beolvasoando byteokat szamolja. 0-ig csokken.
    uint16_t rxCnt;

    //A megszakitaskor hivando, az aktualis allapotnak megfelelo kiszolgalo
    //fuggvenyre mutat.
    //Ez lehet a vonalon torteno alacsonyszintu allapot: MyOWI_wState_xxx
    //De lehet a magasabb szintu allapotget fuggvenye is: MyOWI_mState_xxx
    MyOWI_stateFunc_t* stateFunc;
    //Alacsony szintu (wire) muveletek utan a fo allapotgep azon fuggvenyere
    //mutat, ahova at kell adni a vezerlest.
    MyOWI_stateFunc_t* returnFunc;

    #ifdef USE_FREERTOS
    //Az egyes Wire-ok elerese kozotti kizarast biztosito mutex
    SemaphoreHandle_t mutex;

    //megszakitasban vegrehajtott folyamat vegetvaro szemafor
    SemaphoreHandle_t doneSemaphore;
    #endif //USE_FREERTOS

    //A megszakitasban keletkezo hibakodot orzi meg. Ez kerul visszaadasra
    //a birtoklo taszk szamara.
    status_t asyncStatus;
} MyOWI_Driver_t;
//------------------------------------------------------------------------------
//Tranzakcios leiro.
//A leirot a MyOWI_transaction() fuffvenynek kell atadni.
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

    //olvasando adatblokk cel buffere
    uint8_t*    rxBuff;
    //Az olvasando byteok szama. Ha nincs olvasas, akkor ez 0.
    uint16_t    rxLength;
} MyOWI_transaction_t;
//------------------------------------------------------------------------------
//Single-Wire (OWI) driver kezdeti inicializalasa
void MyOWI_driverInit(MyOWI_Driver_t* this,
                           const MyOWI_DriverConfig_t* cfg);

//Wire letrehozasa, a megadott konfiguracio alapjan
//A fuggveny hivasat meg kell, hoyg elozze a MyOWI_driverInit() !
void MyOWI_createWire(MyOWI_Wire_t* wire,
                           MyOWI_Driver_t* driver,
                           const MyOWI_WireConfig_t* cfg);

//A hozza rendelt TC-hez tartozo interrupt service rutin
void MyOWI_service(MyOWI_Driver_t* driverv);

//Single-Wire (OWI) tranzakcio vegzese.
//A fuggveny blokkolja a hivo oldalt, mig a tranzakcioval nem vegez.
status_t MyOWI_transaction(MyOWI_Wire_t* wire,
                                const MyOWI_transaction_t* transaction);
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
#define MyOWI_PASTER( a, b )       a ## b
#define MyOWI_EVALUATOR( a, b )    MyOWI_PASTER(a, b)
#define MyOWI_H1(...)  _Handler(void) {MyOWI_service(  __VA_ARGS__ );}
#define MyOWI_H2(n)    MyOWI_EVALUATOR(TC, n)
#define MyOWI_TC_HANDLER(n, ...)   \
      void MyOWI_EVALUATOR(MyOWI_H2(n), MyOWI_H1( __VA_ARGS__))

//Seged makro, mellyel egyszeruen definialhato az OWI-hez hasznalando TC
//interrupt rutinok.
//Ez kerul a kodba peldaul:
//  void TCn_Handler(void){ MyOWI_service( xxxx ); }
//         ^n                              ^...
//n: A driverhez hasznalt TC modul  sorszama (0..4)
//xxxx: A MyOWI_Driver_t* tipusu leiro
#define MyOWI_HANDLERS( n, ...) MyOWI_TC_HANDLER(n, __VA_ARGS__)
//------------------------------------------------------------------------------

#endif //MY_OWI_H_
