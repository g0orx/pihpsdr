UNAME_N := $(shell uname -n)
CC=gcc
LINK=gcc

#required for LimeSDR (uncomment line below)
#LIMESDR_INCLUDE=LIMESDR

ifeq ($(LIMESDR_INCLUDE),LIMESDR)
LIMESDR_OPTIONS=-D LIMESDR
SOAPYSDRLIBS=-lSoapySDR -lpulse-simple -lpulse
LIMESDR_SOURCES= \
audio.c \
lime_discovery.c \
lime_protocol.c
LIMESDR_HEADERS= \
audio.h \
lime_discovery.h \
lime_protocol.h
LIMESDR_OBJS= \
audio.o \
lime_discovery.o \
lime_protocol.o
endif


#required for FREEDV (uncomment lines below)
#FREEDV_INCLUDE=FREEDV

ifeq ($(FREEDV_INCLUDE),FREEDV)
FREEDV_OPTIONS=-D FREEDV
FREEDVLIBS=-lcodec2
FREEDV_SOURCES= \
freedv.c
FREEDV_HEADERS= \
freedv.h
FREEDV_OBJS= \
freedv.o
endif

OPTIONS=-g -D $(UNAME_N) $(LIMESDR_OPTIONS) $(FREEDV_OPTIONS) -O3
GTKINCLUDES=`pkg-config --cflags gtk+-3.0`
GTKLIBS=`pkg-config --libs gtk+-3.0`


ifeq ($(UNAME_N),raspberrypi)
GPIOLIBS=-lwiringPi -lpigpio
endif
ifeq ($(UNAME_N),odroid)
GPIOLIBS=-lwiringPi
endif
LIBS=-lrt -lm -lwdsp -lpthread -lpulse-simple -lpulse $(GTKLIBS) $(GPIOLIBS) $(SOAPYSDRLIBS) $(FREEDVLIBS)
INCLUDES=$(GTKINCLUDES)

COMPILE=$(CC) $(OPTIONS) $(INCLUDES)

PROGRAM=pihpsdr

SOURCES= \
audio.c\
band.c \
frequency.c \
discovered.c \
filter.c \
main.c \
menu.c \
meter.c \
mode.c \
old_discovery.c \
new_discovery.c \
old_protocol.c \
new_protocol.c \
new_protocol_programmer.c \
panadapter.c \
property.c \
radio.c \
gpio.c \
splash.c \
toolbar.c \
sliders.c \
version.c \
vfo.c \
waterfall.c \
wdsp_init.c


HEADERS= \
agc.h \
alex.h \
audio.h \
band.h \
frequency.h \
bandstack.h \
channel.h \
discovered.h \
filter.h \
menu.h \
meter.h \
mode.h \
old_discovery.h \
new_discovery.h \
old_protocol.h \
new_protocol.h \
panadapter.h \
property.h \
radio.h \
gpio.h \
splash.h \
toolbar.h \
sliders.h \
version.h \
vfo.h \
waterfall.h \
wdsp_init.h \
xvtr.h


OBJS= \
audio.o \
band.o \
frequency.o \
discovered.o \
filter.o \
version.o \
main.o \
menu.o \
meter.o \
mode.o \
old_discovery.o \
new_discovery.o \
old_protocol.o \
new_protocol.o \
new_protocol_programmer.o \
panadapter.o \
property.o \
radio.o \
gpio.o \
splash.o \
toolbar.o \
sliders.o \
vfo.o \
waterfall.o \
wdsp_init.o

all: prebuild $(PROGRAM) $(HEADERS) $(LIMESDR_HEADERS) $(SOURCES) $(LIMESDR_SOURCES) $(FREEDV_SOURCES)

prebuild:
	rm -f version.o

$(PROGRAM): $(OBJS) $(LIMESDR_OBJS) $(FREEDV_OBJS)
	$(LINK) -o $(PROGRAM) $(OBJS) $(LIMESDR_OBJS) $(FREEDV_OBJS) $(LIBS)

.c.o:
	$(COMPILE) -c -o $@ $<


clean:
	-rm -f *.o
	-rm -f $(PROGRAM)

install:
	cp pihpsdr ../pihpsdr
	cp pihpsdr ./release/pihpsdr
	cd release; tar cvf pihpsdr.tar pihpsdr

