#ifndef __NMEA_GGA_CLASS
#define __NMEA_GGA_CLASS

#include "nmeaparser.h"

struct NMEAGGASentenceData
{
	struct NMEADataHeader head;
	struct NMEAUTC timeUTC;
	struct NMEAPosition latitude;
	struct NMEAPosition longitude;
	unsigned char satquality;
	unsigned char satellites;
	unsigned long horizonalDilutionofPrecisionCM;
	unsigned long altitudeCM;
	unsigned long geoSeparationCM;
	unsigned long dgps;
	unsigned long stationID;
};

extern struct NMEASentenceClass NMEAGGAClass;


#endif