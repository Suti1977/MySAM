//------------------------------------------------------------------------------
//  Altalanos I2C buszon talalhato memoriak (EEPROM/FRAM/SARM) kezelese
//
//    File: MyI2CMem.h
//------------------------------------------------------------------------------
//
//  I2C buszra kotott memoriak elerese. Ez hasznalhato EEPROM, FRAM, SRAM eseten
//  is. Az alabbi ket cimzesi modot hasznalo memoriakhoz alkalmas:
//
//  16 bites cimzes:
//  +--------+--------------+--------+--------+------------+-------+
//	| Start  | I2CSlaveCim  |  AddrH | AddrL  | Adtaok.... |  Stop |
//  +--------+--------------+--------+--------+------------+-------+
//
//  8 bites cimzes:
//  +--------+--------------+--------+---------------------+-------+
//	| Start  | I2CSlaveCim  |  Addr  |        Adtaok....   |  Stop |
//  +--------+--------------+--------+---------------------+-------+
//
// (AddrH:AddrL-->memorian beluli max 16 bites cim)
// (Addr-->memorian beluli max 8 bites cim)
//
//------------------------------------------------------------------------------
#ifndef MY_I2CMEM_H_
#define MY_I2CMEM_H_

#include "MyI2CM.h"

//------------------------------------------------------------------------------
//I2C-s memoria letrehozasakor hasznalt konfiguracios struktura
typedef struct
{
    //I2C periferia (busz) driver
    MyI2CM_t*  i2c;
    //Memoria slave cime
    uint8_t     slaveAddress;
    //Az address mezo hossza a cimzesnel. Ennyi byteos. (1, 2, 3, 4)
    uint8_t     addressFieldSize;
    //Iro blokk merete. Ha ilyen nincs, akkor 0.
    uint16_t    writeBlockSize;
} MyI2CMem_Config_t;
//------------------------------------------------------------------------------
//MyI2CMem valtozoi
typedef struct
{
    //I2C eszkoz buszon valo parameterei
    MyI2CM_Device_t i2cDevice;

    //Az address mezo hossza a cimzesnel. Ennyi byteos. (1, 2, 3, 4)
    uint8_t         addressFieldSize;

    //Iro blokk merete. Ez ketto hatvanya lehet csak!
    uint16_t        writeBlockSize;
} MyI2CMem_t;
//------------------------------------------------------------------------------
//I2C-s memoria letrehozasa
void MyI2CMem_create(MyI2CMem_t* mem, const MyI2CMem_Config_t* cfg);

//I2C-s memoria olvasasa
//mem: Az eszkoz leiroja
//address: Olvasasi kezdocim
//data: Az olvasott adatokat ide helyezi
//length: Olvasott byteok szama
status_t MyI2CMem_read(MyI2CMem_t* mem,
                       uint32_t address,
                       uint8_t* data, uint32_t length);

//I2C-s memoria irasa.
//A fuggveny kezeli a memorian beluli irolapok kezeleset is.
//mem: Az eszkoz leiroja
//address: Irasi kezdocim
//data: A kiirando adatokra mutat
//length: Kiirando byteok szama
status_t MyI2CMem_write(MyI2CMem_t* mem,
                        uint32_t address,
                        uint8_t* data,
                        uint32_t length);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_I2CMEM_H_
