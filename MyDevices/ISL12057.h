//------------------------------------------------------------------------------
//  Renesas/Intersil ISL12057 tipusu I2C-s valos ideju ora (RTC) driver
//
//    File: ISL12057.h
//------------------------------------------------------------------------------
#ifndef ISL12057_H_
#define ISL12057_H_

#include "MyI2CM.h"
#include <time.h>

/* RTC section */
#define ISL12057_REG_RTC_SC     0x00        /* Seconds */
#define ISL12057_REG_RTC_MN     0x01        /* Minutes */
#define ISL12057_REG_RTC_HR     0x02        /* Hours */
#define ISL12057_REG_RTC_HR_PM	BIT(5)      /* AM/PM bit in 12h format */
#define ISL12057_REG_RTC_HR_MIL BIT(6)      /* 24h/12h format */
#define ISL12057_REG_RTC_DW     0x03        /* Day of the Week */
#define ISL12057_REG_RTC_DT     0x04        /* Date */
#define ISL12057_REG_RTC_MO     0x05        /* Month */
#define ISL12057_REG_RTC_MO_CEN	BIT(7)      /* Century bit */
#define ISL12057_REG_RTC_YR     0x06        /* Year */
#define ISL12057_RTC_SEC_LEN	7

/* Alarm 1 section */
#define ISL12057_REG_A1_SC      0x07        /* Alarm 1 Seconds */
#define ISL12057_REG_A1_MN      0x08        /* Alarm 1 Minutes */
#define ISL12057_REG_A1_HR      0x09        /* Alarm 1 Hours */
#define ISL12057_REG_A1_HR_PM	BIT(5)      /* AM/PM bit in 12h format */
#define ISL12057_REG_A1_HR_MIL	BIT(6)      /* 24h/12h format */
#define ISL12057_REG_A1_DWDT	0x0A        /* Alarm 1 Date / Day of the week */
#define ISL12057_REG_A1_DWDT_B	BIT(6)      /* DW / DT selection bit */
#define ISL12057_A1_SEC_LEN     4

/* Alarm 2 section */
#define ISL12057_REG_A2_MN      0x0B        /* Alarm 2 Minutes */
#define ISL12057_REG_A2_HR      0x0C        /* Alarm 2 Hours */
#define ISL12057_REG_A2_DWDT	0x0D        /* Alarm 2 Date / Day of the week */
#define ISL12057_A2_SEC_LEN     3

/* Control/Status registers */
#define ISL12057_REG_INT        0x0E
#define ISL12057_REG_INT_A1IE	BIT(0)      /* Alarm 1 interrupt enable bit */
#define ISL12057_REG_INT_A2IE	BIT(1)      /* Alarm 2 interrupt enable bit */
#define ISL12057_REG_INT_INTCN	BIT(2)      /* Interrupt control enable bit */
#define ISL12057_REG_INT_RS1	BIT(3)      /* Freq out control bit 1 */
#define ISL12057_REG_INT_RS2	BIT(4)      /* Freq out control bit 2 */
#define ISL12057_REG_INT_EOSC	BIT(7)      /* Oscillator enable bit */

#define ISL12057_REG_SR         0x0F
#define ISL12057_REG_SR_A1F     BIT(0)      /* Alarm 1 interrupt bit */
#define ISL12057_REG_SR_A2F     BIT(1)      /* Alarm 2 interrupt bit */
#define ISL12057_REG_SR_OSF     BIT(7)      /* Oscillator failure bit */

//------------------------------------------------------------------------------
//ISL12057 valtozoi
typedef struct
{
    MyI2CM_Device_t device;
} ISL12057_t;
//------------------------------------------------------------------------------
//RTC IC-hez I2C eszkoz letrehozasa a rendszerben
void ISL12057_create(ISL12057_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress);

//ISL12057 irasa. A megadott cimtol kezd kiirni egy megadott adattartalmat,
//a megadott hosszon
status_t ISL12057_write(ISL12057_t* dev,
                       uint8_t address,
                       uint8_t* data, uint8_t length);

//ISL12057  olvasasa. A megadott cimtol kezd olvasni egy megadott adattartalmat
//a megadott hosszon, a megadott celteruletre
status_t ISL12057_read(ISL12057_t* dev,
                      uint8_t address,
                      uint8_t* data, uint8_t length);

//RTC-bol aktualis idopont kiolvasasa.
status_t ISL12057_getTime(ISL12057_t* dev, struct tm* tm);

//RTC-be ido beallitasa
status_t ISL12057_setTime(ISL12057_t* dev, struct tm* tm);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //ISL12057_H_
