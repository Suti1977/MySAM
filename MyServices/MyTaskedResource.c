//------------------------------------------------------------------------------
//  Task-al tamogatott eroforras modul (MyRDM resze)
//
//    File: MyTaskedResource.c
//------------------------------------------------------------------------------
#include "MyTaskedResource.h"

static status_t MyTaskedResource_resource_init(void* param);
static status_t MyTaskedResource_resource_start(void* param);
static status_t MyTaskedResource_resource_stop(void* param);
static void __attribute__((noreturn)) MyTaskedResource_task(void* taskParam);
//------------------------------------------------------------------------------
//Taszkal tamogatott eroforras letrehozasa
void MyTaskedResource_create(resource_t* resource,
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
static status_t MyTaskedResource_resource_init(void* param)
{
    taskedResourceExtension_t* this=(taskedResourceExtension_t*) param;

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

    if (this->cfg.startRequestFunc)
    {   //eroforrast indito funkcio meghivasa, mivel van ilyen beallitva
        this->cfg.startRequestFunc(this->cfg.callbackData);
    }

    //Indito esemeny beallitasa, melyre a taszkban varakozo folyamat elindul
    xEventGroupSetBits(this->events, MyTASKEDRESOURCE_EVENT__START_REQUEST);

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Eroforras leallitasakor hivott callback
static status_t MyTaskedResource_resource_stop(void* param)
{
    taskedResourceExtension_t* this=(taskedResourceExtension_t*) param;

    if (this->cfg.stopRequestFunc)
    {   //eroforrast indito funkcio meghivasa, mivel van ilyen beallitva
        this->cfg.stopRequestFunc(this->cfg.callbackData);
    }

    //Leallitasi kerelem esemeny beallitasa, melyre a taszkban a loop ciklus
    //elott minden korben ravizsglava kezdemenyezheto az eroforras leallasa.
    xEventGroupSetBits(this->events, MyTASKEDRESOURCE_EVENT__STOP_REQUEST);

    return kStatus_Success;
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

        //Vezerlo valtozok, melyeken keresztul adjuk at a fuggevyneknek a
        //kapott esemeny flageket, de ezen keresztul modosithatjak a callbackek
        //a varakozasi idot, vagy irhatjak elo a varakozasra az eventeket.
        taskedRsource_control_t control;

        //Start kerelemre varakozik a taszk
        control.waitedEvents=MyTASKEDRESOURCE_EVENT__START_REQUEST;
        //Vegtelen ideig
        control.waitTime=portMAX_DELAY;
        control.done=0;

        //varakozas arra, hogy az eroforrast elinditjak...
        control.events=xEventGroupWaitBits(this->events,
                                          control.waitedEvents,
                                          pdTRUE,
                                          pdFALSE,
                                          control.waitTime);
        //Eroforras indul...
        if (this->cfg.startFunc)
        {   //eroforrast indito funkcio meghivasa, mivel van ilyen beallitva
            status=this->cfg.startFunc(this->cfg.callbackData, &control);
            if (status)
            {
                goto error;
            }
        }

        //Az eroforras elindult. Reportoljuk az eroforras manager fele...
        MyRDM_resourceStatus(resource, RESOURCE_RUN, status);

        //Az elso futasnal nem akad meg az eventekre varasnal.
        control.waitTime=0;

        //Eroforrast futtato ciklus...
        while(1)
        {
            if (control.done)
            {   //Az eroforras befejezte a mukodeset. Jelzes a manager fele.
                MyRDM_resourceStatus(resource, RESOURCE_DONE, status);

                //A loop funkcio mar nem kerul futtatasra. Kilep a belso
                //ciklusbol, majd a kulos ciklusban ujra a start jelre fog varni.
                break;
            }

            //Varakozas esemenyre...
            //A kilepesei flagre mindig varakozhat
            control.waitedEvents |= MyTASKEDRESOURCE_EVENT__STOP_REQUEST;

            //varakozas esemenyre, vagy csak idozites...
            control.events=xEventGroupWaitBits(this->events,
                                              control.waitedEvents,
                                              pdTRUE,
                                              pdFALSE,
                                              control.waitTime);

            //Annak ellenorzese, hogy le kell-e allitani az eroforrast...
            if (control.events & MyTASKEDRESOURCE_EVENT__STOP_REQUEST)
            {   //leallitasi kerelem van.

                if (this->cfg.stopFunc)
                {   //Van megadva leallitasi funkcio. Meghivjuk...
                    status=this->cfg.stopFunc(this->cfg.callbackData);
                    if (status)
                    {   //hiba volt a leallitas alatt. Hibakezeles...
                        goto error;
                    }
                }

                //Az eroforras leallt. Reportoljuk az eroforras manager fele...
                MyRDM_resourceStatus(resource, RESOURCE_STOP, status);
                //kilepes a belso ciklusbol. A kulso ciklusban ujra a start
                //esemenyre fog varakozni.
                break;
            }

            //A loop ciklusban ha kell valmire varakozni, akkor majd beallitja.
            //Ha nem modositja a loop callbackben futo folyamat ezt a mezot,
            //akkor a loopbol visszaterve, csak a __STOP_REQUEST eventre fog
            //figyelni, azt is a control.waitTime-ban beallitott ideig.
            control.waitedEvents=0;

            //Eroforras loop funkcio futtatasa.
            //Az atadott control strukturan keresztul a loop-ban futo kod
            //kepes modositani a varakozasi idot, az esemenybiteket, melyekre
            //varni kell. Tovabba megkapja azokat az esemyneket, amiket kuldtek
            //az eroforrasnak.
            if (this->cfg.loopFunc)
            {
                status=this->cfg.loopFunc(this->cfg.callbackData,
                                          &control);

                if (status)
                {   //A vegrehajtas kozben valami hiba tortent. Az eroforrast
                    //hiba allapotba mozgatjuk
                    goto error;
                }
            }

            //A loop elhagyasa utan a "control"-ban be vannak allitva az
            //eroforras altal eloirt esemeny flagek, melyekre a ciklus elejen
            //talalhato xEventGroupWaitBits() fuggeveny varakozhat.

        } //while(1)

        continue;

error:  //<--ide ugrunk hibak eseten

        //hiba eseten meghivodo callback, ha van beregisztralva
        if (this->cfg.errorFunc)
        {
            this->cfg.errorFunc(this->cfg.callbackData, status);
        }

        //Jelzes a manager fele, hogy hiba van.
        MyRDM_resourceStatus(resource, RESOURCE_ERROR, status);


        //Hiba eseten a modult le kell allitani, tehat meg kell, hogy hivodjon
        //a STOP kerelme, hogy torlodni tudjon a hiba.

        //Varakozas a stop jelzesre...
        control.events=xEventGroupWaitBits(this->events,
                                          MyTASKEDRESOURCE_EVENT__STOP_REQUEST,
                                          pdTRUE,
                                          pdFALSE,
                                          portMAX_DELAY);

        //Jelzes a manager fele, hogy a modul le lett allitva.
        MyRDM_resourceStatus(resource, RESOURCE_STOP, kStatus_Success);
    } //while(1)

}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
