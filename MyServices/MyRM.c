//------------------------------------------------------------------------------
//  Resource manager
//
//    File: MyRM.c
//------------------------------------------------------------------------------
#include "MyRM.h"
#include <string.h>

#define MyRM_TRACE  0

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
static inline void MyRM_startRequest(MyRM_t* rm, resource_t* resource);
static inline void MyRM_stopRequest(MyRM_t* rm,
                                    resource_t* resource,
                                    bool* continueWorking);
static void MyRM_startResourceCore(MyRM_t* rm, resource_t* resource);
static void MyRM_stopResourceCore(MyRM_t* rm, resource_t* resource);
static void MyRM_resourceStatusCore(MyRM_t* rm,
                                    resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode);
static void MyRM_addToRequestersDepErrorList(MyRM_t* rm, resource_t* resource);
static void MyRM_signallingUsers(MyRM_t* rm,
                                 resource_t* resource,
                                 bool continueWorking);

//Manager taszkot ebreszteni kepes eventek definicioi
#define MyRM_NOTIFY__RESOURCE_START_REQUEST     BIT(0)
#define MyRM_NOTIFY__RESOURCE_STOP_REQUEST      BIT(1)
#define MyRM_NOTIFY__RESOURCE_STATUS            BIT(2)
#define MyRM_NOTIFY__RESOURCE_USE               BIT(3)
#define MyRM_NOTIFY__RESOURCE_UNUSE             BIT(4)

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

    if (resource->reportedError==NULL)
    {   //nincs hiba
        printf("[  ]");
    } else
    {
        if (resource->reportedError==&resource->errorInfo)
        {   //sajat maga hibas
            printf("[E ]");
        } else
        {   //Valamely fuggosege hibas
            printf("[DE]");
        }
    }

    printf("   DC:%d DCnt:%d UCnt:%d Started: %d L.ErrCode:%d",
                                            (int)resource->depCount,
                                            (int)resource->depCnt,
                                            (int)resource->usageCnt,
                                            (int)resource->started,
                                            (int)resource->errorInfo.errorCode);

    printf("\n");


    if (printDeps)
    {
        printf("                   startReqCnt: %d  stopReqCnt: %d  errorFlag: %d\n", (int)resource->startReqCnt, (int)resource->stopReqCnt,  (int)resource->errorFlag);
        printf("                   runningFlag: %d  haltReq: %d  :haltedFlag: %d\n", (int)resource->runningFlag, (int)resource->haltReq, (int)resource->haltedFlag);

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

    resource->state=RESOURCE_STATE_STOP;
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
//Egy eroforras fuggosegi hibaja eseten hivodo fuggveny
//[MyRM_task-bol hivva]
static inline void MyRM_depError(MyRM_t* rm,
                                 resource_t* resource,
                                 resource_t* faultyDependency)
{
    #if MyRM_TRACE
    printf("  DEP ERROR. %s(%d) --> %s(%d)\n",
           faultyDependency->resourceName,
           faultyDependency->state,
           resource->resourceName,
           resource->state);
    #endif

    //Annak ellenorzese, hogy az eroforras mar kapott-e valamely
    //fuggosegetol, vagy akar sajat magatol hibat. Ezt onnan tudni, hogy a
    //reportedError pointere NULL vagy nem. Ha nem NULL, akkor mar egy
    //korabbi hiba miatt az eroforras hibara lett futtatva.
    if (resource->reportedError)
    {   //Az eroforras mar hiba allapotban van.
        return;
    }

    switch((int)resource->state)
    {
        case RESOURCE_STOP:
            //Az eroforras nem mukodik. Nem foglalkozik a fuggoseg hibajaval.
            return;
        case RESOURCE_STATE_STARTING:
        case RESOURCE_STATE_RUN:
        case RESOURCE_STATE_STOPPING:
            break;
        default:
            ASSERT(0);
            return;

    }

    //Az eroforras maga is hibara fut. A hibat atveszi a fuggosegetol.
    resource->reportedError=faultyDependency->reportedError;

    //Itt elvileg mindenkepen kell, hogy hibat mutasson a reportedError!
    ASSERT(resource->reportedError);

    //Hiba statusz generalasa
    //(Itt nem adunk at hibakodot, mivel azt mar beallitottuk az elobb)
    MyRM_resourceStatusCore(rm, resource, RESOURCE_ERROR, 0);
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha egy eroforars fuggosege elindult
//[MyRM_task-bol hivva]
static  void MyRM_dependenyStarted(MyRM_t* rm, resource_t* resource)
{
    //A fuggosegek szamanak csokkentese
    #if MyRM_TRACE
    printf("[%s]MyRM_dependenyStarted\n", resource->resourceName);
    #endif

    if (resource->depCnt==0)
    {   //A fuggoseg szamlalo elkeveredett.
        ASSERT(0);
        while(1);
    }

    resource->depCnt--;

    if (resource->state==RESOURCE_STATE_STARTING)
    {   //Az eroforras inditasra var...

        //Eloirjuk, hogy ellenorizze a fuggosegi szamlalojat, es ha az 0-ra
        //csokken, akkor inditsa el magat az eroforrast is, hiszen minden
        //fuggosege eloallt.
        resource->checkDepCntRequest=true;
        MyRM_addResourceToProcessReqList(rm, resource);
    }
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha egy eroforras fuggosege le lesz allitva
//[MyRM_task-bol hivva]
static  void MyRM_dependenyStop(MyRM_t* rm, resource_t* resource)
{
    (void) rm;

    #if MyRM_TRACE
    printf("[%s]MyRM_dependenyStop\n", resource->resourceName);
    #endif

    //A fuggosegek szamanak novelese
    if (resource->depCnt>=resource->depCount)
    {   //A fuggoseg szamlalo elkeveredett.        
        ASSERT(0);
        while(1);
    }

    resource->depCnt++;
}
//------------------------------------------------------------------------------
//Egy eroforras fuggosegi szamlalojanak tesztelese, es ha minden fuggosege
//elindult (depCnt==0), akkor az eroforras inditasa.
//[MyRM_task-bol hivva]
static inline void MyRM_depCntTest(MyRM_t* rm, resource_t* resource)
{
    //Keres torlese.
    resource->checkDepCntRequest=false;

    if (resource->depCnt!=0)
    {   //Az eroforarsnak meg van nem elindult fuggosege.
        //Tovabb varakozunk, hogy az osszes fuggosege elinduljon...
        return;
    }

    if (resource->state!=RESOURCE_STATE_STARTING)
    {   //Az eroforrasnak ezt a reszet csak STARTING allapotban szabad hivni.
        ASSERT(0);
        return;
    }

    //Az eroforrasnak mar nincsenek inditasra varo fuggosegei, es az
    //eroforras inditasa elo van irva. Az eroforars indithato.

    //Eroforras initek...
    //resource->doneFlag=false;
    resource->reportedError=NULL;

    status_t status;

    //Ha az eroforras meg nincs inicializalva, es van beregisztralva init()
    //fuggvenye, akkor meghivjuk. Ez a mukodese alatt csak egyszer tortenik.
    if ((resource->inited==false) && (resource->funcs->init))
    {
        xSemaphoreGive(rm->mutex);
        status=resource->funcs->init(resource->funcsParam);
        xSemaphoreTake(rm->mutex, portMAX_DELAY);
        if (status)
        {
            MyRM_resourceStatusCore(rm, resource, RESOURCE_ERROR, status);
            return;
        }
    }
    resource->inited=true;
    //Jelzesre kerul, hogy az eroforras el van inditva.
    resource->started=true;

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
        //resource->started=true;
        xSemaphoreGive(rm->mutex);
        status=resource->funcs->start(resource->funcsParam);
        xSemaphoreTake(rm->mutex, portMAX_DELAY);
        if (status)
        {
            MyRM_resourceStatusCore(rm, resource, RESOURCE_ERROR, status);
            return;
        }

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
}
//------------------------------------------------------------------------------
//Annak ellenorzese, hogy az eroforarst el kell-e inditani, vagy le kell-e
//allitani. Hiba eseten ebebn oldjuk meg a hiba miatti leallst is.
//[MyRM_task-bol hivva]
static inline void MyRM_checkStartStop(MyRM_t* rm, resource_t* resource)
{
    if (resource->haltReq)
    {   //hiba miatti leallitasi kerelem van.

        //Kerelem torlese
        resource->haltReq=false;

        //Jelezzuk az ot hasznalo magasabb szinten levo eroforrasoknal es
        //usereknek a hibat....
        MyRM_addToRequestersDepErrorList(rm, resource);
        //[ERROR]
        resource->signallingUsers=true;

        //Ugrik az eroforrast leallito reszre. (igy egyszerubb)
        goto stop_resource;

    }

    if (resource->doneFlag)
    {   //Egy egyszer lefuto eroforras elkeszult.
        if (resource->state!=RESOURCE_STATE_STOPPING)
        {   //Az eroforrasnak ilyenkor STOPPING allapotot kell mutatnia!
            ASSERT(0); while(1);
        }

        if (resource->usageCnt!=1)
        {   //Az eroforrast tobben is hasznaljak. Ez nem megengedett. Az egyszer
            //lefutni kepes eroforrasoknak csak egy hasznalojuk lehet!
            ASSERT(0); while(1);
        }
        //A hasznalati szamlalo nullazasaval elerjuk, hogy az usageCnt==0 ag
        //fusson le, igy a STOPPING allapot automatikusan a STOP allapotba
        //lepest fogja eloidezni.
        resource->usageCnt=0;
    }

    if (resource->usageCnt)
    {   //Vannak olyan eroforrasok/userek, melyek hasznalni akarjak az
        //eroforrast. El kell azt inditani.
        //Ez is lehet, hogy a leallitasi folyamat kozben erkezett, az
        //eroforrast hasznalni akaro keres, a szamlal amiatt novekedett, ezert
        //az eroforrast ujra kell inditani.

        if (resource->errorFlag)
        {   //Ha hibas allapotban van az eroforras, akkor nem hivodhatnak meg az
            //indito fuggvenyek.
            //Hiba eseten varjuk azt, hogy mindenki lemondjun rola, es az
            //usageCnt 0-ra csokkenjen.
            return;
        }

        switch(resource->state)
        {   //..................................................................
            case RESOURCE_STATE_STARTING:
                //Az eroforras elindult
                resource->state=RESOURCE_STATE_RUN;
                resource->runningFlag=true;

                //Minden hasznalojaban jelzi, hogy a fuggoseguk elindult.
                //Ezt ugy teszi, hogy meghivja azok dependencyStart()
                //funkciojukat. Abban azok csokkentik a depCnt szamlalojukat, es
                //ha az 0-ra csokkent, akkor elinditjak sajat folyamataikat.
                resourceDep_t* requester=resource->requesterList.first;
                while(requester)
                {
                    resource_t* reqRes=(resource_t*)requester->requesterResource;
                    MyRM_dependenyStarted(rm, reqRes);
                    requester=(resourceDep_t*)requester->nextRequester;
                }

                //minden userenek jelzi, hogy az eroforras elindult
                //[RUN]
                resource->signallingUsers=true;

                break;
            //..................................................................
            case RESOURCE_STATE_STOPPING:
                //Az eroforras leallt, de ujra kell inditani                
            case RESOURCE_STATE_STOP:
                //Az eroforrast el kell inditani

                //Az eroforras felveszi az inditasi allapotot...
                resource->state=RESOURCE_STATE_STARTING;

                //Noveljuk a futo eroforrasok szamat a managerben.
                MyRM_incrementRunningResourcesCnt(rm);

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

                //ELlenorizni kell, hogy van-e meg fuggosege, amire varni kell.
                //Ha nincs, akkor az eroforras indithato lesz.
                resource->checkDepCntRequest=true;

                break;
            //..................................................................
            default:
                ASSERT(0);
                return;
        } //switch(resource->state)
    } else //-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
    {   //usageCnt==0
        //Az eroforrast mar nem hasznalja senki, vagy hiba miatt le kell azt
        //allitani.
        switch(resource->state)
        {   //..................................................................
            case RESOURCE_STATE_STARTING:
                //Az eroforras elindult, de megis le kell azt allitani, mert
                //azota lemondtak a hasznalatat.
            case RESOURCE_STATE_RUN:
                //Az eroforarst le kell allitani

                //Felvesszuk a leallitasi folyamat allapotat
                resource->state=RESOURCE_STATE_STOPPING;

                //Ugrik az eroforrast leallito reszre. (igy egyszerubb)
                goto stop_resource;

            //..................................................................
            case RESOURCE_STATE_STOPPING:
                //Az eroforras leallt, vagy hiba allapotban van, es midenki
                //lemondott rola.

                if (resource->errorFlag)
                {   //az eroforras hiba allapotban van. Varjuk, hogy a belso
                    //folyamatait lezarja...
                    if (resource->haltedFlag==false)
                    {   //Az eroforras meg nem zarta le a belso folyamatait.
                        //varunk tovabb....
                        break;
                    } else
                    {   //Az eroforars lezarta a belso folyamatokat. Hiba
                        //allapot torlese.
                        resource->haltedFlag=false;
                        resource->errorFlag=false;
                    }
                }

                //Elinditottsag jelzesenek torlese.
                resource->started=false;

                //Az eroforras felveszi a leallitott allapotot
                resource->state=RESOURCE_STATE_STOP;


                //Csokkenteheto a futo eroforrasok szamlalojat a managerben
                MyRM_decrementRunningResourcesCnt(rm);

                //Az eszkoz fuggosegeiben is lemondja a hasznalatot.
                //Vegig fut a fuggosegeken, es kiadja azokra a leallitasi
                //kerelmet...
                resourceDep_t* dep=resource->dependencyList.first;
                while(dep)
                {
                    resource_t* depRes=(resource_t*) dep->requiredResource;
                    MyRM_stopResourceCore(rm, depRes);

                    //lancolt list akovetkezo elemere allas.
                    dep=(resourceDep_t*)dep->nextDependency;
                }

                //minden userenek jelzi, hogy az eroforras elindult
                //[STOP]
                resource->signallingUsers=true;

                break;
            //..................................................................
            default:
                ASSERT(0);
                return;
        } //switch(resource->state)

    } //else


    return;


stop_resource:
    //eroforras leallitasa

    if (resource->runningFlag)
    {   //Az eroforras fut. Korabban ezt mar jelezte a kerelmezoi fele.

        //Minden hasznalojaban jelzi, hogy a fuggoseg leall.
        //Ezt ugy teszi, hogy meghivja azok dependencyStop()
        //funkciojukat. Abban azok novelik a depCnt szamlalojukat
        resource->runningFlag=false;

        resourceDep_t* requester=resource->requesterList.first;
        while(requester)
        {
            resource_t* reqRes=(resource_t*)requester->requesterResource;
            MyRM_dependenyStop(rm, reqRes);
            requester=(resourceDep_t*)requester->nextRequester;
        }
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
        status_t status=resource->funcs->stop(resource->funcsParam);
        xSemaphoreTake(rm->mutex, portMAX_DELAY);
        //resource->started=false;
        if (status)
        {
            MyRM_resourceStatusCore(rm,
                                    resource,
                                    RESOURCE_ERROR,
                                    status);
        }
    } else
    {   //Nincs stop fuggvenye. Generalunk egy "leallt" allapotot.
        //Vagy ha ugyan van stop fuggvenye, de az eroforras meg nem indult
        //el. (Peldaul meg vart a fuggosegeire.)
        //Ennek kiertekelese majd ujra a taszkban fog megtortenni,
        //mely hatasara az eroforrasra varo usreke vagy masik
        //eroforrasok mukodese folytathato.
        MyRM_resourceStatusCore(rm,
                                resource,
                                RESOURCE_STOP,
                                kStatus_Success);
    }
}
//------------------------------------------------------------------------------
//Eroforras allapotainak kezelese
static inline void MyRM_processResource(MyRM_t* rm, resource_t* resource)
{    
    bool continueWorking=false;


    //Annak ellenorzese, hogy valamely fuggosegeben eloallt-e (ujabb) hiba.
    //A hibas fuggosegeket jelzi az eroforras szamara.
    while(resource->depErrorList.first)
    {
        resourceDep_t* dep=resource->depErrorList.first;
        //A soron kovetkezo hibara futott eroforrasra mutat.
        resource_t* faultyDependency= (resource_t*)dep->requiredResource;
        //A feldologozott eroforras kovetkezo  hibas fuggosegere allas.
        //A feldolgozasra varo listaelem torlodik a listabol.
        resource->depErrorList.first=(resourceDep_t*)(dep->nextDepError);
        dep->nextDepError=NULL;
        dep->depErrorInTheList=false;

        MyRM_depError(rm, resource, faultyDependency);
    } //while(dep)


    if (resource->startReqCnt)
    {   //Van fuggoben inditasi kerelem az eroforrasra. Valaki(k) igenybe
        //akarja(k) venni.
       MyRM_startRequest(rm, resource);
    }


    if (resource->stopReqCnt)
    {   //Van fuggoben lemondasi kerelem az eroforrasra. Valaki(k) lemond annak
        //hasznalatarol
        MyRM_stopRequest(rm, resource, &continueWorking);
    }


    //Kell ellenorizni az eroforras leallitasra, vagy inditasra?
    if (resource->checkStartStopReq)
    {   //Ellenorzes eloirva. Ellenorizni kell, hogy hasznalja-e meg valaki az
        //eroforrast, vagy ha nem, akkor az leallithato. Vagy ha le van allitva,
        //viszont van ra hasznalati igeny, akkor induljon el.
        //Hiba eseten pedig allitsa le az eroforarst.

        //Ellenorzesi kerelem torlese.
        resource->checkStartStopReq=false;

        MyRM_checkStartStop(rm, resource);
    } //if (resource->checkStartStopReq)


    //Kell ellenorizni a fuggosegeket, mert varunk valamelyik elindulasara?
    if (resource->checkDepCntRequest)
    {   //Ellenorzes eloirva. Ellenorizni kell, hogy var-e meg valamelyik
        //fuggosegenek az elindulasara.
        MyRM_depCntTest(rm, resource);
    } //if (resource->checkDepCntRequest)


    if (resource->signallingUsers)
    {   //Az eroforrast hasznalo userek fele elo van irva a jelzes
        //kuldes, az eroforras allapotarol.

        //Jelzesi kerelem torlese
        resource->signallingUsers=false;

        MyRM_signallingUsers(rm, resource, continueWorking);

        //Egyszer lefuto eroforarsok eseten a kesz jelzes torlese.
        resource->doneFlag=false;
    }
}
//------------------------------------------------------------------------------
//Inditasi kerelmek kezelese
//[MyRM_task-bol hivva]
static inline void MyRM_startRequest(MyRM_t* rm, resource_t* resource)
{
    (void) rm;

    //Az eroforrast hasznalok szamat annyival noveljuk, amennyi inditasi
    //kerelem futott be az utolso feldolgozas ota...
    resource->usageCnt += resource->startReqCnt;
    //A kerelmek szamat toroljuk, igy az a kovetkezo feldolgozasig az addig
    //befutottakat fogja majd ujra akkumulalni.
    resource->startReqCnt = 0;

    if (resource->errorFlag)
    {   //Az eroforras hibas allapotot mutat. Nem inditunk el belso muveleteket,
        //csak jelezzuk az ot hasznalo magasabb szinten levo eroforrasoknal es
        //usereknek a hibat....

        MyRM_addToRequestersDepErrorList(rm, resource);
        //[ERROR]
        resource->signallingUsers=true;
        return;
    }

    switch((int) resource->state)
    {
        case RESOURCE_STATE_RUN:
            //Az eroforras mar el van inditva.
            //Jelezni kell az eroforarst ujonnan igenybe vevo user(ek) fele,
            //hogy az eroforras mar mukodik. Arra ne varjanak tovabb.
            //[RUN]
            resource->signallingUsers=true;
            return;

        case RESOURCE_STATE_STARTING:
            //Ha az eroforras allapota STARTING, akkor nincs mit tenni.
            //Majd ha az osszes fuggosege elindult, akkor ez is el fog indulni.
            //(Az usageCnt viszont novekedett az eroforrast haszanlni akarok
            //szamaval.)
            return;

        case RESOURCE_STATE_STOPPING:
            //Ha az eroforras STOPPING allapotban van, akkor itt nem csinalunk
            //semmit.
            //(Az usageCnt viszont novekedett az eroforrast haszanlni akarok
            //szamaval.)
            //Az eroforras leallasakor majd eszre fogja venni, hogy az usageCnt
            //nem 0, ezert az eroforrast ujra el fogja inditani, a
            //checkStartStop rezsben
            return;

        case RESOURCE_STATE_STOP:
            //Az eroforras le van allitva. El kell azt inditani.
            //A checkStartStop() reszben el lesz inditva (STOP ag).
            resource->checkStartStopReq=true;
            return;

        default:
            ASSERT(0);
            return;
    }
}
//------------------------------------------------------------------------------
//leallitasi kerelmek kezelese
//[MyRM_task-bol hivva]
static inline void MyRM_stopRequest(MyRM_t* rm,
                                    resource_t* resource,
                                    bool* continueWorking)
{
    (void) rm;

    switch((int) resource->state)
    {
        case RESOURCE_STATE_STOPPING:
            if (resource->errorFlag==false)
            {   //Normal mukodesnel (tehat nincs hiba), mar leallitasi folya-
                //mat van az eroforrason. Annak vegeztevel kielemzesre kerul
                //majd a checkStartStop() reszben aleallsa alatt az usageCnt
                //mutatat e azota ujabb hasznalati igenyt. Ha igen, akkor az
                //eroforars ujra el lesz inditva, ha nem, akkor az eroforras
                //leall, es arrol jelzes keletkezik.
                return;
            }
            break;
        case RESOURCE_STATE_RUN:
            break;
        case RESOURCE_STATE_STARTING:
            break;

        case RESOURCE_STATE_STOP:
            //Az eroforras mar le van allitva.
            //Leallitott allapotban nem fogadhat inditasi kerelmet            
            ASSERT(0);
            return;

        default:
            ASSERT(0);
            return;
    }

    //A lemondasok szamaval csokkentjuk az eroforrast hasznalok szamlalojat.
    int newCnt= (int)(resource->usageCnt - resource->stopReqCnt);
    if (newCnt<0)
    {   //Az eroforras kezelesben hiba van. Nem mondhatnanak le tobben,
        //mint amennyien korabban igenybe vettek!
        ASSERT(0);
        return;
    }
    resource->usageCnt=(uint32_t)newCnt;
    //Az igeny szamlalo nullazasra kerul. Ettol kezdve a friss igenyeket akkumu-
    //lalja ujra.
    resource->stopReqCnt=0;

    if (resource->usageCnt!=0)
    {   //Az eroforrasnak meg maradtak hasznaloi.
        //Jelezni kell a lemondo userek szamara, hogy ne varjanak a befejezsere.
        *continueWorking=true;
        resource->signallingUsers=true;
        return;
    }

    if (resource->state==RESOURCE_STATE_RUN)
    {   //Az eroforras leallitasat kell kezdemenyezni. A checkStartStop()
        //reszben, a RUN agban tortenik.
        resource->checkStartStopReq=true;
    } else
    if (resource->errorFlag)
    {   //Az eroforras hibas allapota torolheto, mert mindenki lemondott az
        //eroforrasrol a magasabb szinteken.
        resource->checkStartStopReq=true;
    }

    //STARTING allapot eseten, majd varjuk, hogy az eroforras elinduljon, es
    //ha za megtortenik, akkor az usageCnt 0 erteket eszreveve, elfogja
    //kezdeni az eroforras leallitasat a checkStartStop() resz STARTING agaban.
}
//------------------------------------------------------------------------------
//Az egyes eroforrasok ezen keresztul jelzik a manager fele az allapotvaltozasa-
//ikat.
static void MyRM_resourceStatusCore(MyRM_t* rm,
                                    resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode)
{

    #if MyRM_TRACE
    const char* statusStr="";
    switch(resourceStatus)
    {
        case RESOURCE_RUN: statusStr="RUN"; break;
        case RESOURCE_STOP: statusStr="STOP"; break;
        case RESOURCE_DONE: statusStr="DONE"; break;
        case RESOURCE_ERROR: statusStr="ERROR"; break;
    }
    printf("%s----RESOURCE STATUS=%s, errorCode:%d\n", resource->resourceName, statusStr, (int)errorCode);
    #endif



    //Ha az erorCode hibat jelez, akkor hibara visszuk az eroforrast
    if (errorCode) resourceStatus=RESOURCE_ERROR;

    switch(resourceStatus)
    {
        //......................................................................
        case RESOURCE_RUN:
            //Az eroforras azt jelzi, hogy elindult

            if (resource->errorFlag)
            {   //A folyamat akkor jelzi, hogy elindult, amikor a manager az
                //eroforrast mar hibara allitotta.
                //Ez konnyen lehet, mert peldaul egy tszkban futo inditasi fo-
                //lyamat vegen jogosan jelzi, hogy elindult, ugyanakkor,
                //ha kozben valamelyik fuggosege hibara fut, akkor arrol meg
                //ezen a ponton nem tudhat.
                //A jelzest egyszeruen eldobjuk.
                return;
            }

            if (resource->state != RESOURCE_STATE_STARTING)
            {   //Az eroforras ugy kuldte az elindult jelzest, hogy kozben nem
                //is inditasi allapotban van. Ez szoftverhiba!
                ASSERT(0); while(1);
            }

            //A ckeckStartStop() reszben "STARTING" agban befejezodik az
            //inditasi folyamat a szal alatt.
            resource->checkStartStopReq=true;
            break;
        //......................................................................
        case RESOURCE_STOP:
            //Az eroforras azt jelzi, hogy leallt.

            if (resource->state != RESOURCE_STATE_STOPPING)
            {   //Az eroforras ugy kuldte a lealt jelzest, hogy az nem volt
                //leallitasi allapotban.
                //Ez szoftverhiba!
                ASSERT(0); while(1);
            }

            if (resource->errorFlag)
            {   //Hibas mukodes lezarasa megtortent. Az eroforras implementa-
                //ciojaban mar nem futnak folyamatok. Jelezzuk, hogy ha az
                //eroforrasrol mindenki lemondott, akkor az eroforras STOP
                //allapotba teheto.
                resource->haltedFlag=true;
            }

            //A ckeckStartStop() reszben "STARTING" agban befejezodik az
            //inditasi folyamat a szal alatt.
            resource->checkStartStopReq=true;
            break;
        //......................................................................
        case RESOURCE_DONE:
            //Az eroforras azt jelzi, hogy vegzett a feladataval
            if (!((resource->state == RESOURCE_STATE_STARTING) ||
                  (resource->state == RESOURCE_STATE_RUN)))
            {   //Az eroforrasnak vagy futnia, vagy inditasi allapotot kell
                //mutatnia. Ha ez nem igy van, akkor az szoftverhiba.
                ASSERT(0); while(1);
            }
            //Jelzes, hogy kesz a folyamat
            resource->doneFlag=true;
            //Az allapot atmegy STOPPING-ba, amig a szalban be nem allitja ra a
            //stop-ot.
            resource->state=RESOURCE_STATE_STOPPING;
            //Le kell futtatni a start/stop igeny ellenorzot
            resource->checkStartStopReq=true;            
            break;
        //......................................................................
        case RESOURCE_ERROR:
            //Az eroforras hibat jelez
            switch((int)resource->state)
            {
                case RESOURCE_STATE_STOPPING:
                case RESOURCE_STATE_RUN:
                case RESOURCE_STATE_STARTING:
                    break;

                case RESOURCE_STATE_STOP:                    
                default:
                    //Az eroforras mar le van allitva.
                    //Leallitott allapotban nem kaphat hibajelzest
                    ASSERT(0);
                    while(1);
            } //switch(resource->state)

            if (resource->errorFlag==true)
            {   //Az eroforras mar hibara van futtatva. Meg egy hibat nem fogad.
                break;
            }

            //Hibas allapot beallitasa
            resource->errorFlag=true;

            //Hiba informacios struktura feltoltese, melyet majd kesobb az
            //eroforarst hasznalo masik eroforarsok, vagy userek fele
            //tovabb adhatunk...
            //Reportoljuk, hogy melyik eroforarssal van a hiba
            resource->errorInfo.resource=(struct resource_t*)resource;
            //informaljuk, hogy mi a hibakod
            resource->errorInfo.errorCode=errorCode;
            //az aktualis modul allapotot is reportoljuk
            resource->errorInfo.resourceState=resource->state;


            //Ha meg nincs tovabb ripoltolhato hiba, akkor ez lesz az, mely
            //a fentebbi szintek fele jelzesre kerul.
            //Ez akkor lehet, ha a hibat o kezdemenyezte.
            if (resource->reportedError==NULL)
            {
                resource->reportedError=&resource->errorInfo;
            }

            //A leallitasi allapot felvetele. (hiba eseten le kell allni.)
            resource->state=RESOURCE_STATE_STOPPING;

            //leallitasi kerelem.
            resource->haltReq=true;

            //
            resource->checkStartStopReq=true;
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
    //Vegighalad az eroforrast hasznalok lancolt listajan...
    resourceDep_t* requesterDep=resource->requesterList.first;
    for(; requesterDep; requesterDep=(resourceDep_t*) requesterDep->nextRequester)
    {
        if (requesterDep->depErrorInTheList)
        {   //A hibazo eroforras mar hozza van adva a listahoz. Megegyszer nem
            //teszi
            continue;
        }

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

    //Vegig halad az eroforras userein...
    for(;user; user=(resourceUser_t*)user->userList.next)
    {
        #if MyRM_TRACE
        printf("SIGNALLING USER! user: %s   resource: %s   state: %d   continueWorking: %d\n",
               user->userName, resource->resourceName, resource->state, continueWorking);
        #endif

        if (user->state==RESOURCEUSERSTATE_IDLE)
        {   //Amig nincs hasznalatban az user reszerol az eroforras, addig
            //nem kaphat jelzest.
            //Ilyen az, amig nincs elinditva az eroforras, vagy ebbe az
            //allapotba kerul, ha a hasznalt eroforras leallt.
            continue;
        }



        if ((resource->state==RESOURCE_STATE_STOPPING) &&
            (resource->errorFlag))
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

                    //Jelezes az usernek a statusz callbacken keresztul.
                    //Egyszer lefuto eroforarsok eseten DONE jelzes kerul
                    //a statuszban atadasra.
                    if (user->statusFunc)
                    {
                        //xSemaphoreGive(rm->mutex);
                        user->statusFunc(resource->doneFlag ? RESOURCE_DONE
                                                            : RESOURCE_STOP,
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

                if ( (resource->state==RESOURCE_STATE_STOP) ||
                    ((resource->state==RESOURCE_STATE_RUN) &&
                     (continueWorking==true)))
                {   //Az eroforras leallt, vagy tovabb mukodik, mert
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

    #if MyRM_TRACE
    printf("MyRM_useResource() user: %s  state: %d\n", user->userName, user->state);
    #endif

    switch((int)user->state)
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

            MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_USE);
            break;

        case RESOURCEUSERSTATE_WAITING_FOR_START:
        case RESOURCEUSERSTATE_RUN:
            //Mar egy korabbi inditasra varunk, vagy mar mukodik az eroforras.
            //Nem inditunk ra ujra.
            break;


        case RESOURCEUSERSTATE_ERROR:
            //Az eroforras hibas allapotban van. Abbol csak ugy lehet kilepni,
            //ha elobb leallitjak az eroforrast.
            //Generalunk az applikacio fele jelzest, hogy az eroforars hibas.
            if (user->statusFunc)
            {
                user->statusFunc(RESOURCE_ERROR,
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

    #if MyRM_TRACE
    printf("MyRM_unuseResource() user: %s  state: %d\n", user->userName, user->state);
    #endif

    switch((int)user->state)
    {

        case RESOURCEUSERSTATE_RUN:
        case RESOURCEUSERSTATE_WAITING_FOR_START:
        case RESOURCEUSERSTATE_ERROR:
            //Az eroforras el van inidtva vagy varunk annak elindulasara.
            //Ez utobbi akkor lehet, ha inditasi folyamat kozben megis lemond
            //az user az eroforrasrol.

            //Vagy az eroforras hibaja miatt az user hiba allapotban van.
            //Ebbol kilepni, az eroforras leallitasaval lehet.

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
