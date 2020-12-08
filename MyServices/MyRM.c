//------------------------------------------------------------------------------
//  Resource manager
//
//    File: MyRM.c
//------------------------------------------------------------------------------
#include "MyRM.h"
#include <string.h>

#define MyRM_TRACE  1

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
static void MyRM_sendStatus(resource_t* resource,
                            resourceStatus_t resourceStatus);
static void MyRM_dependencyStatusCB(struct resourceDep_t *dep,
                                    resourceStatus_t status,
                                    resourceErrorInfo_t* errorInfo);
static inline void MyRM_sendNotify(MyRM_t* rm, uint32_t eventBits);
static inline void MyRM_processResource(MyRM_t* rm, resource_t* resource);
//static inline void MyRM_startRequest(MyRM_t* rm, resource_t* resource);
//static inline void MyRM_stopRequest(MyRM_t* rm,
//                                    resource_t* resource);
//static void MyRM_startResourceCore(MyRM_t* rm, resource_t* resource);
//static void MyRM_stopResourceCore(MyRM_t* rm, resource_t* resource);
static void MyRM_startDependency(resourceDep_t* dep);
static void MyRM_stopDependency(resourceDep_t* dep);
static void MyRM_resourceStatusCore(resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode);
static void MyRM_signallingUsers(MyRM_t* rm,
                                 resource_t* resource);
static void MyRM_restartDo(MyRM_t* rm,
                           resource_t* resource,
                           resourceUser_t* user);
static void MyRM_user_resourceStatusCB(struct resourceDep_t* dep,
                                       resourceStatus_t status,
                                       resourceErrorInfo_t* errorInfo);

//Manager taszkot ebreszteni kepes eventek definicioi
#define MyRM_NOTIFY__RESOURCE_START_REQUEST     BIT(0)
#define MyRM_NOTIFY__RESOURCE_STOP_REQUEST      BIT(1)
#define MyRM_NOTIFY__RESOURCE_STATUS            BIT(2)
#define MyRM_NOTIFY__RESOURCE_USE               BIT(3)
#define MyRM_NOTIFY__RESOURCE_UNUSE             BIT(4)


#define MyRM_LOCK(mutex)    xSemaphoreTakeRecursive(mutex, portMAX_DELAY)
#define MyRM_UNLOCK(mutex)  xSemaphoreGiveRecursive(mutex)

static const char* MyRM_resourceStateStrings[]=RESOURCE_STATE_STRINGS;
static const char* MyRM_resourceStatusStrings[]=RESOURCE_STATUS_STRINGS;
static const char* MyRM_resourceUserStateStrings[]=RESOURCE_USER_STATE_STRINGS;
//------------------------------------------------------------------------------
//Eroforras management reset utani kezdeti inicializalasa
void MyRM_init(const MyRM_config_t* cfg)
{
    MyRM_t* this=&myRM;

    //Manager valtozoinak kezdeti torlese
    memset(this, 0, sizeof(MyRM_t));

    //Managerhez tartozo mutex letrehozasa
  #if configSUPPORT_STATIC_ALLOCATION
    this->mutex=xSemaphoreCreateRecursiveMutexStatic(&this->mutexBuffer);
  #else
    this->mutex=xSemaphoreCreateRecursiveMutex();
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
    //Eroforras nevenek kiirasa. Ha nem ismert, akkor ??? kerul kiirasra
    printf("%12s", resource->resourceName ? resource->resourceName : "???");

    //Eroforras allapotanak kiirasa
    if (resource->state>ARRAY_SIZE(MyRM_resourceStateStrings))
    {   //illegalis allapot. Ez csak valami sw hiba miatt lehet
        printf("  [???]");
    } else
    {
        printf("  [%s]", MyRM_resourceStateStrings[resource->state]);
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
        printf("                   errorFlag: %d\n", (int)resource->errorFlag);
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

        //if (resource->userList.first !=NULL)
        //{   //Van az eroforrasnak hasznaloja
        //
        //    printf("\n              Users: ");
        //
        //    //felhasznalok kilistazasa
        //    resourceUser_t* user=(resourceUser_t*)resource->userList.first;
        //    while(user)
        //    {
        //        const char* userName= user->userName;
        //        if (userName!=NULL)
        //        {
        //            printf("%s", userName);
        //        } else
        //        {
        //            printf("???");
        //        }
        //        //allapot kiirasa
        //        printf("(%d)  ",(int)user->state);
        //        //Kovetkezo elemre lepunk a listaban
        //        user=(resourceUser_t*)user->userList.next;
        //    }
        //}

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
//Eroforrashoz tartozo fuggoseg hozzaadasa.
static inline void MyRM_addDependencyToResource(resource_t* resource,
                                                resourceDep_t* dep)
{
    //A leiroban beallitjuk az eroforrast, akihez tartozik, akinek a fuggoseget
    //leirjuk.
    dep->requesterResource=(struct resource_t*) resource;                       //<-----------void?

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

    MyRM_LOCK(rm->mutex);

    //Beallitjuk a leiroban a kerelmezo es a fuggoseget
    dep->requesterResource=(struct resource_t*) highLevel;
    dep->requiredResource =(struct resource_t*) lowLevel;
    MyRM_addDependencyToResource(highLevel, dep);
    MyRM_addRequesterToResource (lowLevel,  dep);
    //Beallitja a fuggoseg altal hivando statsusz fuggvenyt, melyen keresztul a
    //fuggoseg jelezheti a kerelmezo szamara az allapotvaltozasait
    dep->depStatusFunc=MyRM_dependencyStatusCB;

    MyRM_UNLOCK(rm->mutex);
}
//------------------------------------------------------------------------------
//A rendszerben levo eroforrasok listajahoz adja a hivatkozott eroforras kezelot.
static void MyRM_addResourceToManagedList(resource_t* resource)
{
    MyRM_t* rm=&myRM;

    MyRM_LOCK(rm->mutex);

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

    MyRM_UNLOCK(rm->mutex);
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
    MyRM_LOCK(rm->mutex);
    rm->allResourceStoppedFunc=func;
    rm->callbackData=callbackData;
    MyRM_UNLOCK(rm->mutex);
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
            MyRM_UNLOCK(rm->mutex);
            rm->allResourceStoppedFunc(rm->callbackData);
            MyRM_LOCK(rm->mutex);
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

    MyRM_LOCK(rm->mutex);
    MyRM_resourceStatusCore(resource, resourceStatus, errorCode);
    //Az eroforras kiertekeleset eloirjuk a taszkban
    MyRM_addResourceToProcessReqList(rm, resource);
    MyRM_UNLOCK(rm->mutex);

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_STATUS);
}
//------------------------------------------------------------------------------
#if 0
static void MyRM_startResourceCore(MyRM_t* rm, resource_t* resource)
{
    #if MyRM_TRACE
    printf("MyRM_startResourceCore()  %s\n", resource->resourceName);
    #endif

    //Az inditasi kerelmek szamat noveljuk az eroforrasban.
    //Ez alapjan a taszkban tudni fogjuk, hogy mennyien jeleztek az eroforras
    //hasznalatat.
    resource->startReqCnt++;

    resource->checkStartStopReq=true;

    //Eroforras feldolgozasanak eloirasa...
    MyRM_addResourceToProcessReqList(rm, resource);
}
//------------------------------------------------------------------------------
static void MyRM_stopResourceCore(MyRM_t* rm, resource_t* resource)
{
    //if (resource->state==RESOURCE_STATE_STOP)
    //{   //Az eroforras mar le van allitva, vagy el sem volt inditva.
    //    //A kerst ignoraljuk
    //
    //    #if MyRM_TRACE
    //    printf("MyRM_stopResourceCore() I G N O R E  %s\n", resource->resourceName);
    //    #endif
    //    return;
    //}

    #if MyRM_TRACE
    printf("MyRM_stopResourceCore()  %s\n", resource->resourceName);
    #endif

    //Leallitasi kerelmek szamat noveljuk az eroforrasban.
    //Ez alapjan a taszkban tudni fogjuk, hogy mennyien mondanak le az eroforras
    //hasznalarol.
    resource->stopReqCnt++;

    resource->checkStartStopReq=true;

    //Eroforras feldolgozasanak eloirasa...
    MyRM_addResourceToProcessReqList(rm, resource);
}
#endif
//------------------------------------------------------------------------------
void MyRM_startResource(resource_t* resource)
{
    MyRM_t* rm=&myRM;

    MyRM_LOCK(rm->mutex);
    //MyRM_startResourceCore(rm, resource);
    MyRM_UNLOCK(rm->mutex);

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_START_REQUEST);
}
//------------------------------------------------------------------------------
void MyRM_stopResource(resource_t* resource)
{
    MyRM_t* rm=&myRM;

    MyRM_LOCK(rm->mutex);
    //MyRM_stopResourceCore(rm, resource);
    MyRM_UNLOCK(rm->mutex);

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
static void MyRM_dumpResourceValues(resource_t* resource)
{
    printf("::: %12s  [%8s]::   errorFlag: %d  haltReq:%d  haltedFlag:%d  runFlag:%d  startReq:%2d  stopReq:%2d  usageCnt:%2d  depCnt:%2d :::\n",
           resource->resourceName,
           MyRM_resourceStateStrings[resource->state],
           resource->errorFlag,
           resource->haltReq,
           resource->haltedFlag,
           resource->runFlag,
           resource->startReqCnt,
           resource->stopReqCnt,
           resource->usageCnt,
           resource->depCnt
          );
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


        MyRM_LOCK(rm->mutex);

        //Vegig a feldolgozando eroforrasok listajan...
        resource_t* resource=rm->processReqList.first;
        while(resource)
        {            
            //Az eroforrast kiveszi a listabol. (Ez a lista legelso eleme)
            MyRM_deleteResourceFromProcessReqList(rm, resource);

            #if MyRM_TRACE
            printf("---------------------------------------------------------------------------------\n");
            MyRM_dumpResourceValues(resource);
            #endif
if (resource->debugFlag)
{
    __NOP();
    __NOP();
    __NOP();
}
            //eroforras allapotok feldolgozasa...
            MyRM_processResource(rm, resource);            

            #if MyRM_TRACE
            MyRM_dumpResourceValues(resource);
            #endif

            //kovetkezo eroforrasra allas. (Mindig a lista elso eleme)
            resource=rm->processReqList.first;

        } //while(resource)

        MyRM_UNLOCK(rm->mutex);
    } //while(1)
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//Annak ellenorzese, hogy az eroforarst el kell-e inditani, vagy le kell-e
//allitani. Hiba eseten ebben oldjuk meg a hiba miatti leallst is.
//[MyRM_task-bol hivva]
static inline void MyRM_checkStartStop(MyRM_t* rm, resource_t* resource)
{
    status_t status;

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
        //A hasznalati szamlalo nullazasaval elerjuk, hogy a STOPPING allapotbol
        //automatikusan a STOP allapotba vigye az eroforras allapotat.
        resource->usageCnt=0;
    }


    //Elagazas az eroforras aktualis allapota szerint...
    switch((int)resource->state)
    {   //..................................................................
        case RESOURCE_STATE_STARTING:
        {
            //Az eroforras most indul...
            if (resource->started==false)
            {   //Az eroforras meg nincs elinditva. Varjuk, hogy az inditashoz
                //szukseges feltetelek teljesuljenek. Varjuk, hogy az osszes
                //fuggosege elinduljon...

                if (resource->depCnt!=0)
                {   //Az eroforrasnak meg van nem elindult fuggosege.
                    //Tovabb varakozunk, hogy az osszes fuggosege elinduljon...
                    break;
                }

                //Az eroforrasnak mar nincsenek inditasra varo fuggosegei
                //Az eroforars indithato.

                //Eroforras initek...
                //resource->doneFlag=false;
                //resource->reportedError=NULL;

                //Ha az eroforras meg nincs inicializalva, es van beregisztralva
                //init() fuggvenye, akkor meghivjuk. Ez az eroforras elete alatt
                //csak egyszer tortenik.
                if ((resource->inited==false) && (resource->funcs->init))
                {
                    MyRM_UNLOCK(rm->mutex);
                    status=resource->funcs->init(resource->funcsParam);
                    MyRM_LOCK(rm->mutex);
                    if (status)
                    {
                        MyRM_resourceStatusCore(resource,
                                                RESOURCE_ERROR,
                                                status);
                        break;
                    }
                }
                //Jelezzuk az eroforras sikeres initjet.
                resource->inited=true;
                //Jelzesre kerul, hogy az eroforras el van inditva.
                resource->started=true;

                //Ha van az eroforrasnak start funkcioja, akkor azt meghivja, ha
                //nincs, akkor ugy vesszuk, hogy az eroforras elindult.
                if (resource->funcs->start)
                {   //Indito callback meghivasa.

                    //A hivott callbackban hivodhat a MyRM_resourceStatus()
                    //fuggveny, melyben azonnal jelezheto, ha az eroforras
                    //elindult.
                    //Vagy a start funkcioban indithato mas folyamatok (peldaul
                    //eventek segitsegevel), melyek majd kesobbjelzik vissza a
                    //MyRM_resourceStatusCore() fuggvenyen keresztul az
                    //eroforras allapotat.

                    //A start funkcio alatt a mutexeket feloldjuk
                    MyRM_UNLOCK(rm->mutex);
                    status=resource->funcs->start(resource->funcsParam);
                    MyRM_LOCK(rm->mutex);
                    if (status)
                    {
                        MyRM_resourceStatusCore(resource,
                                                RESOURCE_ERROR,
                                                status);
                        break;
                    }

                } else
                {   //Nincs start fuggvenye. Generalunk egy "elindult" allapotot.
                    //Ennek kiertekelese majd ujra ebben a rutinban fog
                    //megtortenni, mely hatasara az eroforrasra varo usreke
                    //vagy masik eroforrasok mukodese folytathato.
                    MyRM_resourceStatusCore(resource,
                                            RESOURCE_RUN,
                                            kStatus_Success);
                }
            } else
            {   //Az eroforras mar megkapta az inditasi jelzest. Varjuk, hogy
                //jelezze, hogy elindult...

                if (resource->runFlag)
                {   //Az eroforras elindult. (Az eroforras a mukodik jelzest a
                    //status fuggveny hivasavak alitotta be.)

                    //jelzes torolheto.
                    resource->runFlag=0;

                    //jelzes, hogy az eroforras elindult.
                    resource->state=RESOURCE_STATE_RUN;
                    resource->runningFlag=true;

                    //Minden hasznalojaban jelzi, hogy a fuggoseguk elindult.
                    //dependecyStatus() callbackek vegighivasa
                    //[RUN]
                    MyRM_sendStatus(resource, RESOURCE_RUN);

                    //ujra kerjuk a kiertekelest, igy az a RUN agban ellen-
                    //orizheti, hogy az inditasi folyamat kozben nem mondtak-e
                    //le megis az eroforrasrol, mert ha igen, akkor el kell
                    //inditani annak leallitasat.
                    resource->checkStartStopReq=true;
                } else
                {   //Az eroforras meg nincs elinditva. Varakozunk tovabb...

                }
            } //(resource->started==false) else

            break;
        }
        //......................................................................
        case RESOURCE_STATE_STOPPING:
        {
            //Az eroforras all le, vagy hiba miatt le kell allitani, vagy varjuk
            //a leallasi folyamatainak lezarasat...
            if (resource->haltReq)
            {   //Hiba miatt az eroforrast le kell allitani. Ezt a jelzest a
                //status fuggevenyben allitottak be.

                //leallitasi kerelem torlese...
                resource->haltReq=false;

                //Jelezzuk az ot hasznalo magasabb szinten levo eroforrasoknal
                //es usereknek a hibat...
                //Az eroforras hasznaloi enenk hatasara szinten hiba allapotot
                //vesznek, fel, mely hatasara azok is elkezdik a leallst.
                //[ERROR]
                MyRM_sendStatus(resource, RESOURCE_ERROR);

                //Ugrik az eroforrast leallito reszre. (igy egyszerubb)
                goto stop_resource;
            } else
            {   //Varjuk az eroforras leallasanak jelzeset, vagy a hiba torleset

                if (resource->usageCnt)
                {   //Meg vannak hasznaloi. Meg kell varni, hogy mindenki
                    //lemondjon rola...
                    break;
                }

                if (resource->haltedFlag==false)
                {   //Az eroforras meg nem zarta le a belso folyamatait.
                    //varunk tovabb....
                    break;
                }
                resource->haltedFlag=false;

                //Az eroforras leallt, es mar senki sem hasznalja.

                if (resource->errorFlag)
                {   //Az eroforras hiba allapotban volt. Torolheto a hiba.
                    #if MyRM_TRACE
                    printf("MyRM: Resource error cleared.  %s\n", resource->resourceName);
                    #endif
                    resource->errorFlag=false;
                    //A riportolt hiba is torlesre kerul.
                    resource->reportedError=NULL;
                }

                //Elinditottsag jelzesenek torlese.
                resource->started=false;

                //Az eroforras felveszi a leallitott allapotot
                resource->state=RESOURCE_STATE_STOP;

                //Csokkenteheto a futo eroforrasok szamlalojat a managerben
                MyRM_decrementRunningResourcesCnt(rm);
                #if MyRM_TRACE
                printf("MyRM: decremented running resources count.  %s  cnt:%d\n", resource->resourceName, rm->runningResourceCount);
                #endif

                //Az eszkoz fuggosegeiben is lemondja a hasznalatot.
                //Vegig fut a fuggosegeken, es kiadja azokra a leallitasi
                //kerelmet...
                resourceDep_t* dep=resource->dependencyList.first;
                while(dep)
                {
                    MyRM_stopDependency(dep);

                    //lancolt list akovetkezo elemere allas.
                    dep=(resourceDep_t*)dep->nextDependency;
                }

                //Minden userenek jelzi, hogy az eroforras leallt, vagy vegzett.
                //Itt csak az userek kaphatnak jelzest, melyek nincsnenek
                //kesleltetve...
                //[STOP/DONE]
                MyRM_sendStatus(resource, resource->doneFlag ?  RESOURCE_DONE
                                                             :  RESOURCE_STOP);
                resource->doneFlag=false;


                //A jelzes kiadasa utan vegig kell majd nezni az eroforras
                //hasznaloit, es ha van, kesleltetett inidtasi kerelem, akkor
                //azokat ervenyre juttatni.
                //Vegighalad az eroforrss osszes hasznalojan...
                dep=resource->requesterList.first;
                while(dep)
                {
                    if (dep->delayedStartRequest)
                    {   //a vizsgalt kerelmezo varta az eroforars leallasat.

                        //A jelzes torlesre kerul.
                        dep->delayedStartRequest=false;

                        //Noveljuk a hasznalati szamlalot.
                        resource->usageCnt++;
                    }

                    //lancolt list akovetkezo elemere allas.
                    dep=(resourceDep_t*)dep->nextRequester;
                }
            } //(resource->haltReq) else
            break;
        }
        //......................................................................
        case RESOURCE_STATE_STOP:
        {
            //Az eroforras le van allitva

            if (resource->usageCnt==0)
            {   //Nincs hasznaloja. Nincs mit tenni. Varunk ujabb hasznalati
                //igenybe...

            } else
            {   //Van hasznalati kerelem.

                //Az eroforras felveszi az inditasi allapotot...
                resource->state=RESOURCE_STATE_STARTING;

                //Noveljuk a futo eroforrasok szamat a managerben.
                MyRM_incrementRunningResourcesCnt(rm);
                #if MyRM_TRACE
                printf("MyRM: incremented running resources count.  %s  cnt:%d\n", resource->resourceName, rm->runningResourceCount);
                #endif

                //Az eszkoz fuggosegeiben elo kell irni a hasznalatot.
                //Vegig fut a fuggosegeken, es kiadja azokra az inditasi
                //kerelmet...
                resourceDep_t* dep=resource->dependencyList.first;
                while(dep)
                {
                    MyRM_startDependency(dep);

                    //lancolt list akovetkezo elemere allas.
                    dep=(resourceDep_t*)dep->nextDependency;
                }

                //ujra kerjuk a kiertekelest, igy a STARTING agban ellen-
                //orizheti, hogy az inditasi feltetelek teljesultek-e,
                //az osszes fuggosege el van-e inditva, ha egyaltalan van
                //fuggosege.
                resource->checkStartStopReq=true;
            }
            break;
        }
        //......................................................................
        case RESOURCE_STATE_RUN:
        {
            //Az eroforras mukodik
            if (resource->usageCnt)
            {   //Az eroforrasnak meg vannak hasznaloi. Mukodik tovabb...
                //...
            } else
            {   //Az eroforars eddig mukodott, de most mar nincsnek hasznaloi.
                //Az eroforarst le kell allitani.
                resource->state=RESOURCE_STATE_STOPPING;

stop_resource:
                ///eroforras leallitasa...
                //Itt a kod egszerusitese miatt, goto-val ide ugrik, ha hiba
                //miatt is le kell allitani az eroforrast...

                if (resource->runningFlag)                                      //?????????????????????
                {   //Az eroforras fut. Korabban ezt mar jelezte a kerelmezoi
                    //fele.

                    resource->runningFlag=false;

                    //Minden hasznalojaban jelzi, hogy a fuggoseg leall.
                    //Ezt ugy teszi, hogy meghivja azok dependencyStop()
                    //funkciojukat. Abban azok novelik a depCnt szamlalojukat
                    //[STOPPING]
                    MyRM_sendStatus(resource, RESOURCE_STOPPING);
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
                    MyRM_UNLOCK(rm->mutex);
                    status_t status=resource->funcs->stop(resource->funcsParam);
                    MyRM_LOCK(rm->mutex);
                    //resource->started=false;
                    if (status)
                    {
                        MyRM_resourceStatusCore(resource,
                                                RESOURCE_ERROR,
                                                status);
                    }
                } else
                {   //Nincs stop fuggvenye. Generalunk egy "leallt" allapotot.
                    //Vagy ha ugyan van stop fuggvenye, de az eroforras meg nem
                    //indult el. (Peldaul meg vart a fuggosegeire.)
                    //Ennek kiertekelese majd ujra a taszkban fog megtortenni,
                    //mely hatasara az eroforrasra varo userek vagy masik
                    //eroforrasok mukodese folytathato.
                    MyRM_resourceStatusCore(resource,
                                            RESOURCE_STOP,
                                            kStatus_Success);
                }
            }
            break;
        }
        //......................................................................
        default:
            //ismeretlen allapot.
            ASSERT(0);
            while(1);
    } //switch(resource->state)
}
//------------------------------------------------------------------------------
//Eroforras allapotainak kezelese
static inline void MyRM_processResource(MyRM_t* rm, resource_t* resource)
{        
    //Kell ellenorizni az eroforras leallitasra, vagy inditasra?
    //Amig iylen jellegu kereseket allit be, akar statusz, akar a kiertekelo
    //logika (MyRM_checkStartStop), addig a kiertekelest ujra le kell futtatni.
    while(resource->checkStartStopReq)
    {
        //Ellenorzesi kerelem torlese.
        resource->checkStartStopReq=false;

        MyRM_checkStartStop(rm, resource);
    } //while
}
//------------------------------------------------------------------------------
//Az eroforrast hasznalok fele statusz kuldese az eroforras allapotarol
static void MyRM_sendStatus(resource_t* resource,
                            resourceStatus_t resourceStatus)
{
    //Vegighalad az eroforrss osszes hasznalojan...
    resourceDep_t* dep=resource->requesterList.first;
    while(dep)
    {

        if (dep->depStatusFunc)
        {   //Tartozik hozza beregisztralt statusz callback.
            //(Megj: eroforars fele jelzesnel a MyRM_dependencyStatusCB()
            // fuggveny hivodik meg.
            // User eseten pedig a MyRM_user_resourceStatusCB() hivodik meg.)

            dep->depStatusFunc((struct resourceDep_t*)dep,
                               resourceStatus,
                               resource->reportedError);
        }

        //lancolt list akovetkezo elemere allas.
        dep=(resourceDep_t*)dep->nextRequester;
    }
}
//------------------------------------------------------------------------------
//Egy eroforras fuggosegi hibaja eseten hivodo fuggveny
//[MyRM_dependencyStatusCB - hivja]
static inline void MyRM_dependencyError(MyRM_t* rm,
                                        resource_t* resource,
                                        resource_t* faultyDependency)
{
    #if MyRM_TRACE
    printf("  DEPENDENCY ERROR!   %s(%d) ---> %s(%d)\n",
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
    MyRM_resourceStatusCore(resource, RESOURCE_ERROR, 0);

    //eloirjuk az eroforras kiertekeleset...
    MyRM_addResourceToProcessReqList(rm, resource);
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha egy eroforras fuggosege elindult
//[MyRM_dependencyStatusCB - hivja]
static inline void MyRM_dependenyStarted(MyRM_t* rm, resource_t* resource)
{
    #if MyRM_TRACE
    printf("[%s]   MyRM_dependenyStarted\n", resource->resourceName);
    #endif

    //A fuggosegek szamanak csokkentese...

    if (resource->depCnt==0)
    {   //A fuggoseg szamlalo elkeveredett.
        ASSERT(0);
        while(1);
    }
    resource->depCnt--;

    if (resource->state==RESOURCE_STATE_STARTING)
    {   //Az eroforras inditasra var...

        resource->checkStartStopReq=true;
        MyRM_addResourceToProcessReqList(rm, resource);
    }
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha egy eroforras fuggosege le lesz allitva
//[MyRM_dependencyStatusCB - hivja]
static inline void MyRM_dependenyStop(MyRM_t* rm, resource_t* resource)
{
    (void) rm;

    #if MyRM_TRACE
    printf("[%s]   MyRM_dependenyStop\n", resource->resourceName);
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
//Az egyes fuggosegek ezen keresztul jeleznek az oket igenylo magasabb szinten
//levo eroforrasok fele. Vagy forditva, egy eroforras fuggosege ezen keresztul
//jelzi az allapotat.
static void MyRM_dependencyStatusCB(struct resourceDep_t* dep,
                                    resourceStatus_t status,
                                    resourceErrorInfo_t* errorInfo)
{
    (void) errorInfo;
    //A magasabb szinten levo hasznalo
    resource_t* requester= (resource_t*) ((resourceDep_t*) dep)->requesterResource;
    //A jelzest ado fuggoseg
    resource_t* dependency=(resource_t*) ((resourceDep_t*) dep)->requiredResource;

    #if MyRM_TRACE
    printf("MyRM_dependencyStatusCB()  %s --[%s]--> %s\n",
           dependency->resourceName,
           MyRM_resourceStatusStrings[status],
           requester->resourceName);
    #endif

    //elagazas a fuggoseg statusza alapjan...
    switch(status)
    {
        case RESOURCE_STOP:
            //Az fuggoseg leallt
            break;

        case RESOURCE_RUN:
            //A fuggoseg elindult.
            MyRM_dependenyStarted(&myRM, dependency);
            break;

        case RESOURCE_STOPPING:
            //A fuggoseg megkezdte a leallasat.
            MyRM_dependenyStop(&myRM, dependency);
            break;

        case RESOURCE_DONE:
            //Azeroforars befejezte a mukodeset. (egyszer lefuto esetekben)
            break;

        case RESOURCE_ERROR:
            //A fuggoseg mukodese/initje hibara futott.

            //Azok az igenybevevok, melyek fuggosege hiba maitt all le, es
            //az inditasi kerelmuket a leallasi folyamat alatt jeleztek,
            //nem kaphatnak hiba jelzest. Azok varjak, hogy a fuggoseguk
            //elobb lealljon, majd a keresuk ervenyre jutasa utan ujrainduljon.
            if (((resourceDep_t*) dep)->delayedStartRequest==true)
            {   //nem juthat ervenyre

            } else
            {   //A hasznalo hiba allapotba allitasa...
                //(Megj: a kiertekelesi igenyt a hivot rutin allitja be.)
                MyRM_dependencyError(&myRM, requester, dependency);
            }
            break;
    }
}
//------------------------------------------------------------------------------
//Fuggoseg inditasi kerelme (dependencia leiro alapjan.)
static void MyRM_startDependency(resourceDep_t* dep)
{
    //A magasabb szinten levo eroforras, mely igenyli az eroforrast
    resource_t* requester= (resource_t*) ((resourceDep_t*) dep)->requesterResource;
    //Az inditando fuggoseg/eroforras
    resource_t* dependency=(resource_t*) ((resourceDep_t*) dep)->requiredResource;

    #if MyRM_TRACE
    printf("MyRM_startDependency()  %s ----> %s\n",
           requester == NULL ? "(user)" : requester->resourceName,
           dependency->resourceName);
    #endif


    if ((requester) && (requester->state==RESOURCE_STATE_STOPPING))
    {   //Az eroforras leallasi folyamatban van. Ezt a kerest csak az utan
        //lehet ervenyre juttatni, miutan az igenyelt eroforras leallt.
        //A kerelmet eltaroljuk.
        dep->delayedStartRequest=true;

        //Az inditando eroforras a leallasa utan vegignezi majd az igenyloi
        //dependencia leiroit, es ha talal bennuk kesleltetett kerest, akkor
        //azokat ervenyre juttatja.

        //nem tesz semmit...
    } else
    {   //Az eroforrast igenybe vesszuk...

        if (dependency->usageCnt==0)
        {   //Az eroforrasnak ez lett az elso hasznaloja.
            //Ki kell ertekelni az allapotokat..
            dependency->checkStartStopReq=true;
        }

        //Noveljuk a hasznaloinak szamat.
        dependency->usageCnt++;

        //eloirjuk az eroforras kiertekeleset...
        MyRM_addResourceToProcessReqList(&myRM, dependency);
    }
}
//------------------------------------------------------------------------------
//Fuggoseg leallitasi kerelme (dependencia leiro alapjan.)
static void MyRM_stopDependency(resourceDep_t* dep)
{
    //A magasabb szinten levo eroforras, mely lemond az eroforrasrol
    resource_t* requester= (resource_t*) ((resourceDep_t*) dep)->requesterResource;
    //A lemondott fuggoseg/eroforras
    resource_t* dependency=(resource_t*) ((resourceDep_t*) dep)->requiredResource;

    #if MyRM_TRACE
    printf("MyRM_stopDependency()  %s ----> %s\n",
           requester == NULL ? "" : requester->resourceName,
           dependency->resourceName);
    #endif

    //Az eroforarst hasznalok szamanak cskkenetese
    if (dependency->usageCnt==0)
    {   //Az eroforras kezelesben hiba van. Nem mondhatnanak le tobben,
        //mint amennyien korabban igenybe vettek!
        ASSERT(0);
        return;
    }
    dependency->usageCnt--;


    if (dependency->usageCnt==0)
    {   //Elfogyott minden hasznalo.
        //Ki kell ertekelni az allapotokat
        dependency->checkStartStopReq=true;

        //eloirjuk az eroforras kiertekeleset...
        MyRM_addResourceToProcessReqList(&myRM, dependency);
    }

}
//------------------------------------------------------------------------------
//Inditasi kerelmek kezelese
//[MyRM_task-bol hivva]
#if 0
static inline void MyRM_startRequest(MyRM_t* rm, resource_t* resource)
{
    (void) rm;

    #if MyRM_TRACE
    printf("MyRM_startRequest()  %s\n", resource->resourceName);
    #endif
    if (resource->debugFlag)
    {
        __NOP();
        __NOP();
        __NOP();
    }

    if (resource->usageCnt==0)
    {   //Az eroforrasnak lett hasznaloja
        //Ki kell ertekelni az allapotokat
        resource->checkStartStopReq=true;
    } else
    {
        //Jelezni kell az userek fele...
        resource->signallingUsers=true;
    }

    //Az eroforrast hasznalok szamat annyival noveljuk, amennyi inditasi
    //kerelem futott be az utolso feldolgozas ota...
    resource->usageCnt += resource->startReqCnt;
    //A kerelmek szamat toroljuk, igy az a kovetkezo feldolgozasig az addig
    //befutottakat fogja majd ujra akkumulalni.
    resource->startReqCnt = 0;




/*
    if (resource->errorFlag)
    {   //Az eroforras hibas allapotot mutat. Nem inditunk el belso muveleteket,
        //csak jelezzuk az ot hasznalo magasabb szinten levo eroforrasoknal es
        //usereknek a hibat....

        MyRM_addToRequestersDepErrorList(rm, resource);
        //[ERROR]
        resource->signallingUsers=true;
        return;
    }
*/

/*
    switch((int) resource->state)
    {
        case RESOURCE_STATE_RUN:
            //Az eroforras mar el van inditva.
            //Jelezni kell az eroforarst ujonnan igenybe vevo user(ek) fele,
            //hogy az eroforras mar mukodik. Arra ne varjanak tovabb.
            //[RUN]
            resource->signallingUsers=true;
            break;

        case RESOURCE_STATE_STARTING:
            //Ha az eroforras allapota STARTING, akkor nincs mit tenni.
            //Majd ha az osszes fuggosege elindult, akkor ez is el fog indulni.
            //(Az usageCnt viszont novekedett az eroforrast haszanlni akarok
            //szamaval.)
            break;

        case RESOURCE_STATE_STOPPING:
            //Ha az eroforras STOPPING allapotban van, akkor itt nem csinalunk
            //semmit.
            //(Az usageCnt viszont novekedett az eroforrast haszanlni akarok
            //szamaval.)
            //Az eroforras leallasakor majd eszre fogja venni, hogy az usageCnt
            //nem 0, ezert az eroforrast ujra el fogja inditani, a
            //checkStartStop reszben
            break;

        case RESOURCE_STATE_STOP:
            //Az eroforras le van allitva. El kell azt inditani.
            //A checkStartStop() reszben el lesz inditva (STOP ag).
            resource->checkStartStopReq=true;
            break;

        default:
            ASSERT(0);
            return;
    }
*/
}
//------------------------------------------------------------------------------
//leallitasi kerelmek kezelese
//[MyRM_task-bol hivva]
static inline void MyRM_stopRequest(MyRM_t* rm,
                                    resource_t* resource)
{
    (void) rm;

    #if MyRM_TRACE
    printf("MyRM_stopRequest()  %s\n", resource->resourceName);
    #endif

    /*
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
            //Leallitott allapotban nem fogadhat ujabb leallitasi kerelmet
            //ASSERT(0);
            return;
            break;

        default:
            ASSERT(0);
            return;
    }
    */

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
        resource->signallingUsers=true;
    } else
    {   //Elfogyott minden hasznalo.
        //Ki kell ertekelni az alalpotokat
        resource->checkStartStopReq=true;
    }


    /*
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
    */
}
#endif
//------------------------------------------------------------------------------
//Az egyes eroforrasok ezen keresztul jelzik a manager fele az allapotvaltozasa-
//ikat.
static void MyRM_resourceStatusCore(resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode)
{
    if (resourceStatus>ARRAY_SIZE(MyRM_resourceStatusStrings))
    {   //Olyan statusz kodot kapott, ami nincs definialva!
        ASSERT(0);
        return;
    }

    #if MyRM_TRACE    
    printf("----RESOURCE STATUS (%s)[%s], errorCode:%d\n", resource->resourceName, MyRM_resourceStatusStrings[resourceStatus], (int)errorCode);
    #endif

    //Ha az erorCode hibat jelez, akkor hibara visszuk az eroforrast
    if (errorCode) resourceStatus=RESOURCE_ERROR;

    switch(resourceStatus)
    {
        //......................................................................
        case RESOURCE_RUN:
            //Az eroforras azt jelzi, hogy elindult

            if (resource->errorFlag)
            {   //Ha a folyamat akkor jelzi, hogy elindult, amikor a manager az
                //eroforrast mar hibara allitotta.
                //Ez konnyen lehet, mert peldaul egy taszkban futo inditasi fo-
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
            //inditasi folyamat a szal alatt. A STARTING agban erre var a
            //folyamat.
            resource->runFlag=true;
            resource->checkStartStopReq=true;
            break;
        //......................................................................
        case RESOURCE_STOP:
            //Az eroforras azt jelzi, hogy leallt.
            //De evvel tortenik a hiba mnegszunese utani jelzes is.

            if (resource->state != RESOURCE_STATE_STOPPING)
            {   //Az eroforras ugy kuldte a lealt jelzest, hogy az nem volt
                //leallitasi allapotban.
                //Ez szoftverhiba!
                ASSERT(0); while(1);
            }

            //if (resource->errorFlag)
            //{   //Hibas mukodes lezarasa megtortent. Az eroforras implementa-
            //    //ciojaban mar nem futnak folyamatok. Jelezzuk, hogy ha az
            //    //eroforrasrol mindenki lemondott, akkor az eroforras STOP
            //    //allapotba teheto.
            //    resource->haltedFlag=true;
            //}

            //Jelzes, hogy az eroforras befejezte a mukodeset
            resource->haltedFlag=true;

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

            //ki kell ertekelni az uj szituaciot
            resource->checkStartStopReq=true;
            break;            
        //......................................................................
        default:
            //ismeretlen, vagy nem kezelt allapottal hivtak meg a statuszt!
            ASSERT(0);
    } //switch
}
//------------------------------------------------------------------------------
#if 0
//Az eroforrast hasznalo userek fele az eroforras allapotanak jelzese.
static void MyRM_signallingUsers(MyRM_t* rm,
                                 resource_t* resource)
{
    (void) rm;
    resourceUser_t* user=(resourceUser_t*)resource->userList.first;

    //Vegig halad az eroforras userein...
    for(;user; user=(resourceUser_t*)user->userList.next)
    {
        #if MyRM_TRACE
        printf("SIGNALLING USER! user: %s   resource: %s   state:[%s]\n",
               user->userName,
               resource->resourceName,
               MyRM_resourceStateStrings[resource->state]);
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
                    //MyRM_UNLOCK(rm->mutex);
                    user->statusFunc(   RESOURCE_ERROR,
                                        resource->reportedError,
                                        user->callbackData);
                    //MyRM_LOCK(rm->mutex);
                }
            }
            continue;
        }


        //Elagazas az user aktualis allapta szerint...
        switch(user->state)
        {   //..................................................................
            case RESOURCEUSERSTATE_WAITING_FOR_START:
                //Az user varakozik az inditasi jelzesre

                if (resource->state==RESOURCE_STATE_RUN)
                {   //...es az eroforras most elindult jelzest kapott.

                    user->state=RESOURCEUSERSTATE_RUN;
                    //Jelezes az usernek, a callback fuggvenyen
                    //keresztul.
                    if (user->statusFunc)
                    {
                        //MyRM_UNLOCK(rm->mutex);
                        user->statusFunc(RESOURCE_RUN,
                                         resource->reportedError,
                                         user->callbackData);
                        //MyRM_LOCK(rm->mutex);
                    }
                }
                break;
            //..................................................................
            case RESOURCEUSERSTATE_RUN:
                //Az user meg fut, vagy
            case RESOURCEUSERSTATE_ERROR:
                //Az eroforras hiba allapotban van. varja a hiba torleset
            case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
                //Az user varakozik az eroforras leallt vagy vegzett
                //jelzesre. Ez utobbi eseten is STOP allapotba megy az
                //eroforras.

                if ( (resource->state==RESOURCE_STATE_STOP) ||
                     (resource->state==RESOURCE_STATE_RUN) )
                {   //Az eroforras leallt, vagy tovabb mukodik, mert
                    //egy vagy tobb masik eroforrasnak szuksege van ra.
                    //Ez utobbi esetben a leallast varo user fele
                    //jelezheto a leallt allapot. Igy az befejezheti a
                    //mokodeset.
                    user->state=RESOURCEUSERSTATE_IDLE;

                    if (user->restartRequestFlag)
                    {   //Az eroforrason elo van irva az ujrainditasi kerelem
                        MyRM_restartDo(rm, resource, user);
                    } else
                    {
                        if (user->statusFunc)
                        {
                            //xMyRM_UNLOCK(rm->mutex);
                            user->statusFunc(resource->doneFlag ? RESOURCE_DONE
                                                                : RESOURCE_STOP,
                                             resource->reportedError,
                                             user->callbackData);
                            //MyRM_LOCK(rm->mutex);
                        }
                    }
                }

                break;
            //..................................................................
            default:
                //ismeretelen user allapot!
                ASSERT(1); while(1);
        } //switch(user->state)
    } //while
}
//------------------------------------------------------------------------------
//Az eroforras ujrainditasanak kezdete
static void MyRM_restartDo(MyRM_t* rm,
                           resource_t* resource,
                           resourceUser_t* user)
{
    printf("___RESTART REQUEST___ %s\n", user->userName);

    //keres torlese
    user->restartRequestFlag=false;

    //Az usert az eroforras inditasara allitja
    user->state=RESOURCEUSERSTATE_WAITING_FOR_START;

    //eroforrasra inditsi kerest ad.
    MyRM_startResourceCore(rm, resource);
}
#endif
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//Az eroforrashoz az applikacio felol hivhato USER hozzaadasa.
void MyRM_addUser(resource_t* resource,
                  resourceUser_t* user,
                  const char* userName)
{
    MyRM_t* rm=&myRM;

    //user leiro kezdeti nullazasa
    memset(user, 0, sizeof(resourceUser_t));

    //A lista csak mutexelt allapotban bovitheto!
    MyRM_LOCK(rm->mutex);

    //dependencia leiro letrehozasa az elerni kivant eroforrashoz....
    user->dependency.requesterResource=(struct resource_t*) NULL;
    user->dependency.requiredResource =(struct resource_t*) resource;
    //Beallitja az eroforars altal hivando statusz fuggvenyt, melyen keresztul
    //az eroforras jelezheti az user szamara az allapotvaltozasait
    user->dependency.depStatusFunc=MyRM_user_resourceStatusCB;
    MyRM_addRequesterToResource (resource,  &user->dependency);

    //Az eroforras a hozzaadas utan keszenleti allapotot mutat. Amig nincs
    //konkret inditasi kerelem az eroforrasram addig ezt mutatja.
    //(Ebbe ter vissza, ha az eroforrast mar nem hasznaljuk, es az eroforras le
    //is allt.)
    user->state=RESOURCEUSERSTATE_IDLE;

    //User nevenek megjegyzese (ez segiti a nyomkovetest)
    user->userName=(userName!=NULL) ? userName : "?";
    MyRM_UNLOCK(rm->mutex);
}
//------------------------------------------------------------------------------
//Egy eroforrashoz korabban hozzaadott USER kiregisztralasa.
//TODO: implementalni!
void MyRM_removeUser(resource_t* resource, resourceUser_t* user)
{
    MyRM_t* rm=&myRM;
    //A lista csak mutexelt allapotban modosithato!
    MyRM_LOCK(rm->mutex);

    //TODO: implementalni

    MyRM_UNLOCK(rm->mutex);
}
//------------------------------------------------------------------------------
//Eroforras hasznalata.
//Hatasara a kert eroforras ha meg nins elinditva, elindul, majd az user-hez
//beregisztralt callbacken keresztul jelzi, annak sikeresseget, vagy hibajat.
void MyRM_useResource(resourceUser_t* user)
{
    MyRM_t* rm=&myRM;

    MyRM_LOCK(rm->mutex);

    #if MyRM_TRACE
    printf("MyRM_useResource() user:%s   state:%s\n",
           user->userName,
           MyRM_resourceUserStateStrings[user->state]);
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

            //Eroforrast inditjuk az userhez letrehozott fuggosegen keresztul
            MyRM_startDependency(&user->dependency);

            //Jelzes a taszknak...
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
                resource_t* resource=
                            ((resource_t*)user->dependency.requiredResource);

                user->statusFunc(RESOURCE_ERROR,                                
                                 resource->reportedError,
                                 user->callbackData);
            }

            break;

        default:
            //Ismeretlen allapot.
            ASSERT(0);
            break;
    }

    MyRM_UNLOCK(rm->mutex);    
}
//------------------------------------------------------------------------------
//Eroforrasrol lemondas.
//Ha az eroforras mar senki sem hasznalja, akkor az le lessz allitva.
//Az user-hez beregisztralt callbacken keresztul majd vissza fog jelezni, ha az
//eroforras mukodese befejezodott.
void MyRM_unuseResource(resourceUser_t* user)
{
    MyRM_t* rm=&myRM;

    MyRM_LOCK(rm->mutex);

    #if MyRM_TRACE
    printf("MyRM_unuseResource() user:%s   state:%s\n",
           user->userName,
           MyRM_resourceUserStateStrings[user->state]);
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
            MyRM_stopDependency(&user->dependency);

            //Jelzes a taszknak...
            MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_USE);

            break;

        case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
            //Az user mar le van allitva. Varunk annak befejezesere.
            //kilepes, es varakozas tovabb a befejezesre...
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
                resource_t* resource=
                            ((resource_t*)user->dependency.requiredResource);

                user->statusFunc(RESOURCE_ERROR,
                                 resource->reportedError,
                                 user->callbackData);
            }
            break;


        default:
            //Ismeretlen allapot.
            ASSERT(0);
            break;
    }

    MyRM_UNLOCK(rm->mutex);
}
//------------------------------------------------------------------------------
//Eroforras ujrainditasi kerelme. Hibara futott eroforrasok eseten az eroforras
//hibajanak megszunese utan ujrainditja azt.
void MyRM_restartResource(resourceUser_t* user)
{
    MyRM_t* rm=&myRM;

    MyRM_LOCK(rm->mutex);

    #if MyRM_TRACE
    printf("MyRM_restartResource() user:%s   state:%s\n",
           user->userName,
           MyRM_resourceUserStateStrings[user->state]);
    #endif

    //Elso lepesben kezdemenyezzuk az eroforras leallitasat.

    switch((int)user->state)
    {

        case RESOURCEUSERSTATE_RUN:
        case RESOURCEUSERSTATE_WAITING_FOR_START:
        case RESOURCEUSERSTATE_ERROR:
            //Az eroforras el van inidtva vagy varunk annak elindulasara.
            //Ez utobbi akkor lehet, ha inditasi folyamat kozben megis lemond
            //az user az eroforrasrol,
            //vagy az eroforras hibaja miatt az user hiba allapotban van.
            //Ebbol kilepni, az eroforras leallitasaval lehet.

            user->restartRequestFlag=true;

            //Beallitjuk, hogy varunk annak leallasara
            user->state=RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE;

            //Eroforrast leallitjuk (legalabb is bejelezzuk, hogy egy user
            //lemond rola, igy ha annak usageCnt szamlaloja 0-ra csokken, akkor
            //az eroforras le fog allni, majd ha mindenki lemondott rola, akkor
            //az ujrainditasi keres miatt az ujra fog indulni.)
            MyRM_stopDependency(&user->dependency);

            //Jelzes a taszknak...
            MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_UNUSE);
            break;

        case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
            //Az user mar le van allitva. Varunk annak befejezesere.
            //kilepes, es varakozas tovabb a befejezesre...

            //User inditasi kerelme, mely majd a leallas utan fog ervenyre
            //jutni.

            user->restartRequestFlag=true;
            break;

        case RESOURCEUSERSTATE_IDLE:
            //Az user mar le van allitva.
            //User inditasi kerelme.

            //Beallitjuk, hogy varunk annak inditasara.
            user->state=RESOURCEUSERSTATE_WAITING_FOR_START;

            //Eroforrast inditjuk az userhez letrehozott fuggosegen keresztul
            MyRM_startDependency(&user->dependency);

            //Jelzes a taszknak...
            MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_USE);
            break;


        default:
            //Ismeretlen allapot.
            ASSERT(0);
            break;
    }

    MyRM_UNLOCK(rm->mutex);
}
//------------------------------------------------------------------------------
//Az userekhez tartozo callback, melyet az altaluk hasznalt eroforras hiv az
//alalpotvaltozasuk szerint...
static void MyRM_user_resourceStatusCB(struct resourceDep_t* dep,
                                       resourceStatus_t status,
                                       resourceErrorInfo_t* errorInfo)
{
    (void) errorInfo;
    //A magasabb szinten levo hasznalo
    resource_t* requester= (resource_t*) ((resourceDep_t*) dep)->requesterResource;
    //A jelzest ado fuggoseg
    resource_t* dependency=(resource_t*) ((resourceDep_t*) dep)->requiredResource;

    #if MyRM_TRACE
    printf("MyRM_user_resourceStatusCB()  %s --[%s]--> %s\n",
           dependency->resourceName,
           MyRM_resourceStatusStrings[status],
           requester->resourceName);
    #endif

    //elagazas a fuggoseg statusza alapjan...
    switch(status)
    {
        case RESOURCE_STOP:
            //Az eroforras leallt
            break;

        case RESOURCE_RUN:
            //Az eroforars elindult.
            break;

        case RESOURCE_STOPPING:
            //Az eroforars megkezdte a leallasat.
            break;

        case RESOURCE_DONE:
            //Azeroforars befejezte a mukodeset. (egyszer lefuto esetekben)
            break;

        case RESOURCE_ERROR:
            //Az eroforars mukodese/initje hibara futott.

            ////Azok az igenybevevok, melyek fuggosege hiba maitt all le, es
            ////az inditasi kerelmuket a leallasi folyamat alatt jeleztek,
            ////nem kaphatnak hiba jelzest. Azok varjak, hogy a fuggoseguk
            ////elobb lealljon, majd a keresuk ervenyre jutasa utan ujrainduljon.
            //if (((resourceDep_t*) dep)->delayedStartRequest==true)
            //{   //nem juthat ervenyre
            //
            //} else
            //{   //A hasznalo hiba allapotba allitasa...
            //    //(Megj: a kiertekelesi igenyt a hivot rutin allitja be.)
            //    MyRM_dependencyError(&myRM, requester, dependency);
            //}
            break;
    }



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
                //MyRM_UNLOCK(rm->mutex);
                user->statusFunc(   RESOURCE_ERROR,
                                    resource->reportedError,
                                    user->callbackData);
                //MyRM_LOCK(rm->mutex);
            }
        }
        continue;
    }


    //Elagazas az user aktualis allapta szerint...
    switch(user->state)
    {   //..................................................................
        case RESOURCEUSERSTATE_WAITING_FOR_START:
            //Az user varakozik az inditasi jelzesre

            if (resource->state==RESOURCE_STATE_RUN)
            {   //...es az eroforras most elindult jelzest kapott.

                user->state=RESOURCEUSERSTATE_RUN;
                //Jelezes az usernek, a callback fuggvenyen
                //keresztul.
                if (user->statusFunc)
                {
                    //MyRM_UNLOCK(rm->mutex);
                    user->statusFunc(RESOURCE_RUN,
                                     resource->reportedError,
                                     user->callbackData);
                    //MyRM_LOCK(rm->mutex);
                }
            }
            break;
        //..................................................................
        case RESOURCEUSERSTATE_RUN:
            //Az user meg fut, vagy
        case RESOURCEUSERSTATE_ERROR:
            //Az eroforras hiba allapotban van. varja a hiba torleset
        case RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
            //Az user varakozik az eroforras leallt vagy vegzett
            //jelzesre. Ez utobbi eseten is STOP allapotba megy az
            //eroforras.

            if ( (resource->state==RESOURCE_STATE_STOP) ||
                 (resource->state==RESOURCE_STATE_RUN) )
            {   //Az eroforras leallt, vagy tovabb mukodik, mert
                //egy vagy tobb masik eroforrasnak szuksege van ra.
                //Ez utobbi esetben a leallast varo user fele
                //jelezheto a leallt allapot. Igy az befejezheti a
                //mokodeset.
                user->state=RESOURCEUSERSTATE_IDLE;

                if (user->restartRequestFlag)
                {   //Az eroforrason elo van irva az ujrainditasi kerelem
                    MyRM_restartDo(rm, resource, user);
                } else
                {
                    if (user->statusFunc)
                    {
                        //xMyRM_UNLOCK(rm->mutex);
                        user->statusFunc(resource->doneFlag ? RESOURCE_DONE
                                                            : RESOURCE_STOP,
                                         resource->reportedError,
                                         user->callbackData);
                        //MyRM_LOCK(rm->mutex);
                    }
                }
            }

            break;
        //..................................................................
        default:
            //ismeretelen user allapot!
            ASSERT(1); while(1);
    } //switch(user->state)
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
