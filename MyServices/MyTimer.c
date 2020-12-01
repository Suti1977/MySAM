//------------------------------------------------------------------------------
//  Idozito modul
//
//    File: MyTimer.c
//------------------------------------------------------------------------------
#include "MyTimer.h"
#include <string.h>


//------------------------------------------------------------------------------
static void MyTimer_interruptEnable(Tc* hw);
static void MyTimer_interruptDisable(Tc* hw);

//------------------------------------------------------------------------------
//Idozites kezeles kezdeti inicializalasa.
void MyTimer_initManager(MyTimerManager_t* timerManager,
                         const MyTimerManager_config_t* config)
{
    //Modul valtozoinak torlese
    memset(timerManager, 0, sizeof(MyTimerManager_t));

    //Timereket hajto TC periferia inicializalasa
    MyTC_init(&timerManager->tc, &config->tcConfig);

    Tc* hw=timerManager->tc.hw;
    //A hasznalt TC modul szoftveres resteje
    hw->COUNT16.CTRLA.bit.ENABLE=0; __DMB();
    while(hw->COUNT16.SYNCBUSY.reg);

    hw->COUNT16.CTRLA.bit.SWRST=1; __DMB();
    while(hw->COUNT16.SYNCBUSY.reg);

    //TC konfiguralas...
    //Uzemmod kijelolese a TC periferian (16 bites uzemmodot hasznaluk)
    hw->COUNT16.CTRLA.reg = TC_CTRLA_MODE(TC_CTRLA_MODE_COUNT16_Val); __DMB();
    while(hw->COUNT16.SYNCBUSY.reg);

    hw->COUNT16.CTRLA.reg |=
            //ha at lenne irva az idozites, akkor az a GCLK-ra tortenjen azonnal
            TC_CTRLA_PRESCSYNC(TC_CTRLA_PRESCSYNC_GCLK_Val) |
            //Eloosztas beallitasa (nincs)
            TC_CTRLA_PRESCALER(TC_CTRLA_PRESCALER_DIV1_Val) |
            0;

    //Wave regiszter bellitasa
    hw->COUNT16.WAVE.reg= TC_WAVE_WAVEGEN(TC_WAVE_WAVEGEN_MFRQ_Val);

    //Periodusido beallitasa. Ha eddig elszamol a szamlalo,akkor valt ki megsza-
    //kitast, es kezdi elolrol a szamlalast. Eloosztast nem hasznalunk!
    //MyTIMER_CALC_TC_PEIOD_VALUE() makro segitsegevel szamithato.
    hw->COUNT16.CC[0].reg= config->tcPeriodValue;

    //TC megszakitasok engedelyezese az NVIC-ben.
    MyTC_enableIrq(&timerManager->tc);

    //TC periodikus megszakitas engedelyezese
    MyTimer_interruptEnable(hw);

    //TC engedelyezese
    hw->COUNT16.CTRLA.bit.ENABLE=1;
}
//------------------------------------------------------------------------------
static void MyTimer_interruptEnable(Tc* hw)
{
    hw->COUNT16.INTENSET.reg=TC_INTENCLR_OVF;
    __DMB();
}
//------------------------------------------------------------------------------
static void MyTimer_interruptDisable(Tc* hw)
{
    hw->COUNT16.INTENCLR.reg=TC_INTENCLR_OVF;
    __DMB();
}
//------------------------------------------------------------------------------
//A fociklusbol hivogatott timer task. Azok a callback rutinok, melyekhez
//service van rendelve, ebbol kerul meghivasra.
void MyTimer_loop(MyTimerManager_t* timerManager)
{
    volatile MyTimer_t*	timer;
    volatile uint8_t modeFlags;

    //Vegig haladunk a timerek lancolt listajan, es amelyik be van kapcsolva, es
    //meg nem jart le, annak csokkentjuk a szamlalojat.
    //Ha a szamlalo elerte a 0-at, akkor jelezzuk, hogy az idozites letelt.
    //Ha a szamlalo ciklikus, akkor a szamlalot ujrainditjuk.
    //Ha letelt egy timer, es az van uzemmodban megadva, hogy interruptbol kell
    //a callback rutint meghivni, akkor azt is megtesszuk.

    //A legelso timerer mutatunk a lancolt listaban
    timer=timerManager->firstTimer;

    //Ciklus, amig van elem a sorban...
    while(timer)
    {
        //Addig nem csaphat bele interrupt, amig a timer kiertekelese folyik
        MyTimer_interruptDisable(timerManager->tc.hw);

        modeFlags=timer->modeFlags;

        if ((modeFlags & MYTIMER_INTERRUPT) == 0)
        {	//Ez a timer a fociklusbol futtatja a callback rutinjat

            if (timer->done)
            {	//A timer lejart. (A megszakitasi rutin jelzett)

                //Jelzo flag torlese
                timer->done=false;

                //Ha van definialva callback, akkor meghivjuk
                if (timer->callbackFunc)
                {
                    //Timerek ujra futhatnak, amig a callback vacakol
                    //Az aktualisan feldolgozott timer is.
                    //A callback utan lefuto logika mar nem befolyasolja a
                    //mukodest.
                    MyTimer_interruptEnable(timerManager->tc.hw);

                    timer->callbackFunc(timer->callbackData);

                    //Addig nem csaphat bele interrupt, amig a timer
                    //kiertekelese folyik.
                    MyTimer_interruptDisable(timerManager->tc.hw);
                }


                if ((modeFlags & MYTIMER_PERIODIC)==0)
                {	//Ez egyszer lefuto timer.
                    //A timert letiltjuk. tovabb nem fut.
                    timer->running=false;
                } else
                {	//ez egy ciklikusan futo timer.
                    //Ha timer olyan uzemmodban van, hogy csak a callback
                    //lefutasa utan kell ujrainditani, akkor azt most
                    //megteszuk.
                    if ((modeFlags & MYTIMER_PRECISE)==0)
                    {	//Ez egy olyan timer, hogy periodikus, de a timert
                        //csak az utan szabad ujrainditani, miutan lefutott
                        //a callback rutinja.
                        timer->running=true;
                    }
                }

            } //if (timer->done)
        } //if (timer->modeFlags & MYTIMER_INTERRUPT)

        //Soron kovetkezo timer kijelolese. (NULL, ha ez az utolso)
        timer=(MyTimer_t*)timer->next;


        //Timerek ujra futhatnak
        MyTimer_interruptEnable(timerManager->tc.hw);

    } //while(Timer)
}
//------------------------------------------------------------------------------
//Megszakoitasbol hivott rutin, melyben az idozitest vegzi
void MyTimer_service(MyTimerManager_t* timerManager)
{
    MyTimer_t*	timer;

    //Megszakitasi flag(ek) torlese
    timerManager->tc.hw->COUNT16.INTFLAG.reg=0xff;

    //64 bites szabadon futo tick szmlalo novelese...
    timerManager->tickCnt++;

    //Vegig haladunk a timerek lancolt listajan, es amelyik be van kapcsolva, es
    //meg nem jart le, annak csokkentjuk a szamlalojat.
    //Ha a szamlalo elerte a 0-at, akkor jelezzuk, hogy az idozites letelt.
    //Ha a szamlalo ciklikus, akkor a szamlalot ujrainditjuk.
    //Ha letelt egy timer, es az van uzemmodban megadva, hogy interruptbol kell
    //a callback rutint meghivni, akkor azt is megtesszuk.

    //A legelso timerer mutatunk a lancolt listaban
    timer=timerManager->firstTimer;

    //Ciklus, amig van elem a sorban...
    while(timer)
    {
        if (timer->running)
        {   //A timer be van kapcsolva, es futhat

            timer->cnt--;
            if (timer->cnt==0)
            {   //A timer lejart.

                //Jelzes, hogy a timer lejart.
                //A flaget majd a service rutinban kell torolni.
                //Ugyan ezt a flaget varhatjak a timeout rutinok is.
                timer->done=true;

                uint8_t modeFlags=timer->modeFlags;
                //..............................................................
                if (modeFlags & MYTIMER_INTERRUPT)
                {   //ezt a timert interruptban kell lekezelni.
                    //Ha van callbackja, akkor azt itt hivjuk majd meg...

                    if (modeFlags & MYTIMER_PERIODIC)
                    {   //Ez egy periodikus timer. Automatikusan ujraindul.
                        //Szamlalo ujrainicializalasa
                        timer->cnt=timer->interval;

                    } else
                    {   //Ez a timer csak 1-szer fut le.
                        //Mivel csak egyszer fut le,ezert tiltanunk is kell.
                        timer->running=false;
                    }

                    //Ha van hozza callback, akkor azt meghivjuk.
                    if (timer->callbackFunc)
                    {
                        timer->callbackFunc(timer->callbackData);
                    }
                }
                //..............................................................
                else
                {   //Ezt a timert a fociklusban kezeljuk le.

                    if (modeFlags & MYTIMER_PERIODIC)
                    {   //Ez egy periodikus timer. Automatikusan ujraindul.
                        //Szamlalo ujrainicializalasa
                        timer->cnt=timer->interval;

                        if ((modeFlags & MYTIMER_PRECISE)==0)
                        {   //Ez a timer csak az utan indulhat ujra, ha a
                            //callback lefutott.
                            //Ezert most le kell allitanunk arra az idore,
                            //amig a fociklusbol hivogatott service eszre
                            //nem veszi, hogy lejart a timer. Majd az fogja
                            //ujra aktivalni.
                            timer->running=false;
                        }

                    } else
                    {   //Ez a timer csak 1-szer fut le.

                        //A timer kikapcsolasa.
                        timer->running=false;
                        //A tobbbit majd a fociklusban futo resz oldja meg.
                    }
                }
                //..............................................................
            } //if (Timer->Cnt==0)
        } //if (Timer->Running)

        //Soron kovetkezo timer kijelolese. (NULL, ha ez az utolso)
        timer=(MyTimer_t*)timer->next;

    } //while(Timer)
}
//------------------------------------------------------------------------------
//Globalis tick szammalo lekerdezese
uint64_t MyTimer_getTicks(MyTimerManager_t* timerManager)
{
    uint64_t res;

    //Tobbszor kell megprobalni kiolvasni, amig vegre ketszer ugyan azt nem
    //kapjuk, mivel lehet, hogy a kiolvasas alatt pont belecsap az interrupt.
    //(ez egy 64 bites szamlalo, aminel elofordulhat, hogy a ket 32 bites fel
    //kiolvasasakor keletkezik interrupt, es valamelyik fel megvaltozik.)
    do
    {
        res=timerManager->tickCnt;
    } while (res!=timerManager->tickCnt);

    return res;
}
//------------------------------------------------------------------------------
//Timer letrehozasa, es hozzaadasa a megadott timer managgerhez.
//
//timer	:Az idozitohoz tartozo leiro. (RAM-ban kell, hogy legyen, mivel a
//       strukturaba visszairas tortenik)
//interval	: Idozites ideje
//modeFlags : Mukodest bealito flagek (ezeket vagy kapcsolatban lehet hasznalni)
//			/MYTIMER_SINGLE		-Csak egyszer fut le
//			\MYTIMER_CYCLE		-Ciklikusan mukodo idozito
//
//			/MYTIMER_INTERRUPT	-A timert megszakitasban kezeljuk, a callback
//                               fuggvenye megszakitas alol lesz hivva
//			\MYTIMER_SERVICE    -A timert a fociklusbol hivogatott a
//                               Timer_Service() rutin alol kezeljuk.
//
//			-MYTIMER_PRECISE    -A timerhez tartozo idozito a megszakitasban
//                               ujraindul, fuggetlenul attol, hogy a service
//                               rutinban meg lett-e hivva.
//                               Segitsegevel pontos periodikus idozites allit-
//                               hato elo, de a callback hivasok,azok a fociklus
//                               terheltsegetol fuggoen csuszkalhatnak.
//
//callbackFunc : Idozito esemenyre meghivodo rutin
//name: a timernek nev adhato a nyomkovetest segiteni
//
//A rutin a letrehozott timert nem inditja el automatikusan!
void MyTimer_create(MyTimer_t* timer,
                   MyTimerManager_t* timerManager,
                   uint32_t interval,
                   uint8_t modeFlags,
                   MyTimer_callbackFunc_t *callbackFunc,
                   void *callbackData,
                   const char* name)
{
    timer->running=false;
    timer->interval=interval;
    timer->cnt=interval;
    timer->modeFlags=modeFlags;
    timer->callbackFunc=callbackFunc;
    timer->callbackData=callbackData;

    //Ha definialva van a timernek nevadasi lehetoseg, akkor itt tortenik meg a masolas
    timer->name=name;

    //Timerek megszakitasa letiltva, amig matatjuk a lancot
    MyTimer_interruptDisable(timerManager->tc.hw);


    //A timer beszurasa a lancolt listaba...
    if (timerManager->firstTimer==NULL)
    {   //A lancolt lista ures, mivel a legelso elem sincs definialva.

        //Megjegyezzuk, hogy ez az elso timer a sorban
        timerManager->firstTimer=timer;
    } else
    {   //Mar van eleme a sornak. A sor vegen alloban a most hozzaadandot allitjuk be, mint kovetkezot.
        timerManager->lastTimer->next=(struct MyTimer_t*) timer;
    }
    //Az elozo lancszemnek a sorban korabbi legutolso elem lesz megadva
    timer->prev=(struct MyTimer_t*) timerManager->lastTimer;
    //Nincs tovabbi elem a listaban. Ez az utolso
    timer->next=NULL;


    //Megjegyezzuk, hogy ez a timer az utolso a sorban
    timerManager->lastTimer=timer;

    //Timerek ujra futhatnak
    MyTimer_interruptEnable(timerManager->tc.hw);
}
//------------------------------------------------------------------------------
//Timer torlese. (Kiveszi a listabol.)
void MyTimer_delete(MyTimer_t* timer)
{
    MyTimer_t*  prevTimer;
    MyTimer_t*	nextTimer;

    //A lancolt listaban a kiregisztralt timer elott allo elemre mutat
    prevTimer=(MyTimer_t*)timer->prev;
    // --||--  utanna allo elemre mutat
    nextTimer=(MyTimer_t*)timer->next;

    //Timerek megszakitasa letiltva, amig matatjuk a lancot
    MyTimer_interruptDisable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    if (prevTimer==NULL)
    {   //Ez a timer volt a legelso a sorban, mivel nincs elotte masik

        //A sorban utanna allobol csinalunk elsot
        ((MyTimerManager_t*)timer->timerManager)->firstTimer=nextTimer;
        if (nextTimer)
        {   //Van utanna a sorban.
            //Jelezzuk a kovetkezonek, hogy o lesz a legelso a sorban.
            nextTimer->prev=NULL;
        }else
        {   //Nincs utanna masik elem, tehat ez volt a legutolso elem a listaban
            ((MyTimerManager_t*)timer->timerManager)->lastTimer=NULL;
        }
    } else
    {   //Volt elotte a sorban timer.
        if (nextTimer)
        {   //Volt utanan is. (Ez egy kozbulso listaelem)
            nextTimer->prev=(struct MyTimer_t*) prevTimer;
            prevTimer->next=(struct MyTimer_t*) nextTimer;
        } else
        {   //Ez volt a legutolso elem a listaban
            //Az elozo elembol csinalunk legutolsot.
            prevTimer->next=NULL;

            //Az elozo elem a listaban lesz a kezeles szempontjabol a legutolso
            //a sorban.
            ((MyTimerManager_t*)timer->timerManager)->lastTimer=prevTimer;
        }
    }

    //Timerek ujra futhatnak
    MyTimer_interruptEnable(((MyTimerManager_t*)timer->timerManager)->tc.hw);
}
//------------------------------------------------------------------------------
//Timer elinditasa. A timer ott folytatja, ahol korabban abbahagyta, peldaul
//esetleg egy korabbi stop miatt
//Ha a timer azert allt, mert korabban lejart, akkor az ujra fog indulni.
void MyTimer_start(MyTimer_t* timer)
{
    //Addig nem csaphat bele interrupt, amig a timert matatjuk
    MyTimer_interruptDisable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    if (timer->cnt==0)
    {	//A timer le van jarva.
        //Idozitojenek ujra allitasa
        timer->cnt=timer->interval;
    }
    timer->running=true;
    timer->done=false;

    //Timerek ujra futhatnak
    MyTimer_interruptEnable(((MyTimerManager_t*)timer->timerManager)->tc.hw);
}
//------------------------------------------------------------------------------
//Timer megallitasa
//A szamlalot nem nullazzuk, igy egy ujabb start kiadasaval folytathato az
//idozites.
void MyTimer_stop(MyTimer_t* timer)
{
    //Addig nem csaphat bele interrupt, amig a timert matatjuk
    MyTimer_interruptDisable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    timer->running=false;
    timer->done=false;

    //Timerek ujra futhatnak
    MyTimer_interruptEnable(((MyTimerManager_t*)timer->timerManager)->tc.hw);
}
//------------------------------------------------------------------------------
//Timer ujrainditasa.
//A timer teljesen elolrol kezdi az idozitest.
void MyTimer_restart(MyTimer_t* timer)
{
    //Addig nem csaphat bele interrupt, amig a timert matatjuk
    MyTimer_interruptDisable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    //Idozitojenek ujra allitasa
    timer->cnt=timer->interval;
    timer->running=true;
    timer->done=false;

    //Timerek ujra futhatnak
    MyTimer_interruptEnable(((MyTimerManager_t*)timer->timerManager)->tc.hw);
}
//------------------------------------------------------------------------------
//Timer idozites beallitasa. (Fuggetlenul a letrehozaskor megadott idotol)
void MyTimer_setInterval(MyTimer_t* timer, uint32_t interval)
{
    //Addig nem csaphat bele interrupt, amig a timert matatjuk
    MyTimer_interruptDisable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    timer->interval=interval;

    //Timerek ujra futhatnak
    MyTimer_interruptEnable(((MyTimerManager_t*)timer->timerManager)->tc.hw);
}
//------------------------------------------------------------------------------
//True-t ad vissza, ha az adott timer befejezte a futasat
bool MyTimer_expired(MyTimer_t* timer)
{
    bool ret;

    //Addig nem csaphat bele interrupt, amig a timert matatjuk
    MyTimer_interruptDisable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    ret=timer->done;

    //Timerek ujra futhatnak
    MyTimer_interruptEnable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    return ret;
}
//------------------------------------------------------------------------------
//True-t ad vissza, ha az adott timer meg idozit (fut)
bool MyTimer_isRunining(MyTimer_t* timer)
{
    bool ret;

    //Addig nem csaphat bele interrupt, amig a timert matatjuk
    MyTimer_interruptDisable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    ret=timer->running;

    //Timerek ujra futhatnak
    MyTimer_interruptEnable(((MyTimerManager_t*)timer->timerManager)->tc.hw);

    return ret;
}
//------------------------------------------------------------------------------
//stdio-ra debug informacio irasa a timerekrol
void MyTimer_printDebugInfo(MyTimerManager_t* timerManager)
{
    printf("============TIMERS INFO=================\n");
    printf("Tick: %d\n", (int)MyTimer_getTicks(timerManager));

    printf("INERVAL   CNT        ___FLAGS__    PREW     NEXT     CALLBACK  C. DATA    ____MODE FLAGS___\n");

    MyTimer_t* Timer=timerManager->firstTimer;
    while(Timer)
    {
        printf("%08X  %08X|  ",(int)Timer->interval, (int)Timer->cnt);
        if (Timer->running) printf("Run  "); else printf("Stop ");
        if (Timer->done)    printf("Done "); else printf("     ");
        printf("|%08X  %08X  |",(int)Timer->prev,     (int)(Timer->next));
        printf("%08X  %08X  |", (int)Timer->callbackFunc, (int)Timer->callbackData);
        if (Timer->modeFlags & MYTIMER_PERIODIC) printf("PERIODIC ");
        else                                     printf("SINGLE   ");
        if (Timer->modeFlags & MYTIMER_INTERRUPT)printf("INTERRUPT ");
        else                                     printf("SERVICE   ");
        if (Timer->modeFlags & MYTIMER_PRECISE)  printf("PRECISE ");
        else                                     printf("        ");

        printf("%s",Timer->name);
        printf("\n");

        Timer= (MyTimer_t*)Timer->next;
    }
    printf("========================================\n");
}
//------------------------------------------------------------------------------
