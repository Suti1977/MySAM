import qbs

Base
{
    id: testFirmware
	name: "TestFirmware"

    //A projekt freeRTOS-t hasznal.
    useFreeRTOS: true
    memManagerVerison: "4"

    //A projekt hasznalja a SEGGER RTT konzolt. (Hozzaadjuk a projekthez.)
    //useSeggerRTT: true
	//Hasznaljuk a Segegr SystemView modult
    useSeggerSysView: true

	//Heap es stack meret beallitasa a firmware-hez
    heapSize:  0x100
    stackSize: 0x200

	//Framework forditasanak optimalizalasat lehet itt megadni
    //frameworkOptimization : "size"
    //applicationOptimization: "small"

	//A firmware szoftver projekt-je
    references: base.concat([ "TestFirmware/TestFirmware.qbs"  ])
}
