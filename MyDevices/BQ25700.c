//------------------------------------------------------------------------------
//  BQ25700 akkumulator tolto IC driver
//
//    File: BQ25700.c
//------------------------------------------------------------------------------
#include "BQ25700.h"
#include <string.h>

static status_t BQ25700_adc_start(BQ25700_t* dev, int adc_en_mask);
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben, tovabba driver beallitasa
void BQ25700_create(BQ25700_t* dev,
                    MyI2CM_t* i2c,
                    uint8_t slaveAddress,
                    const BQ25700_config_t* cfg)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);

    //Be es kimeneti soros ellenallas konfiguraciok beallitasa, mely alapjan
    //az aramo ertekek atvaltasat el tudja vegezni.
    dev->rac=cfg->rac;
    dev->rsr=cfg->rsr;
}
//------------------------------------------------------------------------------
//Az IC tobb regiszterenek olvasasa
status_t BQ25700_readRegs(BQ25700_t* dev,
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
//Az IC egyetlen 16 bites regiszterenek olvasasa
status_t BQ25700_readReg16(BQ25700_t* dev, uint8_t regAddress, uint16_t* data)
{
    return BQ25700_readRegs(dev, regAddress, (uint8_t*) data, 2);
}
//------------------------------------------------------------------------------
//Az IC egyetlen 16 bites regiszterenek irasa
status_t BQ25700_writeReg16(BQ25700_t* dev, uint8_t regAddress, uint16_t regValue)
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
status_t BQ25700_get_low_power_mode(BQ25700_t* dev, bool *mode)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, &reg);
    if (status) goto error;

    *mode = !!(reg & BQ25700_CHARGE_OPTION_0_EN_LWPWR);
error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_set_low_power_mode(BQ25700_t* dev, bool enable)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, &reg);
    if (status) goto error;

    if (enable)
        reg |= BQ25700_CHARGE_OPTION_0_EN_LWPWR;
    else
        reg &= ~BQ25700_CHARGE_OPTION_0_EN_LWPWR;

    status = BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_0_REG, reg);
error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_learn_mode(BQ25700_t* dev, bool *mode)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, &reg);
    if (status) goto error;

    *mode = !!(reg & BQ25700_CHARGE_OPTION_0_EN_LEARN);

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_set_learn_mode(BQ25700_t* dev, bool enable)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, &reg);
    if (status) goto error;

    if (enable)
        reg |= BQ25700_CHARGE_OPTION_0_EN_LEARN;
    else
        reg &= ~BQ25700_CHARGE_OPTION_0_EN_LEARN;

    status = BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_0_REG, reg);

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_inhibit_mode(BQ25700_t* dev, bool *mode)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, &reg);
    if (status) goto error;

    *mode = !!(reg & BQ25700_CHARGE_OPTION_0_CHRG_INHIBIT);

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_set_inhibit_mode(BQ25700_t* dev, bool enable)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, &reg);
    if (status) goto error;

    if (enable)
        reg |= BQ25700_CHARGE_OPTION_0_CHRG_INHIBIT;
    else
        reg &= ~BQ25700_CHARGE_OPTION_0_CHRG_INHIBIT;

    status = BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_0_REG, reg);

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_idpm_mode(BQ25700_t* dev, bool *mode)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, &reg);
    if (status) goto error;

    *mode = !!(reg & BQ25700_CHARGE_OPTION_0_EN_IDPM);

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_set_idpm_mode(BQ25700_t* dev, bool enable)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, &reg);
    if (status) goto error;

    if (enable)
        reg |= BQ25700_CHARGE_OPTION_0_EN_IDPM;
    else
        reg &= ~BQ25700_CHARGE_OPTION_0_EN_IDPM;

    status = BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_0_REG, reg);

error:
    return status;
}
//------------------------------------------------------------------------------
static status_t BQ25700_adc_start(BQ25700_t* dev, int adc_en_mask)
{
    status_t status;
    uint16_t reg;
    bool mode;
    int tries_left = 30;

    // Save current mode to restore same state after ADC read
    status = BQ25700_get_low_power_mode(dev, &mode);
    if (status) goto error;

    // Exit low power mode so ADC conversion takes typical time
    status = BQ25700_set_low_power_mode(dev, false);
    if (status) goto error;

    // Turn on the ADC for one reading. Note that adc_en_mask
    // maps to bit[7:0] in ADCOption register.
    reg = (adc_en_mask & BQ25700_ADC_OPTION_EN_ADC_ALL) |
          BQ25700_ADC_OPTION_ADC_START;
    status = BQ25700_writeReg16(dev, BQ25700_ADC_OPTION_REG, reg);
    if (status) goto error;

    // Wait until the ADC operation completes. The spec says typical
    // conversion time is 10 msec. If low power mode isn't exited first,
    // then the conversion time jumps to ~60 msec.
    do
    {
        vTaskDelay(2);
        BQ25700_readReg16(dev, BQ25700_ADC_OPTION_REG, &reg);
    } while (--tries_left && (reg & BQ25700_ADC_OPTION_ADC_START));

    // ADC reading attempt complete, go back to low power mode
    status = BQ25700_set_low_power_mode(dev, mode);
    if (status) goto error;

    // Could not complete read
    if (reg & BQ25700_ADC_OPTION_ADC_START)
        status=kStatus_Fail;                    //TODO: hibakodot adni

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_charge_current_limit(BQ25700_t* dev, int *current)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_CHARGE_CURRENT_REG, &reg);
    if (status) goto error;

    if (reg==0)
    {
        *current = 0;
    } else
    {
        if (dev->rsr==BQ25700_SERIES_RES_20)
        {   //20mOhm-os bemeneti soros ellenallas
            *current = reg  / 2;
        } else
        {   //10mOhm-os bemeneti soros ellenallas. Mas esetekben az osztast a
            //hivo oldalon kell elvegezni.
            *current = reg;
        }
    }

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_set_charge_current_limit(BQ25700_t* dev, int current_mA)
{
    uint16_t reg;

    if (dev->rsr==BQ25700_SERIES_RES_20)
    {   //20mOhm-os bemeneti soros ellenallas
        reg = (uint16_t)current_mA * 2;
    } else
    {   //10mOhm-os bemeneti soros ellenallas. Mas esetekben az osztast a
        //hivo oldalon kell elvegezni.
        reg = (uint16_t)current_mA;
    }

    return BQ25700_writeReg16(dev, BQ25700_CHARGE_CURRENT_REG, reg);
}
//------------------------------------------------------------------------------
status_t BQ25700_get_charge_voltage_limit(BQ25700_t* dev, int *voltage_mV)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_MAX_CHARGE_VOLTAGE_REG, &reg);
    if (status) goto error;

    *voltage_mV = reg;

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_set_charge_voltage_limit(BQ25700_t* dev, int voltage_mV)
{
    return BQ25700_writeReg16(dev,
                              BQ25700_MAX_CHARGE_VOLTAGE_REG,
                              voltage_mV & 0xffff);
}
//------------------------------------------------------------------------------
status_t BQ25700_set_input_current_limit(BQ25700_t* dev, int input_current_mA)
{
    uint16_t reg;
    reg= (uint16_t) input_current_mA / BQ25700_CHARGE_MA_PER_STEP;

    if (dev->rac==BQ25700_SERIES_RES_20)
    {   //20mOhm-os bemeneti soros ellenallas
        reg *=2;
    }

    reg <<= BQ25700_CHARGE_IIN_BIT_0FFSET;

    return BQ25700_writeReg16(dev, BQ25700_IIN_HOST_REG, reg);
}
//------------------------------------------------------------------------------
status_t BQ25700_get_input_current_limit(BQ25700_t* dev, int *input_current_mA)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_IIN_DPM_REG, &reg);
    if (status) goto error;

    reg >>= BQ25700_IIN_DPM_BIT_OFFSET;

    if (reg==0)
    {
        *input_current_mA = 0;
    } else
    {
        if (dev->rac==BQ25700_SERIES_RES_20)
        {   //20mOhm-os bemeneti soros ellenallas
            *input_current_mA = (reg * BQ25700_IIN_DPM_STEP)/2;
        } else
        {   //10mOhm-os bemeneti soros ellenallas. Mas esetekben az osztast a
            //hivo oldalon kell elvegezni.
            *input_current_mA = (reg * BQ25700_IIN_DPM_STEP);
        }
    }

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_input_voltage_limit(BQ25700_t* dev, int *voltage_mV)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_INPUT_VOLTAGE_REG, &reg);
    if (status) goto error;

    *voltage_mV = reg + BQ25700_INPUT_VOLTAGE_BASE_MV;

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_set_input_voltage_limit(BQ25700_t* dev, int voltage_mV)
{
    voltage_mV -= BQ25700_INPUT_VOLTAGE_BASE_MV;
    if (voltage_mV < 0) voltage_mV = 0;

    return BQ25700_writeReg16(dev,
                              BQ25700_INPUT_VOLTAGE_REG,
                              voltage_mV & 0xffff);
}
//------------------------------------------------------------------------------
status_t BQ25700_get_system_voltage_limit(BQ25700_t* dev, int *voltage_mV)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_MIN_SYSTEM_VOLTAGE_REG, &reg);
    if (status) goto error;

    *voltage_mV = reg;

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_set_system_voltage_limit(BQ25700_t* dev, uint16_t voltage_mV)
{
    return BQ25700_writeReg16(dev,
                              BQ25700_MIN_SYSTEM_VOLTAGE_REG,
                              (voltage_mV & 0xff00) );
}
//------------------------------------------------------------------------------
status_t BQ25700_manufacturer_id(BQ25700_t* dev, uint16_t *id)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_MANUFACTURER_ID_REG, &reg);
    if (status) goto error;

    *id = reg;

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_device_id(BQ25700_t* dev, uint16_t *id)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_readReg16(dev, BQ25700_DEVICE_ADDRESS_REG, &reg);
    if (status) goto error;

    *id = reg;

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_vbus_voltage(BQ25700_t* dev, int *voltage_mV)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_adc_start(dev, BQ25700_ADC_OPTION_EN_ADC_VBUS);
    if (status) goto error;

    status = BQ25700_readReg16(dev, BQ25700_ADC_VBUS_PSYS_REG, &reg);
    if (status) goto error;

    reg >>= BQ25700_ADC_VBUS_STEP_BIT_OFFSET;

    // LSB => 64mV.
    // Return 0 when VBUS <= 3.2V as ADC can't measure it.
    *voltage_mV = reg ?
           (reg * BQ25700_ADC_VBUS_STEP_MV + BQ25700_ADC_VBUS_BASE_MV) : 0;
error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_option_0(BQ25700_t* dev, uint16_t *option)
{
    return BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_0_REG, option);
}
//------------------------------------------------------------------------------
status_t BQ25700_set_option_0(BQ25700_t* dev, uint16_t option)
{
    return BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_0_REG, option);
}
//------------------------------------------------------------------------------
status_t BQ25700_get_option_1(BQ25700_t* dev, uint16_t *option)
{
    return BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_1_REG, option);
}
//------------------------------------------------------------------------------
status_t BQ25700_set_option_1(BQ25700_t* dev, uint16_t option)
{
    return BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_1_REG, option);
}
//------------------------------------------------------------------------------
status_t BQ25700_get_option_2(BQ25700_t* dev, uint16_t *option)
{
    return BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_2_REG, option);
}
//------------------------------------------------------------------------------
status_t BQ25700_set_option_2(BQ25700_t* dev, uint16_t option)
{
    return BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_2_REG, option);
}
//------------------------------------------------------------------------------
status_t BQ25700_get_option_3(BQ25700_t* dev, uint16_t *option)
{
    return BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_3_REG, option);
}
//------------------------------------------------------------------------------
status_t BQ25700_set_option_3(BQ25700_t* dev, uint16_t option)
{
    return BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_3_REG, option);
}
//------------------------------------------------------------------------------
status_t BQ25700_get_iin_current(BQ25700_t* dev, int *current_mA)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_adc_start(dev, BQ25700_ADC_OPTION_EN_ADC_IIN);
    if (status) goto error;

    status = BQ25700_readReg16(dev, BQ25700_ADC_CMPIN_IIN_REG, &reg);
    if (status) goto error;

    reg >>= BQ25700_ADC_IIN_STEP_BIT_OFFSET;
    reg &= 0xff;

    //bemeneti arammero ellenallas szerint eredmeny kiszamitasa
    if (dev->rac == BQ25700_SERIES_RES_20)
    {   //20mOhm
        *current_mA=((reg * BQ25700_ADC_IIN_STEP_MA + BQ25700_ADC_IIN_BASE_MA)/2);
    } else
    {   //10mOhm (vagy mas esetekben nincs osztas. Azt kivul kell elvegezni)
        *current_mA=((reg * BQ25700_ADC_IIN_STEP_MA + BQ25700_ADC_IIN_BASE_MA));
    }

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_idchg_current(BQ25700_t* dev, int *current_mA)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_adc_start(dev, BQ25700_ADC_OPTION_EN_ADC_IDCHG);
    if (status) goto error;

    status = BQ25700_readReg16(dev, BQ25700_ADC_IBAT_REG, &reg);
    if (status) goto error;

    reg >>= BQ25700_ADC_IDCHG_STEP_BIT_OFFSET;
    reg &= 0xff;

    //kimeneti arammero ellenallas szerint eredmeny kiszamitasa
    if (dev->rsr == BQ25700_SERIES_RES_20)
    {   //20mOhm
        *current_mA=(reg * BQ25700_ADC_IDCHG_STEP_MA + BQ25700_ADC_IDCHG_BASE_MA)/2;
    } else
    {   //10mOhm (vagy mas esetekben nincs osztas. Azt kivul kell elvegezni)
        *current_mA=(reg * BQ25700_ADC_IDCHG_STEP_MA + BQ25700_ADC_IDCHG_BASE_MA);
    }

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_ichg_current(BQ25700_t* dev, int *current_mA)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_adc_start(dev, BQ25700_ADC_OPTION_EN_ADC_ICHG);
    if (status) goto error;

    status = BQ25700_readReg16(dev, BQ25700_ADC_IBAT_REG, &reg);
    if (status) goto error;

    reg >>= BQ25700_ADC_ICHG_STEP_BIT_OFFSET;
    reg &= 0xff;

    //kimeneti arammero ellenallas szerint eredmeny kiszamitasa
    if (dev->rsr == BQ25700_SERIES_RES_20)
    {   //20mOhm
        *current_mA=(reg*BQ25700_ADC_ICHG_STEP_MA + BQ25700_ADC_ICHG_BASE_MA)/2;
    } else
    {   //10mOhm (vagy mas esetekben nincs osztas. Azt kivul kell elvegezni)
        *current_mA=(reg*BQ25700_ADC_ICHG_STEP_MA + BQ25700_ADC_ICHG_BASE_MA);
    }

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_vsys_voltage(BQ25700_t* dev, int *voltage_mV)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_adc_start(dev, BQ25700_ADC_OPTION_EN_ADC_VSYS);
    if (status) goto error;

    status = BQ25700_readReg16(dev, BQ25700_ADC_VSYS_VBAT_REG, &reg);
    if (status) goto error;

    reg >>= BQ25700_ADC_VSYS_STEP_BIT_OFFSET;
    reg &= 0xff;

    *voltage_mV = reg ?
           (reg * BQ25700_ADC_VSYS_STEP_MV + BQ25700_ADC_VSYS_BASE_MV) : 0;

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_vbat_voltage(BQ25700_t* dev, int *voltage_mV)
{
    status_t status;
    uint16_t reg;

    status = BQ25700_adc_start(dev, BQ25700_ADC_OPTION_EN_ADC_VBAT);
    if (status) goto error;

    status = BQ25700_readReg16(dev, BQ25700_ADC_VSYS_VBAT_REG, &reg);
    if (status) goto error;

    reg >>= BQ25700_ADC_VBAT_STEP_BIT_OFFSET;
    reg &= 0xff;

    *voltage_mV = reg ?
           (reg * BQ25700_ADC_VBAT_STEP_MV + BQ25700_ADC_VBAT_BASE_MV) : 0;

error:
    return status;
}
//------------------------------------------------------------------------------
status_t BQ25700_get_charger_status(BQ25700_t* dev, uint16_t *statusBits)
{
    return BQ25700_readReg16(dev, BQ25700_CHARGER_STATUS_REG, statusBits);
}
//------------------------------------------------------------------------------
status_t BQ25700_reset(BQ25700_t* dev)
{
    status_t status;
    uint16_t reg;

    //Reset elott normal modba kapcsoljuk a chipet
    status=BQ25700_set_low_power_mode(dev, false);
    if (status) goto error;

    //Varakozas, hogy a VDDA bealljon
    vTaskDelay(20);

    //vsys regiszter ertekenek elmentese...
    uint16_t vsys_save;
    status=BQ25700_readReg16(dev, BQ25700_MIN_SYSTEM_VOLTAGE_REG, &vsys_save);
    if (status) goto error;

    //Chip reset...
    status = BQ25700_readReg16(dev, BQ25700_CHARGE_OPTION_3_REG, &reg);
    if (status) goto error;

    //Minden regisztert default-ba allitunk
    reg |= BQ25700_CHARGE_OPTION_3_RESET_REG;
    status = BQ25700_writeReg16(dev, BQ25700_CHARGE_OPTION_3_REG, reg);
    if (status) goto error;

    //VSYS regiszter helyreallitasa...
    status = BQ25700_writeReg16(dev, BQ25700_MIN_SYSTEM_VOLTAGE_REG, vsys_save);
    if (status) goto error;

    status=BQ25700_set_low_power_mode(dev, true);
    if (status) goto error;

    return status;

error:
    //hiba eseten megprobalja alacsony fogyasztasu modba kapcsolni a chipet.
    //De ha evevl hiba lenne, akkor annak a hibakodjat mar elnyeljuk.
    BQ25700_set_low_power_mode(dev, true);
    return status;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
