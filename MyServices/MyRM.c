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
static void MyRM_startResourceCore(MyRM_t* rm, resource_t* resource);
static void MyRM_stopResourceCore(MyRM_t* rm, resource_t* resource);
static void MyRM_resourceStatusCore(MyRM_t* rm,
                                    resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode);
static void MyRM_addToRequestersDepErrorList(MyRM_t* rm, resource_t* resource);
static inline bool MyRM_resourceIsActive(resource_t* resource);
static void MyRM_signallingUsers(MyRM_t* rm,
                                 resource_t* resource,
                                 bool continueWorking);

#define MyRM_NOTIFY__RESOURCE_START_REQUEST     BIT(0)
#define MyRM_NOTIFY__RESOURCE_STOP_REQUEST      BIT(1)
#define MyRM_NOTIFY__RESOURCE_STATUS            BIT(2)
#define MyRM_NOTIFY__RESOURCE_USE               BIT(3)
#define MyRM_NOTIFY__RESOURCE_UNUSE             BIT(4)

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

    printf("   DC:%d DCnt:%d UCnt:%d Started: %d L.ErrCode:%d (%s)",
                                            (int)resource->depCount,
                                            (int)resource->depCnt,
                                            (int)resource->usageCnt,
                                            (int)resource->started,
                                            (int)resource->errorInfo.errorCode,
                                            ((resource_t*) resource->errorInfo.resource)->resourceName
                                            );

    printf("\n");


    if (printDeps)
    {
        printf("              startReqCnt: %d  stopReqCnt: %d\n", resource->startReqCnt, resource->stopReqCnt);

        if (resource->reportedError)
        {
            resourceErrorInfo_t* ei=resource->reportedError;
            printf("              Error Initiator: %s   ErrorCode: %d  State: %d\n",
                                            ((resource_t*) ei->resource)->resourceName,
                                            ei->errorCode,
                                            ei->resourceState);
        }

        printf("              Reqs: ");

        resourceDep_t* requester=resource->requesterList.first;
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
        resourceDep_t* dep=resource->dependencyList.first;
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
    if (resource->requesterList.first==NULL)
    {   //Meg nincs beregisztralva kerelmezo. Ez lesz az elso.
        resource->requesterList.first=dep;
        dep->nextRequester=NULL;
    } else
    {   //Mar van a listanak eleme. Az utolso utan fuzzuk.
        resource->requesterList.last->nextRequester=(struct resourceDep_t*)dep;
    }
    //A sort lezarjuk. Ez lesz az utolso.
    dep->nextRequester=NULL;
    resource->requesterList.last=dep;
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
    if (resource->dependencyList.first==NULL)
    {   //Meg nincs beregisztralva fuggoseg. Ez lesz az elso.
        resource->dependencyList.first=dep;
        //Dep->PrevDependency=NULL;
    } else
    {   //Mar van a listanak eleme. Az utolso utan fuzzuk.
        resource->dependencyList.last->nextDependency=(struct resourceDep_t*)dep;
    }
    //A sort lezarjuk. Ez lesz az utolso.
    dep->nextDependency=NULL;
    resource->dependencyList.last=dep;

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
    if (resource->processReqList.inTheList)
    {   //Az eroforras mar szerepel a processzalando eroforrasok listajaban.
        //(nem kerul megegyszer a listaba.)
        return;
    }

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

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_STATUS);
}
//------------------------------------------------------------------------------
static void MyRM_startResourceCore(MyRM_t* rm, resource_t* resource)
{
    //Az inditasi kerelmek szamat noveljuk az eroforrasban.
    //Ez alapjan a taszkban tudni fogjuk, hogy mennyien jeleztek az eroforras
    //hasznalatat.
    resource->startReqCnt++;

    //Eroforras feldolgozasanak eloirasa...
    MyRM_addResourceToProcessReqList(rm, resource);
}
//------------------------------------------------------------------------------
static void MyRM_stopResourceCore(MyRM_t* rm, resource_t* resource)
{
    //Leallitasi kerelmek szamat noveljuk az eroforrasban.
    //Ez alapjan a taszkban tudni fogjuk, hogy mennyien mondanak le az eroforras
    //hasznalarol.
    resource->stopReqCnt++;

    //Eroforras feldolgozasanak eloirasa...
    MyRM_addResourceToProcessReqList(rm, resource);
}
//------------------------------------------------------------------------------
void MyRM_startResource(resource_t* resource)
{
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    MyRM_startResourceCore(rm, resource);
    xSemaphoreGive(rm->mutex);

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_START_REQUEST);
}
//------------------------------------------------------------------------------
void MyRM_stopResource(resource_t* resource)
{
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    MyRM_stopResourceCore(rm, resource);
    xSemaphoreGive(rm->mutex);

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_STOP_REQUEST);
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
//true-t ad vissza, ha az eroforras mukodik, vagy el van inditva, vagy all le.
static inline bool MyRM_resourceIsActive(resource_t* resource)
{
    switch(resource->state)
    {
        case RESOURCE_STATE_STARTING:
        case RESOURCE_STATE_RUN:
        case RESOURCE_STATE_STOPPING:
            return true;

        default:
            //mindne mas esetben azt mondja, hogy inaktiv az eroforras
            return false;
    }
}
//------------------------------------------------------------------------------
//Eroforras allapotainak kezelese
static inline void MyRM_processResource(MyRM_t* rm, resource_t* resource)
{
    status_t status;
    bool continueWorking=false;

volatile static resource_t* rrrrr; rrrrr=resource;

    //Annak ellenorzese, hogy valamely fuggosegeben eloallt-e (ujabb) hiba.
    //A hibas fuggosegeket jelzi az eroforras szamara.
    while(resource->depErrorList.first)
    {
        resourceDep_t* dep=resource->depErrorList.first;
        //A soron kovetkezo hibara futott eroforrasra mutat.
        resource_t* faultyDepResource= (resource_t*)dep->requiredResource;
        //A feldologozott eroforras kovetkezo  hibas fuggosegere allas.
        //A feldolgozasra varo listaelem torlodik a listabol.
        resource->depErrorList.first=(resourceDep_t*)(dep->nextDepError);
        dep->nextDepError=NULL;

        printf("  DEP ERROR. %s --> %s\n", faultyDepResource->resourceName, resource->resourceName);

        //Annak ellenorzese, hogy az eroforras mar kapott-e valamely
        //fuggosegetol hibat, melyet nem nyomott el. Ezt onnan tudni, hogy a
        //reportedError pointere NULL vagy nem. Ha nem NULL, akkor mar egy
        //korabbi fuggosegi hiba miatt az eroforras hibara lett allitva
        if (resource->reportedError)
        {   //Az eroforras mar kapott fuggosegi hibat.
            //Ezt a hibajelzest eldobja.
            continue;
        }

        //A hibat fel kell venni, ha csak nem filterezi le a depError funkcio.

        //ha true, akkor a fuggoseg hibajat nem reportoljuk tovabb.
        bool ignoreError=false;

        if (resource->funcs->depError)
        {   //van hiba jelzo callback beregisztralva. Jelezzuk a fuggoseg
            //hibajat.
            //Ebben a callbackben lehetoseg van a hibak szuresere...
            xSemaphoreGive(rm->mutex);
            resource->funcs->depError(&faultyDepResource->errorInfo,
                                      &ignoreError,
                                      resource->funcsParam);
            xSemaphoreTake(rm->mutex, portMAX_DELAY);
        }

        if (ignoreError)
        {   //a hibat el kell nyomni. Nem adjuk tovabb az eroforrast
            //hasznalok fele.

        } else
        {   //Az eroforras maga is hibara fut. A hibat atveszi a fuggosegetol.            
            resource->reportedError=faultyDepResource->reportedError;

            printf("  do it.\n");

            //Itt elvileg mindenkepen kell, hogy hibat mutasson a reportedError!
            ASSERT(resource->reportedError);

            //A kerelmezok hibalistajahoz adjuk a most hibara futtatott
            //fuggoseget, igy azokban a hiba tovabb terjedhet.
            MyRM_addToRequestersDepErrorList(rm, resource);

            //Az eroforras ha mukodik, akkor annak mukodeset le kell allitani.
            if (MyRM_resourceIsActive(resource) && (resource->started))
            {
                //Az eroforras felveszi a kenyszeritett leallitas allapotat
                resource->state=RESOURCE_STATE_HALTING;

                //Az osszes, az eroforrast igenybevevo magasabb szinten levo
                //erofforras szamara jelezni kell, hogy az eroforras nem
                //hasznalhato. Azokban a fuggosegi szamlalot noveljuk.
                resourceDep_t* requester=resource->requesterList.first;
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

                //Ha a hibara futtatott eroforrasnak van krnxszeritett leallito
                //fuggveny, akkor azt meghivja
                if (resource->funcs->halt)
                {
                    xSemaphoreGive(rm->mutex);
                    resource->funcs->halt(resource->funcsParam);
                    xSemaphoreTake(rm->mutex, portMAX_DELAY);
                } else
                {   //Az eroforrashoz nincs regisztralva kenyszeritett leallitas
                    //callback. Ugy vesszuk, hogy az eroforras kenyszer leallt.
                    resource->state=RESOURCE_STATE_HALTED;

                    //Eroforrason eloirjuk, hogy tesztelje a hasznalati szamla-
                    //lojat (usegeCnt-t), mert ha az 0, akkor az eroforarst mar
                    //nem hasznalja mas, azt le lehet allitani, es jelezheto a
                    //fuggosegei fele is, hogy lemondtunk roluk.
                    //(Ha a hibas eroforarsig eljut a lemondasi hullam, akkor az
                    //is le tud majd allni, es alaphelyzetbe allhat.)
                    resource->checkUsageCntRequest=true;
                }
            } //f (MyRM_resourceIsActive(resource) && (resource->started))
        } //if (ignoreError) else
    } //while(dep)




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
                resource->started=true;             //TODO: mi van, ha hibaval jon vissza a start? a started flaggel mit kezdjunk?
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


    //Kell ellenorizni a fuggosegeket leallitasra, vagy inditasra?
    if (resource->checkUsageCntRequest)
    {   //Ellenorzes eloirva. Ellenorizni kell, hogy hasznalja-e meg valaki az
        //eroforrast, vagy ha nem, akkor az leallithato. Vagy ha le van allitva,
        //viszont van ra hasznalati igeny, akkor induljon el.

        //Ellenorzesi kerelem torlese.
        resource->checkUsageCntRequest=false;

        if ((resource->state==RESOURCE_STATE_STOP) ||
            (resource->state==RESOURCE_STATE_STOPPING) ||
            (resource->state==RESOURCE_STATE_UNKNOWN))                                // +-+-+-
        {   //Az eroforras le van allitva, vagy eppen all le, vagy meg nem volt
            //sosem igenybe veve            

            if (resource->usageCnt)
            {   //Vannak olyan eroforrasok/userek, melyek hasznalni akarjak az
                //eroforrast. El kell azt inditani.
                //Ez is lehet, hogy a leallitasi folyamat kozben erkezett, az
                //eroforrast hasznalni akaro keres, a szamlal amiatt novekedett.

                //Noveljuk a futo eroforrasok szamat a managerben.
                MyRM_incrementRunningResourcesCnt(rm);

                if (resource->state==RESOURCE_STATE_UNKNOWN)
                {   //Az eroforras most lesz eloszor igenybe veve. Annak meg nem
                    //futott le az inicializalo fuggvenye.
                    resource->started=false;

                    //Jelezzuk, hogy az eroforras inditasi allapotba kerult
                    resource->state=RESOURCE_STATE_STARTING;

                    //Ha van init funkcio definialva, akkor azt meghivja...
                    if (resource->funcs->init)
                    {
                        //A callback hivasa alatt a mutexeket oldjuk.
                        xSemaphoreGive(rm->mutex);
                        status=resource->funcs->init(resource->funcsParam);
                        xSemaphoreTake(rm->mutex, portMAX_DELAY);
                        if (status)
                        {   //hiba az init alatt.                            
                            goto init_error;
                        }
                    }
                } else
                {
                    //Jelezzuk, hogy az eroforras inditasi allapotba kerult
                    resource->state=RESOURCE_STATE_STARTING;
                }

                //Az eszkoz fuggosegeiben elo kell irni a hasznalatot.
                //Vegig fut a fuggosegeken, es kiadja azokra az inditasi
                //kerelmet...
                resourceDep_t* dep=resource->dependencyList.first;
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
            {   //Az usageCnt==0.

                if (resource->state==RESOURCE_STATE_STOPPING)                        // +-+-+-
                {   //Az eroforras meg leallitasi allapotban van.

                    //Az eroforras fuggosegeiben lemondjuk a hasznalatot, igy
                    //azok ha mar senkinek sem kellenek, leallhatnak...
                    resourceDep_t* dep=resource->dependencyList.first;
                    while(dep)
                    {
                        resource_t* depRes=(resource_t*) dep->requiredResource;
                        MyRM_stopResourceCore(rm, depRes);

                        //Eloirjuk, hogy ellenorizve legyen az eroforras usage
                        //szamlaloja, es ha az 0 (vagy 0-ra csokkent), akkor
                        //allitsa le az eroforrast.
                        depRes->checkUsageCntRequest=true;
                        MyRM_addResourceToProcessReqList(rm, depRes);

                        //lancolt list akovetkezo elemere allas.
                        dep=(resourceDep_t*)dep->nextDependency;
                    }

                    //Az eroforrast hasznalo userek fele torteno jelzes eloirasa.
                    resource->signallingUsers=true;

                    //Az eroforras ettol kezdve jelezheti, hogy leallt.
                    resource->state=RESOURCE_STATE_STOP;

                    //Csokkenteheto a futo eroforrasok szamlaloja.
                    MyRM_decrementRunningResourcesCnt(rm);
                }
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
                resourceDep_t* requester=resource->requesterList.first;
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
                    resource->started=false;
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

                //Az eroforrasrol lemondo userek fele jelezni kellene, hogy
                //ne varakozzanak tovabb az eroforras leallasara. Akinek nem
                //kell, az mehet tovabb. Az eroforras attol meg tovabb fog
                //mukodni.
                continueWorking=true;
                resource->signallingUsers=true;

            } //if (resource->usageCnt==0)
        } //if RUN, STARTING
        else
        if (resource->state==RESOURCE_STATE_HALTED)
        {   //Az eroforras egy kenyszeritett leallas utani allapotban van.

            if (resource->usageCnt==0)
            {   //Az eroforrast mar nem hasznalja senki. Fel kell venni a leallt
                //allapotot.

                //Az osszes, az eroforrast igenybevevo magasabb szinten levo    EZT MAR HALTING-KOR KI KELLENE ADNI!
                //erofforras szamara jelezni kell, hogy az eroforras nem        HISZEN ATTOL KEZDVE AZOK MAR NEM HASZNALHATOK!
                //hasznalhato. Azokban a fuggosegi szamlalot noveljuk.
                //  resourceDep_t* requester=resource->requesterList.first;
                //  while(requester)
                //  {
                //      resource_t* reqRes=(resource_t*)requester->requesterResource;
                //      if (reqRes->depCnt>=reqRes->depCount)
                //      {   //Program hiba! Nem lehet nagyobb a fuggosegi szamlalo,
                //          //mint az osszes fuggoseg szama!
                //          ASSERT(0);
                //      }
                //      reqRes->depCnt++;
                //      requester=(resourceDep_t*)requester->nextRequester;
                //  }
                //^----ezt atraktam a halting allapotba allashoz.

                //Az eszkoz leallt allapotot vehet fel.
                resource->state=RESOURCE_STATE_STOP;

                //Az eroforrast hasznalo userek fele jelezni kell, hogy az
                //eroforras lealt.
                resource->signallingUsers=true;
            }
            //  else                                                            Lehet olyan, hogy ez kell????
            //  {   //Az eroforrast meg nem allaithatjuk le, mert van aki hasznalja.
            //
            //      //Az eroforrasrol lemondo userek fele jelezni kellene, hogy
            //      //ne varakozzanak tovabb az eroforras leallasara. Akinek nem
            //      //kell, az mehet tovabb. Az eroforras attol meg tovabb fog
            //      //mukodni.
            //      continueWorking=true;
            //      resource->signallingUsers=true;
            //
            //  } //if (resource->usageCnt==0)
        } //if RESOURCE_STATE_HALTING

    } //if (resource->checkUsageCntRequest)

exit:
    if (resource->signallingUsers)
    {   //Az eroforrast hasznalo userek fele elo van irva a jelzes
        //kuldes, az eroforras allapotarol.

        //Jelzesi kerelem torlese
        resource->signallingUsers=false;

        MyRM_signallingUsers(rm, resource, continueWorking);
    }

    return;

init_error:
start_error:
stop_error:
    //Hibakoddal jelzes generalasa. Ez magaval vonja, hogy jelzesre kerul a
    //hiba az eroforrast
    MyRM_resourceStatusCore(rm, resource, RESOURCE_ERROR, status);
    goto exit;
}
//------------------------------------------------------------------------------
//Inditasi kerelmek kezelese
static inline void MyRM_processStartRequest(MyRM_t* rm, resource_t* resource)
{
    (void) rm;

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
    else
    //  if (resource->state==RESOURCE_STATE_ERROR)
    //  {   //Az eroforras hiba allapotban van. Az inditast igenylo user fele
    //      //jelezni kell, hogy az eroforras hibas.
    //      resource->signallingUsers=true;
    //  }
    //  else
    if (resource->state==RESOURCE_STATE_RUN)
    {   //Az eroforras mar el van inditva.
        //Jelezni kell az eroforarst ujonnan igenybe vevo user(ek) fele, hogy
        //az eroforras mar mukodeik. Arra ne varjanak tovabb.
        resource->signallingUsers=true;
    }

    //Ha az eroforras allapota STARTING, akkor nincs mit tenni. Majd ha az
    //osszes fuggosege elindult, akkor ez is el fog indulni. (Az usageCnt
    //viszont novekedett az eroforrast haszanlni akarok szamaval.)

    //Ha az eroforras STOPPING allapotban van, akkor itt nem csinalunk semmit.
    //(Az usageCnt viszont novekedett az eroforrast haszanlni akarok szamaval.)
    //Az eroforras leallasakor majd eszre fogja venni, hogy az usageCnt nem 0,
    //ezert az eroforrast ujra el fogja inditani.
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

        //Elemezve a szituaciot, ilyen normal mukodes eseten nem lehetseges.
        //Az userekben meg van oldva, hogy ha mar meghivtak egy leallitasi
        //parancsot, akkor arra ujabb leallitasi parancsot ne tudjanak hivni.
        ASSERT(0);
    }
    else
    if ((resource->state == RESOURCE_STATE_RUN) ||
        (resource->state == RESOURCE_STATE_STARTING))                              // +-+-+-
    {   //Az eroforras mar el van indulva, vagy most indul. Van ertelme a
        //leallitasnak.
        //De hiba eseten is hivhato. Ilyenkor a hiba ugy torlodik

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
    if (resource->state==RESOURCE_STATE_UNKNOWN)
    {   //Amig az eroforras nincs inicializalva, addig nem fogadhat statuszt!
        ASSERT(0);
    }

    //Ha az erorCode hibat jelez, akkor hibara visszuk az eroforrast
    if (errorCode) resourceStatus=RESOURCE_ERROR;

    switch(resourceStatus)
    {
        //......................................................................
        case RESOURCE_RUN:
            //Az eroforras azt jelzi, hogy elindult

            //if (resource->state == RESOURCE_STATE_ERROR)
            //{   //Az eroforras korabban hibara futott. Ez az elindult jelzes
            //    //nem juthat ervenyre.
            //    //
            //    //Ilyen szituacio pl, ha egy fuggosege hibat jelez, melyet nem
            //    //nyom el az eroforras a depError fuggvenyeben, es kozben tovabb
            //    //fojtatja az indulasi procedurat, mely vegen RUN jelzest ad.
            //    //Ezt a RUN jelzest akkor adja ki, amikor az eroforras mar
            //    //korabban beallitotta magara nezve az ERROR statuszt.
            //    break;
            //}

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
            resourceDep_t* requester=resource->requesterList.first;
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

            //Az eroforrast hasznalo userek fele torteno jelzes eloirasa.
            //A jelzes igy a manager taszkjaban fut majd le.
            resource->signallingUsers=true;

            break;
        //......................................................................
        case RESOURCE_DONE:
            //Az eroforras azt jelzi, hogy vegzett a feladataval
            //TODO: kidolgozni!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            break;

        //......................................................................
        case RESOURCE_STOP:
            //Az eroforras azt jelzi, hogy leallt.
            if (resource->state != RESOURCE_STATE_STOPPING)
            {   //Az eroforras ugy kuldte a lealt jelzest, hogy az nem volt
                //leallitasi allapotban.
                //Ez szoftverhiba!
                ASSERT(0);
            }

            //Eloirjuk, hogy az eroforras tesztelje a hasznalati szamlalojat.
            //Ha az tenyleg 0-ra csokken, akkor az eroforars leallithato.
            resource->checkUsageCntRequest=true;

            break;
        //......................................................................
        case RESOURCE_ERROR: {
            //Az eroforras hibat jelez

            //Hiba informacios struktura feltoltese, melyet majd kesobb az
            //eroforarst hasznalo masik eroforarsok, vagy userek fele
            //tovabb adhatunk...
            //Reportoljuk, hogy melyik eroforarssal van a hiba
            resource->errorInfo.resource=(struct resource_t*)resource;
            //informaljuk, hogy mi a hibakod
            resource->errorInfo.errorCode=errorCode;
            //az aktualis modul allapotot is reportoljuk
            resource->errorInfo.resourceState=resource->state;
            //TODO: itt lehetne esteleg egy hiba queu-t is letrehozni, ahol a
            //      korabbi hibak is lathatok.

            //Hiba figyelmen kivul hagyasanak lehetosege. (ha true)
            bool ignoreError=false;

            //Ha az eroforrasnak van sajat error callbackje, akkor azt meghivja.
            //Megj: az error callbackje a sajat szaljaban fut le.
            if (resource->funcs->error)
            {
                xSemaphoreGive(rm->mutex);
                resource->funcs->error(&resource->errorInfo,
                                       &ignoreError,
                                       resource->funcsParam);
                xSemaphoreTake(rm->mutex, portMAX_DELAY);
            }

            if (ignoreError==true)
            {   //a hibat elnyomjuk.

            } else
            {   //A hibaval kezdeni kell valamit.

                //Az eroforrasban azonnal kivaltjuk a hiba miatti intezkedeseket
                resource->state=RESOURCE_STATE_HALTING;

                //Ha meg nincs tovabb ripoltolhato hiba, akkor ez lesz az, mely
                //a fentebbi szintek fele jelzesre kerul.
                if (resource->reportedError==NULL)
                {
                    resource->reportedError=&resource->errorInfo;
                }

                //Meghivasra kerul az eroforras kenyszeritett leallito
                //callbackje.
                if (resource->funcs->halt)
                {
                    xSemaphoreGive(rm->mutex);
                    resource->funcs->halt(resource->funcsParam);
                    xSemaphoreTake(rm->mutex, portMAX_DELAY);
                }

                //Eroforrason eloirjuk, hogy tesztelje a hasznalati szamlalojat
                //(usegeCnt-t), mert ha az 0, akkor az eroforarst mar nem
                //hasznalja mas, azt le lehet allitani, es jelezheto a
                //fuggosegei fele is, hogy lemondtunk roluk.
                //Ha a hibas eroforarsig eljut a lemondasi hullam, akkor az is
                //le tud majd allni, es alaphelyzetbe allhat.
                resource->checkUsageCntRequest=true;

                //Jelezzuk a hibat az eroforrast hasznalok szamara...
                MyRM_addToRequestersDepErrorList(rm, resource);

                //Az eroforrast hasznalo userek fele torteno jelzes eloirasa.
                //A jelzes igy a manager taszkjaban fut majd le.
                //resource->signallingUsers=true;                               ?????????????????
            }

            break;
            }
        //......................................................................
        case RESOURCE_HALTED:
            //Az eroforras kenyszer leallitasa megtortent

            //Felvesszi az eroforras a kenyszer leallitott allapotot
            resource->state=RESOURCE_STATE_HALTED;

            //Eroforrason eloirjuk, hogy tesztelje a hasznalati szamlalojat
            //(usegeCnt-t), mert ha az 0, akkor az eroforarst mar nem
            //hasznalja mas, azt le lehet allitani, es jelezheto a
            //fuggosegei fele is, hogy lemondtunk roluk.
            //Ha a hibas eroforarsig eljut a lemondasi hullam, akkor az is
            //le tud majd allni, es alaphelyzetbe allhat.
            resource->checkUsageCntRequest=true;
            break;
            //......................................................................
    } //switch

    //Az eroforras kiertekeleset eloirjuk a taszkban
    MyRM_addResourceToProcessReqList(rm, resource);
}
//------------------------------------------------------------------------------
//Egy eroforrast hasznalo magasabb szinten levo eroforrasok hibalistajhoz adja
//az eroforrast.
static void MyRM_addToRequestersDepErrorList(MyRM_t* rm, resource_t* resource)
{
    //Vegighalad az eroforarst hasznalok lancolt listajan...
    resourceDep_t* requesterDep=resource->requesterList.first;
    while(requesterDep)
    {
        //A kovetkezo, a hibas eroforrast hasznalo eroforras
        resource_t* requester=(resource_t*)requesterDep->requesterResource;

        //A hibazo eroforras hozzaadasa az ot hasznalo hibalistajahoz
        requesterDep->nextDepError=NULL;
        if (requester->depErrorList.first==NULL)
        {   //A soron levo hasznalonak meg ures a hibalistaja. Ez lesz az
            //elso fuggosegi leiro a sorban.
            requester->depErrorList.first=requesterDep;
        } else
        {   //A lista mar nem ures. A vegere tesszuk.
            requester->depErrorList.last->nextDepError=
                                    (struct resourceDep_t*)requesterDep;
        }
        //A most hozzaadot fuggosegi kapcsolat lesz az utolso a sorban
        requester->depErrorList.last=requesterDep;

        //A bejegyzett eroforrasokon le kell futtatni a hibakezelest!
        //(Onnan lehet tudni, egy eroforras eseten, hogy valamelyik
        //eroforrasa hibat jelez, hogy a depErrorList.first nem NULL)
        MyRM_addResourceToProcessReqList(rm, requester);

        //A hibara futott eroforrast hasznalo kovetkezo igenylore ugrik.
        requesterDep=(resourceDep_t*) requesterDep->nextRequester;
    }
}
//------------------------------------------------------------------------------
//Az eroforrast hasznalo userek fele az eroforras allapotanak jelzese.
static void MyRM_signallingUsers(MyRM_t* rm,
                                 resource_t* resource,
                                 bool continueWorking)
{
    (void) rm;
    resourceUser_t* user=(resourceUser_t*)resource->userList.first;

    for(;user; user=(resourceUser_t*)user->userList.next)
    {
        if (user->state==RESOURCEUSERSTATE_IDLE)
        {   //Amig nincs hasznalatban az user reszerol az eroforras, addig
            //nem kaphat jelzest.
            //Ilyen az, amig nincs elinditva az eroforras, vagy ebbe az
            //allapotba kerul, ha a hasznalt eroforras leallt.
            continue;
        }


        if ((resource->state==RESOURCE_STATE_HALTING) ||
            (resource->state==RESOURCE_STATE_HALTED))
        {   //Hiba van az eroforrasal. Az eppen a kenyszeritett leallitasi
            //allapotban van, vagy mar le is allt.

            //Ha az user meg nincs hiba allapotban, akkor atadjuk neki a
            //hibat.
            if (user->state != RESOURCEUSERSTATE_ERROR)
            {
                //Az user hibas allapotot vesz fel.
                user->state = RESOURCEUSERSTATE_ERROR;

                //Hiba jelzese az usernek a statusz callbacken keresztul
                if (user->statusFunc)
                {
                    //xSemaphoreGive(rm->mutex);
                    user->statusFunc(   RESOURCE_ERROR,
                                        &resource->errorInfo,
                                        user->callbackData);
                    //xSemaphoreTake(rm->mutex, portMAX_DELAY);
                }
            }
            continue;
        }


        //Elagazas az user aktualis allapta szerint...
        switch(user->state)
        {
            case RESOURCEUSERSTATE_WAITING_FOR_START:
                //Az user varakozik az inditasi jelzesre
                if (resource->state==RESOURCE_STATE_RUN)
                {   //...es az eroforras most elindult jelzest kapott.
                    user->state=RESOURCEUSERSTATE_RUN;
                    //Jelezes az usernek, a callback fuggvenyen
                    //keresztul.
                    if (user->statusFunc)
                    {
                        //xSemaphoreGive(rm->mutex);
                        user->statusFunc(RESOURCE_RUN,
                                         &resource->errorInfo,
                                         user->callbackData);
                        //xSemaphoreTake(rm->mutex, portMAX_DELAY);
                    }
                }
                break;

            case RESOURCEUSERSTATE_RUN:
                //Az user meg fut, vagy
                if (resource->state==RESOURCE_STATE_STOP)
                {   //...es az eroforras most leallt vagy vegzett.
                    user->state=RESOURCEUSERSTATE_IDLE;

                    //Jelezes az usernek a statusz callbacken keresztul
                    if (user->statusFunc)
                    {
                        //xSemaphoreGive(rm->mutex);
                        user->statusFunc(RESOURCE_STOP,
                                         &resource->errorInfo,
                                         user->callbackData);
                        //xSemaphoreTake(rm->mutex, portMAX_DELAY);
                    }
                }
                break;

            case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
                //Az user varakozik az eroforras leallt vagy vegzett
                //jelzesre. Ez utobbi eseten is STOP allapotba megy az
                //eroforras.

                if ((resource->state==RESOURCE_STATE_STOP) ||
                    (resource->state=RESOURCE_STATE_RUN) & (continueWorking))
                {   //Az eroforras leallt, vagy tovabb mukodeik, mert
                    //egy vagy tobb masik eroforrasnak szuksege van ra.
                    //Ez utobbi esetben a leallast varo user fele
                    //jelezheto a leallt allapot. Igy az befejezheti a
                    //mokodeset.
                    user->state=RESOURCEUSERSTATE_IDLE;

                    //Jelezes az usernek a statusz callbacken keresztul
                    if (user->statusFunc)
                    {
                        //xSemaphoreGive(rm->mutex);
                        user->statusFunc(RESOURCE_STOP,
                                         &resource->errorInfo,
                                         user->callbackData);
                        //xSemaphoreTake(rm->mutex, portMAX_DELAY);
                    }
                }

                break;

            default:
                break;
        } //switch(user->state)
    } //while
}
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
//Eroforras hasznalata.
//Hatasara a kert eroforras ha meg nins elinditva, elindul, majd az user-hez
//beregisztralt callbacken keresztul jelzi, annak sikeresseget, vagy hibajat.
//Az atadott user struktura bekerul az eroforrast hasznalok lancolt listajaba.
void MyRM_useResource(resourceUser_t* user)
{
    MyRM_t* rm=&myRM;
    resource_t* resource=user->resource;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);

    //TODO: mit tegyunk, ha hiba allapotban van az user?????????????????????????????????????????????????????????????????????

    switch(user->state)
    {
        case RESOURCEUSERSTATE_IDLE:
        case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
            //- Az eroforras inidtasat meg nem kezdemenyezte a felhasznalo.
            //- Vagy Az eroforrasnak a leallitasara vagy a folyamat vegere
            //  varunk, de kozben jott egy ujboli hasznalati kerelem.

            //Beallitjuk, hogy varunk annak inditasara.
            user->state=RESOURCEUSERSTATE_WAITING_FOR_START;

            //Eroforrast inditjuk.
            MyRM_startResourceCore(rm, resource);
            break;

        case RESOURCEUSERSTATE_WAITING_FOR_START:
        case RESOURCEUSERSTATE_RUN:
            //Mar egy korabbi inditasra varunk, vagy mar mukodik az eroforras.
            //Nem inditunk ra ujra.
            break;

        default:
            //Ismeretlen allapot.
            ASSERT(0);
            break;
    }

    xSemaphoreGive(rm->mutex);

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_USE);
}
//------------------------------------------------------------------------------
//Eroforrasrol lemondas.
//Ha az eroforras mar senki sem hasznalja, akkor az le lessz allitva.
//Az user-hez beregisztralt callbacken keresztul majd vissza fog jelezni, ha az
//eroforras mukodese befejezodott.
void MyRM_unuseResource(resourceUser_t* user)
{
    MyRM_t* rm=&myRM;
    resource_t* resource=user->resource;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);

    //TODO: mit tegyunk, ha hiba allapotban van az user?????????????????????????????????????????????????????????????????????

    switch(user->state)
    {
        case RESOURCEUSERSTATE_RUN:
        case RESOURCEUSERSTATE_WAITING_FOR_START:
            //Az eroforras el van inidtva vagy varunk annak elindulasara.
            //Ez utobbi akkor lehet, ha inditasi folyamat kozben megis lemond
            //az user az eroforrasrol.

            //Beallitjuk, hogy varunk annak leallasara
            user->state=RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE;

            //Eroforrast leallitjuk (legalabb is bejelezzuk, hogy egy user
            //lemond rola, igy ha annak usageCnt szamlaloja 0-ra csokken, akkor
            //az eroforras le fog allni.)
            MyRM_stopResourceCore(rm, resource);

            //Jelzes a taszknak...
            MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_UNUSE);
            break;

        case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
            //Az user mar le van allitva. Varunk annak befejezesere.
            //kilepes, es varakozas tovabba  befejezesre...
            break;

        case RESOURCEUSERSTATE_IDLE:
            //Az user mar le van allitva.

            //Callback-en keresztul jelezzuk az user fele, hogy az eroforras
            //vegzett. Ez peldaul general user eseten is lenyeges, amikor ugy
            //hivnak meg egy generalUnuse() funkciot, hogy az eroforras el sem
            //volt inditva. Ilyenkor a callbackben visszaadott statusz
            //segitsegevel olyan esemeny generalodik, mely az user-ben a
            //leallasra varakozast triggereli.
            if (user->statusFunc)
            {
                user->statusFunc(RESOURCE_STOP,
                                 &resource->errorInfo,
                                 user->callbackData);
            }
            break;

        default:
            //Ismeretlen allapot.
            ASSERT(0);
            break;
    }

    xSemaphoreGive(rm->mutex);    
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//Altalanos callback rutin, melyet az altalanos userre varas eseten hasznalunk
static void MyRM_generalUserStatusCallback(resourceStatus_t resourceStatus,
                                            resourceErrorInfo_t* errorInfo,
                                            void* callbackData)
{
    generalResourceUser_t* genUser=(generalResourceUser_t*) callbackData;

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
            //Ha van megadva az altalanos user-hez statusz callback, akkor hibak
            //eseten az meghivasra kerul.
            if (genUser->errorFunc)
            {   //Van beallitva callback
                genUser->errorFunc(errorInfo, genUser->callbackData);
            }
            xEventGroupSetBits(genUser->events, GENUSEREVENT_ERROR);
            break;
        default:
            ASSERT(0);
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

    //Eroforras hasznalatba vetele. A hivas utan a callbackben kapunk jelzest
    //az eroforras allapotarol.
    MyRM_useResource(&generalUser->user);

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
        status=generalUser->user.resource->errorInfo.errorCode;

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

    //Eroforras hasznalatanak lemondasa. A hivas utan a callbackben kapunk
    //jelzest az eroforras allapotarol.
    MyRM_unuseResource(&generalUser->user);


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
        status=generalUser->user.resource->errorInfo.errorCode;

        //eroforras leallitasa, igy biztositva abban a hiba torleset
        /*status=*/ MyRM_unuseResource(&generalUser->user);
        //if (status) return status;
    }

    return status;
}
//------------------------------------------------------------------------------
