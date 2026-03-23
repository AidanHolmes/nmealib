#include "nmeaparser.h"
#include "ggaclass.h"
#include "gllclass.h"
#include "gpsclass.h"
#include "gsvclass.h"
#include "vtgclass.h"
#include "gsaclass.h"
#include "rmcclass.h"
#include <stdio.h>

#define MAXBUFFER 20

int main(int argc, char **argv)
{
	char str[MAXBUFFER];
	struct GSVSatelliteInfo *sat = NULL;
	struct NMEAGSVSentenceData *gsvdat = NULL;
	struct NMEATalker *gps = NULL;
	struct NMEASentence *gga = NULL, *gll = NULL, *gsv = NULL, *vtg = NULL, *gsa = NULL, *rmc = NULL;
	struct NMEA nmea;
	FILE *f = NULL;
	size_t actualbytes = 0;
	enum nmea_return ret = 0;
	
	if (argc < 2){
		printf("File name required with NMEA sentences\n");
		return 5;
	}
	
	if (argc >= 2){
		if (!(f=fopen(argv[1], "r"))){
			printf("Cannot open %s\n", argv[1]);
			return 5;
		}
	}
	
	if (nmeaInit(&nmea) != ok){
		printf("Couldn't initialise NMEA\n");
		return 5;
	}
	
	if (!(gps=nmeaNewTalkerInstance(&NMEAGPSClass))){
		printf("Couldn't create instance of GPS class\n");
		goto exit;
	}
	
	if (!(gga=nmeaNewSentenceInstance(&NMEAGGAClass))){
		printf("Couldn't create instance of GGA class\n");
		goto exit;
	}

	if (!(gll=nmeaNewSentenceInstance(&NMEAGLLClass))){
		printf("Couldn't create instance of GGA class\n");
		goto exit;
	}
	
	if (!(gsv=nmeaNewSentenceInstance(&NMEAGSVClass))){
		printf("Couldn't create instance of GSV class\n");
		goto exit;
	}
	
	if (!(vtg=nmeaNewSentenceInstance(&NMEAVTGClass))){
		printf("Couldn't create instance of VTG class\n");
		goto exit;
	}
	
	if (!(gsa=nmeaNewSentenceInstance(&NMEAGSAClass))){
		printf("Couldn't create instance of GSA class\n");
		goto exit;
	}
	
	if (!(rmc=nmeaNewSentenceInstance(&NMEARMCClass))){
		printf("Couldn't create instance of RMC class\n");
		goto exit;
	}
	
	if (nmeaAddTalker(&nmea, gps) != ok){
		goto exit;
	}
	
	if (nmeaAddSentence(gps, gga) != ok){ // $GPGGA data
		goto exit;
	}

	if (nmeaAddSentence(gps, gll) != ok){ // $GPGLL data
		goto exit;
	}
	
	if (nmeaAddSentence(gps, gsv) != ok){ // $GPGSV data
		goto exit;
	}
	
	if (nmeaAddSentence(gps, vtg) != ok){ // $GPVTG data
		goto exit;
	}
	
	if (nmeaAddSentence(gps, gsa) != ok){ // $GPGSA data
		goto exit;
	}
	
	if (nmeaAddSentence(gps, rmc) != ok){ // $GPGSA data
		goto exit;
	}
	
	while ((actualbytes = fread(str,1,MAXBUFFER,f)) > 0){
		ret = nmeaParser(&nmea, str, actualbytes);
		if (isNMEAStopError(ret)){
			printf("Stop error 0x%04X\n", ret);
			goto exit;
		}
		if (isNMEAWarnError(ret)){
			printf("Warning error 0x%04X\n", ret);
		}
		if (ret == okcomplete){
			printf("UTC %02d:%02d:%02d.%02d\n", nmea.data.timeUTC.hours, nmea.data.timeUTC.minutes, nmea.data.timeUTC.secs, nmea.data.timeUTC.hsecs);
			printf("Date %02d/%02d/%04d\n", nmea.data.date.day, nmea.data.date.month, nmea.data.date.year);
			printf("Status of position data - %s\n", nmea.data.status == 'A'?"Valid":"Warning");
			printf("Latitude %02d deg, %02d.%d %c\n", nmea.data.latitude.degrees, nmea.data.latitude.minutes, nmea.data.latitude.minfraction, nmea.data.latitude.direction);
			printf("Longitude %02d deg, %02d.%d %c\n", nmea.data.longitude.degrees, nmea.data.longitude.minutes, nmea.data.longitude.minfraction, nmea.data.longitude.direction);
			printf("Satellite quality %d, number %d\n", nmea.data.satquality, nmea.data.satellites);
			printf("Altitude %d meters\n", nmea.data.altitudeCM / 100);
			if (nmea.data.speedOverGroundKPH.precision > 0){
				printf("Speed %d.%u km/h\n", nmea.data.speedOverGroundKPH.integer, nmea.data.speedOverGroundKPH.fraction);
			}else{
				printf("Speed %d km/h\n", nmea.data.speedOverGroundKPH.integer);
			}
			if (nmea.data.courseOverGroundTrue.precision > 0){
				printf("Heading %d.%u deg\n", nmea.data.courseOverGroundTrue.integer, nmea.data.courseOverGroundTrue.fraction);
			}else{
				printf("Heading %d deg\n", nmea.data.courseOverGroundTrue.integer);
			}
			
			if ((gsvdat = (struct NMEAGSVSentenceData*)gsv->class.data)){
				printf("Satellites in view %d:\n", gsvdat->satellitesInView);
				for(sat=gsvdat->satellites; sat; sat=(struct GSVSatelliteInfo*)sat->_n.next){
					printf("    ID %u, ", sat->id);
					if (sat->elevationValid){
						printf("elevation %d, ", sat->elevation);
					}
					if (sat->azimuthValid){
						printf("azimuth %d, ", sat->azimuth);
					}
					printf("SNR %d, ", sat->signalToNoiseRatio);
					if (GSASatelliteFixed((struct NMEASentence*)gsa, sat->id)){
						printf("used for fix, ");
					}
					printf("last reported %02d:%02d:%02d.%02d\n", sat->lastReported.hours, sat->lastReported.minutes, sat->lastReported.secs, sat->lastReported.hsecs);
				}
			}
		}
		
		
	}
	
exit:

	if (gps){
		nmeaFreeTalker(gps);
	}
	if (gga){
		nmeaFreeSentence(gga);
	}
	if (gll){
		nmeaFreeSentence(gll);
	}
	if (gsv){
		nmeaFreeSentence(gsv);
	}
	if (vtg){
		nmeaFreeSentence(vtg);
	}
	return 0;
}