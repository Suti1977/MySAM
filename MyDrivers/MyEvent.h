//------------------------------------------------------------------------------
//	Sajat esemeny alrendszer driver rutinok
//------------------------------------------------------------------------------
#ifndef MY_EVENT_H_
#define MY_EVENT_H_

#include "MyCommon.h"
#include "MyGCLK.h"
//Szoftevres esemeny kivaltasa a megadott csatornan
#define MyEvent_swEvent(ch)  EVSYS->SWEVT.reg = (1 << ch)

//Userhez event csatorna beallitasa.
//(A megadasnal 1-el nagyobbat kell megadni a procinak, mint a csatorna index)
#define MyEvent_setUser(user_id, channelNo) \
                    EVSYS->USER[user_id].reg=channelNo+1

//Event csatornat meghajto generator (forras) beallitasa
#define MyEvent_setGenerator(channelNo, generator_id) \
                    EVSYS->Channel[channelNo].CHANNEL.bit.EVGEN=generator_id

//Event csatornahoz GCLK beallitasa. Ez sync/resync modokban lenyeges
#define MyEvent_setChannelGCLK(channelNo, gclk_generator_id) \
    MyGclk_enablePeripherialClock(EVSYS_GCLK_ID_0+channelNo, gclk_generator_id)

//Event path beallitasa
//  EVSYS_CHANNEL_PATH_SYNCHRONOUS_Val _U_(0x0)     Synchronous path
//  EVSYS_CHANNEL_PATH_RESYNCHRONIZED_Val _U_(0x1)  Resynchronized path
//  EVSYS_CHANNEL_PATH_ASYNCHRONOUS_Val _U_(0x2)    Asynchronous path
#define MyEvent_setChannelPath(channelNo, path) \
                            EVSYS->Channel[channelNo].CHANNEL.bit.PATH=path

//Esemenykezeles kezdeti initje
void MyEvent_init(void);


//Esemeny alrendszerrol debug informaciok kiirasa a konzolra
void MyEvent_printDebugInfo(void);


#endif	//MY_EVENT_H_
