//------------------------------------------------------------------------------
//  Resource manager
//
//    File: MyRM.h
//------------------------------------------------------------------------------
#ifndef MYRM_H_
#define MYRM_H_

#include "MyCommon.h"

//Ha hasznaljuk a modult, akkor annak valtozoira peldanyt kell kesziteni.
//Az alabbi makrot el kell helyezni valahol a forrasban, peldaul main.c-ben
#define MyRM_INSTANCE  MyRM_t myRM;
//------------------------------------------------------------------------------
//Minden eroforras sikeres leallitasa utan hihato callback fuggveny definialasa
typedef void MyRM_allResourceStoppedFunc_t(void* callbackData);
//------------------------------------------------------------------------------
//Eroforras management inicializalasakor atadott konfiguracios struktura
typedef struct
{
    //Eroforras management taszkhoz allokalando stack merete
    uint32_t stackSize;
    //Eroforras management taszk prioritasa
    uint32_t taskPriority;

} MyRM_config_t;
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
    //Az eroforras befejezte a mukodeset (kesobbi fejlesztesre var.)
    RESOURCE_DONE,
} resourceStatus_t;

//eroforrasok statuszahoz tartozo stringek. (Nyomkoveteshez)
//FONTOS, HOGY A SORRENDJUK SZINKRONBAN LEGYEN AZ ENUMOKKAL!
#define RESOURCE_STATUS_STRINGS \
{   "STOP",                     \
    "RUN",                      \
    "ERROR",                    \
    "DONE",                     \
}
//------------------------------------------------------------------------------
//eroforras allapotat leiro enumok.
typedef enum
{
    //Eroforras leallitott allapota. Inicializaltsag utani allapot is ez.
    RESOURCE_STATE_STOP=0,
    //Eroforras inditasi folyamata van
    RESOURCE_STATE_STARTING,
    //Eroforras elindult, az applikacio hasznalhatja.
    RESOURCE_STATE_RUN,
    //Eroforras leallitasa folyamatban van. Varjuk annak veget.
    //Ha az errorFlag true, akkor az eroforras hiba miatt all le.
    RESOURCE_STATE_STOPPING,
} resourceState_t;

//eroforrasok allapotahoz tartozo stringek.
//FONTOS, HOGY A SORRENDJUK SZINKRONBAN LEGYEN AZ ENUMOKKAL!
#define RESOURCE_STATE_STRINGS  \
{   "STOP    ",                 \
    "STARTING",                 \
    "RUN     ",                 \
    "STOPPING",                 \
}
//------------------------------------------------------------------------------
//Eroforras hibajaval kapcsolatos informacios struktura definicioja.
//EZt a strukturat publikaljak magukrol az egyes eroforrasok.
typedef struct
{
    //A hibas fuggosegre mutat
    struct resource_t* resource;
    //a hiba kodja
    status_t errorCode;
    //A hibat jelento eroforras ebben az allapotaban generalta a hibat
    resourceState_t resourceState;
} resourceErrorInfo_t;
//------------------------------------------------------------------------------
//Eroforrast inicializalo callback felepitese
typedef status_t resourceInitFunc_t(void* param);

//eroforrast elindito callback
typedef status_t resourceStartFunc_t(void* param);

//Eroforrast leallito callback
typedef status_t resourceStopFunc_t(void* param);


//Eroforras mukodese kozben keletkezo hiba hatasara hivott callback
//ignoreError true-ba allitasa eseten a hibat nem jelzi tovabb.
//typedef void resourceErrorFunc_t(resourceErrorInfo_t* info,
//                                 bool* ignoreError,
//                                 void* param);

//Az eroforars valamelyik fuggosegenek hibaja eseten hivodo callback.
//typedef void resourceDependencyErrorFunc_t(resourceErrorInfo_t* info,
//                                           bool* ignoreError,
//                                           void* param);
//------------------------------------------------------------------------------
//Eroforrasokhoz tartozo callbackek
typedef struct
{
    //eroforrast inicializalo callback
    resourceInitFunc_t*             init;
    //Eroforrast elindito callback
    resourceStartFunc_t*            start;
    //Eroforrast leallito callback
    resourceStopFunc_t*             stop;
} resourceFuncs_t;
//------------------------------------------------------------------------------
//Egy eroforras mas eroforrasoktol valo fuggoseget leiro struktura.
//Ezekbol egy lancolt lista alakulhat ki, eroforrasonkent.
//Minden eroforrashoz annyit kell letrehozni, amennyi fuggosege van.
//A struktura 2 lancolt listaba is elemkent szerepel.
// - A tulajdonos listaja
// - Az alapfeltetelnek megadott (igenyelt) eroforrashoz tartozo listaban
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

    //Az eroforras altal hasznalt hibara futott eroforrasok listajanak kovetkezo
    //elemere mutat.
    struct resourceDep_t* nextDepError;
    //true, ha mar a fuggosegi hiba listahoz van adva a leiro
    bool depErrorInTheList;

} resourceDep_t;
//------------------------------------------------------------------------------
//Az egyes eroforrasok manageleshez tartozo valtozok halmaza.
//Minden, a rendszerben implementalt, es managelt eroforras rendelkezik egy
//ilyen leiroval.
typedef struct
{
    //Az eroforras managerben az eroforrasok lancolt listajahoz szukseges
    //mutato. Segitsegevel lehet kilistazni a letrehozott eroforrasokat.
    struct resource_t* nextResource;

    //Eroforras aktualis allapota.
    resourceState_t    state;

    //Eroforrashoz tartozo callbackek halmaza
    //Ezeken keresztul initeli, inditja, allitja le a hozza tartozo eroforrasokat
    const resourceFuncs_t*   funcs;
    //A callbackekhez atadott tetszoleges adattartalom
    void*           funcsParam;

    //Az eroforras neve. (Ezt a debuggolashoz tudjuk hasznalni.)
    const char*     resourceName;

    //Az eroforrast hasznalok szama, mely lehet egy masik eroforras is, vagy
    //lehet maga az applikacio is.
    //Minden ujabb hasznalatbaveteli kerelem noveli ezt a szamlalot, majd
    //minden lemondas csokkenti.
    //Ha a szamlalo 0-ra csokken, akkor az eroforras hasznalata leallithato.
    uint32_t        usageCnt;

    //Az eroforrast igenylo eroforrasok lancolt listaja.(Felso szinetek a faban)
    struct
    {
        resourceDep_t*    first;
        resourceDep_t*    last;
    } requesterList;

    //Az eroforrashoz tartozo fuggosegek szama.
    uint32_t        depCount;
    //A meg inditasra varo fuggosegek szamat nyilvantaro szamlalo.
    //Ha erteke 0-ra csokken, akkor az eroforras mukodesehez tratozo egyeb
    //fuggosegek elindultak, es az eroforras is indithato.
    uint32_t        depCnt;

    //Az eroforrashoz tartozo sajat fuggosegek lancolt listajanak eleje es vege.
    //Ebben sorakoznak az eroforras mukodesehez szukseges tovabbi fuggosegek.
    struct
    {
        resourceDep_t*    first;
        resourceDep_t*    last;
    } dependencyList;

    //Az eroforrast hasznalok lancolt listajanak elso es utolso eleme
    struct
    {
        struct resourceUser_t*   first;
        struct resourceUser_t*   last;        
    } userList;


    //Fuggoben levo inditasi kerelmek. Az eroforras hasznalatbavetele noveli.
    uint32_t startReqCnt;
    //Az eroforrasrol torteno lemondasok szama.
    uint32_t stopReqCnt;

    //A kiertekelesre varo eroforrasok lancolt listaja.
    //A taszkban azokkal az eroforrasokkal kell foglalkozni, melyek hozza
    //vannak adva a listahoz.
    struct
    {
        struct resource_t* next;
        struct resource_t* prev;
        bool inTheList;
    } processReqList;


    //Az eroforras altal hasznalt hibara futott eroforrasok listaja.
    //Ha egy fuggosege hibara fut, akkor annak statusz fuggveny hivasaban a
    //hibara futott eroforras regisztralja magat a listaban, mely listat a
    //manager taszkjaban dolgozunk fel. A feldolgozott lista elem torlodik.
    struct
    {
        resourceDep_t* first;
        resourceDep_t* last;
    } depErrorList;

    //Az eroforras utolso hibakodja, melyet kapott egy statusz callbackben
    //status_t lastErrorCode;

    //Hiba informacios blokk, melyen keresztul kesobb az eroforrast hasznalo
    //masik  eroforrasok, vagy userek fele riportolhatja a hibat.
    resourceErrorInfo_t errorInfo;

    //Az eroforras altal riportolt hibara mutat. Ezt beallithatja valamelyik
    //hibat jelzo fuggosege alalapjan, vagy allithatja sajat magara is.
    //Ha ez NULL, akkor sem az eroforras, sem valamelyik fuggosege nem jelez
    //hibat.
    //Amikor az eroforras hibara fut, es ez a pointer NULL, akkor raallitja a
    //sajat errorInfo-jara, hiszen a sorban o idezte elo a hibat, melyet
    //a feljebbi szintek fele reportolunk.
    //Ha fuggosegi hibat kap, akkor az egyik (a feldolgozasi sorban levo elso)
    //fuggosegetol veszi at, es kesobb azt publikalja tovabb.
    //- Ha a reportedError pointere nem NULL, akkor az eroforras hibas.
    //- Ha NULL, akkor nincs hiba
    //- Ha sajat magara mutat, akkor o kezdte a hibas mukodest
    //- Ha az error pointere nem sajat magara mutat, akkor egy fuggosege miatt
    //  kerult hibara, es annak a hibajat mutatja.
    resourceErrorInfo_t* reportedError;

    //True, ha az eroforras el lett inditva. Annak meghivasra kerult az indito
    //fuggvenye. Ha nincs start fuggveny definialva, akkor ez a flag nem kerul
    //beallitasra.
    //Ez akkor erdekes, ha egy eroforras STARTING allapotban van, de meg var
    //valamelyik fuggosegere, igy annak addig nem hivodik meg a start()
    //fuggvenye. A flag segitsegevel tudhato, hogy az eroforras belso mecha-
    //nizmusai meg nem allnak keszen a hibakezeles fogadasara.
    bool    started;

    //true, ha az eroforras fut, es kiadta az igenyloi fele a jelzest, hogy
    //elindult. Csak akkor hivhatjuk meg a kerelmezoinek a dependencyStop()
    //fuggvenyet, ha ez true.
    bool runningFlag;

    //true-val jelezzuk, hogy ellenorizze az eroforras, hogy el kell-e inditani,
    //vagy le kell-e valami miatt allitani az eroforarst.
    bool checkStartStopReq;

    //true, ha az eroforarst hasznalo userek fele jelezni kell az uj allapotot.
    bool signallingUsers;

    //true-val jelzi, hogy az eroforras inicializalva van. Annak az elso
    //inditasnal lefutott az init() callbackje.
    bool inited;

    //true-val jelzi, ha hibas allapotban van
    bool errorFlag;

    //true-val jelzi, hogy az eroforars vegzett a feladataval. Ez az egyszer
    //lefuto feladatokat elvegzo eroforarsoknal hasznaljuk. (kesobbi fejlesztes)
    bool doneFlag;

    //az eroforars kenyszeritett leallitasanak kerelmet jelzo flag
    bool haltReq;

    //Az eroforras hiba allapotban volt, es leallt, tehat STOP statuszt
    //jelentett magarol.
    bool haltedFlag;

    //true jelzi, hogy az eroforras elindult. Az eroforras RUN statuszara all
    //be.
    bool runFlag;

    bool debugFlag;

    //Tetszoleges eroforras kiegeszitesre mutat. Ilyen lehet peldaul, ha egy
    //eroforrashoz letrehoztak taszkot.
    void* ext;
} resource_t;
//------------------------------------------------------------------------------
//Az applikacio fele callback definicio.
//Ezen keresztul jelez vissza, ha az hasznalt eroforrasban valami allapot
//valtozas all be. (pl elindul, megallt, hiba van vele, ...)
typedef void resourceStatusFunc_t( resourceStatus_t resourceStatus,
                                   resourceErrorInfo_t* errorInfo,
                                   void* callbackData);
//------------------------------------------------------------------------------
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
    //Az eroforras hibara futott
    RESOURCEUSERSTATE_ERROR,
} resourceUserState_t;
//------------------------------------------------------------------------------
//Az eroforrast hasznalo (birtoklo) folyamatokhoz tartozik egy-egy ilyen leiro.
//Ezen keresztul tortenik az egyes eroforrasok kerelme az applikacio felol.
typedef struct
{
    //A hasznalni kivant eroforrasra mutat
    resource_t* resource;

    //Az eroforrast hasznalo user allapota.
    resourceUserState_t    state;

    //true, ha a hozza tartozo eroforrasrol valo lemondaskor az eroforrasnak meg
    //tovabb kell mukodni, mivel mas userek vagy eroforrasok meg hasznaljak.
    //bool resourceContinuesWork;

    //Az eroforras allapotvaltozasa eseten feljovo callback. Ez alapjan tudja
    //peldaul a kerelmezo, hogy a kert eroforras elindult, es hasznalhatja, vagy
    //hibara futott.
    //generalResourceUser_t eseten ezt nem allitgatjuk be kulon!
    resourceStatusFunc_t*     statusFunc;
    void*                   callbackData;

    //A felhasznalo neve. (Ezt a debuggolashoz es listazashoz tudjuk hasznalni.)
    const char*             userName;

    //Az ujrainditasi kerelmet jelzo flag.
    bool                    restartRequestFlag;

    //Az eroforrast hasznalo userek lancolt listajahoz szukseges
    struct
    {
        struct resourceUser_t*     next;
        struct resourceUser_t*     prev;
    } userList;

} resourceUser_t;
//------------------------------------------------------------------------------
//MyRM valtozoi
typedef struct
{
    //Eroforras managemenet futtato taszk leiroja
    TaskHandle_t taskHandle;

    //A manager valtozoit vedo mutex
    SemaphoreHandle_t   mutex;
  #if configSUPPORT_STATIC_ALLOCATION
    StaticSemaphore_t   mutexBuffer;
  #endif

    //A futo eroforrasokat szamolja
    uint32_t    runningResourceCount;
    //Minden eroforras sikeres leallitasa utan hihato callback fuggveny
    MyRM_allResourceStoppedFunc_t* allResourceStoppedFunc;
    //A callback szamara atadott tetszoleges parameter
    void* callbackData;


    //A kiertekelesre varo eroforrasok lancolt listaja.
    //A taszkban azokkal az eroforrasokkal kell foglalkozni, melyek hozza
    //vannak adva a listahoz.
    struct
    {
        resource_t* first;
        resource_t* last;
    } processReqList;

    //A nyilvantartott eroforrasok lancolt listajanak az elso es utolso eleme.
    //Az MyRM_createResource() management rutinnal kerulnek hozzadasra.
    //Ez alapjan az eroforrasokon vegig lehet haladni, es azokon csoportos
    //muveleteket vegrehajtani.
    struct
    {
        resource_t*    first;
        resource_t*    last;
    } resourceList;

} MyRM_t;
extern MyRM_t myRM;
//------------------------------------------------------------------------------
// kezdeti inicializalasa
void MyRM_init(const MyRM_config_t* cfg);

//A konzolra irja a szamontartott eroforrasokat, azok allapotait, es azok
//hasznaloit. Debuggolai celra hasznalhato.
void MyRM_printUsages(bool printDeps);

//A futo/aktiv eroforrasok szamanak lekerdezese
uint32_t MyRM_getRunningResourcesCount(void);

//Minden eroforras sikeres leallitasakor hivodo callback funkcio beregisztralasa
void MyRM_register_allResourceStoppedFunc(MyRM_allResourceStoppedFunc_t* func,
                                          void* callbackData);
//Eroforras letrehozasa.
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra!
void MyRM_createResource( resource_t* resource,
                          const resourceFuncs_t* funcs,
                          void* funcsParam,
                          const char* resourceName);

//Egyedi eroforras letrehozasa, bovitmeny megadasaval
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra.
void MyRM_createResourceExt(resource_t* resource,
                          const resourceFuncs_t* funcs,
                          void* funcsParam,
                          const char* resourceName,
                          void* ext);

//Eroforras leirojanak lekerdezese az eroforras neve alapjan.
//NULL-t ad vissza, ha az eroforras nem talalhato.
resource_t* MyRM_getResourceByName(const char* name);

//Eroforrasok koze fuggoseg beallitasa (hozzaadasa).
//highLevel: Magasabb szinten levo eroforras, melynek a fuggoseget definialjuk
//lowLevel: Az alacsonyabb szinten levo eroforras, melyre a magasabban levo
//mukodesehez szukseg van (highLevel).
void MyRM_addDependency(resourceDep_t* dep,
                         resource_t* highLevel,
                         resource_t* lowLevel);

//Olyan specialis makro letrehozasa, mely segit definialni az eroforrasok kozti
//fuggosegeket.
//A makro(k) letrehoz a forraskod sora szerint egy DEP_<sorszam> nevu valtozot.
//A letrehozott valtozo "static" elotagot kap, igy az nem a stacken jon letre,
//igy a definialodo fuggoseg leiro kesobb is a memoriaban marad.
//Ezt kepes helyettesiteni
//  static resourceDep_t valami;
//  MyRM_addDependency(&valami, &hi, &lo);
#define CREATE_RESOURCE_DEPENDENCY(hi, lo)    {static resourceDep_t MY_EVALUATOR(DEP_,__LINE__);  MyRM_addDependency(& MY_EVALUATOR(DEP_,__LINE__), hi, lo);}

//Az egyes eroforrasok ezen keresztul jelzik a manager fele az allapotvaltozasa-
//ikat.
//Megj: Ezt a callbacket az eroforrasok a Start vagy Stop funkciojukbol is
//hivhatjak!
void MyRM_resourceStatus(resource_t* resource,
                         resourceStatus_t resourceStatus,
                         status_t errorCode);

//Eroforrasok inditasa/megallitasa
//csak teszteleshez hasznalhato, ha kivulrol hivjuk.
void MyRM_startResource(resource_t* resource);
void MyRM_stopResource(resource_t* resource);

//------------------------------------------------------------------------------
//Az eroforrashoz az applikacio felol hivhato USER hozzaadasa.
//A hasznalok lancolt listajahoz adja az User-t.
void MyRM_addUser(resource_t* resource,
                   resourceUser_t* user,
                   const char* userName);
//Egy eroforrashoz korabban hozzaadott USER kiregisztralasa.
//A hasznalok lancolt listajabol kivesszuk az elemet
void MyRM_removeUser(resource_t* resource, resourceUser_t* user);

//Eroforras hasznalata.
//Hatasara a kert eroforras ha meg nins elinditva, elindul, majd az user-hez
//beregisztraltkeresztul jelzi, annak sikeresseget, vagy hibajat.
//Az atadott user struktura bekerul az eroforrast hasznalok lancolt listajaba.
void MyRM_useResource(resourceUser_t* user);
//Eroforrasrol lemondas.
//Ha az eroforras mar senki sem hasznalja, akkor az le lessz allitva.
//Az user-hez beregisztralt callbacken keresztul majd vissza fog jelezni, ha az
//eroforras mukodese befejezodott.
void MyRM_unuseResource(resourceUser_t* user);

//Az resourceUser_t struktura beallitasat segito rutin.
//user: a hasznalati leirora mutat
//statusFunc: A hasznalt eroforras allapotat visszajelzo callback rutin
//callbackData: A callbackban visszaadott tetszolseges valtozo
static inline void MyRM_configUser( resourceUser_t* user,
                                    resourceStatusFunc_t* statusFunc,
                                    void* callbackData)
{
    user->statusFunc=statusFunc;
    user->callbackData=callbackData;
}

//Eroforras ujrainditasi kerelme. Hibara futott eroforrasok eseten az eroforras
//hibajanak megszunese utan ujrainditja azt.
void MyRM_restartResource(resourceUser_t* user);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYRM_H_
