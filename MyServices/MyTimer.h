//------------------------------------------------------------------------------
//  Idozito modul
//
//    File: MyTimer.h
//------------------------------------------------------------------------------
#ifndef MYTIMER_H_
#define MYTIMER_H_

#include "MyCommon.h"
#include "MyTC.h"
#include "MyTC_Handler.h"
//------------------------------------------------------------------------------
//Timerek uzemmodjat megado flagek, melyeket vagy kapcsolatba kell megadni a
//timer regisztalasanal.

//Csak egyszer lefuto timer
#define MYTIMER_SINGLE_SHOT 0

//periodikusan mukodo idozito
#define MYTIMER_PERIODIC	1

//A timert a fociklusbol hivogatott a MyTimer_service() rutin alol kezeljuk.
#define MYTIMER_SERVICE     0

//A timert megszakitasban kezeljuk, a callback fuggvenye megszakitas alol lesz
//hivva
#define MYTIMER_INTERRUPT	2

//A timerhez tartozo idozito a megszakitasban ujraindul, fuggetlenul attol, hogy
//a service rutinban meg lett-e hivva.
//Segitsegevel pontos periodikus idozites allithato elo, de a callback hivasok,
//azok a fociklus terheltsegetol fuggoen csuszkalnak.
//Ha ez a flag nincs a SERVICE modnal megadva, akkor a timert a fociklusbol
//hivott callback befejezte utan engedelyezi ujra.
#define MYTIMER_PRECISE     4
//------------------------------------------------------------------------------
//Olyan callback rutint definial, melyet valamely timer esemeny
//bekovetketzesekor hiv meg.
typedef void MyTimer_callbackFunc_t(void*);
//------------------------------------------------------------------------------
//Egyetlen timert leiro struktura
typedef struct _MyTimer_t
{
    //Az idozites ideje.
    uint32_t		interval;
    //a timer szamlaloja
    uint32_t		cnt;

    //Lancolt listaban a kovetkezo timerre mutat.
    //Ha NULL, akkor nincs tobb timer.
    struct MyTimer_t *next;
    //Lancolt listaban az elozo timerre mutat.
    struct MyTimer_t *prev;

    //Az idozites bekovetkezesekor meghivando rutin.
    MyTimer_callbackFunc_t *callbackFunc;

    //A callback fuggvenynek atadott adatok
    void 			*callbackData;

    //A timer mukodeset leiro flagek. (Bitenkent mas a funkcio)
    uint8_t 		modeFlags;
    //True, ha a timer mukodik
    uint8_t			running;
    //True lesz, ha timer lejart. (periodikus timereknel is beall ez a flag.)
    uint8_t			done;

    //Timernek adhato nev (initkor.) Debug celra lehet jo.
    const char*     name;
    //A timert kezelo managerre mutat
    struct MyTimerManager_t* timerManager;
}MyTimer_t;
//------------------------------------------------------------------------------
//Az idozito kezeleshez tartozo valtozok definicioja
typedef struct
{
    MyTC_t  tc;
    //A legelso timer leiroj a lancolt listaban.
    //(A lancolt listat NULL zarja)
    MyTimer_t*	firstTimer;

    //A legutolso elmre mutat a lancolt listaban.
    MyTimer_t*	lastTimer;

    //Szabadon futo szamlalo. Minden timer tick megszakitasban 1-el novekszik
    uint64_t	tickCnt;
}MyTimerManager_t;
//------------------------------------------------------------------------------
//Idozites kezeles konfiguralasa
typedef struct
{
    //A TC modulra vonatkozo konfiguracio
    MyTC_Config_t tcConfig;
    //A TC modul periodus idejehez szukseges elore szamitott ertek.
    //a MyTIMER_CALC_TC_PEIOD_VALUE() makro segitsegevel szamithato.
    uint16_t      tcPeriodValue;
} MyTimerManager_config_t;

//seged makro, mely segit kiszamolni a TC-be beallitando periodus erteket, hogy
//az a kivant idozitest biztositsa.
//gclkfreqHz: a TC modulhoz rendelt GCLK orajel forras frekvenciaja
//tickFreqHz: a TC periodusidejenek megfelelo frekvencia.
#define MyTIMER_CALC_TC_PEIOD_VALUE(gclkFreqHz, tickFreqHz) \
                                                    (gclkFreqHz / tickFreqHz)
//------------------------------------------------------------------------------
//Idozites kezeles kezdeti inicializalasa.
void MyTimer_initManager(MyTimerManager_t* timerManager,
                         const MyTimerManager_config_t* config);

//A fociklusbol hivogatott timer task. Azok a callback rutinok, melyekhez
//service van rendelve, ebbol kerul meghivasra.
void MyTimer_loop(MyTimerManager_t* timerManager);

//Megszakoitasbol hivott rutin, melyben az idozitest vegzi
void MyTimer_service(MyTimerManager_t* timerManager);

//Globalis tick szammalo lekerdezese
uint64_t MyTimer_getTicks(MyTimerManager_t* timerManager);

//stdio-ra debug informacio irasa a timerekrol
void MyTimer_printDebugInfo(MyTimerManager_t* timerManager);

//*Timer	:Az idozitohoz tartozo leiro. (RAM-ban kell, hogy legyen, mivel a
//           strukturaba visszairas tortenik)
//Interval	: Idozites ideje
//ModeFlags : Mukodest bealito flagek (ezeket vagy kapcsolatban lehet hasznalni)
//			/TIMER_SINGLE		-Csak egyszer fut le
//			\TIMER_CYCLE		-Ciklikusan mukodo idozito
//
//			/TIMER_INTERRUPT	-A timert megszakitasban kezeljuk, a callback
//                               fuggvenye megszakitas alol lesz hivva
//			\TIMER_SERVICE		-A timert a fociklusbol hivogatott a
//                               Timer_Service() rutin alol kezeljuk.
//
//			-TIMER_PRECISE		-A timerhez tartozo idozito a megszakitasban
//                               ujraindul, fuggetlenul attol, hogy a service
//                               rutinban meg lett-e hivva.
//						         Segitsegevel pontos periodikus idozites allit-
//                               hato elo, de a callback hivasok,azok a fociklus
//                               terheltsegetol fuggoen csuszkalhatnak.
//
//*Callback : Idozito esemenyre meghivodo rutin
//
//A rutin a letrehozott timert nem inditja el automatikusan!
void MyTimer_create(MyTimer_t* timer,
                   MyTimerManager_t* timerManager,
                   uint32_t interval,
                   uint8_t modeFlags,
                   MyTimer_callbackFunc_t *callbackFunc,
                   void *callbackData,
                   const char* name);

//Timer torlese. (Kiveszi a listabol.)
void MyTimer_delete(MyTimer_t* timer);


//Timer elinditasa. A timer ott folytatja, ahol korabban abbahagyta, peldaul
//esetleg egy korabbi stop miatt
//Ha a timer azert allt, mert korabban lejart, akkor az ujra fog indulni.
void MyTimer_start(MyTimer_t* timer);

//Timer megallitasa
//A szamlalot nem nullazzuk, igy egy ujabb start kiadasaval folytathato az
//idozites.
void MyTimer_stop(MyTimer_t* timer);

//Timer ujrainditasa.
//A timer teljesen elolrol kezdi az idozitest.
void MyTimer_restart(MyTimer_t* timer);

//Timer idozites beallitasa. (Fuggetlenul a letrehozaskor megadott idotol)
void MyTimer_setInterval(MyTimer_t* timer, uint32_t interval);

//True-t ad vissza, ha az adott timer befejezte a futasat
bool MyTimer_expired(MyTimer_t* timer);

//True-t ad vissza, ha az adott timer meg idozit (fut)
bool MyTimer_isRunining(MyTimer_t* timer);

//Az idozitot hajto TC modulhoz IRQ handler definialasat segito makro
#define MYTIMER_HANDLER(tcNum, ...) \
                    MYTC_HANDLER(tcNum) { MyTimer_service(__VA_ARGS__); }
//------------------------------------------------------------------------------
#endif //MYTIMER_H_
