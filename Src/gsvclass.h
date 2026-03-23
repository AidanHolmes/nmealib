#ifndef __NMEA_GSV_CLASS
#define __NMEA_GSV_CLASS

#include "nmeaparser.h"

struct GSVSatelliteInfo
{
	struct NMEANode _n;
	struct NMEAUTC lastReported;
	unsigned short id;
	short elevationValid; // Is a value or NULL
	short elevation;
	short azimuthValid; // Is a value or NULL
	short azimuth;
	short signalToNoiseRatio;
};

struct NMEAGSVSentenceData
{
	struct NMEADataHeader head;
	unsigned short satellitesInView;
	struct GSVSatelliteInfo *satellites;
};

extern struct NMEASentenceClass NMEAGSVClass;


#endif