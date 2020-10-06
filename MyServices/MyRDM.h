//------------------------------------------------------------------------------
//  Resource Dependency Manager
//
//    File: MyRDM.h
//------------------------------------------------------------------------------
#ifndef MyRDM_H_
#define MyRDM_H_

#include "MyCommon.h"

//Ha hasznaljuk a modult, akkor annak valtozoira peldanyt kell kesziteni.
//Az alabbi makrot el kell helyezni valahol a forrasban, peldaul main.c-ben
#define MyRDM_INSTANCE  MyRDM_t myrdm;
//------------------------------------------------------------------------------
//Eroforrasok statusz definicioi.
//Az egyes eroforrasok kezelesenek implementalasakor ezekeken keresztul jelzik
//az eroforras aktualis allapotvaltozasat
typedef enum
{
    //Az eroforras le lett allitva/befejezte a mukodest. A tovabbiakban nem
    //hasznalhato. (initkor is ezt az allapotot vezsi fel.)
    RESOURCE_STOP=0,
    //Eroforras elindult, mukodik, a resdszer szamara hasznalhato
    RESOURCE_RUN,
    //Az eroforras mukodese/initje hibara futott
    RESOURCE_ERROR,
    //Az eroforras befejezte a mukodeset
    RESOURCE_DONE,
} resourceStatus_t;

//Eroforrast inicializalo callback felepitese
//resourceHandler: ezt az eroforras valtozoihoz le kell tarolni, es ezt kell
//visszaadni a status callbackben az RDM-nek.
typedef status_t resourceInitFunc_t(void* param,
                             void* resourceHandler);

//eroforrast elindito callback
typedef status_t resourceStartFunc_t(void* param);

//Eroforrast leallito callback
typedef status_t resourceStopFunc_t(void* param);
//------------------------------------------------------------------------------
//Eroforrasokhoz tartozo callbackek
typedef struct
{
    //eroforrast inicializalo callback
    resourceInitFunc_t*        init;
    //Eroforrast elindito callback
    resourceStartFunc_t*       start;
    //Eroforrast leallito callback
    resourceStopFunc_t*        stop;
} resourceFuncs_t;
//------------------------------------------------------------------------------
//eroforras allapotat leiro enumok.
typedef enum
{
    //Ismeretlen allapot. Reset utan, amig nem kap inicializalast
    RESOURCE_STATE_UNKNOWN=0,
    //Eroforras leallitott allapota. Inicializaltsag utani allapot is ez.
    RESOURCE_STATE_STOP,
    //Eroforras inditasi folyamata van
    RESOURCE_STATE_STARTING,
    //Eroforras elindult, az applikacio hasznalhatja.
    RESOURCE_STATE_RUN,
    //Eroforras leallitasa folyamatban van. Varjuk annak veget.
    RESOURCE_STATE_STOPPING,
    //Eroforras elinditasa vagy megallitasa hibara futott. Ez kulonbozik a RUN
    //illetve STOPPING allapotban keletkezo hibaktol, mert azok nem viszik ebbe
    //a hiba allapotba, azok a hibak csak jelzesre kerulnek az eroforrast
    //hasznalok fele.
    RESOURCE_STATE_ERROR
} resourceState_t;

//eroforrasok allapotahoz tartozo stringek.
//FONTOS, HOGY A SORRENDJUK SZINKRONBAN LEGYEN AZ ENUMOKKAL!
#define RESOURCE_STATE_STRINGS  \
{   "UNKNOWN ",                 \
    "STOP    ",                 \
    "STARTING",                 \
    "RUN     ",                 \
    "STOPPING",                 \
    "ERROR   "}
//------------------------------------------------------------------------------
//Egy eroforras mas eroforrasoktol valo fuggoseget leiro struktura.
//Ezekbol egy lancolt lista alakulhat ki, eroforrasonkent.
//Minden eroforrashoz annyit kell letrehozni, amennyi fuggosege van.
//A struktura 2 lancolt listaba is elemkent szerepel.
// - A tulajdonos listaja
// - Az alapfeltetelnek megadott (igenyelt) eroforrashoz tartozo lancoltlistaban
typedef struct
{
    //A leiro altal igenyelt eroforrasra mutat.
    struct resource_t*     requiredResource;

    //Az eroforrashoz tartozo fuggosegi lancolt lista kezelesehez szukseges.
    //Ezen halad vegig az igenyelt eroforras, es jelez vissza a kerelmezonek az
    //aktualis statuszarol.
    //Ha mindket pointer NULL, akkor ez a fuggoseg nem jatszik.
    struct resourceDep_t*  nextRequester;

    //A leirot birtoklo eroforrasra mutat.
    //Egy fuggosegben levo eroforras ez alapjan tudja, hogy melyik ignylo
    //eroforrasnak kell majd jelezni a statuszaval.
    struct resource_t*     requesterResource;
    //A kerelmezo eroforras fuggosegi listaja
    struct resourceDep_t*  nextDependency;

} resourceDep_t;
//------------------------------------------------------------------------------
//Az egyes eroforrasok manageleshez tartozo valtozok halmaza.
//Minden, a rendszerben implementalt, es managelt eroforras rendelkezik egy
//ilyen leiroval.
typedef struct
{
    //Eroforras aktualis allapota.
    resourceState_t    state;

    //Eroforrashoz tartozo callbackek halmaza
    //Ezeken keresztul initeli, inditja, allitja le a hozza tartozo eroforrasokat
    const resourceFuncs_t*   funcs;
    //A callbackekhez atadott tetszoleges adattartalom
    void*           funcsParam;

    //A manager altal hasznalt valtozoit vedo mutex
    SemaphoreHandle_t   mutex;
  #if configSUPPORT_STATIC_ALLOCATION
    StaticSemaphore_t   mutexBuffer;
  #endif

    //Az eroforras neve. (Ezt a debuggolashoz tudjuk hasznalni.)
    const char*     resourceName;

    //Az eroforrast hasznalok szama, mely lehet egy masik eroforras is, vagy
    //lehet maga az applikacio is.
    //Minden ujabb hasznalatbaveteli kerelem noveli ezt a szamlalot, majd
    //minden lemondas csokkenti.
    //Ha a szamlalo 0-ra csokken, akkor az eroforras hasznalata leallithato.
    uint32_t        usageCnt;

    //Az eroforrast igenylo eroforrasok lancolt listaja.
    resourceDep_t*    firstRequester;
    resourceDep_t*    lastRequester;

    //Az eroforrashoz tartozo sajat fuggosegek lancolt listajanak eleje es vege.
    //Ebben sorakoznak az eroforras mukodesehez szukseges tovabbi fuggosegek.
    resourceDep_t*    firstDependency;
    resourceDep_t*    lastDependency;

    //Az eroforrashoz tartozo fuggosegek szama.
    uint32_t        depCount;
    //A meg inditasra varo fuggosegek szamat nyilvantaro szamlalo.
    //Ha erteke 0-ra csokken, akkor az eroforras mukodesehez tratozo egyeb
    //fuggosegek elindultak, es az eroforras is indithato.
    uint32_t        depCnt;

    //Az eroforrast hasznalok lancolt listajanak elso es utolso eleme
    struct resourceUser_t*   firstUser;
    struct resourceUser_t*   lastUser;

    //Az eroforras managerben az eroforrasok lancolt listajahoz szukseges
    //mutato.
    struct resource_t* next;

    //Az eroforras utolso hibakodja, melyet kapott egy statusz callbackben
    status_t lastErrorCode;

    //True, ha az eroforras el lett inditva. Annak meghivasra kerult az indito
    //fuggvenye.
    bool    started;

} resource_t;
//------------------------------------------------------------------------------
//Minden eroforras sikeres leallitasa utan hihato callback fuggveny definialasa
typedef void MyRDM_allResourceStoppedFunc_t(void* callbackData);
//------------------------------------------------------------------------------
//MyRDM valtozoi
typedef struct
{
    //A manager valtozoit vedo mutex
    SemaphoreHandle_t   mutex;
  #if configSUPPORT_STATIC_ALLOCATION
    StaticSemaphore_t   mutexBuffer;
  #endif

    //A futo eroforrasokat szamolja
    uint32_t    runningResourceCount;

    //A nyilvantartott eroforrasok lancolt listajanak az elso es utolso eleme.
    //Az MyRDM_createResource() management rutinnal kerulnek hozzadasra.
    //Ez alapjan az eroforrasokon vegig lehet haladni, es azokon csoportos
    //muveleteket vegrehajtani.
    resource_t*    firstResource;
    resource_t*    lastResource;

    //Minden eroforras sikeres leallitasa utan hihato callback fuggveny definialasa
    MyRDM_allResourceStoppedFunc_t* allResourceStoppedFunc;
    void* callbackData;

} MyRDM_t;
extern MyRDM_t myrdm;
//------------------------------------------------------------------------------
//Az applikacio fele callback definicio.
//Ezen keresztul jelez vissza, ha az hasznalt eroforrasban valami allapot
//valtozas all be. (pl elindul, megallt, hiba van vele, ...)
typedef void resourceStatusFunc_t( resourceStatus_t resourceStatus,
                                    status_t errorCode,
                                    void* callbackData);

//Eroforrasokat hasznalo userek (igenylok) allapotai.
typedef enum
{
    //A leirohoz tartozo felhasznalo meg nem inditotta el az eroforrast.
    //Amig ebben az allapotban van, addig nem kap jelzeseket a callbacken
    //keresztul. Ebbe az allapotba jut akkor is, amikor egy lemondas utan
    //az eroforras leallt. (Gyakorlatilag ez a STOP allapot is.)
    RESOURCEUSERSTATE_IDLE=0,
    //Az user varja,hogy a hozza tartozo eroforras elinduljon
    RESOURCEUSERSTATE_WAITING_FOR_START,
    //Az eroforras el van inidtva
    RESOURCEUSERSTATE_RUN,
    //Az eroforras varja, hogy az eroforras lealljon, vagy a feladataval
    //elkeszuljon.
    RESOURCEUSERSTATE_WAITING_FOR_STOP_OR_DONE,
    //A z eroforras hibara futott
    RESOURCEUSERSTATE_ERROR,
} resourceUserState_t;

//Az eroforrast hasznalo (birtoklo) egysegekhez tartozik egy-egy ilyen leiro.
//Ezen keresztul tortenik az egyes eroforrasok kerelme az applikacio felol.
typedef struct
{
    //A hasznalni kivant eroforrasra mutat
    resource_t* resource;

    //Az eroforrast hasznalo user allapota.
    resourceUserState_t    state;

    //true, ha a hozza tartozo eroforrasrol valo lemondaskor az eroforrasnak meg
    //tovabb kell mukodni, mivel mas userek vagy eroforrasok meg hasznaljak.
    bool resourceContinuesWork;

    //Az eroforras allapotvaltozasa eseten feljovo callback. Ez alapjan tudja
    //peldaul a kerelmezo, hogy a kert eroforras elindult, es hasznalhatja, vagy
    //hibara futott.
    //generalResourceUser_t eseten ezt nem allitgatjuk be kulon!
    resourceStatusFunc_t*     statusFunc;
    void*                   callbackData;

    //A felhasznalo neve. (Ezt a debuggolashoz tudjuk hasznalni.)
    const char*             userName;

    //Az eroforrast hasznalo userek lancolt listajahoz szukseges
    struct resourceUser_t*     next;
    struct resourceUser_t*     prev;

} resourceUser_t;
//------------------------------------------------------------------------------
//Altalanos eroforras hasznalat eseten beallithato hiba callback felepitese,
//melyben az applikacio fele jelezni tudja az eroforras hibait.
typedef void generalResourceUserErrorFunc_t(status_t errorCode,
                                            void* callbackData);
//------------------------------------------------------------------------------
//Az altalonsitott felhasznalo kezeleshez hasznalt leiro.
typedef struct
{
    //Eroforras hasznalat valtozoi. (Gyakorlatilag ez kerul felbovitesre.)
    resourceUser_t user;

    //Az user mukodesere vonatkozo esemenyflag mezo, melyekkel az applikacio
    //fele lehet jelezni az allapotokat. Ezekre varhatnak az indito/megallito
    //rutinok. (Statikus)
    //GENUSEREVENT_xxx bitekkel operalunk rajta
    EventGroupHandle_t  events;
    StaticEventGroup_t  eventsBuff;

    //Hiba eseten meghivhato callback funkcio cime. Ezen keresztul lehet jelezni
    //az applikacio fele az eroforras mukodese kozbeni hibakat.
    generalResourceUserErrorFunc_t* errorFunc;
    //A funkcio meghivasakor atadott tetszoleges adat
    void*  callbackData;

} generalResourceUser_t;
//------------------------------------------------------------------------------
//Az altalanos eroforras felhasznalok (userek) eseteben hasznalhato esemeny
//flagek, melyekkel az altalanos user statusz callbackbol jelzunk a varakozo
//taszk fele.
//Az eroforras elindult
#define GENUSEREVENT_RUN    BIT(0)
//Az eroforras leallt
#define GENUSEREVENT_STOP   BIT(1)
//Az eroforrassal hiba van.
#define GENUSEREVENT_ERROR  BIT(2)
//Az eroforras vegzett (egyszer lefuto folyamatok eseten)
#define GENUSEREVENT_DONE   BIT(3)
//------------------------------------------------------------------------------
//Eroforras management reset utani kezdeti inicializalasa
void MyRDM_init(void);

//Egyedi eroforras managellesehez szukseges kezdeti inicializalasok.
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra!
void MyRDM_createResource(  resource_t* resource,
                          const resourceFuncs_t* funcs,
                          void* funcsParam,
                          const char* resourceName);
//------------------------------------------------------------------------------
//Az resourceUser_t struktura beallitasat segito rutin.
//user: a hasznalati leirora mutat
//statusFunc: A hasznalt eroforras allapotat visszajelzo callback rutin
//callbackData: A callbackban visszaadott tetszlseges valtozo
static inline void MyRDM_setUser(resourceUser_t* user,
                                 resourceStatusFunc_t* statusFunc,
                                 void* callbackData)
{
    user->statusFunc=statusFunc;
    user->callbackData=callbackData;
}
//------------------------------------------------------------------------------
//Eroforrasok koze fuggoseg beallitasa (hozzaadasa).
//highLevel: Magasabb szinten levo eroforras, melynek a fuggoseget definialjuk
//lowLevel: Az alacsonyabb szinten levo eroforras, melyre a magasabban levo
//mukodesehez szukseg van (highLevel).
void MyRDM_addDependency(resourceDep_t* dep,
                         resource_t* highLevel,
                         resource_t* lowLevel);

//A konzolra irja a szamontartott eroforrasokat, azok allapotait, es azok
//hasznaloit. Debuggolai celra hasznalhato.
void MyRDM_printUsages(bool printDeps);
//------------------------------------------------------------------------------
//Olyan specialis makro letrehozasa, mely segit definialni az eroforrasok kozti
//fuggosegeket.
//A makro(k) letrehoz a forraskod sora szerint egy DEP_<sorszam> nevu valtozot.
//A letrehozott valtozo "static" elotagot kap, igy az nem a stacken jon letre,
//igy a definialodo fuggoseg leiro kesobb is a memoriaban marad.
//Ezt kepes helyettesiteni
//  static resourceDep_t valami;
//  MyRDM_addDependency(&valami, &hi, &lo);
#define RDMDEPCOMBINE1(X,Y) X##Y
#define RDMDEPCOMBINE(X,Y) RDMDEPCOMBINE1(X,Y)
#define CREATE_RESOURCE_DEPENDENCY(hi, lo)    {static resourceDep_t RDMDEPCOMBINE(DEP_,__LINE__);  MyRDM_addDependency(& RDMDEPCOMBINE(DEP_,__LINE__), hi, lo);}
//------------------------------------------------------------------------------
//Eroforras igenylese.
//Hatasara a kert eroforras ha meg nins elinditva, elindul, majd a
//megadott callbacken keresztul jelzi, annak sikeresseget, vagy hibajat.
//Az atadott user struktura bekerul az eroforrast hasznalok lancolt listajaba.
status_t MyRDM_useResource(resourceUser_t* user);
//Eroforrasrol lemondas.
//Ha az eroforras mar senki sem hasznalja, akkor az le lessz allitva.
//A kerelmnel megadott callbacken keresztul majd vissza fog jelezni, ha az
//eroforras mukodese befejezodott.
status_t MyRDM_unuseResource(resourceUser_t* user);


//Az egyes eroforrasok init() rutinjaban atadott callback felepitese,
//melyen keresztul az egyes eroforrasok jelzik a manager fele az allapot-
//valtozasaikat.
//status-ban kell megadni az uj allapotot (lasd enum lista)
//errorCode: hiba eseten csak a hibakod
//callbackData: A init()-ben atadott tetszoleges parameter.
typedef void resourceStatusFunc_t(  resourceStatus_t status,
                                    status_t errorCode,
                                    void* callbackData);


//Az egyes eroforrasok ezen keresztul jelzik a manager fele az allapotvaltozasa-
//ikat.
//Megj: Ezt a callbacket az eroforrasok a Start vagy Stop funkciojukbol is
//hivhatjak!
void MyRDM_resourceStatus(resourceStatus_t resourceStatus,
                          status_t errorCode,
                          void* callbackData);


//Az eroforrashoz az applikacio felol hivhato USER hozzaadasa.
//A hasznalok lancolt listajahoz adja az User-t.
void MyRDM_addUser(resource_t* resource, resourceUser_t* user);
//Egy eroforrashoz korabban hozzaadott USER kiregisztralasa.
//A hasznalok lancolt listajabol kivesszuk az elemet
void MyRDM_removeFromUsers(resource_t* resource, resourceUser_t* user);


//Az eroforrashoz altalanos user kezelo hozzaadasa. A rutin letrehozza a
//szukseges szinkronizacios objektumokat, majd megoldja az eroforrashoz valo
//regisztraciot.
void MyRDM_addGeneralUser(resource_t* resource, generalResourceUser_t* genUser);
//Torli az usert a eroforras hasznaloi kozul.
//Fontos! Elotte a eroforras hasznalatot le kell mondani!
void MyRDM_removeGeneralUser(generalResourceUser_t* generalUser);
//Eroforras hasznalatba vetele. A rutin megvarja, amig az eroforras elindul,
//vagy hibara nem fut az inditasi folyamatban valami miatt.
status_t MyRDM_generalUseResource(generalResourceUser_t* generalUser);
//Eroforras hasznalatanak lemondasa. A rutin megvarja, amig az eroforras leall,
//vagy hibara nem fut a leallitasi folyamatban valami.
status_t MyRDM_generalUnuseResource(generalResourceUser_t* generalUser);



//TODO: teszt kepen kiteve publikusnak. Torolni kell majd!
status_t MyRDM_useResourceCore(resource_t* resource);
status_t MyRDM_unuseResourceCore(resource_t* resource, bool* continueWork);

//Eroforras leirojanak lekerdezese az eroforras neve alapjan.
//NULL-t ad vissza, ha az eroforras nem talalhato.
resource_t* MyRDM_getResourceByName(const char* name);

//A futo/aktiv eroforrasok szamanak lekerdezese
uint32_t MyRDM_getRunningResourcesCount(void);

//Minden eroforras sikeres leallitasakor hivodo callback funkcio beregisztralasa
void MyRDM_Register_allResourceStoppedFunc(MyRDM_allResourceStoppedFunc_t* func,
                                           void* callbackData);
//------------------------------------------------------------------------------
#endif //MyRDM_H_
