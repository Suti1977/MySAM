//------------------------------------------------------------------------------
//  CRC szamitast tamogato rutinok
//
//    File: MyCRC.h
//------------------------------------------------------------------------------
#ifndef MYCRC_H_
#define MYCRC_H_

#include "MyCommon.h"


//CRC szamitas az indulo ertek beallitasaval. Ez lehetove teszi tobb blokk
//egymas utani szamitasat.
uint16_t MyCRC_calcCCIT_next(uint16_t initValue,
                             const uint8_t* data,
                             uint32_t size);

//CRC szamitas. Poligon: 0x1021  kezdo ertek: 0xffff
uint16_t MyCRC_calcCCIT(uint8_t* data, uint32_t length);

//CRC szamitas. Poligon: 0x1021  kezdo ertek: 0x0000
uint16_t MyCRC_calcCCIT_zero(uint8_t* data, uint32_t length);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYCRC_H_
