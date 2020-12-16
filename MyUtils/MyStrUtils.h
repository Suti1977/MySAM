//------------------------------------------------------------------------------
//  Sajat string kezelo segedrutinok
//------------------------------------------------------------------------------
#ifndef MY_STR_UTILS_H_
#define MY_STR_UTILS_H_

#include <stdbool.h>
#include <stdint.h>

//------------------------------------------------------------------------------
//String atalakitasa szamma (decimalisbol vagy hexabol)
//figyelembe veszi, hogy hexa vagy decimalis formaban adtak e meg a szamot.
//hexa eseten vagy "0x"-el kezdodik a szam, vagy a vegen h-van (azt is
//elfogadja, ha mind a ketto meg van adva) "h" utan viszont nem lehet semmi.
//Negativot nem kezel!!!!
int MyStrUtils_strToValueo_value(const char* srcStr,
                                 uint32_t srcStrLength,
                                 uint32_t *destValue);

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
                              uint32_t* length);

//binarisrol hexa stringbe konevrtalo.
void MyStrUtils_binToHexString(const uint8_t* src,
                               uint32_t srcLength,
                               char* dst);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_STR_UTILS_H_
