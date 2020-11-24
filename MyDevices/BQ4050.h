//------------------------------------------------------------------------------
//  BQ4050 BMS IC driver
//
//    File: BQ4050.h
//------------------------------------------------------------------------------
#ifndef BQ4050_H_
#define BQ4050_H_

#include "MyI2CM.h"


#define BQ4050_BATTERY_MODE_REG                 0x03
#define  BQ4050_BATTERY_MODE_CAPM_MA                         0
#define  BQ4050_BATTERY_MODE_CAPM_10MW                       BIT(15)
#define  BQ4050_BATTERY_MODE_CHGM_DISABLE_CHARGING_BCASTS    BIT(14)
#define  BQ4050_BATTERY_MODE_AM_DISABLE_ALARM_BCASTS         BIT(13)
#define BQ4050_TEMPERATURE_REG                  0x08
#define BQ4050_VOLTAGE_REG                      0x09
#define BQ4050_CURRENT_REG                      0x0a
#define BQ4050_ABSOLUTE_STATE_OF_CHARGE_REG     0x0e
#define BQ4050_RELATIVE_STATE_OF_CHARGE_REG     0x0d
#define BQ4050_REMAINING_CAPACITY_REG           0x0f
#define BQ4050_DESIGN_CAPACITY_REG              0x18

#define BQ4050_CELL_VOLTAGE4_REG                0x3c
#define BQ4050_CELL_VOLTAGE3_REG                0x3d
#define BQ4050_CELL_VOLTAGE2_REG                0x3e
#define BQ4050_CELL_VOLTAGE1_REG                0x3f

//BQ4050 altal kezelheto cellak szama
#define BQ4050_MAX_CELL_COUNT   4

//------------------------------------------------------------------------------
//BQ4050 valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} BQ4050_t;
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void BQ4050_create(BQ4050_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);

//A BMS IC egyetlen regiszterenek olvasasa
status_t BQ4050_readReg16(BQ4050_t* dev, uint8_t regAddress, uint16_t* data);

//A BMS IC tobb regiszterenek olvasasa
status_t BQ4050_readRegs(BQ4050_t* dev,
                         uint8_t regAddress,
                         uint8_t* buffer,
                         uint32_t length);


//A BMS IC egyetlen regiszterenek irasa
status_t BQ4050_writeReg16(BQ4050_t* dev, uint8_t regAddress, uint16_t regValue);

//Imbalance feszultseg lekerdezese mV-ban.
status_t BQ4050_getImbalance_mv(BQ4050_t* dev, uint16_t *imbalance_mV);

//BMS beallitasa, hogy a toltottseget mA-ba kozolje
status_t BQ4050_setBatteryModeDefaults_mAh(BQ4050_t* dev);

//Homerseklet lekerdezese, 0.1 Kelvin-ben
// Celsius fokba atszamitas: C = (temperatureK - 2731) / 10;
status_t BQ4050_getTemperature(BQ4050_t* dev, uint16_t *temperatureK);

//Akkumulator pakk feszuktseg lekerdezese. mV-ban adja vissza.
status_t BQ4050_getVoltage_mV(BQ4050_t* dev, uint16_t *voltage_mV);

//Akkumulator pakk aramanak lekerdezese. mA-ban adja vissza.
status_t BQ4050_getCurrent_mA(BQ4050_t* dev, int16_t *current_mA);

//Az akkumulator pakk toltottsegi szintjenek kiolvasasa (%-ban adja vissza)
status_t BQ4050_getAbsoluteStateOfCharge(BQ4050_t* dev, uint16_t *percent);

//Az akkumulator pakk relativ toltottseget adja vissza (%-ban adja vissza)
status_t BQ4050_getRelativeStateOfCharge(BQ4050_t* dev, uint16_t *percent);

//A maradek toltes lekerdezese mAh-ban
status_t BQ4050_getRemainingCapacity_mAh(BQ4050_t* dev, uint16_t *capacity_mAh);

//Tervezett kapacitas lekerdezese mAh-ban
status_t BQ4050_getDesignCapacity(BQ4050_t* dev, uint16_t *capacity_mAh);

//Blokkos akkumulator info lekerdezesnel visszaadott struktura
#pragma pack(1)
typedef struct
{
    //Akkumulatorpakk homerseklet lekerdezese, 0.1 Kelvin-ben
    // Celsius fokba atszamitasa: C = (temperatureK - 2731) / 10;
    uint16_t temperatureK;
    //Akkumulator pakk feszultsege mV-ban
    uint16_t voltage_mV;
    //Akkumulator pakk arama mA-ban
    int16_t  current_mA;
    //Akkumulator pakk atlagos arama mA-ban
    int16_t  averageCurrent_mA;
    //This read-word function returns the expected margin of error, in %, in
    //the state-of-charge calculation with a range of 1 to 100%.
    int16_t  maxError;
    //Az akkumulator pakk relativ toltottsege %-ban
    uint16_t relativeStateOfCharge;
    //Az akkumulator pakk toltottsegi szintje %-ban
    uint16_t absoluteStateOfCharge;
    //A maradek toltes mAh-ban
    uint16_t remaingCapacity;
} BQ4050_batteryInfo_t;
#pragma pack()

//Akkumulator jellemzok blokkos lekerdezese.
//A lekerdezesnel kihasznaljuk, hogy a visszaadott jellmzok egymas utani cimen
//helyezkednek el az eszkozben, igy kevesebb buszciklus elegendo a fontosabb
//informaciok lekerdezesehez.
//Megj: A lekerdezes elott ha meg nem volt a BMS ic uzemmodja beallitva arra,
//      hogy a kapacitas adatokat mAh-ba adja vissza, akkor azt meg kell tenni,
//      a BQ4050_setBatteryModeDefaults_mAh() fuggvennyel
status_t BQ4050_getBatteryInfo(BQ4050_t* dev, BQ4050_batteryInfo_t* info);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //BQ4050_H_
