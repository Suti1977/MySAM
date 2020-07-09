//------------------------------------------------------------------------------
//	Sajat framework kozos definicioi
//------------------------------------------------------------------------------
#ifndef MY_COMMON
#define MY_COMMON

#include <stdio.h>

#include <string.h>
#include <stdint.h>
#include <sam.h>

#ifdef USE_FREERTOS
#include "MyRTOS.h"
#endif

#include "MyAtomic.h"
#include "MyHelpers.h"

//ASF behuzasa...
#include <err_codes.h>
#include <atmel_start.h>


//verzioszam osszeallitasat segito makro
#define MAKE_VERSION(major, minor, bugfix) (((major) << 16) | ((minor) << 8) | (bugfix))

//BIT-re maszkot eloallito seged makro
#ifndef BIT
#define BIT(a)  (1U << a)
#endif

//Deffiniciok osszefuzesehez haszanlhato makrok
#define MY_PASTER( a, b )        a ## b
#define MY_EVALUATOR( a, b )     MY_PASTER(a, b)

//Fuggvenek visszateresi ertekenek tipusdefinicioja
typedef int mystatus_t;

#ifndef status_t
typedef mystatus_t status_t;
#endif

//statuszok eloallitasara szolgalo makro
#define MAKE_STATUS(group, code) ((((group)*100) + (code)))


//Statusz csoportok definialasa.
enum _status_groups
{
    kStatusGroup_ASF=0,				//ASF hibakodok csoportja. Azok viszont
                                    //negativ szamokat hasznalnak

    kStatusGroup_Generic=0,			//Altalanos csoport a frameworkre, es a
                                    //sajat applikaciora is.
	
	kStatusGroup_MyGPIO=10,			//GPIO driver
	kStatusGroup_MyGCLK=11,			//GCLK kezeles drivere
	kStatusGroup_MySercom=12,		//Sercom alap driver    
	kStatusGroup_MyI2CM=13,			//I2C master driver
	kStatusGroup_MyI2CS=14,			//I2C slave driver
    kStatusGroup_MyDMA=15,			//DMA kezelo driver
    kStatusGroup_MyUart=16,         //Sajat uart driver
    kStatusGroup_XComm=17,          //XComm kommunikacios modul hibakodok
    kStatusGroup_MySWI=18,          //Single-Wire driver hibakodok
    kStatusGroup_MyOWI=19,          //One-Wire driver hibakodok
    kStatusGroup_MySPIM=20,         //SPI master hibakodok

	//Applikacio szamara itt kezdodhetnek a hibakodok
    kStatusGroup_Application=100,
};


//Altalanos status visszateresi kodok
//Ezek reszben az ASF-hez igazodnak, igy az ASF-ben kapott fuggvenyekkel
//kompatibilis visszateresi erteket kapunk
enum _generic_status
{
    kStatus_Success 			= MAKE_STATUS(kStatusGroup_Generic, 0),
    kStatus_Fail 				= MAKE_STATUS(kStatusGroup_Generic, 1),
    kStatus_ReadOnly 			= MAKE_STATUS(kStatusGroup_Generic, 2),
    kStatus_OutOfRange 			= MAKE_STATUS(kStatusGroup_Generic, 3),
    kStatus_InvalidArgument 	= MAKE_STATUS(kStatusGroup_Generic, 4),
    kStatus_Timeout 			= MAKE_STATUS(kStatusGroup_Generic, 5),
    kStatus_NoTransferInProgress= MAKE_STATUS(kStatusGroup_Generic, 6),    
};


//------------------------------------------------------------------------------
//Specialis pointerek,melyek segitenek feloldani a "const" pointerek kasztolasat
union my_ptr8_t
{
    uint8_t*        v;
    const uint8_t*  c;
};

union my_ptr16_t
{
    uint16_t*       v;
    const uint16_t* c;
};

union my_ptr32_t
{
    uint32_t*       v;
    const uint32_t* c;
};
//------------------------------------------------------------------------------

#endif // MY_COMMON
