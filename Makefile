UNAME_N := $(shell uname -n)

CC=gcc
LINK=gcc
OPTIONS=-g -D $(UNAME_N) -O3
GTKINCLUDES=`pkg-config --cflags gtk+-3.0`
GTKLIBS=`pkg-config --libs gtk+-3.0`
ifeq ($(UNAME_N),raspberrypi)
LIBS=-lwiringPi -lpigpio -lrt -lm -lwdsp -lpthread $(GTKLIBS)
else
LIBS=-lwiringPi -lrt -lm -lwdsp -lpthread $(GTKLIBS)
endif
INCLUDES=$(GTKINCLUDES)

COMPILE=$(CC) $(OPTIONS) $(INCLUDES)

PROGRAM=pihpsdr


SOURCES= \
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
version.c \
vfo.c \
waterfall.c \
wdsp_init.c

HEADERS= \
agc.h \
alex.h \
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
version.h \
vfo.h \
waterfall.h \
wdsp_init.h \
xvtr.h

OBJS= \
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
vfo.o \
waterfall.o \
wdsp_init.o

all: prebuild $(PROGRAM) $(HEADERS) $(SOURCES)

prebuild:
	rm -f version.o

$(PROGRAM): $(OBJS)
	$(LINK) -o $(PROGRAM) $(OBJS) $(LIBS)

.c.o:
	$(COMPILE) -c -o $@ $<


clean:
	-rm -f *.o
	-rm -f $(PROGRAM)

install:
	cp pihpsdr ../pihpsdr
