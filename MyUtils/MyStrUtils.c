//------------------------------------------------------------------------------
//  Sajat string kezelo segedrutinok
//------------------------------------------------------------------------------
#include "MyStrUtils.h"
#include <stddef.h>
#include <string.h>

//------------------------------------------------------------------------------
//HEXA-ban megadott stringbol binarisra konevrtalo.
//A HEXA digitek lehetnek: 0-9 a-f A-f (vegyesen)
//srcStr: forras string
//dst: egy buffer, ahova a konevrtalt adatokat teszi
//bufferSize: A konvertalt adatoknak fenntartott buffer merete
//length-ben adja vissza a kimeneti bufferbe irt elemek szamat.
//Visstzateresi eretk:
//  -1, ha kicsi a buffer
//  -2, ha illegalis karaktert talalt
//  0-  (egyebkent) a kimeneti adathalmaz merete.
int MyStrUtils_hexStringToBin(const char* srcStr,
                                   uint8_t* dst,
                                   int bufferSize,
                                   uint32_t* length)
{
    //Ebbe rakjuk ossze a ket 4 bites nibblebol a bufferbe irando byteot.
    register uint8_t Value=0;

    //Binaris eredmenytomb meretet szamolja. A 4 bites nibbleket szamoljuk le
    //vele. A vegen majd ha leosztjuk 2-vel, akkor kapjuk meg, hogy mennyi
    //byteot tettunk a bufferbe.
    //(Egyben ez lesz a visszateresi ertek is!)
    //Azert lett ilyen trukkosen megcsinalva,mert igy kisebb kodot fordit ARM-re
    register int LengthCnt=0;


    while(1)
    {
        register uint8_t Ch=*srcStr++;
        if (Ch==0x00) break;

        if (LengthCnt >= bufferSize)
        {   //Kisebb a buffer, mint amekkoraba beleferne az eredmeny
            //(A vegen mivel 2-vel le lesz osztva, ezert 2-vel szorozzuk)
            return -1;            //TODO: HIBAKODOT ADNI!!!
        }

        register uint8_t Digit;
        if ((Ch>='0') && (Ch<='9'))
        {
            Digit=Ch-'0';
        }
        else if ((Ch>='a') && (Ch<='f'))
        {
            Digit=Ch-('a'-10);
        }
        else if ((Ch>='A') && (Ch<='F'))
        {
            Digit=Ch-('A'-10);
        }
        else
        {   //illegalis karaktert talalt. Az nem hexa.
            return -2;            //TODO: HIBAKODOT ADNI!!!
        }

        if (LengthCnt & 1)
        {   //also 4 bit
            Value |= Digit;
            //Uj byte kiirasa a bufferbe
            *dst++=Value;
            //eredmenyben a byteok szamanak novelese
        } else
        {   //felso 4 bit
            Value = (uint8_t) (Digit << 4);
        }

        //uj nibble kijelolese
        LengthCnt++;
    }

    *length=(uint32_t)LengthCnt >> 1;

    return 0;
}
//------------------------------------------------------------------------------
//String atalakitasa szamma (decimalisbol vagy hexabol)
//figyelembe veszi, hogy hexa vagy decimalis formaban adtak e meg a szamot.
//hexa eseten vagy "0x"-el kezdodik a szam, vagy a vegen h-van (azt is
//elfogadja, ha mind a ketto meg van adva) "h" utan viszont nem lehet semmi.
//Negativot nem kezel!!!!
int MyStrUtils_strToValueo_value(const char* srcStr,
                              uint32_t srcStrLength,
                              uint32_t *destValue)
{
    uint64_t temp;
    char *p;
    uint32_t i;
    uint32_t itIsHexa=0;	//1, ha ez a parameter hexa
    char *startP;			//itt kezdodik a szam
    char *endP;				//itt vegzodik a szam
    uint32_t length;		//ilyen hosszu a szam resz
    uint32_t digit;         //viszi a digitet
    uint32_t value;         //digit erteke

    *destValue=0;

    //kezdetben ugyan olyan hosszu a szamresz, amig nem tudjuk, hogy hexa e
    length=srcStrLength;

    //nagybetusites....
    p=srcStr;
    //vegig a karaktereken...
    for(i=length; i>0; i--)
    {
        if ((*p >= 'a') && (*p<='z')) *p = (char) (*p + ('A'-'a'));
        p++;
    }

    //elejere mutat
    startP=srcStr;
    //vegere mutat (utolso karakterre)
    endP=srcStr+(srcStrLength-1);

    //vizsgalat 0x-re...
    if ((*startP=='0') && (*(startP+1)=='X') && (length>2))
    {	//0x-el kezdodik -> hexa
        itIsHexa=1;		//hexa
        startP += 2;	//szam reszre ugrik
        length -= 2;	//0x levonasa a hosszbol
    }
    if (*endP=='H')
    {	//h-ra vegzodik->hexa
        itIsHexa=1;		//hexa
        endP--;			//leszedi a h-t
        length--;		//a hosszbol is
    }
    //ellenorzes a hosszra
    if (length<=0)
    {
        //hiba, mert elfogyott a szamresz, nincs mit atalakitani
        return -1;
    }


    //ellenorzes a szamresz karakterjeire. Ha hexa, akkor megengedett A-F-ig is
    //munka pointer a szam resz elejen
    p=startP;
    for(i=length; i>0; i--)
    {
        if ((*p<'0') || (*p>'9'))
        {	//illegalis karakter vagy hexa
            if (itIsHexa)
            {   //hexa eseten megengedett A-F
                if (!((*p>='A') && (*p<='F')))
                {   //illegalis karakterekbol all
                    return -1;
                }
            } else
            {   //nem hexa, ezert illegalis a karakter
                return -1;
            }
        }
        p++;
    }

    //kezdeti nullak atugrasa...
    for(i=length; i>0; i--)
    {
        if (*startP!='0') break;	//ha nem nulla, akkor kilep
        startP++;					//kezdetet odebbviszi
        length--;					//hosszot csokkenti
    }

    if (length==0)
    {	//csak nullakbol allt a szamresz
        *destValue=0;
        return 0;
    }

    if ((length>8) && (itIsHexa))
    {   //tul hosszu a szam. ekkorat nem kezel, mert nem fer el 32 biten
        return -1;
    }
    if ((length>10) && (itIsHexa==0))
    {   //tul hosszu a szam. ekkorat nem kezel, mert nem fer el 32 biten
        return -1;
    }

    temp=0;
    //munkapointer a szamresz utolso karakteren
    p=endP;
    //legalso helyiertek
    digit=1;
    for(i=length; i>0; i--)
    {
        if (*p>='A') value = (*p - 'A')+10;
        else         value = *p-'0';
        temp += (digit * value);

        //kovetkezo digit kiszamitasa
        if (itIsHexa) digit <<= 4;
        else          digit *= 10;

        p--;
    }

    if (temp > 0xFFFFFFFF)
    {   //tul hosszu a szam, nem fer el 32 biten
        return -1;
    }

    *destValue = (uint32_t)temp;

    //ok
    return 0;
}
//------------------------------------------------------------------------------
//binarisrol hexa stringbe konevrtalo.
void MyStrUtils_binToHexString(const uint8_t* Src, uint32_t srcLength, char* dst)
{
    char* ptr=dst;
    for(; srcLength; srcLength--)
    {
        const char hextable[16]="0123456789ABCDEF";
        *ptr++= hextable[ *Src   >>   4 ];
        *ptr++= hextable[ *Src++ & 0x0f ];
    }
    //String vegere lezaro 0x00 kihelyezese
    *ptr=0x00;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
