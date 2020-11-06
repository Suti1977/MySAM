//------------------------------------------------------------------------------
//  Egyszeru eroforras hasznalatot lehetove tevo user 
//
//    File: MySimpleResourceUser.h
//------------------------------------------------------------------------------
#ifndef MYSIMPLERESOURCEUSER_H_
#define MYSIMPLERESOURCEUSER_H_

#include "MyRM.h"

//------------------------------------------------------------------------------
//Egyszeru eroforras hasznalat eseten beallithato hiba callback felepitese,
//melyben az applikacio fele jelezni tudja az eroforras hibait.
typedef void simplelResourceUserStatusFunc_t(resourceStatus_t resourceStatus,
                                             resourceErrorInfo_t* errorInfo,
                                             void* callbackData);

//Az egyszerusitett felhasznalo kezeleshez hasznalt leiro.
typedef struct
{
    //Eroforras hasznalat valtozoi. (Gyakorlatilag ez kerul felbovitesre.)
    resourceUser_t user;

    //Az user mukodesere vonatkozo esemenyflag mezo, melyekkel az applikacio
    //fele lehet jelezni az allapotokat. Ezekre varhatnak az indito/megallito
    //rutinok. (Statikus)
    EventGroupHandle_t  events;
  #if configSUPPORT_STATIC_ALLOCATION
    StaticEventGroup_t  eventsBuff;
  #endif

    //Hiba eseten meghivhato callback funkcio cime. Ezen keresztul lehet jelezni
    //az applikacio fele az eroforras mukodese kozbeni hibakat.
    simplelResourceUserStatusFunc_t* statusFunc;
    //A funkcio meghivasakor atadott tetszoleges adat
    void*  callbackData;

} simpleResourceUser_t;

//Az eroforrashoz simpleUser kezelo hozzaadasa. A rutin letrehozza a
//szukseges szinkronizacios objektumokat, majd megoldja az eroforrashoz valo
//regisztraciot.
void MySimpleResourceUser_add(resource_t* resource,
                              simpleResourceUser_t* user,
                              const char* userName);

//Torli az usert a eroforras hasznaloi kozul.
//Fontos! Elotte a eroforras hasznalatot le kell mondani!
void MySimpleResourceUser_remove(simpleResourceUser_t* user);

//Eroforras hasznalatba vetele. A rutin megvarja, amig az eroforras elindul,
//vagy hibara nem fut az inditasi folyamatban valami miatt.
status_t MySimpleResourceUser_use(simpleResourceUser_t* user);

//Eroforras hasznalatanak lemondasa. A rutin megvarja, amig az eroforras leall,
//vagy hibara nem fut a leallitasi folyamatban valami.
status_t MySimpleResourceUser_unuse(simpleResourceUser_t* user);


//Userhez statusz funkcio beallitsasa
static inline
void MySimpleResourceUser_setStatusFunc(simpleResourceUser_t* user,
                                        simplelResourceUserStatusFunc_t* func,
                                        void*  callbackData)
{
    user->statusFunc=func;
    user->callbackData=callbackData;
}
//------------------------------------------------------------------------------
#endif //MYSIMPLERESOURCEUSER_H_
