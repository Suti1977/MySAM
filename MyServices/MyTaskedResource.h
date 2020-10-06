//------------------------------------------------------------------------------
//  Task-al tamogatott eroforras modul (MyRDM resze)
//
//    File: MyTaskedResource.h
//------------------------------------------------------------------------------
#ifndef MYTASKEDRESOURCE_H_
#define MYTASKEDRESOURCE_H_

#include "MyRDM.h"
#include "MyCommon.h"

//Eroforras inditasat eloiro esemeny flag
#define MyTASKEDRESOURCE_EVENT__START_REQUEST   BIT(22)
//Eroforras leallitasat kero esemeny flag
#define MyTASKEDRESOURCE_EVENT__STOP_REQUEST    BIT(23)

//------------------------------------------------------------------------------
//Az eroforras inicializalasakor meghivodo callback definicioja
typedef status_t taskedResourceInitFunc_t(void* callbackData);
//Az eroforras inditasi kerelmekor hivott callback rutin definicioja
typedef status_t taskedResourceStartFunc_t(void* callbackData);
//Az eroforras leallitasi kerelmenel hivott callback rutin definicioja
typedef status_t taskedResourceStopFunc_t(void* callbackData);
//Az eroforras elinditasara, a taszkban meghivodo rutin
typedef status_t taskedResourceStartingFunc_t(void* callbackData);
//Az eroforras leallitasi kerelme utan a taszkban meghivodo rutin
typedef status_t taskedResourceStoppingFunc_t(void* callbackData);
//Hiba eseten, a taszkbol hivott callback definicioja
typedef status_t taskedResourceErrorFunc_t(void* callbackData);
//Eroforrast futtato callback definicioja
typedef status_t taskedResourceLoopFunc_t(void* callbackData,
                                          EventBits_t* waitedEvents);
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
    taskedResourceStartFunc_t* startFunc;
    //Az eroforras leallitasi kerelmenel hivott callback rutin definicioja
    taskedResourceStopFunc_t* stopFunc;
    //Az eroforras elinditasara, a taszkban meghivodo rutin
    taskedResourceStartingFunc_t* startingFunc;
    //Az eroforras leallitasi kerelme utan a taszkban meghivodo rutin
    taskedResourceStoppingFunc_t* stoppingFunc;
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
    EventGroupHandle_t events;

    //Az eroforras allapotanak visszajelzesehez szuksehges handler.
    void* statusHandler;

    //letrehozaskor kapott konfiguracio
    taskedResource_config_t cfg;
} taskedResourceExtension_t;
//------------------------------------------------------------------------------
//Taszkal tamogatott eroforras letrehozasa
void MyTaskedResource_createResource(resource_t* resource,
                                     taskedResourceExtension_t* ext,
                                     const taskedResource_config_t* cfg);


//A taszkkal bovitett eroforrashoz tartozo esemenymezo handlerenek lekerdezese
static inline
EventGroupHandle_t MyTaskedResource_getEventHandler(resource_t* resource)
{
    return ((taskedResourceExtension_t*)resource->ext)->events;
}
//------------------------------------------------------------------------------
#endif //MYTASKEDRESOURCE_H_
