#ifndef __NMEA_GLL_CLASS
#define __NMEA_GLL_CLASS

#include "nmeaparser.h"

struct NMEAGLLSentenceData
{
	struct NMEADataHeader head;
	struct NMEAUTC timeUTC;
	struct NMEAPosition latitude;
	struct NMEAPosition longitude;
	char status;
	char faamode;
};

extern struct NMEASentenceClass NMEAGLLClass;


#endif