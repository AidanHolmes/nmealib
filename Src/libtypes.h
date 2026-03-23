#ifndef __NMEA_LIB_TYPES_C
#define __NMEA_LIB_TYPES_C
#include <exec/types.h>

#define NMEA_TALKER_GPS 		1
#define NMEA_TALKER_GLONASS 	2

struct NMEATime
{
	UBYTE hours;
	UBYTE minutes;
	UBYTE secs;
};

struct NMEADateTime
{
	BOOL dateValid;
	UBYTE day;
	UBYTE month;
	UWORD year;
	UBYTE hours;
	UBYTE minutes;
	UBYTE secs;
};

struct NMEAConnectParam
{
	char deviceName[108];
	LONG unit;
};

struct NMEADegPos
{
	char direction;
	unsigned char degrees;
	unsigned char minutes;
	unsigned char precision;
	unsigned short minfraction;
};

struct NMEALocation
{
	struct NMEADegPos latitude;
	struct NMEADegPos longitude;
};

struct NMEASatellite
{
	void *next;
	struct NMEATime lastReported;
	unsigned short id;
	short elevationValid; // Is a value or NULL
	short elevation;
	short azimuthValid; // Is a value or NULL
	short azimuth;
	short signalToNoiseRatio;
	BOOL usedForFix;
};

#endif
