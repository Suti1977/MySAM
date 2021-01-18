//------------------------------------------------------------------------------
//  Task-al tamogatott eroforras modul (MyRM resze)
//
//    File: MyTaskedResource.h
//------------------------------------------------------------------------------
#ifndef MYTASKEDRESOURCE_H_
#define MYTASKEDRESOURCE_H_

#include "MyRM.h"
#include "MyCommon.h"

//------------------------------------------------------------------------------
//Eroforras inditasat eloiro esemeny flag
#define MyTASKEDRESOURCE_EVENT__START_REQUEST   BIT(30)
//Eroforras leallitasat kero esemeny flag
#define MyTASKEDRESOURCE_EVENT__STOP_REQUEST    BIT(31)
//------------------------------------------------------------------------------
//Az eroforrast vezerlo valtozok halmaza
typedef struct
{
    //A taszknak kuldott eventek
    EventBits_t events;

    //true, ha a loop azert fut, mert az applikacio altal megadott idozites
    //letelt.
    bool timed;

    //A ciklusnak erre az esemenyre kell varnia. Ezt a loop-ban, vagy a Starting
    //funkciokban is modosithatja az applikacio
    EventBits_t waitedEvents;

    //A varakozasi ido tick-ben
    uint32_t waitTime;

    //Feladat kesz jelzes. Azoknal az eroforrasoknal, melyek elinditasa utan
    //azok mukodese egy ido utan befejezodik, ott ezen a flagen keresztul tudja
    //jelezni azt a start vagy loop callbackban futtatott kod.
    //Hatasara a manager fele RESOURCE_DONE allapot kerul jelzesre, es az
    //eroforras ujra a start feltetelre fog varni.
    bool done;

    //leallitasi kerelem aktiv jelzes az eroforrasra
    bool resourceStopRequest;

    //Az applikacio jelezheti, hogy nem szabad az eroforrast leallitani.
    //Ez akkor fontos, ha olyan folyamatot futtatunk, melynek mindenkepen vegig
    //kell tudni futnia.
    bool prohibitStop;

    //Az eroforras mukodik jelzes. Alapertelmezesben ez ben van allitva, de van
    //lehetoseg a startFunc() callbackben a flaget torolni. Igy a loop funkcio
    //ugy indul el, hogy az eroforras manager fele nem jelez "elindult"
    //allapotot.
    //A loop-ban igy van lehetoseg az eroforarst inditani, majd ha kesz, akkor
    //a flaget true-ba allitani, mely hatasara a manager fele "elindult" jelzest
    //kuld.
    //Az applikacio a loop-bol kilepve jelezheti, ha elindult az eroforras.
    bool run;

    //Az aktualis tick ido. CSAK A LOOP callbackben hasznalhato!!!!
    uint64_t time;

} taskedRsource_control_t;
//------------------------------------------------------------------------------
//Az eroforras inicializalasakor meghivodo callback definicioja
typedef status_t taskedResourceInitFunc_t(void* callbackData);
//Az eroforras inditasi kerelmekor hivott callback rutin definicioja
typedef void taskedResourceStartReqFunc_t(void* callbackData);
//Az eroforras leallitasi kerelmenel hivott callback rutin definicioja
typedef void taskedResourceStopReqFunc_t(void* callbackData);
//Az eroforras elinditasara, a taszkban meghivodo rutin
typedef status_t taskedResourceStartFunc_t(void* callbackData,
                                              taskedRsource_control_t* control);
//Az eroforras leallitasi kerelme utan a taszkban meghivodo rutin
typedef status_t taskedResourceStopFunc_t(void* callbackData);
//Hiba eseten, a taszkbol hivott callback definicioja
typedef void taskedResourceErrorFunc_t(void* callbackData, status_t errorCode);
//Eroforrast futtato callback definicioja
typedef status_t taskedResourceLoopFunc_t(void* callbackData,
                                          taskedRsource_control_t* control);
//------------------------------------------------------------------------------
//Taszkal rendelkezo eroforras inicializalasanal hasznalt struktura.
typedef struct
{
    //Az eroforras neve. Ugyan ezt hasznalja a letrehozott taszk nevenek is.
    const char* name;
    //Az eroforrast futtato taszk prioritasa
    uint32_t taskPriority;
    //Az eroforrast futtato taszk szamara allokalt taszk merete
    uint32_t taskStackSize;

    //Az eroforras callbckjai szamara atadott tetszoleges parameter
    void* callbackData;

    //Az eroforras inicializalasakor meghivodo callback definicioja
    taskedResourceInitFunc_t* initFunc;
    //Az eroforras inditasi kerelmekor hivott callback rutin definicioja
    taskedResourceStartReqFunc_t* startRequestFunc;
    //Az eroforras leallitasi kerelmenel hivott callback rutin definicioja
    taskedResourceStopReqFunc_t* stopRequestFunc;
    //Az eroforras elinditasara, a taszkban meghivodo rutin
    taskedResourceStartFunc_t* startFunc;
    //Az eroforras leallitasi kerelme utan a taszkban meghivodo rutin
    taskedResourceStopFunc_t* stopFunc;
    //Hiba eseten, a taszkbol hivott callback definicioja
    taskedResourceErrorFunc_t* errorFunc;
    //Eroforrast futtato callback definicioja
    taskedResourceLoopFunc_t* loopFunc;
} taskedResource_config_t;
//------------------------------------------------------------------------------
//Eroforras bovitmeny valtozoi.
typedef struct
{
    //Az eroforrast futtato taszk handlere
    TaskHandle_t taskHandle;

    //Az eroforras mukodeset befolyasolo esemeny flagek csoprtja
    //EventGroupHandle_t events;

    //letrehozaskor kapott konfiguracio
    taskedResource_config_t cfg;
} taskedResourceExtension_t;
//------------------------------------------------------------------------------
//Taszkal tamogatott eroforras letrehozasa
void MyTaskedResource_create(resource_t* resource,
                                     taskedResourceExtension_t* ext,
                                     const taskedResource_config_t* cfg);


////A taszkkal bovitett eroforrashoz tartozo esemenymezo handlerenek lekerdezese
//static inline
//EventGroupHandle_t MyTaskedResource_getEventHandler(resource_t* resource)
//{
//    return ((taskedResourceExtension_t*)resource->ext)->events;
//}
//A taszkkal bovitett eroforrashoz taszk handlerenek lekerdezese

//A taszkkal bovitett eroforrashoz tartozo taszk handlerenek lekerdezese
static inline
TaskHandle_t MyTaskedResource_getTaskHandler(resource_t* resource)
{
    return ((taskedResourceExtension_t*)resource->ext)->taskHandle;
}

//A taszkkal bovitett eroforras taszkjanak esemeny(notify) kuldese
static inline
void MyTaskedResource_setEvent(resource_t* resource, uint32_t event)
{
    //xEventGroupSetBits(((taskedResourceExtension_t*)resource->ext)->events,
    //                   event);
    xTaskNotify(MyTaskedResource_getTaskHandler(resource), event, eSetBits);
}
//A taszkkal bovitett eroforras taszkjanak esemeny(notify) kuldese megszakitasbol
static inline
void MyTaskedResource_setEventFromIsr(resource_t* resource, uint32_t event)
{
    BaseType_t higherPriorityTaskWoken=pdFALSE;
    //xEventGroupSetBits(((taskedResourceExtension_t*)resource->ext)->events,
    //                   event);
    xTaskNotifyFromISR(MyTaskedResource_getTaskHandler(resource),
                       event,
                       eSetBits,
                       &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken)
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
#endif //MYTASKEDRESOURCE_H_
