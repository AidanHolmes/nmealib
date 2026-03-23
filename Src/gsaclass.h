#ifndef __NMEA_GSA_CLASS
#define __NMEA_GSA_CLASS

#define NMEA_GSA_SATELLITE_CHANNELS 12

#include "nmeaparser.h"

int GSASatelliteFixed(struct NMEASentence *s, short satelliteID);

struct NMEAGSASentenceData
{
	struct NMEADataHeader head;
	char selectionMode;
	unsigned char mode;
	unsigned short totalFixes;
	short satelliteFixes[NMEA_GSA_SATELLITE_CHANNELS];
	struct NMEAFloatingPoint pdop; // Position dilution of precision
	struct NMEAFloatingPoint hdop; // Horizontal dilution of precision
	struct NMEAFloatingPoint vdop; // Vertical dilution of precision
};

extern struct NMEASentenceClass NMEAGSAClass;


#endif