/* Copyright 2026 Aidan Holmes

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#ifndef __H_LIBDEV_
#define __H_LIBDEV_
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/exec.h>
#include <proto/exec.h>
#include <exec/semaphores.h>
#include <exec/devices.h>
#include <devices/serial.h>
#include "nmeaparser.h"
#include "compatibility.h"

#ifndef LIBDEVNAME
#define LIBDEVNAME none.device
#endif

#ifndef LIBDEVMAJOR
#define LIBDEVMAJOR 1
#endif

#ifndef LIBDEVMINOR
#define LIBDEVMINOR 0
#endif

#ifndef LIBDEVDATE
#define LIBDEVDATE "1.1.2024"
#endif

#ifndef LIBDEV_VALIDATE_EXEC
#define LIBDEV_VALIDATE_EXEC 36
#endif

struct LibDevBase;

#include "libtypes.h"

__SAVE_DS__ APTR __ASM__ AllocateHandle (__REG__(a6, struct LibDevBase *)base);
__SAVE_DS__ VOID __ASM__ FreeHandle (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base);
__SAVE_DS__ BOOL __ASM__ OpenSerial(__REG__(a3, APTR) handle, __REG__(a0, char*) deviceName, __REG__(d0, ULONG) unit, __REG__(a6, struct LibDevBase *)base);
__SAVE_DS__ BOOL __ASM__ CloseSerial(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base);
__SAVE_DS__ BOOL __ASM__ Connected(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base);
__SAVE_DS__ struct NMEADateTime* __ASM__ GetDateTime(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base);
__SAVE_DS__ struct NMEALocation* __ASM__ GetLocation(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base);
__SAVE_DS__ struct NMEASatellite* __ASM__ GetFirstSatellite(__REG__(a3, APTR) handle, __REG__(d0, UWORD) type, __REG__(a6, struct LibDevBase *)base);
__SAVE_DS__ struct NMEASatellite* __ASM__ GetNextSatellite(__REG__(a3, APTR) handle, __REG__(d0, UWORD) type, __REG__(a6, struct LibDevBase *)base);

/* Customise base according to requirements */
struct LibDevBase
{
	struct Device device;
	APTR seg_list;
	struct ExecBase *sys_base;
	struct Process *drvProc;
	struct Library *dosbase;
	struct MsgPort *drvPort;
	struct IORequest *timer ;
	struct SignalSemaphore sem;
	ULONG idle_sec;
	ULONG idle_microsec;
	BYTE sigTerm;
	UBYTE *buffer;
	
	struct NMEATalker *gps, *glonass;
	struct NMEASentence *gga, *gll, *gsv, *vtg, *gsa, *rmc;
	struct NMEASentence *glgga, *glgll, *glgsv, *glvtg, *glgsa, *glrmc;
	
	struct IOExtSer *serior;
	struct MsgPort *serport ;
	ULONG next_read_length;
	UWORD status_bits;
	ULONG baud;
	ULONG drvbuffsize;
	UBYTE stopbits;
	UBYTE writelen;
	UBYTE readlen;
	BOOL initialised;
	BOOL iorUsed;
	BYTE lastSerialError;
	
	struct NMEA nmea;
};

#endif