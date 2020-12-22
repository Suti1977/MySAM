//------------------------------------------------------------------------------
//  Microchip MCP794XX valos ideju ora (RTC) ic csalad driver
//
//    File: MCP794XX.h
//------------------------------------------------------------------------------
#ifndef MCP794XX_H_
#define MCP794XX_H_

#include "MyI2CM.h"
#include <time.h>

#define MCP794XX_SEC_LEN            7

#define MCP794XX_RTCSEC             0x00
#define MCP794XX_RTCSEC_ST          BIT(7)

#define MCP794XX_RTCMIN             0x01

#define MCP794XX_RTCHOUR            0x02
#define MCP794XX_RTCHOUR_AMPM       BIT(5)
#define MCP794XX_RTCHOUR_1224       BIT(6)

#define MCP794XX_RTCWKDAY           0x03
#define MCP794XX_RTCWKDAY_VBATEN    BIT(3)
#define MCP794XX_RTCWKDAY_PWRFAIL   BIT(4)
#define MCP794XX_RTCWKDAY_OSCRUN    BIT(5)

#define MCP794XX_RTCDATE            0x04

#define MCP794XX_RTCMTH             0x05
#define MCP794XX_RTCMTH_LPYR        BIT(5)

#define MCP794XX_RTCYEAR            0x06
//

#define MCP794XX_CONTROL            0x07
#define MCP794XX_CONTROL_SQWFS0     BIT(0)
#define MCP794XX_CONTROL_SQWFS1     BIT(1)
#define MCP794XX_CONTROL_CRSTRIM    BIT(2)
#define MCP794XX_CONTROL_EXTOSC     BIT(3)
#define MCP794XX_CONTROL_ALM0EN     BIT(4)
#define MCP794XX_CONTROL_ALM1EN     BIT(5)
#define MCP794XX_CONTROL_SQWEN      BIT(6)
#define MCP794XX_CONTROL_OUT        BIT(7)

#define MCP794XX_OSCTRIM            0x08
#define MCP794XX_OSCTRIM_SIGN       BIT(7)

#define MCP794XX_EEUNLOCK           0x09



//EUI-48/64 egyedi azonosito kezdocime
#define MCP794XX_REG_EUI                0xf0

//------------------------------------------------------------------------------
//MCP794XX valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} MCP794XX_t;
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void MCP794XX_create(MCP794XX_t* dev, MyI2CM_t* i2c);

//RTC-bol aktualis idopont kiolvasasa.
status_t MCP794XX_getTime(MCP794XX_t* dev, struct tm* tm);

//RTC-be ido beallitasa
status_t MCP794XX_setTime(MCP794XX_t* dev, struct tm* tm);

//RTC-ben talalhato egyedi azonosito olvasasa (UID-48/64)
status_t MCP794XX_readUID(MCP794XX_t* dev, uint8_t* buff, uint32_t len);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MCP794XX_H_
