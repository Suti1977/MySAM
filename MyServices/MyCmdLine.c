//------------------------------------------------------------------------------
//  Soros (vagy RTT) terminalos altalanos parancsertelmezo
//------------------------------------------------------------------------------
#include "MyCmdLine.h"
#include <string.h>


//Hibauzenet, ha ismeretlen parancsot kapott.
static char const MyCmdLine_str_unknownCommand[]="Unknown command";
//Hibauzenet, ha tobb vagy kevesebb argumentum szukseges a parancshoz
static char const MyCmdLine_str_argumentsCount[]="Arguments count";
//------------------------------------------------------------------------------
static void MyCmdLine_tokenise(MyCmdLine_t* cmd);
static void MyCmdLine_execute(MyCmdLine_t* cmd);
//------------------------------------------------------------------------------
//parancssor kezdeti inicializalasa
void MyCmdLine_init(MyCmdLine_t* cmd, const MyCmdLine_Config_t* config)
{
    ASSERT(cmd);
    ASSERT(config);
    ASSERT(config->lineBuffer);
    ASSERT(config->lineBufferSize);

    //Modul valtozoinak kezdeti torlese.
    memset(cmd, 0, sizeof(MyCmdLine_t));

    //konfiguracio atvetele
    memcpy(&cmd->configs, config, sizeof (MyCmdLine_Config_t));

    cmd->lineBuffer=config->lineBuffer;
    cmd->lineBufferSize=config->lineBufferSize;

    //Line buffer nullazasa
    memset(cmd->lineBuffer, 0, cmd->lineBufferSize);

    //A modul altal hasznalhato hossz egyel rovidebb. Igy a legutolso helyen
    //mindig marad egy 0x00
    cmd->lineBufferSize--;
}
//------------------------------------------------------------------------------
//Prompt kiirasa a konzolra
void MyCmdLine_putPrompt(MyCmdLine_t* cmd)
{
    MyCmdLine_putString(cmd, cmd->configs.promptString);
}
//------------------------------------------------------------------------------
//Parancs vegrehajtasnal hiba string kuldese a konzolra
void MyCmdLine_putError(MyCmdLine_t* cmd, const char* errorStr)
{
    MyCmdLine_putString(cmd, "ERROR:");
    MyCmdLine_putString(cmd, errorStr);
    MyCmdLine_putChar(cmd, (char)'\n');
}
//------------------------------------------------------------------------------
//Parancssor etetese uj karakterekkel. Ha entert kap, akkor a bufferben addig
//tarolt parancssort elkezdi felertelmezni, es a regisztralt parancsokat
//meghivni.
void MyCmdLine_feeding(MyCmdLine_t* cmd, char newChar)
{
    uint32_t toBuffer=1;
    uint32_t toEcho=1;

    //vett karakter ellenorzese, hogy nem-e sor vege jel
    switch(newChar)
    {
        case CHAR_BACKSPACE:
                    if (cmd->lineLength>0)
                    {   //A bufferben van mit visszatorolni
                        cmd->lineLength--;
                    }
                    toBuffer=0;
                    break;

    case CHAR_CR:  //sorvege. jelet egy az egyben eldobjuk!
                    return;
    case CHAR_LF:  //Kocsi vissza
                    toBuffer=0;
                    break;
    }


    //A vett karakter elhelyezese a bufferben...
    if (toBuffer)
    {
        if (cmd->lineLength<cmd->lineBufferSize)
        {   //Van hely a bufferben. Elhelyezzuk az uj karaktert
            //(Megj: a buffer 1 karakterrel hoszabbra van foglalva, ahova egy
            // lezaro karaktert be tudunk tenni.)
            cmd->lineBuffer[cmd->lineLength]=newChar;

            //karakterszam novelese
            cmd->lineLength++;
        } else
        {   //Nincs tobb hely a bufferben.
            //Ezt avval jelezzuk, hogy nem lesz echo
            toEcho=0;
        }
    }

    //Ha az echo nincs kikapcsolva, akkor a bekapott karaktert visszakuldjuk a
    //konzolra.
    if ((cmd->flags.echoOff==0) && (toEcho))
    {   //Az ECHO nincs tiltva
        MyCmdLine_putChar(cmd, newChar);
    }

    if (newChar==CHAR_LF)
    {   //ENTER --> Parancs ertelmezese....
        //parancs/argumentumok szetparzolasa
        MyCmdLine_tokenise(cmd);
        //Parancs keresese, es ha megvan, akkor annak vegrehajtasa
        MyCmdLine_execute(cmd);

        //A parancssorban ugras az elejere
        cmd->lineLength=0;
        //Sor uritese.
        memset(cmd->lineBuffer, 0, cmd->configs.lineBufferSize);

        //Uj sorba uj prompt
        MyCmdLine_putPrompt(cmd);
    }
}
//------------------------------------------------------------------------------
//Parancssor szetbontasa szavakra. A felesleges szokozek kihagyasa
static void MyCmdLine_tokenise(MyCmdLine_t* cmd)
{
    //argumentum leirok nullazasa
    memset(cmd->args, 0, sizeof(cmd->args));

    //Talalt argumentumok szamanak nullazasa
    cmd->argsCount=0;

    //Src a forrast cimzi. (parancssor line buffert)
    char* src=cmd->lineBuffer;

    //1, ha szokoz vagy tab atugralas van
    //(indulaskor 1, hogy atugralhassa a kezdeti spaceket)
    uint32_t  skip=1;

    MyCmdLine_Argument_t* argument=&cmd->args[0];
    //A soron levo szo/argumentum hosszanak szamlaloja
    uint32_t lengthCnt=0;

    //Egy lezaro space elhelyezese az utolso begepelt karakter utan.
    //Ez azert kell, mert majd a tokenizalo rutin a legutolso szo vege utan is
    //talalhasson egy spacet, amire a tokent lezarhatja.
    cmd->lineBuffer[cmd->lineLength]=' ';

    //Vegig a sor karakterein...
    //(A beszurt space miatt 1-el hoszabb a ciklus)
    for(int i=(int)cmd->lineLength+1; i>0; i--, src++)
    {
        //Vesszuk a soron kovetkezo karaktert
        char C=*src;

        //felesleges szokozek, es tabok atugralasa
        if ((C==CHAR_SPACE) || (C==CHAR_TAB))
        {	//szokoz vagy tab
            if (skip)
            {   //ha atugralas van, akkor ugrik
                continue;
            }
            else
            {	//Nincs atugralas. Ez egy argumentum/szo vege.

                //Erre a poziciora egy szavat lezaro 0x00 lesz elhelyezve, hogy
                //stringkent tudjuk majd kezelni
                *src=0x00;
                //hossz beallitasa
                argument->length=(uint16_t)lengthCnt;
                //uj argumentum bejegyzesre allas
                argument++;
                //argumentumok szamlalojanak novelese
                cmd->argsCount++;
                //tovabbi szokozek/tabok atugralasa
                skip=1;
                continue;
            }
        } else
        {	//normal karakter, amelyik a szohoz tartozik
            if (skip)
            {	//atugralas volt
                //(tehat keressuk a szo elejet, ami most lett meg)

                //szo kezdete raalitva erre a poziciora
                argument->str=src;
                //hossz szamlalo nullazasa (az if utan 1-lesz, tehat ezt a
                //karaktert is beleszamolja)
                lengthCnt=0;
                //nem ugral tovabb at karaktereket. Keresi helyett a szohoz
                //tartozo tovabbi betuket...
                skip=0;
            }
        }

        //szo/argumentum hossz novelese
        lengthCnt++;
    } //for i

}
//------------------------------------------------------------------------------
//Parancs/argumentum hasonlito fuggveny
//visszateresi ertek 0, ha nem egyezik. 1, ha egyezik
int MyCmdLine_compare(const char* str1, const char* str2)
{
    while(1)
    {
        if (*str1 != *str2)
        {	//ket karakter nem egyezik
            break;
        }

        //A ket karakter egyezik
        if (*str1==0x00)
        {   //A ket karakter egyezik, es ez a veguk, mert 0x00 a lezaras.
            return 1;
        }

        str1++;
        str2++;
    }
    //A ciklus kilepett, es nem volt egyezes, vagy string vege
    //nem egyeznek

    return 0;
}
//------------------------------------------------------------------------------
//Parancs keresese es ha van olyan, akkor annak vegrahajtasa...
static void MyCmdLine_execute(MyCmdLine_t* cmd)
{
    //Ha ures a parancssor, akkor nincs mit tenni
    if (cmd->argsCount==0) return;

    //Ez cimzi vegig a parancs tablazatot
    const MyCmdLine_CmdTable_t* item=
                            (const MyCmdLine_CmdTable_t*)cmd->configs.cmdTable;
    int found=0;

    while(1)
    {
        //Ellenorzes, hogy a tablazaton vegig ertunk-e
        if (item->cmdStr==0x00) break;

        //Prancs hasonlitasa...
        //A legelso szo a hasonlitott parancs
        if (MyCmdLine_compare(cmd->args[0].str, (const char*) item->cmdStr))
        {   //parancs egyezes!
            //Ellenorzes, hogy a parancshoz eloirt argumentum szam megvan-e...
            uint32_t Temp=cmd->argsCount-1;
            if ((Temp<item->minArgumentsCount) ||
                (Temp>item->maxArgumentsCount))
            {   //Kevesebb vagy tobb az argumentum, mint amennyi elo van irva
                MyCmdLine_putError(cmd, MyCmdLine_str_argumentsCount);
            } else
            {   //argumentumok szama rendben
                //Callback fuggveny meghivasa.
                MyCmdLine_Func_t* func=item->cmdFunc;
                ASSERT(func);
                func(cmd);
            }

            found=1;

            //Kilepes a ciklusbol. Nem kell tovabbiakban parancsot hasonlitgatni
            break;
        }

        //Uj tablazati sor kijelolese
        item++;
    }

    if (found==0)
    {   //nem talalta meg a parancsot
        MyCmdLine_putError(cmd, MyCmdLine_str_unknownCommand);
    }
}
//------------------------------------------------------------------------------
//egyetlen karakter kiirasa a konzolra
void MyCmdLine_putChar(MyCmdLine_t* cmd, char c)
{
    cmd->configs.putCharFunc(c, cmd->configs.userData);
}
//------------------------------------------------------------------------------
//Sring kiirasa a konzolra
void MyCmdLine_putString(MyCmdLine_t* cmd, const char* str)
{
    cmd->configs.putStringFunc(str, cmd->configs.userData);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
