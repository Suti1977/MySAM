//------------------------------------------------------------------------------
//  BCD szamok kezeleset segito fuggvenyek
//
//    File: MyBCD.c
//------------------------------------------------------------------------------
#include "MyBCD.h"

//------------------------------------------------------------------------------
//BCD-bol binarissa alakitas
uint8_t MyBCD_bcd2bin(uint8_t bcdvalue)
{
    return (bcdvalue & 0x0f) + ((bcdvalue >> 4) * 10);
}

//------------------------------------------------------------------------------
//Binarisbol BCD-be alakitas
uint8_t MyBCD_bin2bcd(uint8_t binvalue)
{
    return (uint8_t)((binvalue / 10) << 4) + (binvalue % 10);
}

//------------------------------------------------------------------------------
