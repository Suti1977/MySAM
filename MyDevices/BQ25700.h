//------------------------------------------------------------------------------
//  BQ25700 akkumulator tolto IC driver
//
//    File: BQ25700.h
//------------------------------------------------------------------------------
#ifndef BQ25700_H_
#define BQ25700_H_

#include "MyI2CM.h"

// ChargeOption0 Register
#define BQ25700_CHARGE_OPTION_0_REG                 0x12

#define BQ25700_CHARGE_OPTION_0_EN_LWPWR            BIT(15)

#define BQ25700_CHARGE_OPTION_0_WDTMR_ADJ_DISABLED  0
#define BQ25700_CHARGE_OPTION_0_WDTMR_ADJ_EN_5SEC   BIT(13)
#define BQ25700_CHARGE_OPTION_0_WDTMR_ADJ_EN_88SEC  BIT(14)
#define BQ25700_CHARGE_OPTION_0_WDTMR_ADJ_EN_175SEC (BIT(13) | BIT(14))

#define BQ25700_CHARGE_OPTION_0_PWM_FREQ_800KHZ     BIT(9)
#define BQ25700_CHARGE_OPTION_0_EN_LEARN            BIT(5)
#define BQ25700_CHARGE_OPTION_0_IADPT_GAIN_40       BIT(4)
#define BQ25700_CHARGE_OPTION_0_IBAT_GAIN_16        BIT(3)
#define BQ25700_CHARGE_OPTION_0_EN_LDO              BIT(2)
#define BQ25700_CHARGE_OPTION_0_EN_IDPM             BIT(1)
#define BQ25700_CHARGE_OPTION_0_CHRG_INHIBIT        BIT(0)

// ChargeCurrent Register
#define BQ25700_CHARGE_CURRENT_REG                  0x14
#define BQ25700_CHARGE_CURRENT_STEP                 64
#define BQ25700_CHARGE_CURRENT_BIT_OFFSET           6

// MaxChargeVoltage Register
#define BQ25700_MAX_CHARGE_VOLTAGE_REG              0x15

// ChargeOption1 Register
#define BQ25700_CHARGE_OPTION_1_REG                 0x30

#define BQ25700_CHARGE_OPTION_1_RSNS_RAC_20         BIT(11)
#define BQ25700_CHARGE_OPTION_1_RSNS_RSR_20         BIT(10)
#define BQ25700_CHARGE_OPTION_1_AUTO_WAKEUP_EN      BIT(0)

// ChargeOption2 Register
#define BQ25700_CHARGE_OPTION_2_REG                 0x31

// ChargeOption3 Register
#define BQ25700_CHARGE_OPTION_3_REG                 0x32
#define BQ25700_CHARGE_OPTION_3_RESET_REG           BIT(14)

// ProchotOption0 Register
#define BQ25700_PROCHOT_OPTION_0_REG                0x33

// ProchotOption1 Register
#define BQ25700_PROCHOT_OPTION_1_REG                0x34

// ADCOption Register
#define BQ25700_ADC_OPTION_REG                      0x35

#define BQ25700_ADC_OPTION_ADC_CONV_ONESHOT         0
#define BQ25700_ADC_OPTION_ADC_CONV_CONT            BIT(15)
#define BQ25700_ADC_OPTION_ADC_START                BIT(14)
#define BQ25700_ADC_OPTION_ADC_FULLSCALE_204V       0
#define BQ25700_ADC_OPTION_ADC_FULLSCALE_306V       BIT(13)
#define BQ25700_ADC_OPTION_EN_ADC_ALL               0xff
#define BQ25700_ADC_OPTION_EN_ADC_CMPIN             BIT(7)
#define BQ25700_ADC_OPTION_EN_ADC_VBUS              BIT(6)
#define BQ25700_ADC_OPTION_EN_ADC_PSYS              BIT(5)
#define BQ25700_ADC_OPTION_EN_ADC_IIN               BIT(4)
#define BQ25700_ADC_OPTION_EN_ADC_IDCHG             BIT(3)
#define BQ25700_ADC_OPTION_EN_ADC_ICHG              BIT(2)
#define BQ25700_ADC_OPTION_EN_ADC_VSYS              BIT(1)
#define BQ25700_ADC_OPTION_EN_ADC_VBAT              BIT(0)

// ChargerStatus Register
#define BQ25700_CHARGER_STATUS_REG                  0x20

#define BQ25700_CHARGE_STATUS_AC_STAT               BIT(15)

// ProchotStatus Register
#define BQ25700_PROCHOT_STATUS_REG                  0x21

// IIN_DPM Register
#define BQ25700_IIN_DPM_REG                         0x22
#define BQ25700_IIN_DPM_STEP                        50
#define BQ25700_IIN_DPM_BIT_OFFSET                  8

// ADCVBUS/PSYS Register
#define BQ25700_ADC_VBUS_PSYS_REG                   0x23
#define BQ25700_ADC_VBUS_STEP_MV                    64
#define BQ25700_ADC_VBUS_BASE_MV                    3200
#define BQ25700_ADC_VBUS_STEP_BIT_OFFSET            8

// ADCIBAT Register
#define BQ25700_ADC_IBAT_REG                        0x24

#define BQ25700_ADC_ICHG_STEP_MA                    64
#define BQ25700_ADC_ICHG_BASE_MA                    0
#define BQ25700_ADC_ICHG_STEP_BIT_OFFSET            8

#define BQ25700_ADC_IDCHG_STEP_MA                   256
#define BQ25700_ADC_IDCHG_BASE_MA                   0
#define BQ25700_ADC_IDCHG_STEP_BIT_OFFSET           0

// ADCIINCMPIN Register
#define BQ25700_ADC_CMPIN_IIN_REG                   0x25
#define BQ25700_ADC_IIN_STEP_MA                     50
// ez a 100 mA offset egy tapasztalati ertek
#define BQ25700_ADC_IIN_BASE_MA                     0 //100
#define BQ25700_ADC_IIN_STEP_BIT_OFFSET             8

// ADCVSYSVBAT Register
#define BQ25700_ADC_VSYS_VBAT_REG                   0x26

#define BQ25700_ADC_VSYS_STEP_MV                    64
#define BQ25700_ADC_VSYS_BASE_MV                    2880
#define BQ25700_ADC_VSYS_STEP_BIT_OFFSET            8

#define BQ25700_ADC_VBAT_STEP_MV                    64
#define BQ25700_ADC_VBAT_BASE_MV                    2880
#define BQ25700_ADC_VBAT_STEP_BIT_OFFSET            0

// OTGVoltage Register
#define BQ25700_OTG_VOLTAGE_REG                     0x3B

// OTGCurrent Register
#define BQ25700_OTG_CURRENT_REG                     0x3C

// InputVoltage Register
#define BQ25700_INPUT_VOLTAGE_REG                   0x3D

#define BQ25700_INPUT_VOLTAGE_BASE_MV               3200

// MinSystemVoltage Register
#define BQ25700_MIN_SYSTEM_VOLTAGE_REG              0x3E
#define BQ25700_MIN_SYSTEM_VOLTAGE_STEP             256
#define BQ25700_MIN_SYSTEM_VOLTAGE_BIT_OFFSET       8


// IIN_HOST Register
#define BQ25700_IIN_HOST_REG                        0x3F

// ManufactureID Register
#define BQ25700_MANUFACTURER_ID_REG                 0xFE

// Device ID (DeviceAddress) Register
#define BQ25700_DEVICE_ADDRESS_REG                  0xFF

// IIN_DPM RegisterS
#define BQ25700_CHARGE_IIN_BIT_0FFSET               8
#define BQ25700_CHARGE_MA_PER_STEP                  50
//------------------------------------------------------------------------------
typedef enum
{
    BQ25700_SERIES_RES_10,
    BQ25700_SERIES_RES_20
} BQ25700_series_resistor_t;
//------------------------------------------------------------------------------
//Driver konfiguracios struktura
typedef struct
{
    //Bemeneti soros arammero ellenallas erteke
    BQ25700_series_resistor_t   rac;
    //Kimeneti soros arammero ellenallas erteke
    BQ25700_series_resistor_t   rsr;
} BQ25700_config_t;
//------------------------------------------------------------------------------
//BQ25700 valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;

    //Bemeneti soros arammero ellenallas erteke
    BQ25700_series_resistor_t   rac;
    //Kimeneti soros arammero ellenallas erteke
    BQ25700_series_resistor_t   rsr;
} BQ25700_t;
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben, tovabba driver beallitasa
void BQ25700_create(BQ25700_t* dev,
                    MyI2CM_t* i2c,
                    uint8_t slaveAddress,
                    const BQ25700_config_t* cfg);

//Az IC tobb regiszterenek olvasasa
status_t BQ25700_readRegs(BQ25700_t* dev,
                          uint8_t regAddress,
                          uint8_t* buffer,
                          uint32_t length);

//Az IC egyetlen 16 bites regiszterenek olvasasa
status_t BQ25700_readReg16(BQ25700_t* dev, uint8_t regAddress, uint16_t* data);

//Az IC egyetlen 16 bites regiszterenek irasa
status_t BQ25700_writeReg16(BQ25700_t* dev, uint8_t regAddress, uint16_t regValue);

status_t BQ25700_get_low_power_mode(BQ25700_t* dev, bool *mode);

status_t BQ25700_set_low_power_mode(BQ25700_t* dev, bool enable);

status_t BQ25700_get_learn_mode(BQ25700_t* dev, bool *mode);

status_t BQ25700_set_learn_mode(BQ25700_t* dev, bool enable);

status_t BQ25700_get_inhibit_mode(BQ25700_t* dev, bool *mode);

status_t BQ25700_set_inhibit_mode(BQ25700_t* dev, bool enable);

status_t BQ25700_get_idpm_mode(BQ25700_t* dev, bool *mode);

status_t BQ25700_set_idpm_mode(BQ25700_t* dev, bool enable);

status_t BQ25700_get_charge_current_limit(BQ25700_t* dev, int *current);

status_t BQ25700_set_charge_current_limit(BQ25700_t* dev, int current_mA);

status_t BQ25700_get_charge_voltage_limit(BQ25700_t* dev, int *voltage_mV);

status_t BQ25700_set_charge_voltage_limit(BQ25700_t* dev, int voltage_mV);

status_t BQ25700_set_input_current_limit(BQ25700_t* dev, int input_current_mA);

status_t BQ25700_get_input_current_limit(BQ25700_t* dev, int *input_current_mA);

status_t BQ25700_get_input_voltage_limit(BQ25700_t* dev, int *voltage_mV);

status_t BQ25700_set_input_voltage_limit(BQ25700_t* dev, int voltage_mV);

status_t BQ25700_get_system_voltage_limit(BQ25700_t* dev, int *voltage_mV);

status_t BQ25700_set_system_voltage_limit(BQ25700_t* dev, uint16_t voltage_mV);

status_t BQ25700_manufacturer_id(BQ25700_t* dev, uint16_t *id);

status_t BQ25700_device_id(BQ25700_t* dev, uint16_t *id);

status_t BQ25700_get_vbus_voltage(BQ25700_t* dev, int *voltage_mV);

status_t BQ25700_get_option_0(BQ25700_t* dev, uint16_t *option);

status_t BQ25700_set_option_0(BQ25700_t* dev, uint16_t option);

status_t BQ25700_get_option_1(BQ25700_t* dev, uint16_t *option);

status_t BQ25700_set_option_1(BQ25700_t* dev, uint16_t option);

status_t BQ25700_get_option_2(BQ25700_t* dev, uint16_t *option);

status_t BQ25700_set_option_2(BQ25700_t* dev, uint16_t option);

status_t BQ25700_get_option_3(BQ25700_t* dev, uint16_t *option);

status_t BQ25700_set_option_3(BQ25700_t* dev, uint16_t option);

status_t BQ25700_get_iin_current(BQ25700_t* dev, int *current_mA);

status_t BQ25700_get_idchg_current(BQ25700_t* dev, int *current_mA);

status_t BQ25700_get_ichg_current(BQ25700_t* dev, int *current_mA);

status_t BQ25700_get_vsys_voltage(BQ25700_t* dev, int *voltage_mV);

status_t BQ25700_get_vbat_voltage(BQ25700_t* dev, int *voltage_mV);

status_t BQ25700_reset(BQ25700_t* dev);
//------------------------------------------------------------------------------
#endif //BQ25700_H_
