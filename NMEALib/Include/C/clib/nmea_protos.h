/* Automatically generated header! Do not edit! */

#ifndef CLIB_NMEA_PROTOS_H
#define CLIB_NMEA_PROTOS_H


/*
**	$VER: nmea_protos.h 1.0 (23.03.2026)
**
**	C prototypes. For use with 32 bit integers only.
**
**	Copyright (C) 2026 Aidan Holmes
**	All Rights Reserved
*/

#ifndef  EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef  NMEA_LIBTYPES_H
#include <nmea/libtypes.h>
#endif

APTR AllocateHandle(void);
VOID FreeHandle(APTR handle);
BOOL OpenSerial(APTR handle, char * deviceName, ULONG unit);
BOOL CloseSerial(APTR handle);
BOOL Connected(APTR handle);
struct NMEADateTime* GetDateTime(APTR handle);
struct NMEALocation* GetLocation(APTR handle);
struct NMEASatellite* GetFirstSatellite(APTR handle, ULONG type);
struct NMEASatellite* GetNextSatellite(APTR handle, ULONG type);

#endif	/*  CLIB_NMEA_PROTOS_H  */
