//------------------------------------------------------------------------------
//  Microchip 24AA02E48 tipus EUI-48 egyedi azonosito es EEprom driver
//
//    File: AT24MAC602.c
//------------------------------------------------------------------------------
#include "AT24MAC602.h"
#include <string.h>

//128 bites sorozatszam kezdocime
#define SERIAL128_START_ADDRESS     0x98
//Az eepromban itt kezdodik az EUI-64 azonosito
#define EUI64_START_ADDRESS         0x98

//------------------------------------------------------------------------------
//Memoria handler letrehozasa
void AT24MAC602_create(MyI2CMem_t* dev, MyI2CM_t* i2c, uint8_t slaveAddress)
{
    MyI2CMem_Config_t cfg=
    {
        //I2C periferia (busz) driver
        .i2c=i2c,
        //Memoria slave cime
        .slaveAddress=slaveAddress,
        //Az address mezo hossza a cimzesnel 1 byte
        .addressFieldSize=1,
        //Iro blokk merete: 16 byte
        .writeBlockSize=16,
    };
    MyI2CMem_create(dev, &cfg);
}
//------------------------------------------------------------------------------
//64 bites egyedi azonosito kiolvasasa
status_t AT24MAC602_readEUI64( MyI2CMem_t* dev,
                             uint8_t* buffer,
                             uint32_t bufferSize)
{
    //Csak maximum az azonosito hosszanyit olvasunk
    //if (bufferSize>EUI64_LENGTH) bufferSize=EUI64_LENGTH;

    //Mivel az AT24MAC602 mas slave cimen tartalmazza az EUI-64 adatokat,
    //az eredeti leirobol masolatot keszitunk, melyen a slave cimet modositjuk.
    //Ezt a modositott leirot adjuk at az olvaso rutinnak.
    MyI2CMem_t tempDesc;
    memcpy(&tempDesc, dev, sizeof(MyI2CMem_t));
    tempDesc.i2cDevice.slaveAddress |= 0x08;

    return MyI2CMem_read(&tempDesc, EUI64_START_ADDRESS, buffer, bufferSize);
}
//------------------------------------------------------------------------------
//128 bites egyedi sorszam olvasasa
status_t AT24MAC602_readSerial128( MyI2CMem_t* dev,
                                   uint8_t* buffer,
                                   uint32_t bufferSize)
{
    //Csak maximum az azonosito hosszanyit olvasunk
    //if (bufferSize>EUI64_LENGTH) bufferSize=EUI64_LENGTH;

    //Mivel az AT24MAC602 mas slave cimen tartalmazza a sorozatszam adatokat,
    //az eredeti leirobol masolatot keszitunk, melyen a slave cimet modositjuk.
    //Ezt a modositott leirot adjuk at az olvaso rutinnak.
    MyI2CMem_t tempDesc;
    memcpy(&tempDesc, dev, sizeof(MyI2CMem_t));
    tempDesc.i2cDevice.slaveAddress |= 0x80;

    return MyI2CMem_read(&tempDesc, SERIAL128_START_ADDRESS, buffer, bufferSize);
}
//------------------------------------------------------------------------------
//eeprom terulet olvasasa
status_t AT24MAC602_read( MyI2CMem_t* dev,
                          uint8_t address,
                          uint8_t* src,
                          uint32_t length)
{
    return MyI2CMem_read(dev, address, src, length);
}
//------------------------------------------------------------------------------
//eeprom terulet irasa
status_t AT24MAC602_write(MyI2CMem_t* dev,
                          uint8_t address,
                          uint8_t* dest,
                          uint32_t length)
{
    return MyI2CMem_write(dev, address, dest, length);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
