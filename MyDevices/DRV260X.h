//------------------------------------------------------------------------------
//  DRV260X haptic meghajto ic driver
//
//    File: DRV260X.h
//------------------------------------------------------------------------------
#ifndef DRV260X_H_
#define DRV260X_H_

#include "MyI2CM.h"

//Registers
#define DRV260X_STATUS_REG              0x00
#define DRV260X_MODE_REG                0x01
#define DRV260X_RT_PB_IN_REG            0x02
#define DRV260X_LIB_SEL_REG             0x03
#define DRV260X_WAV_SEQ_1_REG           0x04
#define DRV260X_WAV_SEQ_2_REG           0x05
#define DRV260X_WAV_SEQ_3_REG           0x06
#define DRV260X_WAV_SEQ_4_REG           0x07
#define DRV260X_WAV_SEQ_5_REG           0x08
#define DRV260X_WAV_SEQ_6_REG           0x09
#define DRV260X_WAV_SEQ_7_REG           0x0a
#define DRV260X_WAV_SEQ_8_REG           0x0b
#define DRV260X_GO_REG                  0x0c
#define DRV260X_OVERDRIVE_OFFS_REG      0x0d
#define DRV260X_SUST_POS_OFFS_REG       0x0e
#define DRV260X_SUST_NEG_OFFS_REG       0x0f
#define DRV260X_BRAKE_OFFS_REG          0x10
#define DRV260X_A_TO_V_CTRL_REG         0x11
#define DRV260X_A_TO_V_MIN_INPUT_REG	0x12
#define DRV260X_A_TO_V_MAX_INPUT_REG	0x13
#define DRV260X_A_TO_V_MIN_OUT_REG      0x14
#define DRV260X_A_TO_V_MAX_OUT_REG      0x15
#define DRV260X_RAT_VOL_REG		        0x16
#define DRV260X_ODC_VOL_REG	            0x17
#define DRV260X_CAL_COMP_RES_REG		0x18
#define DRV260X_CAL_BACK_EMF_RES_REG	0x19
#define DRV260X_FB_CTRL_REG             0x1a
#define DRV260X_CTRL1_REG               0x1b
#define DRV260X_CTRL2_REG               0x1c
#define DRV260X_CTRL3_REG               0x1d
#define DRV260X_CTRL4_REG               0x1e
#define DRV260X_CTRL5_REG               0x1f
#define DRV260X_LRA_LOOP_PERIOD_REG     0x20
#define DRV260X_VBAT_MON_REG            0x21
#define DRV260X_LRA_RES_PERIOD_REG      0x22
#define DRV260X_MAX_REG                 0x23

// Register bits, fields

// #define DRV260X_STATUS_REG		0x00
#define DRV260X_STATUS_DEVICE_ID_START      BIT(5)
#define DRV260X_STATUS_DEVICE_ID_LEN        BIT(3)
#define DRV260X_STATUS_DIAG_RESULT          BIT(3)
#define DRV260X_STATUS_OVER_TEMP            BIT(1)
#define DRV260X_STATUS_OC_DETECT            BIT(0)

// #define DRV260X_MODE_REG		0x01
#define DRV260X_MODE_DEV_RESET              BIT(7)
#define DRV260X_MODE_STANDBY                BIT(6)
#define DRV260X_MODE_MODE_START             0
#define DRV260X_MODE_MODE_LEN               3
#define DRV260X_MODE_MODE_INTTRIG           0
#define DRV260X_MODE_MODE_EXTEDGETRIG       1
#define DRV260X_MODE_MODE_DIAG              6
#define DRV260X_MODE_MODE_AUTOCAL           7

// #define DRV260X_LIB_SEL_REG		0x03
#define DRV260X_LIB_SEL_HI_Z                BIT(4)
#define DRV260X_LIB_SEL_LIBRARY_SEL_START   0
#define DRV260X_LIB_SEL_LIBRARY_SEL_LEN     3
#define DRV260X_LIB_SEL_LIBRARY_SEL_EMPTY   BIT(0)
#define DRV260X_LIB_SEL_LIBRARY_SEL_LIB_A   BIT(1)
#define DRV260X_LIB_SEL_LIBRARY_SEL_LIB_B   BIT(2)
#define DRV260X_LIB_SEL_LIBRARY_SEL_LIB_C   BIT(3)
#define DRV260X_LIB_SEL_LIBRARY_SEL_LIB_D   BIT(4)
#define DRV260X_LIB_SEL_LIBRARY_SEL_LIB_E   BIT(5)
#define DRV260X_LIB_SEL_LIBRARY_SEL_LRA_LIB BIT(6)
#define DRV260X_LIB_SEL_LIBRARY_SEL_LIB_F   BIT(7)

// #define DRV260X_WAV_SEQ_1_REG	0x04
// ..
// #define DRV260X_WAV_SEQ_8_REG	0x04
#define DRV260X_WAV_SEQ_X_WAIT              BIT(7)
#define DRV260X_WAV_SEQ_X_WAV_FRM_SEQ_START 0
#define DRV260X_WAV_SEQ_X_WAV_FRM_SEQ_LEN   7
#define DRV260X_WAV_SEQ_X_WAIT_TIME_START   0
#define DRV260X_WAV_SEQ_X_WAIT_TIME_LEN     7

// #define DRV260X_GO_REG				0x0c
#define DRV260X_GO_GO                       0

// #define DRV260X_RAT_VOL_REG		        0x16
#define DRV260X_RAT_VOL_START               0
#define DRV260X_RAT_VOL_LEN                 8

// #define DRV260X_ODC_VOL_REG		        0x17
#define DRV260X_ODC_VOL_START               0
#define DRV260X_ODC_VOL_LEN                 8
//------------------------------------------------------------------------------
//DRV260X valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} DRV260X_t;
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void DRV260X_create(DRV260X_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);


//Az IC olvasasa
status_t DRV260X_readReg(DRV260X_t* dev,
                         uint8_t address,
                         uint8_t* buff);

//Az IC egy 16 bites regiszterenek irasa
status_t DRV260X_writeReg(DRV260X_t* dev, uint8_t address, uint8_t regValue);

//address altal mutatott regiszterben bitek beallitasa/torlese
//clrBitMask szerinti bitek torlesre kerulnek
//setBitMask szerinti bitek beallitasra kerulnek
status_t DRV260X_setReg(DRV260X_t* dev,
                        uint8_t address,
                        uint8_t clrBitMask,
                        uint8_t setBitMask);

//Statusz regiszter olvasasa
status_t DRV260X_getStatus(DRV260X_t* dev, uint8_t* statusRegValue);

//Eszkoz szoftveres resetelese
status_t DRV260X_reset(DRV260X_t* dev);

//Eszkoz standby-ba helyezese/ebresztese
status_t DRV260X_setStandby(DRV260X_t* dev, bool standby);

//Chip inicializalasahoz struktura.
typedef struct
{
    uint8_t ratedVoltage;
    uint8_t odcVoltage;
    uint8_t feedbackControl;
    uint8_t ctrl1;
    uint8_t ctrl2;
    uint8_t ctrl3;
    uint8_t ctrl4;
    uint8_t lib;
} DRV260X_config_t;
//Chip/motor konfiguralasa
status_t DRV260X_config(DRV260X_t* dev, const DRV260X_config_t* cfg);

//Haptic kalibracioja. A rutin nem ter vissza addig, amig a kalibracio be nem
//fejezodik.
status_t DRV260X_calibrate(DRV260X_t* dev);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //DRV260X_H_
