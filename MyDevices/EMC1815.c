//------------------------------------------------------------------------------
//  EMC1815 remote diode sensore driver
//
//    File: EMC1815.c
//------------------------------------------------------------------------------
#include "EMC1815.h"
#include <string.h>

//------------------------------------------------------------------------------
//Homero IC-hez I2C eszkoz letrehozasa a rendszerben
void EMC1815_create(EMC1815_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //modul valtozoinak kezdeti nulalzsasa
    memset(dev, 0, sizeof(EMC1815_t));

    MyI2CM_CreateDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);

}
//------------------------------------------------------------------------------
//EMC1815 irasa. A megadott cimtol kezd kiirni egy megadott adattartalmat,
//a megadott hosszon
status_t EMC1815_write(EMC1815_t* dev,
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
//EMC1815  olvasasa. A megadott cimtol kezd olvasni egy megadott adattartalmat
//a megadott hosszon, a megadott celteruletre
status_t EMC1815_read(EMC1815_t* dev,
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
//EMC1815 konfiguralasa
status_t EMC1815_config(EMC1815_t* dev,
                        const EMC1815_Config_t* cfg)
{
    status_t status;

    //Konfiguracios regiszter beallitasa...
    uint8_t regValue=0;
    if (cfg->range)
    {
        regValue |= EMC1815_CONFIG_RANGE;
        dev->range=true;
    } else
    {
        dev->range=false;
    }

    if (cfg->RECD1_2) regValue |= EMC1815_CONFIG_RECD1_2;
    if (cfg->RECD3_4) regValue |= EMC1815_CONFIG_RECD3_4;
    if (cfg->dynamicFiltering) regValue |= EMC1815_CONFIG_DA_ENA;
    if (cfg->disableAntiParalelDiodeMode) regValue |= EMC1815_CONFIG_APDD;

    status=EMC1815_write(dev,EMC1815_ADDR_CONFIG, &regValue, 1);
    if (status) goto error;

    //Mintaveteli sebesseg beallitasa
    regValue=cfg->convRate;
    status=EMC1815_write(dev,EMC1815_ADDR_CONVERT, &regValue, 1);
    if (status) goto error;

    //filter tipus beallitasa
    regValue=cfg->filter;
    status=EMC1815_write(dev,EMC1815_ADDR_FILTER_SEL, &regValue, 1);
    if (status) goto error;

error:
    return status;
}
//------------------------------------------------------------------------------
//EMC1815 homerseklet kiolvasasa.
//channel 0-3: a 4 kezelt kulso dioda
//        4-: belso homerseklet
status_t EMC1815_readTemperatureValue(EMC1815_t* dev,
                                      uint8_t channel,
                                      uint16_t* value)
{
    status_t status;

    //A kiolvasasokkor felhasznaljuk, hogy az egyes regiszterek sorban
    //helyezkednek el.
    uint8_t address;
    if (channel<4)
    {
        address=EMC1815_ADDR_EXT1_HIGH_BYTE + (channel * 2);
    } else
    {
        address=EMC1815_ADDR_INT_HIGH_BYTE;
    }

    //Felso 8 bit    
    volatile uint16_t val;
    status=EMC1815_read(dev, address, (uint8_t*)&val, 2);
    if (status) goto error;

    //Endian forditas
    val=__builtin_bswap16(val);
    //Eredmeny helyere tolasa. A legalso bit a 0. pozicioba kerul.
    val >>=5;

    *value=val;

error:
    return status;
}
//------------------------------------------------------------------------------
//binaris ertekbol Celsius fokba konvertalas.
double EMC1815_value2Celsius(EMC1815_t* dev, uint16_t value)
{
    if (dev->range)
    {
        return (((double)value)*0.125)-64.0;
    } else
    {
        return ((double)value * 0.125);
    }
}
//------------------------------------------------------------------------------
