//------------------------------------------------------------------------------
//	Ebben a headerben kerulnek definialasra a hasznalt eroforrasok
//------------------------------------------------------------------------------
#include "MyCommon.h"

//A GCLK modulokhoz beallitott orajel frekvenciak
#define GCLK0_FREQ_HZ                   48000000ul
#define GCLK1_FREQ_HZ                    2000000ul
#define GCLK2_FREQ_HZ                    3000000ul
#define GCLK3_FREQ_HZ                      32768ul
//#define GCLK4_FREQ_HZ                   48000000ul
//#define GCLK5_FREQ_HZ                   48000000ul
//#define GCLK6_FREQ_HZ                   48000000ul
//#define GCLK7_FREQ_HZ                   48000000ul
//#define GCLK8_FREQ_HZ                   48000000ul
//#define GCLK9_FREQ_HZ                   48000000ul
//#define GCLK10_FREQ_HZ                  48000000ul
//#define GCLK11_FREQ_HZ                  48000000ul

//------------------------------------------------------------------------------
//Sorosportos konzolhoz rendelt sercom defineialasa
//(Demo boardon Virtual com-portra kotve)
#define CONSOLE_SERCOM_NUM                  2
#define CONSOLE_SERCOM_CORE_GCLK            0
#define CONSOLE_SERCOM_CORE_FREQ            GCLK0_FREQ_HZ
#define CONSOLE_SERCOM_SLOW_GCLK            3
#define CONSOLE_SERCOM_SLOW_FREQ            GCLK3_FREQ_HZ
//------------------------------------------------------------------------------
//Debug konzol sorosportjanak adatatviteli sebessege
#define CONSOLE_BAUDRATE      115200
//------------------------------------------------------------------------------
