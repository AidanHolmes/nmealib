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

#include "nmealib.h"
#include "nmeaparser.h"
#include "ggaclass.h"
#include "gllclass.h"
#include "gpsclass.h"
#include "gsvclass.h"
#include "vtgclass.h"
#include "gsaclass.h"
#include "rmcclass.h"
#include "glonassclass.h"
#include "libtypes.h"
#include "debug.h"
#include <dos/dos.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <string.h>
#include "timing.h"

#define _STR(A) #A
#define STR(A) _STR(A)

// Some custom command (this isn't a driver so numbers don't matter)
#define CMD_CONNECT 	CMD_NONSTD + 0
#define CMD_DISCONNECT	CMD_NONSTD + 1

extern struct ExecBase *SysBase;

struct InitProcessMessage {
	struct Message _msg;
	ULONG devbase;
};

static void closeSerialDevice(struct LibDevBase *base);

struct UserNMEA{
	struct IOStdReq *std;
	struct NMEADateTime dateTimeCache;
	struct NMEAConnectParam param;
	struct NMEALocation locationCache;
	struct NMEASatellite satelliteCache;
};

__SAVE_DS__ void cmdHandler(void);

__INLINE__ static BYTE SERDRV_get_config(struct LibDevBase *base)
{
	BYTE ret = 0;
	base->serior->IOSer.io_Command = SDCMD_QUERY;
	ret = DoIO((struct IORequest *)base->serior);
	base->iorUsed = TRUE;
	
	return ret;
}

__INLINE__ static void SERDRV_read_config(struct LibDevBase *base)
{
	// Will be in serior request
	base->status_bits = base->serior->io_Status;
	base->baud = base->serior->io_Baud;
	base->drvbuffsize = base->serior->io_RBufLen;
	base->stopbits = base->serior->io_StopBits;
	base->writelen = base->serior->io_WriteLen;
	base->readlen = base->serior->io_ReadLen;
}

static BOOL openSerialDriver(struct LibDevBase *base, char *szDriver, ULONG unit)
{
	if (base->initialised){
		closeSerialDevice(base);
	}
	
	base->next_read_length = 1;
	base->initialised = FALSE;
	
	if (OpenDevice(szDriver,unit,(struct IORequest*)base->serior,0) != 0){
		D(DebugPrint(WARN_LEVEL,"openSerialDriver: Cannot open serial device %s on unit %u\n", szDriver, unit));
		goto exit;
	}

	if (SERDRV_get_config(base) != 0){
		goto exit;
	}
	
	SERDRV_read_config(base);

	D(DebugPrint(DEBUG_LEVEL,"openSerialDriver: open %s on unit %u, baud %d, status bits 0x%04X, rbuflen %d, stopbits %d, writelen %d, readlen %d\n", szDriver, unit, base->baud, base->status_bits, base->drvbuffsize, base->stopbits, base->writelen, base->readlen));
	
	base->serior->IOSer.io_Command = CMD_READ;
	base->serior->IOSer.io_Length = 1;
	base->serior->IOSer.io_Data = base->buffer ;
	SendIO((struct IORequest *)base->serior);	
	
	base->lastSerialError = 0;
	
	base->initialised = TRUE;
exit:
	if(!base->initialised){
		closeSerialDevice(base);
	}
	return base->initialised ;
}


static void closeSerialDevice(struct LibDevBase *base)
{
	// Close serial device
	if (base->serior){
		if (base->initialised){
			if (base->iorUsed){
				if (!CheckIO((struct IORequest *)base->serior)){ 
					AbortIO((struct IORequest *)base->serior);
					WaitIO((struct IORequest *)base->serior);
				}
			}
			D(DebugPrint(DEBUG_LEVEL,"closeSerialDevice: closing\n"));
			CloseDevice((struct IORequest *)base->serior);
			base->initialised = FALSE ;
		}
	}
}


static BOOL startProcess(struct LibDevBase *base)
{
	struct Library *DOSBase = NULL;
	struct InitProcessMessage initmes ;
	BOOL success = FALSE;
	
	SysBase = *(struct ExecBase **)4;
	
	initmes._msg.mn_ReplyPort = NULL ;
	base->drvProc = NULL ;
	base->drvPort = NULL ;
	base->timer = NULL ;
	base->idle_sec = 1;
	base->idle_microsec = 0;
	base->sigTerm = -1;	
	base->dosbase = DOSBase = (struct Library*)OpenLibrary("dos.library", 36);
	if (!DOSBase){
		goto end;
	}
	
	D(DebugPrint(DEBUG_LEVEL,"libdev_initalise called\n"));
	
	initmes._msg.mn_ReplyPort = CreateMsgPort();
	if (!initmes._msg.mn_ReplyPort){
		D(DebugPrint(DEBUG_LEVEL,"libdev_initalise failed to create message port\n"));
		goto end;
	}
	
	InitSemaphore(&base->sem);
	
	base->drvProc = CreateNewProcTags(NP_Entry, cmdHandler,
                                 NP_StackSize, 4096,
                                 NP_Name, STR(LIBDEVNAME),
								 NP_Priority, 0,
                                 TAG_DONE);
	if (base->drvProc == NULL){
		D(DebugPrint(DEBUG_LEVEL,"libdev_initalise failed to create work process\n"));
		goto end;
	}
		
	// Send the startup message with the library base pointer
	initmes._msg.mn_Length = sizeof(struct InitProcessMessage) - 
						sizeof (struct Message);

	initmes._msg.mn_Node.ln_Type = NT_MESSAGE;
	initmes.devbase = (ULONG)base;
	PutMsg(&base->drvProc->pr_MsgPort, (struct Message *)&initmes);
	WaitPort(initmes._msg.mn_ReplyPort);

	DeleteMsgPort(initmes._msg.mn_ReplyPort);
	initmes._msg.mn_ReplyPort = NULL;
	if (base->drvPort == NULL){ // cmdHandler allocates this
		D(DebugPrint(DEBUG_LEVEL,"libdev_initalise something went wrong with work process initialisation\n"));
		goto end;
	};

	success = TRUE ;
end:
	if (!success){
		if (initmes._msg.mn_ReplyPort){
			DeleteMsgPort(initmes._msg.mn_ReplyPort);
		}
		if (base->drvProc){
			Signal(&base->drvProc->pr_Task, 1 << base->sigTerm); 
			base->drvProc = NULL ;
		}
		if (base->dosbase){
			CloseLibrary((struct Library*)base->dosbase) ;
			base->dosbase = NULL;
		}
		
		base = NULL;
	}
	
	return success;
}

static void freeNMEA(struct LibDevBase *base)
{
	if (base->gps){
		nmeaFreeTalker(base->gps);
	}
	if (base->glonass){
		nmeaFreeTalker(base->glonass);
	}
	if (base->gga){
		nmeaFreeSentence(base->gga);
	}
	if (base->gll){
		nmeaFreeSentence(base->gll);
	}
	if (base->gsv){
		nmeaFreeSentence(base->gsv);
	}
	if (base->vtg){
		nmeaFreeSentence(base->vtg);
	}
	if (base->gsa){
		nmeaFreeSentence(base->gsa);
	}
	if (base->rmc){
		nmeaFreeSentence(base->rmc);
	}
	
	if (base->glgga){
		nmeaFreeSentence(base->glgga);
	}
	if (base->glgll){
		nmeaFreeSentence(base->glgll);
	}
	if (base->glgsv){
		nmeaFreeSentence(base->glgsv);
	}
	if (base->glvtg){
		nmeaFreeSentence(base->glvtg);
	}
	if (base->glgsa){
		nmeaFreeSentence(base->glgsa);
	}
	if (base->glrmc){
		nmeaFreeSentence(base->glrmc);
	}
}

static BOOL initNMEA(struct LibDevBase *base)
{
	BOOL bOK = FALSE ;
	
	if (nmeaInit(&base->nmea) != ok){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: failed to initilalise NMEA parser\n"));
		goto exit;
	}
	
	if (!(base->gps=nmeaNewTalkerInstance(&NMEAGPSClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GPS class\\n"));
		goto exit;
	}
	
	if (!(base->glonass=nmeaNewTalkerInstance(&NMEAGLONASSClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GLONASS class\\n"));
		goto exit;
	}
	
	if (!(base->gga=nmeaNewSentenceInstance(&NMEAGGAClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GGA class\\n"));
		goto exit;
	}

	if (!(base->gll=nmeaNewSentenceInstance(&NMEAGLLClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GLL class\\n"));
		goto exit;
	}
	
	if (!(base->gsv=nmeaNewSentenceInstance(&NMEAGSVClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GSV class\\n"));
		goto exit;
	}
	
	if (!(base->vtg=nmeaNewSentenceInstance(&NMEAVTGClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of VTG class\\n"));
		goto exit;
	}
	
	if (!(base->gsa=nmeaNewSentenceInstance(&NMEAGSAClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GSA class\\n"));
		goto exit;
	}
	
	if (!(base->rmc=nmeaNewSentenceInstance(&NMEARMCClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of RMC class\\n"));
		goto exit;
	}
	
	
	if (!(base->glgga=nmeaNewSentenceInstance(&NMEAGGAClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GGA class\\n"));
		goto exit;
	}

	if (!(base->glgll=nmeaNewSentenceInstance(&NMEAGLLClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GLL class\\n"));
		goto exit;
	}
	
	if (!(base->glgsv=nmeaNewSentenceInstance(&NMEAGSVClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GSV class\\n"));
		goto exit;
	}
	
	if (!(base->glvtg=nmeaNewSentenceInstance(&NMEAVTGClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of VTG class\\n"));
		goto exit;
	}
	
	if (!(base->glgsa=nmeaNewSentenceInstance(&NMEAGSAClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of GSA class\\n"));
		goto exit;
	}
	
	if (!(base->glrmc=nmeaNewSentenceInstance(&NMEARMCClass))){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't create instance of RMC class\\n"));
		goto exit;
	}
	
	if (nmeaAddTalker(&base->nmea, base->gps) != ok){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add talker GPS\\n"));
		goto exit;
	}
	
	if (nmeaAddTalker(&base->nmea, base->glonass) != ok){
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add talker GLONASS\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->gps, base->gga) != ok){ // $GPGGA data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GPGGA\\n"));
		goto exit;
	}

	if (nmeaAddSentence(base->gps, base->gll) != ok){ // $GPGLL data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GPGLL\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->gps, base->gsv) != ok){ // $GPGSV data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GPGSV\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->gps, base->vtg) != ok){ // $GPVTG data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GPVTG\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->gps, base->gsa) != ok){ // $GPGSA data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GPGSA\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->gps, base->rmc) != ok){ // $GPGSA data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GPGSA\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->glonass, base->glgga) != ok){ // $GLGGA data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GLGGA\\n"));
		goto exit;
	}

	if (nmeaAddSentence(base->glonass, base->glgll) != ok){ // $GLGLL data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GLGLL\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->glonass, base->glgsv) != ok){ // $GLGSV data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GLGSV\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->glonass, base->glvtg) != ok){ // $GLVTG data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GLVTG\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->glonass, base->glgsa) != ok){ // $GLGSA data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GLGSA\\n"));
		goto exit;
	}
	
	if (nmeaAddSentence(base->glonass, base->glrmc) != ok){ // $GLGSA data
		D(DebugPrint(ERROR_LEVEL,"initNMEA: Couldn't add sentence GLGSA\\n"));
		goto exit;
	}

	bOK = TRUE ;
	
exit:
	if (!bOK){
		freeNMEA(base);
	}
	
	return bOK;
}

__SAVE_DS__ void cmdHandler(void)
{
	struct LibDevBase *base ;
    struct Process *proc;
	struct IOExtSer *sio = NULL ;
	struct IOStdReq *std = NULL ;
    struct InitProcessMessage *msg;
	BOOL bTerminate = FALSE, bSetupOK = FALSE;
	ULONG sigs = 0, drivermask = 0, termmask = 0, waitmasks = 0, serialmask = 0;
	const int bufferLen = NMEA_MAX_TMP;

	D(DebugPrint(DEBUG_LEVEL,"cmdHandler startup\n"));
    proc = (struct Process *)FindTask((char *)NULL);
	
	// Wait for the startup message to synchronise and update base
    while((msg = (struct InitProcessMessage *)GetMsg(&proc->pr_MsgPort)) == NULL) {
        WaitPort(&proc->pr_MsgPort);
	}
	
	base = (struct LibDevBase*)msg->devbase;
	
	if (!(base->timer = openTimer())){
		D(DebugPrint(DEBUG_LEVEL,"cmdHandler: failed to create timer. Exiting...\n"));
		goto confirmstartup;
	}

	base->sigTerm = AllocSignal(-1);
	if (base->sigTerm < 0){
		D(DebugPrint(DEBUG_LEVEL,"cmdHandler: failed to allocate terminate signal. Exiting...\n"));
		goto confirmstartup; // reply to startup code with no devPort to kill everything
	}
	termmask = 1 << base->sigTerm ;
	
	base->buffer = AllocVec(bufferLen, MEMF_PUBLIC);
	if (!base->buffer){
		D(DebugPrint(ERROR_LEVEL,"cmdHandler: failed to allocate buffer memory\n"));
		goto confirmstartup;
	}

	if (!initNMEA(base)){
		D(DebugPrint(ERROR_LEVEL,"cmdHandler: initNMEA failed\n"));
		goto confirmstartup;
	}
	
	if (!(base->serport = CreateMsgPort())){
		D(DebugPrint(ERROR_LEVEL,"cmdHandler: Cannot create message port for serial IO\n"));
		goto confirmstartup;
	}
	
	if (!(base->serior = (struct IOExtSer *) CreateIORequest(base->serport, sizeof(struct IOExtSer)))){
		D(DebugPrint(ERROR_LEVEL,"cmdHandler: Cannot create read IO for serial\n"));
		goto confirmstartup;
	}
	serialmask = 1 << base->serport->mp_SigBit;
	
	// Setup message port to handle new messages
	if (!(base->drvPort = CreateMsgPort())){
		D(DebugPrint(DEBUG_LEVEL,"cmdHandler: creation of message port failed\n"));
		goto confirmstartup;
	}
	drivermask = 1 << base->drvPort->mp_SigBit;
	
	bSetupOK = TRUE;

confirmstartup:
	ReplyMsg((struct Message *)msg); // sync with creation process
	if (!bSetupOK){
		D(DebugPrint(DEBUG_LEVEL,"cmdHandler: failed to complete setup. Exiting...\n"));
		goto terminate;
	}

	D(DebugPrint(DEBUG_LEVEL,"cmdHandler: event loop starting...\n"));
	D(DebugPrint(DEBUG_LEVEL,"cmdHandler: drivermask 0x%04X\n", drivermask));
	D(DebugPrint(DEBUG_LEVEL,"cmdHandler: termmask   0x%04X\n", termmask));
	D(DebugPrint(DEBUG_LEVEL,"cmdHandler: serialmask 0x%04X\n", serialmask));
	waitmasks = drivermask | termmask | serialmask;
	while(!bTerminate){
		sigs = timerWaitTO(base->timer, base->idle_sec, base->idle_microsec, waitmasks);
		if (sigs & termmask){
			bTerminate = TRUE ;
			continue;
		}
		
		if (sigs & drivermask){
			while(std = (struct IOStdReq*)GetMsg(base->drvPort)){
				//D(DebugPrint(DEBUG_LEVEL,"cmdHandler: driver command %d\n", std->io_Command));
				std->io_Flags &= ~IOF_QUICK;
				std->io_Error = 0;
				switch(std->io_Command){
					case CMD_CONNECT:
						if (std->io_Length == sizeof(struct NMEAConnectParam)){
							if (!openSerialDriver(base, ((struct NMEAConnectParam*)std->io_Data)->deviceName, ((struct NMEAConnectParam*)std->io_Data)->unit)){
								std->io_Error = IOERR_OPENFAIL;
							}
						}else{
							std->io_Error = IOERR_BADLENGTH;
						}
						break;
					case CMD_DISCONNECT:
						if (base->initialised){
							closeSerialDevice(base);
						}
						break;
					default:
						std->io_Error = IOERR_NOCMD;
				}
				
				ReplyMsg(&std->io_Message);
			}
		}
		
		if (sigs & serialmask){
			//D(DebugPrint(DEBUG_LEVEL,"cmdHandler: serial command %d\n", std->io_Command));
			// Alternate with query and read requests
			while(sio = (struct IOExtSer*)GetMsg(base->serport)){
				if (sio->IOSer.io_Command == SDCMD_QUERY){
					base->lastSerialError = sio->IOSer.io_Error;
					base->next_read_length = 1;
					if (base->lastSerialError == 0){
						SERDRV_read_config(base);
						base->next_read_length = sio->IOSer.io_Actual;
						sio->IOSer.io_Command = CMD_READ;
						if (base->next_read_length > 0){
							sio->IOSer.io_Length = (base->next_read_length > bufferLen)?bufferLen:base->next_read_length;
						}else{
							base->next_read_length = 1;
							sio->IOSer.io_Length = 1;
						}
						sio->IOSer.io_Data = base->buffer ;
						SendIO((struct IORequest *)sio);
					}
				}else if (sio->IOSer.io_Command == CMD_READ){
					base->lastSerialError = sio->IOSer.io_Error;
					if(base->lastSerialError == 0){
						ObtainSemaphore(&base->sem);
						nmeaParser(&base->nmea, sio->IOSer.io_Data, sio->IOSer.io_Actual);
						ReleaseSemaphore(&base->sem);
						// Check remaining buffer and request further data
						sio->IOSer.io_Command = SDCMD_QUERY;
						SendIO((struct IORequest *)sio);
					}
				}
			}
		}
		if(sigs == 0){
			//D(DebugPrint(DEBUG_LEVEL,"cmdHandler: tick\n"));
		}
	}
	
terminate:
	D(DebugPrint(DEBUG_LEVEL,"Exiting driver command handler\n")) ;
	
	if (base->initialised){
		// serial driver open
		closeSerialDevice(base);
	}
	if (base->serior){
		DeleteIORequest((struct IORequest *)base->serior);
		base->serior = NULL ;
	}
	if (base->serport){
		DeleteMsgPort(base->serport);
		base->serport = NULL ;
	}
	
	if (base->sigTerm >= 0){
		FreeSignal(base->sigTerm) ;
		base->sigTerm = -1;
	}
	if (base->drvPort){
		DeleteMsgPort(base->drvPort);
		base->drvPort = NULL ;
	}
	if (base->timer){
		timerCloseTimer(base->timer);
		base->timer = NULL;
	}
	if (base->buffer){
		FreeVec(base->buffer);
		base->buffer = NULL;
	}
	
	freeNMEA(base);
	
	Forbid();
	base->drvProc = NULL ;
	
	return ;
}

__SAVE_DS__ struct LibDevBase* __ASM__ libdev_library_open(__REG__(a6, struct LibDevBase *)base)
{
	return base;
}

__SAVE_DS__ struct LibDevBase* __ASM__ libdev_initalise(__REG__(a6, struct LibDevBase *)base)
{	
	if (!startProcess(base)){
		return NULL;
	}

	return base;
}

__SAVE_DS__ void __ASM__ libdev_cleanup(__REG__(a6, struct LibDevBase *)base)
{
	if (base->drvProc){
		Signal((struct Task*)base->drvProc, 1 << base->sigTerm);
	}
}

///////////////////////////////////////////////////////////////
//
// Start of library functions
//
//
//

__INLINE__ BYTE sendAndWait(struct LibDevBase *base, struct IOStdReq *std)
{
	PutMsg(base->drvPort, (struct Message *)std);
	return WaitIO((struct IORequest*)std);
}

__SAVE_DS__ APTR __ASM__ AllocateHandle (__REG__(a6, struct LibDevBase *)base)
{
	BOOL completedOK = FALSE ;
	struct UserNMEA *usr = NULL;
	
	if (!(usr=AllocVec(sizeof(struct UserNMEA), MEMF_PUBLIC | MEMF_CLEAR))){
		goto end;
	}
	if (!(usr->std = AllocVec(sizeof(struct IOStdReq), MEMF_PUBLIC | MEMF_CLEAR))){
		goto end;
	}

	if (!(usr->std->io_Message.mn_ReplyPort = CreateMsgPort())){
		goto end;
	}

	usr->std->io_Message.mn_Length = sizeof(struct IOStdReq) ;
	usr->std->io_Message.mn_Node.ln_Type = NT_MESSAGE;
	usr->std->io_Command = 0;

	completedOK = TRUE ;
end:
	if (!completedOK && usr){
		if (usr->std){
			if (usr->std->io_Message.mn_ReplyPort ){
				DeleteMsgPort(usr->std->io_Message.mn_ReplyPort);
			}
			FreeVec(usr->std);
		}
		FreeVec(usr);
		usr = NULL;
	}
	return usr;
}

__SAVE_DS__ VOID __ASM__ FreeHandle (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	struct UserNMEA *usr = (struct UserNMEA*)handle;
	if (usr){
		DeleteMsgPort(usr->std->io_Message.mn_ReplyPort);
		FreeVec(usr);
	}
}

__SAVE_DS__ BOOL __ASM__ OpenSerial(__REG__(a3, APTR) handle, __REG__(a0, char*) deviceName, __REG__(d0, ULONG) unit, __REG__(a6, struct LibDevBase *)base)
{
	struct UserNMEA *usr = (struct UserNMEA*)handle;
	
	strcpy(usr->param.deviceName, deviceName);
	usr->param.unit = unit;
	usr->std->io_Command = CMD_CONNECT;
	usr->std->io_Data = (APTR)&usr->param;
	usr->std->io_Length = sizeof(struct NMEAConnectParam);
	if (sendAndWait(base, usr->std) == 0){
		return TRUE ;
	}
	return FALSE;
}

__SAVE_DS__ BOOL __ASM__ CloseSerial(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	struct UserNMEA *usr = (struct UserNMEA*)handle;
	usr->std->io_Command = CMD_DISCONNECT;
	usr->std->io_Length = 0;
	if (sendAndWait(base, usr->std) == 0){
		return TRUE ;
	}
	return FALSE;
}

// Connected to serial and gps data parsing
__SAVE_DS__ BOOL __ASM__ Connected(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	BOOL ret = base->initialised;
	
	if (ret){
		if (base->lastSerialError != 0){
			ret = FALSE;
		}

		if (base->status_bits & (1 << 10)){
			ret = FALSE ;
		}
	}
	
	return ret;
}

__SAVE_DS__ struct NMEADateTime* __ASM__ GetDateTime(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	struct UserNMEA *usr = (struct UserNMEA*)handle;
	
	ObtainSemaphore(&base->sem);
	usr->dateTimeCache.dateValid = base->nmea.data.satellites >0?TRUE:FALSE ;
	usr->dateTimeCache.day = base->nmea.data.date.day;
	usr->dateTimeCache.month = base->nmea.data.date.month;
	usr->dateTimeCache.year = base->nmea.data.date.year;
	
	usr->dateTimeCache.hours = base->nmea.data.timeUTC.hours;
	usr->dateTimeCache.minutes = base->nmea.data.timeUTC.minutes;
	usr->dateTimeCache.secs = base->nmea.data.timeUTC.secs;
	ReleaseSemaphore(&base->sem);

	return &usr->dateTimeCache;
}

__SAVE_DS__ struct NMEALocation* __ASM__ GetLocation(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	struct UserNMEA *usr = (struct UserNMEA*)handle;
	
	ObtainSemaphore(&base->sem);
	usr->locationCache.longitude.direction = base->nmea.data.longitude.direction;
	usr->locationCache.longitude.degrees = base->nmea.data.longitude.degrees;
	usr->locationCache.longitude.minutes = base->nmea.data.longitude.minutes;
	usr->locationCache.longitude.precision = base->nmea.data.longitude.precision;
	usr->locationCache.longitude.minfraction = base->nmea.data.longitude.minfraction;

	usr->locationCache.latitude.direction = base->nmea.data.latitude.direction;
	usr->locationCache.latitude.degrees = base->nmea.data.latitude.degrees;
	usr->locationCache.latitude.minutes = base->nmea.data.latitude.minutes;
	usr->locationCache.latitude.precision = base->nmea.data.latitude.precision;
	usr->locationCache.latitude.minfraction = base->nmea.data.latitude.minfraction;
	ReleaseSemaphore(&base->sem);
	
	return &usr->locationCache ;
}

void setSatellite(APTR handle, struct GSVSatelliteInfo *sat, short *gsafixes)
{
	struct UserNMEA *usr = (struct UserNMEA*)handle;
	UWORD i = 0;
	
	usr->satelliteCache.next = sat->_n.next;
	usr->satelliteCache.lastReported.hours = sat->lastReported.hours;
	usr->satelliteCache.lastReported.minutes = sat->lastReported.minutes;
	usr->satelliteCache.lastReported.secs = sat->lastReported.secs;

	usr->satelliteCache.id = sat->id;
	usr->satelliteCache.elevationValid = sat->elevationValid;
	usr->satelliteCache.elevation = sat->elevation;
	usr->satelliteCache.azimuthValid = sat->azimuthValid;
	usr->satelliteCache.azimuth = sat->azimuth;
	usr->satelliteCache.signalToNoiseRatio = sat->signalToNoiseRatio;
	usr->satelliteCache.usedForFix = FALSE ;
	for (;i<NMEA_GSA_SATELLITE_CHANNELS;i++){
		if (gsafixes[i] == sat->id){
			usr->satelliteCache.usedForFix = TRUE ;
			break;
		}
	}
}	

__SAVE_DS__ struct NMEASatellite* __ASM__ GetFirstSatellite(__REG__(a3, APTR) handle, __REG__(d0, UWORD) type, __REG__(a6, struct LibDevBase *)base)
{
	struct NMEASentence *gsv = NULL, *gsa = NULL;
	struct NMEAGSASentenceData *gsadata = NULL;
	struct NMEAGSVSentenceData *gsvdata = NULL;
	struct UserNMEA *usr = (struct UserNMEA*)handle;
	
	if (type == NMEA_TALKER_GLONASS){
		gsv = base->glgsv;
		gsa = base->glgsa;
	}else{
		gsv = base->gsv;
		gsa = base->gsa;
	}
	gsadata = (struct NMEAGSASentenceData*)gsa->class.data;
	gsvdata = (struct NMEAGSVSentenceData*)gsv->class.data;
	if (!gsadata || !gsvdata){
		return NULL;
	}
	
	if (!gsvdata->satellites){
		return NULL;
	}
	
	setSatellite(handle, gsvdata->satellites, gsadata->satelliteFixes);
	return &usr->satelliteCache;
}

__SAVE_DS__ struct NMEASatellite* __ASM__ GetNextSatellite(__REG__(a3, APTR) handle, __REG__(d0, UWORD) type, __REG__(a6, struct LibDevBase *)base)
{
	struct NMEASentence *gsa = NULL;
	struct NMEAGSASentenceData *gsadata = NULL;
	struct UserNMEA *usr = (struct UserNMEA*)handle;
	
	if (!usr->satelliteCache.next){
		return NULL;
	}
	
	if (type == NMEA_TALKER_GLONASS){
		gsa = base->glgsa;
	}else{
		gsa = base->gsa;
	}
	gsadata = (struct NMEAGSASentenceData*)gsa->class.data;
	if (!gsadata){
		return NULL;
	}
	setSatellite(handle, usr->satelliteCache.next, gsadata->satelliteFixes);
	return &usr->satelliteCache;
}

