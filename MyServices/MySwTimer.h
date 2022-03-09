//------------------------------------------------------------------------------
//  Szoftveres idozito modul
//
//    File: MySwTimer.h
//------------------------------------------------------------------------------
#ifndef MYSWTIMER_H_
#define MYSWTIMER_H_

#include "MyCommon.h"
//------------------------------------------------------------------------------
//Idozito leiro definicioja. Minden idoziteshez tartozik egy ilyen leiro.
typedef struct
{
    //true, ha az idozites aktiv, es mukodik.
    bool active;

    //Az az idopont, amikor az idozites letellik
    uint64_t nextTime;

    //Periodusido. Ha ez nem 0, akkor a timer periodikusan mukodik, es egy
    //idozites letelte utan, az itt beallitott idovel ujrainditja magat.
    uint32_t periodTime;

    //true, ha az idozites letelt
    bool expired;

    //Az idozitot kezelo managerre mutat;
    struct MySwTimerManager_t* manager;


    //Lancolt lista kezeleshez szukseges valtozok
    struct MySwTimer_t* next;
    struct MySwTimer_t* prev;
} MySwTimer_t;
//------------------------------------------------------------------------------
//MySwTimer valtozoi
typedef struct
{
    //A managger altal ismert ido.
    uint64_t time;

    //A manager altal kezelt elso idozito leirora mutat
    MySwTimer_t* firstTimer;
    //A manager altal kezelt utolso idozito leirora mutat
    MySwTimer_t* lastTimer;

    //A kovetkezo futtatasig szukseges ido, amikor valamelyik aktivi idozitoje
    //aktiv lesz.
    uint64_t nextExecutionTime;
} MySwTimerManager_t;
//------------------------------------------------------------------------------
//Idozito manager kezdeti inicializalasa
void MySwTimer_initManager(MySwTimerManager_t* manager);

//Uj idozito hozzaadasa az idozito managerhez
void MySwTimer_addTimer(MySwTimerManager_t* manager, MySwTimer_t* timer);

//Idozito torlese az idozito manager altal kezelt idozitok listajabol
void MySwTimer_deleteTimer(MySwTimerManager_t* manager, MySwTimer_t* timer);

//Idozito manager futtatasa
//time-ben meg kell adni az aktualis idot
status_t MySwTimer_runManager(MySwTimerManager_t* manager, uint64_t time);

//Annak az idonek a lekerdezese, amennyi ido mulva a managert ujra futtatni kell.
uint64_t MySwTimer_getWaitTime64(MySwTimerManager_t* manager);
//Annak az idonek a lekerdezese, amennyi ido mulva a managert ujra futtatni kell.
//A lekerdezes 32 bites intet ad vissza, es PORT_MAX_DELAY vagy 0xffffffff-ra
//kerul szaturalasra, ha 32 bitnel naygaobb szamertek jonne ki.
uint32_t MySwTimer_getWaitTime32(MySwTimerManager_t* manager);

//Timer inditasa.
//actualTime: az aktualis ido, amihez kepest szamolhat
//interval: az az ido, amennyi ido mulva az elso lejarat kovetkezik
//periodTime: ha nem 0, akkor periodikus modban indulva enyni idonkent hivodik meg
void MySwTimer_start(MySwTimer_t* timer,
                     uint32_t interval,
                     uint32_t periodTime);

//Timer leallitasa
void MySwTimer_stop(MySwTimer_t* timer);

//Annak lekerdezese, hogy az idozites lejart-e
//true-t ad vissza, ha lejart. Egyben a jelzes torlese is.
bool MySwTimer_expired(MySwTimer_t* timer);

//Annak lekerdzese, hogy az idozito aktiv-e
bool MySwTimer_isActive(MySwTimer_t* timer);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYSWTIMER_H_
