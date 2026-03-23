#ifndef __NMEA_VTG_CLASS
#define __NMEA_VTG_CLASS

#include "nmeaparser.h"

struct NMEAVTGSentenceData
{
	struct NMEADataHeader head;
	struct NMEAFloatingPoint courseOverGroundTrue;
	struct NMEAFloatingPoint courseOverGroundMagnetic;
	struct NMEAFloatingPoint speedOverGroundKnots;
	struct NMEAFloatingPoint speedOverGroundKPH;
	char faamode;
};

extern struct NMEASentenceClass NMEAVTGClass;


#endif