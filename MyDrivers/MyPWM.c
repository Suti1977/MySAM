//------------------------------------------------------------------------------
//  Impulzus szelesseg modulalt jel (PWM) eloallitas
//
//    File: MyPWM.c
//------------------------------------------------------------------------------
#include "MyPWM.h"
#include <string.h>


static void MyPWM_initHardware(MyPWM_t* pwm, const MyPWM_Config_t* cfg);
//------------------------------------------------------------------------------
//PWM driver kezdeti inicializalasa
void MyPWM_init(MyPWM_t* pwm, const MyPWM_Config_t* cfg)
{    
    //Modul valtozoinak kezdeti torlese.
    memset(pwm, 0, sizeof(MyPWM_t));

    //Alap TC modul inicializalasa
    MyTC_init(&pwm->tc, &cfg->tcConfig);

    MyPWM_initHardware(pwm, cfg);
}
//------------------------------------------------------------------------------
//Alapertelmezett konfiguracioval atadott struktura feltoltese
void MyPWM_getDefaultConfig(MyPWM_Config_t* cfg)
{
    memset(cfg, 0, sizeof(MyPWM_Config_t));
    cfg->period=255;
}
//------------------------------------------------------------------------------
static void MyPWM_initHardware(MyPWM_t* pwm, const MyPWM_Config_t* cfg)
{
    volatile TcCount8* hw=&pwm->tc.hw->COUNT8;

    //TC modul resetelese
    hw->CTRLA.reg=TC_CTRLA_SWRST;
    while(hw->SYNCBUSY.reg);

    //konfiguralas...
    hw->CTRLA.reg |=
            //Uzemmod kijelolese a TC periferian (8 bites uzemmodot hasznaluk)
            TC_CTRLA_MODE(TC_CTRLA_MODE_COUNT8_Val) |
            //ha at lenne irva az idozites, akkor az a GCLK-ra tortenjen azonnal
            TC_CTRLA_PRESCSYNC(TC_CTRLA_PRESCSYNC_GCLK_Val) |
            //Eloosztas beallitasa (nincs)
            TC_CTRLA_PRESCALER((uint32_t)cfg->prescaler) |
            0;

    //PWM uzemmod beallitasa...
    //NPWM uzemmod -->A PER regiszterbe kerul a kitoltes
    hw->WAVE.reg=TC_WAVE_WAVEGEN(TC_WAVE_WAVEGEN_NPWM_Val);

    //PER regiszterben kell megadni a szamlalo TOP erteket. Mi kihasznaljuk a
    //teljes szamlalo tartomanyt.
    hw->PER.reg=cfg->period;

    //A modul mukodhet debug modban is. (igy ha megallitjuk, akkor adja ki az
    //impulzusokat. (nem marad kint esetleg 100%-on valami vezerles.)
    hw->DBGCTRL.reg=TC_DBGCTRL_DBGRUN;

    while(hw->SYNCBUSY.reg);

    //TC engedelyezese... -->PWM elindul
    hw->CTRLA.bit.ENABLE=1;
    while(hw->SYNCBUSY.reg);
}
//------------------------------------------------------------------------------
//Kitoltesi tenyezo allitasa
void MyPWM_setDuty(MyPWM_t* pwm, uint8_t channel, uint8_t duty)
{
    pwm->tc.hw->COUNT8.CC[channel].reg=duty;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
