//------------------------------------------------------------------------------
//  Texas Instruments HDC1080 paratartalom es homerseklet mero driver
//
//    File: HDC1080.c
//------------------------------------------------------------------------------
//  Alkalmazott chip: HDC1080
//  16 bites regiszterparokkal operal. 8 bites a pointere.
//------------------------------------------------------------------------------
#include "HDC1080.h"
//------------------------------------------------------------------------------
//I2C eszkoz letrehozasa a rendszerben
void HDC1080_create(HDC1080_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    //I2C eleres letrehozasa
    MyI2CM_createDevice(&dev->i2cDevice, i2c, slaveAddress, NULL);
}
//------------------------------------------------------------------------------
//Az IC olvasasa
status_t HDC1080_read(HDC1080_t* dev,
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
//Az IC egy 16 bites regiszterenek irasa
status_t HDC1080_writeReg(HDC1080_t* dev, uint8_t address, uint16_t regValue)
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
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd,                 sizeof(cmd) },
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, (uint8_t*)&regValue, 2           },
    };
    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Meres inditasa
//0x00- cimre allitja az IC-ben a pointert, mikozben nincs mas adatatvitel.
//Erre indul a meres.
status_t HDC1080_startMeasure(HDC1080_t* dev)
{
    status_t status;

    //cim (pointer) beallitasa 8 biten
    uint8_t cmd[1];
    cmd[0]= HDC1080_TEMPERATURE;

    MyI2CM_xfer_t xferBlocks[]=
    {
        (MyI2CM_xfer_t){MYI2CM_FLAG_TX, cmd, sizeof(cmd) },
    };

    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//Meres inditasa.
status_t HDC1080_measure(HDC1080_t* dev,
                         uint16_t* temperature,
                         uint16_t*  humidity)
{
    status_t status;

    //IC Reset, tovabba config beallitasa. 14 bites meres!
    uint16_t configRegValue=0x1000;

    status=HDC1080_writeReg(dev, HDC1080_CONFIG, configRegValue);
    if (status)  goto error;

    //Varakozas, amig az eszkoz elindul.
    vTaskDelay(10);

    //meres inditasa
    status=HDC1080_startMeasure(dev);
    if (status)  goto error;

    //Tapasztalat. Ha itt nem varunk, akkor az eszkoz kesobb NACK-t fog adni.
    vTaskDelay(20);

    union
    {
        uint8_t buff[4];
        #pragma pack(1)
        struct
        {
          uint16_t temp;
          uint16_t humy;
        };
        #pragma pack()
    } result;


    for(int i=0; i<10; i++)
    {
        //Olvasas...
        //Adatatviteli blokk leirok listajnak osszeallitasa.
        //(Ez a stcaken marad, amig le nem megy a transzfer!)
        MyI2CM_xfer_t xferBlocks[]=
        {
            (MyI2CM_xfer_t){MYI2CM_FLAG_RX, result.buff, sizeof(result) },
        };
        //I2C mukodes kezdemenyezese.
        //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
        status=MYI2CM_transfer(&dev->i2cDevice, xferBlocks, ARRAY_SIZE(xferBlocks));

        if (status==kStatus_Success) break;
        vTaskDelay(1);
    }
    if (status)  goto error;


    //Eredmenyek olvasasa...
    if (temperature)
    {
        *temperature=__builtin_bswap16(result.temp);
    }

    if (humidity)
    {
        *humidity=__builtin_bswap16(result.humy);
    }


error:
    return  status;
}
//------------------------------------------------------------------------------
//Eszkoz altal visszaadott meresi eredmenybol celsius fokra konvertalas
double HDC1080_value2Celsius(uint16_t temperatureValue)
{
    return (double) ((uint32_t)temperatureValue*165) / (65536.0)-40.0;
}
//------------------------------------------------------------------------------
//Eszkoz altal visszaadott meresi eredmenybol %-os paratartalomakonvertalas
double HDC1080_value2Humidity(uint16_t humidityValue)
{
    return (double) ((uint32_t)humidityValue*100) / (65536.0);
}
//------------------------------------------------------------------------------
//Egyedi azonosito olvasasa.  Az ID hossza: HDC1080_SERIAL_ID_LEN.
status_t HDC1080_readSerialId(HDC1080_t* dev,
                              uint8_t* buff,
                              uint8_t length)
{
    return HDC1080_read(dev, HDC1080_SERIALID1, buff, length);
}
//------------------------------------------------------------------------------
