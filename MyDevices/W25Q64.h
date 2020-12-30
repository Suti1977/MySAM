//------------------------------------------------------------------------------
//  W25Q64 Winbond NOR FLash driver
//
//    File: W25Q64.h
//------------------------------------------------------------------------------
#ifndef W25Q64_H_
#define W25Q64_H_

#include "MyQSPI.h"

//Chipben tarolt egyedi azonosito hossza (byte-ban)
#define W25Q64_UID_LENGTH               8

//Iro lap merete
#define W25Q64_PAGE_SIZE                256

//A legkisebb egyben torolheto szektor merete
#define W25Q64_MIN_ERASE_SECTOR_SIZE    4096
//A chip ezen kivul rendelkezik 32kbyte es 64kbyte tovabba a teljes chipet
//torlo erase lehetosegekkel

//A chip elkeszultenek tesztelesehez kulso callback hivas funkcio
typedef void W25Q64_extWaitFunc_t(void* callbackData);
//------------------------------------------------------------------------------
//W25Q64 valtozoi
typedef struct
{
    //A chip elereset biztosito QSPI driverre mutato pinter.
    MyQSPI_t*    qspi;

    //busz szelesseg szerinti parameter.
    //MYQSPI_INST1_ADDR1_DATA1
    //MYQSPI_INST4_ADDR4_DATA4
    uint32_t     busWidth;

    //A chip elkeszultenek tesztelesehez kulso callback hivas funkcio
    W25Q64_extWaitFunc_t* waitFunc;
    void* waitFuncCallbackData;

} W25Q64_t;
//------------------------------------------------------------------------------
//Nor flash letrehozasa
void W25Q64_create(W25Q64_t* dev,
                   MyQSPI_t* qspi,
                   W25Q64_extWaitFunc_t* waitFunc,
                   void* waitFunCallbackData);

//Regiszter olvasasa
void W25Q64_readReg(W25Q64_t* dev, uint8_t instr, uint8_t* buff, uint32_t len);
//Regiszter irasa
void W25Q64_writeReg(W25Q64_t* dev, uint8_t instr, uint8_t* buff, uint32_t len);

//Chip statusz-1 regiszter olvasasa
uint8_t W25Q64_readStatus1(W25Q64_t* dev);

//Chip statusz-2 regiszter olvasasa
uint8_t W25Q64_readStatus2(W25Q64_t* dev);

//Chip statusz-3 regiszter olvasasa
uint8_t W25Q64_readStatus3(W25Q64_t* dev);

//Chip statusz-1 regiszter irasa
void W25Q64_writeStatus1(W25Q64_t* dev, uint8_t regValue);

//Chip statusz-2 regiszter irasa
void W25Q64_writeStatus2(W25Q64_t* dev, uint8_t regValue);

//Chip statusz-1,2 regiszterek irasa
void W25Q64_writeStatus12(W25Q64_t* dev, uint16_t regValue);

//Chip statusz-3 regiszter irasa
void W25Q64_writeStatus3(W25Q64_t* dev, uint8_t regValue);

//A flash foglaltsag lekerdezese
bool W25Q64_isBusy(W25Q64_t* dev);

//Iras engedelyezes
void W25Q64_writeEnable(W25Q64_t* dev);

//Chip szoftveres resetje
void W25Q64_reset(W25Q64_t* dev);

//Chip 4 bites uzemmodba leptetese
void W25Q64_qspiEnable(W25Q64_t* dev);

//Chip QPI uzemmodba leptetese
//A chip ebben az uzemmodban mind a parancsokat, mind pedig az adatokat is a
//4 bites interfacen keresztul fogadja.
void W25Q64_enterQpiMode(W25Q64_t* dev);

//QPI modbol torteno kileptetes
void W25Q64_exitQpiMode(W25Q64_t* dev);

//Egyedi azonosito olvasasa.
//Megjegyzes: Ezt csak standard SPI modban lehet kiolvasni
void W25Q64_readUid(W25Q64_t* dev, uint8_t* buff, uint32_t length);

//Eszkoz alacsony fogyasztasu modba leptetese
void W25Q64_powerDown(W25Q64_t* dev);

//Eszkoz alacsony fogyasztasu modbol ebresztese
void W25Q64_powerUp(W25Q64_t* dev);

//Olvasas (QPI-ben csak fast read alkalmazhato)
void W25Q64_fastRead(W25Q64_t* dev,
                     uint32_t address,
                     void* buffer,
                     uint32_t len);

//Olvasas (SPI-ben)
void W25Q64_read(W25Q64_t* dev,
                 uint32_t address,
                 void* buffer,
                 uint32_t len);

//Lap irasa
void W25Q64_pageWrite(W25Q64_t* dev,
                      uint32_t address,
                      const void* buffer,
                      uint32_t len);

//4 kbyteos szektor torlese
void W25Q64_sectorErase4k(W25Q64_t* dev, uint32_t address);

//32 kbyteos szektor torlese
void W25Q64_blockErase32k(W25Q64_t* dev, uint32_t address);

//64 kbyteos szektor torlese
void W25Q64_sectorErase64k(W25Q64_t* dev, uint32_t address);

//teljes chip erase
void W25Q64_chipErase(W25Q64_t* dev);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //W25Q64_H_
