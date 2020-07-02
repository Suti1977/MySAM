//------------------------------------------------------------------------------
// Framework alapu egyszeru projektek alap qbs fajlja.
// A projekteket ebbol szarmaztatjuk. (Firmwarek, bootloaderek, stb...)
// Ebben kerul beallitasra az egy processzorra vonatkozo definiciok a framework
// szamara.
//------------------------------------------------------------------------------
import qbs 1.0 

import qbs
import qbs.FileInfo
import "MySAM/QBS/imports/SamFrameworkProject.qbs"    	as FrameworkProject

//------------------------------------------------------------------------------
FrameworkProject
{
    //Alkalmazott mikrovezerlore vonatkozo definicio, ami alapjan az ASF-bol
    //kivalasztja az ahhoz tartozo fajlokat.
    deviceDef: "__SAME54P20A__"

    //Az ASF eleresi utja. (Oda lett masolva a webes configuratorral 
	//osszeallitott projket.)
    asfPath:  path + "/ASF_E54"
    	
	//A QBS fajlok keresei utvonalanak beallitasa
    qbsSearchPaths:	 path +"/MySAM/QBS"
}
//------------------------------------------------------------------------------
