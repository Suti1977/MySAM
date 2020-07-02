//------------------------------------------------------------------------------
//  BCD szamok kezeleset segito fuggvenyek
//
//    File: MyBCD.h
//------------------------------------------------------------------------------
#ifndef MYBCD_H_
#define MYBCD_H_

#include <stdint.h>

//BCD-bol binarissa alakitas
uint8_t MyBCD_bcd2bin(uint8_t bcdvalue);

//Binarisbol BCD-be alakitas
uint8_t MyBCD_bin2bcd(uint8_t binvalue);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYBCD_H_
