#include "gllclass.h"
#include "utils.h"

enum gll_state {latitude,ns,longitude,ew,utc,status,faamode};

struct NMEAGLLSentence
{
	struct NMEASentence base;
	struct NMEAGLLSentenceData data;
};

void GLLInit(struct NMEASentence *s);
enum nmea_return GLLParser(struct NMEASentence *s, char *sentencetxt, unsigned long length) ;

struct NMEASentenceClass NMEAGLLClass = 
	{0, 0, {'G','L','L'}, 0, GLLParser, GLLInit, 0, sizeof(struct NMEAGLLSentence)};

void GLLInit(struct NMEASentence *s)
{
	short i = 0;
	struct NMEAGLLSentence *gll = (struct NMEAGLLSentence*)s;
	gll->data.head.size = sizeof(struct NMEAGLLSentenceData);
	for (;i < NMEA_SENTENCE_ID_LEN; i++){
		gll->data.head.id[i] = gll->base.class.sentenceID[i];
	}
	gll->base.class.data = (struct NMEADataHeader*)&gll->data; // Provide access to the data section
}

enum nmea_return GLLParseStatus(char *actual, char val)
{
	if (val == 'A' || val == 'a'){
		*actual = 'A';
	}else if (val == 'V' || val == 'v'){
		*actual = 'V';
	}else{
		return syntax_error;
	}
	return ok;
}

enum nmea_return GLLParser(struct NMEASentence *s, char *sentencetxt, unsigned long length)
{
	int i = 0, starti = 0;
	char c;
	enum nmea_return ret ;
	enum gll_state state = latitude;
	struct NMEAGLLSentence *gll = (struct NMEAGLLSentence*)s;
	struct NMEAData *rootdata = &s->talker->root->data;
	
	for (i=0; i<length; i++){
		c = sentencetxt[i];
		if (c == ',' || i == (length-1)){
			if (i > starti){
				ret = ok ;
				switch(state){
					case latitude:
						ret=nmeaParsePos(&gll->data.latitude, &sentencetxt[starti], i-starti);
						break;
					case ns:
						ret=nmeaNorthSouth(&gll->data.latitude.direction, sentencetxt[starti]);
						break;
					case longitude:
						ret=nmeaParsePos(&gll->data.longitude, &sentencetxt[starti], i-starti);
						break;
					case ew:
						ret=nmeaEastWest(&gll->data.longitude.direction, sentencetxt[starti]);
						break;
					case utc:
						ret=nmeaParseUTC(&gll->data.timeUTC, &sentencetxt[starti], i-starti);
						break;
					case status:
						ret=GLLParseStatus(&gll->data.status, sentencetxt[starti]);
						break;
					case faamode:
						gll->data.faamode = sentencetxt[starti];
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
	
	// Only write valid data back to root
	if (gll->data.status == 'A'){
		rootdata->latitude = gll->data.latitude;
		rootdata->latitude.direction = gll->data.latitude.direction;
		rootdata->longitude = gll->data.longitude;
		rootdata->longitude.direction = gll->data.longitude.direction;
		rootdata->timeUTC = gll->data.timeUTC;
	}
			
	return ok;
}