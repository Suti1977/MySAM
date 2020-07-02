//------------------------------------------------------------------------------
//  Microchip 24AA02E48 tipus EUI-48 egyedi azonosito es EEprom driver
//
//    File: M24AA02E48.h
//------------------------------------------------------------------------------
#ifndef M24AA02E48_H_
#define M24AA02E48_H_

#include "MyI2CMem.h"

//Az eeprombol olvashato 48 bites azonosito hossza.
//A rendszerben minimum ekkora buffer allokaciojara van szukseg az olvasashoz!
#define EUI48_LENGTH    6

//------------------------------------------------------------------------------
//Memoria handler letrehozasa
void M24AA02E48_create(MyI2CMem_t* dev,
                       MyI2CM_t* i2c,
                       uint8_t slaveAddress);

//48 bites egyedi azonosito kiolvasasa
status_t M24AA02E48_readUID( MyI2CMem_t* dev,
                             uint8_t* buffer,
                             uint32_t bufferSize);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //M24AA02E48_H_
