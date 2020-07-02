//------------------------------------------------------------------------------
//  Teszteleshez Sorosportos/RTT konzolok kezelese
//------------------------------------------------------------------------------
#ifndef CONSOLE_H
#define CONSOLE_H

#include "SerialConsole.h"
#include "RTTConsole.h"

//------------------------------------------------------------------------------
//Sorosportos es RTT konzolok kezdeti intje
void Console_init(void);

//stdio kimenet beallitasa valamelyik konzolra
void Console_setStdioDestination(MyConsole_t* destConsole);
//------------------------------------------------------------------------------
#endif // CONSOLE_H

