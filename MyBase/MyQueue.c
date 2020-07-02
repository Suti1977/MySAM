//------------------------------------------------------------------------------
//  Sajat queue kezelo modul
//
//    File: MyQueue.c
//------------------------------------------------------------------------------
#include "MyQueue.h"
#include <string.h>

//------------------------------------------------------------------------------
//Queue inicializalasa es konfiguralasa
//Az inicializacio folyaman kiszamitasra kerul, hogy az atadtt memoriateruleten
//tenylegesen mennyi elem tarolhato.
void MyQueue_init(MyQueue_t* queue, const MyQueue_Config_t* cfg)
{
    //Modul valtozoinak kezdeti torlese.
    memset(queue, 0, sizeof(MyQueue_t));

    //Konfiguracio atvetele
    ASSERT(cfg->buffer);
    ASSERT(cfg->bufferSize);
    ASSERT(cfg->itemSize);
    memcpy(&queue->cfg, cfg, sizeof(MyQueue_Config_t));

    //Bueffer kezdeti nullazasa
    memset(queue->cfg.buffer, 0, queue->cfg.bufferSize);

    //A queue-ban tarolhato maximalis elemszam meghatarzasa
    queue->itemSize = cfg->itemSize +
                       cfg->metaSize +
                       MY_QUEUE_ITEM_HEADER_SIZE;
    queue->maxItemCount = cfg->bufferSize / queue->itemSize;

    //A buffer vegere mutato pointer kiszamitasa. Kesobb evvel tudja osszevetni
    //az iro/olvaso pointereket.
    queue->bufferEnd = queue->cfg.buffer +
                       (queue->itemSize * queue->maxItemCount);

    //A torlest meghivva inicializalja a mutatokat
    MyQueue_clear(queue);
}
//------------------------------------------------------------------------------
//tarolo korabbi tartalmanak torlese
void MyQueue_clear(MyQueue_t* queue)
{
    queue->readPtr = queue->writePtr = queue->cfg.buffer;
    queue->itemCount = 0;
}
//------------------------------------------------------------------------------
//Uj tartalom elhelyezese a sorba
status_t MyQueue_push(MyQueue_t* queue,
                      const uint8_t* data, uint16_t dataLength,
                      const uint8_t* metaData, uint16_t metaLength)
{
    return MyQueue_push2(queue,
                         data, dataLength,
                         NULL, 0,
                         metaData, metaLength);
}
//------------------------------------------------------------------------------
//Uj tartalom elhelyezese a sorba. Ennel ket adattartalom is megadhato, melye-
//ket egymas utan masol be rutin.
status_t MyQueue_push2(MyQueue_t* queue,
                      const uint8_t* data1, uint16_t dataLength1,
                      const uint8_t* data2, uint16_t dataLength2,
                      const uint8_t* metaData, uint16_t metaDataLength)
{
    if (queue->itemCount >= queue->maxItemCount)
    {   //A queue tele van.
        //TODO: hibakodot adni!
        return kStatus_Fail;
    }
    if ((dataLength1+dataLength2) >queue->cfg.itemSize)
    {   //Tul nagy adatblokkot akar tarolni.
        //TODO: hibakodot adni!
        return kStatus_Fail;
    }
    if (metaDataLength >queue->cfg.metaSize)
    {   //Tul nagy meta blokkot akar tarolni.
        //TODO: hibakodot adni!
        return kStatus_Fail;
    }

    //Adat masolasa a bufferbe, az iro poziciora
    MyQueue_ItemHeaderAndData_t* ptr =
                        (MyQueue_ItemHeaderAndData_t*)queue->writePtr;
    uint8_t* dest=ptr->data;
    memcpy(dest, data1,    dataLength1);
    dest+=dataLength1;
    memcpy(dest, data2,    dataLength2);
    dest+=dataLength2;
    memcpy(dest, metaData, metaDataLength);

    //Fejlec kitoltese
    ptr->header.dataLength=dataLength1 + dataLength2;
    ptr->header.metaLength=metaDataLength;

    //iro pointer mozgatasa
    queue->writePtr += queue->itemSize;
    if (queue->writePtr >= queue->bufferEnd)
    {   //A buffer vegere ert az iro pointer. Ugras vissza az elejere.
        //Cirkularis mukodes.
        queue->writePtr = queue->cfg.buffer;
    }

    //a bufferben tarolt elemszam novelese
    queue->itemCount++;

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//Tarolo leptetese. Mindaddig, mig ez nincs meghivva, addig a legregebbi
//elemet lehet olvasni. A Pop meghivasaval a regi elem torlodik.
void MyQueue_pop(MyQueue_t* queue)
{
    if (queue->itemCount == 0)
    {   //A fifo ures. Nincs mit tenni.
        return;
    }

    //Olvaso pointer leptetese
    queue->readPtr += queue->itemSize;
    if (queue->readPtr >= queue->bufferEnd)
    {   //A buffer vegere ert az olvaso pointer. Ugras vissza az elejere.
        //Cirkularis mukodes.
        queue->readPtr = queue->cfg.buffer;
    }

    //Elemszam is csokken.
    queue->itemCount--;
}
//------------------------------------------------------------------------------
//Visszaadja a sor elejen levo (legregebbi) bejegyzes adattartalmat.
//NULL-ha nincs adat a  queue-ban.
uint8_t *MyQueue_top(MyQueue_t* queue,
                     uint16_t *length,
                     uint8_t** Meta, uint16_t* metaLength)
{
    //Ha ures a queue, akkor NULL-t ad vissza
    if (queue->itemCount == 0)
    {
        *length=0;
        *metaLength=0;
        *Meta=NULL;
        return  NULL;
    }

    MyQueue_ItemHeaderAndData_t* ptr =
                    (MyQueue_ItemHeaderAndData_t*)queue->readPtr;
    *length= ptr->header.dataLength;
    *metaLength=ptr->header.metaLength;
    if (ptr->header.metaLength)
    {   //
        *Meta=ptr->data + ptr->header.dataLength;
    } else
    {
        *Meta=NULL;
    }
    return ptr->data;
}
//------------------------------------------------------------------------------
//true-val ter vissza, ha a queue ures
bool MyQueue_empty(MyQueue_t* queue)
{
    return (queue->itemCount == 0);
}
//------------------------------------------------------------------------------
//true-val ter vissza, ha a queue teljesen tele van
bool MyQueue_full(MyQueue_t* queue)
{
    return (queue->itemCount == queue->maxItemCount);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

