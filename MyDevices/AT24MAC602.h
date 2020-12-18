//------------------------------------------------------------------------------
//  Microchip AT24MAC602 tipus EUI-64 egyedi azonosito es EEprom driver
//
//    File: AT24MAC602.h
//------------------------------------------------------------------------------
#ifndef AT24MAC602_H_
#define AT24MAC602_H_

#include "MyI2CMem.h"

//Az eeprombol olvashato 64 bites azonosito hossza.
//A rendszerben minimum ekkora buffer allokaciojara van szukseg az olvasashoz!
#ifndef EUI64_LENGTH
#define EUI64_LENGTH    8
#endif

//------------------------------------------------------------------------------
//Memoria handler letrehozasa
void AT24MAC602_create(MyI2CMem_t* dev,
                       MyI2CM_t* i2c,
                       uint8_t slaveAddress);

//48 bites egyedi azonosito kiolvasasa
status_t AT24MAC602_readEUI64( MyI2CMem_t* dev,
                             uint8_t* buffer,
                             uint32_t bufferSize);

//128 bites egyedi sorszam olvasasa
status_t AT24MAC602_readSerial128( MyI2CMem_t* dev,
                                   uint8_t* buffer,
                                   uint32_t bufferSize);

//eeprom terulet olvasasa
status_t AT24MAC602_read( MyI2CMem_t* dev,
                          uint8_t address,
                          uint8_t* src,
                          uint32_t length);

//eeprom terulet irasa
status_t AT24MAC602_write(MyI2CMem_t* dev,
                          uint8_t address,
                          uint8_t* dest,
                          uint32_t length);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //AT24MAC602_H_
