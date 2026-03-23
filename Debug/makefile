#   Copyright 2026 Aidan Holmes
#   NMEA parsing library
# 	Makefile for nmea.lib and test application. This uses SAS/C smake to build
SRC = /Src
BIN = Bin
OBJ = Objs

BUILD = Debug
AMINETNAME = NMEALib
DEVICENAME = nmea
LIBNAME = nmea.lib

LIBDEVNAME = $(DEVICENAME).library

EXTRA_H = IncludeDirectory=/$(AMINETNAME)/Include IncludeDirectory=/$(AMINETNAME)/Include/C
DEFSTD = DEFINE=__NOLIBBASE__ DEFINE=AS_LIBRARY DEFINE=LIBDEVMAJOR=$(LIBDEVMAJOR) DEFINE=LIBDEVMINOR=$(LIBDEVMINOR) DEFINE=LIBDEVDATE=$(LIBDEVDATE) DEFINE=LIBDEVNAME=$(LIBDEVNAME)

OBJS= $(OBJ)/utils.o $(OBJ)/ggaclass.o $(OBJ)/gpsclass.o $(OBJ)/glonassclass.o $(OBJ)/nmeaparser.o $(OBJ)/gllclass.o $(OBJ)/gsvclass.o $(OBJ)/vtgclass.o $(OBJ)/gsaclass.o $(OBJ)/rmcclass.o $(OBJ)/debug.o
LIBOBJS = $(OBJ)/timing.o $(OBJ)/libdev.o $(OBJ)/nmealib.o
OBJSTest = $(OBJ)/main.o
OBJSTestLib = $(OBJ)/testlib.o $(OBJ)/timing.o  $(OBJ)/debug.o

# Enable for non-debug and optimised code
SCOPTS = nocheckabort NOSTACKCHECK DEFINE=_DEBUG DEFINE=DEBUG_SERIAL debug=full IncludeDirectory=//NDK3.2R4/Include_H
all: dirs $(BIN)/$(LIBNAME) $(BIN)/$(LIBDEVNAME) $(BIN)/Test $(BIN)/TestLib

dirs:
	- makedir $(BIN)
	- makedir $(OBJ)

clean:
	- delete $(OBJ)/\#?.o $(OBJ)/\#?.asm $(BIN)/\#?.lnk $(BIN)/\#?.info $(BIN)/\#?.map $(BIN)/\#?.gst $(BIN)/$(LIBNAME) $(BIN)/$(LIBDEVNAME) $(BIN)/Test $(BIN)/TestLib

lib: $(BIN)/$(LIBDEVNAME)
	copy $(BIN)/$(LIBDEVNAME) /$(AMINETNAME)/$(LIBDEVNAME)
	copy $(SRC)/libtypes.h /$(AMINETNAME)/Include/nmea/
	
$(BIN)/$(LIBDEVNAME): $(BIN)/$(LIBNAME) $(LIBOBJS)
	sc link to $@ $(LIBOBJS) library=$(BIN)/$(LIBNAME)
	
$(BIN)/Test: $(BIN)/$(LIBNAME) $(OBJSTest)
	sc link to $@ $(OBJSTest) library=$(BIN)/$(LIBNAME)
	
$(BIN)/TestLib: $(BIN)/$(LIBDEVNAME) $(OBJSTestLib)
	sc link to $@ $(OBJSTestLib)

$(BIN)/$(LIBNAME): $(OBJS)
	oml $(BIN)/$(LIBNAME) $(OBJS)
	
.c.o:
	sc $(SCOPTS) $(DEFSTD) $(EXTRA_H) $? ObjectName=$(OBJ)/

$(OBJ)/main.o: $(SRC)/main.c 
$(OBJ)/utils.o: $(SRC)/utils.c 
$(OBJ)/ggaclass.o: $(SRC)/ggaclass.c 
$(OBJ)/gpsclass.o: $(SRC)/gpsclass.c
$(OBJ)/glonassclass.o: $(SRC)/glonassclass.c 
$(OBJ)/gllclass.o: $(SRC)/gllclass.c 
$(OBJ)/gsvclass.o: $(SRC)/gsvclass.c 
$(OBJ)/nmeaparser.o: $(SRC)/nmeaparser.c 
$(OBJ)/vtgclass.o: $(SRC)/vtgclass.c 
$(OBJ)/gsaclass.o: $(SRC)/gsaclass.c
$(OBJ)/rmcclass.o: $(SRC)/rmcclass.c
$(OBJ)/timing.o: $(SRC)/timing.c
$(OBJ)/libdev.o: $(SRC)/libdev.c
$(OBJ)/nmealib.o: $(SRC)/nmealib.c
$(OBJ)/debug.o: $(SRC)/debug.c
$(OBJ)/testlib.o: $(SRC)/testlib.c