//------------------------------------------------------------------------------
//  TMP1075 homero ic driver
//
//    File: TMP1075.c
//------------------------------------------------------------------------------
#include "TMP1075.h"
#include <string.h>

//------------------------------------------------------------------------------
// kezdeti inicializalasa
void TMP1075_create(TMP1075_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);
}
//------------------------------------------------------------------------------
//Az IC olvasasa
status_t TMP1075_read(TMP1075_t* dev,
                      uint8_t address,
                      uint8_t* buff,
                      uint8_t length)
{
    ASSERT(buff);

    status_t status;

    //regiszter cime (8 bites pointer)
    uint8_t cmd[1];
    cmd[0]= address;

    //Olvasas...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, cmd,  sizeof(cmd) },
        (MyI2CM_xfer_t){MYI2CM_DIR_RX, buff, length           },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Az IC egy 16 bites regiszterenek irasa
status_t TMP1075_writeReg(TMP1075_t* dev, uint8_t address, uint16_t regValue)
{
    status_t status;

    //regiszter cime (8 bites pointer)
    uint8_t cmd[1];
    cmd[0]= address;

    //endian csere...
    regValue= __builtin_bswap16(regValue);

    //Iras...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, cmd,                 sizeof(cmd) },
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, (uint8_t*)&regValue, 2           },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Az IC egy 16 bites regiszterenek olvasasa
status_t TMP1075_readReg(TMP1075_t* dev, uint8_t address, uint16_t* regValue)
{
    status_t status;

    //regiszter cime (8 bites pointer)
    uint8_t cmd[1];
    cmd[0]= address;

    uint16_t temp;
    //Iras...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, cmd,                 sizeof(cmd) },
        (MyI2CM_xfer_t){MYI2CM_DIR_RX, (uint8_t*)&temp,     2           },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    *regValue=__builtin_bswap16(temp);

    return status;
}
//------------------------------------------------------------------------------
//Eszkoz altal visszaadott meresi eredmenybol celsius fokra konvertalas
double TMP1075_value2Celsius(uint16_t temperatureValue)
{
    int16_t val=(int16_t) temperatureValue;
    val >>= 4;

    return ((double) val )* 0.0625;
}
//------------------------------------------------------------------------------
//TMP1075 konfiguralasa
status_t TMP1075_config(TMP1075_t* dev,
                        const TMP1075_Config_t* cfg)
{
    uint16_t regValue=0;
    if (cfg->shutDownMode==true)
        regValue |= TMP1075_CFGR_SD;

    if (cfg->alertMode==TMP1075_ALERT_MODE_INTERRUPT)
        regValue |= TMP1075_CFGR_TM;

    if (cfg->alertPolarity==TMP1075_ALERT_POLARITY_ACTIVE_HIGH)
        regValue |= TMP1075_CFGR_POL;

    regValue |= ((uint16_t) cfg->fault) << TMP1075_CFGR_F_pos;
    regValue |= ((uint16_t) cfg->convRate) << TMP1075_CFGR_R_pos;

    return TMP1075_writeReg(dev, TMP1075_ADDR_CFGR, regValue);
}
//------------------------------------------------------------------------------
//TMP1075 homerseklet kiolvasasa.
status_t TMP1075_readTemperatureValue(TMP1075_t* dev,
                                      uint16_t* value)
{    
    status_t status= TMP1075_readReg(dev, TMP1075_ADDR_TEMP, value);
    return status;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
