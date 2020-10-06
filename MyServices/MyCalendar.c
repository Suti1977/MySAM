//------------------------------------------------------------------------------
//  Valos ideju ora es naptar kezeles
//
//    File: MyCalendar.c
//------------------------------------------------------------------------------
#include "MyCalendar.h"
#include <string.h>
#include <stdio.h>

static void MyCalendar_lock(MyCalendar_t* this);
static void MyCalendar_unlock(MyCalendar_t* this);
static void MyCalendar_printAlarmInfo(MyCalendar_alarm_t* Alarm);
static void MyCalendar_setTheFirstAlarmTime(MyCalendar_t* this);
//------------------------------------------------------------------------------
//naptar/ora modul kezdeti inicializalasa.
//Az initet meg kell, hogy elozze az RTC periferia inicializalasa
void MyCalendar_init(const MyCalendar_config_t* cfg)
{
    MyCalendar_t* this=&myCalendar;

    //Modul valtozoinak kezdeti torlese.
    memset(this, 0, sizeof(MyCalendar_t));

    //Valtozokat vedo mutex letrehozasa...
    this->mutex=xSemaphoreCreateMutex();

    //Egy esetlegesen korabban (teszetles/fejlesztes) miatt bent maradt
    //ebresztesi idopont torlese...
    MyRTC_mode0_setCompare0(0xffffffff);

    //RTC periferia IT prioritas beallitasa.
    NVIC_SetPriority(RTC_IRQn, cfg->rtcIrqPriority);

    //RTC-ben comparator It engedelyezese
    RTC->MODE0.INTENSET.reg = RTC_MODE0_INTENSET_CMP0;

    //NVIC-ben IRQ engedelyezese. (Kivettem, mert a riasztas hozzaadasanak
    //hatasara ugy is engedelyezve lesz.)
    //NVIC_EnableIRQ(RTC_IRQn);

    //Ha elo van irva, akkor az RTC-t is engedelyezni fogja
    if (cfg->enable) MyRTC_enable(RTC);
}
//------------------------------------------------------------------------------
static void MyCalendar_lock(MyCalendar_t* this)
{
    xSemaphoreTake(this->mutex, portMAX_DELAY);
}
//------------------------------------------------------------------------------
static void MyCalendar_unlock(MyCalendar_t* this)
{
    xSemaphoreGive(this->mutex);
}
//------------------------------------------------------------------------------
//Uj riasztasi leiro hozzaadasa a riasztasi sorhoz.
//Ez meg nem fogja azt beengedelyezni. Arrol gondoskodni kell!
void MyCalendar_addAlarm(MyCalendar_alarm_t* alarm)
{
    MyCalendar_t* this=&myCalendar;

    //A lista modositasa alatt a tobbi taszk elol vedeni kell a valtozokat
    MyCalendar_lock(this);

    //Hozzaadaskor az engedelyezes flaget toroljuk.
    alarm->enabled=false;

    NVIC_DisableIRQ(RTC_IRQn);

    if (this->firstAlarm==NULL)
    {   //Meg nincs beregisztralva hasznalo. Ez lesz az elso.
        this->firstAlarm=alarm;
        alarm->prev=NULL;
    } else
    {   //Mar van a listanak eleme. Az utolso utan fuzzuk.
        alarm->prev=(struct MyCalendar_alarm_t*) this->lastAlarm;

        ((MyCalendar_alarm_t*)this->lastAlarm)->next=
                (struct MyCalendar_alarm_t*) alarm;
    }
    //A sort lezarjuk. Ez lesz az utolso.
    alarm->next=NULL;
    this->lastAlarm=alarm;

    NVIC_EnableIRQ(RTC_IRQn);
    MyCalendar_unlock(this);
}
//------------------------------------------------------------------------------
//Riasztasi leiro torlese a listabol
void MyCalendar_deleteAlarm(MyCalendar_alarm_t* alarm)
{
    MyCalendar_t* this=&myCalendar;

    //A lista modositasa alatt a tobbi taszk elol vedeni kell a valtozokat
    MyCalendar_lock(this);
    NVIC_DisableIRQ(RTC_IRQn);

    //Az engedelyezes flaget toroljuk.
    alarm->enabled=false;

    MyCalendar_alarm_t* prev=(MyCalendar_alarm_t*)alarm->prev;
    MyCalendar_alarm_t* next=(MyCalendar_alarm_t*)alarm->next;

    if ((prev) && (next))
    {   //lista kozbeni elem. All elotet es utana is elem a listaban
        prev->next=(struct MyCalendar_alarm_t*)next;
        next->prev=(struct MyCalendar_alarm_t*)prev;
    } else
    if (next)
    {   //Ez a lista elso eleme, es van meg utana elem.
        //A kovetkezo elem lesz az elso.
        this->firstAlarm=next;
        next->prev=NULL;
    } else if (prev)
    {   //Ez a lista utolso eleme, es van meg elote elem.
        prev->next=NULL;
        this->lastAlarm=prev;
    } else
    {   //Ez volt a lista egyetlen eleme
        this->lastAlarm=NULL;
        this->firstAlarm=NULL;
    }

    alarm->next=NULL;
    alarm->prev=NULL;

    //A megvaltozott lista szerint az elso riasztasi idopontra allitja az RTC-t.
    MyCalendar_setTheFirstAlarmTime(this);

    NVIC_EnableIRQ(RTC_IRQn);
    MyCalendar_unlock(this);
}
//------------------------------------------------------------------------------
//Egy leiro engedelyezese/tiltasa
//Figyelem! Az engedelyezo rutin nem ellenorzi, hogy a leiro valoban hozza van-e
//adva a riasztasi leirok listajahoz!
void MyCalendar_enableAlarm(MyCalendar_alarm_t* alarm, bool enable)
{
    MyCalendar_t* this=&myCalendar;

    //A lista modositasa alatt a tobbi taszk elol vedeni kell a valtozokat
    MyCalendar_lock(this);
    NVIC_DisableIRQ(RTC_IRQn);

    alarm->enabled=enable;

    //A megvaltozott lista szerint az elso riasztasi idopontra allitja az RTC-t.
    MyCalendar_setTheFirstAlarmTime(this);

    NVIC_EnableIRQ(RTC_IRQn);
    MyCalendar_unlock(this);
}
//------------------------------------------------------------------------------
//A leirokban a legkorabbi idopont kikeresese, es arra az RTC hardver riasztasi
//idopontjanak felprogramozasa.
//Mutexelt, es IT blokkolt allapotban hivhato csak!!!!
static void MyCalendar_setTheFirstAlarmTime(MyCalendar_t* this)
{
    //Megnezzuk, hogy melyik a legkorabbi riasztasi idopont. Az kerul
    //beallitasra az alarm regiszterbe...
    MyCalendar_alarm_t* next=this->firstAlarm;
    uint32_t firstAlarmTime=0xffffffff;
    while(next)
    {
        if (next->enabled)
        {   //Ez a leiro engedelyezve van. Figyelembe kell venni.
            if (next->time<firstAlarmTime)
            {   //Talalt egy kisebbet.
                firstAlarmTime=next->time;
            }
        }
        //Lancolt lista kovetkezo elemere all
        next=(MyCalendar_alarm_t*)next->next;
    }

    if (MyRTC_mode0_getCompare0() !=firstAlarmTime)
    {   //Mas ertek kerul az alarm regiszterbe.

        //Beallitjuk az RTC-ben az uj idopontot.
        MyRTC_mode0_setCompare0(firstAlarmTime);

        //Egyszerusites. Minden idopont allitgatas magaval vonja, hogy kenysze-
        //ritetten meghivjuk az RTC interruptjat, es abban a riasztasi lista
        //kiertekelese le kell, hogy fusson. Igy biztositjuk, hogy nehogy
        //valamelyik riasztas kimaradjon.
        NVIC_SetPendingIRQ(RTC_IRQn);
    }
}
//------------------------------------------------------------------------------
//Egy meglevo riasztasi leiro modositasa elojeles masodpercel.
//Egy beengedelyezett leiro eseten is mukodik.
void MyCalendar_adjustAlarm(MyCalendar_alarm_t* alarm, int sec)
{
    MyCalendar_t* this=&myCalendar;
    //A leiro modositasa alatt nem nyulhat a listahoz masik taszk. Nem modosit-
    //hatja masik taszk az RTC-t. Ezert szukseges a mutex hasznalata.
    MyCalendar_lock(this);
    NVIC_DisableIRQ(RTC_IRQn);

    alarm->time += (uint32_t) sec;

    if (alarm->enabled)
    {   //Ez a riasztasi sor egy aktiv eleme. Szukseges az adjust utani
        //riasztasi idopont megallapitasa...
        MyCalendar_setTheFirstAlarmTime(this);
    }

    NVIC_EnableIRQ(RTC_IRQn);

    MyCalendar_unlock(this);
}
//------------------------------------------------------------------------------
//Egy leiroban idozites letrehozasa, az aktualis idohoz kepest.
//Ez a rutin biztositja, hogy mas taszk kozben ne valtoztathassa az idot.
//sec-ben megadjuk, hogy a mostani idopontjoz kepest mikorra legyen beallitva
//az ebresztesi idopont.
//enable ha true, akkor a riasztast be is engedelyezi.
void MyCalendar_setAlarmFromNow(MyCalendar_alarm_t* alarm,
                                uint32_t sec,
                                bool enable)
{
    MyCalendar_t* this=&myCalendar;
    //A leiro modositasa alatt nem nyulhat a listahoz masik taszk. Nem modosit-
    //hatja masik taszk az RTC-t. Ezert szukseges a mutex hasznalata.
    MyCalendar_lock(this);
    NVIC_DisableIRQ(RTC_IRQn);

    //Uj idopont beallitasa a mostani idoponthoz kepest
    alarm->time = MyRTC_mode0_getCounter()+sec;

    alarm->enabled=enable;
    if (enable)
    {   //Ez a riasztasi sor egy aktiv eleme. Szukseges a beallitas utani
        //riasztasi idopont megallapitasa...
        MyCalendar_setTheFirstAlarmTime(this);
    }

    NVIC_EnableIRQ(RTC_IRQn);

    MyCalendar_unlock(this);
}
//------------------------------------------------------------------------------
//Masodperces ora beallitasa.
//(Az RTC periferia 32 bites counter szamlaloja mutexelten lesz beallitva)
//A relativ idopontok automatikusan adjusztalva lesznek.
status_t MyCalendar_setTimeSec(uint32_t sec)
{
    status_t status=kStatus_Success;
    MyCalendar_t* this=&myCalendar;

    //A beallitas alatt a modult mutexelni kell!
    MyCalendar_lock(this);

    //Lekerdezzuk az aktualis idot az RTC-bol, es csak akkor
    //modositjuk, ha az valtozott.
    uint32_t actual=MyRTC_mode0_getCounter();
    if (sec!=actual)
    {   //Kell valtoztatni

        //Timer leallitasa az atprogramozas idejere
        MyRTC_disable(RTC);

        //......................................................................
        //A riasztasi leirok adjusztalasa...
        MyCalendar_alarm_t* alarm=this->firstAlarm;
        while(alarm)
        {
            if (alarm->mode==ALARM_MODE_TIME)
            {   //Ez a bejegyzes egy fix idopontot hataroz meg. Nem szabad
                //modositani.

                //Megoldani, hogy ha az ora ugy lett beallitva, hogy
                //atlepett ezen az idoponton, akkor a hozza tartozo callback
                //legyen meghivva!!!
                //Megoldas lett, hogy a modositasok kozben ilyent tapasztaltunk,
                //akkor egy kenyszeritett Interrupt segitsegevel le lesz majd
                //kezelve mindez.
            } else
            {   //Olyan bejegyzes, melyet adjusztalni kell.
                if (actual<sec)
                {   //+ iranyban hangoljak az orat
                    alarm->time += (sec-actual);
                } else
                {   //- iranyban modositjuk az RTC-t
                    alarm->time -= (actual-sec);
                }
            }

            if (alarm->time < sec)
            {   //Az uj idoponton atlepett a frissen beallitando idopont.
                //Szukseges lesz az idok kiertekelesere!

                //TODO: megoldani!
            }
            //Lancolt lista kovetkezo elemere all
            alarm=(MyCalendar_alarm_t*)alarm->next;
        }
        //......................................................................

        //Uj masodperc beallitasa az RTC periferian.
        MyRTC_mode0_setCounter(sec);

        //RTC szamlalo tovabb futhat
        MyRTC_enable(RTC);

        //Egyszerusites. Minen idopont allitgatas magaval vonja, hogy kenyszeri-
        //tetten meghivjuk az RTC interruptjat, es abban a riasztasi lista
        //kiertekelese le kell, hogy fusson. Igy biztositjuk, hogy nehogy
        //valami riasztas kimaradjon.        
        NVIC_SetPendingIRQ(RTC_IRQn);        
    }

    MyCalendar_unlock(this);
    return status;
}
//------------------------------------------------------------------------------
//Masodperces ora lekerdezese
uint32_t MyCalendar_getTimeSec(void)
{
    uint32_t Res;

    MY_ENTER_CRITICAL();
    Res=MyRTC_mode0_getCounter();
    MY_LEAVE_CRITICAL();

    return Res;
}
//------------------------------------------------------------------------------
//RTC megszakitasakor hivando fuggveny
//Ebben tortenik meg a riasztasi lista kiertekelese...
void MyCalendar_service(void)
{
    //Megszakitasi flagek olvasasa, es torlese
    Rtc* hw=RTC;
    //CMP0 megszakitas torlese. Ezen alapszik a riasztaskezeles.
    hw->MODE0.INTFLAG.reg=RTC_MODE0_INTFLAG_CMP0;

    //<-Ide jut ha:
    //  Idopont egyezes miatt kaptunk megszakitast, vagy mert kenyszeri-
    //  tetten ellenorizni kell a riasztasi listat

    //Ebben fogjuk megkapni a kovetkezo ebresztesi idopontot. (max int.)
    volatile uint32_t nextAlarmTime=0xffffffff;

    //kiolvassuk a friss idot...
    uint32_t sec=MyRTC_mode0_getCounter();

    sec=MyRTC_mode0_getCounter();
    //printf("_ALARM_  -->T: %d\n", (int)sec);

    //vegig elemezzuk a beregisztralt riasztasokat...
    MyCalendar_alarm_t* alarm=myCalendar.firstAlarm;

    //Ha nincs megadva riasztas, akkor kilep.
    if (alarm==NULL) return;

    while(alarm)
    {
        if (alarm->enabled)
        {   //Ez a leiro hasznalatban van. (Engedelyezett.)

            if (alarm->time<=sec)
            {   //Ez az idopont van, vagy mar at is lepte az RTC.


                if (alarm->mode==ALARM_MODE_PERIODIC)
                {   //Periodikus mukodesu timer.
                    //uj idopont kiszamitasa
                    alarm->time += alarm->period;

                    //Keressuk a legkorabbi idopontot a leirokban...
                    if (alarm->time < nextAlarmTime)
                    {   //Talaltunk egy korabbi idopontot a leirokban.
                        //Aktualizalunk.
                        nextAlarmTime=alarm->time;
                    }
                } else
                {   //Csak egyszer fut le az idozites.
                    //A leirot tiltjuk
                    alarm->enabled=false;
                }

                //A hozza beregisztralt callbacket meg kell hivni
                if (alarm->alarmFunc)
                {
                    alarm->alarmFunc(alarm->callbackParam);
                }


            } else
            {   //Ez az idopont meg nem kovetkezett be.

                //Keressuk a legkorabbi idopontot a leirokban...
                if (alarm->time < nextAlarmTime)
                {   //Talaltunk egy korabbi idopontot a leirokban. Aktualizalunk.
                    nextAlarmTime=alarm->time;
                }
            }
        }

        //A lancolt lista kovetkezo elemere lepunk
        alarm=(MyCalendar_alarm_t*) alarm->next;
    } //while(Alarm)

    if (nextAlarmTime!=0xffffffff)
    {   //Van mit beallitani

        //Uj riasztasi idopont beallitasa.
        MyRTC_mode0_setCompare0(nextAlarmTime);
    }
}
//------------------------------------------------------------------------------
static void MyCalendar_printAlarmInfo(MyCalendar_alarm_t* alarm)
{
    printf("[%s] ", alarm->enabled ? "*" : " ");
    printf("%16s   ", alarm->alarmName ? alarm->alarmName : "????");
    printf("Mode: %d  ", alarm->mode);
    printf("Period: %4d  ", (int)alarm->period);
    printf("Time: %d  ", (int)alarm->time);
    printf("\n");
}
//------------------------------------------------------------------------------
//A modul mokodeserol informacio irasa az stdio-ra.
void MyCalendar_printInfo(void)
{
    printf("------CALENDAR INFO----------\n");
    printf("RTC sec:%d\n", (int)MyCalendar_getTimeSec());
    printf("RTC compare:%d\n", (int)MyRTC_mode0_getCompare0());

    vTaskSuspendAll();
    MyCalendar_alarm_t* alarm=myCalendar.firstAlarm;
    while(alarm)
    {
        MyCalendar_printAlarmInfo(alarm);
        //Lancolt lista kovetkezo elemere all
        alarm=(MyCalendar_alarm_t*)alarm->next;
    }
    xTaskResumeAll();
    printf("-----------------------------\n");
}
//------------------------------------------------------------------------------
