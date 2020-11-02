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
            //Ha van megadva az user-hez statusz callback, akkor hibak
            //eseten az meghivasra kerul.
            if (user->errorFunc)
            {   //Van beallitva callback
                user->errorFunc(errorInfo, user->callbackData);
            }
            xEventGroupSetBits(user->events, SIMPLE_USER_EVENT__ERROR);
            break;
        default:
            ASSERT(0);
            break;
    }
}
//------------------------------------------------------------------------------
//Az eroforrashoz simpleUser kezelo hozzaadasa. A rutin letrehozza a
//szukseges szinkronizacios objektumokat, majd megoldja az eroforrashoz valo
//regisztraciot.
void MySimpleResourceUser_add(resource_t* resource,
                          simpleResourceUser_t* user,
                          const char* userName)
{
    //Esemenyflag mezo letrehozasa, melyekre az alkalmazas taszkja varhat.
  #if configSUPPORT_STATIC_ALLOCATION
    user->events=xEventGroupCreateStatic(&user->eventsBuff);
  #else
    user->events=xEventGroupCreate();
  #endif

    //Az usrekehez tartozo kozos allapot callback lesz beallitva
    MyRM_configUser(&user->user, MySimpleResourceUser_statusCB, user);

    //A kiejlolt eroforrashoz user hozzaadasa
    MyRM_addUser(resource, &user->user, userName);
}
//------------------------------------------------------------------------------
//Torli az usert az eroforras hasznaloi kozul.
//Fontos! Elotte az eroforras hasznalatot le kell mondani!
void MySimpleResourceUser_remove(simpleResourceUser_t* user)
{
    MyRM_removeUser(user->user.resource, &user->user);
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
        //A hibakodot kiolvassuk az eroforrasbol, es avval terunk majd vissza.
        status=user->user.resource->reportedError->errorCode;

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
        //A hibakodot kiolvassuk az eroforrasbol, es avval terunk majd vissza.
        status=user->user.resource->reportedError->errorCode;

        //eroforras leallitasa, igy biztositva abban a hiba torleset
        MyRM_unuseResource(&user->user);
    }

    return status;
}
//------------------------------------------------------------------------------