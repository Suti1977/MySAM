//------------------------------------------------------------------------------
//  Egyszeru eroforras hasznalatot lehetove tevo user 
//
//    File: MySimpleResourceUser.c
//------------------------------------------------------------------------------
#include "MySimpleResourceUser.h"
#include <string.h>

#define MySIMPLERESOURCEUSER_TRACE  1

//Az egyszeru eroforras felhasznalok (userek) eseteben hasznalhato esemeny
//flagek, melyekkel az egyszeru user statusz callbackbol jelzunk a varakozo
//taszk fele.
//Az eroforras elindult
#define SIMPLE_USER_EVENT__RUN                        BIT(0)
//Az eroforras leallt
#define SIMPLE_USER_EVENT__STOP                       BIT(1)
//Az eroforrassal hiba van.
#define SIMPLE_USER_EVENT__ERROR                      BIT(2)
//Az eroforras vegzett (egyszer lefuto folyamatok eseten)
#define SIMPLE_USER_EVENT__DONE                       BIT(3)

//------------------------------------------------------------------------------
//Callback rutin, melyet az userre varas eseten hasznalunk
static void MySimpleResourceUser_statusCB(resourceStatus_t resourceStatus,
                                          resourceErrorInfo_t* errorInfo,
                                          void* callbackData)
{
    simpleResourceUser_t* user=(simpleResourceUser_t*) callbackData;

    #if MySIMPLERESOURCEUSER_TRACE
    printf("MySimpleResourceUser_statusCB() (%s)  status:%d\n",
           user->user.userName, resourceStatus);
    #endif

    //Ha van megadva az user-hez statusz callback, akkor az meghivasra kerul.
    if (user->statusFunc)
    {   //Van beallitva callback
        user->statusFunc(resourceStatus, errorInfo, user->callbackData);
    }

    if (errorInfo)  user->asyncStatus=errorInfo->errorCode;

    //Elagaz a statusz alapjan
    switch((int)resourceStatus)
    {
        case RESOURCE_RUN:
            //Az eroforras elindult. Jelezzuk a varakozo taszknak.
            xEventGroupSetBits(user->events, SIMPLE_USER_EVENT__RUN);
            break;
        case RESOURCE_STOP:
            //Az eroforras rendben leallt. Jelezzuk a varakozo taszknak.
            xEventGroupSetBits(user->events, SIMPLE_USER_EVENT__STOP);
            break;
        case RESOURCE_DONE:
            //Az eroforras vegzett, es leallt
            //vagy az eroforrasrol az user sikeresen lemondott.
            xEventGroupSetBits(user->events, SIMPLE_USER_EVENT__DONE);
            break;
        case RESOURCE_ERROR:
            //Az eroforras hibara futott.
            xEventGroupSetBits(user->events, SIMPLE_USER_EVENT__ERROR);
            break;
        default:
            ASSERT(0);
            break;
    }
}
//------------------------------------------------------------------------------
//Az eroforrashoz simpleUser kezelo letrehozasa. A rutin letrehozza a
//szukseges szinkronizacios objektumokat, majd megoldja az eroforrashoz valo
//regisztraciot.
void MySimpleResourceUser_create(simpleResourceUser_t* user,
                                 const simpleResourceUser_config_t* cfg)
{
    memset(user, 0, sizeof(simpleResourceUser_t));

    //Esemenyflag mezo letrehozasa, melyekre az alkalmazas taszkja varhat.
  #if configSUPPORT_STATIC_ALLOCATION
    user->events=xEventGroupCreateStatic(&user->eventsBuff);
  #else
    user->events=xEventGroupCreate();
  #endif

    //Az alkalmazas fele torteno allapot kozleshez callback beregisztralasa
    user->statusFunc=cfg->statusFunc;
    user->callbackData=cfg->callbackData;

    resourceUser_config_t userCfg=
    {
        .name=cfg->name,
        .resource=cfg->resource,
        .statusFunc=MySimpleResourceUser_statusCB,
        .callbackData=user
    };

    //A kijelolt eroforrashoz aszinkron user letrehozasa
    MyRM_createUser(&user->user, &userCfg);
}
//------------------------------------------------------------------------------
//Torli az usert az eroforras hasznaloi kozul.
//Fontos! Elotte az eroforras hasznalatot le kell mondani!
void MySimpleResourceUser_delete(simpleResourceUser_t* user)
{
    MyRM_deleteUser(&user->user);

    #if configSUPPORT_STATIC_ALLOCATION
      //
    #else
      vEventGroupDelete(user->events);
    #endif
}
//------------------------------------------------------------------------------
//Eroforras hasznalatba vetele. A rutin megvarja, amig az eroforras elindul,
//vagy hibara nem fut az inditasi folyamatban valami miatt.
status_t MySimpleResourceUser_use(simpleResourceUser_t* user)
{
    status_t status=kStatus_Success;

    #if MySIMPLERESOURCEUSER_TRACE
    printf("MySimpleResourceUser_use() (%s)\n", user->user.userName);
    #endif

    user->asyncStatus=kStatus_Success;

    //Eroforras hasznalatba vetele. A hivas utan a callbackben kapunk jelzest
    //az eroforras allapotarol.
    MyRM_useResource(&user->user);

    //Varakozas, hogy megjojjon az inditasi folyamtrol a statusz.
    //TODO: timeoutot belefejleszteni?
    EventBits_t Events=xEventGroupWaitBits( user->events,
                                            SIMPLE_USER_EVENT__RUN |
                                            SIMPLE_USER_EVENT__ERROR,
                                            pdTRUE,
                                            pdFALSE,
                                            portMAX_DELAY);

    if (Events & SIMPLE_USER_EVENT__ERROR)
    {   //Hiba volt az eroforras inditasakor        
        status=user->asyncStatus;

        //eroforras leallitasa, igy biztositva abban a hiba torleset
        MyRM_unuseResource(&user->user);
    }

    return status;
}
//------------------------------------------------------------------------------
//Eroforras hasznalatanak lemondasa. A rutin megvarja, amig az eroforras leall,
//vagy hibara nem fut a leallitasi folyamatban valami.
status_t MySimpleResourceUser_unuse(simpleResourceUser_t* user)
{
    status_t status=kStatus_Success;

    #if MySIMPLERESOURCEUSER_TRACE
    printf("MySimpleResourceUser_unuse() (%s)\n", user->user.userName);
    #endif

    //Eroforras hasznalatanak lemondasa. A hivas utan a callbackben kapunk
    //jelzest az eroforras allapotarol.
    MyRM_unuseResource(&user->user);


    //Varakozas, hogy megjojjon a leallast jelzo esemeny
    //TODO: timeoutot belefejleszteni?
    EventBits_t events=xEventGroupWaitBits( user->events,
                                            SIMPLE_USER_EVENT__STOP |
                                            SIMPLE_USER_EVENT__ERROR |
                                            SIMPLE_USER_EVENT__DONE,
                                            pdTRUE,
                                            pdFALSE,
                                            portMAX_DELAY);

    if (events & SIMPLE_USER_EVENT__ERROR)
    {   //Hiba volt az eroforras leallitasakor        
        status=user->asyncStatus;

        //eroforras leallitasa, igy biztositva abban a hiba torleset
        MyRM_unuseResource(&user->user);
    }

    return status;
}
//------------------------------------------------------------------------------
