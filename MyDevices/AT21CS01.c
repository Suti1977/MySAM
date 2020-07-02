//------------------------------------------------------------------------------
//  AT21CS01, Single-Wire (SWI) EEPROM es serial number IC drver
//
//    File: AT21CS01.c
//------------------------------------------------------------------------------
// http://ww1.microchip.com/downloads/en/DeviceDoc/20005857C.pdf
//AT21CS01 Memory organisation
//Interneal memory array splitted to two regions:
// main: 1 kbit EEPROM
//      16 pages,  8 byte/page
// security registers: 256 bit
//      4 pages, 8 bytes/page
//          Lower 2 pages are factory programmed.
//          - 64 bit UID
//          Upper 2 pages
//          - User-programmable and can be subsequently locked
// main region acces opcode: 0xA
// security region acces opcode: 0xB
//------------------------------------------------------------------------------
#include "AT21CS01.h"
#include <string.h>

#define AT21CS01_OPCODE_EEPROM_ACCESS               0xA0
#define AT21CS01_OPCODE_SECURITY_REGISTER_ACCESS    0xB0
#define AT21CS01_OPCODE_LOCK_SECURITY_REGISTER      0x20
#define AT21CS01_OPCODE_ROM_ZONE_REGISTER_ACCESS    0x70
#define AT21CS01_OPCODE_FREEZE_ROM_ZONE_STATE       0x10
#define AT21CS01_OPCODE_MANUFACTURER_ID_READ        0xC0
#define AT21CS01_OPCODE_STANDARD_SPEED_MODE         0xD0
#define AT21CS01_OPCODE_HIGH_SPEED_MODE             0xE0

//------------------------------------------------------------------------------
//Chip gyari azonosito olvasasa.
status_t AT21CS01_readManufacturerID(MySWI_Wire_t* wire,
                                     uint8_t slaveAddress,
                                     uint32_t* id)
{
    uint8_t opCode=AT21CS01_OPCODE_MANUFACTURER_ID_READ | slaveAddress | 1;

    ASSERT(id);

    *id=0;

    MySWI_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.tx1Buff=&opCode;
    trans.tx1Length=1;
    trans.rxBuff=(uint8_t*) id;
    trans.rxLength=3;

    return MySWI_transaction(wire, &trans);
}
//------------------------------------------------------------------------------
//64 bites egyedi sorozatszam olvasasa
//A visszaadott sorozatszam 7. byteja egy 8 bites crc is egyben. (X^8+X^5+X^4+1)
status_t AT21CS01_readSerialNumber(MySWI_Wire_t* wire,
                                   uint8_t slaveAddress,
                                   uint8_t* buffer,
                                   uint8_t bufferLength)
{
    if (bufferLength>8) bufferLength=8;

    ASSERT(buffer);

    uint8_t wbuff1[2]=
    {
        //Opcode: Security region access, slave address, dumy write
        AT21CS01_OPCODE_SECURITY_REGISTER_ACCESS | slaveAddress,
        //serial numbert start address
        0
    };

    uint8_t wbuff2[1]=
    {
        //Opcode: Security region access, slave address, read
        AT21CS01_OPCODE_SECURITY_REGISTER_ACCESS | slaveAddress | 1
    };

    MySWI_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.tx1Buff=wbuff1;
    trans.tx1Length=2;
    trans.tx2Buff=wbuff2;
    trans.tx2Length=1;
    trans.insertRestart=1;
    trans.rxBuff=buffer;
    trans.rxLength=bufferLength;

    return MySWI_transaction(wire, &trans);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
