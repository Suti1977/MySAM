//------------------------------------------------------------------------------
//  Microchip 24AA02E48 tipus EUI-48 egyedi azonosito es EEprom driver
//
//    File: M24AA02E48.c
//------------------------------------------------------------------------------
#include "M24AA02E48.h"
#include <string.h>

//Az eepromban itt kezdodik az egyedi azonosito
#define EUI48_START_ADDRESS   0xFA

//------------------------------------------------------------------------------
//Memoria handler letrehozasa
void M24AA02E48_create(MyI2CMem_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    MyI2CMem_Config_t cfg=
    {
        //I2C periferia (busz) driver
        .i2c=i2c,
        //Memoria slave cime
        .slaveAddress=slaveAddress,
        //Az address mezo hossza a cimzesnel 1 byte
        .addressFieldSize=1,
        //Iro blokk merete: 8 byte
        .writeBlockSize=8,
    };
    MyI2CMem_create(dev, &cfg);
}
//------------------------------------------------------------------------------
//48 bites egyedi azonosito kiolvasasa
status_t M24AA02E48_readUID( MyI2CMem_t* dev,
                             uint8_t* buffer,
                             uint32_t bufferSize)
{
    //Csak maximum az azonosito hosszanyit olvasunk
    if (bufferSize>EUI48_LENGTH) bufferSize=EUI48_LENGTH;

    return MyI2CMem_read(dev, EUI48_START_ADDRESS, buffer, bufferSize);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
