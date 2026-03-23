# NMEA library
Libraries to read and interpret NMEA strings coming from serial devices. 
This repository comes with a static NMEA lib containing a parser and a shared library which runs the parser for a serial device.

NMEA data supported by the library includes sentences for:
```
GGA
GLL
GSA
GSV
RMC
VTG
```

Two talkers are supported for GPS and GLONASS satellites, although adding new ones isn't complicated.

## Shared library
The shared library should be installed into the **LIBS:** directory.
This library waits for an application to open the library and specify a serial device and unit to use as a source for NMEA data. 
Once the device is open then strings will be continuously read until an application instructs the library to stop. 
As with all libraries, this will continue to be in memory until all applications have closed the library and it is flushed by the OS.
You can force a flush (assuming no applications have the library open) with a shell command: 
> avail flush

The shared library allows many applications to access the NMEA data at once without needing to juggle access to the serial device providing NMEA data.
All of the sentences and talkers above are included in the **nmea.library**. You may need to configure your satellite device to access GLONASS or GPS data.

Prototypes for the library exist in the **NMEALib/Includes** directory. NMEA library also requires inclusion of **NMEALib/Includes/nmea/libtypes.h**.

In brief, the following functionality is available. Note that this doc may not be fully up to date with the latest minor changes and merges. Check the Include directory for all prototypes.

### AllocateHandle
This should be the first call you make to use the library. This will allocate a handle which basically provides some memory handling and caching to simplify the other functions.
Save the handle for use in other function calls.
If the function fails then NULL will be returned.
Note that you must free this handle when you have finished using the library. Failure to release the handle will mean the memory used isn't released back to the OS.

### FreeHandle
When you have finished with your handle then free the memory using this function call. Do not use the handle again, although you can allocate a new one with a fresh call to **AllocateHandle**.

### OpenSerial
Open a new connection to a serial device by specifying the device name and unit. Note that this must match the correct character case used for the device due to device names being case sensitive.
Any existing serial device will close before the new device is attempted. 
The function returns TRUE if successfully opened, otherwise returns FALSE meaning no device is now open.

### CloseSerial
Close the currently open serial device. This will stop new NMEA data being parsed and and returned information will be out of date.

### Connected
Call this function to check the serial connection is still connected. An open serial device may close outside of the CloseSerial function being called.
It's recommended that your application periodically checks **Connected** and attempts to reopen the serial device if it closes. 

### GetDateTime
Returns a pointer to a data/time structure or NULL if an error occurred. The returned value should be copied immediately if the information is needed by your application. 
Subsequent calls will reuse the memory returned by this function pointer. 
Date and time may not be fully valid if the satellites haven't been detected. Time tends to be quickly picked up but date may need longer to get a fix on a satellite. Check the valid flag when accessing date information.

### GetLocation
Returns a pointer to a location structure or NULL if an error occurred. Location data is held in degrees and minutes. 
Floating point numbers are not used due to the standard base Amigas excluding an FPU. Directions chars specify **N, S, E and W** values for bearings. 

### GetFirstSatellite
To iterate through all detected satellites, firstly call this function to get a pointer to a satellite structure or NULL if no satellites have been detected.
The type of satellite data must be specified in the function call. Use the macros in **libtypes.h** header to specify GPS or GLONASS. 
The return value is temporary and content should be copied if required later on in your application.

### GetNextSatellite
This function call obtains the next satellite or NULL if no further satellites exist. Always use **GetFirstSatellite** to start a new iteration.
The type of satellite data must be specified in the function call. Use the macros in **libtypes.h** header to specify GPS or GLONASS. 
You must specify the type used in your call to **GetFirstSatellite**, otherwise incorrect information will be returned in the **usedForFix** attribute. 
Returned satellite data is temporary and must be copied if needed later on in your application. 

### Libtypes.h 
Include this header along with the prototype headers to get all data structure information. 
Data returned in the structures will be a combination of data received by NMEA strings read from the data source. If the source is closed or fails to continue sending data, then data may become out of date.
Satellite data is a complete record of all satellites detected. The **usedForFix** attribute informs you that this is in view and actively used for location data. 
Other satellites may have gone out of view or provide poor signal. Check **elevationValid** and **azimuthValid** and if these return FALSE then this satellite signal is too weak to get a fix.
The **lastReported** attribute uses the received satellite time (not system time) to record when this last appeared in NMEA sentences. If this is old (5 min or more) then the satellite hasn't been reported and can be assumed to be out of contact. 

## Static lib
A static **nmea.lib** is built which contains all the necessary objects to run the NMEA parser. 
The parser is modular with classes existing for talkers and sentences. In brief, talkers are your data sources for sentences. Sentences contain structured data such as position or velocity.
To make the parser work then create an instance of a talker and instances of all sentences required. Then associate the sentences to the talker and attach the talker to the parser.
This approach means you can simplify the parser to just extract information you are interested in and ignore the rest and minimise memory used. 
For instance you can create a GPS talker and only create a GLL sentence if you just need the UTC time and position data. 

The following provides an overview of functions and architecture you need to know about. Not all details are covered.

### nmeaInit
This must be called first to setup the NMEA structure. This is referred to a the root although you can also view this as the main parser object.

### nmeaNewTalkerInstance
Provide a talker class to this function to receive an instance of the talker. All talker class names can be found in the talker class header file. For instance, GPS is NMEAGPSClass.
This may fail if not enough memory is available, so check the returned value for NULL. 
All instances must be freed with a call to **nmeaFreeTalker**.

### nmeaNewSentenceInstance
Provide a sentence class to create an instance of the sentence. Sentences must be created for each talker. You cannot reuse a sentence with another talker.
Returns NULL if memory isn't available.
All instances must be freed with a call to **nmeaFreeSentence**.

### nmeaAddTalker
Add a talker to the root NMEA object structure. Check the return value is **ok**.
A talker can be added at any time and doesn't need to have sentences assigned before adding. 

### nmeaAddSentence
Add a sentence to a talker. The parser reads NMEA data to first identify the talker and then reads the sentence ID characters. If the talker has a sentence object attached then the remaining data will be processed and saved against this sentence.
Check the return value for an **ok** response. 

### nmeaParser
Call this after setting up all talkers and sentences. The parser uses the root node object to remember the state and therefore partial string data can be fed to this function in a continuous loop.
The parser tries to be resilient enough to ignore badly formatted sentences and data without stopping. This may happen if serial data becomes corrupted or stop/starts. 
Only major faults such as memory errors will stop the parser. 
Checksums are validated if they exist in the NMEA string data and sentences rejected if the checksum doesn't match. 
Sentences are buffered into memory, up to 82 bytes long (as specified in NMEA standard). The sentences are then sent to the sentence object for parsing or ignored if no sentence object is attached to the talker.

### Getting data from root parser
The root NMEA object (struct NMEA) contains a **data** attribute that contains typical NMEA data collected from various sentences. 
If you do not use all sentence classes for your talker, then only some of this data will be valid. 
Due to the use of non floating point data, the altitude data is truncated to centimeters. Only kilometers per hour are provided instead of knots and kph. 
The root data is a simplification, with the raw data being saved against the sentence object. 
Data constantly changes with each sentence read from a NMEA data source. It's recommended to think about copying data that is needed at a point in time. For instance, the UTC time data will continue to update in memory which could impact time calculations.

### Sentence data access
To get closer to the raw data from the NMEA sentence, the caller can go direct to the sentence object.
Data is accessible via **sentenceobj->class.data** pointer. This pointer may be NULL if the class doesn't share this information (although all classes currently do share this data).
You must check for NULL before casting to the actual data structure used by the class. Check the class header for the correct type to cast to. 
For example, the GSV sentence contains satellite information which is not saved to the root class. Access would look like:
```
struct NMEAGSVSentenceData *gsvdata;
if (gsv->class.data){
	gsvdata = (struct NMEAGSVSentenceData*)gsv->class.data;
}
```
Data is continuously updated into memory used by the sentence object. Do not assume data remains static. 
Sentences shouldn't remove data so pointers are unlikely to become invalid, but new data could be inserted, appended or moved in lists or arrays. 

## Building
All files are written for SAS/C and VBCC compilers. 
The root directory has a standard makefile which generates the required debug and release makefiles in the directories
```
/amigaui/Release
/amigaui/Debug
```

Building Debug and Release targets gives you a nmea.lib static library, which is then linked by other applications and the nmea.library.
The *Test* application can be used to provide a file name containing NMEA strings that the parser ingests and updates the configured talkers/sentences.

### SAS/C 6.5
Simple usage from root directory to build everything:
```
smake
```

Valid build targets are:
```
smake all
smake clean
smake Release/makefile
smake Debug/makefile
```

The **all** build will invoke the build for Debug and Release targets.
The **makefile** builds will only create the makefiles in Release or Debug target directories and not invoke a build

### VBCC
You will need the standard Linux make build tool. 
Simple usage to build everything:
> make -f makefile.vbcc

Valid build targets are:
```
make -f makefile.vbcc all
make -f makefile.vbcc clean
make -f makefile.vbcc Release/makefile
make -f makefile.vbcc Debug/makefile
```
The **all** build will invoke the build for Debug and Release targets.
The **makefile** builds will only create the makefiles in Release or Debug target directories and not invoke a build