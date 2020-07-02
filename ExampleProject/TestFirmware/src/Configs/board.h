//------------------------------------------------------------------------------
//Mikrovezerlohoz tartozo hardveres definiciok (portok, pinek, ...
//------------------------------------------------------------------------------
#ifndef BOARD_H
#define BOARD_H


#include "MyGPIO.h"

//------------------------------------------------------------------------------
//Status LED1
//[PAD: 72]     DIGITAL OUT
#define PIN_STATUS_LED1             PIN_PC18
#define PIN_STATUS_LED1_CONFIG      PINMUX_PC18_GPIO | PINOP_OUTPUT | PINOP_STRONG | PINOP_INIT0

//soros konzol TX vonala
//[PAD: ]     DIGITAL OUT
#define PIN_CONSOLE_SERCOM_TX           PIN_PB25
#define PIN_CONSOLE_SERCOM_TX_CONFIG    PINMUX_PB25D_SERCOM2_PAD0
//soros konzol RX vonala
//[PAD: ]     DIGITAL IN
#define PIN_CONSOLE_SERCOM_RX           PIN_PB24
#define PIN_CONSOLE_SERCOM_RX_CONFIG    PINMUX_PB24D_SERCOM2_PAD1
//A hasznalt sercom eme PAD-je van bekotve az RX-nek
#define CONSOLE_SERCOM_RX_PAD           1

#endif //BOARD_H
