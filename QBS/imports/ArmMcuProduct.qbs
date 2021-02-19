//------------------------------------------------------------------------------
// ARM mikorvezerlokre valo forditas alap product osztalya. A GCC forditohoz
// szukseges leg alapabb beallitasok kerulnek ebben beallitasra
//------------------------------------------------------------------------------
import qbs
import qbs.FileInfo

Product 
{
	//Alapertelmezett processzor definiciok. Ez majd az alkalmazott
	//processzor fuggvenyeben felul fog irodni.    
    property string cpuName:  "cortex-m4"
    property string floatAbi: "softfp"
    property string fpuName:  "fpv4-sp-d16"
    
    type: "staticlibrary"

    Depends { name: "cpp" }

	//A projekthez hozzaadott ".ld" kiterjesztesu fileok "linkerscript" TAG-et
	//kapnak. 
    //FileTagger
    //{
    //    patterns: "*.ld"
    //    fileTags: ["linkerscript"]
    //}

    cpp.cLanguageVersion: "c11"         //11
    cpp.cxxLanguageVersion: "c++11"     //11
    cpp.positionIndependentCode: false
    cpp.optimization: "small"
    cpp.debugInformation: true
    cpp.enableExceptions: false
    cpp.enableRtti: false
    cpp.enableReproducibleBuilds: true
    cpp.treatSystemHeadersAsDependencies: true
    
    cpp.staticLibraries: base.concat(
	[
        //"gcc",
        //"c",
        //"m",
    ])
    
    //CPP forditonal hasznalt alapertelmezett flagek
    cpp.driverFlags: 
	{
        var arr = [
                    "-mcpu=" + cpuName,
                    "-mfloat-abi=" + floatAbi,
                    "-mthumb",

                    "-mabi=aapcs",
                    "-mno-sched-prolog",
                    "-mabort-on-noreturn",

                    //kiszedi a nem hasznalt data szekciokat -->kisebb kod
                    "-fdata-sections",
                    //kiszedi a nem hasznalt kod szekciokat -->kisebb kod
                    "-ffunction-sections",

                    //"-fno-strict-aliasing",
                    "-fno-builtin",

                    "-specs=nosys.specs",
                    "-specs=nano.specs",

                    "-static",
                    //       "-nodefaultlibs",
                    "-Wdouble-promotion",
                    //"-ggdb",

                    //float es double kezelese printf es scanf-ben
                    //Ezek mindegyike kb 8 - 8 kilobyteal megnovelik a kodot!
                    "-u", "_printf_float",
//                    "-u", "_scanf_float",
                ];

        if (fpuName && typeof(fpuName) === "string") 
		{
            arr.push("-mfpu=" + fpuName);
        }

        return arr;
    }

    cpp.defines : base.concat(project.deviceDef)

	//C++ forditasi flagek
    cpp.cxxFlags: base.concat(
	[
    ])

	//Linker flagek
    cpp.linkerFlags: base.concat(
	[
		//Nem hasznalt szekciokat nem forditja bele a kodba. -->kisebb lesz
        "--gc-sections",
    ])
}

