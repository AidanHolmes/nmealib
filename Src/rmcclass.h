#ifndef __NMEA_RMC_CLASS
#define __NMEA_RMC_CLASS

#include "nmeaparser.h"

int RMCSatelliteFixed(struct NMEASentence *s, short satelliteID);

struct NMEARMCSentenceData
{
	struct NMEADataHeader head;
	struct NMEAUTC timeUTC;
	char status;
	struct NMEAPosition latitude;
	struct NMEAPosition longitude;
	struct NMEAFloatingPoint speedOverGroundKnots;
	struct NMEAFloatingPoint courseOverGroundTrue;
	struct NMEADate date;
	struct NMEAFloatingPoint magneticVariationDegrees;
	char magneticVariationEastWest;
	char faamode;
	char navstatus;
};

extern struct NMEASentenceClass NMEARMCClass;


#endif