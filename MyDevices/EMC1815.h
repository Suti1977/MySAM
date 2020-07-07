//------------------------------------------------------------------------------
//  EMC1815 remote diode sensore driver
//
//    File: EMC1815.h
//------------------------------------------------------------------------------
#ifndef EMC1815_H_
#define EMC1815_H_

#include "MyI2CM.h"


#define EMC1815_ADDR_PRODUCT_ID             0xFD
#define EMC1815_ADDR_MANUFACTURER_ID        0xFE
#define EMC1815_ADDR_REVISION               0xFF
//------------------------------------------------------------------------------
//EMC1815 valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} EMC1815_t;
//------------------------------------------------------------------------------
//Homero IC-hez I2C eszkoz letrehozasa a rendszerben
void EMC1815_create(EMC1815_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);

//EMC1815 irasa. A megadott cimtol kezd kiirni egy megadott adattartalmat,
//a megadott hosszon
status_t EMC1815_write(EMC1815_t* dev,
                       uint8_t address,
                       uint8_t* data, uint8_t length);

//EMC1815  olvasasa. A megadott cimtol kezd olvasni egy megadott adattartalmat
//a megadott hosszon, a megadott celteruletre
status_t EMC1815_read(EMC1815_t* dev,
                      uint8_t address,
                      uint8_t* data, uint8_t length);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //EMC1815_H_
