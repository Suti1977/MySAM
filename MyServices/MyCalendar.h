//------------------------------------------------------------------------------
//  Valos ideju ora es naptar kezeles
//
//    File: MyCalendar.h
//------------------------------------------------------------------------------
#ifndef MyCALENDAR_H_
#define MyCALENDAR_H_


#include "MyRTC.h"
#include "MyCommon.h"
//------------------------------------------------------------------------------
//Riasztasi esemeny eseten meghivot callback fuggvenyek definicioja
typedef void MyCalendar_alarmFunc_t(void* callbackData);
//------------------------------------------------------------------------------
typedef enum
{
    //Egy naptari idopontot hataroz meg. Ha az RTC-ben az idot modositjak, akkor
    //ez az idopont nem valtozik.
    ALARM_MODE_TIME,

    //Egy riasztasi idopontot hataroz meg, mely relativ. Ha az RTC-t ujra-
    //kalibraljak, akkor az ilyen tipusu bejegyzes utan lesz hangolva, mivel
    //valtozik a relativ masodperc szamlalo.
    ALARM_MODE_RELATIVE,

    //Periodikusan bekovetkezo idozito uzemmod
    ALARM_MODE_PERIODIC,
} MyCalendar_alarmMode_t;
//------------------------------------------------------------------------------
//Valos ideju orahoz kotott riasztasok definiciora leiro.
//Ezekbol egy lancolt lista epul fel, melyen az alarm eseten vegighalad a modul,
//es ha egyezik az idopont, akkor meghivja a beregisztralt callback fuggvenyt.
typedef struct
{    
    //A riasztasi bejegyzes tipusa
    MyCalendar_alarmMode_t   mode;

    //Masodpercben megadva a riasztasi idopont.
    //Periodikus mukodes eseten az elso, majd kesobb a periodikus ismetlesek
    //idopontjai.
    uint32_t    time;

    //Periodikus mukodes eseten az idokoz
    uint32_t period;

    //A riasztas bekovetkezesekor meghivando callback funkcio, es az ahhoz
    //atadott tetszoleges parameter.
    MyCalendar_alarmFunc_t*   alarmFunc;
    void*   callbackParam;

    //Nyomkovetest segito nev adhato meg.
    const char* alarmName;

    //True, ha a leiro engedelyezett. Az RTC ebben az esetben kiertekeli.
    //(Privat)
    bool    enabled;
    //Lancolt listahoz pointerek (Privat)
    struct MyCalendar_alarm_t* prev;
    struct MyCalendar_alarm_t* next;

} MyCalendar_alarm_t;
//------------------------------------------------------------------------------
//Modul inicializalasnal atadando konfiguracios struktura
typedef struct
{
    //Az RTC periferia IRQ prioritasa
    uint32_t rtcIrqPriority;
    //ha true, akkor az inicializalaskor engedelyezni is fogja a modult es az
    //RTC-t.
    bool enable;

    //EVCTRL regiszterbe irando bitek.
    RTC_MODE0_EVCTRL_Type evctrl;
} MyCalendar_config_t;
//------------------------------------------------------------------------------
//Modul sajat valtozoi.
typedef struct
{
    //Modul valtozoit ved mutex. (Ez kell peldaul a riasztasok listajanak
    //tobb szalbol torteno utkozesek vedelmehez.)
    SemaphoreHandle_t   mutex;

    //A kovetkezo riasztasi idopont
    uint32_t    nextAlarmSec;

    //A riasztasi leirok lancolt listajanak elso es utolso tagja
    MyCalendar_alarm_t*  firstAlarm;
    MyCalendar_alarm_t*  lastAlarm;

} MyCalendar_t;
extern MyCalendar_t myCalendar;
//------------------------------------------------------------------------------
//naptar/ora modul kezdeti inicializalasa
//Az initet meg kell, hogy elozze az RTC periferia inicializalasa
void MyCalendar_init(const MyCalendar_config_t* cfg);

//Masodperces ora beallitasa
//(Az RTC periferia 32 bites counter szamlaloja mutexelten lesz beallitva)
//A relativ idopontok automatikusan adjusztalva lesznek.
status_t MyCalendar_setTimeSec(uint32_t sec);

//Masodperces ora lekerdezese
uint32_t MyCalendar_getTimeSec(void);

//Uj riasztasi leiro hozzaadasa a riasztasi sorhoz.
//Ez meg nem fogja azt beengedelyezni. Arrol gondoskodni kell!
void MyCalendar_addAlarm(MyCalendar_alarm_t* alarm);
//Riasztasi leiro torlese a listabol
void MyCalendar_deleteAlarm(MyCalendar_alarm_t* alarm);

//Egy leiro engedelyezese/tiltasa
//Figyelem! Az engedelyezo rutin nem ellenorzi, hogy a leiro valoban hozza van-e
//adva a riasztasi leirok listajahoz
void MyCalendar_enableAlarm(MyCalendar_alarm_t* alarm, bool enable);

//Egy meglevo riasztasi leiro modositasa elojeles masodpercel.
//Egy beengedelyezett leiro eseten is mukodik.
void MyCalendar_adjustAlarm(MyCalendar_alarm_t* alarm, int sec);

//Egy leiroban idozites letrehozasa, az aktualis idohoz kepest.
//Ez a rutin biztositja, hogy mas taszk kozben ne valtoztathassa az idot.
//Sec-ben megadjuk, hogy a mostani idopontjoz kepest mikorra legyen beallitva
//az ebresztesi idopont.
//Enable ha tru, akkor a riasztast be is engedelyezi.
void MyCalendar_setAlarmFromNow(MyCalendar_alarm_t* alarm,
                                uint32_t sec,
                                bool enable);

//A modul mokodeserol informacio irasa az stdio-ra.
void MyCalendar_printInfo(void);

//RTC megszakitasakor hivando fuggveny
//Ebben tortenik meg a riasztasi lista kiertekelese...
void MyCalendar_service(void);
//------------------------------------------------------------------------------
#endif //MyCALENDAR_H_
