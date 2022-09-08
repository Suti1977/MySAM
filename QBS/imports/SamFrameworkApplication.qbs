//------------------------------------------------------------------------------
// Ebbol a QBS fajlbol szarmaztatjuk az egyes applikacios projekteket.
//------------------------------------------------------------------------------
//STUFF: https://qbs.qt-project.narkive.com/upzKjMxQ/can-t-find-variable-when-using-custom-property
//https://doc.qt.io/qbs/qml-qbsmodules-cpp.html#toolchainInstallPath-prop

import qbs;
import qbs.FileInfo


SamFramework
{
    type: ["application", "hex", "bin", "size"] 

    //Az alapertelmezett kimeneti file kiterjesztesenek beallitasa
    cpp.executableSuffix: ".elf"
    //cpp.optimization : project.applicationOptimization
    //..........................................................................
    //HEXA-ra konvertalas
    Rule
    {
       id: hex
       inputs: ["application"]

       prepare:
       {
           var args = ["-O", "ihex", input.filePath, output.filePath];
           var cmd = new Command(product.toolchainInstallPath + "/arm-none-eabi-objcopy", args);
           cmd.description = "converting to hex: " + FileInfo.fileName(input.filePath);
           cmd.highlight = "linker";
           return cmd;
       }

       Artifact
       {
           fileTags: ["hex"]
           filePath: FileInfo.baseName(input.filePath) + ".hex"
       }
    }

    //..........................................................................
    //BIN-re konvertalas
    Rule
    {
       id: bin
       inputs: ["application"]
       prepare:
       {           
           var args = ["-O", "binary", input.filePath, output.filePath];
           var cmd = new Command(product.toolchainInstallPath + "/arm-none-eabi-objcopy", args);
           cmd.description = "converting to bin: "+ FileInfo.fileName(input.filePath);
           cmd.highlight = "linker";
           return cmd;
       }

       Artifact
       {
           fileTags: ["bin"]
           filePath: FileInfo.baseName(input.filePath) + ".bin"
       }
    }

    //..........................................................................
    //Memoria informacio megjelenitese a forditasi konzolon
    Rule
    {
       id: size
       inputs: ["application"]
       alwaysRun: true
       prepare:
       {           
           var args = [input.filePath];
           var cmd = new Command(product.toolchainInstallPath + "/arm-none-eabi-size", args);
           cmd.description = "File size: " + FileInfo.fileName(input.filePath);
           cmd.highlight = "linker";
           return cmd;
       }

       Artifact
       {
           fileTags: ["size"]
           filePath: undefined
       }
    }
    //..........................................................................

}
