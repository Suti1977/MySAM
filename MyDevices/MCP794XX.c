//------------------------------------------------------------------------------
//  Microchip MCP794XX valos ideju ora (RTC) ic csalad driver
//
//    File: MCP794XX.c
//------------------------------------------------------------------------------
#include "MCP794XX.h"
#include <string.h>
#include "MyBCD.h"

//RTC/SRAM elereshez tartozo i2c cim
#define I2C_ADDRESS__MCP794XX_RTC           0x6f
//UID/EEPROM terulet eleresehez tartozo i2c cim
#define I2C_ADDRESS__MCP794XX_EEPROM        0x57

//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void MCP794XX_create(MCP794XX_t* dev, MyI2CM_t* i2c)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, I2C_ADDRESS__MCP794XX_RTC, NULL);
}
//------------------------------------------------------------------------------
//MCP794XX irasa. A megadott cimtol kezd kiirni egy megadott adattartalmat,
//a megadott hosszon
status_t MCP794XX_write(MCP794XX_t* dev,
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
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, cmd,  sizeof(cmd)},
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, data, length     },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//MCP794XX  olvasasa. A megadott cimtol kezd olvasni egy megadott adattartalmat
//a megadott hosszon, a megadott celteruletre
status_t MCP794XX_read(MCP794XX_t* dev,
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
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, cmd,  sizeof(cmd)},
        (MyI2CM_xfer_t){MYI2CM_DIR_RX, data, length     },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//RTC-bol aktualis idopont kiolvasasa.
status_t MCP794XX_getTime(MCP794XX_t* dev, struct tm* tm)
{
    status_t status;
    uint8_t regs[MCP794XX_SEC_LEN];

    ASSERT(tm);

    //Idopont regiszterek kiolvasasa
    status=MCP794XX_read(dev, MCP794XX_RTCSEC, regs, sizeof(regs));

    //Hiba eseten kilepes a hibaval
    if (status) return status;

    //RTC-bol olvasott adatok standard time.h-ban definialt "tm" tipusra
    //konvertalsa...

    tm->tm_sec = MyBCD_bcd2bin(regs[MCP794XX_RTCSEC] & 0x7f);
    tm->tm_min = MyBCD_bcd2bin(regs[MCP794XX_RTCMIN]);
    if (regs[MCP794XX_RTCHOUR] & MCP794XX_RTCHOUR_AMPM)
    {   //AM/PM mod
        tm->tm_hour = MyBCD_bcd2bin(regs[MCP794XX_RTCHOUR] & 0x1f);

        if (regs[MCP794XX_RTCHOUR] & MCP794XX_RTCHOUR_AMPM)
            tm->tm_hour += 12;
    } else
    {   //24 oras mod
        tm->tm_hour = MyBCD_bcd2bin(regs[MCP794XX_RTCHOUR] & 0x3f);
    }

    tm->tm_mday = MyBCD_bcd2bin(regs[MCP794XX_RTCDATE]);
    tm->tm_wday = MyBCD_bcd2bin(regs[MCP794XX_RTCWKDAY] & 0x07) - 1;
    tm->tm_mon  = MyBCD_bcd2bin(regs[MCP794XX_RTCMTH] & 0x1f) - 1;
    tm->tm_year = MyBCD_bcd2bin(regs[MCP794XX_RTCYEAR]) + 100;

    return status;

}
//------------------------------------------------------------------------------
//RTC-be ido beallitasa
status_t MCP794XX_setTime(MCP794XX_t* dev, struct tm* tm)
{
    status_t status;
    uint8_t regs[MCP794XX_SEC_LEN];

    ASSERT(tm);
    //Masodperc es oszcillator inditasa (ha nem lenne elinditva.
    regs[MCP794XX_RTCSEC]   = MyBCD_bin2bcd((uint8_t)tm->tm_sec) |
                                MCP794XX_RTCSEC_ST;

    regs[MCP794XX_RTCMIN]   = MyBCD_bin2bcd((uint8_t)tm->tm_min);
    regs[MCP794XX_RTCHOUR]  = MyBCD_bin2bcd((uint8_t)tm->tm_hour);

    //Het napjanak beallitasa, VBAT bemenet engedelyezese
    regs[MCP794XX_RTCWKDAY] = MyBCD_bin2bcd((uint8_t)tm->tm_wday + 1) |
                                MCP794XX_RTCWKDAY_VBATEN;

    regs[MCP794XX_RTCDATE]  = MyBCD_bin2bcd((uint8_t)tm->tm_mday);
    regs[MCP794XX_RTCMTH]   = MyBCD_bin2bcd((uint8_t)tm->tm_mon + 1);
    regs[MCP794XX_RTCYEAR]  = MyBCD_bin2bcd((uint8_t)tm->tm_year);

    //Idopont regiszterek irasa
    status=MCP794XX_write(dev, MCP794XX_RTCSEC, regs, sizeof(regs));
    if (status) goto exit;

    #if 0
    //teszt:  INT kimenet-en 1Hz-es negyszog bekapcsolasa...
    uint8_t regVal=MCP794XX_CONTROL_SQWEN | 0;
    status=MCP794XX_write(dev, MCP794XX_CONTROL, &regVal, sizeof(regVal));
    if (status) goto exit;
    #endif

exit:
    return status;

}
//------------------------------------------------------------------------------
//RTC-ben talalhato egyedi azonosito olvasasa (UID-48/64)
status_t MCP794XX_readUID(MCP794XX_t* dev, uint8_t* buff, uint32_t len)
{
    status_t status;

    //Mivel az MCP794XX mas slave cimen tartalmazza az EUI-48/64 adatokat,
    //az eredeti leirobol masolatot keszitunk, melyen a slave cimet modositjuk.
    //Ezt a modositott leirot adjuk at az olvaso rutinnak.
    MyI2CM_Device_t tempDesc;
    memcpy(&tempDesc, &dev->i2cDevice, sizeof(MyI2CM_Device_t));
    tempDesc.slaveAddress=I2C_ADDRESS__MCP794XX_EEPROM;

    uint8_t cmd[1];
    cmd[0]= MCP794XX_REG_EUI;

    //Olvasas...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, cmd,  sizeof(cmd)},
        (MyI2CM_xfer_t){MYI2CM_DIR_RX, buff, len        },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&tempDesc, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
