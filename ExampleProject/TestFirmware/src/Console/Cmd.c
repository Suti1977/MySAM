//------------------------------------------------------------------------------
//  Konzolos parancsok tablazata es implementacioja
//------------------------------------------------------------------------------
#include "MyCmdLine.h"
#include <string.h>
#include <stdio.h>

#include "Console.h"
#include "MyDump.h"

static void Cmd_hello(MyCmdLine_t* Cmd);
static void Cmd_reset(MyCmdLine_t* Cmd);
static void Cmd_stdtoserial(MyCmdLine_t* Cmd);
static void Cmd_stdtortt(MyCmdLine_t* Cmd);
static void Cmd_readMem(MyCmdLine_t* Cmd);
static void Cmd_writeMem(MyCmdLine_t* Cmd);


const MyCmdLine_CmdTable_t g_Cmd_cmdTable[] /*__attribute__ ((section(".text")))*/=
{   //CmdStr        MinPC.  MaxPC      Func
    {"hello",       0,      2,         Cmd_hello},
    {"reset",       0,      0,         Cmd_reset},
    {"stdtoserial", 0,      0,         Cmd_stdtoserial},
    {"stdtortt",    0,      0,         Cmd_stdtortt},
    {"rmem",        2,      2,         Cmd_readMem},
    {"wmem",        2,      2,         Cmd_writeMem},
    {0,             0,      0,         NULL}
};

//------------------------------------------------------------------------------
//Teszt parancs (TESZT)
static void Cmd_hello(MyCmdLine_t* cmd)
{    
    MyCmdLine_putString(cmd, "Hello bello!\n");
    printf("teszt\n");
}
//------------------------------------------------------------------------------
//mikrovezerlo resetelese parancsrol
static void __attribute__((noreturn)) Cmd_reset(MyCmdLine_t* cmd)
{
    (void) cmd;
    NVIC_SystemReset();
}
//------------------------------------------------------------------------------
//stdio soros konzolra iranyitasa
static void Cmd_stdtoserial(MyCmdLine_t* cmd)
{
    (void) cmd;
    Console_setStdioDestination(&serialConsole.console);
    printf("\nstd in serial.\n");
}
//------------------------------------------------------------------------------
//stdio RTT konzolra iranyitasa
static void Cmd_stdtortt(MyCmdLine_t* cmd)
{
    (void) cmd;
    Console_setStdioDestination(&RTTconsole.console);
    printf("\nstd in RTT.\n");
}
//------------------------------------------------------------------------------
//Processzor memoria olvasasa
static void Cmd_readMem(MyCmdLine_t* cmd)
{
    uint32_t startAddress;
    uint32_t length;
    int res;

    //Kezdocim
    res=MyStrUtils_strToValueo_value(  cmd->args[1].str,
                                    cmd->args[1].length,
                                    &startAddress);
    if (res) goto CmdErr;

    //hossz
    res=MyStrUtils_strToValueo_value(  cmd->args[2].str,
                                    cmd->args[2].length,
                                    &length);
    if (res) goto CmdErr;


    MyDump_memorySpec((void*)startAddress, length, startAddress);

    return;

CmdErr:
    MyCmdLine_putError(cmd, "syntax");
    return;
}

//------------------------------------------------------------------------------
//Processzor memoria irasa
static void Cmd_writeMem(MyCmdLine_t* cmd)
{
    uint32_t startAddress;
    static uint8_t buff[128];
    uint32_t length;
    int res;

    //Kezdocim
    res=MyStrUtils_strToValueo_value(  cmd->args[1].str,
                                    cmd->args[1].length,
                                    &startAddress);
    if (res) goto CmdErr;

    //A hexa adattartalom atalakitasa binarissa
    res=MyStrUtils_hexStringToBin( cmd->args[2].str,
                                        buff,
                                        sizeof(buff),
                                        &length);
    if (res) goto CmdErr;

    //A bufferbe konvertalt tartalom kiirasa a proci memoriajaba
    memcpy((void*) startAddress, buff, length);

    MyCmdLine_putString(cmd, "Ok.\n");
    return;

CmdErr:
    MyCmdLine_putError(cmd, "syntax");
    return;
}
//------------------------------------------------------------------------------
