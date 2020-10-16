//------------------------------------------------------------------------------
//  Resource manager
//
//    File: MyRM.c
//------------------------------------------------------------------------------
#include "MyRM.h"
#include <string.h>

//Ha hasznaljuk a modult, akkor annak valtozoira peldanyt kell kesziteni.
//Az alabbi makrot el kell helyezni valahol a forrasban, peldaul main.c-ben
//MyRM_INSTANCE

static void __attribute__((noreturn)) MyRM_task(void* taskParam);
static void MyRM_addResourceToManagedList(resource_t* resource);
static void MyRM_printResourceInfo(resource_t* resource, bool printDeps);
static void MyRM_incrementRunningResourcesCnt(MyRM_t* rm);
static void MyRM_decrementRunningResourcesCnt(MyRM_t* rm);
static void MyRM_addResourceToProcessReqList(MyRM_t* rm, resource_t* resource);
static void MyRM_deleteResourceFromProcessReqList(MyRM_t* rm, resource_t* resource);
static inline void MyRM_addRequesterToResource(resource_t* resource,
                                               resourceDep_t* dep);
static inline void MyRM_addDependencyToResource(resource_t* resource,
                                                resourceDep_t* dep);
static inline void MyRM_sendNotify(MyRM_t* rm, uint32_t eventBits);
static inline void MyRM_processResource(MyRM_t* rm, resource_t* resource);
static inline void MyRM_processStartRequest(MyRM_t* rm, resource_t* resource);
static inline void MyRM_processStopRequest(MyRM_t* rm, resource_t* resource);
static status_t MyRM_startResourceCore(MyRM_t* rm, resource_t* resource);
static status_t MyRM_stopResourceCore(MyRM_t* rm, resource_t* resource);
static void MyRM_resourceStatusCore(MyRM_t* rm,
                                    resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode);

#define MyRM_NOTIFY__RESOURCE_START_REQUEST     BIT(0)
#define MyRM_NOTIFY__RESOURCE_STOP_REQUEST      BIT(1)
#define MyRM_NOTIFY__RESOURCE_STATUS            BIT(2)


//Az altalanos eroforras felhasznalok (userek) eseteben hasznalhato esemeny
//flagek, melyekkel az altalanos user statusz callbackbol jelzunk a varakozo
//taszk fele.
//Az eroforras elindult
#define GENUSEREVENT_RUN                        BIT(0)
//Az eroforras leallt
#define GENUSEREVENT_STOP                       BIT(1)
//Az eroforrassal hiba van.
#define GENUSEREVENT_ERROR                      BIT(2)
//Az eroforras vegzett (egyszer lefuto folyamatok eseten)
#define GENUSEREVENT_DONE                       BIT(3)
//------------------------------------------------------------------------------
//Eroforras management reset utani kezdeti inicializalasa
void MyRM_init(const MyRM_config_t* cfg)
{
    MyRM_t* this=&myRM;

    //Manager valtozoinak kezdeti torlese
    memset(this, 0, sizeof(MyRM_t));

    //Managerhez tartozo mutex letrehozasa
  #if configSUPPORT_STATIC_ALLOCATION
    this->mutex=xSemaphoreCreateMutexStatic(&this->mutexBuffer);
  #else
    this->mutex=xSemaphoreCreateMutex();
  #endif


    //Eroforras managementet futtato taszk letrehozasa
    if (xTaskCreate(MyRM_task,
                    "ResourceManager",
                    (const configSTACK_DEPTH_TYPE)cfg->stackSize,
                    this,
                    cfg->taskPriority,
                    &this->taskHandle)!=pdPASS)
    {
        ASSERT(0);
    }
}
//------------------------------------------------------------------------------
//Egyetlen eroforras allapotat es hasznaloit irja ki a konzolra
static void MyRM_printResourceInfo(resource_t* resource, bool printDeps)
{

    static const char* resourceStateStrings[]=RESOURCE_STATE_STRINGS;

    //Eroforras nevenek kiirasa. Ha nem ismert, akkor ??? kerul kiirasra
    printf("%12s", resource->resourceName ? resource->resourceName : "???");

    //Eroforras allapotanak kiirasa
    if (resource->state>ARRAY_SIZE(resourceStateStrings))
    {   //illegalis allapot. Ez csak valami sw hiba miatt lehet
        printf("  [???]");
    } else
    {
        printf("  [%s]", resourceStateStrings[resource->state]);
    }

    printf("   D:%d  U:%d Started:%d L.ErrCode:%d",
                                            (int)resource->depCnt,
                                            (int)resource->usageCnt,
                                            (int)resource->started,
                                            (int)resource->lastErrorCode);

    printf("\n");


    if (printDeps)
    {
        printf("              Reqs: ");

        resourceDep_t* requester=resource->firstRequester;
        //Felhasznalok kilistazasa
        while(requester)
        {
            const char* resourceName=
                ((resource_t*)requester->requesterResource)->resourceName;

            if (resourceName!=NULL)
            {
                printf("%s ", resourceName);
            } else
            {
                printf("??? ");
            }
            //allapot kiirasa
            printf("(%d)  ",(int)((resource_t*)requester->requesterResource)->state);
            //Kovetkezo elemre lepunk a listaban
            requester=(resourceDep_t*)requester->nextRequester;
        }

        printf("\n              Deps: ");


        //fuggosegi kerelmezok kilistazasa
        resourceDep_t* dep=resource->firstDependency;
        while(dep)
        {
            const char* depName=
                ((resource_t*)dep->requiredResource)->resourceName;

            if (depName!=NULL)
            {
                printf("%s ", depName);
            } else
            {
                printf("??? ");                
            }
            //allapot kiirasa
            printf("(%d)  ",(int)((resource_t*)dep->requiredResource)->state);
            //Kovetkezo elemre lepunk a listaban
            dep=(resourceDep_t*)dep->nextDependency;
        }

        if (resource->userList.first !=NULL)
        {   //Van az eroforrasnak hasznaloja

            printf("\n              Users: ");

            //felhasznalok kilistazasa
            resourceUser_t* user=(resourceUser_t*)resource->userList.first;
            while(user)
            {
                const char* userName= user->userName;
                if (userName!=NULL)
                {
                    printf("%s", userName);
                } else
                {
                    printf("???");
                }
                //allapot kiirasa
                printf("(%d)  ",(int)user->state);
                //Kovetkezo elemre lepunk a listaban
                user=(resourceUser_t*)user->userList.next;
            }
        }

        printf("\n");
    }
}
//------------------------------------------------------------------------------
//A konzolra irja a szamontartott eroforrasokat, azok allapotait, es azok
//hasznaloit. Debuggolai celra hasznalhato.
void MyRM_printUsages(bool printDeps)
{
    MyRM_t* rm=&myRM;

    vTaskSuspendAll();

    resource_t* resource=rm->resourceList.first;
    printf("---------------------RESOURCE INFO-----------------------------\n");
    while(resource)
    {
        MyRM_printResourceInfo(resource, printDeps);
        //Kovetkezo elelem a listanak
        resource=(resource_t*)resource->nextResource;
    }
    printf(". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .\n");
    printf("Running resources count:%d\n", (int)rm->runningResourceCount);
    printf("---------------------------------------------------------------\n");

    xTaskResumeAll();
}
//------------------------------------------------------------------------------
//Egyedi eroforras letrehozasa, bovitmeny megadasaval
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra.
void MyRM_createResourceExt(resource_t* resource,
                          const resourceFuncs_t* funcs,
                          void* funcsParam,
                          const char* resourceName,
                          void* ext)
{
    memset(resource, 0, sizeof(resource_t));

    resource->state=RESOURCE_STATE_UNKNOWN;
    resource->funcs=funcs;
    resource->funcsParam=funcsParam;
    resource->resourceName=(resourceName==NULL) ? "?" :resourceName;
    resource->ext=ext;

    //Az eroforrast hozzaadjuk a rendszerben levo eroforrasok listajahoz...
    MyRM_addResourceToManagedList(resource);
}
//------------------------------------------------------------------------------
//Eroforras letrehozasa.
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra.
void MyRM_createResource(resource_t* resource,
                          const resourceFuncs_t* funcs,
                          void* funcsParam,
                          const char* resourceName)
{
    MyRM_createResourceExt(resource, funcs, funcsParam, resourceName, NULL);
}
//------------------------------------------------------------------------------
//Eroforras leirojanak lekerdezese az eroforras neve alapjan.
//NULL-t ad vissza, ha az eroforras nem talalhato.
resource_t* MyRM_getResourceByName(const char* name)
{
    resource_t* resource=myRM.resourceList.first;
    while(resource)
    {
        if (resource->resourceName)
        {   //Van beallitva hozza nev
            if (strcmp(resource->resourceName, name)==0)
            {   //Egyezes. Visszaterunk az eroforraslal.
                return resource;
            }
        }
        //Lancolt lista kovetkezo elemere ugrunk
        resource=(resource_t*) resource->nextResource;
    }

    //Az eroforras nem talalhato
    return NULL;
}
//------------------------------------------------------------------------------
//Eroforrashoz igenylo hozzaadasa.
static inline void MyRM_addRequesterToResource(resource_t* resource,
                                               resourceDep_t* dep)
{
    //Az eroforrast igenylok lancolt listajahoz adjuk a fuggosegi leirot...
    if (resource->firstRequester==NULL)
    {   //Meg nincs beregisztralva kerelmezo. Ez lesz az elso.
        resource->firstRequester=dep;
        dep->nextRequester=NULL;
    } else
    {   //Mar van a listanak eleme. Az utolso utan fuzzuk.
        resource->lastRequester->nextRequester=(struct resourceDep_t*)dep;
    }
    //A sort lezarjuk. Ez lesz az utolso.
    dep->nextRequester=NULL;
    resource->lastRequester=dep;
}
//------------------------------------------------------------------------------
//Eroforrashoz tartozo fuggoseg beallitasa. Ebben adhato meg, hogy az
//eroforras melyik masik eroforrasoktol fugg.
static inline void MyRM_addDependencyToResource(resource_t* resource,
                                                resourceDep_t* dep)
{
    //A leiroban beallitjuk az eroforrast, akihez tartozik, akinek a fuggoseget
    //leirjuk.
    dep->requesterResource=(struct resource_t*) resource;

    //Az eroforras lancolt listajahoz adjuk a fuggosegi leirot...
    if (resource->firstDependency==NULL)
    {   //Meg nincs beregisztralva fuggoseg. Ez lesz az elso.
        resource->firstDependency=dep;
        //Dep->PrevDependency=NULL;
    } else
    {   //Mar van a listanak eleme. Az utolso utan fuzzuk.
        resource->lastDependency->nextDependency=(struct resourceDep_t*)dep;
    }
    //A sort lezarjuk. Ez lesz az utolso.
    dep->nextDependency=NULL;
    resource->lastDependency=dep;

    //Az eroforrashoz tartozo fuggosegek szamanak novelese. Ez nem valtozik kesobb.
    resource->depCount++;
    //Az inditasra varo fuggosegek szamanak novelese. Evvel szamoljuk a fuggoben
    //levo inditasok/allitasok szamat.
    resource->depCnt++;
}
//------------------------------------------------------------------------------
//Eroforrasok koze fuggoseg beallitasa (hozzaadasa).
//highLevel: Magasabb szinten levo eroforras, melynek a fuggoseget definialjuk
//lowLevel: Az alacsonyabb szinten levo eroforras, melyre a magasabban levo
//mukodesehez szukseg van (highLevel).
void MyRM_addDependency(resourceDep_t* dep,
                        resource_t* highLevel,
                        resource_t* lowLevel)
{
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);

    //Beallitjuk a leiroban a kerelmezo es a fuggoseget
    dep->requesterResource=(struct resource_t*) highLevel;
    dep->requiredResource =(struct resource_t*) lowLevel;
    MyRM_addDependencyToResource(highLevel, dep);
    MyRM_addRequesterToResource (lowLevel,  dep);

    xSemaphoreGive(rm->mutex);
}
//------------------------------------------------------------------------------
//A rendszerben levo eroforrasok listajahoz adja a hivatkozott eroforras kezelot.
static void MyRM_addResourceToManagedList(resource_t* resource)
{
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);

    if (rm->resourceList.first==NULL)
    {   //Ez lesz az elso eleme a listanak. Lista kezdese
        rm->resourceList.first=resource;
    } else
    {   //Van elotte elem
        rm->resourceList.last->nextResource=(struct resource_t*) resource;
    }

    //Ez lesz az utolso eleme a listanak.
    resource->nextResource=NULL;
    rm->resourceList.last=resource;

    xSemaphoreGive(rm->mutex);
}
//------------------------------------------------------------------------------
//A futo eroforrasok szamanak lekerdezese
uint32_t MyRM_getRunningResourcesCount(void)
{
    uint32_t res;
    portENTER_CRITICAL();
    res=myRM.runningResourceCount;
    portEXIT_CRITICAL();
    return res;
}
//------------------------------------------------------------------------------
//Minden eroforras sikeres leallitasakor hivodo callback funkcio beregisztralasa
void MyRM_register_allResourceStoppedFunc(MyRM_allResourceStoppedFunc_t* func,
                                          void* callbackData)
{
    MyRM_t* rm=&myRM;
    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    rm->allResourceStoppedFunc=func;
    rm->callbackData=callbackData;
    xSemaphoreGive(rm->mutex);
}
//------------------------------------------------------------------------------
//A futo/elinditott eroforrasok szamat noveli
static void MyRM_incrementRunningResourcesCnt(MyRM_t* rm)
{
    rm->runningResourceCount++;
}
//------------------------------------------------------------------------------
//A futo/elinditott eroforrasok szamat csokkenti, ha egy eroforras leallt.
static void MyRM_decrementRunningResourcesCnt(MyRM_t* rm)
{
    //Nem is fut mar elvileg eroforras, megis csokekntik a  futok szamat. Ez hiba.
    //valahol vagy tobbszor csokkentettuk a szamlalot, vagy nem lett novelve.
    ASSERT(rm->runningResourceCount);

    rm->runningResourceCount--;

    if (rm->runningResourceCount==0)
    {   //Az osszes eroforras leallt.
        //jelzes a beregisztralt callbacken keresztul, hogy minden eroforras
        //mukodese leallt.
        //Evvel megoldhato, hogy a vezerles bevarja amig minden
        //folyamat leall.
        if (rm->allResourceStoppedFunc)
        {
            xSemaphoreGive(rm->mutex);
            rm->allResourceStoppedFunc(rm->callbackData);
            xSemaphoreTake(rm->mutex, portMAX_DELAY);
        }
    }
}
//------------------------------------------------------------------------------
//Eroforras hozzaadasa az inditando eroforrasok listajahoz
static void MyRM_addResourceToProcessReqList(MyRM_t* rm, resource_t* resource)
{
    if (rm->processReqList.first==NULL)
    {   //Meg nincs eleme a listanak. Ez lesz az elso.
        rm->processReqList.first=resource;
    } else
    {   //Mar van eleme a listanak. Hozzaadjuk a vegehez.
        rm->processReqList.last->processReqList.next=(struct resource_t*)resource;
    }
    //Az elozo lancszemnek a sorban korabbi legutolso elem lesz megadva
    resource->processReqList.prev=(struct resource_t*) rm->processReqList.last;
    //Nincs tovabbi elem a listaban. Ez az utolso.
    resource->processReqList.next=NULL;
    //Megjegyezzuk, hogy ez az utolso elem a sorban
    rm->processReqList.last=resource;

    //Jelezzuk, hogy az eroforras szerepel a listaban
    resource->processReqList.inTheList=true;
}
//------------------------------------------------------------------------------
//Eroforras torlese az inditando eroforrasok listajabol
static void MyRM_deleteResourceFromProcessReqList(MyRM_t* rm, resource_t* resource)
{
    //A lancolt listaban a torlendo elem elott es utan allo elemre mutatnak
    resource_t* prev= (resource_t*) resource->processReqList.prev;
    resource_t*	next= (resource_t*) resource->processReqList.next;

    if (prev==NULL)
    {   //ez volt az elso eleme a listanak, mivel nincs elotte.
        //Az elso elem ezek utan az ezt koveto lesz.
        rm->processReqList.first=next;
        if (next)
        {   //all utanna a sorban.
            //Jelezzuk a kovetkezonek, hogy o lesz a legelso a sorban.
            next->processReqList.prev=NULL;
        } else
        {
            //Nincs utanna masik elem, tehat ez volt a legutolso elem a listaban
            rm->processReqList.last=NULL;
        }
    } else
    {   //volt elotte a listaban
        if (next)
        {   //Volt utanan is. (Ez egy kozbulso listaelem)
            next->processReqList.prev=(struct resource_t*) prev;
            prev->processReqList.next=(struct resource_t*) next;
        } else
        {   //Ez volt a legutolso elem a listaban
            //Az elozo elembol csinalunk legutolsot.
            prev->processReqList.next=NULL;

            //Az elozo elem a listaban lesz a kezeles szempontjabol a legutolso
            //a sorban.
            rm->processReqList.last=prev;
        }
    }

    //Toroljuk a jelzest, hogy az eroforras szerepel a listaban.
    resource->processReqList.inTheList=false;
    resource->processReqList.prev=NULL;
    resource->processReqList.next=NULL;
}
//------------------------------------------------------------------------------
//Az egyes eroforrasok ezen keresztul jelzik a manager fele az allapotvaltozasa-
//ikat.
//Megj: Ezt a callbacket az eroforrasok a Start vagy Stop funkciojukbol is
//hivhatjak!
void MyRM_resourceStatus(resource_t* resource,
                          resourceStatus_t resourceStatus,
                          status_t errorCode)
{
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    MyRM_resourceStatusCore(rm, resource, resourceStatus, errorCode);
    xSemaphoreGive(rm->mutex);
}
//------------------------------------------------------------------------------
static status_t MyRM_startResourceCore(MyRM_t* rm, resource_t* resource)
{
    status_t status=kStatus_Success;

    //Az inditasi kerelmek szamat noveljuk az eroforrasban.
    //Ez alapjan a taszkban tudni fogjuk, hogy mennyien jeleztek az eroforras
    //hasznalatat.
    resource->startReqCnt++;

    //Eroforras feldolgozasanak eloirasa...
    if (resource->processReqList.inTheList==false)
    {   //Ez eroforras meg nincs a listaban.
        MyRM_addResourceToProcessReqList(rm, resource);
    }

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_START_REQUEST);

    return status;
}
//------------------------------------------------------------------------------
static status_t MyRM_stopResourceCore(MyRM_t* rm, resource_t* resource)
{
    status_t status=kStatus_Success;

    //Leallitasi kerelmek szamat noveljuk az eroforrasban.
    //Ez alapjan a taszkban tudni fogjuk, hogy mennyien mondanak le az eroforras
    //hasznalarol.
    resource->stopReqCnt++;

    //Eroforras feldolgozasanak eloirasa...
    if (resource->processReqList.inTheList==false)
    {   //Ez eroforras meg nincs a listaban.
        MyRM_addResourceToProcessReqList(rm, resource);
    }

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_START_REQUEST);

    return status;
}
//------------------------------------------------------------------------------
status_t MyRM_startResource(resource_t* resource)
{
    status_t status=kStatus_Success;
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    MyRM_startResourceCore(rm, resource);
    xSemaphoreGive(rm->mutex);
    return status;
}
//------------------------------------------------------------------------------
status_t MyRM_stopResource(resource_t* resource)
{
    status_t status=kStatus_Success;
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    MyRM_stopResourceCore(rm, resource);
    xSemaphoreGive(rm->mutex);
    return status;
}
//------------------------------------------------------------------------------
//Taszkot ebreszteni kepes notify event kuldese
static inline void MyRM_sendNotify(MyRM_t* rm, uint32_t eventBits)
{
    xTaskNotify(rm->taskHandle, eventBits, eSetBits);
}
//------------------------------------------------------------------------------

//Eroforras managementet futtato taszk
static void __attribute__((noreturn)) MyRM_task(void* taskParam)
{
    MyRM_t* rm=(MyRM_t*) taskParam;
    while(1)
    {
        //Varakozas ujabb esemenyre
        uint32_t events=0;
        xTaskNotifyWait(0, 0xffffffff, &events, portMAX_DELAY);


        xSemaphoreTake(rm->mutex, portMAX_DELAY);

        //Vegig a feldolgozando eroforrasok listajan...
        resource_t* resource=rm->processReqList.first;
        while(resource)
        {
            //Az eroforrast kiveszi a listabol. (Ez a lista legelso eleme)
            MyRM_deleteResourceFromProcessReqList(rm, resource);

            //eroforras allapotok feldolgozasa...
            MyRM_processResource(rm, resource);

            //kovetkezo eroforrasra allas. (Mindig a lista elso eleme)
            resource=rm->processReqList.first;
        } //while(resource)
        xSemaphoreGive(rm->mutex);


    } //while(1)
}
//------------------------------------------------------------------------------
//Eroforras allapotainak kezelese
static inline void MyRM_processResource(MyRM_t* rm, resource_t* resource)
{
    status_t status;

    if (resource->startReqCnt)
    {   //Van fuggoben inditasi kerelem az eroforrasra. Valaki(k) igenybe
        //akarja(k) venni.
       MyRM_processStartRequest(rm, resource);
    }



    //Kell ellenorizni a fuggosegeket, mert varunk valamelyik elindulasara?
    if (resource->checkDepCntRequest)
    {   //Ellenorzes eloirva. Ellenorizni kell, hogy var-e meg valamelyik
        //fuggosegenek az elindulasara.

        //Keres torlese.
        resource->checkDepCntRequest=false;

        if ((resource->depCnt==0) && (resource->state==RESOURCE_STATE_STARTING))
        {   //Az eroforrasnak mar nincsenek inditasra varo fuggosegei, es az
            //eroforras inditasa elo van irva.
            //Ha van az eroforrasnak start funkcioja, akkor azt meghivja, ha
            //nincs, akkor ugy vesszuk, hogy az eroforras elindult.
            if (resource->funcs->start)
            {   //Indito callback meghivasa.
                //A hivott callbackban hivodhat a MyRM_resourceStatus()
                //fuggveny, melyben azonnal jelezheto, ha az eroforras elindult.
                //Vagy a start funkcioban indithato mas folyamatok (peldaul
                //eventek segitsegevel), melyek majd kesobbjelzik vissza a
                //MyRM_resourceStatusCore() fuggvenyen keresztul az eroforras
                //allapotat.
                //A start funkcio alatt a mutexeket feloldjuk
                resource->started=true;
                xSemaphoreGive(rm->mutex);
                status=resource->funcs->start(resource->funcsParam);
                xSemaphoreTake(rm->mutex, portMAX_DELAY);
                if (status) goto start_error;

            } else
            {   //Nincs start fuggvenye. Generalunk egy "elindult" allapotot.
                //Ennek kiertekelese majd ujra a taszkban fog megtortenni, mely
                //hatasara az eroforrasra varo usreke vagy masik eroforrasok
                //mukodese folytathato.
                MyRM_resourceStatusCore(rm,
                                        resource,
                                        RESOURCE_RUN,
                                        kStatus_Success);
            }
        } else
        {   //Tovabb varakozunk, hogy az osszes fuggosege elinduljon...
            //...
        }
    } //if (resource->checkDepCntRequest)


    if (resource->stopReqCnt)
    {   //Van fuggoben lemondasi kerelem az eroforrasra. Valaki(k) lemond annak
        //hasznalatarol
        MyRM_processStopRequest(rm, resource);
    }


    //Kell ellenorizni a fuggosegeket leallitasra?
    if (resource->checkUsageCntRequest)
    {   //Ellenorzes eloirva. Ellenorizni kell, hogy hasznalja-e meg valaki az
        //eroforrast, vagy ha nem, akkor az leallithato. Vagy ha le van allitva,
        //viszont van ra hasznalati igeny, akkor induljon el.

        //Keres torlese.
        resource->checkUsageCntRequest=false;

        if ((resource->state==RESOURCE_STATE_STOP) ||
            (resource->state==RESOURCE_STATE_UNKNOWN))
        {   //Az eroforras le van allitva, vagy meg nem volt sosom igenybe veve
            if (resource->usageCnt)
            {   //Vannak olyan eroforrasok/userek, melyek hasznalni akarjak az
                //eroforrast. El kell azt inditani.
                //Ez lehet, hogy a leallitasi folyamat kozben erkezett, az
                //eroforrast hasznalni akaro keres miatt novekedett.

                if (resource->state==RESOURCE_STATE_UNKNOWN)
                {   //Az eroforras most lesz eloszor igenybe veve. Anank meg nem
                    //futott le az inicializalo fuggvenye.
                    resource->started=false;

                    //Ha van init funkcio definialva, akkor azt meghivja...
                    if (resource->funcs->init)
                    {
                        //A callback hivasa alatt a mutexeket oldjuk.
                        xSemaphoreGive(rm->mutex);
                        status=resource->funcs->init(resource->funcsParam);
                        xSemaphoreTake(rm->mutex, portMAX_DELAY);
                        if (status)
                        {   //hiba az init alatt.
                            //TODO: kezdeni valamit a hibaval!
                            goto init_error;
                        }
                    }
                }

                //Jelezzuk, hogy az eroforras inditasi allapotba kerult
                resource->state=RESOURCE_STATE_STARTING;

                //Noveljuk a futo eroforrasok szamat a managerben.
                MyRM_incrementRunningResourcesCnt(rm);

                //Az eszkoz fuggosegeiben elo kell irni a hasznalatot.
                //Vegig fut a fuggosegeken, es kiadja azokra az inditasi
                //kerelmet...
                resourceDep_t* dep=resource->firstDependency;
                while(dep)
                {
                    resource_t* depRes=(resource_t*) dep->requiredResource;
                    MyRM_startResourceCore(rm, depRes);

                    //lancolt list akovetkezo elemere allas.
                    dep=(resourceDep_t*)dep->nextDependency;
                }

                //Eloirjuk, hogy ellenorizve legyen az eroforras fuggosegi
                //szamlaloja, es ha az 0 (vagy 0-ra csokkent), akkor inditsa el
                //az eroforrast.
                resource->checkDepCntRequest=true;
                MyRM_addResourceToProcessReqList(rm, resource);
            } else
            {   //Az usageCnt==0. --> Nincs aki hasznalni akarja, es le is van
                //allitva. --> Nincs mit tenni.
            }
        } //if (resource->state==RESOURCE_STATE_STOP)
        else
        if ((resource->state==RESOURCE_STATE_RUN) ||
            (resource->state==RESOURCE_STATE_STARTING))
        {   //Az eroforras el van inditva, vagy eppen indul. Ha viszont az derul
            //ki, hogy nem hasznalja senki, akkor leallitjuk azt.

            if (resource->usageCnt==0)
            {   //Az eroforrast mar nem hasznalja senki. Le kell allitani...

                //Jelezzuk, hogy az eroforras leallasi allapotba kerul
                resource->state=RESOURCE_STATE_STOPPING;

                //Az osszes, az eroforrast igenybevevo magasabb szinten levo
                //erofforras szamara jelezni kell, hogy az eroforras nem
                //hasznalhato. Azokban a fuggosegi szamlalot noveljuk.
                resourceDep_t* requester=resource->firstRequester;
                while(requester)
                {
                    resource_t* reqRes=(resource_t*)requester->requesterResource;
                    if (reqRes->depCnt>=reqRes->depCount)
                    {   //Program hiba! Nem lehet nagyobb a fuggosegi szamlalo,
                        //mint az osszes fuggoseg szama!
                        ASSERT(0);
                    }
                    reqRes->depCnt++;
                    requester=(resourceDep_t*)requester->nextRequester;
                }

                //Ha van az eroforrasnak stop funkcioja, akkor azt meghivja, ha
                //nincs, akkor ugy vesszuk, hogy az eroforras leallt.
                if ((resource->funcs->stop) && (resource->started))
                {   //Leallito callback meghivasa.
                    //A hivott callbackban hivodhat a MyRM_resourceStatus()
                    //fuggveny, melyben azonnal jelezheto, ha az eroforras le-
                    //allt. Vagy a stop funkcioban indithato mas folyamatok
                    //(peldaul eventek segitsegevel), melyek majd kesobbjelzik
                    //vissza a MyRM_resourceStatusCore() fuggvenyen keresztul
                    //az eroforras allapotat.
                    //A stop funkcio alatt a mutexeket feloldjuk
                    xSemaphoreGive(rm->mutex);
                    status=resource->funcs->stop(resource->funcsParam);
                    xSemaphoreTake(rm->mutex, portMAX_DELAY);
                    if (status) goto stop_error;
                } else
                {   //Nincs stop fuggvenye. Generalunk egy "leallt" allapotot.
                    //Ennek kiertekelese majd ujra a taszkban fog megtortenni,
                    //mely hatasara az eroforrasra varo usreke vagy masik
                    //eroforrasok mukodese folytathato.
                    MyRM_resourceStatusCore(rm,
                                            resource,
                                            RESOURCE_STOP,
                                            kStatus_Success);
                }
            } else
            {   //Az eroforrast meg nem allaithatjuk le, mert van aki hasznalja.

                //TODO: Az eroforrasrol lemondo userek fele jelezni kellene, hogy   ******************!!!!!!
                //      ne varakozzanak tovabb az eroforras leallasara. Akinek nem
                //      kell, az mehet tovabb. Az eroforras attol meg tovabb fog
                //      mukodni.
                //...
            } //if (resource->usageCnt==0)
        } //if RUN, STARTING

    } //if (resource->checkUsageCntRequest)

init_error:
start_error:
stop_error:
    ;
}
//------------------------------------------------------------------------------
//Inditasi kerelmek kezelese
static inline void MyRM_processStartRequest(MyRM_t* rm, resource_t* resource)
{
    (void) rm;
    //TODO: hibas eroforrasra kuldott start kerelem kezelese                    ******************!!!!!!

    //Az eroforrast hasznalok szamat annyival noveljuk, amennyi inditasi
    //kerelem futott be az utolso feldolgozas ota...
    resource->usageCnt += resource->startReqCnt;
    //A kerelmek szamat toroljuk, igy az a kovetkezo feldolgozasig az addig
    //befutottakat fogja majd ujra akkumulalni.
    resource->startReqCnt = 0;


    if ((resource->state==RESOURCE_STATE_STOP) ||
        (resource->state==RESOURCE_STATE_UNKNOWN))
    {   //Az eroforras meg nem volt hasznalva. Annak meg nem futott le az
        //inicializalo rutinja, vagy az eroforras meg nincs elinditva, vagy mar
        //egy korabbi futasbol le lett allitva.

        //Eloirjuk, hogy az eroforras usageCnt-t ellenorizze, es ha az nem 0,
        //akkor inditsa el az eroforrast...
        resource->checkUsageCntRequest=true;
    }

    //Ha az eroforras allapota STARTING, akkor nincs mit tenni. Majd ha az
    //osszes fuggosege elindult, akkor ez is el fog indulni. (Az usageCnt
    //viszont novekedett az eroforrast haszanlni akarok szamaval.)

    //Ha az eroforras allapota RUN, akkor az eroforras mar fut. (Az usageCnt
    //viszont novekedett az eroforrast haszanlni akarok szamaval.)
    //TODO: lehet, hogy az eroforrasra varakozo userek szamara itt ki kell      *************** !!!!!!
    //adni majd egy jelzest, hogy azok ne varjanak tovabb az eroforras elindu-
    //lasara.

    //Ha az eroforras STOPPING allapotban van, akkor itt nem csinalunk semmit.
    //(Az usageCnt viszont novekedett az eroforrast haszanlni akarok szamaval.)
    //TODO: az eroforras leallasakor a statusz eszre kellene tudni              ******************!!!!!!
    //venni, hogy az usageCnt nem 0, ezert az eroforrast ujra el kell inditani!
}
//------------------------------------------------------------------------------
//leallitasi kerelmek feldolgozasa
static inline void MyRM_processStopRequest(MyRM_t* rm, resource_t* resource)
{
    (void) rm;

    //TODO: hibas eroforrasra kuldott stop kerelem kezelese!                    ******************!!!!!!

    if (resource->state == RESOURCE_STATE_STOP)
    {   //Az eroforras mar le van allitva.
        //Ha egy user ugy hivott leallitasi kerelmet egy eroforrasra, hogy azt
        //korabban nem vette igenybe, akkor itt kellene kuldeni neki egy
        //eventet, hogy az ne varjon tovabb.
        //TODO: megoldani                                                       ******************!!!!!!
    }
    else
    if ((resource->state == RESOURCE_STATE_RUN) ||
        (resource->state == RESOURCE_STATE_STARTING))
    {   //Az eroforras mar el van indulva, vagy most indul. Van ertelme a
        //leallitasnak.

        //A lemondasok szamaval csokkentjuk az eroforrast hasznalok szamlalojat.
        int newCnt= (int)(resource->usageCnt - resource->stopReqCnt);
        if (newCnt<0)
        {   //Az eroforras kezelesben hiba van. Nem mondhatnanak le tobben,
            //mint amennyien korabban igenybe vettek!
            ASSERT(0);
        }
        resource->usageCnt=(uint32_t)newCnt;

        //A kerelmek szamat nullazzuk, igy abban ujra a kovetkezo kiertekelesig
        //befuto leallitasi kerelmek akkumulalodnak.
        resource->stopReqCnt=0;

        //Eloirjuk a managernek, hogy a taszkban ellenorizze az eroforras
        //usageCnt szamlalojat, es ha az 0-ra csokkent, akkor kezdemenyezze az
        //eroforras leallitasat.
        resource->checkUsageCntRequest=true;
    }


    //Ha az eroforras allapota STOPPING, akkor itt nem csinalunk semmit. Majd
    //a statusz callbackon keresztul elindul a jelzes a leallitast varo
    //userek iranyaba.
}
//------------------------------------------------------------------------------
//Az egyes eroforrasok ezen keresztul jelzik a manager fele az allapotvaltozasa-
//ikat.
static void MyRM_resourceStatusCore(MyRM_t* rm,
                                    resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode)
{
    (void) errorCode;

    switch(resourceStatus)
    {
        //......................................................................
        case RESOURCE_RUN:
            //Az eroforras azt jelzi, hogy elindult
            if (resource->state != RESOURCE_STATE_STARTING)
            {   //Az eroforras ugy kuldte az elindult jelzest, hogy kozben nem
                //is inditasi allapotban van. Ez szoftverhiba!
                ASSERT(0);
            }

            //Futo allapotot allitunk be az eroforrasra.
            resource->state=RESOURCE_STATE_RUN;

            //Az inditasi folyamat alatt viszont elkepzelheto olyan eset, hogy
            //mindenki lemond az eroforras hasznalatarol. Ezt az usageCnt==0
            //jelenti. Ebben az esetben az eroforrast le kell allitani.
            //Eloirjuk, hogy az eroforras tesztelje a hasznalati szamlalojat.
            resource->checkUsageCntRequest=true;

            //Az eroforrast igenylok tudomasara kell hozni, hogy az eroforras
            //elindult. Ezt ugy tesszuk meg, hogy vegighaladva az osszes
            //igenylojen, azokban csokkentjuk a fuggosegi szamlalot (depCnt),
            //tovabba eloirjuk, hogy az igenylokben fusson le a depCnt
            //vizsgalat, igy ha az az adott kerelmezoben 0-ra csokken, akkor az
            //is el tud majd indulni.
            resourceDep_t* requester=resource->firstRequester;
            while(requester)
            {
                resource_t* reqRes=(resource_t*)requester->requesterResource;
                //depCnt csokkentese...
                if (reqRes->depCnt==0)
                {   //Program hiba! Nem lehetne 0 a fuggosegi szamlalo!
                    ASSERT(0);
                }
                reqRes->depCnt--;
                //Eloirjuk a taszkban a fuggosegi szamlalo ellenorzeset
                reqRes->checkDepCntRequest=true;
                //Kerelmezo kiertekelese a taszkban ugy tortenik, hogy azt a
                //kiertekelendok listajahoz adjuk...
                MyRM_addResourceToProcessReqList(rm, reqRes);

                requester=(resourceDep_t*)requester->nextRequester;
            }

            //Jelzes az eroforrasra varo userek szamara, hogy az eroforras
            //elindult. Event.
            //TODO: kidolgozni                                                  *************!!!!!!!!!!!!!!

            break;
        //......................................................................
        case RESOURCE_DONE:
            //Az eroforras azt jelzi, hogy vegzett a feladataval

            break;

        //......................................................................
        case RESOURCE_STOP:
            //Az eroforras azt jelzi, hogy leallt.
            if (resource->state != RESOURCE_STATE_STOPPING)
            {   //Az eroforras ugy kuldte a lealt jelzest, hogy az nem volt
                //leallitasi folyamatban. Ez szoftverhiba!
                ASSERT(0);
            }

            //Leallitott allapotot allitunk be az eroforrasra.
            resource->state=RESOURCE_STATE_STOP;

            //Az eroforras fuggosegeiben lemondjuk a hasznalatot, igy azok ha
            //mar senkinek sem kellenek, leallhatnak
            //Az eszkoz fuggosegeiben elo kell irni a hasznalatot.
            //Vegig fut a fuggosegeken, es kiadja azokra az inditasi
            //kerelmet...
            resourceDep_t* dep=resource->firstDependency;
            while(dep)
            {
                resource_t* depRes=(resource_t*) dep->requiredResource;
                MyRM_stopResourceCore(rm, depRes);

                //Eloirjuk, hogy ellenorizve legyen az eroforras usage
                //szamlaloja, es ha az 0 (vagy 0-ra csokkent), akkor allitsa le
                //az eroforrast.
                depRes->checkUsageCntRequest=true;
                MyRM_addResourceToProcessReqList(rm, depRes);

                //lancolt list akovetkezo elemere allas.
                dep=(resourceDep_t*)dep->nextDependency;
            }

            //A leallitasi folyamat alatt viszont elkepzelheto olyan eset, hogy
            //az eroforrast valaki(k) ujra hasznalatba veszik. Ezt az usageCnt
            //nem 0 erteke jelenti. Ebben az esetben az eroforrast ujra el kell
            //inditani. Ezt a taszkban oldjuk meg.
            //Eloirjuk, hogy az eroforras tesztelje a hasznalati szamlalojat.
            resource->checkUsageCntRequest=true;
            break;

        case RESOURCE_ERROR:
            //Az eroforras hibat jelez

           break;

    }

    //Az eroforras kiertekeleset eloirjuk a taszkban
    MyRM_addResourceToProcessReqList(rm, resource);
    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_STATUS);
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//Az eroforrashoz az applikacio felol hivhato USER hozzaadasa.
//A hasznalok lancolt listajahoz adja az User-t.
void MyRM_addUser(resource_t* resource,
                  resourceUser_t* user,
                  const char* userName)
{
    MyRM_t* rm=&myRM;

    //A lista csak mutexelt allapotban bovitheto!
    xSemaphoreTake(rm->mutex, portMAX_DELAY);

    //Az user-hez ballitja az igenyelt eroforrast, akit kerelmez
    user->resource=resource;

    if (resource->userList.first==NULL)
    {   //Meg nincs beregisztralva hasznalo. Ez lesz az elso.
        resource->userList.first=(struct resourceUser_t*) user;
        user->userList.prev=NULL;
    } else
    {   //Mar van a listanak eleme. Az utolso utan fuzzuk.
        ((resourceUser_t*)resource->userList.last)->userList.next=
                                                (struct resourceUser_t*)user;
    }
    //A sort lezarjuk. Ez lesz az utolso.
    user->userList.next=NULL;
    resource->userList.last=(struct resourceUser_t*)user;


    //Az eroforras a hozzaadas utan keszenleti allapotot mutat. Amig nincs
    //konkret inditasi kerelem az eroforrasram addig ezt mutatja.
    //(Ebbe ter vissza, ha az eroforrast mar nem hasznaljuk, es az eroforras le
    //is allt.)
    user->state=RESOURCEUSERSTATE_IDLE;

    //User nevenek megjegyzese (ez segiti a nyomkovetest)
    user->userName=(userName!=NULL) ? userName : "?";
    xSemaphoreGive(rm->mutex);
}
//------------------------------------------------------------------------------
//Egy eroforrashoz korabban hozzaadott USER kiregisztralasa.
//A hasznalok lancolt listajabol kivesszuk az elemet
void MyRM_removeUser(resource_t* resource, resourceUser_t* user)
{
    MyRM_t* rm=&myRM;
    //A lista csak mutexelt allapotban modosithato!
    xSemaphoreTake(rm->mutex, portMAX_DELAY);

    resourceUser_t* prev=(resourceUser_t*)user->userList.prev;
    resourceUser_t* next=(resourceUser_t*)user->userList.next;


    if ((prev) && (next))
    {   //lista kozbeni elem. All elotet es utana is elem a listaban
        prev->userList.next=(struct resourceUser_t*)next;
        next->userList.prev=(struct resourceUser_t*)prev;
    } else
    if (next)
    {   //Ez a lista elso eleme, es van meg utana elem.
        //A kovetkezo elem lesz az elso.
        resource->userList.first=(struct resourceUser_t*) next;
        next->userList.prev=NULL;
    } else if (prev)
    {   //Ez a lista utolso eleme, es van meg elote elem.
        prev->userList.next=NULL;
        resource->userList.last=(struct resourceUser_t*)prev;
    } else
    {   //Ez volt a lista egyetlen eleme
        resource->userList.last=NULL;
        resource->userList.first=NULL;
    }

    user->userList.next=NULL;
    user->userList.prev=NULL;

    xSemaphoreGive(rm->mutex);
}
//------------------------------------------------------------------------------
//Eroforras igenylese.
//Hatasara a kert eroforras ha meg nins elinditva, elindul, majd az user-hez
//beregisztralt callbacken keresztul jelzi, annak sikeresseget, vagy hibajat.
//Az atadott user struktura bekerul az eroforrast hasznalok lancolt listajaba.
status_t MyRM_useResource(resourceUser_t* user)
{
    status_t status;
    MyRM_t* rm=&myRM;
    resource_t* resource=user->resource;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    switch(user->state)
    {
        case RESOURCEUSERSTATE_IDLE:
        case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
            //Az eroforras inidtasat meg nem kezdemenyezte a felhasznalo.
            //Vagy Az eroforrasnak a leallitasara vagy a folyamat vegere varunk,
            //de kozben jott egy ujboli hasznalati kerelem.

            //Beallitjuk, hogy varunk annak inditasara.
            user->state=RESOURCEUSERSTATE_WAITING_FOR_START;

            //Eroforrast inditjuk.
            status=MyRM_startResourceCore(rm, resource);
            break;

        case RESOURCEUSERSTATE_WAITING_FOR_START:
            //Mar egy korabbi inditasra varunk. Nem inditunk ra ujra
            status=kStatus_Success;
            break;

        default:
            //Ismeretlen allapot.
            status=kStatus_Fail;
            break;
    }

    xSemaphoreGive(rm->mutex);

    return status;
}
//------------------------------------------------------------------------------
//Eroforrasrol lemondas.
//Ha az eroforras mar senki sem hasznalja, akkor az le lessz allitva.
//Az user-hez beregisztralt callbacken keresztul majd vissza fog jelezni, ha az
//eroforras mukodese befejezodott.
status_t MyRM_unuseResource(resourceUser_t* user)
{
    status_t status=kStatus_Success;
    MyRM_t* rm=&myRM;
    resource_t* resource=user->resource;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    switch(user->state)
    {
        case RESOURCEUSERSTATE_RUN:
        case RESOURCEUSERSTATE_WAITING_FOR_START:
            //Az eroforras el van inidtva vagy varunk annak leallasara.
            //Ez utobbi akkor lehet, ha inditasi folyamat kozben megis lemond
            //az user az eroforrasrol.

            //Beallitjuk, hogy varunk annak leallasara
            user->state=RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE;
            user->resourceContinuesWork=false;

            //Eroforrast leallitjuk
            status=MyRM_stopResourceCore(rm, resource
                                         /*&user->resourceContinuesWork*/);

            if (user->resourceContinuesWork)
            {   //Az eroforras tovabb folytatja a mukodeset, mivel mas
                //eroforrasok vagy userek meg hasznaljak.

                //Jeleznunk kell az user fele, hogy reszerol vege az eroforras
                //hasznalatnak.
                if (user->statusFunc)
                {   //olyan jelzest kuldunk az user fele, hogy az eroforras
                    //befejezte a mukodest.
                    //TODO: lehet, hogy itt valami mas jelzes lenne jobb, pl
                    //      hogy sikeres lemondas.
                    user->statusFunc(RESOURCE_DONE,
                                     status,
                                     user->callbackData);
                }
            }
            break;

        case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
            //Meg egy korabbi inditasra varunk.
            if (resource->state==RESOURCE_STATE_ERROR)
            {   //Az eroforras leallitasa hibara futott.
                printf("_________LE KELLENE ALLITANI_______%s\n", resource->resourceName);
                //Eroforrast leallitjuk
                status=MyRM_stopResourceCore(rm, resource
                                             /*&user->resourceContinuesWork*/);
                printf("statusza a leallitasnak: %d\n", status);
            }
            //kilepes, es varakozas tovabba  befejezesre...
            break;

        case RESOURCEUSERSTATE_IDLE:
            //Callback-en keresztul jelezzuk az user fele, hogy az eroforras
            //vegzett. Ez peldaul general user eseten is lenyeges, amikor ugy
            //hivnak meg egy generalUnuse() funkciot, hogy az eroforras el sem
            //volt inditva. Ilyenkor a callbackben visszaadott statusz
            //segitsegevel olyan esemeny generalodik, mely az user-ben a
            //leallasra varakozast triggereli.
            if (user->statusFunc)
            {
                user->statusFunc(RESOURCE_STOP,
                                 status,
                                 user->callbackData);
            }
            break;

        default:
            //Ismeretlen allapot.
            status=kStatus_Fail;
            break;
    }

    xSemaphoreGive(rm->mutex);

    return status;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//Altalanos callback rutin, melyet az altalanos userre varas eseten hasznalunk
static void MyRM_generalUserStatusCallback(resourceStatus_t resourceStatus,
                                            status_t errorCode,
                                            void* callbackData)
{
    generalResourceUser_t* genUser=(generalResourceUser_t*) callbackData;

    //Ha van megadva az altalanos user-hez statusz callback, akkor hibak eseten
    //az meghivasra kerul.
    if (errorCode)
    {   //Volt hiba az userban.
        if (genUser->errorFunc)
        {   //Van beallitva callback
            genUser->errorFunc(errorCode, genUser->callbackData);
        }
    }

    //Elagaz a statusz alapjan
    switch((int)resourceStatus)
    {
        case RESOURCE_RUN:
            //Az eroforras elindult. Jelezzuk a varakozo taszknak.
            xEventGroupSetBits(genUser->events, GENUSEREVENT_RUN);
            break;
        case RESOURCE_STOP:
            //Az eroforras rendben leallt. Jelezzuk a varakozo taszknak.
            xEventGroupSetBits(genUser->events, GENUSEREVENT_STOP);
            break;
        case RESOURCE_DONE:
            //Az eroforras vegzett, es leallt
            //vagy az eroforrasrol az user sikeresen lemondott.
            xEventGroupSetBits(genUser->events, GENUSEREVENT_DONE);
            break;
        case RESOURCE_ERROR:
        default:
            //Az eroforras mukodeseben hiba kovetkezett be.
            //Jelezzuk a varakozo tszaknak. A hibakodot majd az eroforrashoz
            //tartozo lastErrorCode valtozobol olvassuk ki.
            xEventGroupSetBits(genUser->events, GENUSEREVENT_ERROR);
            break;
    }

}
//------------------------------------------------------------------------------
//Az eroforrashoz altalanos user kezelo hozzaadasa. A rutin letrehozza a
//szukseges szinkronizacios objektumokat, majd megoldja az eroforrashoz valo
//regisztraciot.
void MyRM_addGeneralUser(resource_t* resource,
                          generalResourceUser_t* genUser,
                          const char* userName)
{
    //Esemenyflag mezo letrehozasa, melyekre az alkalmazas taszkja varhat.
  #if configSUPPORT_STATIC_ALLOCATION
    genUser->events=xEventGroupCreateStatic(&genUser->eventsBuff);
  #else
    genUser->events=xEventGroupCreate();
  #endif

    //Az altalanos usrekehez tartozo kozos allapot callback lesz beallitva
    genUser->user.statusFunc=MyRM_generalUserStatusCallback;
    genUser->user.callbackData=genUser;

    //A kiejlolt eroforrashoz user hozzaadasa
    MyRM_addUser(resource, &genUser->user, userName);
}
//------------------------------------------------------------------------------
//Torli az usert az eroforras hasznaloi kozul.
//Fontos! Elotte az eroforras hasznalatot le kell mondani!
void MyRM_removeGeneralUser(generalResourceUser_t* generalUser)
{
    MyRM_removeUser(generalUser->user.resource, &generalUser->user);
}
//------------------------------------------------------------------------------
//Eroforras hasznalatba vetele. A rutin megvarja, amig az eroforras elindul,
//vagy hibara nem fut az inditasi folyamatban valami miatt.
status_t MyRM_generalUseResource(generalResourceUser_t* generalUser)
{
    status_t status=kStatus_Success;

    //Eroforras hasznalatba vetele
    status=MyRM_useResource(&generalUser->user);
    if (status) return status;

    //Varakozas, hogy megjojjon az inditasi folyamtrol a statusz.
    //TODO: timeoutot belefejleszteni?
    EventBits_t Events=xEventGroupWaitBits( generalUser->events,
                                            GENUSEREVENT_RUN |
                                            GENUSEREVENT_ERROR,
                                            pdTRUE,
                                            pdFALSE,
                                            portMAX_DELAY);
    if (Events & GENUSEREVENT_RUN)
    {   //elindult az eroforras.

    }

    if (Events & GENUSEREVENT_ERROR)
    {   //Hiba volt az eroforras inditasakor
        //A hibakodot kiolvassuk az eroforrasbol, es avval terunk majd vissza.
        status=generalUser->user.resource->lastErrorCode;

        //eroforras leallitasa, igy biztositva abban a hiba torleset
        /*status=*/ MyRM_unuseResource(&generalUser->user);
        //if (status) return status;

    }

    return status;
}
//------------------------------------------------------------------------------
//Eroforras hasznalatanak lemondasa. A rutin megvarja, amig az eroforras leall,
//vagy hibara nem fut a leallitasi folyamatban valami.
status_t MyRM_generalUnuseResource(generalResourceUser_t* generalUser)
{
    status_t status=kStatus_Success;

    //Eroforras hasznalatanak lemondasa
    status=MyRM_unuseResource(&generalUser->user);
    if (status) return status;

    //Varakozas, hogy megjojjon a leallast jelzo esemeny
    //TODO: timeoutot belefejleszteni?
    EventBits_t events=xEventGroupWaitBits( generalUser->events,
                                            GENUSEREVENT_STOP |
                                            GENUSEREVENT_ERROR |
                                            GENUSEREVENT_DONE,
                                            pdTRUE,
                                            pdFALSE,
                                            portMAX_DELAY);
    //if (events & GENUSEREVENT_STOP)
    //{   //az eroforras rendben leallt
    //
    //}

    if (events & GENUSEREVENT_ERROR)
    {   //Hiba volt az eroforras leallitasakor
        //A hibakodot kiolvassuk az eroforrasbol, es avval terunk majd vissza.
        status=generalUser->user.resource->lastErrorCode;

        //eroforras leallitasa, igy biztositva abban a hiba torleset
        /*status=*/ MyRM_unuseResource(&generalUser->user);
        //if (status) return status;
    }

    return status;
}
//------------------------------------------------------------------------------
