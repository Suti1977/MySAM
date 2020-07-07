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
//------------------------------------------------------------------------------
