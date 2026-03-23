#ifndef __NMEA_GLONASS_CLASS
#define __NMEA_GLONASS_CLASS

#include "nmeaparser.h"

struct NMEAGLONASSTalker
{
	struct NMEATalker base;
};

extern struct NMEATalkerClass NMEAGLONASSClass;


#endif