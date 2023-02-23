//------------------------------------------------------------------------------
//  1 vezeteskes homerseklet mero driver
//
//    File: DS18B20.c
//------------------------------------------------------------------------------
#include "DS18B20.h"
#include <string.h>

//------------------------------------------------------------------------------
//Scratch pad kiolvasasa.
status_t DS18B201_readScratchPad(MyOWI_Wire_t* wire,
                                 DS18B201_scratchPad_t* buff)
{
    //Mivel egyetlen eszkoz van a buszon, a ROM alapu azonositast kihagyjuk.
    uint8_t romCmd=DS18B201_SKIP_ROM;
    uint8_t functionCmd=DS18B201_READ_SCRATCHPAD;
    MyOWI_transaction_t trans;

    trans.tx1Buff=&romCmd;
    trans.tx1Length=1;
    trans.tx2Buff=&functionCmd;
    trans.tx2Length=1;
    trans.rxBuff=(uint8_t*) buff;
    trans.rxLength=sizeof(DS18B201_scratchPad_t);
    return MyOWI_transaction(wire, &trans);
}
//------------------------------------------------------------------------------
//Homerseklet meres/konverzio inditasa
status_t DS18B201_startMeasure(MyOWI_Wire_t* wire)
{
    //Mivel egyetlen eszkoz van a buszon, a ROM alapu azonositast kihagyjuk.
    uint8_t romCmd=DS18B201_SKIP_ROM;
    uint8_t functionCmd[2];
    MyOWI_transaction_t trans;
    memset(&trans, 0, sizeof(trans));

    //Homerseklet meres (konverzio) inditasa parancs...
    functionCmd[0]=DS18B201_CONVERT_T;
    //Az eszkoz tapot kap. Nem parazita power mode-ban mukodik.
    functionCmd[1]=0;

    trans.tx1Buff=&romCmd;
    trans.tx1Length=1;
    trans.tx2Buff=functionCmd;
    trans.tx2Length=1;
    trans.rxBuff=NULL;
    trans.rxLength=0;    
    return MyOWI_transaction(wire, &trans);
}
//------------------------------------------------------------------------------
//Homerseklet meres/konverzio eredmenyenek kiolvasasa
status_t DS18B201_readResult(MyOWI_Wire_t* wire, uint16_t* temperatureValue)
{
    status_t status;
    DS18B201_scratchPad_t sp;
    //Scratchpad kiolvasasa
    status=DS18B201_readScratchPad(wire, &sp);
    if (status) goto error;

    //CRC ellenorzes. (A vegen levo crc nem szamolodik bele)
    uint8_t calcCRC=DS18B201_crc8((uint8_t*) &sp, 8);
    if (calcCRC !=sp.crc) return kMyOWI_Status_crcError;

    switch (sp.config & 0x60)
    { // lower resolution means shorter conversion time, low bits need masking
      case 0x00: sp.tempLsb &= ~0b111; break;           //  9 bit  93.75 ms
      case 0x20: sp.tempLsb &= ~0b011; break;           // 10 bit 187.50 ms
      case 0x40: sp.tempLsb &= ~0b001; break;           // 11 bit 375.00 ms
      default: break;                                   // 12 bit 750.00 ms
    }

    uint16_t tempValue=(uint16_t) ((uint16_t)sp.tempMsb << 8) | (sp.tempLsb);
    *temperatureValue=tempValue;

error:
    return status;
}
//------------------------------------------------------------------------------
//Homerseklet kiolvasasa (RAW adat)
status_t DS18B201_readTemperatute(MyOWI_Wire_t* wire,
                                  uint16_t* temperatureValue)
{
    status_t status;

    //konverzio inditasa
    status=DS18B201_startMeasure(wire);
    if (status) goto error;

    //Varakozas, amig a konverzio befejezodik...
    vTaskDelay(DS18B201_CONV_TIME_12_BIT);

    //eredmeny olvasasa
    status=DS18B201_readResult(wire, temperatureValue);
error:
    return status;
}
//------------------------------------------------------------------------------
//12 bites meresi adat celsius fojra konvertalasa
double DS18B201_value2Celsius(uint16_t temperatureValue)
{
    int16_t temp=(int16_t)temperatureValue;
    if (temperatureValue & 0x8000)
    {   //negativ. 2-es komplemens
        temp = ((temp ^ 0xffff) + 1) * -1;
    }

    return (double) temp / 16.0;
}
//------------------------------------------------------------------------------
static const uint8_t DS18B201_crcTable[] =
{
    0,   94,  188, 226, 97,  63,  221, 131, 194, 156, 126, 32,  163, 253, 31,
    65,  157, 195, 33,  127, 252, 162, 64,  30,  95,  1,   227, 189, 62,  96,
    130, 220, 35,  125, 159, 193, 66,  28,  254, 160, 225, 191, 93,  3,   128,
    222, 60,  98,  190, 224, 2,   92,  223, 129, 99,  61,  124, 34,  192, 158,
    29,  67,  161, 255, 70,  24,  250, 164, 39,  121, 155, 197, 132, 218, 56,
    102, 229, 187, 89,  7,   219, 133, 103, 57,  186, 228, 6,   88,  25,  71,
    165, 251, 120, 38,  196, 154, 101, 59,  217, 135, 4,   90,  184, 230, 167,
    249, 27,  69,  198, 152, 122, 36,  248, 166, 68,  26,  153, 199, 37,  123,
    58,  100, 134, 216, 91,  5,   231, 185, 140, 210, 48,  110, 237, 179, 81,
    15,  78,  16,  242, 172, 47,  113, 147, 205, 17,  79,  173, 243, 112, 46,
    204, 146, 211, 141, 111, 49,  178, 236, 14,  80,  175, 241, 19,  77,  206,
    144, 114, 44,  109, 51,  209, 143, 12,  82,  176, 238, 50,  108, 142, 208,
    83,  13,  239, 177, 240, 174, 76,  18,  145, 207, 45,  115, 202, 148, 118,
    40,  171, 245, 23,  73,  8,   86,  180, 234, 105, 55,  213, 139, 87,  9,
    235, 181, 54,  104, 138, 212, 149, 203, 41,  119, 244, 170, 72,  22,  233,
    183, 85,  11,  136, 214, 52,  106, 43,  117, 151, 201, 74,  20,  246, 168,
    116, 42,  200, 150, 21,  75,  169, 247, 182, 232, 10,  84,  215, 137, 107,
    53
};
//------------------------------------------------------------------------------
uint8_t DS18B201_crc8(const uint8_t *addr, uint8_t len)
{   /*
    uint8_t crc = 0;

    while (len--)
    {
       crc = DS18B201_crcTable[ (crc ^ *addr++) ];
    }
    return crc;
    */
    uint8_t crc = 0;
      while (len--)
      {
        uint8_t inbyte = *addr++;
        for (uint8_t i = 8; i; i--)
        {
          uint8_t mix = (crc ^ inbyte) & 0x01;
          crc >>= 1;
          if (mix) crc ^= 0x8C;
          inbyte >>= 1;
        }
      }
      return crc;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
