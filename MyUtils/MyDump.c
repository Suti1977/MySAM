//------------------------------------------------------------------------------
//  Memoriatartalmat konzolra dump-olo rutinok
//
//    File: MyDump.c
//------------------------------------------------------------------------------
#include "MyDump.h"
#include <stdio.h>

//------------------------------------------------------------------------------
//memoriatartalom kilistazasa a konzolra
void MyDump_memory(void* src, unsigned int length)
{
    unsigned char   cnt=0;  //16 bejegyzesenkent kiirja a cimet
    unsigned char*  hexPtr;
    unsigned int    rem;
    unsigned char   ch;
    unsigned char*  srcP=(unsigned char*)src;
    unsigned int    x=0;

    while(length)
    {
        //Cim kiirasa
        hexPtr=srcP;
        printf("[%3.3d] %8.8x: ", x, (unsigned int)hexPtr);
        x+=16;
        //Hexa resz kiirasa...
        rem=length;
        for(cnt=16; cnt!=0; cnt--)
        {
            if (rem==0)
            {
                printf("   ");
            } else
            {
                printf("%2.2x ", *hexPtr++);
                rem--;
            }
        }
        //ASCII resz kiirasa...
        printf("|");
        for(cnt=16; cnt!=0; cnt--)
        {
            if (length==0)
            {
            } else
            {
                ch=(unsigned char)*srcP++;
                if (ch==0) ch=' ';
                else if ((ch<32) || (ch>126)) ch='.';


                printf("%c", ch);
                length--;
            }
        }
        //Sor vege
        printf("|\n");
    }
}
//------------------------------------------------------------------------------
//memoriatartalom kilistazasa a konzolra
void MyDump_memorySpec(void* src,
                        unsigned int length,
                        unsigned int firstPrintedAddress)
{
    unsigned char   cnt=0;  //16 bejegyzesenkent kiirja a cimet
    unsigned char*  hexPtr;
    unsigned int    rem;
    unsigned char   ch;
    unsigned char*  srcP=(unsigned char*)src;
    unsigned int    x=0;

    while(length)
    {
        //Cim kiirasa
        hexPtr=srcP;
        printf("[%3.3d] %8.8x: ", x, (unsigned int)firstPrintedAddress);
        x+=16;
        //Hexa resz kiirasa...
        rem=length;
        for(cnt=16; cnt!=0; cnt--)
        {
            if (rem==0)
            {
                printf("   ");
            } else
            {
                printf("%2.2x ", *hexPtr++);
                firstPrintedAddress++;
                rem--;
            }
        }
        //ASCII resz kiirasa...
        printf("|");
        for(cnt=16; cnt!=0; cnt--)
        {
            if (length==0)
            {
            } else
            {
                ch=(unsigned char)*srcP++;
                if (ch==0) ch=' ';
                else if ((ch<32) || (ch>126)) ch='.';


                printf("%c", ch);
                length--;
            }
        }
        //Sor vege
        printf("|\n");
    }
}
//------------------------------------------------------------------------------
