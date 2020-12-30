//------------------------------------------------------------------------------
//  A sajat SAM framework-re epito projektek alapja.
//  Ez a qbs tartalmazza a frameworkos projekt epiteshez szukseges property-ket.
//------------------------------------------------------------------------------
import qbs

//------------------------------------------------------------------------------
Project
{
	minimumQbsVersion: "1.6"

	//Az ASF eleresi utja, melyet a projektnek kell beallitania.
	property path asfPath: "__NINCS MEGADVA AZ ASF ELERESI UTJA!__"

	//A mikrovezerlohoz tartozo definicio (__ATSAMxxxx__)
	property string deviceDef : "__NINCS MEGDADVA AZ ESZKOZ DEFINICIO__"

	//Beallithato a heap merete
	property int heapSize:  undefined
	//Beallithato a stack merete
	property int stackSize: undefined

    //Az alabbi property-k egyes komponensek beforditasat engedelyezik...
	//Az egyes komponenseket true-val lehet engedelyezni.

	//SEGGER RTT hasznlata.
    property bool useSeggerRTT: false

    //SEGGER SYSTEMVIEW hasznalata
    property bool useSeggerSysView: false

    //FreeRTOS hasznalva van, akkor true.
    property bool useFreeRTOS: false
    //FreeRTOS eseten hasznalando memoria manager verzioja. Ennek megfelelo
    //heap_1.c, heap_2.c ... fordul bele
    property string memManagerVerison: "1"

    //libfixmath hasznalata (fix pontos MIT licences lib)
    property bool uselibfixmath: false

    //spiffs opensource lib hasznalata
    property bool useSpiffs: false


	//Ha ez true, akkor az indulo (startup) fileokat az applikacionak kell 
	//biztositani, es nem az kerul beforditasra, amit a CMSIS-ek biztositanak.
	//Ez foleg bootloaderes applikaciok eseten szukseges.
	//property bool dontUseDefaultStartupFiles: false

	//A framework-re vonatkozo optimalizacios property. (Kesobb felulirhato.)
    //property string frameworkOptimization: "small"

    //property string applicationOptimization: "small"
}
//------------------------------------------------------------------------------
