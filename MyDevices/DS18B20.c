//------------------------------------------------------------------------------
//  1 vezeteskes homerseklet mero driver
//
//    File: DS18B20.c
//------------------------------------------------------------------------------
#include "DS18B20.h"
#include <string.h>
#include "MyCRC.h"

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
    uint8_t calcCRC=MyCRC_crc8_X8X5X41((uint8_t*) &sp, 8);
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
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
