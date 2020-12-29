//------------------------------------------------------------------------------
//  W25Q64 Winbond NOR FLash driver
//
//    File: W25Q64.c
//------------------------------------------------------------------------------
//stuff: https://github.com/nimaltd/w25qxx/blob/master/w25qxx.c
//https://github.com/Xilinx/embeddedsw.github.io/blob/master/spi/examples/xspi_numonyx_flash_quad_example.c

#include "W25Q64.h"
#include <string.h>

//------------------------------------------------------------------------------
//Nor flash letrehozasa
void W25Q64_create(W25Q64_t* dev, MyQSPI_t* qspi)
{
    memset(dev, 0, sizeof (W25Q64_t));
    dev->qspi=qspi;

    //kezdetben az eszkozt 1 bites modban eri el.
    dev->busWidth=MYQSPI_INST1_ADDR1_DATA1;
}
//------------------------------------------------------------------------------
//Regiszter olvasasa
void W25Q64_readReg(W25Q64_t* dev, uint8_t instr, uint8_t* buff, uint32_t len)
{
    MyQSPI_cmd_t cmd=
    {
        .instFrame.bits.width    = dev->busWidth,
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
        .instFrame.bits.width    = dev->busWidth,
        .instFrame.bits.inst_en  = 1,
        .instFrame.bits.data_en  = (len != 0),
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
uint8_t W25Q64_readStatus1(W25Q64_t* dev)
{
    uint8_t regValue;
    W25Q64_readReg(dev, 0x05, &regValue, 1);
    return regValue;
}
//------------------------------------------------------------------------------
//Chip statusz-2 regiszter olvasasa
uint8_t W25Q64_readStatus2(W25Q64_t* dev)
{
    uint8_t regValue;
    W25Q64_readReg(dev, 0x35, &regValue, 1);
    return regValue;
}
//------------------------------------------------------------------------------
//Chip statusz-3 regiszter olvasasa
uint8_t W25Q64_readStatus3(W25Q64_t* dev)
{
    uint8_t regValue;
    W25Q64_readReg(dev, 0x15, &regValue, 1);
    return regValue;
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
//Chip statusz-1,2 regiszterek irasa
void W25Q64_writeStatus12(W25Q64_t* dev, uint16_t regValue)
{
    W25Q64_writeReg(dev, 0x01, (uint8_t*)&regValue, 2);
}
//------------------------------------------------------------------------------
//Chip statusz-3 regiszter irasa
void W25Q64_writeStatus3(W25Q64_t* dev, uint8_t regValue)
{
    W25Q64_writeReg(dev, 0x11, &regValue, 1);
}
//------------------------------------------------------------------------------
//A flash foglaltsag lekerdezese
bool W25Q64_isBusy(W25Q64_t* dev)
{
    //legalso bit 1, ha az eszkoz foglalt
    return (W25Q64_readStatus1(dev) & 1);
}
//------------------------------------------------------------------------------
//Iras engedelyezes
void W25Q64_writeEnable(W25Q64_t* dev)
{
    //Varakozas, amig az eszkoz foglalt...
    while(W25Q64_isBusy(dev));

    //Write enable parancs kiadasa
    W25Q64_writeReg(dev, 0x06, NULL, 0);
}
//------------------------------------------------------------------------------
//Chip Quad SPI uzemmodba leptetese.
//Ez lehetove teszi, hogy mig a parancsokat standard SPI-n keresztul fogadja,
//az adatokat egyes utasitasoknal (pl fast read) 4 bites interfacen kuldje es
//fogadja.
void W25Q64_qspiEnable(W25Q64_t* dev)
{    
    //QE bit allapotanak ellenorzese.
    uint8_t status1Reg=W25Q64_readStatus1(dev);
    uint8_t status2Reg=W25Q64_readStatus2(dev);

    if ((status2Reg & 0x02) == 0)
    {   //A QSPI funkcio nincs engedelyezve.

        //A QE bit a nem felejto memoriabit. Annak atprogramozasa idobe telik!
        //Ha "IQ" lenne a partnumberban, akkor ez gyarilag engedelyezve van.

        //Irasvedelem feloldasa szukseges az uzemmod valtashoz
        W25Q64_writeEnable(dev);

        //A QSPI engedelyezesehez a status2 regiszter QE bitjet kell beallitani.
        //Az alabbi parancsal 1 es 2 status regiszterek egyszere allnak be.

        uint16_t status12=(status2Reg | 0x02);
        status12 <<= 8;
        status12 |= status1Reg;
        W25Q64_writeStatus12(dev, status12);

        //Varakozas, amig az eszkoz foglalt...
        while(W25Q64_isBusy(dev));
    }
}
//------------------------------------------------------------------------------
//Chip QPI uzemmodba leptetese
//A chip ebben az uzemmodban mind a parancsokat, mind pedig az adatokat is a
//4 bites interfacen keresztul fogadja.
void W25Q64_enterQpiMode(W25Q64_t* dev)
{
    //Ha veletlenul (peldaul tapfesz reset miatt) a flash QPI modban uzemelne,
    //akkor azt elobb ki kell abbol leptetni.
    W25Q64_exitQpiMode(dev);

    //QSPI mod engedelyezese szukseges a QPI modba leptetes elott.
    W25Q64_qspiEnable(dev);

    //Enter QPI (38h)
    W25Q64_writeReg(dev, 0x38, NULL, 0);

    //mind a 4 adatvonalat hasznaljuk
    dev->busWidth=MYQSPI_INST4_ADDR4_DATA4;
}
//------------------------------------------------------------------------------
//QPI modbol torteno kileptetes
void W25Q64_exitQpiMode(W25Q64_t* dev)
{
    //mind a 4 adatvonalat hasznaljuk, amig a 0xff parancs kimegy
    dev->busWidth=MYQSPI_INST4_ADDR4_DATA4;

    //Exit QPI (FFh)
    W25Q64_writeReg(dev, 0xff, NULL, 0);

    //standard SPI modban mukodik a driver
    dev->busWidth=MYQSPI_INST1_ADDR1_DATA1;
}
//------------------------------------------------------------------------------
//Chip szoftveres resetje
void W25Q64_reset(W25Q64_t* dev)
{
    W25Q64_writeReg(dev, 0x66, NULL, 0);
    W25Q64_writeReg(dev, 0x99, NULL, 0);
}
//------------------------------------------------------------------------------
//Egyedi azonosito olvasasa.
//Megjegyzes: Ezt csak standard SPI modban lehet kiolvasni
void W25Q64_readUid(W25Q64_t* dev, uint8_t* buff, uint32_t length)
{
    //0x4B 4 darab dummy byte, majd a chip kiadja a 8 byte UID-ot
    //24 bit-et a dummy mezo ad, a maradekot az option mezo 8 bitje.
    MyQSPI_cmd_t cmd=
    {
        .instFrame.bits.width    = MYQSPI_INST1_ADDR1_DATA1,
        .instFrame.bits.inst_en  = 1,
        .instFrame.bits.data_en  = 1,
        .instFrame.bits.addr_en  = 0,
        .instFrame.bits.addr_len = 0,
        .instFrame.bits.opt_en   = 1,
        .instFrame.bits.opt_len  = 3,
        .instFrame.bits.tfr_type = MYQSPI_READ_ACCESS,
        .instFrame.bits.dummy_cycles= 24,
        .instruction             = 0x4B,
        .bufLen                  = length,
        .rxBuf                   = buff,
        .address                 = 0,
        .option                  = 0,
    };

    MyQSPI_transfer(dev->qspi, &cmd);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
