#include "gsaclass.h"
#include "utils.h"
#include <stdlib.h>
#include <stddef.h>

enum gsa_state {modes, modet, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, pdop, hdop, vdop};

struct NMEAGSASentence
{
	struct NMEASentence base;
	struct NMEAGSASentenceData data;
};

void GSAInit(struct NMEASentence *s);
enum nmea_return GSAParser(struct NMEASentence *s, char *sentencetxt, unsigned long length) ;

struct NMEASentenceClass NMEAGSAClass = 
	{0, 0, {'G','S','A'}, 0, GSAParser, GSAInit, 0, sizeof(struct NMEAGSASentence)};

void GSAInit(struct NMEASentence *s)
{
	short i = 0;
	struct NMEAGSASentence *gsa = (struct NMEAGSASentence*)s;
	
	memoryclear(&gsa->data, sizeof(struct NMEAGSASentenceData));
	gsa->data.head.size = sizeof(struct NMEAGSASentenceData);
	for (;i < NMEA_SENTENCE_ID_LEN; i++){
		gsa->data.head.id[i] = gsa->base.class.sentenceID[i];
	}
	gsa->data.totalFixes = 0;
	for (i=0;i < NMEA_GSA_SATELLITE_CHANNELS; i++){
		gsa->data.satelliteFixes[i] = -1;
	}
	gsa->base.class.data = (struct NMEADataHeader*)&gsa->data; // Provide access to the data section
}

int GSASatelliteFixed(struct NMEASentence *s, short satelliteID)
{
	unsigned short i = 0;
	struct NMEAGSASentence *gsa = (struct NMEAGSASentence*)s;
	
	if (s->class.sentenceID[0] == 'G' && s->class.sentenceID[1] == 'S' && s->class.sentenceID[2] == 'A'){
		for (;i<gsa->data.totalFixes; i++){
			if (gsa->data.satelliteFixes[i] == satelliteID){
				return 1;
			}
		}
	}
	return 0;
}

static enum nmea_return nmeaModeSelect(char *c, char symbol)
{
	if (symbol == 'A' || symbol == 'a'){
		*c = 'A';
	}else if (symbol == 'M' || symbol == 'm'){
		*c = 'M';
	}else{
		return syntax_error;
	}
	
	return ok;
}

enum nmea_return GSAParser(struct NMEASentence *s, char *sentencetxt, unsigned long length)
{
	int i = 0, sat=0, starti = 0;
	char c;
	unsigned long tmp;
	enum nmea_return ret ;
	enum gsa_state state = modes;
	struct NMEAGSASentence *gsa = (struct NMEAGSASentence*)s;
	struct NMEAData *rootdata = &s->talker->root->data;
	
	gsa->data.totalFixes = 0;
	
	for (i=0; i<length; i++){
		c = sentencetxt[i];
		if (c == ',' || i == (length-1)){
			if (i > starti){
				ret = ok ;
				switch(state){
					case modes:
						ret = nmeaModeSelect(&gsa->data.selectionMode, sentencetxt[starti]);
						break;
					case modet:
						ret = nmeaParseNumber(&tmp, &sentencetxt[starti], i-starti);
						if (ret == ok){
							if (tmp < 1 || tmp > 3){
								return syntax_error;
							}else{
								gsa->data.mode = (unsigned char)tmp;
								for(sat=0; sat < NMEA_GSA_SATELLITE_CHANNELS; sat++){
									gsa->data.satelliteFixes[sat] = -1;
								}
							}
						}
						break;
					case s1: case s2: case s3: case s4: case s5: case s6: case s7: case s8: case s9: case s10: case s11: case s12:
						if (gsa->data.totalFixes < NMEA_GSA_SATELLITE_CHANNELS){
							ret = nmeaParseNumber(&tmp, &sentencetxt[starti], i-starti);
							if (ret == ok){
								gsa->data.satelliteFixes[gsa->data.totalFixes++] = (short)tmp;
							}
						}
						break;
					case pdop:
						ret = nmeaParseFloatingNumber(&gsa->data.pdop, &sentencetxt[starti], i-starti);
						break;
					case hdop:
						ret = nmeaParseFloatingNumber(&gsa->data.hdop, &sentencetxt[starti], i-starti);
						break;
					case vdop:
						ret = nmeaParseFloatingNumber(&gsa->data.vdop, &sentencetxt[starti], i-starti);
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