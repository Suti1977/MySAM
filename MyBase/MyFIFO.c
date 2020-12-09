//------------------------------------------------------------------------------
//  FIFO kezelo modul. A FIFO ringbufferes megoldason alapul.
//
//    File: MyFIFO.c
//------------------------------------------------------------------------------
#include "MyFIFO.h"
#include <string.h>


//------------------------------------------------------------------------------
//fifo kezdeti inicializalasa es konfiguralasa
void MyFIFO_init(MyFIFO_t* fifo, const MyFIFO_Config_t* cfg)
{
    ASSERT(cfg);
    ASSERT(cfg->buffer);
    ASSERT(cfg->bufferSize);

    //Modul valtozoinak kezdeti torlese.
    memset(fifo, 0, sizeof(MyFIFO_t));

    fifo->buffer=cfg->buffer;
    fifo->bufferSize=cfg->bufferSize;
    //Buffer vegere mutato pointer beallitasa
    fifo->bufferEnd=fifo->buffer + cfg->bufferSize;

    //A pointerek a Reset() fuggvenyben be lesznek allitva
    MyFIFO_reset(fifo);
}
//------------------------------------------------------------------------------
//Fifo uritese.
void MyFIFO_reset(MyFIFO_t* fifo)
{
    MY_ENTER_CRITICAL();

    //Iro, olvaso pointerek a buffer elejen
    fifo->readPtr=fifo->writePtr=fifo->buffer;

    //Byteok szama 0 a bufferben. A teljes szabad...
    fifo->bytesInBuffer=0;
    fifo->freeBytes=fifo->bufferSize;

    MY_LEAVE_CRITICAL();
}
//------------------------------------------------------------------------------
//Uj elem helyezese a FIFO-ba megszakitasbol.
//Ha a fifo tele lenne, akkor kStatus_Fail hibaval ter vissza.
status_t MyFIFO_putBytesFromIsr(MyFIFO_t* fifo, uint8_t data)
{
    if (fifo->freeBytes==0) return kStatus_Fail;

    *fifo->writePtr++=data;

    if (fifo->writePtr>=fifo->bufferEnd)
    {   //A buffer vegere ert a pointer. Ugras az elejere (Circularis mukodes)
        fifo->writePtr=fifo->buffer;
    }

    //Szabad helyek szama csokken
    fifo->freeBytes--;
    //A bufferben levo byteok szama no
    fifo->bytesInBuffer++;

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Uj elem helyezese a FIFO-ba normal futasbol
//Ha a fifo tele lenne, akkor kStatus_Fail hibaval ter vissza.
status_t MyFIFO_putByte(MyFIFO_t* fifo, uint8_t data)
{
    status_t status;
    MY_ENTER_CRITICAL();
    status=MyFIFO_putBytesFromIsr(fifo, data);
    MY_LEAVE_CRITICAL();
    return status;
}
//------------------------------------------------------------------------------
//Uj elem olvasasa a FIFO-bol megszakitasban
//Ha a fifo ures, akkor kStatus_Fail hibaval ter vissza.
status_t MyFIFO_getByteFromIsr(MyFIFO_t* fifo, uint8_t* data)
{
    if (fifo->bytesInBuffer==0) return kStatus_Fail;

    *data=*fifo->readPtr++;

    if (fifo->readPtr>=fifo->bufferEnd)
    {   //A buffer vegere ert a pointer. Ugras az elejere (Circularis mukodes)
        fifo->readPtr=fifo->buffer;
    }

    //Szabad helyek szama no
    fifo->freeBytes++;
    //A bufferben levo byteok szama csokken
    fifo->bytesInBuffer--;

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Uj elem olvasasa a FIFO-bol normal futasban
//Ha a fifo ures, akkor kStatus_Fail hibaval ter vissza.
status_t MyFIFO_getByte(MyFIFO_t* fifo, uint8_t* data)
{
    status_t status;
    MY_ENTER_CRITICAL();
    status=MyFIFO_getByteFromIsr(fifo, data);
    MY_LEAVE_CRITICAL();
    return status;
}
//------------------------------------------------------------------------------
