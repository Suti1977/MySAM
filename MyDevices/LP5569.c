//------------------------------------------------------------------------------
//  LP5569 I2C-s LED meghajto driver
//
//    File: LP5569.c
//------------------------------------------------------------------------------
#include "LP5569.h"
#include <string.h>

//------------------------------------------------------------------------------
//Eszkoz letrehozasa
void LP5569_create(LP5569_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);
}
//------------------------------------------------------------------------------
//Az IC egy 8 bites regiszterenek irasa
status_t LP5569_writeReg(LP5569_t* dev, uint8_t address, uint8_t regValue)
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
//Az IC tobb 8 bites regiszterenek irasa
status_t LP5569_writeMultipleRegs(LP5569_t* dev,
                                  uint8_t address,
                                  const uint8_t* values,
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
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, (uint8_t*)values, length   },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    return MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));
}
//------------------------------------------------------------------------------
//Az IC egy 8 bites regiszterenek olvasasa
status_t LP5569_readReg(LP5569_t* dev, uint8_t address, uint8_t* regValue)
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
//LP5569 konfiguralasa
status_t LP5569_config( LP5569_t* dev, const LP5569_Config_t* config)
{
    status_t status;
    uint8_t regValue=0;
    regValue |= (uint8_t)config->internalClk << 0;
    regValue |= (uint8_t)config->cpReturn1x << 2;
    regValue |= (uint8_t)config->cpMode << 3;
    regValue |= (uint8_t)config->powerSaving << 5;
    regValue |= (uint8_t)config->autoIncrement << 6;

    status=LP5569_writeReg(dev, LP5569_REG_MISC, regValue);
    if (status) return status;

    if (config->cpDisableDischarge)
    {   //Shutdown alatti charge pump kisutes tiltasa.
        status=LP5569_writeReg(dev, LP5569_REG_MISC2, 1);
    }

    return status;
}
//LED vezerles beallitasa
status_t LP5569_setLedControl(LP5569_t* dev,
                              uint8_t ledIndex,
                              LP5569_MF_MAPPING_t masterFader,
                              bool ratioMetricDimming,
                              bool exponentialAdjustment,
                              bool externalPowerd)
{
    uint8_t regValue;
    regValue=(uint8_t) masterFader;
    if (ratioMetricDimming)    regValue |= BIT(4);
    if (exponentialAdjustment) regValue |= BIT(3);
    if (externalPowerd)        regValue |= BIT(2);

    //(Kihasznaljuk, hogy a LED_CONTROL regiszterek egymas utan helyezkednek el)
    return LP5569_writeReg(dev, LP5569_REG_LED0_CONTROL+ledIndex, regValue);
}
//------------------------------------------------------------------------------
//LED csatorna PWM beallitasa (0x00-0% 0x80-50% 0xff-100%)
status_t LP5569_setPWM(LP5569_t* dev,
                       uint8_t ledIndex,
                       uint8_t pwmValue)
{
    return LP5569_writeReg(dev, LP5569_REG_LED0_PWM+ledIndex, pwmValue);
}
//------------------------------------------------------------------------------
//LED csatorna aram beallitasa (0x00-0mA 0x01-0.1mA 0xaf-17.5mA 0xFF-25.5mA)
status_t LP5569_setCurrent( LP5569_t* dev,
                            uint8_t ledIndex,
                            uint8_t currentValue)
{
    return LP5569_writeReg(dev, LP5569_REG_LED0_CURRENT+ledIndex, currentValue);
}
//------------------------------------------------------------------------------
//Az osszes LED csatorna (9) aramanak csoportos beallitasa.
//(0x00-0mA 0x01-0.1mA 0xaf-17.5mA 0xFF-25.5mA)
status_t LP5569_setAllCurrent( LP5569_t* dev, const uint8_t* currentValues)
{
    return LP5569_writeMultipleRegs(dev,
                                    LP5569_REG_LED0_CURRENT,
                                    currentValues,
                                    9);
}
//------------------------------------------------------------------------------
//A 3 master fader valamelyikenek beallitasa
status_t LP5569_setMasterFader( LP5569_t* dev,
                                uint8_t faderIndex,
                                uint8_t faderValue)
{
    return  LP5569_writeReg(dev,
                            LP5569_REG_MASTER_FADER1+faderIndex,
                            faderValue);
}
//------------------------------------------------------------------------------
//Az osszes led egy lepesben torteno beallitasa
status_t LP5569_setAllPwm( LP5569_t* dev,
                           uint8_t* pwmValues)
{
    return LP5569_writeMultipleRegs(dev, LP5569_REG_LED0_PWM, pwmValues, 9);
}
//------------------------------------------------------------------------------
//IC engedelyezese
status_t LP5569_enable(LP5569_t* dev)
{
    uint8_t regVal;

    status_t status=LP5569_readReg(dev, LP5569_REG_CONFIG, &regVal);
    if (status) return status;

    regVal |=LP5569_CONFIG_CHIP_EN;

    return LP5569_writeReg(dev, LP5569_REG_CONFIG, regVal);
}
//------------------------------------------------------------------------------
//IC tiltasa
status_t LP5569_disable(LP5569_t* dev)
{
    uint8_t regVal;

    status_t status=LP5569_readReg(dev, LP5569_REG_CONFIG, &regVal);
    if (status) return status;

    regVal &=~LP5569_CONFIG_CHIP_EN;

    return LP5569_writeReg(dev, LP5569_REG_CONFIG, regVal);
}
//------------------------------------------------------------------------------
//IC szoftveres reset
status_t LP5569_reset(LP5569_t* dev)
{
    //0xff-et irva a RESET registrebe
    return LP5569_writeReg(dev, LP5569_REG_RESET, 0xff);
}

