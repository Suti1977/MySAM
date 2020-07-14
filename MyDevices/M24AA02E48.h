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

//eeprom terulet olvasasa
status_t M24AA02E48_read( MyI2CMem_t* dev,
                          uint8_t address,
                          uint8_t* src,
                          uint32_t length);

//eeprom terulet irasa
status_t M24AA02E48_write(MyI2CMem_t* dev,
                          uint8_t address,
                          uint8_t* dest,
                          uint32_t length);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //M24AA02E48_H_
