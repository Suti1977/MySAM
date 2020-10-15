//------------------------------------------------------------------------------
//  Resource Dependency Manager
//
//    File: MyRDM.c
//------------------------------------------------------------------------------
#include "MyRDM.h"
#include <string.h>
#include "MyTaskedResource.h"

//Ha hasznaljuk a modult, akkor annak valtozoira peldanyt kell kesziteni.
//Az alabbi makrot el kell helyezni valahol a forrasban, peldaul main.c-ben
//MyRDM_INSTANCE

static inline status_t MyRDM_dependencyStarted(resource_t* resource);
static inline void MyRDM_dependencyStop(resource_t* resource);
static inline status_t MyRDM_requesterStopped(resource_t* resource);
static inline status_t MyRDM_resourceStarted(resource_t* resource);
static inline status_t MyRDM_startResourceDeps(resource_t* resource);
static void MyRDM_addResource(resource_t* resource);

static void MyRDM_signallingUsers(resource_t* resource,
                                      resourceStatus_t resourceStatus,
                                      status_t errorCode);
static inline void MyRDM_sendErrorSignalToReqesters(resource_t* resource,
                                                        resourceStatus_t resourceStatus,
                                                        status_t errorCode);
static void MyRDM_generalUserStatusCallback(resourceStatus_t resourceStatus,
                                                status_t errorCode,
                                                void* callbackData);
static void MyRDM_incrementRunningResourcesCnt(void);
static void MyRDM_decrementRunningResourcesCnt(void);
//------------------------------------------------------------------------------
//Eroforras management reset utani kezdeti inicializalasa
void MyRDM_init(void)
{
    MyRDM_t* this=&myrdm;

    //Manager valtozoinak kezdeti torlese
    memset(this, 0, sizeof(MyRDM_t));

    //Managerhez tartozo mutex letrehozasa
  #if configSUPPORT_STATIC_ALLOCATION
    this->mutex=xSemaphoreCreateMutexStatic(&this->mutexBuffer);
  #else
    this->mutex=xSemaphoreCreateMutex();
  #endif
}
//------------------------------------------------------------------------------
//Egyedi eroforras letrehozasa, bovitmeny megadasaval
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra.
void MyRDM_createResourceExt(resource_t* resource,
                          const resourceFuncs_t* funcs,
                          void* funcsParam,
                          const char* resourceName,
                          void* ext)
{
    memset(resource, 0, sizeof(resource_t));

    //Eroforrashoz tartozo mutex letrehozasa
    //(statik)
  #if configSUPPORT_STATIC_ALLOCATION
    resource->mutex=xSemaphoreCreateRecursiveMutexStatic(&resource->mutexBuffer);
  #else
    resource->mutex=xSemaphoreCreateRecursiveMutex();
  #endif

    resource->state=RESOURCE_STATE_UNKNOWN;
    resource->funcs=funcs;
    resource->funcsParam=funcsParam;
    resource->resourceName=(resourceName==NULL) ? "?" :resourceName;
    resource->ext=ext;

    //Az eroforrast hozzaadjuk a rendszerben levo eroforrasok listajahoz...
    MyRDM_addResource(resource);
}
//------------------------------------------------------------------------------
//Eroforras letrehozasa.
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra.
void MyRDM_createResource(resource_t* resource,
                          const resourceFuncs_t* funcs,
                          void* funcsParam,
                          const char* resourceName)
{
    MyRDM_createResourceExt(resource, funcs, funcsParam, resourceName, NULL);
}
//------------------------------------------------------------------------------
//A rendszerben levo eroforrasok listajahoz adja a hivatkozott eroforras kezelot.
static void MyRDM_addResource(resource_t* resource)
{
    MyRDM_t* man=&myrdm;

    //A manager valtozoinak mutexelt vedese
    xSemaphoreTake(man->mutex, portMAX_DELAY);

    if (man->firstResource==NULL)
    {   //Ez lesz az elso eleme a listanak. Lista kezdese
        man->firstResource=resource;
    } else
    {   //Van elotte elem        
        man->lastResource->next=(struct resource_t*) resource;
    }

    //Ez lesz az utolso eleme a listanak.
    resource->next=NULL;
    man->lastResource=resource;

    xSemaphoreGive(man->mutex);
}
//------------------------------------------------------------------------------
//Egyetlen eroforras allapotat es hasznaloit irja ki a konzolra
static void MyRDM_printResourceInfo(resource_t* resource, bool printDeps)
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

        if (resource->firstUser !=NULL)
        {   //Van az eroforrasnak hasznaloja

            printf("\n              Users: ");

            //felhasznalok kilistazasa
            resourceUser_t* user=(resourceUser_t*)resource->firstUser;
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
                user=(resourceUser_t*)user->next;
            }
        }

        printf("\n");
    }
}
//------------------------------------------------------------------------------
//A konzolra irja a szamontartott eroforrasokat, azok allapotait, es azok
//hasznaloit. Debuggolai celra hasznalhato.
void MyRDM_printUsages(bool printDeps)
{
    vTaskSuspendAll();

    resource_t* resource=myrdm.firstResource;
    printf("---------------------RESOURCE INFO-----------------------------\n");
    while(resource)
    {
        MyRDM_printResourceInfo(resource, printDeps);
        //Kovetkezo elelem a listanak
        resource=(resource_t*)resource->next;
    }
    printf(". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .\n");
    printf("Running resources count:%d\n", (int)myrdm.runningResourceCount);
    printf("---------------------------------------------------------------\n");

    xTaskResumeAll();
}
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
//Egy eroforras fuggosegeit elindito rutin.
//A rutin vegighalad az eroforras initkor beregisztralt fuggosegein, es azokra
//meghivja az indito rutint.
//Hivodhat rekurzivan a MyRDM_useResource()- bol, vagy a callbackbol is!
static inline status_t MyRDM_startResourceDeps(resource_t* resource)
{
    status_t status=kStatus_Success;

    resourceDep_t* dependency=resource->firstDependency;
    while(dependency)
    {
        //A fuggoseg inditasanak kezdemenyzese. CSak akkor tortenik majd valami,
        //Ha az a fuggoseg meg nincs elinditva, vagy nem fut rajta egy korabbi
        //inditasi kerelem.
        //A hivaskor noni fog a hivott eroforras usageCnt szamlaloja.
        status=MyRDM_useResourceCore((resource_t*)dependency->requiredResource);

        //Hiba eseten nem folytatja a sort!
        if (status) break;

        //Kovetkezo elemre lepes a lancolt listaban
        dependency=(resourceDep_t*)dependency->nextDependency;
    }

    return status;
}
//------------------------------------------------------------------------------
//Akkor hivodik meg, ha egy eroforras elindult. Az eroforrashoz tartozo allapot
//callback hivja, vagy valamelyik mas rutin, ha kihagyhato a callback.
//Minden elindulo eroforras vegig halad a hozza tartozo kerelmezok listan (tehat
//azokon, akik fuggenek tole) es azokban csokkenti a fuggosegi szamlalok er-
//teket.
//!!! Ugy kell hivni, hogy az elindul eroforras mutexelve van !!!
static inline status_t MyRDM_resourceStarted(resource_t* resource)
{
    status_t status=kStatus_Success;

    //Vegig halad az eroforrast kerelmezok listajan...
    resourceDep_t* requester=resource->firstRequester;
    while(requester)
    {
        //Jelzes a kerelmezonek...
        //A kerelmezo fuggosegi szamlaloja csokkenni fog.
        //Ha az 0-ra csokken, akkor a fuggo eroforras el fog indulni.
        status=MyRDM_dependencyStarted((resource_t*)requester->requesterResource);

        //Hiba eseten nem folytatja a sort!
        if (status) break;

        //Kovetkezo elemre lepes a lancolt listaban
        requester=(resourceDep_t*)requester->nextRequester;
    }

    return status;
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha az eroforras valamelyik fuggosege elindult, mely
//alapfeltetele a mukodesenek.
//Minden elindulo fuggoseg hatasara csokkeni fog a sajat fuggosegi szamlaloja,
//tehat fogynak a meg varando elofeltetelek. Ha 0-ra csokken, akkor azt jelenti,
//hogy az eroforras maga is indithato, annak hivhato a start() fuggvenye.
//De csak akkor hivjuk a start fuggvenyet, ha az eroforras allapota azt mutatja,
//hogy elinditasi allapotban (STARTING) van.
//Ha az eroforras nincs elinditva (STOP) akkor egyszeruen csak a fuggosegi
//szamlaloja fog csokkenni. Egy kesobbi inditasi kerelem eseten mar ez jelzi,
//hogy mennyi fuggoseg van meg hatra. Ott majd csak azok fognak elindulni,
//amik meg nincsenek elinditva.
static inline status_t MyRDM_dependencyStarted(resource_t* resource)
{
    status_t status=kStatus_Success;

    //Az eroforras valtozoit csak mutexelten lehet valtoztatni, mivel ez a rutin
    //tobb masik taszkban futo folyamtbol is meghivodhat
    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);

    //Csokkentheto a fuggosegi szamlaloja, mivel egyel kevesebb fuggosegre kell
    //mar varnia.
    if (resource->depCnt==0)
    {   //SW hiba! Nem lehet 0!
        printf("MyRDM error. DepCnt.\n");
        ASSERT(0);
        while(1);
    }
    resource->depCnt--;

    if (resource->depCnt==0)
    {   //Az eroforrashoz tartozo osszes fuggoseg elindult.
        //Ha az eroforrasra szukseg van, akkor azt is inditjuk...

        switch(resource->state)
        {
            case RESOURCE_STATE_UNKNOWN:
                //Lehet olyan eset, hogy egy eroforrasra meg nem volt szukseg,
                //viszont kozos fuggosege van egy vele egy szinten levo masik
                //eroforrassal.
                //Amikor  a kozos eroforras elindul, akkor jelzest kuld ennek az
                //eroforrasnak is, de az meg nem volt initelve.
                //Ez nem hiba. Nem csinalunk semmit. Csak a DepCnt csokkent.
                break;

            case RESOURCE_STATE_STOP:
                //Az eroforras le van allitva. Nem csinalunk semmit. Nincs
                //inditasi kerelem se.
                //Ebben az allapotban csak a DepCnt csokkentese volt a cel.
                break;

            case RESOURCE_STATE_STARTING:
                //Az eroforras el van inditva. Vartuk ezt a jelzest, hogy a
                //fuggosegek mind elinduljanak.
                //Az eroforras maga is elindithato.
                if (resource->funcs->start)
                {
                    //A startResource cimkere ugrunk.
                    //(Kevesebb stack felhasznalas)
                    //A cimke unlockolja a mutexet, majd meghivja az eroforras
                    //indito funkciojat. Utana a folytatas mar a callbackben
                    //tortenhet.
                    goto startResource;
                } else
                {   //Ha nincs callback, akkor ugy vesszuk, hogy el van inditva
                    resource->state=RESOURCE_STATE_RUN;

                    //Jelzes, hogy az eroforras el lett inditva
                    resource->started=true;

                    //Az eroforras elindult jelzes kiadasa a ra varakozokra,
                    //akik tole fuggenek. (mutexelve kell hivni)
                    status=MyRDM_resourceStarted(resource);

                    //TODO: megnezni, hogy mi tortenik, ha itt nem sucess-al jon vissza!!!!
                    //      Ez azt jelentene, hogy az initben hiba volt, es
                    //      az eroforras nem indult el. Hibara kene menni, nem
                    //      RUN-ra!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                    //Jelzes az applikacio fele, hogy az eroforras elindult
                    MyRDM_signallingUsers(resource, RESOURCE_RUN, 0);
                }
                break;

            case RESOURCE_STATE_RUN:
                //Az eroforras mar fut. Ez hiba. Az eroforras nem futhatna,
                //hiszen most keszult meg csak el az osszes fuggosege.
                printf("MyRDM error. RESOURCE_STATE_RUN.\n");
                ASSERT(0);
                while(1);
                //break;

            case RESOURCE_STATE_STOPPING:
                //Az eroforras egy korabban kiadott leallitasi folyamatban van.
                //Ez hiba.
                printf("MyRDM error. RESOURCE_STATE_STOPPING.\n");
                ASSERT(0);
                while(1);
                //break;

            //case RESOURCE_STATE_ERROR:
            default:
                //Az eroforras hibas allapotban van, vagy ismeretlen allapotkod.
                status=kStatus_Fail;
                //TODO: Kidolgozni, hogy hiba eseten mi legyen!
                printf("MyRDM error. RESOURCE_STATE_ERROR/Unknown state.\n");
                ASSERT(0);
                while(1);
                //break;
        } //switch(resource->state)
    } //if (resource->depCnt==0)

    xSemaphoreGiveRecursive(resource->mutex);
    return status;

startResource:
    //<--ide ugrik, ha az eroforrashoz tartozo indito funkciot meg kell hivni

    //Jelzes, hogy az eroforras fele el lett kuldve az inditasi kerelem.
    resource->started=true;

    //megj:
    //Az indito parancs alatt mar nem mutexelunk.
    //Ez biztositja, hogy az eroforras Start() funkcioja is hivhassa a
    //status callbacket, ha az inditas azonnak elvegezheto.

    //Az indito callbacket meghivjuk.
    status=resource->funcs->start(resource->funcsParam);

    //Ezek utan majd az eroforrashoz tartozo callbackben kapunk
    //ertesitest a leallitasi parancs kimenetelerol.

    xSemaphoreGiveRecursive(resource->mutex);
    return status;
}
//------------------------------------------------------------------------------
//Akkor hivodik meg, ha egy eroforras leallt.
//Az eroforrashoz tartozo allapot callback hivja, vagy valamelyik mas rutin,
//ha kihagyhato a callback.
//Minden fuggosege fele jelzi, hogy leallt, igy a fuggosegek egyre jobban
//felszabadulnak.
//!!! Ugy kell hivni, hogy a leallt eroforras mutexelve van !!!
static inline status_t MyRDM_resourceStopped(resource_t* resource)
{
    status_t status=kStatus_Success;

    //Az eroforras fuggosegeinek jelezzuk, hogy kevesebben fuggenek toluk...
    resourceDep_t* dependency=resource->firstDependency;
    while(dependency)
    {
        //A fuggoseg leallitasanak kezdemenyzese.
        //Csak akkor tortenik majd valami, ha a fuggosegrol mindenki lemond,
        //es ez volt az utolso hasznalo eroforras.
        status=MyRDM_requesterStopped((resource_t*)dependency->requiredResource);

        //Hiba eseten nem folytatja a sort!
        if (status) break;

        //Kovetkezo elemre lepes a lancolt listaban
        dependency=(resourceDep_t*)dependency->nextDependency;
    }

    return status;
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha valamelyik kerelmezoje (hasznaloja) leallt.
//Minden leallo kerelmezo hatasara csokkenni fog a hasznalati szamlaloja,
//es ha eleri a 0-at, akkor az eroforras is megkezdi a leallast.
static inline status_t MyRDM_requesterStopped(resource_t* resource)
{
    status_t status=kStatus_Success;

    //Az eroforras valtozoit csak mutexelten lehet valtoztatni, mivel ez a rutin
    //tobb masik taszkban futo folyamtbol is meghivodhat
    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);

    //Hasznalati szamlalo csokkentese, mivel egyel kevesebben hasznaljak
    if (resource->usageCnt==0)
    {   //SW hiba! Nem lehet 0!
        printf("MyRDM error. usageCnt.\n");
        ASSERT(0);
        while(1);
    }
    resource->usageCnt--;


    if (resource->usageCnt==0)
    {   //Az eroforrashoz tartozo osszes hasznalo elfogyott.
        //Ha meg fut az eroforras, akkor elkezdhetjuk annak leallitasat.

        switch(resource->state)
        {
            case RESOURCE_STATE_STOPPING:
                //Az eroforras leallitasa mar korabban elkezdodott.
                //Nem kell tenni semmit.
                break;

            case RESOURCE_STATE_ERROR:
                //Az eroforras hiba allapotban van. Ilyenkor lehetove kell
                //tennunk, hogy le legyen allitva, mely torli a hiba jelzest,
                //es ujra stop funkciot idez elo.
            case RESOURCE_STATE_RUN:
                //Az eroforras meg mukodik. le kell allitani.

                //Az eroforras igenyloi fele  jelezni kell, hogy az eroforras
                //nem hasznalhato, igy azoknal a fuggosegi szamlalot novelni
                //kell!
                if (resource->state != RESOURCE_STATE_ERROR)
                {
                    //A tovabbiakban azt mutatja, hogy leallasi folyamatban van.
                    resource->state=RESOURCE_STATE_STOPPING;

                    resourceDep_t* requester=resource->firstRequester;
                    while(requester)
                    {
                        //Jelzes a kerelmezonek...
                        //A kerelmezo fuggosegi szamlaloja novekedni fog.
                        MyRDM_dependencyStop((resource_t*)requester->requesterResource);

                        //Kovetkezo elemre lepes a lancolt listaban
                        requester=(resourceDep_t*)requester->nextRequester;
                    }
                } else
                {
                    //A tovabbiakban azt mutatja, hogy leallasi folyamatban van.
                    resource->state=RESOURCE_STATE_STOPPING;
                }


                if ((resource->funcs->stop)&&(resource->started==true))
                {
                    //A stopResource cimkere ugrunk. (Jobb stack felhasznalas)
                    //A cimke unlockolja a mutexet, majd meghivja az eroforras
                    //leallito funkciojat. Utana a folytatas mar a callbackben
                    //tortenhet.
                    //Csak akkor ugorhatunk a leallitasi funkciora, ha elotte az
                    //eroforras el lett inditva!
                    goto stopResource;
                } else
                {   //Ha nincs Stop funkcio,akkor ugy vesszuk, hogy le van allva

                    //Az eroforras leallt jelzes kiadasa a ra varakozokra
                    //(mutexelve kell hivni)
                    status=MyRDM_resourceStopped(resource);

                    //Az eroforras innentol leallitott allapotot mutat.
                    resource->state=RESOURCE_STATE_STOP;
                    resource->started=false;

                    //Jelzes az applikacio fele, hogy az eroforras leallt
                    MyRDM_signallingUsers(resource, RESOURCE_STOP, 0);

                    //Csokekntjuk a futo eroforrasok szamat
                    MyRDM_decrementRunningResourcesCnt();
                }
                break;

            case RESOURCE_STATE_STOP:
                //Az eroforras mar le van allitva. Ez hiba. Az eroforrasnak meg
                //mukodnie kellene.
                printf("MyRDM error. RESOURCE_STATE_STOP.\n");
                ASSERT(0);
                while(1);
                //break;

            case RESOURCE_STATE_STARTING:
                //Az eroforras egy korabban kiadott inditasi folyamatban van.
                //Ez hiba.
                printf("MyRDM error. RESOURCE_STATE_STARTING.\n");
                ASSERT(0);
                while(1);
                //break;

            default:
                break;
        } //switch(resource->state)

    } else //if (resource->usageCnt==0)
    {
        //Meg hasznalja mas az eroforrast. Nincs mit tenni. Nem kell leallitani.
    }

    xSemaphoreGiveRecursive(resource->mutex);
    return status;


stopResource:
    //<--ide ugrik, ha az eroforrashoz tartozo leallito funkciot meg kell hivni

    //A leallito parancs alatt mar nem mutexelunk.
    //Ez biztositja, hogy az eroforras Stop funkcioja is hivhassa a
    //status callbacket, ha a leallitas azonnak elvegezheto.

    //A leallito callbacket meghivjuk
    status=resource->funcs->stop(resource->funcsParam);

    //Ezek utan majd az eroforrashoz tartozo callbackben kapunk
    //ertesitest a leallitasi parancs kimenetelerol.
    xSemaphoreGiveRecursive(resource->mutex);
    return status;
}
//------------------------------------------------------------------------------
//Akkor hivodik, ha valamelyik fuggosege leall.
//A rutinban novelesre kerul a fuggoseg szamlaloja, mivel a leallt eroforras miatt
//not a fuggosegek szama. Annak ujra 0 ra kell csokkenni, ha inden elofeltetele
//megvana  mukodeshez.
static inline void MyRDM_dependencyStop(resource_t* resource)
{
    //Az eroforras valtozoit csak mutexelten lehet valtoztatni, mivel ez a rutin
    //tobb masik taszkban futo folyamtbol is meghivodhat
    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);

    //TODO: kivenni!
    if (resource->depCnt>=resource->depCount)
    {
        printf("DEPCNT BUG!!!\n");
    }

    resource->depCnt++;

    //TODO: lehetne ellenorizni, hogy nem lett-e tobb, mint amennyi fuggosege
    // valojaban van.

    xSemaphoreGiveRecursive(resource->mutex);
}
//------------------------------------------------------------------------------
/*
Egy ujabb hasznalati igeny esete.
Ha az eroforras meg sosoem volt initelve, akkor azt minden elott elvegzi.

Minden haszanlati kerelemnel novekszik a hasznalok szamlaloja (UsageCnt).

Ha az eroforras inditasa mar folyamatban van (STARTING), akkor nem csinal semmit.
Majd a callbackben kerul jelzesre az ot hasznaloknak, ha elindult az eroforras.
Mindenkinek el lesz kuldve, aki

Ha az eroforras leallitasa folyamatban van, akkor nem csinal semmit. Az eroforras ujra-
iniditasat a callbackben kell megoldani. Az ott kiadott ujrainditas vegen
fog majd jelzest kapni a hasznalo, hogy elindult az eroforras.

Ha az eroforras meg nincs elinditva (STOP), akkor atvaltunk inditas folyamatban
allapotra (STARTING).
--  Ha az eroforrashoz tartoznak fuggosegek, melyeknek el kell indulnia elobb, akkor
    azokat elinditja. Rekurzio!
    + Vegighalad a hozza beregisztralt fuggsegi lancolt listan.
    + Az eroforrashoz tartozik egy fuggoseg szamlalo, mely majd ugy csokken, ahogy
      a statusz callbackben jonnek a jelzesek,hogy valamelyik fuggoseg elindult.
    + Ahogy ez a fuggosegi szamlalo 0-ra csokkent, ugy az eroforras elindithato,
      meghivasra kerul a Start funkcioja. (Az allapota meg mindig STARTING).
      Varjuk majd a callback-et, hogy visszajojjon, hogy az eroforras elindult.
    -- Ha nincs Start funkcioja, akkor azonnal felveszi a futo allapotot, es
       az ot hasznaloknak jelez, hogy az eroforras elindult.

--  Ha nincsenek mar fuggosegei, akkor az eroforrashoz tartozo Start funkciot azonnnal
    meghivja.
    -- Ha nincs Start funkcioja, akkor azonnal felveszi a futo allapotot, es
       az ot hasznaloknak jelez, hogy az eroforras elindult.


-- Ha az eroforras mar el van inditva (RUN), akkor azert nem csinalunk semmit, mert
   errol minden hasznalonak kellet jelzest kapnia, ezert az azokban levo
   fuggosegi szamlalo mar csokkent.
*/

//Eroforras hasznalatanak kezdemenyezese.
status_t MyRDM_useResourceCore(resource_t* resource)
{
    status_t status=kStatus_Success;

    //Az eroforras valtozoit csak mutexelten lehet valtoztatni, mivel ez a rutin
    //tobb masik taszkban futo folyamtbol is meghivodhat
    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);

    if (resource->state==RESOURCE_STATE_UNKNOWN)
    {   //Ez az eroforras meg nem lett inicializalva. Az az elso lepes, minden mas
        //elott. Ezt meg a fuggosegeinek az inditasa/initje elett el fogja
        //vegezni. Ha van beregisztralva init funkcio, akkor azt meg kell
        //hivni...
        if (resource->funcs->init)
        {
            status=resource->funcs->init(resource->funcsParam);
            //Hiba eseten nem folytatjuk az inicializaciot
            if (status) goto initError;
        }
        //Az eroforras inicializalas utan leallitott allapotot vesz fel.
        resource->state=RESOURCE_STATE_STOP;
        resource->started=false;
    }


    //Noveljuk a hasznalati szamlalot, mivel egyel tobben igenylik a
    //mukodestet.
    resource->usageCnt++;



    switch(resource->state)
    {
        case RESOURCE_STATE_STOP:
            //Az eroforras le van allitva. El kell inditani.
            //Ha vannak fuggosegei, akkor elobb azokat kell elinditani. Ha
            //nincsenek, akkor azonnal indulhat.

            //Noveljuk a futo eroforrasok szamat
            MyRDM_incrementRunningResourcesCnt();

            //Az eroforras ettol kezdve inditasi folyamatot jelzo allapotot
            //fog mutatni, mindaddig, amig minden fuggosege, es o maga is el nem
            //indul.
            resource->state=RESOURCE_STATE_STARTING;

            if (resource->depCnt==0)
            {   //Az eroforrasnak nincs fuggosege, amit inditani kellene.
                //Azonnal indulhatunk...

                //xxx
                resourceDep_t* dependency=resource->firstDependency;
                while(dependency)
                {
                     resource_t* req=(resource_t*)dependency->requiredResource;

                     xSemaphoreTakeRecursive(req->mutex, portMAX_DELAY);
                     req->usageCnt++;
                     xSemaphoreGiveRecursive(req->mutex);

                    //Kovetkezo elemre lepes a lancolt listaban
                    dependency=(resourceDep_t*)dependency->nextDependency;
                }
                //xxx

                if (resource->funcs->start)
                {
                    //A startResource cimkere ugrunk. (Jobb stack felhasznalas)
                    //A cimke unlockolja a mutexet, majd meghivja az eroforras
                    //leallito funkciojat. Utana a folytatas mar a callbackben
                    //tortenhet.
                    goto startResource;
                }
            } else
            {   //Az eroforrasnak van vagy vannak fuggosegei. Azokat indijuk.
                //Az eroforrashoz tartozo fuggosegi szamlalo majd a fuggosegek
                //elindulasara csokkanni fognak 0-ra, melynek bekovetkezesekor
                //Az eroforras maga is indithato lesz. Ez viszont majd a callbackben
                //tortenik meg.                
                status=MyRDM_startResourceDeps(resource);
            }
            break;

        case RESOURCE_STATE_STARTING:
            //Az eroforras el van mar inditva. Majd varjuk a callback
            //bekovetkezeset. Addig is kilepes...
            break;

        case RESOURCE_STATE_RUN:
            //Az eroforras mar el van inditva. A callbcaken keresztul mar jelzest
            //kellet, hogy kapjon a kerelmezo eroforras, ezert itt nem csinalunk
            //semmit. Nem szabad jelzest adni a kerelmezo fele, mivel az
            //egy esetleges masik kerelmezo miatt ujra csokkentene az osszes
            //kerelmezo szamlalojat. (DepCnt-ket)
            //
            //Az userek fel viszont lehet jelezni, mivel lehet, hogy egy
            //mar mukodo eroforrashoz kapcsolodik, es varna a jelzest, hogy az
            //elindult.
            MyRDM_signallingUsers(resource, RESOURCE_RUN, 0);

            break;

        case RESOURCE_STATE_STOPPING:
            //Az eroforras egy korabban kiadott leallitasi folyamatban van.
            //Annak veget meg kell varni. Ha vegzett, akkor majd a
            //eroforrashoz tartozo status callbackben tortenhet meg annak
            //ujboli elinditasa.
            //Ezt onnan tudjuk majd, hogy az usage szamlalo nem 0, tehat a
            //leallitas alatt erkezett egy ujabb hasznalati kerelem.
            break;

        case RESOURCE_STATE_ERROR:
        default:
            //Az eroforras hibas allapotban van. Tehat egy korabban kiadott inditasi
            //vag ymegallitasi folyamat hibara futott.
            //Ebben az esetben nem inditjuk ujra az eroforrast. Egyetlen muvelet
            //engedelyezett, ha leallitjak az eroforrast.
            //
            //TODO: meggondolni, hoyg ilyenkor ezt a leallitast majd ujrain-
            //ditast nem lehetne-e automatizalni, es a callbackban megolddani.
            //TODO: esetleg masik hibakodot adni
            status=kStatus_Fail;
            break;
    }

initError:  //<--ide ugrik, ha az init funkcio alatt hibat detektalt.
    xSemaphoreGiveRecursive(resource->mutex);
    return status;

startResource:
    //<--ide ugrik, ha az eroforrashoz tartozo indito funkciot meg kell hivni

    //Jelzes, hogy az eroforras fele el lett kuldve az inditasi kerelem.
    resource->started=true;
    //Az indito parancs alatt mar nem mutexelunk.
    //Ez biztositja, hogy az eroforras Start funkcioja is hivhassa a
    //status callbacket, ha az inditas azonnak elvegezheto.

    //Az indito callbacket meghivjuk.
    status=resource->funcs->start(resource->funcsParam);

    //Ezek utan majd az eroforrashoz tartozo callbackben kapunk
    //ertesitest a leallitasi parancs kimenetelerol.
    xSemaphoreGiveRecursive(resource->mutex);
    return status;
}
//------------------------------------------------------------------------------
/*
Leallitasi kerelem eseten, akkor eloszor  az eroforras mokodese kerul leallitasra,
majd ha az vegzett, akkor a fuggosegeiben is csokkenti a hasznalati szamlalokat.

Minden lemondas csokkenti a hasznalok szamlalojat (UsageCnt).

-- Ha az eroforras mar le van allitva (STOP), akkor azert nem csinalunk semmit, mert
   errol minden hasznalonak kellet jelzest kapnia, ezert az azokban levo
   fuggosegi szamlalo mar novekedett.

Ha az eroforras leallitasa folyamatban van, akkor nem csinal semmit. Az eroforras ujra-
iniditasat a callbackben kell megoldani. Az ott kiadott ujrainditas vegen
fog majd jelzest kapni a hasznalo, hogy elindult az eroforras.


Ha az eroforras inditasa mar folyamatban van (STARTING), akkor nem csinal semmit.
Majd a callbackben kerul jelzesre az ot hasznaloknak, ha elindult az eroforras.
Mindenkinek el lesz kuldve, aki


Ha az eroforras meg nincs elinditva (STOP), akkor atvaltunk inditas folyamatban
allapotra (STARTING).
--  Ha az eroforrashoz tartoznak fuggosegek, melyeknek el kell indulnia elobb, akkor
    azokat elinditja. Rekurzio!
    + Vegighalad a hozza beregisztralt fuggsegi lancolt listan.
    + Az eroforrashoz tartozik egy fuggoseg szamlalo, mely majd ugy csokken, ahogy
      a statusz callbackben jonnek a jelzesek,hogy valamelyik fuggoseg elindult.
    + Ahogy ez a fuggosegi szamlalo 0-ra csokkent, ugy az eroforras elindithato,
      meghivasra kerul a Start funkcioja. (Az allapota meg mindig STARTING).
      Varjuk majd a callback-et, hogy visszajojjon, hogy az eroforras elindult.
    -- Ha nincs Start funkcioja, akkor azonnal felveszi a futo allapotot, es
       az ot hasznaloknak jelez, hogy az eroforras elindult.

--  Ha nincsenek mar fuggosegei, akkor az eroforrashoz tartozo Start funkciot azonnnal
    meghivja.
    -- Ha nincs Start funkcioja, akkor azonnal felveszi a futo allapotot, es
       az ot hasznaloknak jelez, hogy az eroforras elindult.


*/
//Az eroforras hasznalatanak lemondasa.
status_t MyRDM_unuseResourceCore(resource_t* resource, bool* continueWork)
{
    status_t status=kStatus_Success;

    //Az eroforras valtozoit csak mutexelten lehet valtoztatni, mivel ez a rutin
    //tobb masik taszkban futo folyamtbol is meghivodhat
    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);

    switch(resource->state)
    {
        case RESOURCE_STATE_ERROR:
            //Az eroforras hibas allapotban van. Ebben az esetben ez az egyetlen
            //mod hogy visszaterjunk a normal mukodeshez, ha elobb leallitjuk
            //az eroforrast. Az eroforrasokat ugy kell megimplementalni, hogy
            //inditasi vagy megallitasi hiba esten is lehessen hivni a STOP
            //funkciojukat, melyekben az eroforrasok mukodese alaphelyzetbe
            //kerulhet.Az eroforrast minden hasznalojanak le kell allitania,
            //hogy ervenyre jusson a leallitasi kerelem.

        case RESOURCE_STATE_RUN:
        case RESOURCE_STATE_STARTING:
            //Az eroforras mukodik, vagy mar el van inditva...

            //Az eroforras hasznalati szamlalojanak csokkentese
            if (resource->usageCnt==0)
            {   //Hiba! Nem mehetne negativba!
                printf("MyRDM error. UsageCnt!\n");
                ASSERT(0);
                while(1);
            }
            resource->usageCnt--;

            if (resource->state==RESOURCE_STATE_STARTING)
            {
                //Az eroforras el van inditva, de az meg folyamatban van.Itt most nem
                //teszunk semmit, majd csak a callbackban. Amikor a callbackban
                //visszajon a jelzes, hogy az eroforras elindult, majd meg lesz vizs-
                //galva az UsageCnt, ami ha 0, akkor azt jelenti,hogy az indulas
                //kozben megszunt minden hasznalati kerelem, ezert inkabb elkezd
                //egy leallitasi folyamatot.
                break;
            }


            if (resource->usageCnt==0)
            {   //Az eroforrasrol mindenki lemondott. Az eroforrast le kell allitani.

                //Az eroforras ettol kezdve leallitasi folyamatot jelzo allapotot
                //fog mutatni, mindaddig, amig o maga le nem allt.
                resource->state=RESOURCE_STATE_STOPPING;

                //Az eroforras igenyloi fele  jelezni kell, hogy az eroforras nem
                //hasznalhato.
                //Igy azoknal a fuggosegi szamlalot novelni kell!
                resourceDep_t* requester=resource->firstRequester;
                while(requester)
                {
                    //Jelzes a kerelmezonek...
                    //A kerelmezo fuggosegi szamlaloja novekedni fog.
                    MyRDM_dependencyStop((resource_t*)requester->requesterResource);

                    //Kovetkezo elemre lepes a lancolt listaban
                    requester=(resourceDep_t*)requester->nextRequester;
                }


                if ((resource->funcs->stop)&&(resource->started==true))
                {
                    //A stopResource cimkere ugrunk. (Jobb stack felhasznalas)
                    //A cimke unlockolja a mutexet, majd meghivja az eroforras
                    //leallito funkciojat. Utana a folytatas mar a callbackben
                    //tortenhet.
                    //Csak akkor ugorhatunk a leallitasi funkciora, ha elotte a
                    //eroforras el lett inditva!
                    goto stopResource;
                } else
                {   //Ha nincs callback, akkor ugy vesszuk, hogy le van allitva

                    //Az eroforras fuggosegeiben lemondjuk a hasznalatot. Igy a mar
                    //nem szukseges eroforrasok (melyekben az UsageCnt 0-ra
                    //csokkent, szinten leallithato...
                    //(mutexelve kell hivni)
                    status=MyRDM_resourceStopped(resource);

                    //Az eroforras ettol kezdve azt mutatja, hogy le van allitva.
                    resource->state=RESOURCE_STATE_STOP;
                    resource->started=false;

                    //Jelzes az applikacio fele, hogy az eroforras leallt
                    MyRDM_signallingUsers(resource, RESOURCE_STOP, 0);

                    //Csokekntjuk a futo eroforrasok szamat
                    MyRDM_decrementRunningResourcesCnt();
                }
            } else
            {
                //Az eroforrasnak meg vannak hasznaloi.Az eroforrasall nem teszunk semmit.
                //Meg mukodnie kell.
                //Viszont az user fele jelezni kell valahogy, hogy sikeresen
                //lemondott az eroforrasrol, igy annak leallasara nem kell varnia.
                //(Ha van beallitva hozza flag, akkor jelezzuk)
                if (continueWork) *continueWork=true;

            }
            break;

        case RESOURCE_STATE_STOP:
            //Az eroforras le van allva. A callbcaken keresztul mar jelzest
            //kellet, hogy kapjon a kerelmezo, ezert itt nem csinalunk
            //semmit. Nem szabad jelzest adni a kerelmezo fele, mivel az
            //egy esetleges masik kerelmezo miatt ujra novelne az osszes
            //kerelmezo szamlalojat. (DepCnt-ket)
            //
            //Az userek fele viszont lehet jelezni, hogy le van allitva az eroforras.
            //Ez akkor lehet, ha egy leallito parancsot kuldenek egy leallitott
            //eroforrasra.
            MyRDM_signallingUsers(resource, RESOURCE_STOP, 0);
            break;
        case RESOURCE_STATE_STOPPING:
            //Az eroforras egy korabban kiadott leallitasi folyamatban van.
            //Annak veget meg kell varni.
            break;


        default:
            //ismeretlen eroforras allapot. elvileg nem lehetne ilyen.
            status=kStatus_Fail;
            break;
    }

    xSemaphoreGiveRecursive(resource->mutex);
    return status;

stopResource:
    //<--ide ugrik, ha az eroforrashoz tartozo leallito funkciot meg kell hivni

    //A leallito parancs alatt mar nem mutexelunk.
    //Ez biztositja, hogy az eroforras Stop funkcioja is hivhassa a
    //status callbacket, ha a leallitas azonnak elvegezheto.

    //A leallito callbacket meghivjuk
    status=resource->funcs->stop(resource->funcsParam);

    //Ezek utan majd az eroforrashoz tartozo callbackben kapunk
    //ertesitest a leallitasi parancs kimenetelerol.
    xSemaphoreGiveRecursive(resource->mutex);
    return status;
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
    //Az eroforras valtozoit csak mutexelten lehet modositgatni
    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);

    //if (errorCode)
    //{   //Ha jonne at hibakod, akkor mindenkepen hiba statuszt csinalunk
    //    //belole. Minden mas allapotot felulirunk vele!
    //    resourceStatus=RESOURCE_ERROR;
    //}

    //Elagazas a frissen kapott statusz(allapot) alapjan.
    switch((int)resourceStatus)
    {
        //......................................................................
        case RESOURCE_RUN:
            //Az eroforras elindult.

            if (resource->usageCnt==0)
            {   //De nincs mar hasznalo hozza. Ilyen lehet, ha kozben az indu-
                //lasi folyamat alatt lemondta megis az osszes igenylo.
                //Le kell allitani az eroforrast...

                //Az eroforras ettol kezdve leallitasi folyamatot jelzo allapotot
                //fog mutatni, mindaddig, amig o maga le nem all, ezt mutatja.
                resource->state=RESOURCE_STATE_STOPPING;


                //Az eroforras igenyloi fele  jelezni kell, hogy az eroforras nem
                //hasznalhato.
                //Igy azoknal a fuggosegi szamlalot novelni kell!
                resourceDep_t* requester=resource->firstRequester;
                while(requester)
                {
                    //Jelzes a kerelmezonek...
                    //A kerelmezo fuggosegi szamlaloja novekedni fog.
                    MyRDM_dependencyStop((resource_t*)requester->requesterResource);

                    //Kovetkezo elemre lepes a lancolt listaban
                    requester=(resourceDep_t*)requester->nextRequester;
                }


                if ((resource->funcs->stop)&&(resource->started==true))
                {
                    //A stopResource cimkere ugrunk. (Jobb stack felhasznalas)
                    //A cimke unlockolja a mutexet, majd meghivja az eroforras
                    //leallito funkciojat. Utana a folytatas mar a callbackben
                    //tortenhet.
                    //Csak akkor ugorhatunk a leallitasi funkciora, ha elotte a
                    //eroforras el lett inditva!
                    goto stopResource;
                } else
                {   //Ha nincs callback, akkor ugy vesszuk, hogy le van allitva

                    //Az eroforras fuggosegeiben lemondjuk a hasznalatot. Igy a mar
                    //nem szukseges eroforrasok (melyekben az UsageCnt 0-ra
                    //csokkent, szinten leallithato...
                    //(mutexelve kell hivni)
                    errorCode=MyRDM_resourceStopped(resource);

                    //Az eroforras ettol kezdve leallitott allapotot mutat.
                    resource->state=RESOURCE_STATE_STOP;
                    resource->started=false;

                    //Jelzes az applikacio fele, hogy az eroforras leallt
                    MyRDM_signallingUsers(resource, RESOURCE_STOP, 0);

                    //Csokekntjuk a futo eroforrasok szamat
                    MyRDM_decrementRunningResourcesCnt();
                }
            } else
            {
                //Az eroforras elindult. Hasznalhato. Jelezzuk az uj allapotot.
                resource->state=RESOURCE_STATE_RUN;

                //Az eroforras elindult jelzes kiadasa a ra varakozokra, akik
                //tole fuggenek. (Mutexelve kell hivni!
                errorCode=MyRDM_resourceStarted(resource);

                //Ha itt keletkezne hibakod, akkor az majd a vegen eljut a
                //hasznalokhoz!

                //Jelzes az applikacio fele, hogy az eroforras elindult
                MyRDM_signallingUsers(resource, RESOURCE_RUN, 0);
            }
            break;
        //......................................................................
        case RESOURCE_DONE:
            //Az eroforras vegzett a feladataval.
            if (resource->usageCnt)
            {   //Az eroforras meg hasznalatban van. Csokkentjuk a hasznalati szamla-
                //lot. Ha az 0-ra csokken, akkor vegez a mukodessel.
                resource->usageCnt--;
            }
        case RESOURCE_STOP:
            //Az eroforras leallt.

            if (resource->usageCnt)
            {   //Most viszont azt latni, hogy a leallitasi folyamat kozben
                //ujabb hasznalati igeny erkezett. (Mivel a hasznalati szamlalo
                //nem 0.)
                //Ujrainditjuk az eroforrast...

                //Az eroforras ettol kezdve inditasi folyamatot jelzo allapotot
                //fog mutatni, mindaddig, amig minden fuggosege, es o maga is el
                //nem indul.
                resource->state=RESOURCE_STATE_STARTING;

                if (resource->depCnt==0)
                {   //Az eroforrasnak nincs fuggosege, amire varni kellene.
                    //Azonnal indulhatunk...
                    if (resource->funcs->start)
                    {
                        //A startResource cimkere ugrunk.(Jobb stack felhasznalas)
                        //A cimke unlockolja a mutexet, majd meghivja az
                        //eroforras leallito funkciojat. Utana a folytatas mar a
                        //callbackben tortenhet.
                        goto startResource;
                    } else
                    {   //Ha nincs Start funkcio, akkor ugy vesszuk, hogy el van
                        //inditva
                        resource->state=RESOURCE_STATE_RUN;

                        //Jelzes, hogy az eroforras el lett inditva
                        resource->started=true;

                        //Az eroforras elindult jelzes kiadasa a ra varakozokra,
                        //akik tole fuggenek. (mutexelve kell hivni)
                        errorCode=MyRDM_resourceStarted(resource);

                        //Ha itt keletkezne hibakod, akkor az majd a vegen eljut a
                        //hasznalokhoz!

                        //Jelzes az applikacio fele, hogy az eroforras elindult
                        MyRDM_signallingUsers(resource, RESOURCE_RUN, 0);
                    }

                } else
                {   //Az eroforrasnak van vagy vannak fuggosegei. Azokat indijuk.
                    //Az eroforrashoz tartozo fuggosegi szamlalo majd a
                    //fuggosegek elindulasara csokkanni fognak 0-ra, melynek
                    //bekovetkezesekor az eroforras maga is indithato lesz.
                    //Ez viszont  majd a kesobbi callbackben tortenik meg.

                    errorCode=MyRDM_startResourceDeps(resource);

                    //Ha itt keletkezne hibakod, akkor az majd a vegen eljut a
                    //hasznalokhoz!
                }
            } else
            {   //Az eroforrasban jelezhetjuk, hogy leallt, mivel senki sem
                //hasznalja.

                //Az eroforras fuggosegeiben lemondjuk a hasznalatot. Igy a mar
                //nem szukseges eroforrasok (melyekben az UsageCnt 0-ra
                //csokkent, szinten leallithato...
                //(mutexelve kell hivni)
                errorCode=MyRDM_resourceStopped(resource);

                //Az eroforras ettol kezdve azt mondja, hogy teljesen le van
                //allitva
                resource->state=RESOURCE_STATE_STOP;
                resource->started=false;

                //Jelzes az applikacio fele, hogy az eroforras leallt
                MyRDM_signallingUsers(resource, RESOURCE_STOP, 0);

                //Csokekntjuk a futo eroforrasok szamat
                MyRDM_decrementRunningResourcesCnt();
            }
            break;
        //......................................................................
        //case RESOURCE_DONE:
        //    //Az eroforras befejezte a feladatat, ezert leallt.
        //    //(ujra kepes inditas fogadasara.)
        //    //TODO: KIDOLGOZNI!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //    break;
        //......................................................................
        case RESOURCE_ERROR:
        default:
            //Az eroforras inditasa/leallitasa/mukodese hibara futott.
            //de ide jut, ha valami invalid statuszt kapott.

            //Ha nem lenne megadva hibakod, akkor egyszeru fail lesz az.
            if (errorCode==kStatus_Success) errorCode=kStatus_Fail;
            break;

    } //switch(resourceStatus)


exit:   //<--kilepeskor ugrik ide
    if (errorCode)
    {   //Volt valami hiba a folyamatokban. Ezt jelezzuk a hasznalok iranyaba...

        //Az eroforras utolos kapott hibakodjat megjegyzezzuk
        resource->lastErrorCode=errorCode;

        //Ha az eroforras inditasi vagy megallitasi folyamatban volt, akkor az
        //hiba allapotba viszi az eroforrast.
        //Ha normal mukodes (RUN) kozben kap hibat, akkor azt csak
        //tovabbitjuk az eroforrast hasznalok fele, de az eroforrast nem
        //allitjuk le.

        switch(resource->state)
        {
            case RESOURCE_STATE_STARTING:
            case RESOURCE_STATE_STOPPING:
                //Az eroforrast hibas allapotba visszuk. Ebbol kimozditani csak
                //ugy lehet, ha elobb leallitasi kerelmet kap, majd ha az
                //hiba nelkul lement, akkor all vissza STOP-ba.
                resource->state=RESOURCE_STATE_ERROR;
                break;

            default:
            //case RESOURCE_STATE_RUN:
                //Csak jelzes a hasznaloknak, hogy hibat jelez az eroforras
                 break;
        }

        //Jelzes a hasznalo eroforrasok fele...
        //(Azokat mind vegigfertozi a hibaval)
        MyRDM_sendErrorSignalToReqesters(resource,
                                             RESOURCE_ERROR,
                                             errorCode);
        //Jelzes az applikacio fele...
        MyRDM_signallingUsers(resource,
                                  RESOURCE_ERROR,
                                  errorCode);
    }

    xSemaphoreGiveRecursive(resource->mutex);
    return;


stopResource:
    //<--ide ugrik, ha az eroforrashoz tartozo leallito funkciot meg kell hivni

    //A leallito parancs alatt mar nem mutexelunk.
    //Ez biztositja, hogy az eroforras Stop funkcioja is hivhassa a
    //status callbacket, ha a leallitas azonnak elvegezheto.

    //A leallito callbacket meghivjuk
    errorCode=resource->funcs->stop(resource->funcsParam);

    //Ezek utan majd az eroforrashoz tartozo callbackben kapunk
    //ertesitest a leallitasi parancs kimenetelerol.
    goto exit;

startResource:
    //<--ide ugrik, ha az eroforrashoz tartozo indito funkciot meg kell hivni

    //Jelzes, hogy az eroforras fele el lett kuldve az inditasi kerelem.
    resource->started=true;
    //Az indito parancs alatt mar nem mutexelunk.
    //Ez biztositja, hogy az eroforras Start funkcioja is hivhassa a
    //status callbacket, ha az inditas azonnak elvegezheto.

    //Az indito callbacket meghivjuk.
    errorCode=resource->funcs->start(resource->funcsParam);

    //Ezek utan majd az eroforrashoz tartozo callbackben kapunk
    //ertesitest a leallitasi parancs kimenetelerol.
    goto exit;
}
//------------------------------------------------------------------------------
//Eroforrashoz igenylo hozzaadasa.
static inline void MyRDM_addRequesterToResource(resource_t* resource,
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
static inline void MyRDM_addDependencyToResource(resource_t* resource,
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
void MyRDM_addDependency(resourceDep_t* dep,
                         resource_t* highLevel,
                         resource_t* lowLevel)
{
    //Beallitjuk a leiroban a kerelmezo es a fuggoseget
    dep->requesterResource=(struct resource_t*) highLevel;
    dep->requiredResource =(struct resource_t*) lowLevel;
    MyRDM_addDependencyToResource(highLevel, dep);
    MyRDM_addRequesterToResource (lowLevel,  dep);
}
//------------------------------------------------------------------------------
//Eroforras hibas allapotbol torteno kimozditasa.
//Az eroforrasokon a hiba ugy torolheto, ha azokat leallitjuk
status_t MyRDM_clearResourceError(resource_t* resource)
{
    status_t status=kStatus_Success;

    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);
    if (resource->state!=RESOURCE_STATE_ERROR)
    {   //Az eroforras nics is hiba allapotban
        //nem csinalunk semmit
    } else
    {   //Hiba allapotra meg kell hivni a leallitasi kerelmet...

    }
    xSemaphoreGiveRecursive(resource->mutex);
    return status;
}
//------------------------------------------------------------------------------
//Az eroforrashoz az applikacio felol hivhato USER hozzaadasa.
//A hasznalok lancolt listajahoz adja az User-t.
void MyRDM_addUser(resource_t* resource,
                   resourceUser_t* user,
                   const char* userName)
{
    //A lista csak mutexelt allapotban bovitheto!
    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);

    //Az user-hez ballitja az igenyelt eroforrast, akit kerelmez
    user->resource=resource;

    if (resource->firstUser==NULL)
    {   //Meg nincs beregisztralva hasznalo. Ez lesz az elso.
        resource->firstUser=(struct resourceUser_t*) user;
        user->prev=NULL;
    } else
    {   //Mar van a listanak eleme. Az utolso utan fuzzuk.
        ((resourceUser_t*)resource->lastUser)->next=(struct resourceUser_t*)user;
    }
    //A sort lezarjuk. Ez lesz az utolso.
    user->next=NULL;
    resource->lastUser=(struct resourceUser_t*)user;


    //Az eroforras a hozzaadas utan keszenleti allapotot mutat. Amig nincs konkret
    //inditasi kerelem az eroforrasram addig ezt mutatja.
    //(Ebbe ter vissza, ha az eroforrast mar nem hasznaljuk, es az eroforras le is allt.)
    user->state=RESOURCEUSERSTATE_IDLE;

    //Eroforras nevenek megjegyzese
    user->userName=(userName!=NULL) ? userName : "?";
    xSemaphoreGiveRecursive(resource->mutex);
}
//------------------------------------------------------------------------------
//Egy eroforrashoz korabban hozzaadott USER kiregisztralasa.
//A hasznalok lancolt listajabol kivesszuk az elemet
void MyRDM_removeFromUsers(resource_t* resource, resourceUser_t* user)
{
    //A lista csak mutexelt allapotban modosithato!
    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);

    resourceUser_t* prev=(resourceUser_t*)user->prev;
    resourceUser_t* next=(resourceUser_t*)user->next;


    if ((prev) && (next))
    {   //lista kozbeni elem. All elotet es utana is elem a listaban
        prev->next=(struct resourceUser_t*)next;
        next->prev=(struct resourceUser_t*)prev;
    } else
    if (next)
    {   //Ez a lista elso eleme, es van meg utana elem.
        //A kovetkezo elem lesz az elso.
        resource->firstUser=(struct resourceUser_t*) next;
        next->prev=NULL;
    } else if (prev)
    {   //Ez a lista utolso eleme, es van meg elote elem.
        prev->next=NULL;
        resource->lastUser=(struct resourceUser_t*)prev;
    } else
    {   //Ez volt a lista egyetlen eleme
        resource->lastUser=NULL;
        resource->firstUser=NULL;
    }

    user->next=NULL;
    user->prev=NULL;

    xSemaphoreGiveRecursive(resource->mutex);
}
//------------------------------------------------------------------------------
//Eroforras igenylese.
//Hatasara a kert eroforras ha meg nins elinditva, elindul, majd az user-hez
//beregisztraltkeresztul jelzi, annak sikeresseget, vagy hibajat.
//Az atadott user struktura bekerul az eroforrast hasznalok lancolt listajaba.
status_t MyRDM_useResource(resourceUser_t* user)
{
    status_t status;
    resource_t* resource=user->resource;

    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);
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
            status=MyRDM_useResourceCore(resource);
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

    xSemaphoreGiveRecursive(resource->mutex);

    return status;
}
//------------------------------------------------------------------------------
//Eroforrasrol lemondas.
//Ha az eroforras mar senki sem hasznalja, akkor az le lessz allitva.
//Az user-hez beregisztralt callbacken keresztul majd vissza fog jelezni, ha az
//eroforras mukodese befejezodott.
status_t MyRDM_unuseResource(resourceUser_t* user)
{
    status_t status=kStatus_Success;
    resource_t* resource=user->resource;

    xSemaphoreTakeRecursive(resource->mutex, portMAX_DELAY);
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
            status=MyRDM_unuseResourceCore(resource,
                                           &user->resourceContinuesWork);

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
                status=MyRDM_unuseResourceCore(resource,
                                               &user->resourceContinuesWork);
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

    xSemaphoreGiveRecursive(resource->mutex);

    return status;
}
//------------------------------------------------------------------------------
//Userek fele jelzes kuldese a hozzajuk rendelt eroforrasok allapotarol
static void MyRDM_signallingUsers(resource_t* resource,
                                  resourceStatus_t resourceStatus,
                                  status_t errorCode)
{
    resourceUser_t* user=(resourceUser_t*)resource->firstUser;
    while(user)
    {
        if (user->state==RESOURCEUSERSTATE_IDLE)
        {   //Amig nincs hasznalatban az user reszerol az eroforras, addig
            //nem kaphat jelzest.
            //Ilyen az, amig nincs elinditva az eroforras, vagy ebbe az
            //allapotba kerul, ha a hasznalt eroforras leallt.
        } else
        {
            if (resourceStatus==RESOURCE_ERROR)
            {   //Hiba van az eroforrasal.
                //A hibajelzest at kell kuldeni mindenkepen.
                if (user->statusFunc)
                {
                        user->statusFunc(   resourceStatus,
                                            errorCode,
                                            user->callbackData);
                }
            }
            else
            {   //Elagazas az user aktualis allapta szerint...
                switch(user->state)
                {
                    case  RESOURCEUSERSTATE_WAITING_FOR_START:
                        //Az user varakozik az inditasi jelzesre
                        if (resourceStatus==RESOURCE_RUN)
                        {   //...es az eroforras most elindult jelzest kapott.
                            user->state=RESOURCEUSERSTATE_RUN;
                            //Jelezes az usernek...
                            //Jelzes az usernek.
                            if (user->statusFunc)
                            {
                                    user->statusFunc(resourceStatus,
                                                   errorCode,
                                                   user->callbackData);
                            }
                        }
                        break;

                    case  RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE:
                        //Az user varakozik az eroforras leallt vagy vegzett
                        //jelzesres...
                        if ((resourceStatus==RESOURCE_STOP) ||
                           (resourceStatus==RESOURCE_DONE))
                        {   //...es az eroforras most leallt vagy vegzett.
                            user->state=RESOURCEUSERSTATE_IDLE;
                            //Jelezes az usernek...
                            if (user->statusFunc)
                            {
                                    user->statusFunc(resourceStatus,
                                                   errorCode,
                                                   user->callbackData);
                            }
                        }
                        break;

                    case  RESOURCEUSERSTATE_RUN:
                        //Az user fut
                        if ((resourceStatus==RESOURCE_STOP) ||
                           (resourceStatus==RESOURCE_DONE))
                        {   //...es az eroforras most leallt vagy vegzett.
                            user->state=RESOURCEUSERSTATE_IDLE;
                            //Jelezes az usernek...
                            if (user->statusFunc)
                            {
                                    user->statusFunc(resourceStatus,
                                                   errorCode,
                                                   user->callbackData);
                            }
                        }
                        break;

                    default:
                        break;
                } //switch(user->state)
            }
        }

        //Kovetkezo elemre lepes a lancolt listaban
        user=(resourceUser_t*)user->next;
    } //while
}
//------------------------------------------------------------------------------
//Egy eroforras minden kerelmezoje es hasznaloja fele elkuldi a hibajelzest...
static inline void MyRDM_sendErrorSignalToReqesters(resource_t* resource,
                                                    resourceStatus_t resourceStatus,
                                                    status_t errorCode)
{
    resourceDep_t* requester=resource->firstRequester;
    while(requester)
    {
        //Jelzes a kerelmezonek...
        MyRDM_resourceStatus((resource_t*)requester->requesterResource,
                             resourceStatus,
                             errorCode);

        requester=(resourceDep_t*) requester->nextRequester;
    }
}
//------------------------------------------------------------------------------
//Altalanos callback rutin, melyet az altalanos userre varas eseten hasznalunk
static void MyRDM_generalUserStatusCallback(resourceStatus_t resourceStatus,
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
void MyRDM_addGeneralUser(resource_t* resource,
                          generalResourceUser_t* genUser,
                          const char* userName)
{
    //Esemenyflag mezo letrehozasa
  #if configSUPPORT_STATIC_ALLOCATION
    genUser->events=xEventGroupCreateStatic(&genUser->eventsBuff);
  #else
    genUser->events=xEventGroupCreate();
  #endif

    //Az altalanos usrekehez tartozo kozos allapot callback lesz beallitva
    genUser->user.statusFunc=MyRDM_generalUserStatusCallback;
    genUser->user.callbackData=genUser;

    //A kiejlolt eroforrashoz user hozzaadasa
    MyRDM_addUser(resource, &genUser->user, userName);
}
//------------------------------------------------------------------------------
//Torli az usert az eroforras hasznaloi kozul.
//Fontos! Elotte az eroforras hasznalatot le kell mondani!
void MyRDM_removeGeneralUser(generalResourceUser_t* generalUser)
{
    MyRDM_removeFromUsers(generalUser->user.resource, &generalUser->user);
}
//------------------------------------------------------------------------------
//Eroforras hasznalatba vetele. A rutin megvarja, amig az eroforras elindul,
//vagy hibara nem fut az inditasi folyamatban valami miatt.
status_t MyRDM_generalUseResource(generalResourceUser_t* generalUser)
{
    status_t status=kStatus_Success;

    //Eroforras hasznalatba vetele
    status=MyRDM_useResource(&generalUser->user);
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
        /*status=*/ MyRDM_unuseResource(&generalUser->user);
        //if (status) return status;

    }

    return status;
}
//------------------------------------------------------------------------------
//Eroforras hasznalatanak lemondasa. A rutin megvarja, amig az eroforras leall,
//vagy hibara nem fut a leallitasi folyamatban valami.
status_t MyRDM_generalUnuseResource(generalResourceUser_t* generalUser)
{
    status_t status=kStatus_Success;

    //Eroforras hasznalatanak lemondasa
    status=MyRDM_unuseResource(&generalUser->user);
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
        /*status=*/ MyRDM_unuseResource(&generalUser->user);
        //if (status) return status;
    }

    return status;
}
//------------------------------------------------------------------------------
//Eroforras leirojanak lekerdezese az eroforras neve alapjan.
//NULL-t ad vissza, ha az eroforras nem talalhato.
resource_t* MyRDM_getResourceByName(const char* name)
{
    resource_t* resource=myrdm.firstResource;
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
        resource=(resource_t*) resource->next;
    }

    //Az eroforras nem talalhato
    return NULL;
}
//------------------------------------------------------------------------------
//A futo/elinditott eroforrasok szamat noveli
static void MyRDM_incrementRunningResourcesCnt(void)
{
    MyRDM_t* managger=&myrdm;
    xSemaphoreTake(managger->mutex, portMAX_DELAY);
    managger->runningResourceCount++;
    xSemaphoreGive(managger->mutex);
}
//------------------------------------------------------------------------------
//A futo/elinditott eroforrasok szamat csokkenti, ha egy eroforras leallt.
static void MyRDM_decrementRunningResourcesCnt(void)
{    
    MyRDM_t* man=&myrdm;
    xSemaphoreTake(man->mutex, portMAX_DELAY);

    //Nem is fut mar elvileg eroforras, megis csokekntika  futok szamat. ez hiba.
    //valahol vagy tobbszor csokkentettuk a szamlalot, vagy nem lett novelve.
    ASSERT(man->runningResourceCount);

    man->runningResourceCount--;

    if (man->runningResourceCount==0)
    {   //Az osszes eroforras leallt.
        xSemaphoreGive(man->mutex);
        //jelzes a beregisztralt callbacken keresztul, hogy minden eroforras
        //mukodese leallt.
        //Evvel megoldhato, hogy a vezerles bevarja amig minden
        //folyamat leall.
        if (man->allResourceStoppedFunc)
        {
            man->allResourceStoppedFunc(man->callbackData);
        }

    } else
    {
        xSemaphoreGive(man->mutex);
    }
}
//------------------------------------------------------------------------------
//A futo eroforrasok szamanak lekerdezese
uint32_t MyRDM_getRunningResourcesCount(void)
{
    uint32_t res;
    portENTER_CRITICAL();
    res=myrdm.runningResourceCount;
    portEXIT_CRITICAL();
    return res;
}
//------------------------------------------------------------------------------
//Minden eroforras sikeres leallitasakor hivodo callback funkcio beregisztralasa
void MyRDM_register_allResourceStoppedFunc(MyRDM_allResourceStoppedFunc_t* func,
                                           void* callbackData)
{
    MyRDM_t* man=&myrdm;
    xSemaphoreTake(man->mutex, portMAX_DELAY);
    man->allResourceStoppedFunc=func;
    man->callbackData=callbackData;
    xSemaphoreGive(man->mutex);
}
//------------------------------------------------------------------------------

