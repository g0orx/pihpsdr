UNAME_N := $(shell uname -n)

CC=gcc
LINK=gcc
OPTIONS=-g -D $(UNAME_N)
GTKINCLUDES=`pkg-config --cflags gtk+-3.0`
GTKLIBS=`pkg-config --libs gtk+-3.0`
LIBS=-lwiringPi -lpigpio -lrt -lm -lwdsp -lpthread $(GTKLIBS)
INCLUDES=$(GTKINCLUDES)

COMPILE=$(CC) $(OPTIONS) $(INCLUDES)

PROGRAM=pihpsdr


SOURCES= \
band.c \
filter.c \
main.c \
meter.c \
mode.c \
new_discovery.c \
new_protocol.c \
new_protocol_programmer.c \
panadapter.c \
property.c \
radio.c \
rotary_encoder.c \
splash.c \
toolbar.c \
vfo.c \
waterfall.c

HEADERS= \
agc.h \
alex.h \
band.h \
bandstack.h \
channel.h \
discovered.h \
filter.h \
meter.h \
mode.h \
new_discovery.h \
new_protocol.h \
panadapter.h \
property.h \
radio.h \
rotary_encoder.h \
splash.h \
toolbar.h \
vfo.h \
waterfall.h \
xvtr.h

OBJS= \
band.o \
filter.o \
main.o \
meter.o \
mode.o \
new_discovery.o \
new_protocol.o \
new_protocol_programmer.o \
panadapter.o \
property.o \
radio.o \
rotary_encoder.o \
splash.o \
toolbar.o \
vfo.o \
waterfall.o

all: $(PROGRAM) $(HEADERS) $(SOURCES)

$(PROGRAM): $(OBJS)
	$(LINK) -o $(PROGRAM) $(OBJS) $(LIBS)

.c.o:
	$(COMPILE) $(OPTIONS) -c -o $@ $<


clean:
	-rm -f *.o
	-rm -f $(PROGRAM)

