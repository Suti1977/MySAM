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

    //Olvasas...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, (uint8_t*)&address,  1 },
        (MyI2CM_xfer_t){MYI2CM_FLAG_RX, buff,                1 },
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

    //Iras...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, (uint8_t*)&address,  1 },
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, (uint8_t*)&regValue, 1 },
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
        return DRV260X_writeReg(dev, DRV260X_MODE_REG, DRV260X_MODE_STANDBY);
    } else
    {
        return DRV260X_writeReg(dev, DRV260X_MODE_REG, 0x00);
    }
}
//------------------------------------------------------------------------------
//Chip/motor konfiguralasa
status_t DRV260X_config(DRV260X_t* dev, const DRV260X_config_t* cfg)
{
    status_t status;

    //Standby mod-ba leptetjuk az IC-t
    status=DRV260X_writeReg(dev, DRV260X_MODE_REG, DRV260X_MODE_STANDBY);
    if (status) goto error;

    //picit varni kell. min 250usec
    vTaskDelay(2);

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

    //Aktiv modba lepteti a vezerlot
    status=DRV260X_writeReg(dev, DRV260X_MODE_REG, 0);
    if (status) goto error;

error:
    return status;
}
//------------------------------------------------------------------------------
//Haptic kalibracioja. A rutin nem ter vissza addig, amig a kalibracio be nem
//fejezodik.
status_t DRV260X_autoCalibrate(DRV260X_t* dev,
                               DRV260X_calibrationData_t* calibData)
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

    //Auto kalibracio sikresessegenek olvasasa
    uint8_t statusRegValue;
    status=DRV260X_getStatus(dev, &statusRegValue);
    if (status) goto error;
    if (statusRegValue & DRV260X_STATUS_DIAG_RESULT)
    {   //Az auto kalibracio hibat jelez
        status=kStatus_Fail;
        goto error;
    }

    if (calibData)
    {   //Kalibracios adatok kiolvasasa...
        //A kiolvasott valtozokat kesobb a chipbe toltve, nem kell futtatni a
        //kalibraciot.

        status=DRV260X_readReg(dev,
                               DRV260X_CAL_COMP_RES_REG,
                               &calibData->compensation);
        if (status) goto error;

        status=DRV260X_readReg(dev,
                               DRV260X_CAL_BACK_EMF_RES_REG,
                               &calibData->backEmf);
        if (status) goto error;

        status=DRV260X_readReg(dev,
                               DRV260X_FB_CTRL_REG,
                               &calibData->backEmfGain);
        if (status) goto error;

    }

error:
    return status;
}
//------------------------------------------------------------------------------
//Kalibracios adatok beallitasa. A calibData-ban atadott strukturat egy korabbi
// DRV260X_autoCalibrate() hivaskor adta at a driver.
status_t DRV260X_setCalibrationParameters(DRV260X_t* dev,
                                          DRV260X_calibrationData_t* calibData)
{
    status_t status;
    status=DRV260X_writeReg(dev,
                            DRV260X_CAL_COMP_RES_REG,
                            calibData->compensation);
    if (status) goto error;

    status=DRV260X_writeReg(dev,
                            DRV260X_CAL_BACK_EMF_RES_REG,
                            calibData->backEmf);
    if (status) goto error;

    status=DRV260X_writeReg(dev,
                            DRV260X_FB_CTRL_REG,
                            calibData->backEmfGain);
    if (status) goto error;

error:
    return status;
}
//------------------------------------------------------------------------------
//trigger mod beallitasa
status_t DRV260X_setTriggerMode(DRV260X_t* dev, uint8_t triggerMode)
{
    status_t status;
    status=DRV260X_writeReg(dev, DRV260X_MODE_REG, triggerMode);
    if (status) goto error;
error:
    return status;
}
//------------------------------------------------------------------------------
//program/folyamat inditasa
status_t DRV260X_go(DRV260X_t* dev)
{
    return DRV260X_writeReg(dev, DRV260X_GO_REG, DRV260X_GO_GO);
}
//------------------------------------------------------------------------------
//program/folyamat azonnali megszakitasa
status_t DRV260X_stop(DRV260X_t* dev)
{
    return DRV260X_writeReg(dev, DRV260X_GO_REG, 0);
}
//------------------------------------------------------------------------------
//Annak tesztelese, hogy a lejatszas meg fut-e...
bool DRV260X_isDone(DRV260X_t* dev, status_t* status)
{
    status_t stat;
    uint8_t goRegVale;

    stat=DRV260X_readReg(dev, DRV260X_GO_REG, &goRegVale);

    if (status) *status=stat;
    if (stat) return false;

    //Amig a go regiszter nem 0, addig fut a lejatszas
    if (goRegVale & DRV260X_GO_GO) return false;

    return true;
}
//------------------------------------------------------------------------------
//seq regiszter beallitasa
status_t DRV260X_setSeqReg(DRV260X_t* dev, uint8_t regIndex, uint8_t regValue)
{
    return DRV260X_writeReg(dev, DRV260X_WAV_SEQ_1_REG+regIndex, regValue);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
