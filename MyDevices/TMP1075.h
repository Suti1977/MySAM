//------------------------------------------------------------------------------
//  TMP1075 homero ic driver
//
//    File: TMP1075.h
//------------------------------------------------------------------------------
#ifndef TMP1075_H_
#define TMP1075_H_

#include "MyI2CM.h"



#define TMP1075_ADDR_TEMP       0x00
#define TMP1075_ADDR_CFGR       0x01
#define TMP1075_ADDR_LLIM       0x02
#define TMP1075_ADDR_HLIM       0x03
#define TMP1075_ADDR_DIEID      0x0F


#define TMP1075_CFGR_SD         0x0100
#define TMP1075_CFGR_TM         0x0200
#define TMP1075_CFGR_POL        0x0400
#define TMP1075_CFGR_F_pos      11
#define TMP1075_CFGR_R_pos      13
#define TMP1075_CFGR_OS         0x8000


//Conversion rate setting when device is in continuos conversion
typedef enum
{
   TMP1075_CONV_RATE_27ms  =0,
   TMP1075_CONV_RATE_55ms  =1,
   TMP1075_CONV_RATE_110ms =2,
   TMP1075_CONV_RATE_220ms =3,

} TMP1075_convRate_t;

//Selects the function of the ALERT pin
typedef enum
{
    //ALERT pin functions in comparator mode
    TMP1075_ALERT_MODE_COMPARATOR=0,
    //ALERT pin functions in interrupt mode
    TMP1075_ALERT_MODE_INTERRUPT=1,
} TMP1075_alertMode_t;

//Polarity of the output pin
typedef enum
{
    TMP1075_ALERT_POLARITY_ACTIVE_LOW=0,
    TMP1075_ALERT_POLARITY_ACTIVE_HIGH=1,
} TMP1075_alertPolarity_t;

//Consecutive fault measurements to trigger the alert function
typedef enum
{
    TMP1075_FAULT_1=0,
    TMP1075_FAULT_2=1,
    TMP1075_FAULT_3=2,
    TMP1075_FAULT_4=3,
} TMP1075_fault_t;

typedef struct
{

    //SD
    //0: Device is in continuos conversion
    //1: Device is in shutdown mode
    bool shutDownMode;

    //TM
    //Selects the function of the ALERT pin
    TMP1075_alertMode_t alertMode;

    //POL
    //Polarity of the output pin
    TMP1075_alertPolarity_t alertPolarity;

    //Consecutive fault measurements to trigger the alert function
    TMP1075_fault_t fault;

    //Conversion rate setting when device is in continuos conversion
    TMP1075_convRate_t convRate;
} TMP1075_Config_t;

//------------------------------------------------------------------------------
//TMP1075 valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} TMP1075_t;

//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void TMP1075_create(TMP1075_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);

//Az IC olvasasa
status_t TMP1075_read(TMP1075_t* dev,
                      uint8_t address,
                      uint8_t* buff,
                      uint8_t length);


//Az IC egy 16 bites regiszterenek irasa
status_t TMP1075_writeReg(TMP1075_t* dev, uint8_t address, uint16_t regValue);

//Az IC egy 16 bites regiszterenek olvasasa
status_t TMP1075_readReg(TMP1075_t* dev, uint8_t address, uint16_t* regValue);

//Eszkoz altal visszaadott meresi eredmenybol celsius fokra konvertalas
double TMP1075_value2Celsius(uint16_t temperatureValue);

//TMP1075 homerseklet kiolvasasa.
status_t TMP1075_readTemperatureValue(TMP1075_t* dev,
                                      uint16_t* value);

//TMP1075 konfiguralasa
status_t TMP1075_config(TMP1075_t* dev,
                        const TMP1075_Config_t* cfg);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //TMP1075_H_
