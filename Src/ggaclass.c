#include "ggaclass.h"
#include "utils.h"

enum gga_state {utc,latitude,ns,longitude,ew,quality,sats,hzpos,alt,altunits,geosep,geounits,dgps,refid};

struct NMEAGGASentence
{
	struct NMEASentence base;
	struct NMEAGGASentenceData data;
};

void GGAInit(struct NMEASentence *s);
enum nmea_return GGAParser(struct NMEASentence *s, char *fragment, unsigned long length) ;

struct NMEASentenceClass NMEAGGAClass = 
	{0, 0, {'G','G','A'}, 0, GGAParser, GGAInit, 0, sizeof(struct NMEAGGASentence)};

void GGAInit(struct NMEASentence *s)
{
	short i = 0;
	struct NMEAGGASentence *gga = (struct NMEAGGASentence*)s;
	gga->data.head.size = sizeof(struct NMEAGGASentenceData);
	for (;i < NMEA_SENTENCE_ID_LEN; i++){
		gga->data.head.id[i] = gga->base.class.sentenceID[i];
	}
	gga->base.class.data = (struct NMEADataHeader *)&gga->data; // Provide access to the data section
}

enum nmea_return GGAParser(struct NMEASentence *s, char *sentencetxt, unsigned long length)
{
	int i = 0, starti = 0;
	char c;
	enum nmea_return ret ;
	enum gga_state state = utc;
	struct NMEAGGASentence *gga = (struct NMEAGGASentence*)s;
	struct NMEAData *rootdata = &s->talker->root->data;
	
	for (i=0; i<length; i++){
		c = sentencetxt[i];
		if (c == ','){
			if (i > starti){
				ret = ok ;
				switch(state){
					case utc:
						if ((ret=nmeaParseUTC(&gga->data.timeUTC, &sentencetxt[starti], i-starti)) == ok){
							rootdata->timeUTC = gga->data.timeUTC;
						}
						break;
					case latitude:
						if ((ret=nmeaParsePos(&gga->data.latitude, &sentencetxt[starti], i-starti)) == ok){
							rootdata->latitude = gga->data.latitude;
						}
						break;
					case ns:
						if ((ret=nmeaNorthSouth(&gga->data.latitude.direction, sentencetxt[starti])) == ok){
							rootdata->latitude.direction = gga->data.latitude.direction;
						}
						break;
					case longitude:
						if ((ret=nmeaParsePos(&gga->data.longitude, &sentencetxt[starti], i-starti)) == ok){
							rootdata->longitude = gga->data.longitude;
						}
						break;
					case ew:
						if ((ret=nmeaEastWest(&gga->data.longitude.direction, sentencetxt[starti])) == ok){
							rootdata->longitude.direction = gga->data.longitude.direction;
						}
						break;
					case quality:
						if (i-starti > 1){
							ret = syntax_error;
						}else{
							gga->data.satquality = decstrtoval(sentencetxt[starti]);
							rootdata->satquality = gga->data.satquality;
						}
						break;
					case sats:
						if (i-starti != 2){
							ret = syntax_error;
						}else{
							gga->data.satellites = (decstrtoval(sentencetxt[starti])*10) + decstrtoval(sentencetxt[starti+1]);
							rootdata->satellites = gga->data.satellites;
						}
						break;
					case hzpos:	
						ret=nmeaParseMeters(&gga->data.horizonalDilutionofPrecisionCM, &sentencetxt[starti], i-starti);
						break;
					case alt:
						if ((ret=nmeaParseMeters(&gga->data.altitudeCM, &sentencetxt[starti], i-starti)) == ok){
							rootdata->altitudeCM = gga->data.altitudeCM;
						}
						break;
					case geosep:
						ret=nmeaParseMeters(&gga->data.geoSeparationCM, &sentencetxt[starti], i-starti);
						break;
					case dgps:
						ret=nmeaParseAsMilliseconds(&gga->data.dgps, &sentencetxt[starti], i-starti);
						break;
					default:
						break;
				}
				if (ret != ok && ret != oknull){
					return ret;
				}
			}						
			
			state++;
			starti = i+1;
		}else if(c == ' ' || c == '\t'){
			// Ignore or error? Error
			return syntax_error;
		}
	}
	if (i > starti){
		//final field being differential reference station ID
		if (state == refid){
			nmeaParseNumber(&gga->data.stationID, &sentencetxt[starti], i-starti);
		}
	}
			
	return ok;
}