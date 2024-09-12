//------------------------------------------------------------------------------
//  BQ4050 BMS IC driver
//
//    File: BQ4050.c
//------------------------------------------------------------------------------
//STUFF:
//  http://www.ti.com/lit/ug/sluuaq3/sluuaq3.pdf
//  https://chromium.googlesource.com/chromiumos/platform/ec/+/master/driver/battery/smart.c
//  https://github.com/PX4/PX4-Autopilot/blob/main/src/lib/drivers/smbus/SMBus.cpp
#include "BQ4050.h"

//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void BQ4050_create(BQ4050_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);
}
//------------------------------------------------------------------------------
//A BMS IC tobb regiszterenek olvasasa
status_t BQ4050_readRegs(BQ4050_t* dev,
                         uint8_t regAddress,
                         uint8_t* buffer,
                         uint32_t length)
{
    ASSERT(buffer);

    status_t status;

    //regiszter cime (8 bites pointer)
    uint8_t cmd[1];
    cmd[0]= regAddress;

    //Olvasas...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,  sizeof(cmd) },
        (MyI2CM_xfer_t){MYI2CM_FLAG_RX, (uint8_t*) buffer, length},
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//A BMS IC egyetlen 16 bites regiszterenek olvasasa
status_t BQ4050_readReg16(BQ4050_t* dev, uint8_t regAddress, uint16_t* data)
{
    return BQ4050_readRegs(dev, regAddress, (uint8_t*) data, 2);
}
//------------------------------------------------------------------------------
//A BMS manufacturer access blockjanak egyetlen 16 bites regiszterenek olvasasa
status_t BQ4050_readManuAccReg16(BQ4050_t* dev,
                                 uint16_t regAddress,
                                 uint16_t* data)
{
    status_t status;

    //Block iro parancs
    uint8_t cmd[4];
    cmd[0]= 0x44;                       //ManufacturerBlockAccess
    cmd[1]= 2;                          //block len
    cmd[2]= regAddress & 0xff;
    cmd[3]= (regAddress >> 8) & 0xff;


    MyI2CM_xfer_t xferBlocksW[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,  sizeof(cmd) },
    };
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocksW, ARRAY_SIZE(xferBlocksW));
    if (status) goto error;


    uint8_t buff[5];
    memset(buff, 0x00, sizeof(buff));
    MyI2CM_xfer_t xferBlocksR[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,  1 },
        (MyI2CM_xfer_t){MYI2CM_FLAG_RX, (uint8_t*) buff, sizeof(buff)},
    };
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocksR, ARRAY_SIZE(xferBlocksR));
    if (status) goto error;

    //Buff[0] vaalsz hossza
    //Buff[1] Buff[2] a command/reg address
    //Buff[3]... a visszadott tenyleges adtattartalom

    *data = *((uint16_t*) &buff[3]);

error:
    return status;
}
//------------------------------------------------------------------------------
//A BMS IC egyetlen regiszterenek irasa
status_t BQ4050_writeReg16(BQ4050_t* dev, uint8_t regAddress, uint16_t regValue)
{
    status_t status;

    //regiszter cime (8 bites pointer)
    uint8_t cmd[1];
    cmd[0]= regAddress;

    //Iras...
    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,                 sizeof(cmd) },
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, (uint8_t*)&regValue, 2           },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Az akkumulator pakk toltottsegi szintjenek kiolvasasa (%-ban adja vissza)
status_t BQ4050_readRelativeStateOfCharge(BQ4050_t* dev,uint16_t* stateOfCharge)
{
    return BQ4050_readReg16(dev, 0x0D, stateOfCharge);
}
//------------------------------------------------------------------------------
//Imbalance feszultseg lekerdezese mV-ban.
status_t BQ4050_getImbalance_mv(BQ4050_t* dev, uint16_t *imbalance_mV)
{
    status_t status;

    uint16_t cellVoltages[BQ4050_MAX_CELL_COUNT];
    uint32_t cellCount = 0;
    uint16_t maxVoltage = 0;
    uint16_t minVoltage = 0xffff;

    //Cella feszultegek lekerdezese...
    status = BQ4050_readRegs(dev,
                            BQ4050_CELL_VOLTAGE4_REG,
                            (uint8_t*)cellVoltages,
                            sizeof(cellVoltages));
    if (status) goto error;

    //Keressuk a legmagasabb es a legalacsonyabb feszultsegu cellat
    for (uint32_t i=0; i != BQ4050_MAX_CELL_COUNT; ++i)
    {
        uint16_t cellVoltage=cellVoltages[i];

        //Csak azokat a cellakat vesszuk be a szamitasba, melyek leteznek
        if (cellVoltage != 0)
        {
            cellCount++;
            if (cellVoltage>maxVoltage) maxVoltage=cellVoltage;
            if (cellVoltage<minVoltage) minVoltage=cellVoltage;
        }
    }

error:
    *imbalance_mV = (cellCount == 0) ? 0 :
                                       (maxVoltage - minVoltage);
    return status;
}
//------------------------------------------------------------------------------
//BMS beallitasa, hogy a toltottseget mA-ba kozolje
status_t BQ4050_setBatteryModeDefaults_mAh(BQ4050_t* dev)
{
    return BQ4050_writeReg16( dev,
                            BQ4050_BATTERY_MODE_REG,
                            //mA-ban reportolja a toltottseget
                            BQ4050_BATTERY_MODE_CAPM_MA |
                            //
                            BQ4050_BATTERY_MODE_CHGM_DISABLE_CHARGING_BCASTS |
                            //
                            BQ4050_BATTERY_MODE_AM_DISABLE_ALARM_BCASTS
                            );
}
//------------------------------------------------------------------------------
//Homerseklet lekerdezese, 0.1 Kelvin-ben
status_t BQ4050_getTemperature(BQ4050_t* dev, uint16_t *temperatureK)
{
    return BQ4050_readReg16(dev, BQ4050_TEMPERATURE_REG, temperatureK);
}
//------------------------------------------------------------------------------
//Akkumulator pakk feszuktseg lekerdezese (mV-ban)
status_t BQ4050_getVoltage_mV(BQ4050_t* dev, uint16_t *voltage_mV)
{
    return BQ4050_readReg16(dev, BQ4050_VOLTAGE_REG, voltage_mV);
}
//------------------------------------------------------------------------------
//Akkumulator pakk aramanak lekerdezese. (mA-ben)
status_t BQ4050_getCurrent_mA(BQ4050_t* dev, int16_t *current_mA)
{
    return BQ4050_readReg16(dev, BQ4050_CURRENT_REG, (uint16_t*)current_mA);
}
//------------------------------------------------------------------------------
//Akkumulator pakk atlag aramanak lekerdezese. (mA-ben)
status_t BQ4050_getAverageCurrent_mA(BQ4050_t* dev, int16_t *current_mA)
{
    return BQ4050_readReg16(dev, BQ4050_AVERAGE_CURRENT_REG, (uint16_t*)current_mA);
}
//------------------------------------------------------------------------------
//Az elore lathato akkumulator kapacitast adja vissza %-ban
status_t BQ4050_getAbsoluteStateOfCharge(BQ4050_t* dev, uint16_t *percent)
{
    return BQ4050_readReg16(dev, BQ4050_ABSOLUTE_STATE_OF_CHARGE_REG, percent);
}
//------------------------------------------------------------------------------
//Az akkumulator pakk relativ toltottseget adja vissza (%-ban adja vissza)
status_t BQ4050_getRelativeStateOfCharge(BQ4050_t* dev, uint16_t *percent)
{
    return BQ4050_readReg16(dev, BQ4050_RELATIVE_STATE_OF_CHARGE_REG, percent);
}
//------------------------------------------------------------------------------
//A maradek toltes lekerdezese mAh-ban
status_t BQ4050_getRemainingCapacity_mAh(BQ4050_t* dev, uint16_t *capacity_mAh)
{
    status_t status;
    //mA-ben akarjuk lekerdezni a maradek energiat
    status=BQ4050_setBatteryModeDefaults_mAh(dev);
    if (status)
    {
        *capacity_mAh=0;
        return status;
    }

    return BQ4050_readReg16(dev, BQ4050_REMAINING_CAPACITY_REG, capacity_mAh);
}
//------------------------------------------------------------------------------
//Tervezett kapacitas lekerdezese mAh-ban
status_t BQ4050_getDesignCapacity(BQ4050_t* dev, uint16_t *capacity_mAh)
{
    status_t status;
    //mA-ben akarjuk lekerdezni a tervezett energiat
    status=BQ4050_setBatteryModeDefaults_mAh(dev);
    if (status)
    {
        *capacity_mAh=0;
        return status;
    }

    return BQ4050_readReg16(dev, BQ4050_DESIGN_CAPACITY_REG, capacity_mAh);
}
//------------------------------------------------------------------------------
//SHUTDOWN modba lepes.
status_t BQ4050_shutDown(BQ4050_t* dev)
{
    //A shutdown paarncsot ketszer kell a BMS-re kuldeni, hogy azt ervenyre is
    //juttassa!
    status_t status;
    status=BQ4050_writeReg16(dev,
                             BQ4050_MANUFACTURER_ACCESS_REG,
                             BQ4050_MAC_SHUTDOWN);
    if (status) return status;
    status=BQ4050_writeReg16(dev,
                             BQ4050_MANUFACTURER_ACCESS_REG,
                             BQ4050_MAC_SHUTDOWN);
    return status;
}
//------------------------------------------------------------------------------
//Akkumulator pakk toltesi feszultsegkorlat lekerdezese (mV-ban)
status_t BQ4050_getChargingVoltage_mV(BQ4050_t* dev, uint16_t *voltage_mV)
{
    return BQ4050_readReg16(dev, BQ4050_CHARGING_VOLTAGE_REG, voltage_mV);
}
//------------------------------------------------------------------------------
//Akkumulator pakk toltesi aramkorlat lekerdezese (mV-ban)
status_t BQ4050_getChargingCurrent_mA(BQ4050_t* dev, int16_t *current_mA)
{
    return BQ4050_readReg16(dev, BQ4050_CHARGING_CURRENT_REG, (uint16_t*)current_mA);
}

//------------------------------------------------------------------------------
//SafetyStatus regiszter lekerdezese
status_t BQ4050_getSafetyStatus(BQ4050_t* dev, uint16_t *safetyStatus)
{
    return BQ4050_readManuAccReg16(dev, BQ4050_SAFETY_STATUS_REG,  safetyStatus);
}
//------------------------------------------------------------------------------
//PFStatus regiszter lekerdezese
status_t BQ4050_getPfStatus(BQ4050_t* dev, uint16_t *pfStatus)
{
    return BQ4050_readManuAccReg16(dev, BQ4050_PF_STATUS_REG,  pfStatus);
}
//------------------------------------------------------------------------------
//OperationStatus regiszter lekerdezese
status_t BQ4050_getOperationStatus(BQ4050_t* dev, uint16_t *opStatus)
{
    return BQ4050_readManuAccReg16(dev,  BQ4050_OPERATION_STATUS_REG,  opStatus);
}
//------------------------------------------------------------------------------
