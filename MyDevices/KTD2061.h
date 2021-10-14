//------------------------------------------------------------------------------
//  KTD2061/58/59/60 driver
//
//    File: KTD2061.h
//------------------------------------------------------------------------------
#ifndef KTD2061_H_
#define KTD2061_H_

#include "MyI2CM.h"

#define KTD2061_REG_ID          0x00
#define KTD2061_REG_MONITOR     0x01
#define KTD2061_REG_CONTROL     0x02
#define KTD2061_REG_IRED0       0x03
#define KTD2061_REG_IGRN0       0x04
#define KTD2061_REG_IBLU0       0x05
#define KTD2061_REG_IRED1       0x06
#define KTD2061_REG_IGRN1       0x07
#define KTD2061_REG_IBLU1       0x08
#define KTD2061_REG_ISELA12     0x09
#define KTD2061_REG_ISELA34     0x0A
#define KTD2061_REG_ISELB12     0x0B
#define KTD2061_REG_ISELB34     0x0C
#define KTD2061_REG_ISELC12     0x0D
#define KTD2061_REG_ISELC34     0x0E

//global off request (fade all LEDs to zero and then shutdown)
#define KTD2061_CONTROL_ENMODE_GLOBAL_OFFval    0
#define KTD2061_CONTROL_ENMODE_GLOBAL_OFF       (0 << 6 )
//enable Night Mode (0 to 1.5mA range)
#define KTD2061_CONTROL_ENMODE_NIGHT_MODEval    1
#define KTD2061_CONTROL_ENMODE_NIGHT_MODE       (1 << 6 )
//enable Normal Mode (0 to 24mA range)
#define KTD2061_CONTROL_ENMODE_NORMAL_MODEval   2
#define KTD2061_CONTROL_ENMODE_NORMAL_MODE      (2 << 6 )
//reset all registers to default settings
#define KTD2061_CONTROL_ENMODE_RESETval         3
#define KTD2061_CONTROL_ENMODE_RESET            (3 << 6 )

//CoolExtend (TM) Temperature Settings [celsius]
#define KTD2061_CONTROL_COOL_EXTEND_135val      0
#define KTD2061_CONTROL_COOL_EXTEND_135         (0 << 3 )
#define KTD2061_CONTROL_COOL_EXTEND_120val      1
#define KTD2061_CONTROL_COOL_EXTEND_120         (1 << 3 )
#define KTD2061_CONTROL_COOL_EXTEND_105val      2
#define KTD2061_CONTROL_COOL_EXTEND_105         (2 << 3 )
#define KTD2061_CONTROL_COOL_EXTEND_90val       3
#define KTD2061_CONTROL_COOL_EXTEND_90          (3 << 3 )

//Fade Rate Exponential Time-Constant Setting [msec]
#define KTD2061_CONTROL_FADE_RATE_31val         0
#define KTD2061_CONTROL_FADE_RATE_31            0
#define KTD2061_CONTROL_FADE_RATE_63val         1
#define KTD2061_CONTROL_FADE_RATE_63            1
#define KTD2061_CONTROL_FADE_RATE_125val        2
#define KTD2061_CONTROL_FADE_RATE_125           2
#define KTD2061_CONTROL_FADE_RATE_250val        3
#define KTD2061_CONTROL_FADE_RATE_250           3
#define KTD2061_CONTROL_FADE_RATE_500val        4
#define KTD2061_CONTROL_FADE_RATE_500           4
#define KTD2061_CONTROL_FADE_RATE_1000val       5
#define KTD2061_CONTROL_FADE_RATE_1000          5
#define KTD2061_CONTROL_FADE_RATE_2000val       6
#define KTD2061_CONTROL_FADE_RATE_2000          6
#define KTD2061_CONTROL_FADE_RATE_4000val       7
#define KTD2061_CONTROL_FADE_RATE_4000          7


typedef union
{
    struct
    {
        uint8_t die_id     :5;
        uint8_t vendor     :3;
    } bit;
    uint8_t reg;
} KTD2061_id_t;

typedef union
{
    struct
    {
        uint8_t uv_ot_stat :1;
        uint8_t ce_stat    :1;
        uint8_t be_stat    :1;
        uint8_t sc_stat    :1;
        uint8_t die_rev    :4;
    } bit;
    uint8_t reg;
} KTD2061_monitor_t;

typedef union
{
    struct
    {
        uint8_t fade_rate   :3;
        uint8_t ce_temp     :2;
        uint8_t be_en       :1;
        uint8_t en_mode     :2;
    } bit;
    uint8_t reg;
} KTD2061_control_t;

typedef union
{
    struct
    {
        uint8_t sel2_4      :3;
        uint8_t en2_4       :1;
        uint8_t sel1_3      :3;
        uint8_t en1_3       :1;
    } bit;
    uint8_t reg;
} KTD2061_isel_t;

//------------------------------------------------------------------------------
//KTD2061 valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} KTD2061_t;
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void KTD2061_create(KTD2061_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);

//Az IC blokkos olvasasa
status_t KTD2061_read(KTD2061_t* dev,
                      uint8_t address,
                      uint8_t* buff,
                      uint8_t length);

//Az IC blokkos irasa
status_t KTD2061_write(KTD2061_t* dev,
                      uint8_t address,
                      uint8_t* buff,
                      uint8_t length);

//Az IC egy 8 bites regiszterenek irasa
status_t KTD2061_writeReg(KTD2061_t* dev, uint8_t address, uint8_t regValue);

//Az IC egy 8 bites regiszterenek olvasasa
status_t KTD2061_readReg(KTD2061_t* dev, uint8_t address, uint8_t* regValue);

//Az ID olvasasa
status_t KTD2061_readID(KTD2061_t* dev, KTD2061_id_t* id);

//Monitor regiszter olvasasa
status_t KTD2061_readMonitor(KTD2061_t* dev, KTD2061_monitor_t* monitor);

//control regiszter olvasasa
status_t KTD2061_readControl(KTD2061_t* dev, KTD2061_control_t* control);
//control regiszter irasa
status_t KTD2061_writeControl(KTD2061_t* dev, KTD2061_control_t control);

//Chip reset. Minden regiszter alapertelmezesbe all.
status_t KTD2061_reset(KTD2061_t* dev);

//enable mod kijelolese
//KTD2061_CONTROL_ENMODE_XXXXXval szerint
status_t KTD2061_setEnableMode(KTD2061_t* dev, uint8_t enMode);

//BrightExtend mod be/ki kapcsolasa
status_t KTD2061_setBrightExtendMode(KTD2061_t* dev, uint8_t enable);

//CoolExtend mod beallitasa
//KTD2061_CONTROL_COOL_EXTEND_XXXval szerint
status_t KTD2061_setColExtendTemperature(KTD2061_t* dev, uint8_t ceTempValue);

//fade rate beallitasa
//KTD2061_CONTROL_FADE_RATE_XXXXXval szerint
status_t KTD2061_setFadeRate(KTD2061_t* dev, uint8_t fadeRateVal);


//Color 0 beallitasa
status_t KTD2061_setColor0(KTD2061_t* dev, uint8_t r, uint8_t g, uint8_t b);
//Color 1 beallitasa
status_t KTD2061_setColor1(KTD2061_t* dev, uint8_t r, uint8_t g, uint8_t b);

//------------------------------------------------------------------------------
#endif //KTD2061_H_
