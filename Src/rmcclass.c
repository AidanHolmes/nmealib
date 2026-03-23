#include "rmcclass.h"
#include "utils.h"
#include <stdlib.h>
#include <stddef.h>

enum rmc_state {utc, status, latitude, ns, longitude, ew, knots, truecourse, date, magvariation, magew, faamode, navstatus};

struct NMEARMCSentence
{
	struct NMEASentence base;
	struct NMEARMCSentenceData data;
};

void RMCInit(struct NMEASentence *s);
enum nmea_return RMCParser(struct NMEASentence *s, char *sentencetxt, unsigned long length) ;

struct NMEASentenceClass NMEARMCClass = 
	{0, 0, {'R','M','C'}, 0, RMCParser, RMCInit, 0, sizeof(struct NMEARMCSentence)};

void RMCInit(struct NMEASentence *s)
{
	short i = 0;
	struct NMEARMCSentence *rmc = (struct NMEARMCSentence*)s;
	
	memoryclear(&rmc->data, sizeof(struct NMEARMCSentenceData));
	rmc->data.head.size = sizeof(struct NMEARMCSentenceData);
	for (;i < NMEA_SENTENCE_ID_LEN; i++){
		rmc->data.head.id[i] = rmc->base.class.sentenceID[i];
	}
	rmc->base.class.data = (struct NMEADataHeader*)&rmc->data; // Provide access to the data section
}

static enum nmea_return nmeaStatus(char *c, char symbol)
{
	if (symbol == 'A' || symbol == 'a'){
		*c = 'A';
	}else if (symbol == 'V' || symbol == 'v'){
		*c = 'V';
	}else{
		return syntax_error;
	}
	
	return ok;
}

static enum nmea_return nmeaNavStatus(char *c, char symbol)
{
	*c = chartoupper(symbol);
	// A=autonomous, D=differential, E=Estimated, M=Manual input mode N=not valid, S=Simulator, V = Valid
	if (*c != 'A' && *c != 'D' && *c != 'E' && *c != 'M' && *c != 'N' && *c != 'S' && *c != 'V'){
		return syntax_error;
	}
	
	return ok;
}

enum nmea_return RMCParser(struct NMEASentence *s, char *sentencetxt, unsigned long length)
{
	int i = 0, sat=0, starti = 0;
	char c;
	unsigned long tmp;
	enum nmea_return ret ;
	enum rmc_state state = utc;
	struct NMEARMCSentence *rmc = (struct NMEARMCSentence*)s;
	struct NMEAData *rootdata = &s->talker->root->data;
	
	for (i=0; i<length; i++){
		c = sentencetxt[i];
		if (c == ',' || i == (length-1)){
			if (i > starti){
				ret = ok ;
				switch(state){
					case utc:
						ret = nmeaParseUTC(&rmc->data.timeUTC, &sentencetxt[starti], i-starti);
						if (ret == ok){
							rootdata->timeUTC = rmc->data.timeUTC;
						}
						break;
					case status:
						ret = nmeaStatus(&rmc->data.status, sentencetxt[starti]);
						if (ret == ok){
							rootdata->status = rmc->data.status;
						}
						break;
					case latitude:
						if ((ret=nmeaParsePos(&rmc->data.latitude, &sentencetxt[starti], i-starti)) == ok){
							rootdata->latitude = rmc->data.latitude;
						}
						break;
					case ns:
						if ((ret=nmeaNorthSouth(&rmc->data.latitude.direction, sentencetxt[starti])) == ok){
							rootdata->latitude.direction = rmc->data.latitude.direction;
						}
						break;
					case longitude:
						if ((ret=nmeaParsePos(&rmc->data.longitude, &sentencetxt[starti], i-starti)) == ok){
							rootdata->longitude = rmc->data.longitude;
						}
						break;
					case ew:
						if ((ret=nmeaEastWest(&rmc->data.longitude.direction, sentencetxt[starti])) == ok){
							rootdata->longitude.direction = rmc->data.longitude.direction;
						}
						break;
					case knots:
						ret = nmeaParseFloatingNumber(&rmc->data.speedOverGroundKnots, &sentencetxt[starti], i-starti);
						break;
					case truecourse:
						ret = nmeaParseFloatingNumber(&rmc->data.courseOverGroundTrue, &sentencetxt[starti], i-starti);
						break;
					case date:
						ret = nmeaParseDate(&rmc->data.date, &sentencetxt[starti], i-starti);
						if (ret == ok){
							rootdata->date = rmc->data.date;
						}
						break;
					case magvariation:
						ret = nmeaParseFloatingNumber(&rmc->data.magneticVariationDegrees, &sentencetxt[starti], i-starti);
						break;
					case magew:
						ret=nmeaEastWest(&rmc->data.magneticVariationEastWest, sentencetxt[starti]);
						break;
					case faamode:
						rmc->data.faamode = sentencetxt[starti];
						break;
					case navstatus:
						ret = nmeaNavStatus(&rmc->data.navstatus, sentencetxt[starti]);
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
			
	return ok;
}