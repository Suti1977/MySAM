//------------------------------------------------------------------------------
//  AT21CS01, Single-Wire (SWI) EEPROM es serial number IC drver
//
//    File: AT21CS01.h
//------------------------------------------------------------------------------
#ifndef AT21CS01_H_
#define AT21CS01_H_

#include "MySWI.h"

//------------------------------------------------------------------------------
//Chip gyari azonosito olvasasa.
status_t AT21CS01_readManufacturerID(MySWI_Wire_t* wire,
                                     uint8_t slaveAddress,
                                     uint32_t* id);

//64 bites egyedi sorozatszam olvasasa
//A visszaadott sorozatszam 7. byteja egy 8 bites crc is egyben. (X^8+X^5+X^4+1)
status_t AT21CS01_readSerialNumber(MySWI_Wire_t* wire,
                                   uint8_t slaveAddress,
                                   uint8_t* buffer,
                                   uint8_t bufferLength);

//------------------------------------------------------------------------------
#endif //AT21CS01_H_
