import qbs

//A projektet az SamFrameworkApplication QBS osztalybol szarmaztatjuk, igy a 
//most definialt projekt fuggosege lesz a Framework, es igy az hozza fog adodni.
SamFrameworkApplication
{      
    name: "_TestFirmware APP_"
    //cpp.optimization: "size"


    Group
    {
        name: "MAIN"
        files:
        [
            "src/*.c",
            "src/*.h",
        ]
    }
    Group
    {
        name: "CONSOLE"
        prefix: "src/Console/"
        files:
        [
            "*.c",
            "*.h",
        ]
    }

    Group
    {
        name: "CONFIGS"
        prefix: "src/Configs/"
        files:
        [
            "*.c",
            "*.h",
        ]
    }
	
	Group
    {
        name: "SYSTEM"
        prefix: "src/System/"
        files:
        [
            "*.c",
            "*.h",
        ]
    }
	
    //--------------------------------------------------------------------------
	
	//____Projekten beluli keresesi utvonalak hozzaadasa____
    cpp.includePaths: base.concat
    ([
         "src",
         "src/Configs",
         "src/Console",
     ])
	 
	 
	//____A megfelelo linker file kivalasztasa a forditasi mod szerint___ 
    Group
    {
        name: "LINKER_SCRIPT DEV"
        condition: qbs.buildVariant == "debug"
        files:
        [
            //linkerFileName
            "src/LinkerScripts/same54p20a_flash.ld"
        ]
    } 

    Group
    {
        name: "LINKER_SCRIPT FOR BOOTLOADER"
        condition: qbs.buildVariant == "release"
        files:
        [
            //linkerFileName
            "src/LinkerScripts/same54p20a_flash_bootloader.ld"
        ]
    }

	
	//____Itt adhatok hozza az egyedi linker flagek...____
    cpp.linkerFlags:
    [
        "-cref",
        //Generaltatunk map file-t. (Ez hasznos lehet kesobb.)
        "-Map=" + buildDirectory  + "/" + name + ".map",
        "-defsym=__heap_size__=+"+heapSize,
        "-defsym=__stack_size__=+"+stackSize,
    ].concat(base)

    //Applikaciora vonatkozo globalis definiciok hozzaadasa
    appDefines: base.concat(["FOO", "DEBUG"])
}

