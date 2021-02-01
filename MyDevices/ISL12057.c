//------------------------------------------------------------------------------
//  Renesas/Intersil ISL12057 tipusu I2C-s valos ideju ora (RTC) driver
//
//    File: ISL12057.c
//------------------------------------------------------------------------------
#include "ISL12057.h"
#include <string.h>
#include "MyBCD.h"

//------------------------------------------------------------------------------
//RTC IC-hez I2C eszkoz letrehozasa a rendszerben
void ISL12057_create(ISL12057_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //modul valtozoinak kezdeti nulalzsasa
    memset(dev, 0, sizeof(ISL12057_t));

    MyI2CM_createDevice(&dev->device, i2c, slaveAddress, NULL);
}
//------------------------------------------------------------------------------
//ISL12057 irasa. A megadott cimtol kezd kiirni egy megadott adattartalmat,
//a megadott hosszon
status_t ISL12057_write(ISL12057_t* dev,
                        uint8_t address,
                        uint8_t* data, uint8_t length)
{
    status_t status;

    uint8_t cmd[1];
    cmd[0]= address;

    //Iras...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,  sizeof(cmd)},
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, data, length     },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->device, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//ISL12057  olvasasa. A megadott cimtol kezd olvasni egy megadott adattartalmat
//a megadott hosszon, a megadott celteruletre
status_t ISL12057_read(ISL12057_t* dev,
                      uint8_t address,
                      uint8_t* data, uint8_t length)
{
    status_t status;

    uint8_t cmd[1];
    cmd[0]= address;

    //Olvasas...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,  sizeof(cmd)},
        (MyI2CM_xfer_t){MYI2CM_FLAG_RX, data, length     },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->device, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//RTC-bol aktualis idopont kiolvasasa
status_t ISL12057_getTime(ISL12057_t* dev, struct tm* tm)
{
    status_t status;
    uint8_t regs[ISL12057_RTC_SEC_LEN];

    ASSERT(tm);

    //Idopont regiszterek kiolvasasa
    status=ISL12057_read(dev, ISL12057_REG_RTC_SC, regs, sizeof(regs));

    //Hiba eseten kilepes a hibaval
    if (status) return status;

    //RTC-bol olvasott adatok standard time.h-ban definialt "tm" tipusra
    //konvertalsa...

    tm->tm_sec = MyBCD_bcd2bin(regs[ISL12057_REG_RTC_SC]);
    tm->tm_min = MyBCD_bcd2bin(regs[ISL12057_REG_RTC_MN]);
    if (regs[ISL12057_REG_RTC_HR] & ISL12057_REG_RTC_HR_MIL)
    {   //AM/PM mod
        tm->tm_hour = MyBCD_bcd2bin(regs[ISL12057_REG_RTC_HR] & 0x1f);

        if (regs[ISL12057_REG_RTC_HR] & ISL12057_REG_RTC_HR_PM)
            tm->tm_hour += 12;
    } else
    {   //24 oras mod
        tm->tm_hour = MyBCD_bcd2bin(regs[ISL12057_REG_RTC_HR] & 0x3f);
    }

    tm->tm_mday = MyBCD_bcd2bin(regs[ISL12057_REG_RTC_DT]);
    tm->tm_wday = MyBCD_bcd2bin(regs[ISL12057_REG_RTC_DW]) - 1;        //starts at 1
    tm->tm_mon  = MyBCD_bcd2bin(regs[ISL12057_REG_RTC_MO] & 0x1f) - 1; // ditto
    tm->tm_year = MyBCD_bcd2bin(regs[ISL12057_REG_RTC_YR]) + 100;

    return status;
}
//------------------------------------------------------------------------------
//RTC-be ido beallitasa
status_t ISL12057_setTime(ISL12057_t* dev, struct tm* tm)
{
    status_t status;
    uint8_t regs[ISL12057_RTC_SEC_LEN];

    ASSERT(tm);

    //tm strukturabol az RTC-be irando adathalmaz eloallitasa...
    uint8_t centuryBit;

    centuryBit = (tm->tm_year > 199) ? ISL12057_REG_RTC_MO_CEN : 0;

    regs[ISL12057_REG_RTC_SC] = MyBCD_bin2bcd((uint8_t)tm->tm_sec);
    regs[ISL12057_REG_RTC_MN] = MyBCD_bin2bcd((uint8_t)tm->tm_min);
    regs[ISL12057_REG_RTC_HR] = MyBCD_bin2bcd((uint8_t)tm->tm_hour);
    regs[ISL12057_REG_RTC_DT] = MyBCD_bin2bcd((uint8_t)tm->tm_mday);
    regs[ISL12057_REG_RTC_MO] =
                        MyBCD_bin2bcd((uint8_t)tm->tm_mon + 1) | centuryBit;
    regs[ISL12057_REG_RTC_YR] = MyBCD_bin2bcd((uint8_t)tm->tm_year % 100);
    regs[ISL12057_REG_RTC_DW] = MyBCD_bin2bcd((uint8_t)tm->tm_wday + 1);

    //Idopont regiszterek kiolvasasa
    status=ISL12057_write(dev, ISL12057_REG_RTC_SC, regs, sizeof(regs));

    //Hiba eseten kilepes a hibaval
    if (status) goto exit;

    //Az RTC-ben az oszcillator hiba jelzest toroljuk, ha esetleg korabban az
    //be volt allitva.
    uint8_t statusregValue;
    status=ISL12057_read(dev, ISL12057_REG_SR, &statusregValue, 1);
    if (status) goto exit;
    statusregValue &= ~ISL12057_REG_SR_OSF;
    status=ISL12057_write(dev, ISL12057_REG_SR, &statusregValue, 1);
    if (status) goto exit;

exit:
    return status;
}
//------------------------------------------------------------------------------
