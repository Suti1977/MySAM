//------------------------------------------------------------------------------
//  FIFO kezelo modul. A FIFO ringbufferes megoldason alapul.
//
//    File: MyFIFO.h
//------------------------------------------------------------------------------
#ifndef MY_FIFO_H_
#define MY_FIFO_H_

#include "MyCommon.h"
#include "MyAtomic.h"
//------------------------------------------------------------------------------
//Fifot inicailizalo konfiguracios parameterek
typedef struct
{
    //A FIFO szamara foglalt buffer, es annak hossza
    uint8_t* buffer;
    uint32_t bufferSize;

} MyFIFO_Config_t;
//------------------------------------------------------------------------------
//MyFIFO valtozoi
typedef struct
{
    //A korabban foglalt buffer kezdocime
    uint8_t*    buffer;
    //A buffer vegere mutato pointer
    uint8_t*    bufferEnd;
    //A buffer merete
    uint32_t    bufferSize;

    //A buffert iro pointer
    uint8_t*    writePtr;
    //buffert olvaso pointer
    uint8_t*    readPtr;

    //A bufferben talalhato byteok szama
    uint32_t    bytesInBuffer;
    //A bufferben talalhato szabad byteok szama
    uint32_t    freeBytes;
} MyFIFO_t;
//------------------------------------------------------------------------------
//fifo kezdeti inicializalasa es konfiguralasa
void MyFIFO_init(MyFIFO_t* fifo, const MyFIFO_Config_t* cfg);

//Fifo uritese.
void MyFIFO_reset(MyFIFO_t* fifo);

//Uj elem helyezese a FIFO-ba megszakitasbol.
//Ha a fifo tele lenne, akkor kStatus_Fail hibaval ter vissza.
status_t MyFIFO_putBytesFromIsr(MyFIFO_t* fifo, uint8_t data);

//Uj elem helyezese a FIFO-ba normal futasbol
//Ha a fifo tele lenne, akkor kStatus_Fail hibaval ter vissza.
status_t MyFIFO_putByte(MyFIFO_t* fifo, uint8_t data);

//Uj elem olvasasa a FIFO-bol megszakitasban
//Ha a fifo ures, akkor kStatus_Fail hibaval ter vissza.
status_t MyFIFO_getByteFromIsr(MyFIFO_t* This, uint8_t* data);

//Uj elem olvasasa a FIFO-bol normal futasban
//Ha a fifo ures, akkor kStatus_Fail hibaval ter vissza.
status_t MyFIFO_getByte(MyFIFO_t* fifo, uint8_t* data);


//Szabad helyek szamanak lekerdezese megszakitasi rutinban
static inline uint32_t MyFIFO_getFreeFromIsr(MyFIFO_t* fifo)
{
    return fifo->freeBytes;
}

//A FIFO-ban levo byteok szamanak lekerdezese megszakitasi rutinban
static inline uint32_t MyFIFO_getAvailableFromIsr(MyFIFO_t* fifo)
{
    return fifo->bytesInBuffer;
}


//Szabad helyek szamanak lekerdezese
static inline uint32_t MyFIFO_getFree(MyFIFO_t* fifo)
{
    uint32_t Ret;
    MY_ENTER_CRITICAL();
    Ret=MyFIFO_getFreeFromIsr(fifo);
    MY_LEAVE_CRITICAL();
    return Ret;
}

//A FIFO-ban levo byteok szamanak lekerdezese
static inline uint32_t MyFIFO_getAvailable(MyFIFO_t* fifo)
{
    uint32_t Ret;
    MY_ENTER_CRITICAL();
    Ret=MyFIFO_getAvailableFromIsr(fifo);
    MY_LEAVE_CRITICAL();
    return Ret;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_FIFO_H_
