//------------------------------------------------------------------------------
//  KTD2061/58/59/60 driver
//
//    File: KTD2061.c
//------------------------------------------------------------------------------
#include "KTD2061.h"

//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void KTD2061_create(KTD2061_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);
}
//------------------------------------------------------------------------------
//Az IC blokkos olvasasa
status_t KTD2061_read(KTD2061_t* dev,
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
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,  sizeof(cmd) },
        (MyI2CM_xfer_t){MYI2CM_FLAG_RX, buff, length           },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;

}
//------------------------------------------------------------------------------
//Az IC blokkos irasa
status_t KTD2061_write(KTD2061_t* dev,
                      uint8_t address,
                      uint8_t* buff,
                      uint8_t length)
{
    uint8_t cmd[1];
    cmd[0]= address;

    //Iras...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,  sizeof(cmd)},
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, (uint8_t*)buff, length   },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    return MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));
}
//------------------------------------------------------------------------------
//Az IC egy 8 bites regiszterenek irasa
status_t KTD2061_writeReg(KTD2061_t* dev, uint8_t address, uint8_t regValue)
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
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, &regValue, 1      },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Az IC egy 8 bites regiszterenek olvasasa
status_t KTD2061_readReg(KTD2061_t* dev, uint8_t address, uint8_t* regValue)
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
        (MyI2CM_xfer_t){MYI2CM_FLAG_RX, regValue, 1      },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Az ID olvasasa
status_t KTD2061_readID(KTD2061_t* dev, KTD2061_id_t* id)
{
    return KTD2061_readReg(dev, KTD2061_REG_ID, (uint8_t*)id);
}
//------------------------------------------------------------------------------
//Monitor regiszter olvasasa
status_t KTD2061_readMonitor(KTD2061_t* dev, KTD2061_monitor_t* monitor)
{
    return KTD2061_readReg(dev, KTD2061_REG_MONITOR, (uint8_t*)monitor);
}
//------------------------------------------------------------------------------
//control regiszter olvasasa
status_t KTD2061_readControl(KTD2061_t* dev, KTD2061_control_t* control)
{
    return KTD2061_readReg(dev, KTD2061_REG_CONTROL, (uint8_t*)control);
}
//------------------------------------------------------------------------------
//control regiszter irasa
status_t KTD2061_writeControl(KTD2061_t* dev, KTD2061_control_t control)
{
    return KTD2061_writeReg(dev, KTD2061_REG_CONTROL, control.reg);
}
//------------------------------------------------------------------------------
//Chip reset. Minden regiszter alapertelmezesbe all.
status_t KTD2061_reset(KTD2061_t* dev)
{
    return KTD2061_writeReg(dev,
                            KTD2061_REG_CONTROL,
                            KTD2061_CONTROL_ENMODE_RESET);
}
//------------------------------------------------------------------------------
//enable mod kijelolese
//KTD2061_CONTROL_ENMODE_XXXXXval szerint
status_t KTD2061_setEnableMode(KTD2061_t* dev, uint8_t enMode)
{
    status_t status;
    KTD2061_control_t control;
    status=KTD2061_readControl(dev, &control);
    if (status) return status;
    control.bit.en_mode=enMode;
    status=KTD2061_writeControl(dev, control);
    return status;
}
//------------------------------------------------------------------------------
//BrightExtend mod be/ki kapcsolasa
status_t KTD2061_setBrightExtendMode(KTD2061_t* dev, uint8_t enable)
{
    status_t status;
    KTD2061_control_t control;
    status=KTD2061_readControl(dev, &control);
    if (status) return status;
    control.bit.be_en=enable;
    status=KTD2061_writeControl(dev, control);
    return status;
}
//------------------------------------------------------------------------------
//CoolExtend mod beallitasa
//KTD2061_CONTROL_COOL_EXTEND_XXXval szerint
status_t KTD2061_setColExtendTemperature(KTD2061_t* dev, uint8_t ceTempValue)
{
    status_t status;
    KTD2061_control_t control;
    status=KTD2061_readControl(dev, &control);
    if (status) return status;
    control.bit.ce_temp=ceTempValue;
    status=KTD2061_writeControl(dev, control);
    return status;
}
//------------------------------------------------------------------------------
//fade rate beallitasa
//KTD2061_CONTROL_FADE_RATE_XXXXXval szerint
status_t KTD2061_setFadeRate(KTD2061_t* dev, uint8_t fadeRateVal)
{
    status_t status;
    KTD2061_control_t control;
    status=KTD2061_readControl(dev, &control);
    if (status) return status;
    control.bit.fade_rate=fadeRateVal;
    status=KTD2061_writeControl(dev, control);
    return status;
}
//------------------------------------------------------------------------------
//Color 0 beallitasa
status_t KTD2061_setColor0(KTD2061_t* dev, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t buff[3]={ r, g, b };
    return KTD2061_write(dev, KTD2061_REG_IRED0, buff, 3);
}
//------------------------------------------------------------------------------
//Color 1 beallitasa
status_t KTD2061_setColor1(KTD2061_t* dev, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t buff[3]={ r, g, b };
    return KTD2061_write(dev, KTD2061_REG_IRED1, buff, 3);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
