//------------------------------------------------------------------------------
// GPIO kezelest segito rutinok.
// Gyorsabb, mint az ASF altal biztositott rutinokon keresztul
//------------------------------------------------------------------------------
#include "MyGPIO.h"

//------------------------------------------------------------------------------
//GPIO kezeles kezdeti initje
void MyGPIO_initPorts(void)
{
    //A SAM proci GPIO vonalainak bemeneteinek mintazasat kulon engedelyezni
    //kell. Ez a mintazas 2 orajelnyi kesleltetest okoz, mert szinkronizalnia
    //kell. Ezen ugy lehet gyorsitani,ha egy csoporton belul minden egyes pinhez
    //engedelyezzuk a bemenetek mintazasat.(Picit magasabb lesz az aramfelvetel,
    //de az szamunkra nem relevans.)
    //Ennek megfeleloen, itt most minden port (A, B, C) minden PIN-jehez enge-
    //delyezni fogom a bemenetek mintazasat.
    PORT->Group[PORT_A].CTRL.bit.SAMPLING = 0xffffffff;
    PORT->Group[PORT_B].CTRL.bit.SAMPLING = 0xffffffff;
    PORT->Group[PORT_C].CTRL.bit.SAMPLING = 0xffffffff;
#ifdef PORT_D
    PORT->Group[PORT_D].CTRL.bit.SAMPLING = 0xffffffff;
#endif

}
//------------------------------------------------------------------------------
//GPIO pin beallitasa, az Config-ban megadott modon.
//Config egyes bitmezoi mas es mast jelentenek. Meghatarozzak pl a portot,
// a porton beluli pint, a pinhez tartozo jellemzoket (pl felhuzas, lehuzas,
// meghajtas, stb...)
void MyGPIO_pinConfig(uint32_t config)
{
    //Port es a porton beluli PIN meghatarozasa...
    //A Port valtozo azonnal a porthoz tartozo leiro halmazra fog mutatni
    PortGroup*  portP=&PORT->Group[ config >> _PINOP_PORT_POS  ];
    uint32_t  	pin = (config >> _PINOP_PIN_POS) & 31;


    //Alapbol beallitjuk, hogy irva lesz a multiplexer, es a pin konfiguracio is
    //Alapbol engedelyzve lesz a bemenet is, akkor is ha nem hszanaljuk, mert
    //legalabb vissza lehet olvasni az allapotat.
    #define FIX_BITS  PORT_WRCONFIG_WRPINCFG |  \
                      PORT_WRCONFIG_WRPMUX   |  \
                      PORT_WRCONFIG_INEN

    //WRConfig regiszterbe irando bitmezoket ebben rakjuk ossze.
    //A pinre vonatkozo beallitasok atvetele.
    //A header fajlban le van irva, hogy ez miert ilyen egyszeruen tortenik
    uint32_t    wrConfig = FIX_BITS;


    //A megcelzott PIN kijelolese. A Wrconfig regiszter also 16 bitje jeloli ki
    //hogy mely PIN-ekre menjen a beallitas. A felso (31.) bit pedig megmondja,
    //hogy a 32 bites port also vagy felso 16 bitjere vonatkozzon a kijelolo
    //mezo.
    if (pin & 0x10)
    {
        wrConfig |=  PORT_WRCONFIG_HWSEL;
    }    
    //Bitmaszkal a beallitando PIN kijelolese
    wrConfig |= 1 << (pin & 0xf);

    //Strength kezelese...
    if (config & _PINOP_STRENGTH_MASK)
    {   //eros vonal meghajts eloirva.
        wrConfig |= PORT_WRCONFIG_DRVSTR;
    }

    //Fel/lehuzas ellenorzese...
    if (config & _PINOP_PULL_MASK)
    {
        wrConfig |= PORT_WRCONFIG_PULLEN;
    }

    //GPIO vagy periferia hasznalja a PIN-t?
    //Ha periferia, akkor a multiplexert engedelyezni kell
    if (!(config & _PINOP_GPIO_MASK))
    {   //ezt egy periferia hasznalja. Multiplexert hasznaljuk
        wrConfig |= PORT_WRCONFIG_PMUXEN;
    }
    //A multiplexert minedenkepen beallitjuk ha hasznaljuk, ha nem.
    wrConfig |= ((config & _PINOP_MUX_MASK) >> _PINOP_MUX_POS)<<PORT_WRCONFIG_PMUX_Pos;


    //Port beallitasa, egyetlen irassal a WRCONFIG regiszteren keresztul.
    portP->WRCONFIG.reg = wrConfig;
    //..........................................................................


    uint32_t    pinMask= 1 << pin;
    //Port kezdo allapot beallitasa.
    //A fel/le huzas evvel lesz teljes, mivel a kimeneti regiszter befolyasolja
    if (config & _PINIOPC_INITIAL_MASK)
    {   //1
        portP->OUTSET.reg=pinMask;
    } else
    {   //0
        portP->OUTCLR.reg=pinMask;
    }

    //Adatirany beallitasa...
    if ((config & _PINOP_DIR_MASK) == PINOP_INPUT)
    {   //A portot bemenetnek kell konfiguralni
        //Adatirany beallitasa bemenetnek.
        portP->DIRCLR.reg = pinMask;
    } else
    {   //A portot kimenetnek kell konfiguralni
        //Adatirany beallitasa kimenetnek.
        portP->DIRSET.reg = pinMask;
    }
}
//------------------------------------------------------------------------------
//IO-k csoportos beallitasa. Addig megy amig a PINCONFIG_END bejegyzesbe nem
//utkozik.
void MyGPIO_pinGroupConfig(const uint32_t* pinConfigs)
{
    while(*pinConfigs!=PINCONFIG_END)
    {
        MyGPIO_pinConfig(*pinConfigs++);
    }
}
//------------------------------------------------------------------------------

