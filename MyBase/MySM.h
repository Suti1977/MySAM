//------------------------------------------------------------------------------
//  Allapotgep tamogato modul
//------------------------------------------------------------------------------
//  Ez a modul segit abban, hogy egyszeruen kezelheto allapotgepeket hozzunk
//  letre. Minden allapotgephez tartzik egy MySM_t tipusu leiro struktura.
//  Az allapotgep egyes allapotai, kulon fuggvenyek, melyek hivogatasra
//  kerulnek, a MySM_run()-bol. Ha uj allapotot valtunk, akkor a MySM_run nem
//  lep ki, hanem az uj allapotnak megadott fuggvenyt hivja meg.
//  Ha egy allapot futtatas alatt nem tortenik allapotvaltas, akkor a MySM_run
//  fuggveny is kilep.
//  Az allapotvaltasokkor az MySM_t struktura "init" eleme true-ba all. Igy a
//  frissen hivott allapot elvegezheti az inicializalasait.
//  Allapotot valtani a MySM_changeState() fuggvennyel, vagy a MYSM_NEW_STATE
//  makroval lehet. Ez utobbi ki is lep a hivott allapotbol.
//  Minden allapoton belul beallithato egy kilepesi fuggveny, melyet akkor hiv,
//  ha uj allapotba leptetjuk az allapotgepet. Beallitasa a MySM_setExitFunc()
//  fugvenynel lehetseges. A fuggvenyt a hivasa utan elfelejti.
//  Beallithato egy olyan fuggveny is, melyet minden futaskor/allapotvaltaskor
//  meghiv. Ennek beallitasa a MySM_setAlwaysRunFunc() fugvenynel lehetseges.
//------------------------------------------------------------------------------
#ifndef MY_SM_H_
#define MY_SM_H_

#include "MyCommon.h"

struct MySM_t;

//Allapotokhoz tartozo fuggvenyek felepitese
typedef status_t MySM_stateFunc_t(struct MySM_t* sm);
//Az allapotok elhagyasakor hivhato fuggveny definialasa
typedef status_t MySM_stateExitFunc(struct MySM_t* sm);
//Az allapotgep belso ciklusaban, minden allapotvaltas vagy allapot futtatas
//elott meghivodo fuggevny definicioja
typedef status_t MySM_alwaysRunFunc(struct MySM_t* sm);
//------------------------------------------------------------------------------
//MySM valtozoi
typedef struct _MySM
{
    //Az uj allapotra mutat. Ha NULL, akkor nincs allapotvaltas.
    MySM_stateFunc_t* newState;
    //Aktualis allapotra mutato pointer
    MySM_stateFunc_t* state;

    //Az allapotok elhagyasakor hivhato fuggveny.
    //Az allapotbol torteno kilepes utan torlodik.
    MySM_stateExitFunc* exitFunc;

    //Az allapotgep belso ciklusaban, minden allapotvaltas vagy allapot futtatas
    //elott meghivodo fuggevny. Ha ez nem NULL, akkor hivja.
    MySM_alwaysRunFunc* alwaysRunFunc;

    //Tetszolegesen beallithato valtozo. Az allapotgep allapotaiban az "sm"
    //mezobol kiolvashato.
    void* userData;

    //True, ha az allapotvaltas miatt az uj allapot most van hivva eloszor.
    bool    init;

} MySM_t;
//------------------------------------------------------------------------------
//Allapotgep kezdeti inicializalasa
void MySM_init(MySM_t* sm, MySM_stateFunc_t* initialState, void* userData);

//Allapotgep kiertekelese, es futtatasa
status_t MySM_run(MySM_t* sm);


//Allapotot valto rutin.
void MySM_changeState(MySM_t* sm, MySM_stateFunc_t* newState);

//Allapot elhagyasakor hivando fuggveny beallitasa.
//Allapotokon belul is hivhato.
void MySM_setExitFunc(MySM_t* sm, MySM_stateExitFunc* exitFunc);

//Az allapotgep belso ciklusaban, minden allapotvaltas vagy allapot futtatas
//elott meghivodo fuggevny beallitasa.
//Allapotokon belul is hivhato.
void MySM_setAlwaysRunFunc(MySM_t* sm, MySM_alwaysRunFunc* alwaysRunFunc);

//Az allapotokban egyszerubb allapotvaltast leiro makro.
//A makro kStatus_Succes-el ki is lep a futtatott allapotbol
//Fontos, hogy az allapotgep handlere az "sm" valtozonevet hasznlja!
#define MYSM_NEW_STATE(newState)                   \
        MySM_changeState((MySM_t*)sm, newState); \
        return kStatus_Success

//Egy makro, mely visszaadja az initkor beallitott userData parametrert.
//A makrot az allapotfuggvenyeken belul lehet hasznalni. Parametereben
//megadhato, hogy milyen tipusra kasztolja a parametert.
#define MYSM_USER_DATA(type) ((type) (((MySM_t*)sm)->userData))

//Makro, mely true-t ad vissza, ha egy allapotnak ez az elso hivasa.
#define MYSM_STATE_INIT()     (((MySM_t*)sm)->init)

//Makro, mely segit letrehozni az allapotfuggvenyeket
//MySM_stateFunc_t
#define MYSM_STATE(functionName) \
    status_t functionName (struct MySM_t* sm)

//Makro, mely segit letrehozni az allapotok elhagyasakor hivhato fuggevnyeket
//MySM_stateExitFunc
#define MYSM_STATE_EXIT_FUNC(functionName) \
    status_t functionName (struct MySM_t* sm)

//Makro, mely segit letrehozni az allapotgepben mindig lefutatott fuggvenyt
//MySM_alwaysRunFunc
#define MYSM_ALWAYS_RUN_FUNC(functionName) \
    status_t functionName (struct MySM_t* sm)

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_SM_H_
