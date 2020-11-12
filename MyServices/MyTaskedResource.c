//------------------------------------------------------------------------------
//  Task-al tamogatott eroforras modul (MyRM resze)
//
//    File: MyTaskedResource.c
//------------------------------------------------------------------------------
#include "MyTaskedResource.h"

#define MyTASKEDRESOURCE_TRACING    1

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

    //Eroforras letrehozasa...    
    static const resourceFuncs_t resourceFuncs=
    {
        .init =MyTaskedResource_resource_init,
        .start=MyTaskedResource_resource_start,
        .stop =MyTaskedResource_resource_stop,
    };

    //Eroforras letrehozasa.
    //Az "ext" mezojeben beallitasra kerul a taszkos mukodeshez szukseges
    //bovitett adatok (tehat amit most hozunk letre)
    MyRM_createResourceExt(resource,
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

    #if MyTASKEDRESOURCE_TRACING
    printf("MyTaskedResource_resource_init(). (%s)\n", this->cfg.name);
    #endif

    status_t status=kStatus_Success;

    if (this->cfg.initFunc)
    {   //init funkcio meghivasa, mivel van ilyen beallitva
        status=this->cfg.initFunc(this->cfg.callbackData);
    }

    //Megj: Ha a callback nem kStatus_Success-el terne vissza, akkor a modul
    //      hiba allapotot fog felvenni. Ezt a MyRM Eroforras manager oldja meg

    return status;
}
//------------------------------------------------------------------------------
//Eroforras inditasakor hivott callback
static status_t MyTaskedResource_resource_start(void* param)
{
    taskedResourceExtension_t* this=(taskedResourceExtension_t*) param;

    #if MyTASKEDRESOURCE_TRACING
    printf("MyTaskedResource_resource_start(). (%s)\n", this->cfg.name);
    #endif

    if (this->cfg.startRequestFunc)
    {   //eroforrast indito funkcio meghivasa, mivel van ilyen beallitva
        this->cfg.startRequestFunc(this->cfg.callbackData);
    }

    //Indito esemeny beallitasa, melyre a taszkban varakozo folyamat elindul
    xTaskNotify(this->taskHandle,
                MyTASKEDRESOURCE_EVENT__START_REQUEST,
                eSetBits);

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Eroforras leallitasakor hivott callback
static status_t MyTaskedResource_resource_stop(void* param)
{
    taskedResourceExtension_t* this=(taskedResourceExtension_t*) param;

    #if MyTASKEDRESOURCE_TRACING
    printf("MyTaskedResource_resource_stop(). (%s)\n", this->cfg.name);
    #endif


    if (this->cfg.stopRequestFunc)
    {   //eroforrast indito funkcio meghivasa, mivel van ilyen beallitva
        this->cfg.stopRequestFunc(this->cfg.callbackData);
    }

    //Leallitasi kerelem esemeny beallitasa, melyre a taszkban a loop ciklus
    //elott minden korben ravizsglava kezdemenyezheto az eroforras leallasa.
    xTaskNotify(this->taskHandle,
                MyTASKEDRESOURCE_EVENT__STOP_REQUEST,
                eSetBits);

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Eroforrast futtato taszk
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
        control.prohibitStop=0;
        control.resourceStopRequest=0;

        //varakozas arra, hogy az eroforrast elinditjak...
        control.events=MyRTOS_waitForNotifyEvents(control.waitedEvents,
                                                  control.waitTime);

        //Az elso futasnal alapertelmezesben nem akad meg az eventekre varasnal,
        //de a start callbackben ez feluldefinialhato.
        control.waitTime=0;

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
        MyRM_resourceStatus(resource, RESOURCE_RUN, status);


        //Eroforrast futtato ciklus...
        while(1)
        {
            if (control.done)
            {   //Az eroforras befejezte a mukodeset. Jelzes a manager fele.
                MyRM_resourceStatus(resource, RESOURCE_DONE, status);

                //A loop funkcio mar nem kerul futtatasra. Kilep a belso
                //ciklusbol, majd a kulos ciklusban ujra a start jelre fog
                //varni.
                break;
            }

            if ((control.resourceStopRequest) && (control.prohibitStop==0))
            {   //leallitasi kerelem van, es mar az applikacio is engedi
                if (this->cfg.stopFunc)
                {   //Van megadva leallitasi funkcio. Meghivjuk...
                    status=this->cfg.stopFunc(this->cfg.callbackData);
                    if (status)
                    {   //hiba volt a leallitas alatt. Hibakezeles...
                        goto error;
                    }
                }

                //Az eroforras leallt. Reportoljuk az eroforras manager fele...
                MyRM_resourceStatus(resource, RESOURCE_STOP, status);

                //kilepes a belso ciklusbol. A kulso ciklusban ujra a start
                //esemenyre fog varakozni.
                break;
            }

            //varakozas a kilepesre, vagy az applikacio altal megszabott
            //valamelyik esemenyre. Ha van megadva ido, es nem jon egyetlen
            //olyan esemeny sem, amit az applikacio var, akkor addig nem lephet
            //ki. Ez alol egyetleb feltetel, ha leallitasi kerelem van, es az
            //applikacio azt engedi.
            uint64_t enterTime=MyRTOS_getTick();
            uint32_t wt=control.waitTime;
            while(1)
            {
                //Esemenyre, vagy timeoutra varas...
                control.events=
                MyRTOS_waitForNotifyEvents(control.waitedEvents |
                                           MyTASKEDRESOURCE_EVENT__STOP_REQUEST,
                                           wt);

                if (control.events & MyTASKEDRESOURCE_EVENT__STOP_REQUEST)
                {   //leallitasi kerelem erkezett. Ezt a kerest eltaroljuk, de
                    //csak akkor juthat ervenyre, ha a futtatott folyamat ezt
                    //engedi.
                    control.resourceStopRequest=true;

                    if (control.prohibitStop==0)
                    {   //az applikacio nem blokkolja a leallitasi kerelmet.
                        //nem var tovabb. A leallitas ervenyre fog jutni.
                        break;
                    }
                }

                //kilepes, ha letet a megszabott ido, vagy jott legalabb egy
                //olyan event, amire varunk
                if ((control.events==0) ||
                    (control.events & control.waitedEvents)) break;

                //<--ha ide jut, akkor varakozni kell meg

                //kiszamoljuk, hogy mennyit kell meg varni...
                uint64_t elapTime= MyRTOS_getTick() - enterTime;
                if (elapTime>=control.waitTime) break;
                wt=(uint32_t)(control.waitTime - elapTime);
            }


            if ((control.resourceStopRequest) && (control.prohibitStop==0))
            {   //leallitasi kerelem van, es mar az applikacio is engedi
                if (this->cfg.stopFunc)
                {   //Van megadva leallitasi funkcio. Meghivjuk...
                    status=this->cfg.stopFunc(this->cfg.callbackData);
                    if (status)
                    {   //hiba volt a leallitas alatt. Hibakezeles...
                        goto error;
                    }
                }

                //Az eroforras leallt. Reportoljuk az eroforras manager fele...
                MyRM_resourceStatus(resource, RESOURCE_STOP, status);
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
            //talalhato MyRTOS_waitForNotifyEvents() fuggeveny varakozhat.

        } //while(1)

        continue;

error:  //<--ide ugrunk hibak eseten

        //hiba eseten meghivodo callback, ha van beregisztralva
        if (this->cfg.errorFunc)
        {
            this->cfg.errorFunc(this->cfg.callbackData, status);
        }

        //Jelzes a manager fele, hogy hiba van.
        MyRM_resourceStatus(resource, RESOURCE_ERROR, status);


        //Hiba eseten a modult le kell allitani, tehat meg kell, hogy hivodjon
        //a STOP kerelme, hogy torlodni tudjon a hiba.

        //Varakozas a stop jelzesre...
        control.events=MyRTOS_waitForNotifyEvents(MyTASKEDRESOURCE_EVENT__STOP_REQUEST,
                                            portMAX_DELAY);

        //Jelzes a manager fele, hogy a modul le lett allitva.
        MyRM_resourceStatus(resource, RESOURCE_STOP, kStatus_Success);
    } //while(1)

}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
