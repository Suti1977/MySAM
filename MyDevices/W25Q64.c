//------------------------------------------------------------------------------
//  W25Q64 Winbond NOR FLash driver
//
//    File: W25Q64.c
//------------------------------------------------------------------------------
//stuff: https://github.com/nimaltd/w25qxx/blob/master/w25qxx.c
//
#include "W25Q64.h"
#include <string.h>

//------------------------------------------------------------------------------
//Nor flash letrehozasa
void W25Q64_create(W25Q64_t* dev, MyQSPI_t* qspi)
{
    memset(dev, 0, sizeof (W25Q64_t));
    dev->qspi=qspi;
}
//------------------------------------------------------------------------------
//Regiszter olvasasa
void W25Q64_readReg(W25Q64_t* dev, uint8_t instr, uint8_t* buff, uint32_t len)
{
    MyQSPI_cmd_t cmd=
    {
        .instFrame.bits.width    = MYQSPI_INST1_ADDR1_DATA1,
        .instFrame.bits.inst_en  = 1,
        .instFrame.bits.data_en  = 1,
        .instFrame.bits.addr_en  = 0,
        .instFrame.bits.tfr_type = MYQSPI_READ_ACCESS,
        .instFrame.bits.dummy_cycles=0,
        .instruction             = instr,
        .bufLen                  = len,
        .rxBuf                   = buff,
        .address                 = 0,
    };

    MyQSPI_transfer(dev->qspi, &cmd);
}
//------------------------------------------------------------------------------
//Regiszter irasa
void W25Q64_writeReg(W25Q64_t* dev, uint8_t instr, uint8_t* buff, uint32_t len)
{
    MyQSPI_cmd_t cmd=
    {
        .instFrame.bits.width    = MYQSPI_INST1_ADDR1_DATA1,
        .instFrame.bits.inst_en  = 1,
        .instFrame.bits.data_en  = 1,
        .instFrame.bits.addr_en  = 0,
        .instFrame.bits.tfr_type = MYQSPI_WRITE_ACCESS,
        .instFrame.bits.dummy_cycles=0,
        .instruction             = instr,
        .bufLen                  = len,
        .txBuf                   = buff,
        .address                 = 0,
    };

    MyQSPI_transfer(dev->qspi, &cmd);
}
//------------------------------------------------------------------------------
//Chip statusz-1 regiszter olvasasa
void W25Q64_readStatus1(W25Q64_t* dev, uint8_t* regValue)
{
    W25Q64_readReg(dev, 0x05, regValue, 1);
}
//------------------------------------------------------------------------------
//Chip statusz-2 regiszter olvasasa
void W25Q64_readStatus2(W25Q64_t* dev, uint8_t* regValue)
{
    W25Q64_readReg(dev, 0x35, regValue, 1);
}
//------------------------------------------------------------------------------
//Chip statusz-3 regiszter olvasasa
void W25Q64_readStatus3(W25Q64_t* dev, uint8_t* regValue)
{
    W25Q64_readReg(dev, 0x15, regValue, 1);
}
//------------------------------------------------------------------------------
//Chip statusz-1 regiszter irasa
void W25Q64_writeStatus1(W25Q64_t* dev, uint8_t regValue)
{
    W25Q64_writeReg(dev, 0x01, &regValue, 1);
}
//------------------------------------------------------------------------------
//Chip statusz-2 regiszter irasa
void W25Q64_writeStatus2(W25Q64_t* dev, uint8_t regValue)
{
    W25Q64_writeReg(dev, 0x31, &regValue, 1);
}
//------------------------------------------------------------------------------
//Chip statusz-3 regiszter irasa
void W25Q64_writeStatus3(W25Q64_t* dev, uint8_t regValue)
{
    W25Q64_writeReg(dev, 0x11, &regValue, 1);
}
//------------------------------------------------------------------------------
//Chip 4 bites uzemmodba leptetese
void W25Q64_qspiEnable(W25Q64_t* dev)
{
    //A QSPI engedelyezesehez a status2 regiszter QE bitjet kell beallitani.
    uint8_t status2=0xaa;
    W25Q64_readStatus2(dev, &status2);
    printf("STATUS2: %02x\n", status2);
    W25Q64_writeStatus2(dev, status2 | 0x02);

    W25Q64_readStatus2(dev, &status2);
    printf("STATUS2: %02x\n", status2);

}
//------------------------------------------------------------------------------
//Egyedi azonosito olvasasa
status_t W25Q64_readUid(W25Q64_t* dev, uint8_t* buff, uint32_t length)
{

}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
