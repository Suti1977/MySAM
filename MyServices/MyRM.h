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
    //hasznalhato. (initkor is ezt az allapotot veszi fel.)
    RESOURCE_STOP=0,
    //Az eroforras elindult.
    RESOURCE_STARTED,
    //Eroforras mukodik, a rendszer szamara hasznalhato
    RESOURCE_RUN,
    //Az eroforras megkezdte a leallasat
    RESOURCE_STOPPING,
    //Az eroforras befejezte a mukodeset
    RESOURCE_DONE,
    //Az eroforras mukodese/initje hibara futott
    RESOURCE_ERROR,
} resourceStatus_t;

//eroforrasok statuszahoz tartozo stringek. (Nyomkoveteshez)
//FONTOS, HOGY A SORRENDJUK SZINKRONBAN LEGYEN AZ ENUMOKKAL!
#define RESOURCE_STATUS_STRINGS \
{   "STOP",                     \
    "STARTED",                  \
    "RUN",                      \
    "STOPPING",                 \
    "DONE",                     \
    "ERROR",                    \
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
{   "STOP",                     \
    "STARTING",                 \
    "RUN",                      \
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

//Eroforrasok/userek fele adott statusz callback tipus definicioja
struct resourceDep_t;
typedef void resourceDependencyStatusFunc_t(struct resourceDep_t* dep,
                                            resourceStatus_t status,
                                            resourceErrorInfo_t* errorInfo);
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
//Az eroforrast hasznalo user tipusa.
typedef enum
{
    //eroforras
    RESOURCE_REQUESTER_TYPE__RESOURCE,
    //eroforrast hasznalo user
    RESOURCE_REQUESTER_TYPE__USER,
} resourceRequesterType_t;
//------------------------------------------------------------------------------
//Egy eroforras mas eroforrasoktol valo fuggoseget leiro struktura.
//Ezekbol egy lancolt lista alakulhat ki, eroforrasonkent.
//Minden eroforrashoz annyit kell letrehozni, amennyi fuggosege van.
//A struktura 2 lancolt listaba is elemkent szerepel.
// - A tulajdonos listaja
// - Az alapfeltetelnek megadott (igenyelt) eroforrashoz tartozo listaban
typedef struct
{
    struct
    {
        //A fuggosegi leirohoz tartozo igenylo tipusat azonositja.
        uint32_t    requesterType :1;

        //true-val jelzi, ha egy inditasi kerelem fuggoben van, melyrol a
        //fuggosegnek csak a leallasa utan szabad tudomast szereznie.
        //Olyan esetekben tarolodik le ez a jelzes, amikor egy egy fuggoseg a
        //leallitasi folyamata alatt kap ujabb haszanlati kerelmet.
        uint32_t delayedStartRequest :1;

        //Statusz kerese. Akkor erdekes, amikor egy olyan hasznalat/lemondas
        //tortenik, ami nem okoz az eroforrasban allapot valtozast, igy abban
        //az allapotvaltozaskor egyebkent lefuto statusz kuldesek nem jutnak el
        //a kerelmezokhoz. A jelzes hatasara a kerelmezok a MyRM taszkjabol
        //kapnak statusz informaciot.
        uint32_t statusRequest :1;
    } flags;

    //A leiro altal igenyelt eroforrasra mutat.
    struct resource_t*     requiredResource;
    //Az eroforrashoz tartozo fuggosegi lancolt lista kezelesehez szukseges.
    //Ezen halad vegig az igenyelt eroforras, es jelez vissza a kerelmezoknek az
    //aktualis statuszarol.   
    struct resourceDep_t*  nextRequester;

    //A leirot birtoklo eroforrasra/userre mutato pointerek. A requesterType
    //donti el.
    union
    {
        struct resource_t*     requesterResource;
        struct resourceUser_t* requesterUser;
        void*                  requester;
    };

    //A kerelmezo eroforras fuggosegi listaja
    struct resourceDep_t*  nextDependency;

    //fuggoseg altal hasznalt statsusz callback. A fuggoseg az egyes allapot
    //valtozasiairol ezen keresztul tajekoztatja az ot hasznalo eroforrast vagy
    //usert.
    resourceDependencyStatusFunc_t* depStatusFunc;

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
    resourceFuncs_t    funcs;
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
    //Ha erteke 0-ra csokken, akkor az eroforras mukodesehez tartozo egyeb
    //fuggosegek elindultak, es az eroforras is indithato.
    uint32_t        depCnt;

    //Az eroforrashoz tartozo sajat fuggosegek lancolt listajanak eleje es vege.
    //Ebben sorakoznak az eroforras mukodesehez szukseges tovabbi fuggosegek.
    struct
    {
        resourceDep_t*    first;
        resourceDep_t*    last;
    } dependencyList;

    //A kiertekelesre varo eroforrasok lancolt listaja.
    //A taszkban azokkal az eroforrasokkal kell foglalkozni, melyek hozza
    //vannak adva a listahoz.
    struct
    {
        struct resource_t* next;
        struct resource_t* prev;
        bool inTheList;
    } processReqList;

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

    struct
    {
    //true-val jelezzuk, hogy ellenorizze az eroforras, hogy el kell-e inditani,
    //vagy le kell-e valami miatt allitani az eroforrast.
    uint16_t checkStartStopReq :1;

    //True, ha az eroforras el lett inditva. Annak meghivasra kerult az indito
    //fuggvenye. Ha nincs start fuggveny definialva, akkor ez a flag nem kerul
    //beallitasra.
    //Ez akkor erdekes, ha egy eroforras STARTING allapotban van, de meg var
    //valamelyik fuggosegere, igy annak addig nem hivodik meg a start()
    //fuggvenye. A flag segitsegevel tudhato, hogy az eroforras belso mecha-
    //nizmusai meg nem allnak keszen a hibakezeles fogadasara.    
    uint16_t started :1;

    //true, ha az eroforras fut, es kiadta az igenyloi fele a jelzest, hogy
    //elindult. Csak akkor hivhatjuk meg a kerelmezoinek a dependencyStop()
    //fuggvenyet, ha ez true.
    uint16_t running :1;


    //true, ha az eroforrast hasznalo userek fele jelezni kell az uj allapotot.
    //bool signallingUsers;

    //true-val jelzi, hogy az eroforras inicializalva van. Annak az elso
    //inditasnal lefutott az init() callbackje.
    uint16_t inited :1;

    //true-val jelzi, ha hibas allapotban van
    uint16_t error :1;

    //true-val jelzi, hogy az eroforras vegzett a feladataval. Ez az egyszer
    //lefuto feladatokat elvegzo eroforrasoknal hasznaljuk. (kesobbi fejlesztes)
    uint16_t done :1;

    //az eroforras kenyszeritett leallitasanak kerelmet jelzo flag
    uint16_t haltReq :1;

    //Az eroforras hiba allapotban volt, es leallt, tehat STOP statuszt
    //jelentett magarol.
    uint16_t halted :1;

    //true jelzi, hogy az eroforras elindult. Az eroforras RUN statuszara all
    //be.
    uint16_t run :1;

    //Statusz kerese. Akkor erdekes, amikor egy olyan hasznalat/lemondas
    //tortenik, ami nem okoz az eroforrasban allapot valtozast, igy abban
    //az allapotvaltozaskor egyebkent lefuto statusz kuldesek nem jutnak el
    //a kerelmezokhoz. A jelzes hatasara a kerelmezok a MyRM taszkjabol
    //kapnak statusz informaciot.
    uint32_t statusRequest :1;

    //nyomkoveteshez hasznalt tetszoleges flag bit.
    uint16_t debug :1;
    } flags;


    //Statuszt kerelmezok lancolt listaja.
    //A listaba regisztralt callbackek kerulnek vegighivasra, ha az eroforras
    //allapota valtozott. Egy eroforrashoz igy tobb statusz kerelmezo is
    //regisztralhat.
    struct resourceStatusRequest_t* firstStatusRequester;

    //Tetszoleges eroforras kiegeszitesre mutat. Ilyen lehet peldaul, ha egy
    //eroforrashoz letrehoztak taszkot.
    void* ext;
} resource_t;
//------------------------------------------------------------------------------
//Az applikacio fele callback definicio.
//Ezen keresztul jelez vissza, ha a hasznalt eroforrasban valami allapot
//valtozas all be. (pl elindul, megallt, hiba van vele, ...)
typedef void resourceStatusFunc_t( resource_t* resource,
                                   resourceStatus_t resourceStatus,
                                   resourceErrorInfo_t* errorInfo,
                                   void* callbackData);

//Az egyes eroforrasokhoz tartozo, eroforars statuszt kerelmezok listaja.
typedef struct
{
    //A hivott callback funkcio
    resourceStatusFunc_t* statusFunc;
    //tetszoleges adattartalom
    void* callbackData;
    //Lancolt lista kovetkezo elemere mutat
    struct resourceStatusRequest_t* next;
} resourceStatusRequest_t;
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

//userek allapotahoz tartozo stringek.
//FONTOS, HOGY A SORRENDJUK SZINKRONBAN LEGYEN AZ ENUMOKKAL!
#define RESOURCE_USER_STATE_STRINGS \
{   "IDLE",                         \
    "WF START",                     \
    "RUN",                          \
    "WF STOP/DONE",                 \
    "ERROR",                        \
}
//------------------------------------------------------------------------------
//Az eroforrast hasznalo (birtoklo) folyamatokhoz tartozik egy-egy ilyen leiro.
//Ezen keresztul tortenik az egyes eroforrasok kerelme az applikacio felol.
typedef struct
{
    //A hasznalni kivant eroforras eleresehez dependencia leiro.
    resourceDep_t dependency;

    //Az eroforrast hasznalo user allapota.
    resourceUserState_t    state;

    //Az eroforras allapotvaltozasa eseten feljovo callback. Ez alapjan tudja
    //peldaul a kerelmezo, hogy a kert eroforras elindult, es hasznalhatja, vagy
    //hibara futott.
    //generalResourceUser_t eseten ezt nem allitgatjuk be kulon!
    resourceStatusFunc_t*   statusFunc;
    void*                   callbackData;

    //A felhasznalo neve. (Ezt a debuggolashoz es listazashoz tudjuk hasznalni.)
    const char*             userName;

    //Az ujrainditasi kerelmet jelzo flag.
    bool                    restartRequestFlag;

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

//Eroforras letrehozasanal hasznalt konfiguracios struktura
typedef struct
{
    //Az eroforras neve. Nyomkoveteshez.
    const char* name;
    //Az eroforrast vezerlo callback fuggvenyek
    const resourceFuncs_t funcs;
    //A funkciohivasoknak atadott tetszoleges adattartalom.
    void* callbackData;
    //Az eroforras bovitmenyere mutato tetszoleges pointer.
    void* ext;
} resource_config_t;

//Eroforras letrehozasa.
//Csak egyszer hivodhat meg reset utan, minden egyes managelt eroforrasra!
void MyRM_createResource(resource_t* resource, const resource_config_t* cfg);

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

//Eroforrashoz statusz callback beregisztralasa. Egy eroforrashoz tobb ilyen
//kerelem is beregisztralhato.
void MyRM_addResourceStatusRequest(resource_t* resource,
                                   resourceStatusRequest_t* request);


//Eroforrasok inditasa/megallitasa
//csak teszteleshez hasznalhato, ha kivulrol hivjuk.
void MyRM_startResource(resource_t* resource);
void MyRM_stopResource(resource_t* resource);

//------------------------------------------------------------------------------
//User letrehozasahoz hasznalt konfiguracios struktura
typedef  struct
{
    //Az usernek adhato nev, mely segiti a nyomkovetest.
    const char* name;
    //Az user altal hasznalt eroforras
    resource_t* resource;
    //Az user allapotvaltozasaira meghivodo callback funkcio
    resourceStatusFunc_t* statusFunc;
    //A callback szamara atadott tetszoleges adat
    void* callbackData;
} resourceUser_config_t;
//------------------------------------------------------------------------------
//Az eroforras hasznalatat legetove tevo user letrehozasa
void MyRM_createUser(resourceUser_t* user, const resourceUser_config_t* cfg);

//Egy eroforrashoz korabban hozzaadott USER megszuntetese
void MyRM_deleteUser(resourceUser_t* user);

//Eroforras hasznalata.
//Hatasara a kert eroforras ha meg nins elinditva, elindul, majd az user-hez
//beregisztraltkeresztul jelzi, annak sikeresseget, vagy hibajat.
//Az atadott user struktura bekerul az eroforrast hasznalok lancolt listajaba.
void MyRM_useResource(resourceUser_t* user);
//Eroforrasrol lemondas.
//Ha az eroforras mar senki sem hasznalja, akkor az le lesz allitva.
//Az user-hez beregisztralt callbacken keresztul majd vissza fog jelezni, ha az
//eroforras mukodese befejezodott.
void MyRM_unuseResource(resourceUser_t* user);

//Eroforras ujrainditasi kerelme. Hibara futott eroforrasok eseten az eroforras
//hibajanak megszunese utan ujrainditja azt.
void MyRM_restartResource(resourceUser_t* user);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYRM_H_
