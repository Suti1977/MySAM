//------------------------------------------------------------------------------
//  Impulzus szelesseg modulalt jel (PWM) eloallitas
//
//    File: MyPWM.h
//------------------------------------------------------------------------------
#ifndef MY_PWM_H_
#define MY_PWM_H_

#include "MyTC.h"

//------------------------------------------------------------------------------
//MyPWM driver inicializacios konfiguracios parameterei
typedef struct
{
    //A generatorkent hasznalt TC modul beallitasai
    MyTC_Config_t    tcConfig;

    //Eloosztas beallitasa
    //    0 - TC_CTRLA_PRESCALER_DIV1_Val
    //    1 - TC_CTRLA_PRESCALER_DIV2_Val
    //    2 - TC_CTRLA_PRESCALER_DIV4_Val
    //    3 - TC_CTRLA_PRESCALER_DIV8_Val
    //    4 - TC_CTRLA_PRESCALER_DIV16_Val
    //    5 - TC_CTRLA_PRESCALER_DIV64_Val
    //    6 - TC_CTRLA_PRESCALER_DIV256_Val
    //    7 - TC_CTRLA_PRESCALER_DIV1024_Val
    uint8_t prescaler;

    //Periodus regiszter erteke. A kitoltesi tenyezo maximum ennyi lehet.
    //8 bites esetben ez max 255.
    uint8_t period;

} MyPWM_Config_t;
//------------------------------------------------------------------------------
//MyPWM valtozoi
typedef struct
{
    //A generatorkent hasznalt TC modul alap driver valtozoi
    MyTC_t   tc;
} MyPWM_t;
//------------------------------------------------------------------------------
//PWM driver kezdeti inicializalasa
void MyPWM_init(MyPWM_t* pwm, const MyPWM_Config_t* cfg);
//Alapertelmezett konfiguracioval atadott struktura feltoltese
void MyPWM_getDefaultConfig(MyPWM_Config_t* cfg);

//Kitoltesi tenyezo allitasa
void MyPWM_setDuty(MyPWM_t* pwm, uint8_t channel, uint8_t duty);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_PWM_H_
