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
static void MyRM_deleteResourceFromProcessReqList(MyRM_t* rm,
                                                  resource_t* resource);
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
static void MyRM_startDependency(resourceDep_t* dep);
static void MyRM_stopDependency(resourceDep_t* dep);
static void MyRM_resourceStatusCore(resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode);
static void MyRM_user_resourceStatusCB(struct resourceDep_t* dep,
                                       resourceStatus_t status,
                                       resourceErrorInfo_t* errorInfo);
static void MyRM_restartDo(resourceUser_t* user);

//Manager taszkot ebreszteni kepes eventek definicioi
#define MyRM_NOTIFY__RESOURCE_START_REQUEST     BIT(0)
#define MyRM_NOTIFY__RESOURCE_STOP_REQUEST      BIT(1)
#define MyRM_NOTIFY__RESOURCE_STATUS            BIT(2)
#define MyRM_NOTIFY__RESOURCE_USE               BIT(3)
#define MyRM_NOTIFY__RESOURCE_UNUSE             BIT(4)


#define MyRM_LOCK(mutex)    xSemaphoreTakeRecursive(mutex, portMAX_DELAY)
#define MyRM_UNLOCK(mutex)  xSemaphoreGiveRecursive(mutex)

static const char* MyRM_resourceStateStrings[]=RESOURCE_STATE_STRINGS;
#if MyRM_TRACE
static const char* MyRM_resourceStatusStrings[]=RESOURCE_STATUS_STRINGS;
#endif
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
    printf("%-12s", resource->resourceName ? resource->resourceName : "???");

    //Eroforras allapotanak kiirasa
    if (resource->state>ARRAY_SIZE(MyRM_resourceStateStrings))
    {   //illegalis allapot. Ez csak valami sw hiba miatt lehet
        printf("  [???]");
    } else
    {
        printf("  [%-8s]", MyRM_resourceStateStrings[resource->state]);
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
                                            (int)resource->flags.started,
                                            (int)resource->errorInfo.errorCode);

    printf("\n");


    if (printDeps)
    {

        printf("                           errorFlag: %d runningFlag: %d  haltReq: %d  :haltedFlag: %d\n",
               (int)resource->flags.error,
               (int)resource->flags.running,
               (int)resource->flags.haltReq,
               (int)resource->flags.halted);

        if (resource->reportedError)
        {
            resourceErrorInfo_t* ei=resource->reportedError;
            printf("              Error Initiator: %s   ErrorCode: %d  State: %d\n",
                                            ((resource_t*) ei->resource)->resourceName,
                                            ei->errorCode,
                                            ei->resourceState);
        }


        //fuggosegek kilistazasa...
        printf("                Deps: ");
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
            printf("[%s] ",MyRM_resourceStateStrings[((resource_t*)dep->requiredResource)->state]);
            //Kovetkezo elemre lepunk a listaban
            dep=(resourceDep_t*)dep->nextDependency;
        }


        //Felhasznalok kilistazasa...
        printf("\n                Reqs: ");

        resourceDep_t* requester=resource->requesterList.first;
        while(requester)
        {
            const char* requesterName;

            if (requester->flags.requesterType==RESOURCE_REQUESTER_TYPE__RESOURCE)
            {
                requesterName=((resource_t*)requester->requesterResource)->resourceName;
                if (requesterName==NULL) requesterName="???";
                //Nev es allapot kiirasa
                printf("%s[%s]",
                       requesterName,
                       MyRM_resourceStateStrings[((resource_t*)requester->requesterResource)->state]
                       );

            } else
            {
                requesterName=((resourceUser_t*)requester->requesterUser)->userName;
                if (requesterName==NULL) requesterName="???";
                printf("(U)%s[%s]",
                       requesterName,
                       MyRM_resourceUserStateStrings[((resourceUser_t*)requester->requesterUser)->state]
                       );
            }

            printf("  ");

            //Kovetkezo elemre lepunk a listaban
            requester=(resourceDep_t*)requester->nextRequester;
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
//Eroforras letrehozasa.
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra.
void MyRM_createResource(resource_t* resource, const resource_config_t* cfg)
{
    memset(resource, 0, sizeof(resource_t));

    resource->state=RESOURCE_STATE_STOP;
    memcpy(&resource->funcs, &cfg->funcs, sizeof(resourceFuncs_t));
    resource->funcsParam=cfg->callbackData;
    resource->resourceName=(cfg->name==NULL) ? "???" :cfg->name;
    resource->ext=cfg->ext;

    //Az eroforrast hozzaadjuk a rendszerben levo eroforrasok listajahoz...
    MyRM_addResourceToManagedList(resource);
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
    //jelezzuk, hogy egy eroforras az igenylo.
    dep->flags.requesterType=RESOURCE_REQUESTER_TYPE__RESOURCE;
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
    //Nem is fut mar elvileg eroforras, megis csokkentik a  futok szamat. Ez hiba.
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
        {   //Volt utanna is. (Ez egy kozbulso listaelem)
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
//Taszkot ebreszteni kepes notify event kuldese
static inline void MyRM_sendNotify(MyRM_t* rm, uint32_t eventBits)
{
    xTaskNotify(rm->taskHandle, eventBits, eSetBits);
}
//------------------------------------------------------------------------------
#if MyRM_TRACE
static void MyRM_dumpResourceValues(resource_t* resource)
{
    printf("::: %12s  [%-8s]:: {started:%x running:%x inited:%x error:%x done:%x haltReq:%x halted:%x run:%x} usageCnt:%2d  depCnt:%2d :::\n",
           resource->resourceName,
           MyRM_resourceStateStrings[resource->state],
           (int)resource->flags.started,
           (int)resource->flags.running,
           (int)resource->flags.inited,
           (int)resource->flags.error,
           (int)resource->flags.done,
           (int)resource->flags.haltReq,
           (int)resource->flags.halted,
           (int)resource->flags.run,
           (int)resource->usageCnt,
           (int)resource->depCnt
          );
}
#endif
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
if (resource->flags.debug)
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
//Annak ellenorzese, hogy az eroforrast el kell-e inditani, vagy le kell-e
//allitani. Hiba eseten ebben oldjuk meg a hiba miatti leallst is.
//[MyRM_task-bol hivva]
//#pragma GCC push_options
//#pragma GCC optimize ("-O0")
static inline void MyRM_checkStartStop(MyRM_t* rm, resource_t* resource)
{
    status_t status;

    if (resource->flags.done)
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

        //Jelzes, hogy az eroforras leallt. Igy a STOPPING allapotban le tud
        //futni a befejezes.
        resource->flags.halted=true;
    }


    //Elagazas az eroforras aktualis allapota szerint...
    switch((int)resource->state)
    {   //..................................................................
        case RESOURCE_STATE_STARTING:
        {
            //Az eroforras most indul...
            if (resource->flags.started==false)
            {   //Az eroforras meg nincs elinditva. Varjuk, hogy az inditashoz
                //szukseges feltetelek teljesuljenek. Varjuk, hogy az osszes
                //fuggosege elinduljon...

                if (resource->depCnt!=0)
                {   //Az eroforrasnak meg van nem elindult fuggosege.
                    //Tovabb varakozunk, hogy az osszes fuggosege elinduljon...
                    break;
                }

                //Az eroforrasnak mar nincsenek inditasra varo fuggosegei
                //Az eroforras indithato.

                //Eroforras initek...
                //resource->doneFlag=false;
                //resource->reportedError=NULL;

                //Ha az eroforras meg nincs inicializalva, es van beregisztralva
                //init() fuggvenye, akkor meghivjuk. Ez az eroforras elete alatt
                //csak egyszer tortenik.
                if ((resource->flags.inited==false) && (resource->funcs.init))
                {
                    MyRM_UNLOCK(rm->mutex);
                    status=resource->funcs.init(resource->funcsParam);
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
                resource->flags.inited=true;
                //Jelzesre kerul, hogy az eroforras el van inditva.
                resource->flags.started=true;

                //Ha van az eroforrasnak start funkcioja, akkor azt meghivja, ha
                //nincs, akkor ugy vesszuk, hogy az eroforras elindult.
                if (resource->funcs.start)
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
                    status=resource->funcs.start(resource->funcsParam);
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

                if (resource->flags.run)
                {   //Az eroforras elindult. (Az eroforras a mukodik jelzest a
                    //status fuggveny hivasavak alitotta be.)

                    //jelzes torolheto.
                    resource->flags.run=0;

                    //jelzes, hogy az eroforras elindult.
                    resource->state=RESOURCE_STATE_RUN;
                    resource->flags.running=true;

                    //Minden hasznalojaban jelzi, hogy a fuggoseguk elindult.
                    //dependecyStatus() callbackek vegighivasa
                    //[STARTED]
                    MyRM_sendStatus(resource, RESOURCE_STARTED);

                    //ujra kerjuk a kiertekelest, igy az a RUN agban ellen-
                    //orizheti, hogy az inditasi folyamat kozben nem mondtak-e
                    //le megis az eroforrasrol, mert ha igen, akkor el kell
                    //inditani annak leallitasat.
                    resource->flags.checkStartStopReq=true;
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
            if (resource->flags.haltReq)
            {   //Hiba miatt az eroforrast le kell allitani. Ezt a jelzest a
                //status fuggevenyben allitottak be.

                //leallitasi kerelem torlese...
                resource->flags.haltReq=false;

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

                if (resource->flags.halted==false)
                {   //Az eroforras meg nem zarta le a belso folyamatait.
                    //varunk tovabb....
                    break;
                }
                resource->flags.halted=false;

                //Az eroforras leallt, es mar senki sem hasznalja.

                if (resource->flags.error)
                {   //Az eroforras hiba allapotban volt. Torolheto a hiba.
                    #if MyRM_TRACE
                    printf("MyRM: Resource error cleared.  %s\n",
                           resource->resourceName);
                    #endif
                    resource->flags.error=false;
                    //A riportolt hiba is torlesre kerul.
                    resource->reportedError=NULL;
                }

                //Elinditottsag jelzesenek torlese.
                resource->flags.started=false;

                //Az eroforras felveszi a leallitott allapotot
                resource->state=RESOURCE_STATE_STOP;

                //Csokkenteheto a futo eroforrasok szamlalojat a managerben
                MyRM_decrementRunningResourcesCnt(rm);
                #if MyRM_TRACE
                printf("MyRM: decremented running resources count.  %s  cnt:%d\n",
                       resource->resourceName,
                       rm->runningResourceCount);
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
                MyRM_sendStatus(resource,
                                resource->flags.done ?  RESOURCE_DONE
                                                         :  RESOURCE_STOP);
                resource->flags.done=false;


                //A jelzes kiadasa utan vegig kell majd nezni az eroforras
                //hasznaloit, es ha van, kesleltetett inidtasi kerelem, akkor
                //azokat ervenyre juttatni.
                //Vegighalad az eroforras osszes hasznalojan...
                dep=resource->requesterList.first;
                while(dep)
                {
                    if (dep->flags.delayedStartRequest)
                    {   //a vizsgalt kerelmezo varta az eroforras leallasat.

                        //A jelzes torlesre kerul.
                        dep->flags.delayedStartRequest=false;

                        //Noveljuk a hasznalati szamlalot.
                        resource->usageCnt++;
                    }

                    //lancolt list akovetkezo elemere allas.
                    dep=(resourceDep_t*)dep->nextRequester;
                }

                if (resource->usageCnt)
                {   //Vannak ujra hasznaloi. Ki kell ertekelni ezt az eroforrast
                    resource->flags.checkStartStopReq=true;
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
                printf("MyRM: incremented running resources count.  %s  cnt:%d\n",
                       resource->resourceName,
                       rm->runningResourceCount);
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
                resource->flags.checkStartStopReq=true;
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
            {   //Az eroforras eddig mukodott, de most mar nincsnek hasznaloi.
                //Az eroforrast le kell allitani.
                resource->state=RESOURCE_STATE_STOPPING;

stop_resource:
                ///eroforras leallitasa...
                //Itt a kod egszerusitese miatt, goto-val ide ugrik, ha hiba
                //miatt is le kell allitani az eroforrast...

                if (resource->flags.running)                                      //?????????????????????
                {   //Az eroforras fut. Korabban ezt mar jelezte a kerelmezoi
                    //fele.

                    resource->flags.running=false;

                    //Minden hasznalojaban jelzi, hogy a fuggoseg leall.
                    //Ezt ugy teszi, hogy meghivja azok dependencyStop()
                    //funkciojukat. Abban azok novelik a depCnt szamlalojukat
                    //[STOPPING]
                    MyRM_sendStatus(resource, RESOURCE_STOPPING);
                }

                //Ha van az eroforrasnak stop funkcioja, akkor azt meghivja, ha
                //nincs, akkor ugy vesszuk, hogy az eroforras leallt.
                if ((resource->funcs.stop) && (resource->flags.started))
                {   //Leallito callback meghivasa.

                    //A hivott callbackban hivodhat a MyRM_resourceStatus()
                    //fuggveny, melyben azonnal jelezheto, ha az eroforras le-
                    //allt. Vagy a stop funkcioban indithato mas folyamatok
                    //(peldaul eventek segitsegevel), melyek majd kesobbjelzik
                    //vissza a MyRM_resourceStatusCore() fuggvenyen keresztul
                    //az eroforras allapotat.

                    //A stop funkcio alatt a mutexeket feloldjuk
                    MyRM_UNLOCK(rm->mutex);
                    status_t status=resource->funcs.stop(resource->funcsParam);
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



    if (resource->flags.statusRequest)
    {   //Valamelyik hasznaloja keri, hogy kuldjon az allapotarol statuszt.

        #if MyRM_TRACE
        printf("!!!MyRM: Sending requested status...\n");
        #endif

        resource->flags.statusRequest=0;

        resourceStatus_t resourceStatus=RESOURCE_STOP;
        switch((int)resource->state)
        {
            case RESOURCE_STATE_RUN:
                resourceStatus=RESOURCE_RUN;
                break;
            case RESOURCE_STATE_STOP:
                resourceStatus=RESOURCE_STOP;
                break;
            case RESOURCE_STATE_STARTING:
                resourceStatus=RESOURCE_STOP;
                break;
            case RESOURCE_STATE_STOPPING:
                if (resource->flags.error)
                    resourceStatus=RESOURCE_ERROR;
                else
                    resourceStatus=RESOURCE_STOPPING;
                break;
            default:
                //Ez az allapot elvileg nem lehetne.
                ASSERT(0);
                break;
        }

        //Vegighalad az eroforras osszes hasznalojan...
        resourceDep_t* dep=resource->requesterList.first;
        while(dep)
        {
            if (dep->flags.statusRequest==1)
            {   //A soron levo hasznalonak kell kuldeni statuszt.

                //A flaget lehet torolni.
                dep->flags.statusRequest=0;

                if (dep->depStatusFunc)
                {   //Tartozik hozza beregisztralt statusz callback.
                    //Megj:
                    //-Eroforras fele jelzesnel a MyRM_dependencyStatusCB()
                    // fuggveny hivodik meg.
                    //-User eseten pedig a MyRM_user_resourceStatusCB()
                    // hivodik meg.)
                    dep->depStatusFunc((struct resourceDep_t*)dep,
                                       resourceStatus,
                                       resource->reportedError);
                }
            }


            //lancolt list akovetkezo elemere allas.
            dep=(resourceDep_t*)dep->nextRequester;
        }
    }
}
//#pragma GCC pop_options
//------------------------------------------------------------------------------
//Eroforras allapotainak kezelese
static inline void MyRM_processResource(MyRM_t* rm, resource_t* resource)
{        
    //Kell ellenorizni az eroforras leallitasra, vagy inditasra?
    //Amig ilyen jellegu kereseket allit be, akar statusz, akar a kiertekelo
    //logika (MyRM_checkStartStop), addig a kiertekelest ujra le kell futtatni.
    while(resource->flags.checkStartStopReq)
    {
        //Ellenorzesi kerelem torlese.
        resource->flags.checkStartStopReq=false;

        MyRM_checkStartStop(rm, resource);
    } //while
}
//------------------------------------------------------------------------------
//Az eroforrast hasznalok fele statusz kuldese az eroforras allapotarol
static void MyRM_sendStatus(resource_t* resource,
                            resourceStatus_t resourceStatus)
{
    //Vegighalad az eroforras osszes hasznalojan...
    resourceDep_t* dep=resource->requesterList.first;
    while(dep)
    {
        //Ha ez egy kerelmezesre kiadott statusz volt, akkor a flaget lehet
        //torolni.
        dep->flags.statusRequest=0;

        if (dep->depStatusFunc)
        {   //Tartozik hozza beregisztralt statusz callback.
            //(Megj: eroforras fele jelzesnel a MyRM_dependencyStatusCB()
            // fuggveny hivodik meg.
            // User eseten pedig a MyRM_user_resourceStatusCB() hivodik meg.)

            dep->depStatusFunc((struct resourceDep_t*)dep,
                               resourceStatus,
                               resource->reportedError);
        }


        //lancolt lista kovetkezo elemere allas.
        dep=(resourceDep_t*)dep->nextRequester;
    }

    //A beregisztralt statusz kerelmezesek kiszolgalasa...
    resourceStatusRequest_t* nextStatusReq=
                    (resourceStatusRequest_t*)resource->firstStatusRequester;
    while(nextStatusReq)
    {
        if (nextStatusReq->statusFunc)
        {   //A beregisztralt status callback meghivasa
            nextStatusReq->statusFunc(  resource,
                                        resourceStatus,
                                        resource->reportedError,
                                        nextStatusReq->callbackData);
        }
        //lepes a lancolt listaban
        nextStatusReq = (resourceStatusRequest_t*)nextStatusReq->next;
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
    printf("MyRM_dependenyStarted  --->%s  [%s]\n",
           resource->resourceName,
           MyRM_resourceStateStrings[resource->state]
           );
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

        resource->flags.checkStartStopReq=true;
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

    if (((resourceDep_t*) dep)->flags.requesterType!=RESOURCE_REQUESTER_TYPE__RESOURCE)
    {   //Hiba!
        ASSERT(0);
    }

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

        case RESOURCE_STARTED:
            //A fuggoseg elindult.
            MyRM_dependenyStarted(&myRM, requester);
            break;

        case RESOURCE_RUN:
            //A fuggoseg fut.
            if (requester->state==RESOURCE_STATE_STARTING)
            {   //A kerelmezo eroforras inditasra var...
                //Le kell futtatni a fuggosegej vizsgalatat.
                requester->flags.checkStartStopReq=true;
                MyRM_addResourceToProcessReqList(&myRM, requester);
            }
            break;

        case RESOURCE_STOPPING:
            //A fuggoseg megkezdte a leallasat.
            MyRM_dependenyStop(&myRM, requester);
            break;

        case RESOURCE_DONE:
            //Azeroforras befejezte a mukodeset. (egyszer lefuto esetekben)
            break;

        case RESOURCE_ERROR:
            //A fuggoseg mukodese/initje hibara futott.

            //Azok az igenybevevok, melyek fuggosege hiba maitt all le, es
            //az inditasi kerelmuket a leallasi folyamat alatt jeleztek,
            //nem kaphatnak hiba jelzest. Azok varjak, hogy a fuggoseguk
            //elobb lealljon, majd a keresuk ervenyre jutasa utan ujrainduljon.
            if (((resourceDep_t*) dep)->flags.delayedStartRequest==true)
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
    //Az inditando fuggoseg/eroforras
    resource_t* dependency=(resource_t*) ((resourceDep_t*) dep)->requiredResource;

    #if MyRM_TRACE
        const char* requesterName=NULL;
        if (dep->flags.requesterType==RESOURCE_REQUESTER_TYPE__RESOURCE)
        {   //Egy eroforras igenyli az inditast
            requesterName=((resource_t*)dep->requesterResource)->resourceName;
        } else
        {   //Egy user kezdemenyezi az inditast
            requesterName=((resourceUser_t*)dep->requesterUser)->userName;
        }
        if (requesterName==NULL) requesterName="???";
        printf("MyRM_startDependency()  %s%s ----> %s\n",
               (dep->flags.requesterType==RESOURCE_REQUESTER_TYPE__USER) ? "(U)"
                                                                         : "",
               requesterName,
               dependency->resourceName);
    #endif



    if (dependency->state==RESOURCE_STATE_STOPPING)
    {   //Az eroforras leallasi folyamatban van. Ezt a kerest csak az utan
        //lehet ervenyre juttatni, miutan az igenyelt eroforras leallt.
        //A kerelmet eltaroljuk.
        dep->flags.delayedStartRequest=true;

        //Az inditando eroforras a leallasa utan vegignezi majd az igenyloi
        //dependencia leiroit, es ha talal bennuk kesleltetett kerest, akkor
        //azokat ervenyre juttatja.

        //nem tesz semmit...
    } else
    {   //Az eroforrast igenybe vesszuk...

        //Noveljuk a hasznaloinak szamat.
        dependency->usageCnt++;


        //Biztositani kell tudni, hogy az inditast kero kapjon statuszt az
        //allapotrol.
        //stop: el fog indulni. ->majd a taszkbol kap jelzest
        //stopping: nem fut le, mert az elozo feltetel ezt kiejti
        //starting: el fog indulni. ->majd a taszkbol kap jelzest
        //run: ezt az esetet kezelni kellene, mert a taszkban erre nem
        //     tortenik semmi.        
        if ((dependency->state==RESOURCE_STATE_RUN) && (dependency->usageCnt>1))
        {   //Eloirjuk, hogy a taszkbol kapjon jelzest
            #if MyRM_TRACE
            printf("S t a t u s  r e q u e s t! %s\n", requesterName);
            #endif
            dep->flags.statusRequest=1;
            dependency->flags.statusRequest=1;
            dependency->flags.checkStartStopReq=1;

            //eloirjuk az eroforras kiertekeleset...
            MyRM_addResourceToProcessReqList(&myRM, dependency);
        } else
        if (dependency->usageCnt==1)
        {   //Az eroforrasnak ez lett az elso hasznaloja.
            //Ki kell ertekelni az allapotokat..
            dependency->flags.checkStartStopReq=1;

            //eloirjuk az eroforras kiertekeleset...
            MyRM_addResourceToProcessReqList(&myRM, dependency);
        }
    }
}
//------------------------------------------------------------------------------
//Fuggoseg leallitasi kerelme (dependencia leiro alapjan.)
static void MyRM_stopDependency(resourceDep_t* dep)
{
    //A lemondott fuggoseg/eroforras
    resource_t* dependency=(resource_t*) ((resourceDep_t*) dep)->requiredResource;

    #if MyRM_TRACE
        const char* requesterName=NULL;
        if (dep->flags.requesterType==RESOURCE_REQUESTER_TYPE__RESOURCE)
        {   //Egy eroforras igenyli az inditast
            requesterName=((resource_t*)dep->requesterResource)->resourceName;
        } else
        {   //Egy user kezdemenyezi az inditast
            requesterName=((resourceUser_t*)dep->requesterUser)->userName;
        }
        if (requesterName==NULL) requesterName="???";
        printf("MyRM_stopDependency()  %s%s ----> %s\n",
               (dep->flags.requesterType==RESOURCE_REQUESTER_TYPE__USER) ? "(U)" : "",
               requesterName,
               dependency->resourceName);
    #endif

    //Az eroforrast hasznalok szamanak cskkenetese
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
        dependency->flags.checkStartStopReq=true;

        #if MyRM_TRACE
            printf(" %s  usageCnt==0\n", dependency->resourceName);
        #endif

        //eloirjuk az eroforras kiertekeleset...
        MyRM_addResourceToProcessReqList(&myRM, dependency);
    }

}
//------------------------------------------------------------------------------
//Az egyes eroforrasok ezen keresztul jelzik a manager fele az allapotvaltozasa-
//ikat.
static void MyRM_resourceStatusCore(resource_t* resource,
                                    resourceStatus_t resourceStatus,
                                    status_t errorCode)
{

    #if MyRM_TRACE
    if (resourceStatus>ARRAY_SIZE(MyRM_resourceStatusStrings))
    {   //Olyan statusz kodot kapott, ami nincs definialva!
        ASSERT(0);
        return;
    }

    printf("----RESOURCE STATUS (%s)[%s], errorCode:%d\n",
           resource->resourceName,
           MyRM_resourceStatusStrings[resourceStatus],
           (int)errorCode);
    #endif

    //Ha az erorCode hibat jelez, akkor hibara visszuk az eroforrast
    if (errorCode) resourceStatus=RESOURCE_ERROR;

    switch(resourceStatus)
    {
        //......................................................................
        case RESOURCE_RUN:
            //Az eroforras azt jelzi, hogy elindult

            if (resource->flags.error)
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
            resource->flags.run=true;
            resource->flags.checkStartStopReq=true;
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
            resource->flags.halted=true;

            //A ckeckStartStop() reszben "STARTING" agban befejezodik az
            //inditasi folyamat a szal alatt.
            resource->flags.checkStartStopReq=true;
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
            resource->flags.done=true;
            //Az allapot atmegy STOPPING-ba, amig a szalban be nem allitja ra a
            //stop-ot.
            resource->state=RESOURCE_STATE_STOPPING;
            //Le kell futtatni a start/stop igeny ellenorzot
            resource->flags.checkStartStopReq=true;
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

            if (resource->flags.error==true)
            {   //Az eroforras mar hibara van futtatva. Meg egy hibat nem fogad.
                break;
            }

            //Hibas allapot beallitasa
            resource->flags.error=true;

            //Hiba informacios struktura feltoltese, melyet majd kesobb az
            //eroforrast hasznalo masik eroforrasok, vagy userek fele
            //tovabb adhatunk...
            //Reportoljuk, hogy melyik eroforrassal van a hiba
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
            resource->flags.haltReq=true;

            //ki kell ertekelni az uj szituaciot
            resource->flags.checkStartStopReq=true;
            break;            
        //......................................................................
        default:
            //ismeretlen, vagy nem kezelt allapottal hivtak meg a statuszt!
            ASSERT(0);
    } //switch
}
//------------------------------------------------------------------------------
//Eroforrashoz statusz callback beregisztralasa. Egy eroforrashoz tobb ilyen
//kerelem is beregisztralhato.
void MyRM_addResourceStatusRequest(resource_t* resource,
                                   resourceStatusRequest_t* request)
{
    MyRM_t* rm=&myRM;
    MyRM_LOCK(rm->mutex);

    if (resource->firstStatusRequester==NULL)
    {   //ez lesz az elso beregisztralt kerelmezo
        resource->firstStatusRequester=(struct resourceStatusRequest_t*)request;
    } else
    {
        //Lista vegenek keresese. Addig lepked, mig a listat lezaro NULL-ra nem
        //talal.
        resourceStatusRequest_t* item=(resourceStatusRequest_t*)
                                                resource->firstStatusRequester;
        while(item->next)
        {
            item = (resourceStatusRequest_t*)item->next;
        }
        item->next=(struct resourceStatusRequest_t*)request;
    }
    request->next=NULL;

    MyRM_UNLOCK(rm->mutex);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//Az eroforras hasznalatat legetove tevo user letrehozasa
void MyRM_createUser(resourceUser_t* user, const resourceUser_config_t* cfg)
{
    MyRM_t* rm=&myRM;

    //user leiro kezdeti nullazasa
    memset(user, 0, sizeof(resourceUser_t));

    //A lista csak mutexelt allapotban bovitheto!
    MyRM_LOCK(rm->mutex);

    //dependencia leiro letrehozasa az elerni kivant eroforrashoz....
    user->dependency.requesterResource=(struct resource_t*) NULL;
    user->dependency.requiredResource =(struct resource_t*) cfg->resource;
    //Jelezzuk a dependenciaban, hogy egy user a birtokosa.
    user->dependency.flags.requesterType=RESOURCE_REQUESTER_TYPE__USER;
    user->dependency.requesterUser=(struct resourceUser_t*)user;
    //Beallitja az eroforras altal hivando statusz fuggvenyt, melyen keresztul
    //az eroforras jelezheti az user szamara az allapotvaltozasait
    user->dependency.depStatusFunc=MyRM_user_resourceStatusCB;
    MyRM_addRequesterToResource (cfg->resource,  &user->dependency);

    //Az eroforras a hozzaadas utan keszenleti allapotot mutat. Amig nincs
    //konkret inditasi kerelem az eroforrasram addig ezt mutatja.
    //(Ebbe ter vissza, ha az eroforrast mar nem hasznaljuk, es az eroforras le
    //is allt.)
    user->state=RESOURCEUSERSTATE_IDLE;

    user->restartRequestFlag=false;

    //User nevenek megjegyzese (ez segiti a nyomkovetest)
    user->userName=(cfg->name!=NULL) ? cfg->name : "?";

    //Az user allapotvaltozasakor hivhato statusz callback
    user->statusFunc=cfg->statusFunc;
    user->callbackData=cfg->callbackData;

    MyRM_UNLOCK(rm->mutex);
}
//------------------------------------------------------------------------------
//Egy eroforrashoz korabban hozzaadott USER megszuntetese
void MyRM_deleteUser(resourceUser_t* user)
{
    MyRM_t* rm=&myRM;
    (void) user;
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
            //Generalunk az applikacio fele jelzest, hogy az eroforras hibas.
            if (user->statusFunc)
            {
                resource_t* resource=
                            ((resource_t*)user->dependency.requiredResource);

                user->statusFunc(resource,
                                 RESOURCE_ERROR,
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
//Ha az eroforras mar senki sem hasznalja, akkor az le lesz allitva.
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
            MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_UNUSE);

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

                user->statusFunc(resource,
                                 RESOURCE_STOP,
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
//allapotvaltozasuk szerint...
static void MyRM_user_resourceStatusCB(struct resourceDep_t* dep,
                                       resourceStatus_t status,
                                       resourceErrorInfo_t* errorInfo)
{
    (void) errorInfo;

    if (((resourceDep_t*) dep)->flags.requesterType!=RESOURCE_REQUESTER_TYPE__USER)
    {   //Hiba!
        ASSERT(0);
    }

    //A magasabb szinten levo hasznalo (user)
    resourceUser_t* user= (resourceUser_t*) ((resourceDep_t*) dep)->requesterUser;
    //A jelzest ado fuggoseg/eroforras
    resource_t* resource=(resource_t*) ((resourceDep_t*) dep)->requiredResource;

    #if MyRM_TRACE
    printf("MyRM_user_resourceStatusCB()  %s --[%s]--> %s[%s]\n",
           resource->resourceName,
           MyRM_resourceStatusStrings[status],
           user->userName,
           MyRM_resourceUserStateStrings[user->state]
           );
    #endif


    if (user->state==RESOURCEUSERSTATE_IDLE)
    {   //Amig nincs hasznalatban az user reszerol az eroforras, addig
        //nem kaphat jelzest.
        //Ilyen az, amig nincs elinditva az eroforras, vagy ebbe az
        //allapotba kerul, ha a hasznalt eroforras leallt.
        return;
    }


    if (status==RESOURCE_ERROR)
    {   //Az eroforras hibara futott.

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
                user->statusFunc(   resource,
                                    RESOURCE_ERROR,
                                    resource->reportedError,
                                    user->callbackData);
                //MyRM_LOCK(rm->mutex);
            }
        }
        return;
    }


    //Elagazas az user aktualis allapta szerint...
    switch(user->state)
    {   //..................................................................
        case RESOURCEUSERSTATE_WAITING_FOR_START:
            //Az user varakozik az inditasi jelzesre

            if ((status==RESOURCE_RUN) || (status==RESOURCE_STARTED))
            {   //...es az eroforras most elindult jelzest kapott.

                user->state=RESOURCEUSERSTATE_RUN;
                //Jelezes az usernek, a callback fuggvenyen
                //keresztul.
                if (user->statusFunc)
                {
                    //MyRM_UNLOCK(rm->mutex);
                    user->statusFunc(resource,
                                     RESOURCE_RUN,
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

            if ( (status==RESOURCE_STOP) ||
                 (status==RESOURCE_DONE) ||
                 (status==RESOURCE_STARTED) ||
                 (status==RESOURCE_RUN) )
            {   //Az eroforras leallt, vagy tovabb mukodik, mert
                //egy vagy tobb masik eroforrasnak szuksege van ra.
                //Ez utobbi esetben a leallast varo user fele
                //jelezheto a leallt allapot. Igy az befejezheti a
                //mokodeset.
                user->state=RESOURCEUSERSTATE_IDLE;

                if (user->restartRequestFlag)
                {   //Az eroforrason elo van irva az ujrainditasi kerelem
                    MyRM_restartDo(user);
                } else
                {
                    if (user->statusFunc)
                    {
                        //xMyRM_UNLOCK(rm->mutex);
                        user->statusFunc(resource,
                                         (status==RESOURCE_DONE) ? RESOURCE_DONE
                                                                 : RESOURCE_STOP,
                                         errorInfo,
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
//Az eroforras ujrainditasanak kezdete
static void MyRM_restartDo(resourceUser_t* user)
{
    #if MyRM_TRACE
    printf("___RESTART REQUEST___\n");
    #endif

    //keres torlese
    user->restartRequestFlag=false;

    //Az usert az eroforras inditasara allitja
    user->state=RESOURCEUSERSTATE_WAITING_FOR_START;

    //eroforrasra inditsi kerest ad.
    MyRM_startDependency(&user->dependency);
}
//------------------------------------------------------------------------------
//Eroforras inditasa
//csak teszteleshez hasznalhato, ha kivulrol hivjuk.
void MyRM_startResource(resource_t* resource)
{
    MyRM_t* rm=&myRM;

    MyRM_LOCK(rm->mutex);
    if (resource->state==RESOURCE_STATE_STOPPING)
    {   //Az eroforras leallasi folyamatban van. Ezt a kerest csak az utan
        //lehet ervenyre juttatni, miutan az igenyelt eroforras leallt.
        //A kerelmet elutasitjuk.
        return;
    } else
    {   //Az eroforrast igenybe vesszuk...

        if (resource->usageCnt==0)
        {   //Az eroforrasnak ez lett az elso hasznaloja.
            //Ki kell ertekelni az allapotokat..
            resource->flags.checkStartStopReq=true;
        }

        //Noveljuk a hasznaloinak szamat.
        resource->usageCnt++;

        //eloirjuk az eroforras kiertekeleset...
        MyRM_addResourceToProcessReqList(&myRM, resource);
    }
    MyRM_UNLOCK(rm->mutex);

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_START_REQUEST);
}
//------------------------------------------------------------------------------
//Eroforras megallitasa
//csak teszteleshez hasznalhato, ha kivulrol hivjuk.
void MyRM_stopResource(resource_t* resource)
{
    MyRM_t* rm=&myRM;

    MyRM_LOCK(rm->mutex);

    //Az eroforrast hasznalok szamanak cskkenetese
    if (resource->usageCnt==0)
    {   //Az eroforras kezelesben hiba van. Nem mondhatnanak le tobben,
        //mint amennyien korabban igenybe vettek!
        return;
    }
    resource->usageCnt--;


    if (resource->usageCnt==0)
    {   //Elfogyott minden hasznalo.
        //Ki kell ertekelni az allapotokat
        resource->flags.checkStartStopReq=true;

        //eloirjuk az eroforras kiertekeleset...
        MyRM_addResourceToProcessReqList(&myRM, resource);
    }
    MyRM_UNLOCK(rm->mutex);

    //Jelzes a taszknak...
    MyRM_sendNotify(rm, MyRM_NOTIFY__RESOURCE_STOP_REQUEST);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
