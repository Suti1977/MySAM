//------------------------------------------------------------------------------
//  W25Q64 Winbond NOR FLash driver
//
//    File: W25Q64.h
//------------------------------------------------------------------------------
#ifndef W25Q64_H_
#define W25Q64_H_

#include "MyQSPI.h"

//Chipben tarolt egyedi azonosito hossza (byte-ban)
#define W25Q64_UID_LENGTH   8

//------------------------------------------------------------------------------
//W25Q64 valtozoi
typedef struct
{
    //A chip elereset biztosito QSPI driverre mutato pinter.
    MyQSPI_t*    qspi;
} W25Q64_t;
//------------------------------------------------------------------------------
//Nor flash letrehozasa
void W25Q64_create(W25Q64_t* dev, MyQSPI_t* qspi);

//Regiszter olvasasa
void W25Q64_readReg(W25Q64_t* dev, uint8_t instr, uint8_t* buff, uint32_t len);
//Regiszter irasa
void W25Q64_writeReg(W25Q64_t* dev, uint8_t instr, uint8_t* buff, uint32_t len);


//Chip statusz-1 regiszter olvasasa
void W25Q64_readStatus1(W25Q64_t* dev, uint8_t* regValue);

//Chip statusz-2 regiszter olvasasa
void W25Q64_readStatus2(W25Q64_t* dev, uint8_t* regValue);

//Chip statusz-3 regiszter olvasasa
void W25Q64_readStatus3(W25Q64_t* dev, uint8_t* regValue);

//Chip statusz-1 regiszter irasa
void W25Q64_writeStatus1(W25Q64_t* dev, uint8_t regValue);

//Chip statusz-2 regiszter irasa
void W25Q64_writeStatus2(W25Q64_t* dev, uint8_t regValue);

//Chip statusz-3 regiszter irasa
void W25Q64_writeStatus3(W25Q64_t* dev, uint8_t regValue);

//Chip 4 bites uzemmodba leptetese
void W25Q64_qspiEnable(W25Q64_t* dev);

//Egyedi azonosito olvasasa
status_t W25Q64_readUid(W25Q64_t* dev, uint8_t* buff, uint32_t length);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //W25Q64_H_
