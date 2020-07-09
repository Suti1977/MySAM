//------------------------------------------------------------------------------
//  1 vezeteskes homerseklet mero driver
//
//    File: DS18B20.h
//------------------------------------------------------------------------------
#ifndef DS18B20_H_
#define DS18B20_H_

#include "MyOWI.h"

#define DS18B201_SEARCH_ROM         0xF0
#define DS18B201_READ_ROM           0x33
#define DS18B201_MATCH_ROM          0x55
#define DS18B201_SKIP_ROM           0xCC
#define DS18B201_ALARM_SEARCH       0xEC
#define DS18B201_CONVERT_T          0x44
#define DS18B201_WRITE_SCRATCHPAD   0x4E
#define DS18B201_READ_SCRATCHPAD    0xBE
#define DS18B201_COPY_SCRATCHPAD    0x48
#define DS18B201_RECALL             0xB8
#define DS18B201_READ_POWER_SUPPLY  0xB4

#define DS18B201_MODEL_DS1820       0x10
#define DS18B201_MODEL_DS18S20      0x10
#define DS18B201_MODEL_DS1822       0x22
#define DS18B201_MODEL_DS18B20      0x28

#define DS18B201_SIZE_SCRATCHPAD    9
#define DS18B201_TEMP_LSB           0
#define DS18B201_TEMP_MSB           1
#define DS18B201_ALARM_HIGH         2
#define DS18B201_ALARM_LOW          3
#define DS18B201_CONFIGURATION      4

#define DS18B201_CRC8               8
#define DS18B201_RES_9_BIT          0x1F
#define DS18B201_RES_10_BIT         0x3F
#define DS18B201_RES_11_BIT         0x5F
#define DS18B201_RES_12_BIT         0x7F

#define DS18B201_CONV_TIME_9_BIT    94
#define DS18B201_CONV_TIME_10_BIT   188
#define DS18B201_CONV_TIME_11_BIT   375
#define DS18B201_CONV_TIME_12_BIT   750

#pragma pack(1)
typedef struct
{
    uint8_t tempLsb;
    uint8_t tempMsb;
    uint8_t tH;
    uint8_t tL;
    uint8_t config;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    uint8_t crc;
} DS18B201_scratchPad_t;
#pragma pack()

//------------------------------------------------------------------------------
//Scratch pad kiolvasasa.
status_t DS18B201_readScratchPad(MyOWI_Wire_t* wire,
                                 DS18B201_scratchPad_t* buff);

//Homerseklet kiolvasasa (RAW adat)
status_t DS18B201_readTemperatute(MyOWI_Wire_t* wire,
                                  uint16_t* temperatureValue);

//Homerseklet meres/konverzio inditasa
status_t DS18B201_startMeasure(MyOWI_Wire_t* wire);

//Homerseklet meres/konverzio eredmenyenek kiolvasasa
status_t DS18B201_readResult(MyOWI_Wire_t* wire, uint16_t* temperatureValue);

//12 bites meresi adat celsius fojra konvertalasa
double DS18B201_value2Celsius(uint16_t temperatureValue);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //DS18B20_H_
