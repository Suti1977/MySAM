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
#include "MySM.h"
#include <string.h>

static status_t MySM_dummyState(struct MySM_t* sm);
//------------------------------------------------------------------------------
//Allapotgep kezdeti inicializalasa
void MySM_init(MySM_t* sm, MySM_stateFunc_t* initialState, void* userData)
{
    //Allapotgep valtozoinak torlese
    memset(sm, 0, sizeof(MySM_t));

    //Megjegyzi a tetszoleges parametert, ami kesobb is elerheto lesz az
    //allapotokban
    sm->userData=userData;

    //Az indulo allapothoz tartozo fuggveny beallitasa
    if (initialState==NULL)
    {
        sm->state=MySM_dummyState;
    } else
    {
        sm->state=initialState;
    }

    //Jelzes az elso allapotnak, hogy elso futtatas. Az allapotban el lehet
    //vegezni az inicializaciokat.
    sm->init=true;
}
//------------------------------------------------------------------------------
//Alapertelmezett allapot. Akkor hivja meg, ha nincs beallitva kovetkezo
//allapothoz fuggveny (NULL)
static status_t MySM_dummyState(struct MySM_t* sm)
{
    (void) sm;
    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Allapotot valto rutin.
void MySM_ChangeState(MySM_t* sm, MySM_stateFunc_t* newState)
{
    sm->newState=newState;
    //Eloirjuk, hogy uj allapot eseten (vagy ha ugyan az az allapot is marad, de
    //ezt a fuggvenyt meghivtak), hogy inicializalja az allapotot.
    sm->init=true;
}
//------------------------------------------------------------------------------
//Allapotgep futtatasa.
status_t MySM_run(MySM_t* sm)
{
    status_t status;

    //Ciklus mindaddig, amig van uj allapotvaltas...
    while(1)
    {
        if (sm->newState)
        {   //Van eloirva allapotvaltas. Ellenorizzuk, hogy nem e ugyan az, mint
            //ami fut...
            if (sm->newState != sm->state)
            {   //Ez egy uj allapot
                //Jelezni fogjuk az uj allapotnak, hogy ha kell inicializaljon,
                //mert ez az elso futtatasa
                sm->init=true;

                //Az uj allapot lesz aktiv...
                sm->state=sm->newState;
            }

            //Kerelem torlese
            sm->newState=NULL;
        } else
        {
            //Vedelem, ha nem lenne megadva allapot, akkor is tudjon hivni
            //valamit.
            if (sm->state==NULL) sm->state=MySM_dummyState;
        }

        //Allapothoz tartozo funkcio meghivsa...
        status=sm->state((struct MySM_t*) sm);

        //Korabban beallitott init jelzes torlese.
        sm->init=false;

        if (sm->newState)
        {   //Van eloirva uj allapot
            if (sm->newState==sm->state)
            {   //Ha ugyan az az allpot, mint ami most fut, akkor nincs mit
                //tenni. Kilepunk. Majd a kovetkezo korben futhat.
                break;
            }
            //Uj allapot van eloirva. Ugras a ciklus elejere, es az uj allapot
            //kiertekelese...
            continue;

        } else
        {   //Nincs uj allapotvaltas. Maradunk ezen az allapoton. Kilepunk.
            break;
        }

    } //while

    return status;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
