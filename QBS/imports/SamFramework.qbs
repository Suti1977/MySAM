//------------------------------------------------------------------------------
// Atmel/Microchip ASF alapu projektek alap osztalya. Ebben kerul illesztesre
// a projektekhez a webfeluleten osszeallitott ASF csomag.
//------------------------------------------------------------------------------
import qbs;
import qbs.FileInfo
import qbs.File
import qbs.Environment
//------------------------------------------------------------------------------
ArmMcuProduct 
{
    id: SAM_Framework
    name: "SAM_FRAMEWORK"

	//A webfeluleten osszeallitott ASF csomag lokacioja
    property path asfPath : project.asfPath

    //A framework gyoker konyvtara, ehehz a fajlhoz kepest relativ megadva.
    // SAMFW/QBS/import
    // ^
    property path mywayPath: path + "/../../"

    //A frameworkhoz adott FreeRTOS forrasok eleresi utja. A QBS-ben megtortenik
    //annak felderitese, es a forrasok hozzaadas
    property path freeRTOS_Path: mywayPath+"Thirdparty/FreeRTOS/"
    //FreeRTOS eseten alkalmazando memoria managger verzioja.
    //https://www.freertos.org/a00111.html
    property string memManagerVerison: project.memManagerVerison;


    //Az applikacioban megadott globalis definiciok gyujtoje
    property stringList  appDefines

    //--------------------------------------------------------------------------
    //Globalis definiciok listajanak kialakitasa...
    property stringList globalDefs:
    {
        var arr = [];

        //Applikacio projektben megadott definiciok hozzaadasa...
        for(var i in appDefines)
        {
            arr.push(appDefines[i]);
        }

        //FREE RTOS hasznalat jelzese egy globalis definiciok szerint
        if (project.useFreeRTOS)
        {   //SEGGER RTT hozzaadasa
            arr.push("USE_FREERTOS");
        }

        return arr;
    }
    //--------------------------------------------------------------------------
    //3rd party konyvtarak felsorolasa
    property pathList thidpartySourceDirectories:
    {
        var arr = [];

        if (project.useSeggerRTT)
        {   //SEGGER RTT hozzaadasa
            arr.push(mywayPath+"/Thirdparty/SEGGER_RTT");
        }

        if (project.useSeggerSysView)
        {   //SEGGER SYSTEM VIEW hozzaadasa
            arr.push(mywayPath+"/Thirdparty/SEGGER_SYSVIEW");
        }

        if (project.uselibfixmath)
        {   //libfixmath fixpontos lib hozzaadasa
            arr.push(mywayPath+"/Thirdparty/libfixmath/libfixmath");
        }

        return arr;
    }
    //A forraskonyvtarakban a forras fileok nevere filter letrehozasa
    property pathList thirdpartySourceFiles:
    {
        var arr = [];
        for(var i in mywaySourceDirectories)
        {
            arr.push(thidpartySourceDirectories[i]+"/*.c");
            arr.push(thidpartySourceDirectories[i]+"/*.s");
            arr.push(thidpartySourceDirectories[i]+"/*.h");
        }

        for(var i in freeRTOS_SourceFiles)
        {
            arr.push(freeRTOS_SourceFiles[i]);
        }
        for(var i in freeRTOS_HeaderFiles)
        {
            arr.push(freeRTOS_HeaderFiles[i]);
        }
        for(var i in freeRTOS_AssemblyFiles)
        {
            arr.push(freeRTOS_AssemblyFiles[i]);
        }

        return arr;
    }

    //3rd party konyvtarak, melyeket hozza kell adni az eleresi utakhoz
    property pathList thirdpartyIncluePaths:
    {
        var arr = [];
        for(var i in thidpartySourceDirectories)
        {
            arr.push(thidpartySourceDirectories[i]);
        }
        return arr;
    }
    //--------------------------------------------------------------------------
    //Sajat framework forraskonyvtarak felsorolasa
    property pathList mywaySourceDirectories:
    {
        var arr = [];
        arr.push(mywayPath+"/MyDrivers");
        arr.push(mywayPath+"/MyBase");
        arr.push(mywayPath+"/MyUtils");
        arr.push(mywayPath+"/MyServices");
        arr.push(mywayPath+"/MyDevices");
        return arr;
    }

    //A forraskonyvtarakban a forras fileok nevere filter letrehozasa
    property pathList mywaySourceFiles:
    {
        var arr = [];
        for(var i in mywaySourceDirectories)
        {
            arr.push(mywaySourceDirectories[i]+"/*.c");
            arr.push(mywaySourceDirectories[i]+"/*.s");
            arr.push(mywaySourceDirectories[i]+"/*.h");
        }

        return arr;
    }

    //Sajat framework konyvtarak, melyeket hozza kell adni az eleresi utakhoz
    property pathList mywayIncluePaths:
    {
        var arr = [];
        for(var i in mywaySourceDirectories)
        {
            arr.push(mywaySourceDirectories[i]);
        }
        return arr;
    }
    //--------------------------------------------------------------------------

		 	
	//Fraework forditasara vonatkozo optimalizazios beallitas atvetele.
    //cpp.optimization: "size" //project.frameworkOptimization

    //Common functions
    property var functions:
    {
        return {
            makePath: (function (path, subpath)
            {
                return FileInfo.joinPaths(path, subpath);
            }),
            getFilePath: (function (file)
            {
                return FileInfo.path(file);
            }),
            getFileName: (function (file)
            {
                return FileInfo.baseName(file);
            }),
            print: (function (message)
            {
                console.info(message);
            }),
            getDirectories: (function (directory)
            {
                var dirs = File.directoryEntries(directory, File.Dirs | File.NoDotAndDotDot);
                dirs.forEach(function (file, index, array)
                {
                    array[index] = FileInfo.joinPaths(this.path, file);
                },
                {
                    path: directory
                });
                return dirs;
            }),
            getFiles: (function (directory)
            {
                var files = File.directoryEntries(directory, File.Files);
                files.forEach(function (file, index, array)
                {
                    array[index] = FileInfo.joinPaths(this.path, file);
                },
                {
                    path: directory
                });
                return files;
            }),
            getKeys: (function (object)
            {
                return Object.keys(object);
            }),
            prependPath: (function (file, index, array)
            {
                array[index] = FileInfo.joinPaths(this.path, file);
            }),
            forAll: (function (object, func, context)
            {
                context.object = object;
                context.func = func;
                Object.keys(object).forEach(function (name)
                {
                    this.name = name;
                    this.func.call(this, this.object[name]);
                }, context);
                return object;
            }),
            callByName: (function (name)
            {
                this.name = name;
                this.func.call(this, this.object[name]);
            }),
            isValid: (function (object)
            {
                return object && Object.keys(object).length > 0;
            }),
            capitalizeFirst: (function (str)
            {
                return str.charAt(0).toUpperCase() + str.slice(1);
            }),
            addFind: (function ()
            {
                if(!Array.prototype.find)
                {
                    Object.defineProperty(Array.prototype, 'find',
                    {
                        value: function (predicate)
                        {
                            if(this === null)
                            {
                                throw new TypeError('"this" is null or not defined');
                            }
                            if(typeof predicate !== 'function')
                            {
                                throw new TypeError('predicate must be a function');
                            }
                            for(var k = 0; k < (Object(this).length >>> 0); k++)
                            {
                                if(predicate.call(arguments[1], Object(this)[k], k, Object(this)))
                                {
                                    return Object(this)[k];
                                }
                            }
                            return undefined;
                        }
                    });
                }
            })
        };
    }
    //End of common functions

    //--------------------------------------------------------------------------
    //ASF csomagon beluli osszes konyvtar megkeresese...
    property stringList asfPaths: asf_scanner.asfDirectories
    property stringList asfHeaderFiles: asf_scanner.asfHeaderFiles
    property stringList asfSourceFiles: asf_scanner.asfSourceFiles
    property stringList asfAssemblyFiles: asf_scanner.asfAssemblyFiles

    Probe
    {
        id: asf_scanner
        //ASF-ben talalhato konyvtarak listaja
        property var asfDirectories: ({})
        //ASF-ben talalhato header fajlok listaja (*.h, *.hpp)
        property var asfHeaderFiles: ({})
        //ASF-ben talalhato forras fajlok listaja (*.c, *.cpp)
        property var asfSourceFiles: ({})
        //ASF-ben talalhato assembly fajlok listaja (*.s)
        property var asfAssemblyFiles: ({})

        property var functions: parent.functions
        property var asfPath:   parent.asfPath

        configure:
        {                        
            //Rekurzioval vegigkeresi az oszes konyvtarat az ASF-es csomagban
            function scanAsfDirectories(path)
            {
                var files = [];
                var dirs = [];
                var dirs1 = [];
                //Konyvtarban levo alkonyvtarak listajanak lekerdezese
                dirs1 = File.directoryEntries(path, File.Dirs | File.NoDotAndDotDot);
                //Vegighaladunk az alkonyvtar neveken, es kiszurjuk azokat,
                //melyek biztosan nem a GCC-s forditohoz keszultek
                for(var i in dirs1)
                {
                    var d=dirs1[i];

                    if (d=="armcc") continue;
                    if (d=="IAR") continue;
                    if (d=="RVDS") continue;
                    //hozzafuzi az eddigi pathoz az megfelelo path
                    //szeparatorral ("/") a soron kovetkezo konyvtar nevet.
                    //Hozzaadjuk a listaba kerulo konyvtarakhoz...
                    dirs = dirs.concat( FileInfo.joinPaths( path, d ));
                }

                //A megmaradt konyvtarak hozzafuzese a visszaadando listahoz.
                files=files.concat(dirs);

                for(var i in dirs)
                {
                    files = files.concat(scanAsfDirectories(dirs[i]));
                }

                return files;
            } //scanAsfDirectories

            functions.print("Scanning ASF directiries....");
            asfDirectories=scanAsfDirectories(asfPath);

            //A talalt konyvtarakbol kigyujti a forras es header es asm fajlokat
            functions.print("Scanning ASF sources...");
            //Minden ASF-es konyvtaron (amit korabban felderitett) vegigmegy, es
            //azokban megkeresei a c fajlokat, melyeket hozzaad a listahoz.
            var headerFiles = [];
            var sourceFiles = [];
            var assemblyFiles = [];
            for(var i in asfDirectories)
            {
                var actualDir=asfDirectories[i];
                functions.print("___ " + actualDir + " ___");

                //Az osszes file lekerdezese, amit talal a konyvtarakban
                var files=functions.getFiles(actualDir);
                for(var j in files)
                {
                    //Kiterjesztes alapjan a megfelelo listahoz adja a fajlokat
                    var actualFile=files[j];
                    var suffix=FileInfo.suffix(actualFile);

                    //if (FileInfo.fileName(actualFile)=="startup_samd51.c") continue;

                    if ((suffix=="c") || (suffix=="cpp"))
                    {
                        functions.print(" " + suffix +"  " + actualFile);
                        sourceFiles = sourceFiles.concat(actualFile);
                    } else
                    if ((suffix=="h") || (suffix=="hpp"))
                    {
                        functions.print(" " + suffix +"  " + actualFile);
                        headerFiles = headerFiles.concat(actualFile);
                    } else
                    if (suffix=="s")
                    {
                        functions.print(" " + suffix +"  " + actualFile);
                        assemblyFiles = assemblyFiles.concat(actualFile);
                    }
                } //for(var j in files)
            } //for(var i in asfDirectories)

            //Az ASF rootjaban levo ASF-et inicializalo forrasok hozzaadasa
            sourceFiles=sourceFiles.concat(asfPath+"/atmel_start.c");
            sourceFiles=sourceFiles.concat(asfPath+"/driver_init.c");

            //Talalt forrasok publikalasa...
            asfHeaderFiles=headerFiles;
            asfSourceFiles=sourceFiles;
            asfAssemblyFiles=assemblyFiles;

            //Jelzes, hogy a probe lefutott. (kotelezo elem)
            found=true
        } //configure
    } //probe

    //--------------------------------------------------------------------------
    //FreeRTOS hozzaadasa a projekthez
    property stringList freeRTOS_Directories:   freeRTOS_scanner.rtosDirectories
    property stringList freeRTOS_HeaderFiles:   freeRTOS_scanner.rtosHeaderFiles
    property stringList freeRTOS_SourceFiles:   freeRTOS_scanner.rtosSourceFiles
    property stringList freeRTOS_AssemblyFiles: freeRTOS_scanner.rtosAssemblyFiles

    Probe
    {
        id: freeRTOS_scanner
        condition:  parent.useFreeRTOS

        //FreeRTOS-forrasokhoz tartozo konyvtarak listaja
        property var rtosDirectories
        //FreeRTOS-hez tartozo header fajlok listaja (*.h, *.hpp)
        property var rtosHeaderFiles
        //FreeRTOS-hez tartozo forras fajlok listaja (*.c, *.cpp)
        property var rtosSourceFiles
        //FreeRTOS-hez tartozo assembly fajlok listaja (*.s)
        property var rtosAssemblyFiles

        property var functions:       parent.functions
        property var freeRTOS_Path:   parent.freeRTOS_Path
        property var memManVerison:   parent.memManagerVerison


        configure:
        {
            //Rekurzioval vegigkeresi az oszes konyvtarat ami a FreeRTOS-hez van
            function scanRtosDirectories(path)
            {
                var files = [];
                var dirs = [];
                var dirs1 = [];
                //Konyvtarban levo alkonyvtarak listajanak lekerdezese
                dirs1 = File.directoryEntries(path, File.Dirs | File.NoDotAndDotDot);
                //Vegighaladunk az alkonyvtar neveken, es kiszurjuk azokat,
                //melyek biztosan nem a GCC-s forditohoz keszultek
                for(var i in dirs1)
                {
                    var d=dirs1[i];
                    //hozzafuzi az eddigi pathoz az megfelelo path
                    //szeparatorral ("/") a soron kovetkezo konyvtar nevet.
                    //Hozzaadjuk a listaba kerulo konyvtarakhoz...
                    dirs = dirs.concat( FileInfo.joinPaths( path, d ));
                }

                //A megmaradt konyvtarak hozzafuzese a visszaadando listahoz.
                files=files.concat(dirs);

                for(var i in dirs)
                {
                    files = files.concat(scanRtosDirectories(dirs[i]));
                }

                return files;
            } //scanRtosDirectories

            functions.print("Scanning FreeRTOS directiries....");
            rtosDirectories=scanRtosDirectories(freeRTOS_Path);

            //A talalt konyvtarakbol kigyujti a forras es header es asm fajlokat
            functions.print("Scanning FreeRTOS sources...");
            //Minden konyvtaron (amit korabban felderitett) vegigmegy, es
            //azokban megkeresei a c fajlokat, melyeket hozzaad a listahoz.
            var headerFiles = [];
            var sourceFiles = [];
            var assemblyFiles = [];
            for(var i in rtosDirectories)
            {
                var actualDir=rtosDirectories[i];
                functions.print("___ " + actualDir + " ___");

                //Az osszes file lekerdezese, amit talal a konyvtarakban
                var files=functions.getFiles(actualDir);
                for(var j in files)
                {
                    //Kiterjesztes alapjan a megfelelo listahoz adja a fajlokat
                    var actualFile=files[j];
                    var suffix=FileInfo.suffix(actualFile);
                    var fileName=FileInfo.fileName(actualFile);




                    if ((suffix=="c") || (suffix=="cpp"))
                    {
                        functions.print(" " + suffix +"  " + actualFile);
                        //Csak a specifikalt memoria manager forrasat adjuk hozza.
                        if (fileName.startsWith("heap_"))
                        {   //ez egy memoria manager file.

                            if (fileName != ("heap_" + memManVerison + ".c"))
                                continue;
                        }

                        sourceFiles = sourceFiles.concat(actualFile);
                    } else
                    if ((suffix=="h") || (suffix=="hpp"))
                    {
                        functions.print(" " + suffix +"  " + actualFile);
                        headerFiles = headerFiles.concat(actualFile);
                    } else
                    if (suffix=="s")
                    {
                        functions.print(" " + suffix +"  " + actualFile);
                        assemblyFiles = assemblyFiles.concat(actualFile);
                    }
                } //for(var j in files)
            } //for(var i in rtosDirectories)

            //Talalt forrasok publikalasa...
            rtosHeaderFiles=headerFiles;
            rtosSourceFiles=sourceFiles;
            rtosAssemblyFiles=assemblyFiles;

            //Jelzes, hogy a probe lefutott. (kotelezo elem)
            found=true
        } //configure
    } //Probe


    //--------------------------------------------------------------------------
    //A framework hasznalata altal a projektekhez hozzaadando eleresi utak
	//listajanak osszeallitasa.
	property pathList frameworkIncludePaths:
	{
		var arr =	
        [   //Az asf root-ja hozza lesz adva
            asfPath,
        ];
		
        //Minden alkonyvtart hozzaad az eleresi utakhoz
        for(var i in asfPaths)
        {
            arr.push(asfPaths[i]);
        }

        //sajat modulok eleresi utjainak hozzaadasa
        for(var i in mywayIncluePaths)
        {
            arr.push(mywayIncluePaths[i]);
        }

        //3rd party konyvtarak utjainak hozzaadasa
        for(var i in thirdpartyIncluePaths)
        {
            arr.push(thirdpartyIncluePaths[i]);
        }

        //FreeRTOS forraskonytarak eleresi utjainak hozzaadasa
        if (project.useFreeRTOS)
        {
            for(var i in freeRTOS_Directories)
            {
                arr.push(freeRTOS_Directories[i]);
            }
        }

		return arr;
    }

    Group
    {
        name: "_ASF SOURCES"
        files: asfSourceFiles
    }

    Group
    {
        name: "_MyWAY SOURCES"
        files: mywaySourceFiles
    }

    Group
    {
        name: "_3RD PARTY SOURCES"
        files: thirdpartySourceFiles
    }

    //A framework forditasahoz szukseges eleresi utak beallitasa a GCC szamara
    cpp.includePaths: base.concat(frameworkIncludePaths)

    //globalis definiciok hozzaadasa
    cpp.defines: base.concat(globalDefs)

    Export 
	{
        Depends { name: "cpp" }
        cpp.includePaths: product.frameworkIncludePaths
        cpp.defines: product.globalDefs
    }
}

