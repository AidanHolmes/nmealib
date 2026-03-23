#include "vtgclass.h"
#include "utils.h"
#include <stdlib.h>
#include <stddef.h>

enum vtg_state {coursetrue, coursetrueind, coursemagnetic, coursemagind, speedknots, indknots, speedkph, indkph, faamode};

struct NMEAVTGSentence
{
	struct NMEASentence base;
	struct NMEAVTGSentenceData data;
};

void VTGInit(struct NMEASentence *s);
enum nmea_return VTGParser(struct NMEASentence *s, char *sentencetxt, unsigned long length) ;

struct NMEASentenceClass NMEAVTGClass = 
	{0, 0, {'V','T','G'}, 0, VTGParser, VTGInit, 0, sizeof(struct NMEAVTGSentence)};

void VTGInit(struct NMEASentence *s)
{
	short i = 0;
	struct NMEAVTGSentence *vtg = (struct NMEAVTGSentence*)s;
	
	memoryclear(&vtg->data, sizeof(struct NMEAVTGSentenceData));
	vtg->data.head.size = sizeof(struct NMEAVTGSentenceData);
	for (;i < NMEA_SENTENCE_ID_LEN; i++){
		vtg->data.head.id[i] = vtg->base.class.sentenceID[i];
	}
	vtg->base.class.data = (struct NMEADataHeader*)&vtg->data; // Provide access to the data section
}

enum nmea_return VTGParser(struct NMEASentence *s, char *sentencetxt, unsigned long length)
{
	int i = 0, starti = 0, nmeaold = 0;
	char c;
	enum nmea_return ret ;
	enum vtg_state state = coursetrue;
	struct NMEAVTGSentence *vtg = (struct NMEAVTGSentence*)s;
	struct NMEAData *rootdata = &s->talker->root->data;
	
	for (i=0; i<length; i++){
		c = sentencetxt[i];
		if (c == ',' || i == (length-1)){
			if (i > starti){
				ret = ok ;
				switch(state){
					case coursetrue:
						ret = nmeaParseFloatingNumber(&vtg->data.courseOverGroundTrue, &sentencetxt[starti], i-starti);
						if (ret == ok){
							rootdata->courseOverGroundTrue = vtg->data.courseOverGroundTrue;
						}
						break;
					case coursetrueind:
						if (sentencetxt[starti] != 'T' && sentencetxt[starti] != 't'){
							nmeaold = 1;
							state += 1;
						}else{
							break;
						}
						// Fall through for old encoding
					case coursemagnetic:
						ret = nmeaParseFloatingNumber(&vtg->data.courseOverGroundMagnetic, &sentencetxt[starti], i-starti);
						break;
					case coursemagind:
						if (!nmeaold){
							break;
						}else{
							state += 1;
						}
						// Fall through for old encoding
					case speedknots:
						ret = nmeaParseFloatingNumber(&vtg->data.speedOverGroundKnots, &sentencetxt[starti], i-starti);
						break;
					case indknots:
						if (!nmeaold){
							break;
						}else{
							state += 1;
						}
						// Fall through for old encoding
					case speedkph:
						ret = nmeaParseFloatingNumber(&vtg->data.speedOverGroundKPH, &sentencetxt[starti], i-starti);
						if (ret == ok){
							rootdata->speedOverGroundKPH = vtg->data.speedOverGroundKPH;
						}
						break;
					case indkph:
						break;
					case faamode:
						vtg->data.faamode = sentencetxt[starti];
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