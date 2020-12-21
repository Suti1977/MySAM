//------------------------------------------------------------------------------
//  Single-Wire (SWI) driver
//  A rendszerben tobb Wire is lehet. Az eroforrasok optimalizalasa erdekeben a
//  Wire-okhoz egyetlen kozos kezelo modul tartozik, kozos hardver peri-
//  feriakkal. Az egyes Wire-ok kezelese viszont egy idoben nem lehetseges.
//  Ezt a kizarast mutexelessel oldjuk meg.
//
//    File: MySWI.c
//------------------------------------------------------------------------------
#include "MySWI.h"
#include <string.h>


//Ha ez benne van, akkor az egyik GPIO vonalat huzgod a megsazkitas be es
//kilepesenel. A driver teszteleshez hasznalhatjuk.
#if 0
#define MySWI_IRQ_TRACE
#include "MyGPIO.h"
#defien MySWI_IRQ_TRACE_PIN PIN_DEBUG_TP2
#endif

//Ha ez 1, akkor a bit kiirasnal timerhez koti az '1'-es bitek kuldesenel az
//alacsony szint (a busz hajtasanak) idejet.
//Ha 0, akkor kevesebb megszakitas keletkezik. Az alacsonyszintu impulzus idejet
//a processzor sebesseg befolyasolja. Viszont kevesebb nagyon suru, 1us-enkent
//feljovo mesgazkitas kell.
#ifndef MySWI_TX_BIT1_LOW_TIMED
#define MySWI_TX_BIT1_LOW_TIMED    0
#endif


static void MySWI_initTC(MySWI_Driver_t* driver,
                                const MySWI_DriverConfig_t* cfg);
//Wire szintu allapotok
static void MySWI_wState_reset_driveWire(MySWI_Driver_t* driver);
static void MySWI_wState_reset_releaseWire(MySWI_Driver_t* driver);
static void MySWI_wState_readBit_driveWire(MySWI_Driver_t* driver);
static void MySWI_wState_readBit_releaseWire(MySWI_Driver_t* driver);
static void MySWI_wState_readBit_samplingWire(MySWI_Driver_t* driver);
static void MySWI_wState_writeBit_driveWire(MySWI_Driver_t* driver);
static void MySWI_wState_writeBit_releaseWire(MySWI_Driver_t* driver);
//Fo allapotgephez tartozo allpotok fuggvenyei
static void MySWI_mState_checkDiscoveryBit(MySWI_Driver_t* driver);
static void MySWI_startCondition(MySWI_Driver_t* driver);
static void MySWI_mState_sendByte(MySWI_Driver_t* driver);
static void MySWI_mState_byteSent(MySWI_Driver_t* driver);
static void MySWI_readByte(MySWI_Driver_t* driver);
static void MySWI_mState_byteReaded(MySWI_Driver_t* driver);
static void MySWI_mState_byteSent(MySWI_Driver_t* driver);
static void MySWI_stopCondition(MySWI_Driver_t* driver);
static void MySWI_mState_end(MySWI_Driver_t* driver);
static void MySWI_mState_osSignallingd(MySWI_Driver_t* driver);
//------------------------------------------------------------------------------
//Single-Wire (SWI) driver kezdeti inicializalasa
void MySWI_driverInit(MySWI_Driver_t* driver,
                           const MySWI_DriverConfig_t* cfg)
{
    //Driver valtozoinak kezdeti torlese.
    memset(driver, 0, sizeof(MySWI_Driver_t));

    //Idozitesi parameterek atvetele
    memcpy(&driver->times, &cfg->times, sizeof(MySWI_Times_t));

    //TC modul inicializalasa
    MySWI_initTC(driver, cfg);

    driver->tcIrqPriority_timing=cfg->tcIrqPriority_timing;
    driver->tcIrqPriority_osSignalling=cfg->tcIrqPriority_osSignalling;

    //Elereseket szabalyozo mutex letrehozasa
    #ifdef USE_FREERTOS
    driver->mutex=xSemaphoreCreateMutex();
    ASSERT(driver->mutex);

    //folyamat veget jelzo szemafor, mellyel a birtoklo taszk varakozik
    driver->doneSemaphore=xSemaphoreCreateBinary();
    ASSERT(driver->doneSemaphore);
    #endif //USE_FREERTOS
}
//------------------------------------------------------------------------------
//Wire letrehozasa, a megadott konfiguracio alapjan.
//A fuggveny hivasat meg kell, hogy elozze a MySWI_driverInit() !
void MySWI_createWire(MySWI_Wire_t* wire,
                           MySWI_Driver_t* driver,
                           const MySWI_WireConfig_t* cfg)
{
    //Wire leiro kezdeti torlese.
    memset(wire, 0, sizeof(MySWI_Wire_t));

    //konfiguracio atvetele
    memcpy(&wire->cfg, cfg, sizeof(MySWI_WireConfig_t));
    //Megjegyezzuk, hogy a wire-hoz melyik driver tartozik.
    wire->driver=(struct MySWI_Driver_t*) driver;
}
//------------------------------------------------------------------------------
//TC beallitasa a konfiguracio szerint
static void MySWI_initTC(MySWI_Driver_t* driver,
                                const MySWI_DriverConfig_t* cfg)
{
    //A driverhez rendelt TC modul inicializalasa
    MyTC_init(&driver->tc, &cfg->tcConfig);

    //A TC-t 16 bites modban hasznaljuk, igy nem kell foglalkozni majd az
    //eloosztas belekalkulalasaval az idozitesi konstansokba. Az elooszatas
    //mardhat 1. A 16 bit 100MHz es orajel eseten is elegendo az idozitesekhez.
    //A TC modult basic timer modban hasznaljuk ki, es one shot modban kezeljuk.
    //CC0 regiszterben kell megadni az idozitest.

    TcCount16* hw=&driver->tc.hw->COUNT16;

    //TC reset
    hw->CTRLA.reg = TC_CTRLA_SWRST;
    while(hw->SYNCBUSY.reg);


    hw->CTRLA.reg =
        //16 bites mod kijelolese.
        TC_CTRLA_MODE_COUNT16 |
        //1-es eloosztas kivalasztasa
        TC_CTRLA_PRESCALER_DIV1 |
        //A timer a GCLK val szinkronban fog ujra toltodni/resetelodni.
        //Avval, hogy ezt a modot jeloljuk ki, minimalizaljuk a latency-t.
        TC_CTRLA_PRESCSYNC_RESYNC |
        0;

    //debug modban is futhat a timer.
    hw->DBGCTRL.bit.DBGRUN=1;

    //Wave regiszter bellitasa
    hw->WAVE.reg = TC_WAVE_WAVEGEN(TC_WAVE_WAVEGEN_MFRQ_Val);
    //Szamlalo nullazsasa
    hw->COUNT.reg=0;
    //Kezdetben a TOP ertek a maximum, amig tud szamolni.
    hw->CC[0].reg=0;
    hw->CC[1].reg=0;

    //One-shot mode fixen beallitva a timerhez.
    //Az one-shot akkor valtodik ki, amikor a timer eleri a CC0-ban beallitott
    //erteket, majd visszaall 0-ra. Indulaskor ezert mindenkepen keletkezik egy
    //megszakitas, de utana a timer leall, es varja a retrigger jelet.
    hw->CTRLBSET.reg=TC_CTRLBCLR_ONESHOT;
    while(hw->SYNCBUSY.reg);

    //TC engedelyezese. Utana mar bizonyos regiszterek nem irhatok!
    hw->CTRLA.bit.ENABLE=1;
    while(hw->SYNCBUSY.reg);

    //Overflow interrupt engedelyezese. Ez akkor kovetkezik be, amikor a
    //COUNTER regiszter elerte a CC0-ban beallitott erteket.
    hw->INTFLAG.reg=0xff;    
    hw->INTENSET.reg=TC_INTENSET_OVF;
    //NVIC-ben IRQ engedelyezese
    MyTC_enableIrq(&driver->tc);

}
//------------------------------------------------------------------------------
static inline void MySWI_startTimer(MySWI_Driver_t* driver,
                                         uint16_t ccValue)
{
    //TC CC0 regiszterenek segitsegevel idozites beallitasa.
    driver->tc.hw->COUNT16.CC[0].reg=ccValue;
    //Timer inditasa
    driver->tc.hw->COUNT16.CTRLBSET.reg=TC_CTRLBCLR_CMD_RETRIGGER;
}
//------------------------------------------------------------------------------
//A driverhez  rendelt TC-hez tartozo interrupt service rutin
//[INTERRUPTBAN FUT!]
void MySWI_service(MySWI_Driver_t* driver)
{
  #ifdef MySWI_IRQ_TRACE
    MyGPIO_set(MySWI_IRQ_TRACE_PIN);
  #endif

    //megszkitasi flag torlese
    driver->tc.hw->COUNT16.INTFLAG.reg = TC_INTFLAG_OVF;

    //A kijelolt akcio/allapot kiszolgalo fuggvenyere ugrik
    driver->stateFunc((struct MySWI_Driver_t*) driver);

  #ifdef MySWI_IRQ_TRACE
    MyGPIO_clr(MySWI_IRQ_TRACE_PIN);
  #endif
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//                      Alacsony, wire szintu allapotok
//------------------------------------------------------------------------------
// Reset feltetel generalasa a buszon.
//Were LOW --> 500us ->Wire release --> 200 us --> Discovery (bit read)
static void MySWI_wState_reset_driveWire(MySWI_Driver_t* driver)
{
    driver->wireConfig.driveWireFunc();
    MySWI_startTimer(driver, driver->times.reset_lowTime);
    driver->stateFunc=
            (MySWI_stateFunc_t*) MySWI_wState_reset_releaseWire;
}
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MySWI_wState_reset_releaseWire(MySWI_Driver_t* driver)
{
    driver->wireConfig.releaseWireFunc();
    MySWI_startTimer(driver, driver->times.reset_highTime);
    //egyetlen bit olvasasa kovetkezik, mely a discovery.
    driver->bitCnt=0;
    driver->stateFunc=
            (MySWI_stateFunc_t*) MySWI_wState_readBit_driveWire;
}
//------------------------------------------------------------------------------
//Az olvasas idozitesei kritikusak. 1us kozeli idozitesekkel operalunk.
//Az elemi lepesekhez egyedi callbackek tartoznak, melyeket azonnal hivunk az
//interrupt service rutinbol.
//Wire LOW / Wire release --> 1us --> sampling  --> 20us  --> End
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MySWI_wState_readBit_driveWire(MySWI_Driver_t* driver)
{
    //48MHz es orajel eseten elegendo volt ebben a fuggvenyben egy rovid
    //impulzus, igy elegendo a samplinget idoziteni.
    //Korabban volt hozza egy a vonal felhuzasat idozito allapot is.
    //Ha nagyobb sebessegen menen a proci, akkor lehet, hogy kell ide par nop
    //a releaseWireFunc() ele.

    //Vonalat foldre huzzuk.
    driver->wireConfig.driveWireFunc();

    //////
    MySWI_startTimer(driver, driver->times.readBit_lowTime);
    driver->stateFunc= (MySWI_stateFunc_t*) MySWI_wState_readBit_releaseWire;

    //////

    /*
    MySWI_startTimer(driver, driver->times.readBit_samplingTime);
    driver->stateFunc=
              (MySWI_stateFunc_t*) MySWI_wState_readBit_samplingWire;
    //__asm volatile("nop");
    //__asm volatile("nop");
    //__asm volatile("nop");
    //__asm volatile("nop");
    driver->wireConfig.releaseWireFunc();
    */
}
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MySWI_wState_readBit_releaseWire(MySWI_Driver_t* driver)
{
    //Vonalat elengedi
    driver->wireConfig.releaseWireFunc();
    //Idozitjuk az elengedeshez kepest a mintavetelt. (hagyunk idot a busznak,
    //hogy az biztonsaggal felfusson.
    MySWI_startTimer(driver, driver->times.readBit_samplingTime);
    driver->stateFunc= (MySWI_stateFunc_t*) MySWI_wState_readBit_samplingWire;
}
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MySWI_wState_readBit_samplingWire(MySWI_Driver_t* driver)
{
    //Also bitre beulteti a vonal aktualis allapotanak megfelelo bitet.
    driver->rxShiftReg |= driver->wireConfig.readWireFunc();
    //timer indul.
    MySWI_startTimer(driver, driver->times.readBit_highTime);

    if (driver->bitCnt==0)
    {   //ez egy ACK/NACK bit olvasasa volt, mivel a cnt csak akkor lehet
        //itt 0. Folyamat befejezve.
        driver->stateFunc= driver->returnFunc;
        return;
    }

    //Bitek szamlalasa...
    if (--driver->bitCnt)
    {   //Van meg hatra bit, amit be kell olvasni. Tartalom tolasa balra, igy
        //helyet keszitve a kovetkezo bitnek.
        driver->rxShiftReg <<= 1;
        //uj bit olvasasaval folytatodik a folyamat.
        driver->stateFunc=
                (MySWI_stateFunc_t*) MySWI_wState_readBit_driveWire;
    } else
    {   //Minden eloirt bit be lett olvasva. ACK/NACK kuldese...

        //bitCnt=0 (a tx most csak 1 bitet ad ki, es befejezi a mukodest)

        if (driver->lastByte)
        {   //Utolso byte jelezve. NACK-t kell kuldeni a slave fele.
            //Kikuldot bit = 1 --> NACK
            driver->txShiftReg = 0x80;
        } else
        {   //Lesz meg hatra byte. ACK-t kell kuldeni.
            //Kikuldot bit = 0 --> ACK
            driver->txShiftReg = 0x00;
        }

        //ACK bit kiirasaval folytatodik...
        driver->stateFunc=
                (MySWI_stateFunc_t*) MySWI_wState_writeBit_driveWire;
    }
}
//------------------------------------------------------------------------------
static void MySWI_wState_writeBit_driveWire(MySWI_Driver_t* driver)
{
  #if MySWI_TX_BIT1_LOW_TIMED
    //Ha nincs optimalizacio, akkor elobb beallitjuk a kovetkezo state, hogy
    //evvel se menjen az ido.
    driver->stateFunc=
            (MySWI_stateFunc_t*) MySWI_wState_writeBit_releaseWire;
  #endif

    //Vonal alacsonyba huzasa
    driver->wireConfig.driveWireFunc();

    //MSB megy elol...
    if (driver->txShiftReg & 0x80)
    {   //bit=1

      #if MySWI_TX_BIT1_LOW_TIMED
        MySWI_startTimer(driver, driver->times.writeBit_1_lowTime);
      #else
        //__asm volatile("nop");
        //__asm volatile("nop");
        //__asm volatile("nop");
        //__asm volatile("nop");
        //(A state beallitas egy kis idot huz, igy talan nem kell nop.)
        driver->stateFunc=
            (MySWI_stateFunc_t*) MySWI_wState_writeBit_releaseWire;
        //Vonal elengedese
        MySWI_wState_writeBit_releaseWire(driver);
      #endif

    } else
    {   //bit=0. (Ez minden esetben idozitve van.)
        MySWI_startTimer(driver, driver->times.writeBit_0_lowTime);              

      #if !MySWI_TX_BIT1_LOW_TIMED
        driver->stateFunc=
            (MySWI_stateFunc_t*) MySWI_wState_writeBit_releaseWire;
      #endif
    }

}
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MySWI_wState_writeBit_releaseWire(MySWI_Driver_t* driver)
{
    //Vonal elengedese
    driver->wireConfig.releaseWireFunc();

    //MSB meg elol...
    if (driver->txShiftReg & 0x80)
    {   //bit=1
        MySWI_startTimer(driver, driver->times.writeBit_1_highTime);
    } else
    {   //bit=0
        MySWI_startTimer(driver,  driver->times.writeBit_0_highTime);
    }

    if (driver->bitCnt==0)
    {   //ez egy ACK/NACK bit kibocsatasa volt, mivel a cnt csak akkor lehet
        //itt 0. Folyamat befejezve.
        driver->stateFunc=driver->returnFunc;
        return;
    }

    //hatralevo kiirando bit szamlalas..
    if (--driver->bitCnt)
    {   //van meg hatra kuldendo bit.
        //kovetkezo bit kerul az MSB helyere, mely a kovetkezo IT-nel indul
        driver->txShiftReg <<= 1;
        //Kovetkezo kuldendo bittel folytatodik a folyamat.
        driver->stateFunc=
                (MySWI_stateFunc_t*) MySWI_wState_writeBit_driveWire;
    } else
    {   //minden bit ki lett irva.
        //A protokol szerint itt az ACK bit kovetkezik, melyet az eszkoztol
        //be kell olvasni. Ezt most megtesszuk. A bit olvasasa zarja a byte
        //kiirast, es a vezerles vissza jut a fo allapotgepre.
        //A beolvasott ACK majd az rxShiftReg 0. bitjen lesz elerheto.

        //bitCnt marad 0-an, igy az olvaso rutin tudni fogja, hogy csak egy
        //bitet kell olvasni.

        driver->stateFunc=
                (MySWI_stateFunc_t*) MySWI_wState_readBit_driveWire;
    }
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//                      Magas szintu allapotgep
//------------------------------------------------------------------------------
//Reset feltetel kezdemenyezese, majd discovery. (Ez a ketto ossze van
//vonva. az alacsonyszintu Wire allapotokban.)
static void MySWI_mState_resetAndDiscovery(MySWI_Driver_t* driver)
{
    //A reset es felderites majd a felderiteskor kapott discovery bit
    //ellenorzesevel kell, hogy folytatodjon.
    driver->returnFunc=
            (MySWI_stateFunc_t*)MySWI_mState_checkDiscoveryBit;

    MySWI_wState_reset_driveWire(driver);
}
//------------------------------------------------------------------------------
//Discovery folyaman kapott visszajelzes bit kiertekelese.
static void MySWI_mState_checkDiscoveryBit(MySWI_Driver_t* driver)
{
    if (driver->rxShiftReg & 1)
    {   //Nem kapott a Discovery folymatban fisszajelzest.
        //Nem folytatja tovabb a muveleteket
        driver->asyncStatus=kMySWI_Status_discoveryNACK;
        //Stop feltetel es vege.
        MySWI_stopCondition(driver);
        return;
    }
    //Visszajelzes ok. Van eszkoz a buszon.

    //Start feltetelell indulhat a keret...
    MySWI_startCondition(driver);
}
//------------------------------------------------------------------------------
//Buszon start feltetel generalasa. Ez gyakorlatilag csak magasszinten tartast
//jelent. A fuggvenyt akkor is hivjuk, ha restrat kell olvasas miatt.
//A start utan mindig irassal folytatodik az allapotgep.
static void MySWI_startCondition(MySWI_Driver_t* driver)
{
    MySWI_startTimer(driver, driver->times.startStopTime);

    //A star utan a cimek kiirasa fog kovetkezni.
    driver->stateFunc=(MySWI_stateFunc_t*)MySWI_mState_sendByte;
}
//------------------------------------------------------------------------------
//A soron kovetkezo byte kiirasanak kezdemenyezese.
static void MySWI_mState_sendByte(MySWI_Driver_t* driver)
{
    //Soron kovetkezo byte beallitasa a shift registrebe.
    driver->txShiftReg = *driver->txPtr++;

    //Hatralevo elemszam csokken.
    driver->txCnt--;

    //A byte kiiras vegen a vezerles az ACK kiertekelesen fog folytatodni
    driver->returnFunc=(MySWI_stateFunc_t*)MySWI_mState_byteSent;

    driver->bitCnt=8;
    MySWI_wState_writeBit_driveWire(driver);
}
//------------------------------------------------------------------------------
//Byteok kiirasa utan jon fel, es ebben ellenorizzuk a kiirasra kapott ACK
//bit allapotat, illetve ebben kerul inditasra a kovetkezo byteok irasa vagy
//olvasasa.
static void MySWI_mState_byteSent(MySWI_Driver_t* driver)
{
    //ACK ellenorzese
    if (driver->rxShiftReg & 1)
    {   //Nem akpott ACK-t.
        //Nem folytatja tovabb a muveleteket
        driver->asyncStatus=kMySWI_Status_NACK;
        //Stop feltetel es vege.
        MySWI_stopCondition(driver);
        return;
    }

    //Van meg hatra kiirando byte?
    if (driver->txCnt)
    {   //Van meg mit kiirni

        //Uj byte kiirasanak kezdemenyezese, melyet a txPtr mutat.
        MySWI_mState_sendByte(driver);
    } else
    {   //Minden byte ki lett irva

        //Ellenorizzuk, hogy van e masodik koros adathalmaz, amit ki kell irni.
        if (driver->txCnt2)
        {   //Van meg masodik korben kiirando adathalmaz. Arra aktualizalunk.
            driver->txPtr=driver->txPtr2;
            driver->txCnt=driver->txCnt2;
            driver->txCnt2=0;

            //Van eloirva restart feltetel? (Ez csak a masodik blokk kiirasa
            //eseten fordulhat elo.)
            if (driver->insertRestart)
            {   //szukseges a restart.
                MySWI_startCondition(driver);
                //A start kondicio utan mindenkepen a kuldo allapot fog hivodni!
            } else
            {   //nincs restart. Mehet azonnal a masodik adathalmaz kuldese.
                MySWI_mState_sendByte(driver);
            }
        } else
        {   //minden kiirando adattal vegzett. Kell olvasni?
            if (driver->rxCnt)
            {   //van olvasni valo. Elso byte olvasasanak kezdemenyezese.
                MySWI_readByte(driver);
                //Ide ne keruljon utasitas, mivel ido kritikus a folytatas!
                return;
            } else
            {   //vegzett az atvitellel. Stopfeltetellel lezarjuk a mukodest.
                MySWI_stopCondition(driver);
                return;
            }
        }
    }
}
//------------------------------------------------------------------------------
//Byte olvasas kezdemenyezese.
static void MySWI_readByte(MySWI_Driver_t* driver)
{
    //Az utolso byte olvasasa eseten NACK-t kell majd kuldeni. Ezt jelezzuk az
    //alacsony szintu allapotgepeknek.
    if (driver->rxCnt==1) driver->lastByte=1;

    driver->returnFunc=(MySWI_stateFunc_t*)MySWI_mState_byteReaded;

    driver->bitCnt=8;
    MySWI_wState_readBit_driveWire(driver);
    //Ide ne keruljon utasitas, mivel ido kritikus a folytatas!
}
//------------------------------------------------------------------------------
//Byte olvasas vegen feljovo allapot. Ebben taroljuk le a fogadott byteot, es
//ha van meg hatra olvasni valo, akkor kezdemenyezzuk a kovetkezo olvasasat.
static void MySWI_mState_byteReaded(MySWI_Driver_t* driver)
{
    //Beolvasott byte az rxShiftReg-ben talalhato. Letaroljuk...
    *driver->rxPtr++ = driver->rxShiftReg;

    //Hatralevo byteok szamanak csokkentese. Van meg hatra olvasni valo?
    if (--driver->rxCnt)
    {   //meg nem vegeztunk az olvasassal. Uj byte olvasas kezdemenyezese
        MySWI_readByte(driver);
        //Ide ne keruljon utasitas, mivel ido kritikus a folytatas!
        return;
    } else
    {   //nincs tobb olvasni valo. Folyamatot lezarjuk.
        MySWI_stopCondition(driver);
    }
}
//------------------------------------------------------------------------------
static void MySWI_stopCondition(MySWI_Driver_t* driver)
{
    MySWI_startTimer(driver, driver->times.startStopTime);

    //A stop utan lezarjuk a folyamatokat.
    driver->stateFunc=(MySWI_stateFunc_t*)MySWI_mState_end;
}
//------------------------------------------------------------------------------
static void MySWI_mState_end(MySWI_Driver_t* driver)
{
    //A lezaro allapotban majd az OS-nek jelzunk
    driver->stateFunc=(MySWI_stateFunc_t*)MySWI_mState_osSignallingd;

    //A TC IRQ alacsony(abb) prioritasra kerul, olyanra, melyen a freeRTOS API
    //hivasok hasznalhatok.
    uint16_t irqn=driver->tc.info->irqn;
    NVIC_SetPriority(irqn, driver->tcIrqPriority_osSignalling);

    //Ujabb, de mar alacsonyabb prioritason futo IRQ generalasa.
    NVIC_SetPendingIRQ(irqn);

    return;

}
//------------------------------------------------------------------------------
static void MySWI_mState_osSignallingd(MySWI_Driver_t* driver)
{
    //Jelzes a varakozo taszknak, hogy befejezodtek a muveletek a megszakitasban
    #ifdef USE_FREERTOS
    xSemaphoreGiveFromISR(driver->doneSemaphore, NULL);
    #endif //USE_FREERTOS
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//                              API
//------------------------------------------------------------------------------
//Single-Wire (SWI) tranzakcio vegzese.
//A fuggveny blokkolja a hivo oldalt, mig a tranzakcioval nem vegez.
status_t MySWI_transaction(MySWI_Wire_t* wire,
                                const MySWI_transaction_t* transaction)
{
    status_t status;
    MySWI_Driver_t* driver=(MySWI_Driver_t*) wire->driver;

    //Mivel az egyes wire-ok azonos eroforrasokat hasznalnak, ezert szukseges
    //azok egyidoben torteno eleresenek a kizarasa. --> kell a mutex.
    #ifdef USE_FREERTOS
    xSemaphoreTake(driver->mutex, portMAX_DELAY);
    #endif //USE_FREERTOS

    //driver szamara beallitjuk azt a wire-t, amit kezelnie kell
    //driver->wire=wire;
    //wire konfiguracio lemasolasa.
    memcpy(&driver->wireConfig, &wire->cfg, sizeof(MySWI_WireConfig_t));
    driver->asyncStatus=kStatus_Success;

    //Tranzakcios parameterek atvetele...
    driver->txPtr=transaction->tx1Buff;
    driver->txCnt=transaction->tx1Length;
    driver->insertRestart=transaction->insertRestart;
    driver->txPtr2=transaction->tx2Buff;
    driver->txCnt2=transaction->tx2Length;
    driver->rxPtr=transaction->rxBuff;
    driver->rxCnt=transaction->rxLength;

    driver->lastByte=0;
    driver->rxShiftReg=0;
    //A mukodes a fo allapotgepben indul a resetelessel.
    driver->stateFunc =
                (MySWI_stateFunc_t*)MySWI_mState_resetAndDiscovery;
    //A TC IRQ magas prioritasra kerul felprogramozasra. Az OS nem szakithatja
    //meg.
    NVIC_SetPriority(driver->tc.info->irqn, driver->tcIrqPriority_timing);

    //TimerIT kivaltasa. Folytatas a megszakitasban.
    MySWI_startTimer(driver, 1);

    //varakozas arra, hogy a folyamat befejezodjon...
    #ifdef USE_FREERTOS
    xSemaphoreTake(driver->doneSemaphore, portMAX_DELAY);
    #endif //USE_FREERTOS
    //Megszakitasban keletkezett statuszkoddal terunk vissza.
    status=driver->asyncStatus;

    #ifdef USE_FREERTOS
    xSemaphoreGive(driver->mutex);
    #endif //USE_FREERTOS

    return  status;
}
//------------------------------------------------------------------------------
