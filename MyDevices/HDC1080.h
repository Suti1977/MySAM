//------------------------------------------------------------------------------
//  Texas Instruments HDC1080 paratartalom es homerseklet mero driver
//
//    File: HDC1080.h
//------------------------------------------------------------------------------
#ifndef HDC1080_H_
#define HDC1080_H_

#include "MyI2CM.h"

#define HDC1080_TEMPERATURE         0x00
#define HDC1080_HUMIDITY            0x01
#define HDC1080_CONFIG              0x02
#define HDC1080_SERIALID1           0xFB
#define HDC1080_SERIALID2           0xFC
#define HDC1080_SERIALID3           0xFD
#define HDC1080_MANUFACTURER_ID     0xfe
#define HDC1080_DEVICE_ID           0xff

//------------------------------------------------------------------------------
//HDC1080 driver valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} HDC1080_t;
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void HDC1080_create(HDC1080_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);


//Az IC olvasasa
status_t HDC1080_read(HDC1080_t* dev,
                      uint8_t address,
                      uint8_t* buff,
                      uint8_t length);

//Az IC egy 16 bites regiszterenek irasa
status_t HDC1080_writeReg(HDC1080_t* dev, uint8_t address, uint16_t regValue);

//Meres inditasa.
status_t HDC1080_measure(HDC1080_t* dev,
                         uint16_t* temperature,
                         uint16_t*  humidity);

//Eszkoz altal visszaadott meresi eredmenybol celsius fokra konvertalas
double HDC1080_value2Celsius(uint16_t temperatureValue);

//Eszkoz altal visszaadott meresi eredmenybol %-os paratartalomakonvertalas
double HDC1080_value2Humidity(uint16_t humidityValue);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //HDC1080_H_
