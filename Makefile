# get the OS Name
UNAME_S := $(shell uname -s)

# Get git commit version and date
GIT_DATE := $(firstword $(shell git --no-pager show --date=short --format="%ai" --name-only))
GIT_VERSION := $(shell git describe --abbrev=0 --tags)

# uncomment the following line to force 480x320 screen
#SMALL_SCREEN_OPTIONS=-D SMALL_SCREEN

# uncomment the line below to include GPIO
# For support of:
#    CONTROLLER1 (Original Controller)
#    CONTROLLER2_V1 single encoders with MCP23017 switches
#    CONTROLLER2_V2 dual encoders with MCP23017 switches
#
GPIO_INCLUDE=GPIO

# uncomment the line below to include Pure Signal support
PURESIGNAL_INCLUDE=PURESIGNAL

# uncomment the line below to include MIDI support
#MIDI_INCLUDE=MIDI

# uncomment the line below to include USB Ozy support
# USBOZY_INCLUDE=USBOZY

# uncomment the line to below include support local CW keyer
#LOCALCW_INCLUDE=LOCALCW

# uncomment the line below for SoapySDR
#SOAPYSDR_INCLUDE=SOAPYSDR

# uncomment the line to below include support for sx1509 i2c expander
#SX1509_INCLUDE=sx1509

# uncomment the line below to include support for STEMlab discovery (WITH AVAHI)
#STEMLAB_DISCOVERY=STEMLAB_DISCOVERY

# uncomment the line below to include support for STEMlab discovery (WITHOUT AVAHI)
#STEMLAB_DISCOVERY=STEMLAB_DISCOVERY_NOAVAHI


# uncomment the line below for various debug facilities
#DEBUG_OPTION=-D DEBUG

#PTT_INCLUDE=PTT

# very early code not included yet
#SERVER_INCLUDE=SERVER

CC=gcc
LINK=gcc

ifeq ($(MIDI_INCLUDE),MIDI)
MIDI_OPTIONS=-D MIDI
MIDI_HEADERS= midi.h midi_menu.h
ifeq ($(UNAME_S), Darwin)
MIDI_SOURCES= mac_midi.c midi2.c midi3.c
MIDI_OBJS= mac_midi.o midi2.o midi3.o midi_menu.o
MIDI_LIBS= -framework CoreMIDI -framework Foundation
endif
ifeq ($(UNAME_S), Linux)
MIDI_SOURCES= alsa_midi.c midi2.c midi3.c midi_menu.c
MIDI_OBJS= alsa_midi.o midi2.o midi3.o midi_menu.o
MIDI_LIBS= -lasound
endif
endif

ifeq ($(PURESIGNAL_INCLUDE),PURESIGNAL)
PURESIGNAL_OPTIONS=-D PURESIGNAL
PURESIGNAL_SOURCES= \
ps_menu.c
PURESIGNAL_HEADERS= \
ps_menu.h
PURESIGNAL_OBJS= \
ps_menu.o
endif

ifeq ($(REMOTE_INCLUDE),REMOTE)
REMOTE_OPTIONS=-D REMOTE
REMOTE_SOURCES= \
remote_radio.c \
remote_receiver.c
REMOTE_HEADERS= \
remote_radio.h \
remote_receiver.h
REMOTE_OBJS= \
remote_radio.o \
remote_receiver.o
endif

ifeq ($(USBOZY_INCLUDE),USBOZY)
USBOZY_OPTIONS=-D USBOZY
USBOZY_LIBS=-lusb-1.0
USBOZY_SOURCES= \
ozyio.c
USBOZY_HEADERS= \
ozyio.h
USBOZY_OBJS= \
ozyio.o
endif


ifeq ($(SOAPYSDR_INCLUDE),SOAPYSDR)
SOAPYSDR_OPTIONS=-D SOAPYSDR
SOAPYSDRLIBS=-lSoapySDR
SOAPYSDR_SOURCES= \
soapy_discovery.c \
soapy_protocol.c
SOAPYSDR_HEADERS= \
soapy_discovery.h \
soapy_protocol.h
SOAPYSDR_OBJS= \
soapy_discovery.o \
soapy_protocol.o
endif


ifeq ($(LOCALCW_INCLUDE),LOCALCW)
LOCALCW_OPTIONS=-D LOCALCW
LOCALCW_SOURCES= iambic.c
LOCALCW_HEADERS= iambic.h
LOCALCW_OBJS   = iambic.o
endif

ifeq ($(PTT_INCLUDE),PTT)
PTT_OPTIONS=-D PTT
endif

ifeq ($(GPIO_INCLUDE),GPIO)
GPIOD_VERSION=$(shell pkg-config --modversion libgpiod)
ifeq ($(GPIOD_VERSION),1.2)
GPIOD_OPTIONS=-D OLD_GPIOD
endif
GPIO_OPTIONS=-D GPIO
GPIO_LIBS=-lgpiod -li2c
GPIO_SOURCES= \
  configure.c \
  i2c.c \
  gpio.c \
  encoder_menu.c \
  switch_menu.c
GPIO_HEADERS= \
  configure.h \
  i2c.h \
  gpio.h \
  encoder_menu.h \
  switch_menu.h
GPIO_OBJS= \
  configure.o \
  i2c.o \
  gpio.o \
  encoder_menu.o \
  switch_menu.o
endif

#
# We have two versions of STEMLAB_DISCOVERY here,
# the second one has to be used
# if you do not have the avahi (devel-) libraries
# on your system.
#
ifeq ($(STEMLAB_DISCOVERY), STEMLAB_DISCOVERY)
STEMLAB_OPTIONS=-D STEMLAB_DISCOVERY \
  `pkg-config --cflags avahi-gobject` \
  `pkg-config --cflags libcurl`
STEMLAB_LIBS=`pkg-config --libs avahi-gobject` `pkg-config --libs libcurl`
STEMLAB_SOURCES=stemlab_discovery.c
STEMLAB_HEADERS=stemlab_discovery.h
STEMLAB_OBJS=stemlab_discovery.o
endif

ifeq ($(STEMLAB_DISCOVERY), STEMLAB_DISCOVERY_NOAVAHI)
STEMLAB_OPTIONS=-D STEMLAB_DISCOVERY -D NO_AVAHI `pkg-config --cflags libcurl`
STEMLAB_LIBS=`pkg-config --libs libcurl`
STEMLAB_SOURCES=stemlab_discovery.c
STEMLAB_HEADERS=stemlab_discovery.h
STEMLAB_OBJS=stemlab_discovery.o
endif

ifeq ($(SERVER_INCLUDE), SERVER)
SERVER_OPTIONS=-D SERVER
SERVER_SOURCES= \
hpsdr_server.c
SERVER_HEADERS= \
hpsdr_server.h
SERVER_OBJS= \
hpsdr_server.o
endif

GTKINCLUDES=`pkg-config --cflags gtk+-3.0`
GTKLIBS=`pkg-config --libs gtk+-3.0`

ifeq ($(UNAME_S), Linux)
AUDIO_LIBS=-lpulse-simple -lpulse -lpulse-mainloop-glib
AUDIO_SOURCES=pulseaudio.c
AUDIO_OBJS=pulseaudio.o
endif
ifeq ($(UNAME_S), Darwin)
AUDIO_OPTIONS=-DPORTAUDIO
AUDIO_LIBS=-lportaudio
AUDIO_SOURCES=portaudio.c
AUDIO_OBJS=portaudio.o
endif

//CFLAGS=	-g -Wno-deprecated-declarations -O3
CFLAGS=	-g -Wno-deprecated-declarations
OPTIONS=$(SMALL_SCREEN_OPTIONS) $(MIDI_OPTIONS) $(PURESIGNAL_OPTIONS) $(REMOTE_OPTIONS) $(USBOZY_OPTIONS) \
	$(GPIO_OPTIONS) $(GPIOD_OPTIONS) $(SOAPYSDR_OPTIONS) $(LOCALCW_OPTIONS) \
	$(STEMLAB_OPTIONS) \
        $(PTT_OPTIONS) \
	$(SERVER_OPTIONS) \
	$(AUDIO_OPTIONS) \
	-D GIT_DATE='"$(GIT_DATE)"' -D GIT_VERSION='"$(GIT_VERSION)"' $(DEBUG_OPTION)


ifeq ($(UNAME_S), Linux)
RT_OPTION=-lrt
endif

LIBS=$(RT_OPTION) -lm -lwdsp -lpthread $(AUDIO_LIBS) $(USBOZY_LIBS) $(GTKLIBS) $(GPIO_LIBS) $(SOAPYSDRLIBS) $(STEMLAB_LIBS) $(MIDI_LIBS)
INCLUDES=$(GTKINCLUDES)

COMPILE=$(CC) $(CFLAGS) $(OPTIONS) $(INCLUDES)

.c.o:
	$(COMPILE) -c -o $@ $<

PROGRAM=pihpsdr

SOURCES= \
band.c \
discovered.c \
discovery.c \
filter.c \
main.c \
new_menu.c \
about_menu.c \
exit_menu.c \
radio_menu.c \
rx_menu.c \
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
vox_menu.c \
fft_menu.c \
diversity_menu.c \
tx_menu.c \
vfo_menu.c \
test_menu.c \
meter.c \
mode.c \
old_discovery.c \
new_discovery.c \
old_protocol.c \
new_protocol.c \
new_protocol_programmer.c \
rx_panadapter.c \
tx_panadapter.c \
property.c \
radio.c \
receiver.c \
rigctl.c \
rigctl_menu.c \
toolbar.c \
transmitter.c \
zoompan.c \
sliders.c \
version.c \
vfo.c \
waterfall.c \
button_text.c \
vox.c \
update.c \
store.c \
store_menu.c \
memory.c \
led.c \
ext.c \
error_handler.c \
cwramp.c \
protocols.c \
css.c \
actions.c \
action_dialog.c \
configure.c \
i2c.c \
gpio.c \
encoder_menu.c \
switch_menu.c \
toolbar_menu.c



HEADERS= \
agc.h \
alex.h \
band.h \
bandstack.h \
channel.h \
discovered.h \
discovery.h \
filter.h \
new_menu.h \
about_menu.h \
rx_menu.h \
exit_menu.h \
radio_menu.h \
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
vox_menu.h \
fft_menu.h \
diversity_menu.h \
tx_menu.h \
vfo_menu.h \
test_menu.h \
meter.h \
mode.h \
old_discovery.h \
new_discovery.h \
old_protocol.h \
new_protocol.h \
rx_panadapter.h \
tx_panadapter.h \
property.h \
radio.h \
receiver.h \
rigctl.h \
rigctl_menu.h \
toolbar.h \
transmitter.h \
zoompan.h \
sliders.h \
version.h \
vfo.h \
waterfall.h \
button_text.h \
vox.h \
update.h \
store.h \
store_menu.h \
memory.h \
led.h \
ext.h \
error_handler.h \
protocols.h \
css.h \
actions.h \
action_dialog.h \
configure.h \
i2c.h \
gpio.h \
encoder_menu.h \
switch_menu.h \
toolbar_menu.h



OBJS= \
band.o \
discovered.o \
discovery.o \
filter.o \
version.o \
main.o \
new_menu.o \
about_menu.o \
rx_menu.o \
exit_menu.o \
radio_menu.o \
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
vox_menu.o \
fft_menu.o \
diversity_menu.o \
tx_menu.o \
vfo_menu.o \
test_menu.o \
meter.o \
mode.o \
old_discovery.o \
new_discovery.o \
old_protocol.o \
new_protocol.o \
new_protocol_programmer.o \
rx_panadapter.o \
tx_panadapter.o \
property.o \
radio.o \
receiver.o \
rigctl.o \
rigctl_menu.o \
toolbar.o \
transmitter.o \
zoompan.o \
sliders.o \
vfo.o \
waterfall.o \
button_text.o \
vox.o \
update.o \
store.o \
store_menu.o \
memory.o \
led.o \
ext.o \
error_handler.o \
cwramp.o \
protocols.o \
css.o \
actions.o \
action_dialog.o \
configure.o \
i2c.o \
gpio.o \
encoder_menu.o \
switch_menu.o \
toolbar_menu.o

$(PROGRAM):  $(OBJS) $(AUDIO_OBJS) $(REMOTE_OBJS) $(USBOZY_OBJS) $(SOAPYSDR_OBJS) \
		$(LOCALCW_OBJS) $(PURESIGNAL_OBJS) \
		$(MIDI_OBJS) $(STEMLAB_OBJS) $(SERVER_OBJS)
	$(LINK) -o $(PROGRAM) $(OBJS) $(AUDIO_OBJS) $(REMOTE_OBJS) $(USBOZY_OBJS) \
		$(SOAPYSDR_OBJS) $(LOCALCW_OBJS) $(PURESIGNAL_OBJS) \
		$(MIDI_OBJS) $(STEMLAB_OBJS) $(SERVER_OBJS) $(LIBS)

.PHONY:	all
all:	prebuild  $(PROGRAM) $(HEADERS) $(AUDIO_HEADERS) $(USBOZY_HEADERS) $(SOAPYSDR_HEADERS) \
	$(LOCALCW_HEADERS) \
	$(PURESIGNAL_HEADERS) $(MIDI_HEADERS) $(STEMLAB_HEADERS) $(SERVER_HEADERS) \
	$(AUDIO_SOURCES) $(SOURCES) \
	$(USBOZY_SOURCES) $(SOAPYSDR_SOURCES) $(LOCALCW_SOURCE) \
	$(PURESIGNAL_SOURCES) $(MIDI_SOURCES) $(STEMLAB_SOURCES) $(SERVER_SOURCES)

.PHONY:	prebuild
prebuild:
	rm -f version.o

#
# On some platforms, INCLUDES contains "-pthread"  (from a pkg-config output)
# which is not a valid cppcheck option
# Therefore, correct this here. Furthermore, we can add additional options to CPP
# in the variable CPPOPTIONS
#
CPPOPTIONS= --enable=all --suppress=shadowVariable --suppress=variableScope
CPPINCLUDES:=$(shell echo $(INCLUDES) | sed -e "s/-pthread / /" )

.PHONY:	cppcheck
cppcheck:
	cppcheck $(CPPOPTIONS) $(OPTIONS) $(CPPINCLUDES) $(AUDIO_SOURCES) $(SOURCES) $(REMOTE_SOURCES) \
	$(USBOZY_SOURCES) $(SOAPYSDR_SOURCES) \
	$(PURESIGNAL_SOURCES) $(MIDI_SOURCES) $(STEMLAB_SOURCES) $(LOCALCW_SOURCES) \
	$(SERVER_SOURCES)

.PHONY:	clean
clean:
	-rm -f *.o
	-rm -f $(PROGRAM) hpsdrsim

.PHONY:	install
install: $(PROGRAM)
	cp $(PROGRAM) /usr/local/bin

.PHONY:	release
release: $(PROGRAM)
	cp $(PROGRAM) release/pihpsdr
	cd release; tar cvf pihpsdr.tar pihpsdr
	cd release; tar cvf pihpsdr-$(GIT_VERSION).tar pihpsdr

.PHONY:	nocontroller
nocontroller: clean controller1 $(PROGRAM)
	cp $(PROGRAM) release/pihpsdr
	cd release; tar cvf pihpsdr-nocontroller.$(GIT_VERSION).tar pihpsdr

.PHONY:	controller1
controller1: clean $(PROGRAM)
	cp $(PROGRAM) release/pihpsdr
	cd release; tar cvf pihpsdr-controller1.$(GIT_VERSION).tar pihpsdr

.PHONY:	controller2v1
controller2v1: clean $(PROGRAM)
	cp $(PROGRAM) release/pihpsdr
	cd release; tar cvf pihpsdr-controller2-v1.$(GIT_VERSION).tar pihpsdr

.PHONY:	controller2v2
controller2v2: clean $(PROGRAM)
	cp $(PROGRAM) release/pihpsdr
	cd release; tar cvf pihpsdr-controller2-v2.$(GIT_VERSION).tar pihpsdr


#############################################################################
#
# hpsdrsim is a cool program that emulates an SDR board with UDP and TCP
# facilities. It even feeds back the TX signal and distorts it, so that
# you can test PURESIGNAL.
# This feature only works if the sample rate is 48000
#
#############################################################################

hpsdrsim.o:	hpsdrsim.c hpsdrsim.h
	$(CC) -c -O -DALSASOUND hpsdrsim.c

newhpsdrsim.o:	newhpsdrsim.c hpsdrsim.h
	$(CC) -c -O newhpsdrsim.c

hpsdrsim:	hpsdrsim.o newhpsdrsim.o
	$(LINK) -o hpsdrsim hpsdrsim.o newhpsdrsim.o -lasound -lm -lpthread


debian:
	cp $(PROGRAM) pkg/pihpsdr/usr/local/bin
	cp /usr/local/lib/libwdsp.so pkg/pihpsdr/usr/local/lib
	cp release/pihpsdr/hpsdr.png pkg/pihpsdr/usr/share/pihpsdr
	cp release/pihpsdr/hpsdr_icon.png pkg/pihpsdr/usr/share/pihpsdr
	cp release/pihpsdr/pihpsdr.desktop pkg/pihpsdr/usr/share/applications
	cd pkg; dpkg-deb --build pihpsdr

