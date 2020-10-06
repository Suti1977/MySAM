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
//  Allapotot valtani a MySM_ChangeState() fuggvennyel, vagy a MYSM_NEW_STATE
//  makroval lehet. Ez utobbi ki is lep a hivott allapotbol.
//------------------------------------------------------------------------------
#ifndef MY_SM_H_
#define MY_SM_H_

#include "MyCommon.h"

struct MySM_t;

//Allapotokhoz tartozo fuggvenyek felepitese
typedef status_t MySM_stateFunc_t(struct MySM_t* sm);
//------------------------------------------------------------------------------
//MySM valtozoi
typedef struct _MySM
{
    //Az uj allapotra mutat. Ha NULL, akkor nincs allapotvaltas.
    MySM_stateFunc_t* newState;
    //Aktualis allapotra mutato pointer
    MySM_stateFunc_t* state;

    //Tetszolegesen beallithato valtozo. Az allapotgep allpotaiban az "sm"
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
void MySM_ChangeState(MySM_t* sm, MySM_stateFunc_t* newState);

//Az allapotokban egyszerubb allapotvaltast leiro makro.
//A makro kStatus_Succes-el ki is lep a futtatott allapotbol
//Fontos, hogy az allapotgep handlere az "sm" valtozonevet hasznlja!
#define MYSM_NEW_STATE(newState)                   \
        MySM_ChangeState((MySM_t*)sm, newState); \
        return kStatus_Success

//Egy makro, mely visszaadja az initkor beallitott userData parametrert.
//A makrot az allapotfuggvenyeken belul lehet hasznalni. Parametereben
//megadhato, hogy milyen tipusra kasztolja a parametert.
#define MYSM_USER_DATA(type) ((type) (((MySM_t*)sm)->userData))

//Makro, mely true-t ad vissza, ha egy allapotnak ez az elso hivasa.
#define MYSM_INIT()     (((MySM_t*)sm)->init)

//Makro, mely segit letrehozni az allapotfuggvenyeket
#define MYSM_STATE(functionName) \
    status_t functionName (struct MySM_t* sm)
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_SM_H_
