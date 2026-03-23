#include <proto/exec.h>
#include "nmeaparser.h"
#include "utils.h"
#include <stdlib.h>
#include <stddef.h>

#ifndef ALLOCMEM
#define ALLOCMEM(x) (malloc(x))
#endif

#ifndef FREEMEM
#define FREEMEM(x) free(x);x=NULL
#endif


enum nmea_return nmeaInit(struct NMEA *root)
{
	memoryclear(root, sizeof(struct NMEA));
	root->state = sync;
	root->data.status = 'V';
	
	return ok;
}

int isNMEAStopError(enum nmea_return ret)
{
	return (ret & 0x8000)?1:0;
}

int isNMEAWarnError(enum nmea_return ret)
{
	return (ret & 0x4000)?1:0;
}

static struct NMEASentence* nmeaGetParser(struct NMEA *root)
{
	struct NMEATalker *dti;
	struct NMEASentence *tsi;
	for(dti=root->dt; dti; dti=(struct NMEATalker*)dti->class.n.next){
		if (memorycompare(root->currentTalker, dti->class.talkerID, NMEA_TALKER_ID_LEN)){
			break;
		}
	}
	if (!dti){
		// not found
		return NULL;
	}
	for(tsi=dti->sentenceParsers; tsi; tsi = (struct NMEASentence*)tsi->class.n.next){
		if (memorycompare(root->currentSentence, tsi->class.sentenceID, NMEA_SENTENCE_ID_LEN)){
			return tsi; // found sentence parser
		}
	}
	
	return NULL ; // couldn't find the sentence parser
}

struct NMEATalker* nmeaNewTalkerInstance(struct NMEATalkerClass *class)
{
	struct NMEATalker *new ;
	
	if (!(new = (struct NMEATalker*)ALLOCMEM(class->size))){
		return NULL;
	}
	memoryclear(new, class->size);
	new->class = *class;
	return new;
}

void nmeaFreeTalker(struct NMEATalker *t)
{
	FREEMEM(t);
}

struct NMEASentence* nmeaNewSentenceInstance(struct NMEASentenceClass *class)
{
	struct NMEASentence *new ;
	
	if (!(new = (struct NMEASentence*)ALLOCMEM(class->size))){
		return NULL;
	}
	memoryclear(new, class->size);
	new->class = *class;
	if (new->class.init){
		new->class.init(new); // poor mans constructor
	}
	return new;
}

void nmeaFreeSentence(struct NMEASentence *s)
{
	if (s->class.free){
		s->class.free(s); // poor mans destructor
	}
	FREEMEM(s);
}

enum nmea_return nmeaAddTalker(struct NMEA *root, struct NMEATalker *new)
{
	struct NMEATalker *t;
	if (!root->dt){
		root->dt = new;
	}else{
		// Find end of list
		for (t=root->dt;t->class.n.next;t=(struct NMEATalker*)t->class.n.next){
			if (memorycompare(t->class.talkerID, new->class.talkerID, NMEA_TALKER_ID_LEN)){
				return exists_warn;
			}
		}
		t->class.n.next = (struct NMEANode*)new;
		new->class.n.prev = (struct NMEANode*)t;
	}
	new->root = root;
	
	return ok;
}

enum nmea_return nmeaAddSentence(struct NMEATalker *t, struct NMEASentence *new)
{
	struct NMEASentence *s;
	if (!t->sentenceParsers){
		t->sentenceParsers = new ;
	}else{
		for (s=t->sentenceParsers; s->class.n.next; s=(struct NMEASentence*)s->class.n.next){
			if (memorycompare(s->class.sentenceID, new->class.sentenceID, NMEA_SENTENCE_ID_LEN)){
				return exists_warn;
			}
		}
		s->class.n.next = (struct NMEANode*)new;
		new->class.n.prev = (struct NMEANode*)s;
	}
	new->talker = t;
	
	return ok;
}

static void nmeaSetParserState(struct NMEA *root, enum nmea_parser_state new)
{
	switch(new){
		case sync:
			root->currentSentencePos = 0;
			root->currentTalkerPos = 0;
			root->sentenceParser=NULL;
			root->tmpi = 0;
			root->tmpchecksum=0;
			root->checksumsentence = 0;
			root->checksumi =0;
			break;
		case talkerid:
			root->tmpchecksum=0;
			root->currentSentencePos = 0;
			break;
		case sentenceid:
			root->sentenceParser = NULL;
			break;
		case sentence:
			root->tmpi = 0;
			break;
		case checksumval:
			root->checksumsentence = 0;
			root->checksumi = 0;
			break;
		default:
			break;
	}
	root->state = new;
}

enum nmea_return nmeaParser(struct NMEA *root, char *fragment, unsigned long length)
{
	int i = 0, completedSentence = 0;
	char c;
	enum nmea_return ret;
	
	for (i=0; i<length; i++){
		c = fragment[i];
		if (c != '*'){
			root->tmpchecksum ^= (unsigned char)c;
		}
		switch(root->state){
			case sync:
				if (c == '$' || c == '!'){
					nmeaSetParserState(root, talkerid);
				}
				break;
			case talkerid:
				if (root->currentTalkerPos == 0 && c == 'P'){
					// resync for unknown proprietary sentences
					nmeaSetParserState(root, sync);
				}else{
					root->currentTalker[root->currentTalkerPos++] = c;
					if (root->currentTalkerPos >= NMEA_TALKER_ID_LEN){
						nmeaSetParserState(root, sentenceid);
					}
				}
				break;
			case sentenceid:
				root->currentSentence[root->currentSentencePos++] = c;
				if (root->currentSentencePos >= NMEA_SENTENCE_ID_LEN){
					if ((root->sentenceParser = nmeaGetParser(root))){
						nmeaSetParserState(root, separator);
					}else{
						nmeaSetParserState(root, sync);
					}
				}
				break;
			case separator:
				// Should whitespace be tolerated?
				if (c == ' ' || c == '\t'){
					break;
				}
				if (c == ','){
					nmeaSetParserState(root, sentence);
				}else{ // Something else that is unexpected
					nmeaSetParserState(root, sync);
				}
				break;
			case sentence:
				if (c == '*'){
					// checksum state
					root->checksum = root->tmpchecksum;
					nmeaSetParserState(root, checksumval);
				}else if (c == '\r' || c == '\n'){
					// end of sentence
					if ((ret=root->sentenceParser->class.parser(root->sentenceParser, root->tmp, root->tmpi)) != ok){
						if (isNMEAStopError(ret)){
							return ret; // only stop on non-recoverable errors
						}
					}else{
						completedSentence = 1 ;
					}
					nmeaSetParserState(root, sync); // done here
				}else{
					// Save the sentence
					if (root->tmpi >= NMEA_MAX_TMP){
						// overflow error, something wrong with syntax
						nmeaSetParserState(root, sync); // done here
					}else{
						// remember char in sentence
						root->tmp[root->tmpi++] = c;
					}
				}
				break;
			case checksumval:
				root->checksumsentence = (root->checksumsentence << 4) | hexstrtoval(c);
				if (++root->checksumi >= 2){
					if (root->checksum == root->checksumsentence){
						if ((ret=root->sentenceParser->class.parser(root->sentenceParser, root->tmp, root->tmpi)) != ok){
							if (isNMEAStopError(ret)){
								return ret; // only stop on non-recoverable errors
							}
						}else{
							completedSentence = 1 ;
						}
					}
					nmeaSetParserState(root, sync); // done here
				}	
				break;
			default: // Shouldn't happen
				return fault;
				break;
		}
	}
	
	if (completedSentence > 0){
		return okcomplete;
	}
	return ok;
}

enum nmea_return nmeaInsertNode(struct NMEANode *existing, struct NMEANode *new, enum nmea_node_position position)
{
	struct NMEANode *n=existing;
	if (position == last){
		for (n=existing; n->next; n=n->next);
		position = after;
	}
	if (position == after){
		if (n->next){
			n->next->prev = new;
		}
		new->next = n->next; // could be null
		n->next = new;
		new->prev = n;

		return ok;
	}
	
	// Unknown position
	return internal_error;
}

void nmeaRemoveNode(struct NMEANode *n)
{
	if (n->prev){
		n->prev->next = n->next;
		n->prev = NULL;
	}
	if (n->next){
		n->next->prev = n->prev;
		n->prev = NULL;
	}
}

enum nmea_return nmeaParseUTC(struct NMEAUTC *utc, char *str, short length)
{
	if (!utc){
		return internal_error;
	}
	
	if (length == 0){
		return oknull;
	}
	
	if (length != 9){
		return syntax_error;
	}
	
	utc->hours = ((unsigned char)decstrtoval(str[0])*10) + (unsigned char)decstrtoval(str[1]);
	utc->minutes = ((unsigned char)decstrtoval(str[2])*10) + (unsigned char)decstrtoval(str[3]);
	utc->secs = ((unsigned char)decstrtoval(str[4])*10) + (unsigned char)decstrtoval(str[5]);
	utc->hsecs = ((unsigned char)decstrtoval(str[7])*10) + (unsigned char)decstrtoval(str[8]);
	
	return ok;
}

enum nmea_return nmeaParseDate(struct NMEADate *date, char *str, short length)
{
	if (!date){
		return internal_error;
	}
	
	if (length == 0){
		return oknull;
	}
	
	if (length != 6){
		return syntax_error;
	}
	
	date->day = ((unsigned char)decstrtoval(str[0])*10) + (unsigned char)decstrtoval(str[1]);
	date->month = ((unsigned char)decstrtoval(str[2])*10) + (unsigned char)decstrtoval(str[3]);
	date->year = ((unsigned char)decstrtoval(str[4])*10) + (unsigned char)decstrtoval(str[5]) + 2000;
	if (date->year <=20){
		date->year += 100;
	}

	return ok;
}

enum nmea_return nmeaParsePos(struct NMEAPosition *pos, char *str, short length)
{
	int i = 0;
	
	if (!pos){
		return internal_error;
	}
	
	if (length == 0){
		return oknull;
	}
	
	if (length < 7){
		return syntax_error;
	}
	if (str[4] != '.' && str[5] != '.'){
		return syntax_error;
	}
	
	if (str[4] == '.'){
		pos->degrees = ((unsigned char)decstrtoval(str[0])*10) + (unsigned char)decstrtoval(str[1]);
		pos->minutes = ((unsigned char)decstrtoval(str[2])*10) + (unsigned char)decstrtoval(str[3]);
		i = 5;
	}else{
		pos->degrees = ((unsigned char)decstrtoval(str[0])*100) + ((unsigned char)decstrtoval(str[1])*10) + (unsigned char)decstrtoval(str[2]);
		pos->minutes = ((unsigned char)decstrtoval(str[3])*10) + (unsigned char)decstrtoval(str[4]);
		i=6;
	}
	
	pos->precision = length - i;
	
	pos->minfraction = 0;
	for (; i < length; i++){
		pos->minfraction = (pos->minfraction*10) + decstrtoval(str[i]);
	}
	
	return ok;
}

enum nmea_return nmeaNorthSouth(char *c, char symbol)
{
	if (symbol == 'N' || symbol == 'n'){
		*c = 'N';
	}else if (symbol == 'S' || symbol == 's'){
		*c = 'S';
	}else{
		return syntax_error;
	}
	
	return ok;
}

enum nmea_return nmeaEastWest(char *c, char symbol)
{
	if (symbol == 'E' || symbol == 'e'){
		*c = 'E';
	}else if (symbol == 'W' || symbol == 'w'){
		*c = 'W';
	}else{
		return syntax_error;
	}
	
	return ok;
}

enum nmea_return nmeaParseMeters(unsigned long *cm, char *str, short length)
{
	unsigned short i = 0;
	short precision = -1;
	
	if (!cm){
		return internal_error;
	}
	
	if (length == 0){
		return oknull;
	}
	
	*cm = 0;
	for (i = 0; i < length; i++){
		if (str[i] != '.'){
			*cm = (*cm*10)+decstrtoval(str[i]);
			if (precision >= 0){
				precision++;
			}
		}else{
			precision = 0;
		}
		if (precision == 2){
			break; // cannot be more accurate
		}
	}
	if (precision < 0){
		*cm *= 100;
	}
	if (precision == 1){
		*cm *= 10;
	}

	return ok;
}

enum nmea_return nmeaParseAsMilliseconds(unsigned long *ms, char *str, short length)
{
	unsigned short i = 0;
	short precision = -1;
	
	if (length == 0){
		return oknull;
	}
	
	*ms = 0;
	for (i = 0; i < length; i++){
		if (str[i] != '.'){
			*ms = (*ms*10) + decstrtoval(str[i]);
			if (precision >= 0){
				precision++;
			}
		}else{
			precision = 0;
		}
		if (precision == 3){
			break; // cannot be more accurate
		}
	}
	if (precision < 0){
		precision = 0;
	}
	for (i=0; i < (3-precision) ; i++){
		*ms *= 10;
	}

	return ok;
}

enum nmea_return nmeaParseFloatingNumber(struct NMEAFloatingPoint *fp, char *str, short length)
{
	unsigned short i = 0;
	short multi = 1;
	
	if (!fp){
		return internal_error;
	}
	
	fp->integer = 0;
	fp->precision = -1;
	fp->fraction = 0;
	
	if (length == 0){
		return oknull;
	}
	
	if (str[i] == '-'){
		multi = -1;
		i=1;
	}
	for (; i < length; i++){
		if (str[i] != '.'){
			if (fp->precision >= 0){
				fp->precision++;
				fp->fraction = (fp->fraction*10) + decstrtoval(str[i]);
			}else{
				fp->integer = (fp->integer*10) + decstrtoval(str[i]);
			}
		}else{
			fp->precision = 0;
		}
	}
	if (fp->precision < 0){
		fp->precision = 0;
	}
	
	fp->integer *= multi;

	return ok;
}

enum nmea_return nmeaParseNumber(unsigned long *num, char *str, short length)
{
	int i = 0;
	*num = 0;
	if (length == 0){
		return oknull;
	}
	for (; i < length; i++){
		if (str[i] < '0' || str[i] > '9'){
			return syntax_error;
		}
		*num = (*num * 10) + decstrtoval(str[i]);
	}
	
	return ok;
}

enum nmea_return nmeaParseNumberSigned(long *num, char *str, short length)
{
	int i = 0, multi = 1;
	*num = 0;
	if (length == 0){
		return oknull;
	}
	if (str[0] == '-'){
		multi = -1;
		i = 1;
	}
	for (; i < length; i++){
		if (str[i] < '0' || str[i] > '9'){
			return syntax_error;
		}
		*num = (*num * 10) + decstrtoval(str[i]);
	}
	
	*num *= multi;
	
	return ok;
}