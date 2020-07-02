//------------------------------------------------------------------------------
//  Microchip MCP9800 tipusu homero IC kezelo driver
//
//    File: MCP9800.h
//------------------------------------------------------------------------------
#ifndef MCP9800_H_
#define MCP9800_H_

#include "MyI2CM.h"

#define MCP9800_TEMPERATURE_REG     0x00
#define MCP9800_CONFIGURATION_REG   0x01
#define MCP9800_HYSTERESIS_REG      0x02
#define MCP9800_LIMIT_REG           0x03
//------------------------------------------------------------------------------
//MCP9800 valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} MCP9800_t;
//------------------------------------------------------------------------------
//Homero IC-hez I2C eszkoz letrehozasa a rendszerben
void MCP9800_create(MCP9800_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);

//Homero IC konfiguracios regisztre beallitasa
status_t MCP9800_setConfigReg(MCP9800_t* dev, uint8_t config);

//MCP9800 irasa. A megadott cimtol kezd kiirni egy megadott adattartalmat,
//a megadott hosszon
status_t MCP9800_write(MCP9800_t* dev,
                       uint8_t address,
                       uint8_t* data, uint8_t length);

//MCP9800  olvasasa. A megadott cimtol kezd olvasni egy megadott adattartalmat
//a megadott hosszon, a megadott celteruletre
status_t MCP9800_read(MCP9800_t* dev,
                      uint8_t address,
                      uint8_t* data, uint8_t length);

//MCP9800 homero IC homerseklet regiszter kiolvasasa
//Az elojelesen visszaadott homerseklet erteket 0.062-el megszorozva kapjuk a
//homersekletet °C-ban.
status_t MCP9800_readTemperatureValue(MCP9800_t* dev, int16_t* value);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MCP9800_H_
