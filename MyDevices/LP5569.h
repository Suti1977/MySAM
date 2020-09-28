//------------------------------------------------------------------------------
//  LP5569 I2C-s LED meghajto driver
//
//    File: LP5569.h
//------------------------------------------------------------------------------
#ifndef LP5569_H_
#define LP5569_H_

#include "MyI2CM.h"
#include "MyCommon.h"


#define LP5569_REG_CONFIG              0x00    //Configuration Register
#define LP5569_CONFIG_CHIP_EN          BIT(6)
#define LP5569_REG_LED_ENGINE_CONTROL1 0x01    //Engine Execution Control Register
#define LP5569_REG_LED_ENGINE_CONTROL2 0x02    //Engine Operation Mode Register
#define LP5569_REG_LED0_CONTROL        0x07    //LED0 Control Register
#define LP5569_REG_LED1_CONTROL        0x08    //LED1 Control Register
#define LP5569_REG_LED2_CONTROL        0x09    //LED2 Control Register
#define LP5569_REG_LED3_CONTROL        0x0A    //LED3 Control Register
#define LP5569_REG_LED4_CONTROL        0x0B    //LED4 Control Register
#define LP5569_REG_LED5_CONTROL        0x0C    //LED5 Control Register
#define LP5569_REG_LED6_CONTROL        0x0D    //LED6 Control Register
#define LP5569_REG_LED7_CONTROL        0x0E    //LED7 Control Register
#define LP5569_REG_LED8_CONTROL        0x0F    //LED8 Control Register
#define LP5569_REG_LED0_PWM            0x16    //LED0 PWM Duty Cycle
#define LP5569_REG_LED1_PWM            0x17    //LED1 PWM Duty Cycle
#define LP5569_REG_LED2_PWM            0x18    //LED2 PWM Duty Cycle
#define LP5569_REG_LED3_PWM            0x19    //LED3 PWM Duty Cycle
#define LP5569_REG_LED4_PWM            0x1A    //LED4 PWM Duty Cycle
#define LP5569_REG_LED5_PWM            0x1B    //LED5 PWM Duty Cycle
#define LP5569_REG_LED6_PWM            0x1C    //LED6 PWM Duty Cycle
#define LP5569_REG_LED7_PWM            0x1D    //LED7 PWM Duty Cycle
#define LP5569_REG_LED8_PWM            0x1E    //LED8 PWM Duty Cycle
#define LP5569_REG_LED0_CURRENT        0x22    //LED0 Current Control
#define LP5569_REG_LED1_CURRENT        0x23    //LED1 Current Control
#define LP5569_REG_LED2_CURRENT        0x24    //LED2 Current Control
#define LP5569_REG_LED3_CURRENT        0x25    //LED3 Current Control
#define LP5569_REG_LED4_CURRENT        0x26    //LED4 Current Control
#define LP5569_REG_LED5_CURRENT        0x27    //LED5 Current Control
#define LP5569_REG_LED6_CURRENT        0x28    //LED6 Current Control
#define LP5569_REG_LED7_CURRENT        0x29    //LED7 Current Control
#define LP5569_REG_LED8_CURRENT        0x2A    //LED8 Current Control
#define LP5569_REG_MISC                0x2F    //I2C, Charge Pump and Clock Configuration
#define LP5569_REG_ENGINE1_PC          0x30    //Engine1 Program Counter
#define LP5569_REG_ENGINE2_PC          0x31    //Engine2 Program Counter
#define LP5569_REG_ENGINE3_PC          0x32    //Engine3 Program Counter
#define LP5569_REG_MISC2               0x33    //Charge Pump and LED Configuration
#define LP5569_REG_ENGINE_STATUS       0x3C    //Engine 1, 2 & 3 Status
#define LP5569_REG_IO_CONTROL          0x3D    //TRIG, INT and CLK Configuration
#define LP5569_REG_VARIABLE_D          0x3E    //Global Variable D
#define LP5569_REG_RESET               0x3F    //Software Reset
#define LP5569_REG_ENGINE1_VARIABLE_A  0x42    //Engine 1 Local Variable A
#define LP5569_REG_ENGINE2_VARIABLE_A  0x43    //Engine 2 Local Variable A
#define LP5569_REG_ENGINE3_VARIABLE_A  0x44    //Engine 3 Local Variable A
#define LP5569_REG_MASTER_FADER1       0x46    //Engine 1 Master Fader
#define LP5569_REG_MASTER_FADER2       0x47    //Engine 2 Master Fader
#define LP5569_REG_MASTER_FADER3       0x48    //Engine 3 Master Fader
#define LP5569_REG_MASTER_FADER_PWM    0x4A    //PWM Input Duty Cycle
#define LP5569_REG_ENGINE1_PROG_START  0x4B    //Engine 1 Program Starting Address
#define LP5569_REG_ENGINE2_PROG_START  0x4C    //Engine 2 Program Starting Address
#define LP5569_REG_ENGINE3_PROG_START  0x4D    //Engine 2 Program Starting Address
#define LP5569_REG_PROG_MEM_PAGE_SELECT0x4F    //Program Memory Page Select
#define LP5569_REG_PROGRAM_MEM_00      0x50    //MSB 0
#define LP5569_REG_PROGRAM_MEM_01      0x51    //LSB 0
#define LP5569_REG_PROGRAM_MEM_02      0x52    //MSB 1
#define LP5569_REG_PROGRAM_MEM_03      0x53    //LSB 1
#define LP5569_REG_PROGRAM_MEM_04      0x54    //MSB 2
#define LP5569_REG_PROGRAM_MEM_05      0x55    //LSB 2
#define LP5569_REG_PROGRAM_MEM_06      0x56    //MSB 3
#define LP5569_REG_PROGRAM_MEM_07      0x57    //LSB 3
#define LP5569_REG_PROGRAM_MEM_08      0x58    //MSB 4
#define LP5569_REG_PROGRAM_MEM_09      0x59    //LSB 4
#define LP5569_REG_PROGRAM_MEM_10      0x5A    //MSB 5
#define LP5569_REG_PROGRAM_MEM_11      0x5B    //LSB 5
#define LP5569_REG_PROGRAM_MEM_12      0x5C    //MSB 6
#define LP5569_REG_PROGRAM_MEM_13      0x5D    //LSB 6
#define LP5569_REG_PROGRAM_MEM_14      0x5E    //MSB 7
#define LP5569_REG_PROGRAM_MEM_15      0x5F    //LSB 7
#define LP5569_REG_PROGRAM_MEM_16      0x60    //MSB 8
#define LP5569_REG_PROGRAM_MEM_17      0x61    //LSB 8
#define LP5569_REG_PROGRAM_MEM_18      0x62    //MSB 9
#define LP5569_REG_PROGRAM_MEM_19      0x63    //LSB 9
#define LP5569_REG_PROGRAM_MEM_20      0x64    //MSB 10
#define LP5569_REG_PROGRAM_MEM_21      0x65    //LSB 10
#define LP5569_REG_PROGRAM_MEM_22      0x66    //MSB 11
#define LP5569_REG_PROGRAM_MEM_23      0x67    //LSB 11
#define LP5569_REG_PROGRAM_MEM_24      0x68    //MSB 12
#define LP5569_REG_PROGRAM_MEM_25      0x69    //LSB 12
#define LP5569_REG_PROGRAM_MEM_26      0x6A    //MSB 13
#define LP5569_REG_PROGRAM_MEM_27      0x6B    //LSB 13
#define LP5569_REG_PROGRAM_MEM_28      0x6C    //MSB 14
#define LP5569_REG_PROGRAM_MEM_29      0x6D    //LSB 14
#define LP5569_REG_PROGRAM_MEM_30      0x6E    //MSB 15
#define LP5569_REG_PROGRAM_MEM_31      0x6F    //LSB 15
#define LP5569_REG_ENGINE1_MAPPING1    0x70    //Engine 1 LED [8] and Master Fader Mapping
#define LP5569_REG_ENGINE1_MAPPING2    0x71    //Engine 1 LED [7:0] Mapping
#define LP5569_REG_ENGINE2_MAPPING1    0x72    //Engine 2 LED [8] and Master Fader Mapping
#define LP5569_REG_ENGINE2_MAPPING2    0x73    //Engine 2 LED [7:0] Mapping
#define LP5569_REG_ENGINE3_MAPPING1    0x74    //Engine 3 LED [8] and Master Fader Mapping
#define LP5569_REG_ENGINE3_MAPPING2    0x75    //Engine 3 LED [7:0] Mapping
#define LP5569_REG_PWM_CONFIG          0x80    //PWM Input Configuration
#define LP5569_REG_LED_FAULT1          0x81    //LED [8] Fault Status
#define LP5569_REG_LED_FAULT2          0x82    //LED [7:0] Fault Status
#define LP5569_REG_GENERAL_FAULT       0x83    //CP Cap, UVLO and TSD Fault Status




//------------------------------------------------------------------------------
//LP5569 valtozoi
typedef struct
{
    //Az eszkoz I2C-s eleresehez tartozo jellemzok. (busz, slave cim, stb...)
    MyI2CM_Device_t i2cDevice;
} LP5569_t;
//------------------------------------------------------------------------------
//Eszkoz letrehozasa
void LP5569_create(LP5569_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);

//Az IC egy 8 bites regiszterenek irasa
status_t LP5569_writeReg(LP5569_t* dev, uint8_t address, uint8_t regValue);

//Az IC tobb 8 bites regiszterenek irasa
status_t LP5569_writeMultipleRegs(LP5569_t* dev,
                                  uint8_t address,
                                  uint8_t* values,
                                  uint8_t length);

//Az IC egy 8 bites regiszterenek olvasasa
status_t LP5569_readReg(LP5569_t* dev, uint8_t address, uint8_t* regValue);

//IC engedelyezese
status_t LP5569_enable(LP5569_t* dev);
//IC tiltasa
status_t LP5569_disable(LP5569_t* dev);

//LED vezerlesek bealliatsanal, a master fader beallitashoz hasznalhato enumok.
//(Olyan bitmaszkok, melyeket azonnal a regiszterbe irhatunk.)
typedef enum
{
    //A LED csatornat nem befolyasolja egyetlen master fader sem
    LP5569_NO_MASTER_FADING=0,
    //A LED csatornahoz az alabbiak szerint 1, 2, 3 master fadert rendeli
    LP5569_MASTER_FADER1=1 << 5,
    LP5569_MASTER_FADER2=2 << 5,
    LP5569_MASTER_FADER3=3 << 5,
    //A PWM bemenet szerint fadeli a csatornat
    LP5569_PWM_INPUT_MASTER_FADING=5 << 5,
} LP5569_MF_MAPPING_t;

//LED vezerles beallitasa
status_t LP5569_setLedControl(LP5569_t* dev,
                              uint8_t ledIndex,
                              LP5569_MF_MAPPING_t masterFader,
                              bool ratioMetricDimming,
                              bool exponentialAdjustment,
                              bool externalPowerd);

//LED csatorna PWM beallitasa (0x00-0% 0x80-50% 0xff-100%)
status_t LP5569_setPWM(LP5569_t* dev,
                       uint8_t ledIndex,
                       uint8_t pwmValue);

//LED csatorna aram beallitasa (0x00-0mA 0x01-0.1mA 0xaf-17.5mA 0xFF-25.5mA)
status_t LP5569_setCurrent( LP5569_t* dev,
                            uint8_t ledIndex,
                            uint8_t currentValue);


//Charge-pump mode selection
typedef enum
{
    //00 = Disabled (cp output pulled-down internally, default)
    LP5569_CP_MODE_DISABLED,
    //01 = 1× mode
    LP5569_CP_MODE_1X,
    //10 = 1.5× mode
    LP5569_CP_MODE_1_5X,
    //11 = Auto mode
    LP5569_CP_MODE_AUTO
} LP5569_CP_MODE_t;

//LP5569 konfiguralasanal hasznalt konfiguracios struktura
typedef struct
{
    //Internal 32-kHz clock-enable select
    bool internalClk;

    //Charge-pump mode selection
    LP5569_CP_MODE_t cpMode;

    //Charge-pump return to 1× mode select
    //false: Charge-pump mode is not affected during shutdown or powersave entry
    //true: Charge-pump mode is forced to 1x mode during shutdown or power-save entry
    bool cpReturn1x;

    //Power-save mode enable select
    //false: Power-save mode is disabled (default)
    //true: Power-save mode is enabled
    bool powerSaving;

    //I2C address auto-increment enable
    //false: Address auto-increment is disabled
    //true: Address auto-increment is enabled (default)
    bool autoIncrement;

    //Charge pump discharge disable
    //false: discharging is enabled in shutdown and standby states, absent of
    //TSD. (default)    -->more standby current!
    //true: discharging is disabled
    bool cpDisableDischarge;

} LP5569_Config_t;

//LP5569 konfiguralasa
status_t LP5569_config(LP5569_t* dev, const LP5569_Config_t* config);

//A 3 master fader valamelyikenek beallitasa.
status_t LP5569_setMasterFader( LP5569_t* dev,
                                uint8_t faderIndex,
                                uint8_t faderValue);

//Az osszes led egy lepesben torteno beallitasa
status_t LP5569_setAllPwm( LP5569_t* dev,
                           uint8_t* pwmValues);

//IC szoftveres reset
status_t LP5569_reset(LP5569_t* dev);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //LP5569_H_
