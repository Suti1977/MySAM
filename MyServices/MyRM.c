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
static void MyRDM_printResourceInfo(resource_t* resource, bool printDeps);
static void MyRM_addResourceToStartReqList(MyRM_t* rm, resource_t* resource);
static void MyRM_deleteResourceFromStartReqList(MyRM_t* rm, resource_t* resource);
static void MyRM_addResourceToStopReqList(MyRM_t* rm, resource_t* resource);
static void MyRM_deleteResourceFromStopReqList(MyRM_t* rm, resource_t* resource);
static inline void MyRM_addRequesterToResource(resource_t* resource,
                                               resourceDep_t* dep);
static inline void MyRM_addDependencyToResource(resource_t* resource,
                                                resourceDep_t* dep);
static inline void MyRM_sendNotify(MyRM_t* rm, uint32_t eventBits);

#define MyRM_NOTIFY__RESOURCE_START_REQUEST  BIT(0)
#define MyRM_NOTIFY__RESOURCE_STOP_REQUEST   BIT(1)
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
static void MyRDM_printResourceInfo(resource_t* resource, bool printDeps)
{

    static const char* resourceStateStrings[]=RESOURCE_STATE_STRINGS;

    //Eroforars nevenek kiirasa. Ha nem ismert, akkor ??? kerul kiirasra
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
        MyRDM_printResourceInfo(resource, printDeps);
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
void MyRDM_register_allResourceStoppedFunc(MyRM_allResourceStoppedFunc_t* func,
                                           void* callbackData)
{
    MyRM_t* rm=&myRM;
    xSemaphoreTake(rm->mutex, portMAX_DELAY);
    rm->allResourceStoppedFunc=func;
    rm->callbackData=callbackData;
    xSemaphoreGive(rm->mutex);
}
//------------------------------------------------------------------------------
//Eroforras hozzaadasa az inditando eroforrasok listajahoz
static void MyRM_addResourceToStartReqList(MyRM_t* rm, resource_t* resource)
{
    if (rm->startReqList.first==NULL)
    {   //Meg nincs eleme a listanak. Ez lesz az elso.
        rm->startReqList.first=resource;
    } else
    {   //Mar van eleme a listanak. Hozzaadjuk a vegehez.
        rm->startReqList.last->startReqList.next=(struct resource_t*)resource;
    }
    //Az elozo lancszemnek a sorban korabbi legutolso elem lesz megadva
    resource->startReqList.prev=(struct resource_t*) rm->startReqList.last;
    //Nincs tovabbi elem a listaban. Ez az utolso.
    resource->startReqList.next=NULL;
    //Megjegyezzuk, hogy ez az utolso elem a sorban
    rm->startReqList.last=resource;

    //Jelezzuk, hogy az eroforras szerepel a listaban
    resource->startReqList.inTheList=true;
}
//------------------------------------------------------------------------------
//Eroforras torlese az inditando eroforrasok listajabol
static void MyRM_deleteResourceFromStartReqList(MyRM_t* rm, resource_t* resource)
{
    //A lancolt listaban a torlendo elem elott es utan allo elemre mutatnak
    resource_t* prev= (resource_t*) resource->startReqList.prev;
    resource_t*	next= (resource_t*) resource->startReqList.next;

    if (prev==NULL)
    {   //ez volt az elso eleme a listanak, mivel nincs elotte.
        //Az elso elem ezek utan az ezt koveto lesz.
        rm->startReqList.first=next;
        if (next)
        {   //all utanna a sorban.
            //Jelezzuk a kovetkezonek, hogy o lesz a legelso a sorban.
            next->startReqList.prev=NULL;
        } else
        {
            //Nincs utanna masik elem, tehat ez volt a legutolso elem a listaban
            rm->startReqList.last=NULL;
        }
    } else
    {   //volt elotte a listaban
        if (next)
        {   //Volt utanan is. (Ez egy kozbulso listaelem)
            next->startReqList.prev=(struct resource_t*) prev;
            prev->startReqList.next=(struct resource_t*) next;
        } else
        {   //Ez volt a legutolso elem a listaban
            //Az elozo elembol csinalunk legutolsot.
            prev->startReqList.next=NULL;

            //Az elozo elem a listaban lesz a kezeles szempontjabol a legutolso
            //a sorban.
            rm->startReqList.last=prev;
        }
    }

    //Toroljuk a jelzest, hogy az eroforras szerepel a listaban.
    resource->startReqList.inTheList=false;
}
//------------------------------------------------------------------------------
//Eroforras hozzaadasa a leallitando eroforrasok listajahoz
static void MyRM_addResourceToStopReqList(MyRM_t* rm, resource_t* resource)
{
    if (rm->stopReqList.first==NULL)
    {   //Meg nincs eleme a listanak. Ez lesz az elso.
        rm->stopReqList.first=resource;
    } else
    {   //Mar van eleme a listanak. Hozzaadjuk a vegehez.
        rm->stopReqList.last->stopReqList.next=(struct resource_t*)resource;
    }
    //Az elozo lancszemnek a sorban korabbi legutolso elem lesz megadva
    resource->stopReqList.prev=(struct resource_t*) rm->stopReqList.last;
    //Nincs tovabbi elem a listaban. Ez az utolso.
    resource->stopReqList.next=NULL;
    //Megjegyezzuk, hogy ez az utolso elem a sorban
    rm->stopReqList.last=resource;

    //Jelezzuk, hogy az eroforras szerepel a listaban
    resource->startReqList.inTheList=true;
}
//------------------------------------------------------------------------------
//Eroforras torlese a leallitando eroforrasok listajabol
static void MyRM_deleteResourceFromStopReqList(MyRM_t* rm, resource_t* resource)
{
    //A lancolt listaban a torlendo elem elott es utan allo elemre mutatnak
    resource_t* prev= (resource_t*) resource->stopReqList.prev;
    resource_t*	next= (resource_t*) resource->stopReqList.next;

    if (prev==NULL)
    {   //ez volt az elso eleme a listanak, mivel nincs elotte.
        //Az elso elem ezek utan az ezt koveto lesz.
        rm->stopReqList.first=next;
        if (next)
        {   //all utanna a sorban.
            //Jelezzuk a kovetkezonek, hogy o lesz a legelso a sorban.
            next->stopReqList.prev=NULL;
        } else
        {
            //Nincs utanna masik elem, tehat ez volt a legutolso elem a listaban
            rm->stopReqList.last=NULL;
        }
    } else
    {   //volt elotte a listaban
        if (next)
        {   //Volt utanan is. (Ez egy kozbulso listaelem)
            next->stopReqList.prev=(struct resource_t*) prev;
            prev->stopReqList.next=(struct resource_t*) next;
        } else
        {   //Ez volt a legutolso elem a listaban
            //Az elozo elembol csinalunk legutolsot.
            prev->stopReqList.next=NULL;

            //Az elozo elem a listaban lesz a kezeles szempontjabol a legutolso
            //a sorban.
            rm->stopReqList.last=prev;
        }
    }

    //Toroljuk a jelzest, hogy az eroforras szerepel a listaban.
    resource->startReqList.inTheList=false;
}
//------------------------------------------------------------------------------
//Az egyes eroforrasok ezen keresztul jelzik a manager fele az allapotvaltozasa-
//ikat.
//Megj: Ezt a callbacket az eroforrasok a Start vagy Stop funkciojukbol is
//hivhatjak!
void MyRDM_resourceStatus(resource_t* resource,
                          resourceStatus_t resourceStatus,
                          status_t errorCode)
{
    MyRM_t* rm=&myRM;

}
//------------------------------------------------------------------------------
status_t MyRM_useResourceCore(resource_t* resource)
{
    status_t status=kStatus_Success;
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);


    //Eroforrasban jelezzuk az inditasi kerelmet, de csak, ha az meg
    //nincs kiadva. Ezt onnan lehet tudni, hogy az eroforras szerepel-e az
    //inditando modulok listajan.
    if (resource->startReqList.inTheList==false)
    {   //Ez eroforras kerelme meg nem szerepel az inditandok listajaban.
        //Hozzaadjuk.
        MyRM_addResourceToStartReqList(rm, resource);

        //Jelzes a taszknak...
        MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_START_REQUEST);
    }

    xSemaphoreGive(rm->mutex);

    return status;
}
//------------------------------------------------------------------------------
status_t MyRM_unuseResourceCore(resource_t* resource)
{
    status_t status=kStatus_Success;
    MyRM_t* rm=&myRM;

    xSemaphoreTake(rm->mutex, portMAX_DELAY);

    //Eroforrasban leallitasi kerelem jelzese, de csak, ha az meg
    //nincs kiadva. Ezt onnan lehet tudni, hogy az eroforras szerepel-e a
    //leallitando modulok listajan.
    if (resource->stopReqList.inTheList==false)
    {   //Ez eroforras kerelme meg nem szerepel a leallitandok listajaban.
        //Hozzaadjuk.
        MyRM_addResourceToStartReqList(rm, resource);

        //Jelzes a taszknak...
        MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_START_REQUEST);
    }

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
        //Vegig az inditando eroforrasok listajan. Keressuk az elsot a sorban,
        //melyen van inditasi kerelem, es amely indithato...
        resource_t* resource=rm->startReqList.first;
        while(resource)
        {
            //Ellenorzes, hogy a soron levo eroforras inditahato-e...
            if (resource->depCnt==0)
            {   //az eroforrasnak nincs mar fuggosege.

            }


            //kovetkezo eroforrasra allas.
            resource=(resource_t*) resource->startReqList.next;
        } //while(resource)
        xSemaphoreGive(rm->mutex);


    } //while(1)
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

