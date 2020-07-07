//------------------------------------------------------------------------------
//  EMC1815 remote diode sensore driver
//
//    File: EMC1815.h
//------------------------------------------------------------------------------
#ifndef EMC1815_H_
#define EMC1815_H_

#include "MyI2CM.h"

#define EMC1815_ADDR_STATUS                         0x02
#define EMC1815_STATUS_ROCF         128
#define EMC1815_STATUS_HOTCHG       64
#define EMC1815_STATUS_BUSY         32
#define EMC1815_STATUS_HIGH         16
#define EMC1815_STATUS_LOW          8
#define EMC1815_STATUS_FAULT        4
#define EMC1815_STATUS_ETHRM        2
#define EMC1815_STATUS_ITHRM        1

#define EMC1815_ADDR_CONFIG                         0x03
#define EMC1815_CONFIG_MSKAL        128
#define EMC1815_CONFIG_RS           64
#define EMC1815_CONFIG_AT_THM       32
#define EMC1815_CONFIG_RECD1_2      16
#define EMC1815_CONFIG_RECD3_4      8
#define EMC1815_CONFIG_RANGE        4
#define EMC1815_CONFIG_DA_ENA       2
#define EMC1815_CONFIG_APDD         1


#define EMC1815_ADDR_CONVERT                        0x04

#define EMC1815_ADDR_INT_DIODE_HIGH_LIMIT           0x05
#define EMC1815_ADDR_INT_DIODE_LOW_LIMIT            0x06

#define EMC1815_ADDR_EXT1_HIGH_LIMIT_HIGH_BYTE      0x07
#define EMC1815_ADDR_EXT1_LOW_LIMIT_HIGH_BYTE       0x08

#define EMC1815_ADDR_ONE_SHOT                       0x0F

#define EMC1815_ADDR_SCRTCHPD1                      0x11
#define EMC1815_ADDR_SCRTCHPD2                       0x12

#define EMC1815_ADDR_EXT1 HIGH_LIMIT_LOW_BYTE       0x13
#define EMC1815_ADDR_EXT1_LOW_LIMIT_LOW_BYTE        0x14
#define EMC1815_ADDR_EXT2_HIGH_LIMIT_HIGH_BYTE      0x15
#define EMC1815_ADDR_EXT2_LOW_LIMIT_HIGH_BYTE       0x16
#define EMC1815_ADDR_EXT2_HIGH_LIMIT_LOW_BYTE       0x17
#define EMC1815_ADDR_EXT2_LOW_LIMIT_LOW_BYTE        0x18
#define EMC1815_ADDR_EXT1_THERM_LIMIT               0x19
#define EMC1815_ADDR_EXT2_THERM_LIMIT               0x1A
#define EMC1815_ADDR_EXTERNAL_DIODE_FAULT_STATUS    0x1B
#define EMC1815_ADDR_DIODE_FAULT_MASK               0x1F
#define EMC1815_ADDR_INT_DIODE_THERM_LIMIT          0x20
#define EMC1815_ADDR_THRM_HYS                       0x21
#define EMC1815_ADDR_CONSEC_ALERT                   0x22

#define EMC1815_ADDR_EXT1_BETA_CONFIG               0x25
#define EMC1815_ADDR_EXT2_BETA_CONFIG               0x26
#define EMC1815_ADDR_EXT1_IDEALITY_FACTOR           0x27
#define EMC1815_ADDR_EXT2_IDEALITY_FACTOR           0x28

#define EMC1815_ADDR_EXT3_HIGH_LIMIT_HIGH_BYTE      0x2C
#define EMC1815_ADDR_EXT3_LOW_LIMIT_HIGH_BYTE       0x2D
#define EMC1815_ADDR_EXT3_HIGH_LIMIT_LOW_BYTE       0x2E
#define EMC1815_ADDR_EXT3_LOW_LIMIT_LOW_BYTE        0x2F
#define EMC1815_ADDR_EXT3_THERM_LIMIT               0x30
#define EMC1815_ADDR_EXT3_IDEALITY_FACTOR           0x31

#define EMC1815_ADDR_EXT4_HIGH_LIMIT_HIGH_BYTE      0x34
#define EMC1815_ADDR_EXT4_LOW_LIMIT_HIGH_BYTE       0x35
#define EMC1815_ADDR_EXT4_HIGH_LIMIT_LOW_BYTE       0x36
#define EMC1815_ADDR_EXT4_LOW_LIMIT_LOW_BYTE        0x37
#define EMC1815_ADDR_EXT4_THERM_LIMIT               0x38
#define EMC1815_ADDR_EXT4_IDEALITY_FACTOR           0x39

#define EMC1815_ADDR_HIGH_LIMIT_STATUS              0x3A
#define EMC1815_ADDR_LOW_LIMIT_STATUS               0x3B
#define EMC1815_ADDR_THERM_LIMIT_STATUS             0x3C

#define EMC1815_ADDR_ROC_GAIN                       0x3D
#define EMC1815_ADDR_ROC_CONFIG                     0x3E
#define EMC1815_ADDR_ROC_STATUS                     0x3F
#define EMC1815_ADDR_R1_RESH                        0x40
#define EMC1815_ADDR_R1_LIMH                        0x41
#define EMC1815_ADDR_R1_LIML                        0x42
#define EMC1815_ADDR_R1_SMPL                        0x43
#define EMC1815_ADDR_R2_RESH                        0x44
#define EMC1815_ADDR_R2_3_RES                       0x45
#define EMC1815_ADDR_R2_LIMH                        0x46
#define EMC1815_ADDR_R2_LIML                        0x47
#define EMC1815_ADDR_R2_SMPL                        0x48
#define EMC1815_ADDR_PER_MAXTH                      0x49
#define EMC1815_ADDR_PER_MAXT1L                     0x4A
#define EMC1815_ADDR_PER_MAXT2_3L                   0x4C
#define EMC1815_ADDR_GBL_MAXT1H                     0x4D
#define EMC1815_ADDR_GBL_MAXT1L                     0x4E
#define EMC1815_ADDR_GBL_MAXT2H                     0x4F
#define EMC1815_ADDR_GBL_MAXT2L                     0x50

#define EMC1815_ADDR_FILTER_SEL                     0x51

#define EMC1815_ADDR_INT_HIGH_BYTE                  0x60
#define EMC1815_ADDR_INT_LOW_BYTE                   0x61
#define EMC1815_ADDR_EXT1_HIGH_BYTE                 0x62
#define EMC1815_ADDR_EXT1_LOW_BYTE                  0x63
#define EMC1815_ADDR_EXT2_HIGH_BYTE                 0x64
#define EMC1815_ADDR_EXT2_LOW_BYTE                  0x65
#define EMC1815_ADDR_EXT3_HIGH_BYTE                 0x66
#define EMC1815_ADDR_EXT3_LOW_BYTE                  0x67
#define EMC1815_ADDR_EXT4_HIGH_BYTE                 0x68
#define EMC1815_ADDR_EXT4_LOW_BYTE                  0x69

#define EMC1815_ADDR_HOTTEST_DIODE_HIGH_BYTE        0x6A
#define EMC1815_ADDR_HOTTEST_DIODE_LOW_BYTE         0x6B
#define EMC1815_ADDR_HOTTEST_STATUS                 0x6C
#define EMC1815_ADDR_HOTTEST_CONFIG                 0x6D

#define EMC1815_ADDR_PRODUCT_ID                     0xFD
#define EMC1815_ADDR_MANUFACTURER_ID                0xFE
#define EMC1815_ADDR_REVISION                       0xFF



////////////
/*
  0x02 STATUS 7:0
    ROCF HOTCHG BUSY HIGH LOW FAULT ETHRM ITHRM



  0x03 CONFIG 7:0
    MSKAL R/S AT/THM RECD1/2 RECD3/4 RANGE DA_ENA APDD

  0x1B EXTERNAL_DIODE_FAULT_STATUS
  7:0 E4FLT E3FLT E2FLT E1FLT

  0x1F DIODE_FAULT_MASK
  7:0 E4MASK E3MASK E2MASK E1MASK INTMASK

  0x22 CONSEC_ALERT
    TMOUT CTHRM[2:0] CALRT[2:0]

  0x25 EXT1_BETA_CONFIG
    ENBL(N) BETA(N)[3:0]
  0x26 EXT2_BETA_CONFIG
  7:0 ENBL(N) BETA(N)[3:0]
  0x27 EXT1_IDEALITY_FACTOR
  7:0 IDEAL(N)[5:0]
  0x28 EXT2_IDEALITY_FACTOR
  7:0 IDEAL(N)[5:0]
  0x29 INT_LOW_BYTE 7:0 ILB[2:0]
  0x2A EXT3_HIGH_BYTE 7:0 EXT(N)HB[7:0]
  0x2B EXT3_LOW_BYTE 7:0 EXT(N)LB[2:0]
  0x2C EXT3_HIGH_LIMIT_HIGH_BYTE
  7:0 EXT(N)HLHB[7:0]
  0x2D EXT3_LOW_LIMIT_HIGH_BYTE
  7:0 EXT(N)LLHB[7:0]
  0x2E EXT3_HIGH_LIMIT_LOW_BYTE
  7:0 EXT(N)HLLB[2:0]
  0x2F EXT3_LOW_LIMIT_LOW_BYTE
  7:0 EXT(N)LLLB[2:0]
  0x30 EXT3_THERM_LIMIT 7:0 EXT(N)THL[7:0]
  0x31 EXT3_IDEALITY_FACTOR
  7:0 IDEAL(N)[5:0]
  0x32 EXT4_HIGH_BYTE 7:0 EXT(N)HB[7:0]
  0x33 EXT4_LOW_BYTE 7:0 EXT(N)LB[2:0]
  0x34 EXT4_HIGH_LIMIT_HIGH_BYTE
  7:0 EXT(N)HLHB[7:0]
  0x35 EXT4_LOW_LIMIT_HIGH_BYTE
  7:0 EXT(N)LLHB[7:0]
  0x36 EXT4_HIGH_LIMIT_LOW_BYTE
  7:0 EXT(N)HLLB[2:0]
  0x37 EXT4_LOW_LIMIT_LOW_BYTE
  7:0 EXT(N)LLLB[2:0]
  0x38 EXT4_THERM_LIMIT 7:0 EXT(N)THL[7:0]
  0x39 EXT4_IDEALITY_FACTOR
  7:0 IDEAL(N)[5:0]
  0x3A HIGH_LIMIT_STATUS
  7:0 E4HIGH E3HIGH E2HIGH E1HIGH IHIGH
  0x3B LOW_LIMIT_STATUS 7:0 E4LOW E3LOW E2LOW E1LOW ILOW
  0x3C THERM_LIMIT_STATUS
  7:0 E4THERM E3THERM E2THERM E1THERM ITHERM
  0x3D ROC_GAIN 7:0 RC1G[7:0]
  0x3E ROC_CONFIG 7:0 EN_ROC MASK2/3 MASK1 RCHY[2:0]
  0x3F ROC_STATUS 7:0 SLCG2/3 SLCG1 R2/3ODD R1ODD RC2/3HI RC1HI RC2/3LO RC1LO
  0x40 R1_RESH 7:0 R(N)RH[7:0]
  0x41 R1_LIMH 7:0 R(N)LIMH[7:0]
  0x42 R1_LIML 7:0 R(N)LIML[3:0]
  0x43 R1_SMPL 7:0 R(N)SH[3:0]
  0x44 R2_RESH 7:0 R(N)RH[7:0]
  0x45 R2_3_RESL 7:0 R2/3_RL[3:0] R1_RL[3:0]
  0x46 R2_LIMH 7:0 R(N)LIMH[7:0]
  0x47 R2_LIML 7:0 R(N)LIML[3:0]
  0x48 R2_SMPL 7:0 R(N)SH[3:0]
  0x49 PER_MAXTH 7:0 GM(N)HB[7:0]
  0x4A PER_MAXT1L 7:0 PM(N)L[2:0]
  0x4B PER_MAXTH 7:0 GM(N)HB[7:0]
  0x4C PER_MAXT2/3L 7:0 PM(N)L[2:0]
  0x4D GBL_MAXT1H 7:0 GM(N)HB[7:0]
  0x4E GBL_MAXT1L 7:0 GM(N)LB[2:0]
  0x4F GBL_MAXT2H 7:0 GM(N)HB[7:0]
  0x50 GBL_MAXT2L 7:0 GM(N)LB[2:0]
  0x51 FILTER_SEL 7:0 FILTER[1:0]
  0x60 INT_HIGH_BYTE 7:0 IHB[7:0]
  0x61 INT_LOW_BYTE 7:0 ILB[2:0]
  0x62 EXT1_HIGH_BYTE 7:0 EXT(N)HB[7:0]
  0x63 EXT1_LOW_BYTE 7:0 EXT(N)LB[2:0]
  0x64 EXT2_HIGH_BYTE 7:0 EXT(N)HB[7:0]
  0x65 EXT2_LOW_BYTE 7:0 EXT(N)LB[2:0]
  0x66 EXT3_HIGH_BYTE 7:0 EXT(N)HB[7:0]
  0x67 EXT3_LOW_BYTE 7:0 EXT(N)LB[2:0]
  0x68 EXT4_HIGH_BYTE 7:0 EXT(N)HB[7:0]
  0x69 EXT4_LOW_BYTE 7:0 EXT(N)LB[2:0]
  0x6A HOTTEST_DIODE_HIGH_BYTE
  7:0 HDHB[7:0]
  0x6B HOTTEST_DIODE_LOW_BYTE
  7:0 HDLB[2:0]
  0x6C HOTTEST_STATUS 7:0 E4HOT E3HOT E2HOT E1HOT IHOT
  0x6D HOTTEST_CONFIG
*/


//------------------------------------------------------------------------------
//EMC1815 valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
    //true, ha a homerseklet adatokat a nagy felbontasu es eltolt modban kell
    //ertelmezni.
    bool range;
} EMC1815_t;
//------------------------------------------------------------------------------
typedef struct
{
    //Disables the Resistance Error Correction (REC) for the DP1/DN1 pins.
    bool RECD1_2;
    //Disables the Resistance Error Correction (REC) for the DP2/DN2 pins.
    bool RECD3_4;
    //Configures the measurement range and data format
    //false:  0°C to +127.875°C and the data format is binary
    //true: -64°C to +191.875°C and the data format is offset binary
    bool range;
    //Enables the dynamic averaging feature on all temperature channels
    bool dynamicFiltering;
    //Disables the anti-parallel diode operation only allowing each APD pin
    //set to bias and measure one diode.
    bool disableAntiParalelDiodeMode;

    //Conversion rate
    //0: 1/16
    //1: 1/8
    //2: 1/4
    //3: 1/2
    //4: 1
    //5: 2
    //6: 4
    //7: 8
    //8: 16
    //9: 32
    //10:64
    //others: 1
    uint8_t convRate;

    //filter setting
    //0: disabled
    //1: Level1
    //3: Level2
    uint8_t filter;
} EMC1815_Config_t;
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

//EMC1815 konfiguralasa
status_t EMC1815_config(EMC1815_t* dev,
                        const EMC1815_Config_t* cfg);

//EMC1815 homerseklet kiolvasasa.
//channel 0-3: a 4 kezelt kulso dioda
//        4-: belso homerseklet
status_t EMC1815_readTemperatureValue(EMC1815_t* dev,
                                      uint8_t channel,
                                      uint16_t* value);

//binaris ertekbol Celsius fokba konvertalas.
double EMC1815_value2Celsius(EMC1815_t* dev, uint16_t value);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //EMC1815_H_
