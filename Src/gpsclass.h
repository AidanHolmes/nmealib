#ifndef __NMEA_GPS_CLASS
#define __NMEA_GPS_CLASS

#include "nmeaparser.h"

struct NMEAGPSTalker
{
	struct NMEATalker base;
};

extern struct NMEATalkerClass NMEAGPSClass;


#endif