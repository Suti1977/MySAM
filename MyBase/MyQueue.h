//------------------------------------------------------------------------------
//  Sajat queue kezelo modul
//
//    File: MyQueue.h
//------------------------------------------------------------------------------
#ifndef MY_QUEUE_H_
#define MY_QUEUE_H_

#include "MyCommon.h"

//Queue konfiguracios struktura
typedef struct
{
    //Az adatok tarolasara foglalt (allokalt) buffer
    uint8_t* buffer;
    //A buffer merete
    uint32_t bufferSize;
    //A tarolt elemek maximalis merete.
    //(Jelenleg a queue megoldas fix meretu bejegyzesekkel operal)
    uint32_t itemSize;

    //Az adatok melle rendelt Meta adatblokk merete.
    uint32_t metaSize;

    //A buffer merete, es az elemek merete meghatarozza, hogy mennyi elem fer el
    //a taroloban.
} MyQueue_Config_t;
//------------------------------------------------------------------------------
//A queue-ban tarolt elemek elotti fejlec
#pragma pack(1)
typedef struct
{
    uint16_t metaLength;
    uint16_t dataLength;
} MyQueue_ItemHeader_t;
#pragma pack()

#pragma pack(1)
typedef struct
{
    MyQueue_ItemHeader_t  header;
    uint8_t               data[1];
} MyQueue_ItemHeaderAndData_t;
#pragma pack()
//------------------------------------------------------------------------------
#define MY_QUEUE_ITEM_HEADER_SIZE    sizeof(MyQueue_ItemHeader_t)

//Buffer meretenek kiszamitasat segito makro.
//ItemSize: A legnagyobb tarolando elem merete
//MetaSize: Az uzenetek melle tarolando legnagyobb kisero adatblokk merete
//ItemCount: Maximalis elemszam a queue-ban
#define MYQUEUE_CALC_BUFFER(ItemSize, MetaSize, ItemCount) \
    ((ItemSize + MetaSize + MY_QUEUE_ITEM_HEADER_SIZE) * ItemCount)
//------------------------------------------------------------------------------
//Queue valtozoi
typedef struct
{
    //Inicializalaskor atvett konfiguracio
    MyQueue_Config_t cfg;
    //A buffer veget mutato pinter.
    uint8_t* bufferEnd;
    //A queue-ban tarolhato maximalis elemszam. Ez a buffer merete es az elemek
    //merete alapjan kerul kiszamitasra initkor.
    uint32_t maxItemCount;
    //Az egyes bejegyzesek merete, fejleccel egyutt
    uint32_t itemSize;

    uint8_t* writePtr;
    uint8_t* readPtr;

    //A bufferben talalhato elemek szama
    uint32_t itemCount;
} MyQueue_t;
//------------------------------------------------------------------------------
//Queue inicializalasa es konfiguralasa
//Az inicializacio folyaman kiszamitasra kerul, hogy az atadtt memoriateruleten
//tenylegesen mennyi elem tarolhato.
//A pontos memoriameret meghatarozasahoz hasznalhato a MYQUEUE_CALC_BUFFER()
//makro.
void MyQueue_init(MyQueue_t* queue, const MyQueue_Config_t* cfg);

//tarolo korabbi tartalmanak torlese
void MyQueue_clear(MyQueue_t* queue);

//Uj tartalom elhelyezese a sorba
status_t MyQueue_push(MyQueue_t* queue,
                      const uint8_t* data, uint16_t dataLength,
                      const uint8_t* metaData, uint16_t metaDataLength);

//Uj tartalom elhelyezese a sorba. Ennel ket adattartalom is megadhato, melye-
//ket egymas utan masol be rutin.
status_t MyQueue_push2(MyQueue_t* queue,
                      const uint8_t* data1, uint16_t dataLength1,
                      const uint8_t* data2, uint16_t dataLength2,
                      const uint8_t* metaData, uint16_t metaDataLength);

//Tarolo leptetese. Mindaddig, mig ez nincs meghivva, addig a legregebbi
//elemet lehet olvasni. A Pop meghivasaval a regi elem torlodik.
void MyQueue_pop(MyQueue_t* queue);

//Visszaadja a sor elejen levo (legregebbi) bejegyzes adattartalmat.
//NULL-ha nincs adat a  queue-ban.
//Meta / MetaLength -ben visszaadja a jarulekos adatok kezdocimet, es meretet
uint8_t* MyQueue_top(MyQueue_t* queue,
                     uint16_t* length,
                     uint8_t** meta, uint16_t* meta_length);

//true-val ter vissza, ha a queue ures
bool MyQueue_empty(MyQueue_t* queue);

//true-val ter vissza, ha a queue teljesen tele van
bool MyQueue_full(MyQueue_t* queue);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MY_QUEUE_H_
