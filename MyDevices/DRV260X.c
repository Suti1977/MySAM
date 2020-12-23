//------------------------------------------------------------------------------
//  DRV260X haptic meghajto ic driver
//
//    File: DRV260X.c
//------------------------------------------------------------------------------
#include "DRV260X.h"
#include <string.h>

#define DRV260X_TRACE 0
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void DRV260X_create(DRV260X_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);
}
//------------------------------------------------------------------------------
//Az IC olvasasa
status_t DRV260X_readReg(DRV260X_t* dev,
                        uint8_t address,
                        uint8_t* buff)
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
        (MyI2CM_xfer_t){MYI2CM_DIR_RX, buff, 1           },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Az IC egy 16 bites regiszterenek irasa
status_t DRV260X_writeReg(DRV260X_t* dev, uint8_t address, uint8_t regValue)
{
    status_t status;

    //regiszter cime (8 bites pointer)
    uint8_t cmd[1];
    cmd[0]= address;

    //Iras...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, cmd,                 sizeof(cmd) },
        (MyI2CM_xfer_t){MYI2CM_DIR_TX, (uint8_t*)&regValue, 1           },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//address altal mutatott regiszterben a bitMask szerinti bitek beallitasa
status_t DRV260X_setBit(DRV260X_t* dev, uint8_t address, uint8_t bitMask)
{
    status_t status;
    uint8_t regVal;
    status=DRV260X_readReg(dev, address, &regVal);
    if (status) return status;
    regVal |= bitMask;
    status=DRV260X_writeReg(dev, address, regVal);
    return  status;
}
//------------------------------------------------------------------------------
//address altal mutatott regiszterben a bitMask szerinti bitek torlese
status_t DRV260X_clrBit(DRV260X_t* dev, uint8_t address, uint8_t bitMask)
{
    status_t status;
    uint8_t regVal;
    status=DRV260X_readReg(dev, address, &regVal);
    if (status) return status;
    regVal &= ~bitMask;
    status=DRV260X_writeReg(dev, address, regVal);
    return  status;
}
//------------------------------------------------------------------------------
//address altal mutatott regiszterben bitek beallitasa/torlese
//clrBitMask szerinti bitek torlesre kerulnek
//setBitMask szerinti bitek beallitasra kerulnek
//Megj: az eredeti regiszter erteke
status_t DRV260X_setReg(DRV260X_t* dev,
                         uint8_t address,
                         uint8_t clrBitMask, uint8_t setBitMask)
{
    #if DRV260X_TRACE
    printf("DRV260X_setReg  [%02x] clr: %02x  set: %02x\n", address, clrBitMask, setBitMask);
    #endif

    status_t status;
    uint8_t regVal;

    status=DRV260X_readReg(dev, address,  &regVal);
    if (status) return status;

    regVal &= ~clrBitMask;
    regVal |= setBitMask;
    status=DRV260X_writeReg(dev, address, regVal);
    return  status;
}
//------------------------------------------------------------------------------
//Statusz regiszter olvasasa
status_t DRV260X_getStatus(DRV260X_t* dev, uint8_t* statusRegValue)
{
    return DRV260X_readReg(dev, DRV260X_STATUS_REG, statusRegValue);
}
//------------------------------------------------------------------------------
//Eszkoz szoftveres resetelese
status_t DRV260X_reset(DRV260X_t* dev)
{
    return DRV260X_writeReg(dev, DRV260X_MODE_REG, DRV260X_MODE_DEV_RESET);
}
//------------------------------------------------------------------------------
//Eszkoz standby-ba helyezese/ebresztese
status_t DRV260X_setStandby(DRV260X_t* dev, bool standby)
{
    if (standby)
    {
        return DRV260X_setBit(dev, DRV260X_MODE_REG, DRV260X_MODE_STANDBY);
    } else
    {
        return DRV260X_clrBit(dev, DRV260X_MODE_REG, DRV260X_MODE_STANDBY);
    }
}
//------------------------------------------------------------------------------
//Chip/motor konfiguralasa
status_t DRV260X_config(DRV260X_t* dev, const DRV260X_config_t* cfg)
{
    status_t status;
    //set rated voltage
    status=DRV260X_writeReg(dev, DRV260X_RAT_VOL_REG, cfg->ratedVoltage);
    if (status) goto error;

    //set overdrive clamp voltage
    status=DRV260X_writeReg(dev, DRV260X_ODC_VOL_REG, cfg->odcVoltage);
    if (status) goto error;

    //set feedback control
    status=DRV260X_writeReg(dev, DRV260X_FB_CTRL_REG, cfg->feedbackControl);
    if (status) goto error;

    //control 1
    status=DRV260X_writeReg(dev, DRV260X_CTRL1_REG, cfg->ctrl1);
    if (status) goto error;

    //control 2
    status=DRV260X_writeReg(dev, DRV260X_CTRL2_REG, cfg->ctrl2);
    if (status) goto error;

    //control 3
    status=DRV260X_writeReg(dev, DRV260X_CTRL3_REG, cfg->ctrl3);
    if (status) goto error;

    //control 4
    status=DRV260X_writeReg(dev, DRV260X_CTRL4_REG, cfg->ctrl4);
    if (status) goto error;

    //library selection
    status=DRV260X_writeReg(dev, DRV260X_LIB_SEL_REG, cfg->lib);
    if (status) goto error;

error:
    return status;
}
//------------------------------------------------------------------------------
//Haptic kalibracioja. A rutin nem ter vissza addig, amig a kalibracio be nem
//fejezodik.
status_t DRV260X_calibrate(DRV260X_t* dev)
{
    status_t status;

    //Auto kalibracios modba valtas
    status=DRV260X_writeReg(dev, DRV260X_MODE_REG, DRV260X_MODE_MODE_AUTOCAL);
    if (status) goto error;

    //Auto kalibracio inditasa.
    status=DRV260X_writeReg(dev, DRV260X_GO_REG, DRV260X_GO_GO);
    if (status) goto error;

    //Varakozik, amig a kalibracio elkeszul...
    //A go flaget kell figyelni. Amikor az torlodik, akkor van kesz.
    while(1)
    {
        uint8_t goRegVale;
        status=DRV260X_readReg(dev, DRV260X_GO_REG, &goRegVale);
        if (status) goto error;
        if ((goRegVale & DRV260X_GO_GO)==0) break;
        vTaskDelay(20);
    }

error:
    return status;
}
//------------------------------------------------------------------------------
