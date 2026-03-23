#include <nmea/libtypes.h>
#include <proto/nmea.h>
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <dos/dos.h>

int main(int argc, char **argv)
{
	struct Library *NMEABase = NULL ;
	APTR handle = NULL ;
	struct IORequest *tmr = NULL;
	ULONG sigmask = 0, sigs = 0, unit = 0;
	BOOL serialOpen = FALSE ;
	struct NMEADateTime *dt;
	
	if (argc == 0){
		// Run from workbench
		return 0;
	}
	
	if (argc <= 1){
		printf("Arguments: %s <serial device> <unit>\n", argv[0]);
		return 0;
	}
	
	if (argc == 3){
		unit = atoi(argv[2]);
	}
	
	if (!(NMEABase = OpenLibrary("nmea.library", 0))){
		printf("Cannot open NMEA library\n");
		return 20;
	}
	
	if (!(handle = AllocateHandle())){
		printf("Couldn't alloc handle\n") ;
		goto exit;
	}
	
	if (!(tmr = openTimer())){
		printf("Failed to open timer resource\n");
		goto exit;
	}
	
	if (!OpenSerial(handle, argv[1], unit)){
		printf("Cannot open serial device %s unit %u\n", argv[1], unit);
		goto exit;
	}
	serialOpen = TRUE ;
	
	printf("Printing information. Press CTRL-C to exit\n");
	sigmask = SIGBREAKF_CTRL_C ;
	for ( ; ; ){
		sigs = timerWaitTO(tmr, 1,0,sigmask);

		if (sigs & SIGBREAKF_CTRL_C){
			break;
		}
		if (!sigs){
			// Timer completed
			if (!Connected(handle)){
				// lost connection, retry
				if (!OpenSerial(handle, argv[1], unit)){
					printf("Cannot retry an open of serial device %s unit %u\n", argv[1], unit);
					goto exit;
				}
			}
			if ((dt = GetDateTime(handle))){
				if (dt->dateValid){
					printf("Date %02u/%02u/%04u, ", dt->day, dt->month, dt->year);
				}					
				printf("Time %02u:%02u:%02u\n", dt->hours, dt->minutes, dt->secs);
			}else{
				printf("Cannot get date and time\n");
			}
		}
	}
	
exit:

	if (serialOpen){
		CloseSerial(handle);
	}
	if (handle){
		FreeHandle(handle);
	}
	if (tmr){
		timerCloseTimer(tmr);
	}
	CloseLibrary(NMEABase);
	return 0;
}