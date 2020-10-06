//------------------------------------------------------------------------------
//  Microchip MCP9800 tipusu homero IC kezelo driver
//
//    File: MCP9800.c
//------------------------------------------------------------------------------
#include "MCP9800.h"



//------------------------------------------------------------------------------
//Homero IC-hez I2C eszkoz letrehozasa a rendszerben
void MCP9800_create(MCP9800_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);

    //I2C eszkoz felkonfiguralasa. Folyamatos mukodes, 12 bites meresi
    //eredmenyek alkalmazasa.
    //MCP9800_setConfigReg(dev, 0x60);
}
//------------------------------------------------------------------------------
//MCP9800 irasa. A megadott cimtol kezd kiirni egy megadott adattartalmat,
//a megadott hosszon
status_t MCP9800_write( MCP9800_t* dev,
                        uint8_t address,
                        uint8_t* data, uint8_t length)
{
    int status;

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
    status=MYI2CM_transfer(&dev->i2cDevice,
                            xferBlocks,
                            ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//MCP9800  olvasasa. A megadott cimtol kezd olvasni egy megadott adattartalmat
//a megadott hosszon, a megadott celteruletre
status_t MCP9800_read(MCP9800_t* dev,
                             uint8_t address,
                             uint8_t* data, uint8_t length)
{
    int status;

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
    status=MYI2CM_transfer(&dev->i2cDevice,
                            xferBlocks,
                            ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//MCP9800 homero IC homerseklet regiszter kiolvasasa
//Az elojelesen visszaadott homerseklet erteket 0.062-el megszorozva kapjuk a
//homersekletet °C-ban.
status_t MCP9800_readTemperatureValue(MCP9800_t* dev, int16_t* value)
{
    status_t status;

    ASSERT(value);

    int16_t temperature;

    //regiszter kiolvasasa
    status=MCP9800_read(dev,
                        MCP9800_TEMPERATURE_REG,
                        (uint8_t*)&temperature,
                        2);

    if (status==kStatus_Success)
    {
        //Endian forgatas.
        temperature=(int16_t)__builtin_bswap16((uint16_t)temperature);

        //Also 4 bit eltolasa (elojeles)
        temperature >>= 4;
        *value=temperature;
    }

    return status;
}
//------------------------------------------------------------------------------
//Homero IC konfiguracios regisztre beallitasa
status_t MCP9800_setConfigReg(MCP9800_t* dev, uint8_t config)
{
    return MCP9800_write(dev, MCP9800_CONFIGURATION_REG, &config, 1);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
