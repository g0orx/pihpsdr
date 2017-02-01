# Get git commit version and date
#GIT_VERSION := $(shell git --no-pager describe --tags --always --dirty)
GIT_DATE := $(firstword $(shell git --no-pager show --date=short --format="%ai" --name-only))
GIT_VERSION := $(shell git describe --abbrev=0 --tags)

# uncomment the line below to include support for psk31
PSK_INCLUDE=PSK

# uncomment the line to below include support for FreeDV codec2
#FREEDV_INCLUDE=FREEDV

# uncomment the line to below include support for sx1509 i2c expander
#SX1509_INCLUDE=sx1509

# uncomment the line to below include support local CW keyer
#LOCALCW_INCLUDE=LOCALCW

#uncomment the line below for the platform being compiled on
UNAME_N=raspberrypi
#UNAME_N=odroid
#UNAME_N=up
#UNAME_N=pine64
#UNAME_N=x86

CC=gcc
LINK=gcc

# uncomment the line below for various debug facilities
#DEBUG_OPTION=-D DEBUG

# uncomment the line below for LimeSDR (uncomment line below)
#LIMESDR_INCLUDE=LIMESDR

# uncomment the line below when Radioberry radio cape is plugged in
RADIOBERRY_INCLUDE=RADIOBERRY

ifeq ($(RADIOBERRY_INCLUDE),RADIOBERRY)
RADIOBERRY_OPTIONS=-D RADIOBERRY
RADIOBERRY_SOURCES= \
radioberry_discovery.c \
radioberry.c
RADIOBERRY_HEADERS= \
radioberry_discovery.h \
radioberry.h
RADIOBERRY_OBJS= \
radioberry_discovery.o \
radioberry.o
endif


ifeq ($(LIMESDR_INCLUDE),LIMESDR)
LIMESDR_OPTIONS=-D LIMESDR
SOAPYSDRLIBS=-lSoapySDR
LIMESDR_SOURCES= \
lime_discovery.c \
lime_protocol.c
LIMESDR_HEADERS= \
lime_discovery.h \
lime_protocol.h
LIMESDR_OBJS= \
lime_discovery.o \
lime_protocol.o
endif


ifeq ($(PSK_INCLUDE),PSK)
PSK_OPTIONS=-D PSK
PSKLIBS=-lpsk
PSK_SOURCES= \
psk.c \
psk_waterfall.c
PSK_HEADERS= \
psk.h \
psk_waterfall.h
PSK_OBJS= \
psk.o \
psk_waterfall.o
endif


ifeq ($(FREEDV_INCLUDE),FREEDV)
FREEDV_OPTIONS=-D FREEDV
FREEDVLIBS=-lcodec2
FREEDV_SOURCES= \
freedv.c \
freedv_menu.c
FREEDV_HEADERS= \
freedv.h \
freedv_menu.h
FREEDV_OBJS= \
freedv.o \
freedv_menu.o
endif

ifeq ($(LOCALCW_INCLUDE),LOCALCW)
LOCALCW_OPTIONS=-D LOCALCW
LOCALCW_SOURCES= \
beep.c \
iambic.c
LOCALCW_HEADERS= \
beep.h \
iambic.h
LOCALCW_OBJS= \
beep.o \
iambic.o
endif

#required for MRAA GPIO
#MRAA_INCLUDE=MRAA

ifeq ($(MRAA_INCLUDE),MRAA)
  GPIO_OPTIONS=-D GPIO
  GPIO_LIBS=-lmraa
  GPIO_SOURCES= \
  gpio_mraa.c
  GPIO_HEADERS= \
  gpio.h
  GPIO_OBJS= \
  gpio_mraa.o
else
  ifeq ($(UNAME_N),raspberrypi)
  GPIO_OPTIONS=-D GPIO
  GPIO_LIBS=-lwiringPi -lpigpio  -lbcm2835
  endif
  ifeq ($(UNAME_N),odroid)
  GPIO_LIBS=-lwiringPi
  endif
  ifeq ($(SX1509_INCLUDE),sx1509)
  GPIO_OPTIONS=-D GPIO
  GPIO_OPTIONS+=-D sx1509
  GPIO_LIBS+=-lsx1509
  endif
  GPIO_SOURCES= \
  gpio.c
  GPIO_HEADERS= \
  gpio.h
  GPIO_OBJS= \
  gpio.o
endif

#uncomment if build for SHORT FRAMES (MIC and Audio)
SHORT_FRAMES="-D SHORT_FRAMES"

GTKINCLUDES=`pkg-config --cflags gtk+-3.0`
GTKLIBS=`pkg-config --libs gtk+-3.0`

AUDIO_LIBS=-lasound

OPTIONS=-g -D $(UNAME_N) $(GPIO_OPTIONS) $(LIMESDR_OPTIONS) $(RADIOBERRY_OPTIONS) $(FREEDV_OPTIONS) $(LOCALCW_OPTIONS) $(PSK_OPTIONS) $(SHORT_FRAMES) -D GIT_DATE='"$(GIT_DATE)"' -D GIT_VERSION='"$(GIT_VERSION)"' $(DEBUG_OPTION) -O3

LIBS=-lrt -lm -lwdsp -lpthread $(AUDIO_LIBS) $(PSKLIBS) $(GTKLIBS) $(GPIO_LIBS) $(SOAPYSDRLIBS) $(FREEDVLIBS)
INCLUDES=$(GTKINCLUDES)

COMPILE=$(CC) $(OPTIONS) $(INCLUDES)

PROGRAM=pihpsdr

SOURCES= \
audio.c \
band.c \
configure.c \
frequency.c \
discovered.c \
filter.c \
main.c \
new_menu.c \
exit_menu.c \
general_menu.c \
audio_menu.c \
ant_menu.c \
display_menu.c \
dsp_menu.c \
pa_menu.c \
cw_menu.c \
oc_menu.c \
xvtr_menu.c \
equalizer_menu.c \
step_menu.c \
meter_menu.c \
band_menu.c \
bandstack_menu.c \
mode_menu.c \
filter_menu.c \
noise_menu.c \
agc_menu.c \
fm_menu.c \
vox_menu.c \
diversity_menu.c \
freqent_menu.c \
test_menu.c \
rit.c \
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
rigctl.c \
signal.c \
splash.c \
toolbar.c \
sliders.c \
version.c \
vfo.c \
waterfall.c \
wdsp_init.c \
button_text.c \
vox.c \
update.c \
memory.c


HEADERS= \
audio.h \
agc.h \
alex.h \
band.h \
configure.h \
frequency.h \
bandstack.h \
channel.h \
discovered.h \
filter.h \
new_menu.h \
exit_menu.h \
general_menu.h \
audio_menu.h \
ant_menu.h \
display_menu.h \
dsp_menu.h \
pa_menu.h \
cw_menu.h \
oc_menu.h \
xvtr_menu.h \
equalizer_menu.h \
step_menu.h \
meter_menu.h \
band_menu.h \
bandstack_menu.h \
mode_menu.h \
filter_menu.h \
noise_menu.h \
agc_menu.h \
fm_menu.h \
vox_menu.h \
diversity_menu.h \
freqent_menu.h \
test_menu.h \
rit.h \
meter.h \
mode.h \
old_discovery.h \
new_discovery.h \
old_protocol.h \
new_protocol.h \
panadapter.h \
property.h \
radio.h \
rigctl.h \
signal.h \
splash.h \
toolbar.h \
sliders.h \
version.h \
vfo.h \
waterfall.h \
wdsp_init.h \
button_text.h \
vox.h \
update.h \
memory.h


OBJS= \
audio.o \
band.o \
configure.o \
frequency.o \
discovered.o \
filter.o \
version.o \
main.o \
new_menu.o \
exit_menu.o \
general_menu.o \
audio_menu.o \
ant_menu.o \
display_menu.o \
dsp_menu.o \
pa_menu.o \
cw_menu.o \
oc_menu.o \
xvtr_menu.o \
equalizer_menu.o \
step_menu.o \
meter_menu.o \
band_menu.o \
bandstack_menu.o \
mode_menu.o \
filter_menu.o \
noise_menu.o \
agc_menu.o \
fm_menu.o \
vox_menu.o \
diversity_menu.o \
freqent_menu.o \
test_menu.o \
rit.o \
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
rigctl.o \
signal.o \
splash.o \
toolbar.o \
sliders.o \
vfo.o \
waterfall.o \
wdsp_init.o \
button_text.o \
vox.o \
update.o \
memory.o

all: prebuild $(PROGRAM) $(HEADERS) $(LIMESDR_HEADERS) $(RADIOBERRY_HEADERS) $(FREEDV_HEADERS) $(LOCALCW_HEADERS) $(GPIO_HEADERS) $(PSK_HEADERS) $(SOURCES) $(LIMESDR_SOURCES) $(RADIOBERRY_SOURCES) $(FREEDV_SOURCES) $(GPIO_SOURCES) $(PSK_SOURCES)

prebuild:
	rm -f version.o

$(PROGRAM): $(OBJS) $(LIMESDR_OBJS) $(RADIOBERRY_OBJS) $(FREEDV_OBJS) $(LOCALCW_OBJS) $(GPIO_OBJS) $(PSK_OBJS)
	$(LINK) -o $(PROGRAM) $(OBJS) $(GPIO_OBJS) $(LIMESDR_OBJS) $(RADIOBERRY_OBJS) $(FREEDV_OBJS) $(LOCALCW_OBJS) $(PSK_OBJS) $(LIBS)

.c.o:
	$(COMPILE) -c -o $@ $<


clean:
	-rm -f *.o
	-rm -f $(PROGRAM)

install:
	cp pihpsdr ../pihpsdr
	cp pihpsdr ./release/pihpsdr
	cd release; echo $(GIT_VERSION) > pihpsdr/latest
	cd release; tar cvf pihpsdr_$(GIT_VERSION).tar pihpsdr
	cd release; tar cvf pihpsdr.tar pihpsdr

