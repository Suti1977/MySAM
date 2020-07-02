//------------------------------------------------------------------------------
//  Teszteleshez Sorosportos/RTT konzolok kezelese
//------------------------------------------------------------------------------
#include "Console.h"
#include <stdio.h>
#include "SEGGER_RTT.h"

//stdio kimenetet erre a konzolra iranyitja
static MyConsole_t* stdio_destinationConsole=&RTTconsole.console;
//true, ha  akonzol alrendszer inicializalva lett.
static bool consoleInited=false;

static void Console_stdio_putChar(char Ch);
//------------------------------------------------------------------------------
//Sorosportos es RTT konzolok kezdeti intje
void Console_init(void)
{   
    //Specify that stdout and stdin should not be buffered.
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    //RTT-s konzol inicializalsa
    RTTconsole_init();

    //Sorosportos konzol inicializalasa
    SerialConsole_init();

    //Jelezes, hogy inicializalva vannak a konzolok. Lehet oket hasznalni az
    //stdio szamara.
    consoleInited=true;
}
//------------------------------------------------------------------------------
//A _write() rutinban van egy trukkos pointer aritmetikai kifejezes
//melyre mindig warningot kaptunk. Ezt az alabbi 2 pragmaval elnyomjuk.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"

//stdio altal hivott iro rutin. Ezt a sorosportos konzolra iranyitjuk.
int  __attribute__((used)) _write (int fd, const void *buf, size_t count)
{
    //WARNING elnyomasa. fd-t nem hasznaljuk.
    (void)fd;
    register size_t CharCnt = count;

    for (;CharCnt > 0x00; --CharCnt)
    {
        if (*(uint8_t*)buf == '\n')
        {   //'\r' (0x0d) kuldese, minden '\n' (0x0a) elott
            Console_stdio_putChar('\r');
        }

        Console_stdio_putChar(*(uint8_t*)buf++);
    }

    //Visszaadjuk a kiirt karakter szamot
    return (int) count;
}
//A fenti pragma parja
#pragma GCC diagnostic pop
//------------------------------------------------------------------------------
//stdio konzolra iras
static void Console_stdio_putChar(char Ch)
{
    if (consoleInited==false)
    {   //A konzolok meg nincsenek inicializalva.
        //Minden csak az RTT-re mehet
        SEGGER_RTT_PutChar(0, Ch);
    } else
    {   //Konzolok hasznalhatok.
        MyConsole_putChar(stdio_destinationConsole, Ch);
    }
}
//------------------------------------------------------------------------------
//stdio kimenet beallitasa valamelyik konzolra
void Console_setStdioDestination(MyConsole_t* destConsole)
{
    stdio_destinationConsole=destConsole;
}
//------------------------------------------------------------------------------
