#include "gsvclass.h"
#include "utils.h"
#include <stdlib.h>
#include <stddef.h>

#ifndef ALLOCMEM
#define ALLOCMEM(x) (malloc(x))
#endif

#ifndef FREEMEM
#define FREEMEM(x) free(x);x=NULL
#endif

enum gsv_state {totalgroup,sentenceno,totalsats,satelliteid,elevation,azimuth,nsr};

struct NMEAGSVSentence
{
	struct NMEASentence base;
	struct NMEAGSVSentenceData data;
};

void GSVInit(struct NMEASentence *s);
void GSVFree(struct NMEASentence *s);
enum nmea_return GSVParser(struct NMEASentence *s, char *sentencetxt, unsigned long length) ;

struct NMEASentenceClass NMEAGSVClass = 
	{0, 0, {'G','S','V'}, 0, GSVParser, GSVInit, GSVFree, sizeof(struct NMEAGSVSentence)};

void GSVInit(struct NMEASentence *s)
{
	short i = 0;
	struct NMEAGSVSentence *gsv = (struct NMEAGSVSentence*)s;
	
	memoryclear(&gsv->data, sizeof(struct NMEAGSVSentenceData));
	gsv->data.head.size = sizeof(struct NMEAGSVSentenceData);
	for (;i < NMEA_SENTENCE_ID_LEN; i++){
		gsv->data.head.id[i] = gsv->base.class.sentenceID[i];
	}
	gsv->base.class.data = (struct NMEADataHeader*)&gsv->data; // Provide access to the data section
}

void GSVFree(struct NMEASentence *s)
{
	struct GSVSatelliteInfo *sat = NULL, *tmpnext = NULL;
	struct NMEAGSVSentence *gsv = (struct NMEAGSVSentence*)s;
	if (gsv->data.satellites){
		sat = gsv->data.satellites;
		while (sat){
			tmpnext = (struct GSVSatelliteInfo*)sat->_n.next;
			FREEMEM(sat);
			sat = tmpnext;
		}
		gsv->data.satellites = NULL;
	}
}

static struct GSVSatelliteInfo* findSatellite(struct NMEAGSVSentenceData *dat, unsigned short id)
{
	struct GSVSatelliteInfo *sat = NULL;
	
	if (dat->satellites){
		for (sat=dat->satellites; sat; sat=(struct GSVSatelliteInfo*)sat->_n.next){
			if (sat->id == id){
				break;
			}
		}
	}
	return sat;
}

static struct GSVSatelliteInfo* addNewSatellite(struct NMEAGSVSentenceData *dat, unsigned short id)
{
	struct GSVSatelliteInfo *sat = NULL;
	if (!(sat=ALLOCMEM(sizeof(struct GSVSatelliteInfo)))){
		return NULL;
	}
	memoryclear(sat, sizeof(struct GSVSatelliteInfo));
	
	if (!dat->satellites){
		dat->satellites = sat;
		sat->_n.prev = NULL;
	}else{
		if (nmeaInsertNode((struct NMEANode*)dat->satellites, (struct NMEANode*)sat, last) != ok){
			FREEMEM(sat);
			return NULL;
		}
	}
	
	sat->_n.next = NULL;
	sat->id = id;
	
	return sat;
}

enum nmea_return GSVParser(struct NMEASentence *s, char *sentencetxt, unsigned long length)
{
	int i = 0, starti = 0;
	struct GSVSatelliteInfo *sat = NULL;
	char c;
	unsigned long satid = 0, tmpnum =0;
	enum nmea_return ret ;
	enum gsv_state state = totalgroup;
	struct NMEAGSVSentence *gsv = (struct NMEAGSVSentence*)s;
	struct NMEAData *rootdata = &s->talker->root->data;
	
	for (i=0; i<length; i++){
		c = sentencetxt[i];
		if (c == ',' || i == (length-1)){

			ret = ok ;
			switch(state){
				case totalgroup:
				case sentenceno:
					break; // don't need to record these
				case totalsats:
					ret=nmeaParseNumber(&tmpnum, &sentencetxt[starti], i-starti);
					if (ret == ok){
						gsv->data.satellitesInView = (unsigned short)tmpnum;
					}
					break;
				case satelliteid:
					// New satellite?
					ret = nmeaParseNumber(&satid, &sentencetxt[starti], i-starti);
					if (ret == ok){
						if (!(sat=findSatellite(&gsv->data, (unsigned short)satid))){
							// Create and add a new satellite record
							if (!(sat = addNewSatellite(&gsv->data, (unsigned short)satid))){
								return memory_error;
							}
							sat->elevationValid = 0;
							sat->azimuthValid = 0;
						}
						// Update with latest UTC time from root records (assuming we have sentences with time to prime this record)
						sat->lastReported = rootdata->timeUTC;
					}
					break;
				case elevation:
					if (sat){
						ret = nmeaParseNumberSigned((long*)&tmpnum, &sentencetxt[starti], i-starti);
						if (ret == ok){
							sat->elevation = (short)tmpnum;
							sat->elevationValid = 1;
						}
						if (ret == oknull){
							sat->elevationValid = 0;
						}
					}
					break;
				case azimuth:
					if (sat){
						ret = nmeaParseNumber(&tmpnum, &sentencetxt[starti], i-starti);
						if (ret == ok){
							sat->azimuth = (short)tmpnum;
							sat->azimuthValid = 1;
						}
						if (ret == oknull){
							sat->azimuthValid = 0;
						}
					}
					break;
				case nsr:
					if (sat){
						ret = nmeaParseNumber(&tmpnum, &sentencetxt[starti], i-starti);
						if (ret == ok){
							sat->signalToNoiseRatio = (short)tmpnum;
						}
					}
					break;
				default:
					break;
			}
			if (ret != ok && ret != oknull){
				return ret;
			}	
			
			if (state == nsr){
				sat = NULL; // reset for new satellite info
				state = satelliteid;
			}else{
				state++;
			}
			starti = i+1;
		}else if(c == ' ' || c == '\t'){
			// Ignore or error? Error
			return syntax_error;
		}
	}
			
	return ok;
}