//------------------------------------------------------------------------------
//  CRC szamitast tamogato rutinok
//
//    File: MyCRC.c
//------------------------------------------------------------------------------
#include "MyCRC.h"
#include <string.h>



//------------------------------------------------------------------------------
//TODO: tablazatosra es hardveresre cserelni!!!
static uint16_t crc_ccitt(uint16_t crc, uint8_t byte)
{
    crc = (uint8_t)(crc >> 8) | (crc << 8);
    crc ^= byte;
    crc ^= (uint8_t)(crc & 0xff) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0xff) << 5;
    return(crc);
}

static uint16_t crc_ccitt_of_data(uint8_t *data, uint32_t length)
{
    uint8_t i;
    uint16_t crc = 0xffff;
    for (i = 0; i < length; ++i) {
        crc = crc_ccitt(crc, data[i]);
    }
    return crc;
}

uint16_t MyCRC_calcCCIT(uint8_t* data, uint32_t length)
{
    return crc_ccitt_of_data(data, length);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
