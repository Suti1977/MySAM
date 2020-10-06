//------------------------------------------------------------------------------
// GPIO kezelest segito rutinok.
// Gyorsabb, mint az ASF altal biztositott rutinokon keresztul
//------------------------------------------------------------------------------
// A modul az ASF PORT es PIN definicioira alapoz. Igy a kompatibilitas
// megtarthato.
// FIGYELEM: Ha az ASF pin/port definicioi strukturalisan valtoznanak, akkor
// ezt a modult is utana kellene modositani.
//
//  A hasznalt ASF-es definiciok az eszkoz PIO alkonyvtaraban talalhato
//
//  ASF port/pin alapok:
//  Az ASF-ben a pineket 0-tol sorszamozzak, folytonosan a csoportokon keresztul
//  PA00= 0,  PA01= 1, ... PA31=31,
//  PB00=32,  PB33=33, ... PB31=63,
//  PC00=64,  PC01=65, ... PC31=95,
//  Mindebbol kovetkezik, ha a sorszamot elosztjuk 32-vel, akkor megkapjuk azt,
//  hogy meliyk 32 bites port csoportban van. Ha meg az also 5 bit meg megadja
//  a porton beluli bit poziciot.
//
//  Az ASF a PINMUX_... definicioban 32 biten definialj a multiplex funkciot,
//  es a pin sorszamot is.
//      Felso 16 biten: pin sorszama
//      Also 16 biten: pin multiplexer beallitas
//------------------------------------------------------------------------------
#ifndef MY_GPIO_H
#define MY_GPIO_H

#include "MyCommon.h"

//------------------------------------------------------------------------------
//Pin indexbol csoport sorszamanak kinyerese
#define PIN2BIT(pin)        (pin & 31)
//egy olyan maszkot ad vissza, ahol ott lesz 1, ahol a pin van a csoporton belul
#define PIN2BITMASK(pin)    (1ul << PIN2BIT(pin))
//csoport sorszamanak kinyerese a pin sorszamabol  (A, B, C, (D))
//TOODO: ha lenne tobb port, akkor a maszkolast ki kellene szelesiteni
#define PIN2GROUP(pin)      (((pin >> 5)) & 0x03)
//PIN neve alapjan a hozza tartozo port baziscimenek visszanyerese
#define PIN2IN_ADDR(pin)   ((uint32_t)&PORT->Group[ PIN2GROUP(pin) ].IN)
//Port es ID megadasval PIN azonosito letrehozasa
#define MyGPIO_ID(port, pin)  (((uint32_t)port << 5) | pin)
//------------------------------------------------------------------------------
//PORT csoportok definialasa (hatha jo lesz valahova)
#define PORT_A  0
#define PORT_B  1
#define PORT_C  2
#define PORT_D  3

//------------------------------------------------------------------------------
// PIN KONFIGURACIOS REKORD FELEPITESE (bitkiosztas)
//
//  31..16  PIN sorszama (16..20 porton beluli pozicio, 21..23 csoport)
//  15..12  reserved (tovabbi beallitasokhoz fenntartva)
//  11      Strength  (meghajtas erossege)
//  10      PullEn    (fel/lehuzas engedelyezese)
//  9       Direction (1 kimenet, 0 bemenet GPIO eseten)
//  8       Initial state (GPIO kimenet eseten a kezdo allapot)
//  7       GPIO/Periferia  (GPIO eseten ez 1)
//  6..0    Multiplexer
//------------------------------------------------------------------------------
//31..21
#define _PINOP_PORT_POS        21
#define _PINOP_PORT_MASK       (3<<_PINOP_PORT_POS)
//20..16
#define _PINOP_PIN_POS         16
#define _PINOP_PIN_MASK        (31<<_PINOP_PIN_POS)

//11
#define _PINOP_STRENGTH_POS    11
#define _PINOP_STRENGTH_MASK   (1<<_PINOP_STRENGTH_POS)

//10
#define _PINOP_PULL_POS        10
#define _PINOP_PULL_MASK       (1<<_PINOP_PULL_POS)

//9
#define _PINOP_DIR_POS         9
#define _PINOP_DIR_MASK        (1<<_PINOP_DIR_POS)

//8
#define _PINIOPC_INITIAL_POS    8
#define _PINIOPC_INITIAL_MASK   (1<<_PINIOPC_INITIAL_POS)

//7
#define _PINOP_GPIO_POS        7
#define _PINOP_GPIO_MASK       (1<<_PINOP_GPIO_POS)
//6..0
#define _PINOP_MUX_POS         0
#define _PINOP_MUX_MASK        (15<<_PINOP_MUX_POS)
//------------------------------------------------------------------------------
//Kezdeti jelszint kimenet eseten
#define PINOP_INIT0            0
#define PINOP_INIT1            _PINIOPC_INITIAL_MASK
//Fel/Lehuzo opciok
#define PINOP_PULLDOWN         (_PINOP_PULL_MASK | 0)
#define PINOP_PULLUP           (_PINOP_PULL_MASK | _PINIOPC_INITIAL_MASK)
//Adatirany beallitasok
#define PINOP_INPUT            0
#define PINOP_OUTPUT           _PINOP_DIR_MASK
//Kimeneti meghajtas erossge
#define PINOP_NORMAL           0
#define PINOP_STRONG           _PINOP_STRENGTH_MASK
//GPIO pin
#define PINOP_GPIO             _PINOP_GPIO_MASK
//multiplexelt pin eseten
#define PINOP_MUX_EN           0
//------------------------------------------------------------------------------
//GPIO pin multiplexer funkciok definialasara definiciok
#define PINMUX_NONE             (PINOP_GPIO)
#define PINMUX_A                (0 << _PINOP_MUX_POS)
#define PINMUX_B                (1 << _PINOP_MUX_POS)
#define PINMUX_C                (2 << _PINOP_MUX_POS)
#define PINMUX_D                (3 << _PINOP_MUX_POS)
#define PINMUX_E                (4 << _PINOP_MUX_POS)
#define PINMUX_F                (5 << _PINOP_MUX_POS)
#define PINMUX_G                (6 << _PINOP_MUX_POS)
#define PINMUX_H                (7 << _PINOP_MUX_POS)
#define PINMUX_I                (8 << _PINOP_MUX_POS)
#define PINMUX_J                (9 << _PINOP_MUX_POS)
#define PINMUX_K                (10<< _PINOP_MUX_POS)
#define PINMUX_L                (11<< _PINOP_MUX_POS)
#define PINMUX_M                (12<< _PINOP_MUX_POS)
#define PINMUX_N                (13<< _PINOP_MUX_POS)

//------------------------------------------------------------------------------
//Ezt a definiciot kell elhelyezni a PIN leirokat felsorolo tablazatok vegere.
//A MyGPIO_pinGroupConfig() rutin eddig a bejegyzesig dolgozza fel a tablazatot.
#define PINCONFIG_END           0xFFFFFFFF
//------------------------------------------------------------------------------
//PIN config bitmaszkbol port sorszam kinyerese
#define PINCFG2PORT(a)   ((a >> _PINOP_PORT_POS) & 3)
//PIN config bitmaszkbol porton beluli pin sorszamanak kinyerese
#define PINCFG2BIT(a)    ((a >> _PINOP_PIN_POS) & 31)
//PIN configuraciobol PIN azonosito konvertalasa
#define MyGPIO_CFG2ID(cfg)    MyGPIO_ID(PINCFG2PORT(cfg), PINCFG2BIT(cfg))
//------------------------------------------------------------------------------
//PINEK kezelese gyors makrokon keresztul

//GPIO PIN magasba allitasa. id-ben a PIN azonositot kell megadni
#define MyGPIO_set(id)\
    PORT->Group[PIN2GROUP(id)].OUTSET.reg = PIN2BITMASK(id)

//GPIO PIN alacsonyba allitasa. id-ben a PIN azonositot kell megadni
#define MyGPIO_clr(id)\
    PORT->Group[PIN2GROUP(id)].OUTCLR.reg = PIN2BITMASK(id)

//GPIO PIN invertalasa. Id-ben a PIN azonositot kell megadni
#define MyGPIO_toggle(id)\
    PORT->Group[PIN2GROUP(id)].OUTTGL.reg = PIN2BITMASK(id)

//GPIO PIN bementnek allitasa. id-ben a PIN azonositot kell megadni
#define MyGPIO_setDirInput(id)\
    PORT->Group[PIN2GROUP(id)].DIRCLR.reg = PIN2BITMASK(id)

//GPIO PIN kimentnek allitasa. id-ben a PIN azonositot kell megadni
#define MyGPIO_setDirOutput(id)\
    PORT->Group[PIN2GROUP(id)].DIRSET.reg = PIN2BITMASK(id)

//GPIO PIN bemeneti allapotanak lekerdezese. A makro TRUE-t ad vissza, ha a
//lekerdezett PIN magas szinten van. id-ben a PIN azonositot kell megadni
#define MyGPIO_inputState(id)\
    ((PORT->Group[PIN2GROUP(id)].IN.reg & PIN2BITMASK(id))!=0)

//GPIO PIN kimeneti allapotanak lekerdezese. A makro TRUE-t ad vissza, ha a
//lekerdezett PIN magas szinten van. id-ben a PIN azonositot kell megadni.
#define MyGPIO_outputState(id)\
    ((PORT->Group[PIN2GROUP(id)].OUT.reg & PIN2BITMASK(id))!=0)

//------------------------------------------------------------------------------
//Az ASF-el hasonlo PINMUX_ definiciok letrehozasa, mely a sima GPIO-k eseten
//lesz hasznalatos.
//Mivel a SAM procikban a GPIO es az egyebb funkciok kulon vannak kezelve.
//Sajnos csak a multiplexer alapjan nem lehet eldonteni, hogy az egy GPIO, vagy
//egy periferiahoz tartozo funkcios PIN.
//Az ASF-ben talalni egy olyant, hogy SYSTEM_PINMUX_GPIO (1<<7) definiciot.
//tehat a 7. biten jelezni lehetne, hogy az egy sima GPIO vonal.
//A multiplexet hasznalo PINMUX_P... definiciokhoz hasonloan itt lesz definialva
//a sima GPIO-knak is ertek, ahol a 7. bit 1 lesz.
//A beallito rutinom ez alapjan majd tudj, hogy kell hozza MUX vagy nem.
#define PINMUX_PA00_GPIO        ((PIN_PA00 << 16)  |   PINOP_GPIO)
#define PINMUX_PA01_GPIO        ((PIN_PA01 << 16)  |   PINOP_GPIO)
#define PINMUX_PA02_GPIO        ((PIN_PA02 << 16)  |   PINOP_GPIO)
#define PINMUX_PA03_GPIO        ((PIN_PA03 << 16)  |   PINOP_GPIO)
#define PINMUX_PA04_GPIO        ((PIN_PA04 << 16)  |   PINOP_GPIO)
#define PINMUX_PA05_GPIO        ((PIN_PA05 << 16)  |   PINOP_GPIO)
#define PINMUX_PA06_GPIO        ((PIN_PA06 << 16)  |   PINOP_GPIO)
#define PINMUX_PA07_GPIO        ((PIN_PA07 << 16)  |   PINOP_GPIO)
#define PINMUX_PA08_GPIO        ((PIN_PA08 << 16)  |   PINOP_GPIO)
#define PINMUX_PA09_GPIO        ((PIN_PA09 << 16)  |   PINOP_GPIO)
#define PINMUX_PA10_GPIO        ((PIN_PA10 << 16)  |   PINOP_GPIO)
#define PINMUX_PA11_GPIO        ((PIN_PA11 << 16)  |   PINOP_GPIO)
#define PINMUX_PA12_GPIO        ((PIN_PA12 << 16)  |   PINOP_GPIO)
#define PINMUX_PA13_GPIO        ((PIN_PA13 << 16)  |   PINOP_GPIO)
#define PINMUX_PA14_GPIO        ((PIN_PA14 << 16)  |   PINOP_GPIO)
#define PINMUX_PA15_GPIO        ((PIN_PA15 << 16)  |   PINOP_GPIO)
#define PINMUX_PA16_GPIO        ((PIN_PA16 << 16)  |   PINOP_GPIO)
#define PINMUX_PA17_GPIO        ((PIN_PA17 << 16)  |   PINOP_GPIO)
#define PINMUX_PA18_GPIO        ((PIN_PA18 << 16)  |   PINOP_GPIO)
#define PINMUX_PA19_GPIO        ((PIN_PA19 << 16)  |   PINOP_GPIO)
#define PINMUX_PA20_GPIO        ((PIN_PA20 << 16)  |   PINOP_GPIO)
#define PINMUX_PA21_GPIO        ((PIN_PA21 << 16)  |   PINOP_GPIO)
#define PINMUX_PA22_GPIO        ((PIN_PA22 << 16)  |   PINOP_GPIO)
#define PINMUX_PA23_GPIO        ((PIN_PA23 << 16)  |   PINOP_GPIO)
#define PINMUX_PA24_GPIO        ((PIN_PA24 << 16)  |   PINOP_GPIO)
#define PINMUX_PA25_GPIO        ((PIN_PA25 << 16)  |   PINOP_GPIO)
#define PINMUX_PA26_GPIO        ((PIN_PA26 << 16)  |   PINOP_GPIO)
#define PINMUX_PA27_GPIO        ((PIN_PA27 << 16)  |   PINOP_GPIO)
#define PINMUX_PA28_GPIO        ((PIN_PA28 << 16)  |   PINOP_GPIO)
#define PINMUX_PA29_GPIO        ((PIN_PA29 << 16)  |   PINOP_GPIO)
#define PINMUX_PA30_GPIO        ((PIN_PA30 << 16)  |   PINOP_GPIO)
#define PINMUX_PA31_GPIO        ((PIN_PA31 << 16)  |   PINOP_GPIO)

#define PINMUX_PB00_GPIO        ((PIN_PB00 << 16)  |   PINOP_GPIO)
#define PINMUX_PB01_GPIO        ((PIN_PB01 << 16)  |   PINOP_GPIO)
#define PINMUX_PB02_GPIO        ((PIN_PB02 << 16)  |   PINOP_GPIO)
#define PINMUX_PB03_GPIO        ((PIN_PB03 << 16)  |   PINOP_GPIO)
#define PINMUX_PB04_GPIO        ((PIN_PB04 << 16)  |   PINOP_GPIO)
#define PINMUX_PB05_GPIO        ((PIN_PB05 << 16)  |   PINOP_GPIO)
#define PINMUX_PB06_GPIO        ((PIN_PB06 << 16)  |   PINOP_GPIO)
#define PINMUX_PB07_GPIO        ((PIN_PB07 << 16)  |   PINOP_GPIO)
#define PINMUX_PB08_GPIO        ((PIN_PB08 << 16)  |   PINOP_GPIO)
#define PINMUX_PB09_GPIO        ((PIN_PB09 << 16)  |   PINOP_GPIO)
#define PINMUX_PB10_GPIO        ((PIN_PB10 << 16)  |   PINOP_GPIO)
#define PINMUX_PB11_GPIO        ((PIN_PB11 << 16)  |   PINOP_GPIO)
#define PINMUX_PB12_GPIO        ((PIN_PB12 << 16)  |   PINOP_GPIO)
#define PINMUX_PB13_GPIO        ((PIN_PB13 << 16)  |   PINOP_GPIO)
#define PINMUX_PB14_GPIO        ((PIN_PB14 << 16)  |   PINOP_GPIO)
#define PINMUX_PB15_GPIO        ((PIN_PB15 << 16)  |   PINOP_GPIO)
#define PINMUX_PB16_GPIO        ((PIN_PB16 << 16)  |   PINOP_GPIO)
#define PINMUX_PB17_GPIO        ((PIN_PB17 << 16)  |   PINOP_GPIO)
#define PINMUX_PB18_GPIO        ((PIN_PB18 << 16)  |   PINOP_GPIO)
#define PINMUX_PB19_GPIO        ((PIN_PB19 << 16)  |   PINOP_GPIO)
#define PINMUX_PB20_GPIO        ((PIN_PB20 << 16)  |   PINOP_GPIO)
#define PINMUX_PB21_GPIO        ((PIN_PB21 << 16)  |   PINOP_GPIO)
#define PINMUX_PB22_GPIO        ((PIN_PB22 << 16)  |   PINOP_GPIO)
#define PINMUX_PB23_GPIO        ((PIN_PB23 << 16)  |   PINOP_GPIO)
#define PINMUX_PB24_GPIO        ((PIN_PB24 << 16)  |   PINOP_GPIO)
#define PINMUX_PB25_GPIO        ((PIN_PB25 << 16)  |   PINOP_GPIO)
#define PINMUX_PB26_GPIO        ((PIN_PB26 << 16)  |   PINOP_GPIO)
#define PINMUX_PB27_GPIO        ((PIN_PB27 << 16)  |   PINOP_GPIO)
#define PINMUX_PB28_GPIO        ((PIN_PB28 << 16)  |   PINOP_GPIO)
#define PINMUX_PB29_GPIO        ((PIN_PB29 << 16)  |   PINOP_GPIO)
#define PINMUX_PB30_GPIO        ((PIN_PB30 << 16)  |   PINOP_GPIO)
#define PINMUX_PB31_GPIO        ((PIN_PB31 << 16)  |   PINOP_GPIO)

#define PINMUX_PC00_GPIO        ((PIN_PC00 << 16)  |   PINOP_GPIO)
#define PINMUX_PC01_GPIO        ((PIN_PC01 << 16)  |   PINOP_GPIO)
#define PINMUX_PC02_GPIO        ((PIN_PC02 << 16)  |   PINOP_GPIO)
#define PINMUX_PC03_GPIO        ((PIN_PC03 << 16)  |   PINOP_GPIO)
#define PINMUX_PC04_GPIO        ((PIN_PC04 << 16)  |   PINOP_GPIO)
#define PINMUX_PC05_GPIO        ((PIN_PC05 << 16)  |   PINOP_GPIO)
#define PINMUX_PC06_GPIO        ((PIN_PC06 << 16)  |   PINOP_GPIO)
#define PINMUX_PC07_GPIO        ((PIN_PC07 << 16)  |   PINOP_GPIO)
#define PINMUX_PC08_GPIO        ((PIN_PC08 << 16)  |   PINOP_GPIO)
#define PINMUX_PC09_GPIO        ((PIN_PC09 << 16)  |   PINOP_GPIO)
#define PINMUX_PC10_GPIO        ((PIN_PC10 << 16)  |   PINOP_GPIO)
#define PINMUX_PC11_GPIO        ((PIN_PC11 << 16)  |   PINOP_GPIO)
#define PINMUX_PC12_GPIO        ((PIN_PC12 << 16)  |   PINOP_GPIO)
#define PINMUX_PC13_GPIO        ((PIN_PC13 << 16)  |   PINOP_GPIO)
#define PINMUX_PC14_GPIO        ((PIN_PC14 << 16)  |   PINOP_GPIO)
#define PINMUX_PC15_GPIO        ((PIN_PC15 << 16)  |   PINOP_GPIO)
#define PINMUX_PC16_GPIO        ((PIN_PC16 << 16)  |   PINOP_GPIO)
#define PINMUX_PC17_GPIO        ((PIN_PC17 << 16)  |   PINOP_GPIO)
#define PINMUX_PC18_GPIO        ((PIN_PC18 << 16)  |   PINOP_GPIO)
#define PINMUX_PC19_GPIO        ((PIN_PC19 << 16)  |   PINOP_GPIO)
#define PINMUX_PC20_GPIO        ((PIN_PC20 << 16)  |   PINOP_GPIO)
#define PINMUX_PC21_GPIO        ((PIN_PC21 << 16)  |   PINOP_GPIO)
#define PINMUX_PC22_GPIO        ((PIN_PC22 << 16)  |   PINOP_GPIO)
#define PINMUX_PC23_GPIO        ((PIN_PC23 << 16)  |   PINOP_GPIO)
#define PINMUX_PC24_GPIO        ((PIN_PC24 << 16)  |   PINOP_GPIO)
#define PINMUX_PC25_GPIO        ((PIN_PC25 << 16)  |   PINOP_GPIO)
#define PINMUX_PC26_GPIO        ((PIN_PC26 << 16)  |   PINOP_GPIO)
#define PINMUX_PC27_GPIO        ((PIN_PC27 << 16)  |   PINOP_GPIO)
#define PINMUX_PC28_GPIO        ((PIN_PC28 << 16)  |   PINOP_GPIO)
#define PINMUX_PC29_GPIO        ((PIN_PC29 << 16)  |   PINOP_GPIO)
#define PINMUX_PC30_GPIO        ((PIN_PC30 << 16)  |   PINOP_GPIO)
#define PINMUX_PC31_GPIO        ((PIN_PC31 << 16)  |   PINOP_GPIO)

#define PINMUX_PD00_GPIO        ((PIN_PD00 << 16)  |   PINOP_GPIO)
#define PINMUX_PD01_GPIO        ((PIN_PD01 << 16)  |   PINOP_GPIO)
#define PINMUX_PD02_GPIO        ((PIN_PD02 << 16)  |   PINOP_GPIO)
#define PINMUX_PD03_GPIO        ((PIN_PD03 << 16)  |   PINOP_GPIO)
#define PINMUX_PD04_GPIO        ((PIN_PD04 << 16)  |   PINOP_GPIO)
#define PINMUX_PD05_GPIO        ((PIN_PD05 << 16)  |   PINOP_GPIO)
#define PINMUX_PD06_GPIO        ((PIN_PD06 << 16)  |   PINOP_GPIO)
#define PINMUX_PD07_GPIO        ((PIN_PD07 << 16)  |   PINOP_GPIO)
#define PINMUX_PD08_GPIO        ((PIN_PD08 << 16)  |   PINOP_GPIO)
#define PINMUX_PD09_GPIO        ((PIN_PD09 << 16)  |   PINOP_GPIO)
#define PINMUX_PD10_GPIO        ((PIN_PD10 << 16)  |   PINOP_GPIO)
#define PINMUX_PD11_GPIO        ((PIN_PD11 << 16)  |   PINOP_GPIO)
#define PINMUX_PD12_GPIO        ((PIN_PD12 << 16)  |   PINOP_GPIO)
#define PINMUX_PD13_GPIO        ((PIN_PD13 << 16)  |   PINOP_GPIO)
#define PINMUX_PD14_GPIO        ((PIN_PD14 << 16)  |   PINOP_GPIO)
#define PINMUX_PD15_GPIO        ((PIN_PD15 << 16)  |   PINOP_GPIO)
#define PINMUX_PD16_GPIO        ((PIN_PD16 << 16)  |   PINOP_GPIO)
#define PINMUX_PD17_GPIO        ((PIN_PD17 << 16)  |   PINOP_GPIO)
#define PINMUX_PD18_GPIO        ((PIN_PD18 << 16)  |   PINOP_GPIO)
#define PINMUX_PD19_GPIO        ((PIN_PD19 << 16)  |   PINOP_GPIO)
#define PINMUX_PD20_GPIO        ((PIN_PD20 << 16)  |   PINOP_GPIO)
#define PINMUX_PD21_GPIO        ((PIN_PD21 << 16)  |   PINOP_GPIO)
#define PINMUX_PD22_GPIO        ((PIN_PD22 << 16)  |   PINOP_GPIO)
#define PINMUX_PD23_GPIO        ((PIN_PD23 << 16)  |   PINOP_GPIO)
#define PINMUX_PD24_GPIO        ((PIN_PD24 << 16)  |   PINOP_GPIO)
#define PINMUX_PD25_GPIO        ((PIN_PD25 << 16)  |   PINOP_GPIO)
#define PINMUX_PD26_GPIO        ((PIN_PD26 << 16)  |   PINOP_GPIO)
#define PINMUX_PD27_GPIO        ((PIN_PD27 << 16)  |   PINOP_GPIO)
#define PINMUX_PD28_GPIO        ((PIN_PD28 << 16)  |   PINOP_GPIO)
#define PINMUX_PD29_GPIO        ((PIN_PD29 << 16)  |   PINOP_GPIO)
#define PINMUX_PD30_GPIO        ((PIN_PD30 << 16)  |   PINOP_GPIO)
#define PINMUX_PD31_GPIO        ((PIN_PD31 << 16)  |   PINOP_GPIO)
//------------------------------------------------------------------------------
//GPIO kezeles kezdeti initje
void MyGPIO_initPorts(void);

//GPIO pin beallitasa, az config-ban megadott modon.
//Config egyes bitmezoi mas es mast jelentenek. Meghatarozzak pl a portot,
// a porton beluli pint, a pinhez tartozo jellemzoket (pl felhuzas, lehuzas,
// meghajtas, stb...)
void MyGPIO_pinConfig(uint32_t config);

//IO-k csoportos beallitasa. Addig megy amig a PINCONFIG_END bejegyzesbe nem
//utkozik.
void MyGPIO_pinGroupConfig(const uint32_t* pinConfigs);


#endif // MY_GPIO_H

