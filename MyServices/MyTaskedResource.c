//------------------------------------------------------------------------------
//  Task-al tamogatott eroforras modul (MyRDM resze)
//
//    File: MyTaskedResource.c
//------------------------------------------------------------------------------
#include "MyTaskedResource.h"

static

static status_t MyTaskedResource_resource_init(void* param, void* statusHandler);
static status_t MyTaskedResource_resource_start(void* param);
static status_t MyTaskedResource_resource_stop(void* param);
static void __attribute__((noreturn)) MyTaskedResource_task(void* taskParam);
//------------------------------------------------------------------------------
//Taszkal tamogatott eroforras letrehozasa
void MyTaskedResource_createResource(resource_t* resource,
                                     taskedResourceExtension_t* this,
                                     const taskedResource_config_t* cfg)
{
    //Konfiguracio masolasa
    memcpy(&this->cfg, cfg, sizeof(taskedResource_config_t));

    //Eroforras vezerleset biztosito esemenymezo letrehozasa
    this->events=xEventGroupCreate();
    ASSERT(this->events);

    //Eroforras letrehozasa...
    //I2C buszok eroforras kezeleset biztosito fuggvenyek
    static const resourceFuncs_t resourceFuncs=
    {
        .init =MyTaskedResource_resource_init,
        .start=MyTaskedResource_resource_start,
        .stop =MyTaskedResource_resource_stop,
    };

    //Eroforras letrehozasa.
    //Az "ext" mezojeben beallitasra kerul a taszkos mukodeshez szukseges
    //bovitett adatok (tehat amit most hozunk letre)
    MyRDM_createResourceExt(resource,
                            &resourceFuncs,
                            this,
                            this->cfg.name,
                            this);

   //Eroforrast futtato taszk letrehozasa
   if (xTaskCreate(MyTaskedResource_task,
                   cfg->name,
                   (const configSTACK_DEPTH_TYPE) cfg->taskStackSize,
                   resource,
                   cfg->taskPriority,
                   &this->taskHandle)!=pdPASS)
   {
       ASSERT(0);
   }
}
//------------------------------------------------------------------------------
//Eroforras kezdeti inicializalasakor hivott callback
static status_t MyTaskedResource_resource_init(void* param, void* statusHandler)
{
    taskedResourceExtension_t* this=(taskedResourceExtension_t*) param;

    //A manager altal adott status handlert meg kell jegyezni. Ezen keresztul
    //lehet uj allapotot jelenti az eroforras allapotarol.
    this->statusHandler=statusHandler;
    status_t status=kStatus_Success;

    if (this->cfg.initFunc)
    {   //init funkcio meghivasa, mivel van ilyen beallitva
        status=this->cfg.initFunc(this->cfg.callbackData);
    }

    //Megj: Ha a callback nem kStatus_Success-el terne vissza, akkor a modul
    //      hiba allapotot fog felvenni. Ezt a MyRDM Eroforras manager oldja meg

    return status;
}
//------------------------------------------------------------------------------
//Eroforras inditasakor hivott callback
static status_t MyTaskedResource_resource_start(void* param)
{
    taskedResourceExtension_t* this=(taskedResourceExtension_t*) param;

    status_t status=kStatus_Success;

    if (this->cfg.startFunc)
    {   //eroforrast indito funkcio meghivasa, mivel van ilyen beallitva
        status=this->cfg.startFunc(this->cfg.callbackData);
    }

    if (status==kStatus_Success)
    {   //A start callback nem adott hibat (ha egyaltalan volt beregisztralva)
        //Jelzes az eroforrast futtato szalnak, hogy kezdje meg a mukodest.
        xEventGroupSetBits(this->events, MyTASKEDRESOURCE_EVENT__START_REQUEST);
    } else
    {   //Hiba volt a start callback alatt.
        //TODO: mi legyen a strategia???????????????????????????????????????????????????????????
        // kuldunk error statuszt, vagy bovitjuk a MyRDM modult, hogy ilyenkor
        // automatikusan menjen hibara?
    }

    return status;
}
//------------------------------------------------------------------------------
//Eroforras leallitasakor hivott callback
static status_t MyTaskedResource_resource_stop(void* param)
{
    taskedResourceExtension_t* this=(taskedResourceExtension_t*) param;

    status_t status=kStatus_Success;

    if (this->cfg.stopFunc)
    {   //eroforrast indito funkcio meghivasa, mivel van ilyen beallitva
        status=this->cfg.stopFunc(this->cfg.callbackData);
    }

    if (status==kStatus_Success)
    {   //A stop callback nem adott hibat (ha egyaltalan volt beregisztralva)
        //Jelzes az eroforrast futtato szalnak, hogy kezdje meg a mukodest.
        xEventGroupSetBits(this->events, MyTASKEDRESOURCE_EVENT__STOP_REQUEST);
    } else
    {   //Hiba volt a stop callback alatt.
        //TODO: mi legyen a strategia???????????????????????????????????????????????????????????
        // kuldunk error statuszt, vagy bovitjuk a MyRDM modult, hogy ilyenkor
        // automatikusan menjen hibara?
        //A szalat mindenkepen le kell allitani szerintem.
        xEventGroupSetBits(this->events, MyTASKEDRESOURCE_EVENT__STOP_REQUEST);
    }

    return status;
}
//------------------------------------------------------------------------------
static void __attribute__((noreturn)) MyTaskedResource_task(void* taskParam)
{
    resource_t* resource=(resource_t*) taskParam;
    taskedResourceExtension_t* this=(taskedResourceExtension_t*)resource->ext;


    //Eroforras fociklusa...
    while(1)
    {
        status_t status=kStatus_Success;

        //varakozas arra, hogy az eroforrast elinditjak...
        xEventGroupWaitBits(this->events,
                            MyTASKEDRESOURCE_EVENT__START_REQUEST, pdTRUE,
                            pdFALSE,
                            portMAX_DELAY);
        //Eroforras indul...
        if (this->cfg.startingFunc)
        {   //eroforrast indito funkcio meghivasa, mivel van ilyen beallitva
            status=this->cfg.startingFunc(this->cfg.callbackData);
            if (status)
            {
                goto error;
            }
        }

        //Az eroforras elindult. Reportoljuk az eroforras manager fele...
        MyRDM_resourceStatus(RESOURCE_RUN, 0, this->statusHandler);

        //Eroforrast futtato ciklus...
        while(1)
        {
            //Annak ellenorzese, hogy nem volt-e leallitasi kerelem...
            EventBits_t events;
            events=xEventGroupGetBits(this->events);
            if (events & MyTASKEDRESOURCE_EVENT__STOP_REQUEST)
            {   //leallitasi kerelem erkezett.
                //Nem toroljuk az esemeny flaget, hogy a kilepes utan is
                //tovabb tudja billenteni a mukodest.
                //xEventGroupClearBits(this->events, MyTASKEDRESOURCE_EVENT__STOP_REQUEST);
                break;
            }



        } //while(1)



error:  ;
        __NOP();
    } //while(1)

}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
