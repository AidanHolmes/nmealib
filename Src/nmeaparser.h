#ifndef __NMEA_PARSER_H
#define __NMEA_PARSER_H

#define NMEA_SENTENCE_ID_LEN 	3
#define NMEA_TALKER_ID_LEN		2
#define NMEA_MAX_TMP			82

enum nmea_parser_state {sync,talkerid,sentenceid,proprietaryid,separator,sentence,checksumval,fault=99};
enum nmea_return {ok=0x0000, oknull = 0x0001, okcomplete = 0x0002, exists_warn = 0x4001, syntax_error=0x4002, memory_error=0x8001, internal_error=0x8002};
enum nmea_node_position {after,last};

struct NMEA;
struct NMEANode ;
struct NMEASentence;
struct NMEATalker;
struct NMEADataHeader;

struct NMEANode
{
	struct NMEANode *next;
	struct NMEANode *prev;
};

typedef enum nmea_return (*tfnNMEAParser)(struct NMEASentence *s, char *sentencetxt, unsigned long length) ;
typedef void (*tfnNMEAInit)(struct NMEASentence *s) ;
typedef void (*tfnNMEAFree)(struct NMEASentence *s) ;

struct NMEASentenceClass
{
	struct NMEANode n;
	char sentenceID[NMEA_SENTENCE_ID_LEN];
	struct NMEADataHeader *data; // optional data access, could be null
	tfnNMEAParser parser;
	tfnNMEAInit init;
	tfnNMEAFree free;
	unsigned long size;
};

struct NMEASentence
{
	struct NMEASentenceClass class;
	struct NMEATalker *talker;
};

struct NMEATalkerClass
{
	struct NMEANode n;
	char talkerID[NMEA_TALKER_ID_LEN];
	unsigned long size;
};

struct NMEATalker
{
	struct NMEATalkerClass class;
	struct NMEA *root;
	struct NMEASentence *sentenceParsers;
};

struct NMEAUTC
{
	unsigned char hours;
	unsigned char minutes;
	unsigned char secs;
	unsigned char hsecs;
};

struct NMEADate
{
	unsigned char day;
	unsigned char month;
	unsigned short year;
};


struct NMEAPosition
{
	char direction;
	unsigned char degrees;
	unsigned char minutes;
	unsigned char precision;
	unsigned short minfraction;
};

struct NMEAFloatingPoint
{
	short integer;
	short precision;
	unsigned short fraction;
};

struct NMEADataHeader
{
	unsigned short size;
	char id[NMEA_SENTENCE_ID_LEN];
};

struct NMEAData
{
	struct NMEAUTC timeUTC;
	struct NMEADate date;
	char status;
	struct NMEAPosition latitude;
	struct NMEAPosition longitude;
	unsigned char satquality;
	unsigned char satellites;
	unsigned long altitudeCM;
	struct NMEAFloatingPoint courseOverGroundTrue;
	struct NMEAFloatingPoint speedOverGroundKPH;
};

struct NMEA
{
	struct NMEATalker *dt;
	enum nmea_parser_state state;
	char currentSentence[NMEA_SENTENCE_ID_LEN];
	short currentSentencePos;
	char currentTalker[NMEA_TALKER_ID_LEN];
	short currentTalkerPos;
	struct NMEASentence *sentenceParser;
	unsigned char tmpchecksum;
	unsigned char checksum;
	unsigned char checksumsentence;
	short checksumi;
	char tmp[NMEA_MAX_TMP];
	short tmpi;
	struct NMEAData data;
};

enum nmea_return nmeaInit(struct NMEA *root);
int isNMEAStopError(enum nmea_return ret);
int isNMEAWarnError(enum nmea_return ret);
enum nmea_return nmeaParser(struct NMEA *root, char *fragment, unsigned long length);
struct NMEATalker* nmeaNewTalkerInstance(struct NMEATalkerClass *class);
void nmeaFreeTalker(struct NMEATalker *t);
struct NMEASentence* nmeaNewSentenceInstance(struct NMEASentenceClass *class);
void nmeaFreeSentence(struct NMEASentence *s);
enum nmea_return nmeaAddTalker(struct NMEA *root, struct NMEATalker *new);
enum nmea_return nmeaAddSentence(struct NMEATalker *t, struct NMEASentence *new);
enum nmea_return nmeaInsertNode(struct NMEANode *existing, struct NMEANode *new, enum nmea_node_position position);
void nmeaRemoveNode(struct NMEANode *n);

// utility for NMEA strings
enum nmea_return nmeaParseUTC(struct NMEAUTC *utc, char *str, short length);
enum nmea_return nmeaParseDate(struct NMEADate *date, char *str, short length);
enum nmea_return nmeaParsePos(struct NMEAPosition *pos, char *str, short length);
enum nmea_return nmeaNorthSouth(char *c, char symbol);
enum nmea_return nmeaEastWest(char *c, char symbol);
enum nmea_return nmeaParseMeters(unsigned long *cm, char *str, short length);
enum nmea_return nmeaParseAsMilliseconds(unsigned long *ms, char *str, short length);
enum nmea_return nmeaParseNumber(unsigned long *num, char *str, short length);
enum nmea_return nmeaParseNumberSigned(long *num, char *str, short length);
enum nmea_return nmeaParseFloatingNumber(struct NMEAFloatingPoint *fp, char *str, short length);

#endif