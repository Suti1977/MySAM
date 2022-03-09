//------------------------------------------------------------------------------
//  Szoftveres idozito modul
//
//    File: MySwTimer.c
//------------------------------------------------------------------------------
#include "MySwTimer.h"
#include <string.h>


//------------------------------------------------------------------------------
//Idozito manager kezdeti inicializalasa
void MySwTimer_initManager(MySwTimerManager_t* manager)
{
    //Modul valtozoinak kezdeti torlese.
    memset(manager, 0, sizeof(MySwTimerManager_t));
}
//------------------------------------------------------------------------------
//Uj idozito hozzaadasa az idozito managerhez
void MySwTimer_addTimer(MySwTimerManager_t* manager, MySwTimer_t* timer)
{
    memset(timer, 0, sizeof(MySwTimer_t));

    if (manager->firstTimer==NULL)
    {   //Meg nincs beregisztralva hasznalo. Ez lesz az elso.
        manager->firstTimer=timer;
        timer->prev=NULL;
    } else
    {   //Mar van a listanak eleme. Az utolso utan fuzzuk.
        timer->prev=(struct MySwTimer_t*) manager->lastTimer;

        ((MySwTimer_t*)manager->lastTimer)->next=
                (struct MySwTimer_t*) timer;
    }
    //A sort lezarjuk. Ez lesz az utolso.
    timer->next=NULL;
    manager->lastTimer=timer;

    //Megjegyzi a timerhez tartozo managert.
    timer->manager=(struct MySwTimerManager_t*) manager;
}
//------------------------------------------------------------------------------
//Idozito torlese az idozito manager altal kezelt idozitok listajabol
void MySwTimer_deleteTimer(MySwTimerManager_t* manager, MySwTimer_t* timer)
{
    MySwTimer_t* prev=(MySwTimer_t*)timer->prev;
    MySwTimer_t* next=(MySwTimer_t*)timer->next;

    if ((prev) && (next))
    {   //lista kozbeni elem. All elotet es utana is elem a listaban
        prev->next=(struct MySwTimer_t*)next;
        next->prev=(struct MySwTimer_t*)prev;
    } else
    if (next)
    {   //Ez a lista elso eleme, es van meg utana elem.
        //A kovetkezo elem lesz az elso.
        manager->firstTimer=next;
        next->prev=NULL;
    } else if (prev)
    {   //Ez a lista utolso eleme, es van meg elote elem.
        prev->next=NULL;
        manager->lastTimer=prev;
    } else
    {   //Ez volt a lista egyetlen eleme
        manager->lastTimer=NULL;
        manager->firstTimer=NULL;
    }

    timer->next=NULL;
    timer->prev=NULL;
}
//------------------------------------------------------------------------------
//Idozito manager futtatasa
//tick-ben meg kell adni az aktualis idot
status_t MySwTimer_runManager(MySwTimerManager_t* manager, uint64_t time)
{
    //A manager vegigszalad a beregisztralt timereken, es amelyik fut, azt
    //ellenorzi, hogy az idozitese letelt e. Ha letelt, akkor a leiroban
    //bejegyzi annak tenyet, melyet kesobb le lehet kerdezni.
    //Ha az idozito csk egysezr fut le, akkro az idozitot azonnal le is tiltja.
    //Ha az idozito periodikus, akkor a periodusidovel kesobbi idopontot
    //allit be ra.

    //Aktualis ido elmentese.
    manager->time=time;

    //Ellenorzes, hogy egyaltalan van-e mit futtatni...
    //Ha Meg nem erte el a futtatas idejet, akkor kilepes.
    if (manager->nextExecutionTime>time) return kStatus_Success;


    //A leheto legnagyobb idore allitjuk a manager kovetkezo futtatasi
    //idopontjat. Ha marad aktiv timer, akkor a legkisebb idot veszi majd fel.
    manager->nextExecutionTime=~0ull;   //0xffffffffffffffff;

    //ciklus,a mig minden timeren vegigert.
    MySwTimer_t* timer=manager->firstTimer;
    for(; timer; timer=(MySwTimer_t*)timer->next)
    {
        //Ha  atimer nem mukodik, akkor atugorja
        if (timer->active==false) continue;

        if (timer->nextTime > time) continue;

        //A timer idozitese lejart.

        //jelezi a timerben
        timer->expired=true;

        if (timer->periodTime)
        {   //ez egy periodikusan mukodo idozito. Uj idozitesi idopont
            //beallitasa
            timer->nextTime += timer->periodTime;

            //A kovetkezo futtatas idejenek szamitasa.
            //A legkorabbi idopontot veszi fel a manager.
            if (timer->nextTime < manager->nextExecutionTime)
            {
                manager->nextExecutionTime=timer->nextTime;
            }

        } else
        {   //CSak egyszer lefuto idozito. Az idozito leall.
            timer->active=false;
        }

    } //for

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Annak az idonek a lekerdezese, amennyi ido mulva a managert ujra futtatni kell.
uint64_t MySwTimer_getWaitTime64(MySwTimerManager_t* manager)
{
    return manager->nextExecutionTime - manager->time;
}
//------------------------------------------------------------------------------
//Annak az idonek a lekerdezese, amennyi ido mulva a managert ujra futtatni kell.
//A lekerdezes 32 bites intet ad vissza, es PORT_MAX_DELAY vagy 0xffffffff-ra
//kerul szaturalasra, ha 32 bitnel naygaobb szamertek jonne ki.
uint32_t MySwTimer_getWaitTime32(MySwTimerManager_t* manager)
{
    uint64_t waitTime=MySwTimer_getWaitTime64(manager);

    #ifdef PORT_MAX_DELAY
      #define MAX_WT  PORT_MAX_DELAY
    #else
      #define MAX_WT 0xffffffff
    #endif

    if (waitTime > MAX_WT) return  MAX_WT;
    else return (uint32_t) waitTime;
}
//------------------------------------------------------------------------------
//Annak lekerdezese, hogy az idozites lejart-e
//true-t ad vissza, ha lejart. Egyben a jelzes torlese is.
bool MySwTimer_expired(MySwTimer_t* timer)
{
    bool expired=timer->expired;

    //jelzes torlese a leiroban
    timer->expired=false;

    return expired;
}
//------------------------------------------------------------------------------
//Annak lekerdzese, hogy az idozito aktiv-e
bool MySwTimer_isActive(MySwTimer_t* timer)
{
    return timer->active;
}
//------------------------------------------------------------------------------
//Timer leallitasa
void MySwTimer_stop(MySwTimer_t* timer_)
{
    timer_->active=false;

    MySwTimerManager_t* manager=(MySwTimerManager_t*) timer_->manager;

    //A leheto legnagyobb idore allitjuk a manager kovetkezo futtatasi
    //idopontjat. Ha marad aktiv timer, akkor a legkisebb idot veszi majd fel.
    manager->nextExecutionTime=~0ull;   //0xffffffffffffffff;

    //Managerben idozites ujraszamitasa. A legorabbi idopont kikeresese...
    //ciklus,a mig minden timeren vegigert.
    MySwTimer_t* next=manager->firstTimer;
    for(; next; next=(MySwTimer_t*)next->next)
    {
        //Ha  atimer nem mukodik, akkor atugorja
        if (next->active==false) continue;

        //A legkorabbi idopontot veszi fel a manager.
        if (next->nextTime < manager->nextExecutionTime)
        {
            manager->nextExecutionTime=next->nextTime;
        }

    } //for

}
//------------------------------------------------------------------------------
void MySwTimer_start(MySwTimer_t* timer,
                     uint32_t interval,
                     uint32_t periodTime)
{
    MySwTimerManager_t* manager=(MySwTimerManager_t*) timer->manager;

    //Az (elso) idozites idopontjanak kiszamitasa.
    timer->nextTime = manager->time + interval;

    //Periodusido megjegyzese. Ha ez nem 0, akkor enyni idonkent automatikusan
    //ujra fog indulni a timer, az elso (interval) ido utan.
    timer->periodTime=periodTime;

    //A timer meg nem jart le.
    timer->expired=false;

    //timer inditasa
    timer->active=true;

    //Managerben idozites ujraszamitasa...
    if (timer->nextTime < manager->nextExecutionTime)
    {   //E timer korababn kell, hogy kiszolgaalsra keruljon, mint amit a
        //managerben futtatasi idopontot isemerunk. Ez a timer lesz kiszolgalva
        //eloszor.
        manager->nextExecutionTime=timer->nextTime;
    }
}
//------------------------------------------------------------------------------
