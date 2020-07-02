//------------------------------------------------------------------------------
//  Soros (vagy RTT) terminalos altalanos parancsertelmezo
//------------------------------------------------------------------------------
#ifndef MYCMDLINE_H_
#define MYCMDLINE_H_

#include "MyCommon.h"

//Maximum ennyi argumentumig kepes a parancsertelmezo szetszedni a parancssort.
//Ebbe beletartozik maga a parancs is, a 0-as indexen
//[Globalisan feluldefinialhato.]
#ifndef MY_CMDLINE_MAX_ARGUMENTS_COUNT
#define MY_CMDLINE_MAX_ARGUMENTS_COUNT     8
#endif

//------------------------------------------------------------------------------
//karakter kiirasat megvalosito callback funkcio prototipusa
typedef void MyCmdLine_putCharFunc_t(char c, void* userData);
//0 vegu string kiirasat megvalosito callback funkcio prototipusa
typedef void MyCmdLine_putStringFunc_t(const char* str, void* userData);
//------------------------------------------------------------------------------
//A parancssor parzolt szavainak leiroja
typedef struct
{
    //A pointer a szo elejere mutat
    char*       str;
    //Az argumentum hossza
    uint32_t    length;
} MyCmdLine_Argument_t;
//------------------------------------------------------------------------------
//Cmd flagjeinek definailasa
typedef struct
{
    //Echo tiltott, ha ez a flag 1
    unsigned echoOff :1;

} MyCmdLine_Flags_t;
//------------------------------------------------------------------------------
//A parancsertelmezo konfiguracios beallitasai
typedef struct
{
    //Az applikacioban a parancsornak allokalt memoriateruletre mutat
    char* lineBuffer;
    //A foglalt terulet hossza.
    //Megj: Ennel egyel kevesebb karakter irhato a bufferbe, mivel az utolso
    //      helyre egy lezaro 0x00 kerul.
    uint32_t lineBufferSize;

    //Karaktert kiiro kulso callback fuggeveny
    MyCmdLine_putCharFunc_t*     putCharFunc;
    //Stringet kiiro kulso callback fuggveny
    MyCmdLine_putStringFunc_t*   putStringFunc;
    //A callback fuggevenyeknek atadando tetszoleges adat
    void* userData;

    //Prompt eseten kiirando string
    const char* promptString;
    //A parancsertelmezohoz tartozo parancs tablazatra mutat
    const struct MyCmdLine_CmdTable_t* cmdTable;
}MyCmdLine_Config_t;
//------------------------------------------------------------------------------
//Cmd valtozoi
typedef struct
{
    //Ebbe a bufferbe kerul a parancssor
    char*   lineBuffer;
    //A buffer meretenel 1-el kisebb, hogy a valos buffer legutolso helyen
    //eltudjunk helyezni egy lezaro 0x00-at, mely az utolso beirhato karakter
    //utan all.
    uint32_t lineBufferSize;

    //A parancssor hossza. Ennyi karakter van a bufferben.
    //A buffert is evvel indexeljuk
    uint32_t lineLength;

    //A parzolt argumentumok/szavak leiroi.A 0. indexen maga a parancs talalhato
    MyCmdLine_Argument_t args[MY_CMDLINE_MAX_ARGUMENTS_COUNT];
    //Argumentumok(szavak) szama
    uint32_t argsCount;

    //A mukodeshez flagek
    MyCmdLine_Flags_t flags;
    //A parancsertelmezo konfiguracios beallitasai
    MyCmdLine_Config_t configs;

} MyCmdLine_t;
//------------------------------------------------------------------------------
//A parancsertelmezo altal hivhato parancsok prototipusa. (Callback fuggvenyek)
typedef void MyCmdLine_Func_t(MyCmdLine_t* cmd);
//------------------------------------------------------------------------------
//Parancseretlmezo altal ismert parancstablazaton beluli bejegyzesek felepitese
typedef struct
{
    //Parancs string
    const char* cmdStr;
    //A parancshoz minimalisan igenyelt argumentum szam
    uint8_t minArgumentsCount;
    //A parancshoz maximalisan megadhato argumentum szam
    uint8_t maxArgumentsCount;
    //A parancshoz tartozo callback rutin, mely egyezeskor meghivasra kerul
    MyCmdLine_Func_t* cmdFunc;
} MyCmdLine_CmdTable_t;
//------------------------------------------------------------------------------
//Nehany karakter ascci kodjanak definialasa
#define CHAR_SPACE 			(char)' '
#define CHAR_TAB			(char)0x09
#define CHAR_BACKSPACE      (char)0x08
#define CHAR_LF             (char)0x0A
#define CHAR_CR             (char)0x0D
//------------------------------------------------------------------------------
//Parancssor kezdeti inicializalasa es konfiguralasa
void MyCmdLine_init(MyCmdLine_t* cmd, const MyCmdLine_Config_t* config);

//Parancssor etetese uj karakterekkel. Ha entert kap, akkor a bufferben addig
//tarolt parancssort elkezdi felertelmezni, es a regisztralt parancsokat
//meghivni.
void MyCmdLine_feeding(MyCmdLine_t* cmd, char new_char);

//Prompt kiirasa a konzolra
void MyCmdLine_putPrompt(MyCmdLine_t* cmd);

//Egyetlen karakter kiirasa a konzolra
void MyCmdLine_putChar(MyCmdLine_t* cmd, char c);

//Sring kiirasa a konzolra
void MyCmdLine_putString(MyCmdLine_t* cmd, const char* str);

//Parancs vegrehajtasnal hivhato. Hiba string kuldese a konzolra.
void MyCmdLine_putError(MyCmdLine_t* cmd, const char* errorStr);

//Parancs/argumentum hasonlito fuggveny
//visszateresi ertek 0, ha nem egyezik. 1, ha egyezik
int MyCmdLine_compare(const char* str1, const char* str2);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYCMDLINE_H_
