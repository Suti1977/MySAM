//------------------------------------------------------------------------------
//  One-Wire (OWI) driver
//  A rendszerben tobb Wire is lehet. Az eroforrasok optimalizalasa erdekeben a
//  Wire-okhoz egyetlen kozos kezelo modul tartozik, kozos hardver peri-
//  feriakkal. Az egyes Wireo-ok kezelese viszont egy idoben nem lehetseges.
//  Ezt a kizarast mutexelessel oldjuk meg.
//
//    File: MyOWI.c
//------------------------------------------------------------------------------
#include "MyOWI.h"
#include <string.h>


//Ha ez benne van, akkor az egyik GPIO vonalat huzgod a megsazkitas be es
//kilepesenel. A driver teszteleshez hasznalhatjuk.
#if 0
#define MyOWI_IRQ_TRACE
#include "MyGPIO.h"
#defien MyOWI_IRQ_TRACE_PIN PIN_DEBUG_TP2
#endif

//Ha ez 1, akkor a bit kiirasnal timerhez koti az '1'-es bitek kuldesenel az
//alacsony szint (a busz hajtasanak) idejet.
//Ha 0, akkor kevesebb megszakitas keletkezik. Az alacsonyszintu impulzus idejet
//a processzor sebesseg befolyasolja. Viszont kevesebb nagyon suru, 1us-enkent
//feljovo mesgazkitas kell.
#ifndef MyOWI_TX_BIT1_LOW_TIMED
#define MyOWI_TX_BIT1_LOW_TIMED    1
#endif


static void MyOWI_initTC(MyOWI_Driver_t* driver,
                                const MyOWI_DriverConfig_t* cfg);
//Wire szintu allapotok
static void MyOWI_wState_reset_driveWire(MyOWI_Driver_t* driver);
static void MyOWI_wState_reset_releaseWire(MyOWI_Driver_t* driver);
static void MyOWI_wState_presence_samplingWire(MyOWI_Driver_t* driver);
static void MyOWI_wState_readBit_driveWire(MyOWI_Driver_t* driver);
static void MyOWI_wState_readBit_samplingWire(MyOWI_Driver_t* driver);
static void MyOWI_wState_writeBit_driveWire(MyOWI_Driver_t* driver);
static void MyOWI_wState_writeBit_releaseWire(MyOWI_Driver_t* driver);

//Fo allapotgephez tartozo allpotok fuggvenyei
static void MyOWI_mState_sendByte(MyOWI_Driver_t* driver);
static void MyOWI_mState_byteSent(MyOWI_Driver_t* driver);
static void MyOWI_readByte(MyOWI_Driver_t* driver);
static void MyOWI_mState_byteReaded(MyOWI_Driver_t* driver);
static void MyOWI_mState_byteSent(MyOWI_Driver_t* driver);
static void MyOWI_mState_end(MyOWI_Driver_t* driver);
//------------------------------------------------------------------------------
//Single-Wire (OWI) driver kezdeti inicializalasa
void MyOWI_driverInit(MyOWI_Driver_t* driver,
                           const MyOWI_DriverConfig_t* cfg)
{
    //Driver valtozoinak kezdeti torlese.
    memset(driver, 0, sizeof(MyOWI_Driver_t));

    //Idozitesi parameterek atvetele
    memcpy(&driver->times, &cfg->times, sizeof(MyOWI_Times_t));

    //TC modul inicializalasa
    MyOWI_initTC(driver, cfg);

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
//A fuggveny hivasat meg kell, hoyg elozze a MyOWI_driverInit() !
void MyOWI_createWire(MyOWI_Wire_t* wire,
                           MyOWI_Driver_t* driver,
                           const MyOWI_WireConfig_t* cfg)
{
    //Wire leiro kezdeti torlese.
    memset(wire, 0, sizeof(MyOWI_Wire_t));

    //konfiguracio atvetele
    memcpy(&wire->cfg, cfg, sizeof(MyOWI_WireConfig_t));
    //Megjegyezzuk, hogy a wire-hoz melyik driver tartozik.
    wire->driver=(struct MyOWI_Driver_t*) driver;
}
//------------------------------------------------------------------------------
//TC beallitasa a konfiguracio szerint
static void MyOWI_initTC(MyOWI_Driver_t* driver,
                                const MyOWI_DriverConfig_t* cfg)
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
static inline void MyOWI_startTimer(MyOWI_Driver_t* driver,
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
void MyOWI_service(MyOWI_Driver_t* driver)
{
  #ifdef MyOWI_IRQ_TRACE
    MyGPIO_set(MyOWI_IRQ_TRACE_PIN);
  #endif

    //megszkitasi flag torlese
    driver->tc.hw->COUNT16.INTFLAG.reg = TC_INTFLAG_OVF;

    //A kijelolt akcio/allapot kiszolgalo fuggvenyere ugrik
    driver->stateFunc((struct MyOWI_Driver_t*) driver);

  #ifdef MyOWI_IRQ_TRACE
    MyGPIO_clr(MyOWI_IRQ_TRACE_PIN);
  #endif
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//                      Alacsony, wire szintu allapotok
//------------------------------------------------------------------------------
//Reset feltetel generalasa a buszon, majd presence bit olvasasa
static void MyOWI_wState_reset_driveWire(MyOWI_Driver_t* driver)
{
    //Ellenorzes, hogy a busz magasban van-e...
    if (driver->wireConfig.readWireFunc()==0)
    {   //A busz alacsonyban van. ez hiba
        driver->asyncStatus=kMyOWI_Status_busError;
        MyOWI_mState_end(driver);
        return;
    }

    driver->wireConfig.driveWireFunc();
    MyOWI_startTimer(driver, driver->times.reset_lowTime);
    driver->stateFunc=
            (MyOWI_stateFunc_t*) MyOWI_wState_reset_releaseWire;
}
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MyOWI_wState_reset_releaseWire(MyOWI_Driver_t* driver)
{
    driver->wireConfig.releaseWireFunc();
    MyOWI_startTimer(driver, driver->times.presence_samplingTime);
    driver->stateFunc=
            (MyOWI_stateFunc_t*) MyOWI_wState_presence_samplingWire;
}
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MyOWI_wState_presence_samplingWire(MyOWI_Driver_t* driver)
{
    MyOWI_startTimer(driver,
                     driver->times.reset_highTime-
                     driver->times.presence_samplingTime);

    if (driver->wireConfig.readWireFunc())
    {   //Nem kapott a reset folymatban visszajelzest.
        //Nem folytatja tovabb a muveleteket
        driver->asyncStatus=kMyOWI_Status_noPresence;
        driver->stateFunc= (MyOWI_stateFunc_t*) MyOWI_mState_end;
    } else
    {   //ok.

        //Parancs kuldesevel folytatjuk. (Mindenkepen kuldessel megy tovabb a
        //folyamat.)
        driver->stateFunc=(MyOWI_stateFunc_t*)MyOWI_mState_sendByte;
    }
}
//------------------------------------------------------------------------------
//Az olvasas idozitesei kritikusak. 1us kozeli idozitesekkel operalunk.
//Az elemi lepesekhez egyedi callbackek tartoznak, melyeket azonnal hivunk az
//interrupt service rutinbol.
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MyOWI_wState_readBit_driveWire(MyOWI_Driver_t* driver)
{
    //48MHz es orajel eseten elegendo volt ebben a fuggvenyben egy rovid
    //impulzus, igy elegendo a samplinget idoziteni.
    //Korabban volt hozza egy a vonal felhuzasat idozito allapot is.
    //Ha nagyobb sebessegen menen a proci, akkor lehet, hogy kell ide par nop
    //a releaseWireFunc() ele.

    driver->wireConfig.driveWireFunc();

    MyOWI_startTimer(driver, driver->times.readBit_samplingTime);
    driver->stateFunc=
              (MyOWI_stateFunc_t*) MyOWI_wState_readBit_samplingWire;

    __asm volatile("nop");
    __asm volatile("nop");
    __asm volatile("nop");
    __asm volatile("nop");

    driver->wireConfig.releaseWireFunc();
}
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MyOWI_wState_readBit_samplingWire(MyOWI_Driver_t* driver)
{
    //Felso bitre beulteti a vonal aktualis allapotanak megfelelo bitet.
    driver->rxShiftReg |= driver->wireConfig.readWireFunc() ? 0x80 : 0;

    //timer indul.
    MyOWI_startTimer(driver, driver->times.readBit_highTime);

    //Bitek szamlalasa...
    if (--driver->bitCnt)
    {   //Van meg hatra bit, amit be kell olvasni. Tartalom tolasa jobbra, igy
        //helyet keszitve a kovetkezo bitnek.
        driver->rxShiftReg >>= 1;
        //uj bit olvasasaval folytatodik a folyamat.
        driver->stateFunc=
                (MyOWI_stateFunc_t*) MyOWI_wState_readBit_driveWire;
    } else
    {   //Minden eloirt bit be lett olvasva. Visszaterhet a fo allapotgephez.
        driver->stateFunc = driver->returnFunc;
    }
}
//------------------------------------------------------------------------------
static void MyOWI_wState_writeBit_driveWire(MyOWI_Driver_t* driver)
{
  #if MyOWI_TX_BIT1_LOW_TIMED
    //Ha nincs optimalizacio, akkor elobb beallitjuk a kovetkezo state, hogy
    //evvel se menjen az ido.
    driver->stateFunc=
            (MyOWI_stateFunc_t*) MyOWI_wState_writeBit_releaseWire;
  #endif

    //Vonal alacsonyba huzasa
    driver->wireConfig.driveWireFunc();

    //LSB megy elol...
    if (driver->txShiftReg & 1)
    {   //bit=1

      #if MyOWI_TX_BIT1_LOW_TIMED
        MyOWI_startTimer(driver, driver->times.writeBit_1_lowTime);
      #else
        //__asm volatile("nop");
        //__asm volatile("nop");
        //__asm volatile("nop");
        //__asm volatile("nop");
        //(A state beallitas egy kis idot huz, igy talan nem kell nop.)
        driver->stateFunc=
            (MyOWI_stateFunc_t*) MyOWI_wState_writeBit_releaseWire;
        //Vonal elengedese
        MyOWI_wState_writeBit_releaseWire(driver);
      #endif

    } else
    {   //bit=0. (Ez minden esetben idozitve van.)
        MyOWI_startTimer(driver, driver->times.writeBit_0_lowTime);              

      #if !MyOWI_TX_BIT1_LOW_TIMED
        driver->stateFunc=
            (MyOWI_stateFunc_t*) MyOWI_wState_writeBit_releaseWire;
      #endif
    }

}
//. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
static void MyOWI_wState_writeBit_releaseWire(MyOWI_Driver_t* driver)
{
    //Vonal elengedese
    driver->wireConfig.releaseWireFunc();

    //LSB megy elol...
    if (driver->txShiftReg & 1)
    {   //bit=1
        MyOWI_startTimer(driver, driver->times.writeBit_1_highTime);
    } else
    {   //bit=0
        MyOWI_startTimer(driver,  driver->times.writeBit_0_highTime);
    }

    //hatralevo kiirando bit szamlalas..
    if (--driver->bitCnt)
    {   //van meg hatra kuldendo bit.
        //kovetkezo bit kerul az LSB helyere, mely a kovetkezo IT-nel indul
        driver->txShiftReg >>= 1;

        //Kovetkezo kuldendo bittel folytatodik a folyamat.
        driver->stateFunc=
                (MyOWI_stateFunc_t*) MyOWI_wState_writeBit_driveWire;
    } else
    {   //minden bit ki lett irva.

        driver->stateFunc=driver->returnFunc;
    }
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//                      Magas szintu allapotgep
//------------------------------------------------------------------------------
//Reset feltetel kezdemenyezese, majd presence bit olvasasa. (Ez a ketto ossze
//van vonva az alacsonyszintu Wire allapotokban.)
static void MyOWI_mState_resetAndReadPresenceBit(MyOWI_Driver_t* driver)
{
    //A reset es felderites majd a felderiteskor kapott discovery bit
    //ellenorzesevel kell, hogy folytatodjon.
    //driver->returnFunc=
    //        (MyOWI_stateFunc_t*)MyOWI_mState_checkPresenceBit;

    MyOWI_wState_reset_driveWire(driver);
}
//------------------------------------------------------------------------------
//A soron kovetkezo byte kiirasanak kezdemenyezese.
static void MyOWI_mState_sendByte(MyOWI_Driver_t* driver)
{
    //Soron kovetkezo byte beallitasa a shift registrebe.
    driver->txShiftReg = *driver->txPtr++;

    //Hatralevo elemszam csokken.
    driver->txCnt--;

    //A byte kiiras vegen a vezerles az ACK kiertekelesen fog folytatodni
    driver->returnFunc=(MyOWI_stateFunc_t*)MyOWI_mState_byteSent;

    driver->bitCnt=8;
    MyOWI_wState_writeBit_driveWire(driver);
}
//------------------------------------------------------------------------------
//Byteok kiirasa utan jon fel, es ebben ellenorizzuk a kiirasra kapott ACK
//bit allapotat, illetve ebben kerul inditasra a kovetkezo byteok irasa vagy
//olvasasa.
static void MyOWI_mState_byteSent(MyOWI_Driver_t* driver)
{
    //Van meg hatra kiirando byte?
    if (driver->txCnt)
    {   //Van meg mit kiirni

        //Uj byte kiirasanak kezdemenyezese, melyet a txPtr mutat.
        MyOWI_mState_sendByte(driver);
    } else
    {   //Minden byte ki lett irva

        //Ellenorizzuk, hogy van e masodik koros adathalmaz, amit ki kell irni.
        if (driver->txCnt2)
        {   //Van meg masodik korben kiirando adathalmaz. Arra aktualizalunk.
            driver->txPtr=driver->txPtr2;
            driver->txCnt=driver->txCnt2;
            driver->txCnt2=0;

            MyOWI_mState_sendByte(driver);

        } else
        {   //minden kiirando adattal vegzett. Kell olvasni?
            if (driver->rxCnt)
            {   //van olvasni valo. Elso byte olvasasanak kezdemenyezese.
                MyOWI_readByte(driver);

                //Ide ne keruljon utasitas, mivel ido kritikus a folytatas!
                return;
            } else
            {   //vegzett az atvitellel.
                MyOWI_mState_end(driver);
                return;
            }
        }
    }
}
//------------------------------------------------------------------------------
//Byte olvasas kezdemenyezese.
static void MyOWI_readByte(MyOWI_Driver_t* driver)
{
    driver->returnFunc=(MyOWI_stateFunc_t*)MyOWI_mState_byteReaded;

    driver->bitCnt=8;
    MyOWI_wState_readBit_driveWire(driver);
    //Ide ne keruljon utasitas, mivel ido kritikus a folytatas!
}
//------------------------------------------------------------------------------
//Byte olvasas vegen feljovo allapot. Ebben taroljuk le a fogadott byteot, es
//ha van meg hatra olvasni valo, akkor kezdemenyezzuk a kovetkezo olvasasat.
static void MyOWI_mState_byteReaded(MyOWI_Driver_t* driver)
{
    //Beolvasott byte az rxShiftReg-ben talalhato. Letaroljuk...
    *driver->rxPtr++ = driver->rxShiftReg;

    //Hatralevo byteok szamanak csokekntese. Van meg hatra olvasni valo?
    if (--driver->rxCnt)
    {   //meg nem vegeztunk az olvasassal. Uj byte olvasas kezdemenyezese
        MyOWI_readByte(driver);
        //Ide ne keruljon utasitas, mivel ido kritikus a folytatas!
        return;
    } else
    {   //nincs tobb olvasni valo. Folyamatot lezarjuk.
        MyOWI_mState_end(driver);
    }
}
//------------------------------------------------------------------------------
//Folyamat vegen meghivodo allapot.
static void MyOWI_mState_end(MyOWI_Driver_t* driver)
{
    //Jelzes a varakozo taszknak, hogy befejezodtek a muveletek a megszakitasban
    #ifdef USE_FREERTOS
    xSemaphoreGiveFromISR(driver->doneSemaphore, NULL);
    #endif //USE_FREERTOS
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//                              API
//------------------------------------------------------------------------------
//Single-Wire (OWI) tranzakcio vegzese.
//A fuggveny blokkolja a hivo oldalt, mig a tranzakcioval nem vegez.
status_t MyOWI_transaction(MyOWI_Wire_t* wire,
                                const MyOWI_transaction_t* transaction)
{
    status_t status;
    MyOWI_Driver_t* driver=(MyOWI_Driver_t*) wire->driver;

    //Mivel az egyes wire-ok azonos eroforrasokat hasznalnak, ezert szukseges
    //azok egyidoben torteno eleresenek a kizarasa. --> kell a mutex.
    #ifdef USE_FREERTOS
    xSemaphoreTake(driver->mutex, portMAX_DELAY);
    #endif //USE_FREERTOS

    //driver szamara beallitjuk azt a wire-t, amit kezelnie kell
    //driver->wire=wire;
    //wire konfiguracio lemasolasa.
    memcpy(&driver->wireConfig, &wire->cfg, sizeof(MyOWI_WireConfig_t));
    driver->asyncStatus=kStatus_Success;

    //Tranzakcios parameterek atvetele...
    driver->txPtr=transaction->tx1Buff;
    driver->txCnt=transaction->tx1Length;
    driver->txPtr2=transaction->tx2Buff;
    driver->txCnt2=transaction->tx2Length;
    driver->rxPtr=transaction->rxBuff;
    driver->rxCnt=transaction->rxLength;

    driver->rxShiftReg=0;
    //A mukodes a fo allapotgepben indul a resetelessel.
    driver->stateFunc =
                (MyOWI_stateFunc_t*)MyOWI_mState_resetAndReadPresenceBit;
    //TimerIT kivaltasa. Folytatas a megszakitasban.
    MyOWI_startTimer(driver, 1);

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
