/*
 * MIDI support for pihpsdr
 * (C) Christoph van Wullen, DL1YCF.
 *
 * This is the "Layer-1" for Apple Macintosh.
 *
 * This file implements the function register_midi_device,
 * which is called with the name of a supported MIDI device.
 * This name may be truncated:  For example, the call
 * register_midi_device("COMPANY MIDI")
 * will accept both "COMPANY MIDI X" and "COMPANY MIDI Y" devices.
 *
 * If more than one MIDI device matches the name, the LAST ONE
 * found will be taken. This may not be predictable, so it is
 * better to say that one of the matching MIDI devices will be taken.
 * It is easy to change the code such that ALL devices matching the
 * name will be taken. But who cares? Normally there will only be a
 * single MIDI controller connected to the computer running the SDR
 * program.
 *
 * The name is actually specified by the user in the midi.inp file
 * (see midi2.c)
 *
 * This file must generate calls to Layer-2 NewMidiEvent().
 * Some type of messages are not consideres (pressure change, etc.),
 * they are silently dropped but must be processed.
 *
 */

#include "midi.h"

#ifdef __APPLE__

/*
 * For MacOS, things are easy:
 * The OS takes care of everything, we only have to register a callback
 * routine.
 */

#include <Carbon/Carbon.h>

#include <CoreMIDI/MIDIServices.h>
#include <CoreAudio/HostTime.h>
#include <CoreAudio/CoreAudio.h>

//
// MIDI callback function
// called by MacOSX when data from the specified MIDI device arrives.
// We process *all* data but only generate calls to layer-2 for Note On/Off
// and ControllerChange events.
//

//
// Although MacOS does all the grouping of MIDI bytes into commands for us,
// we use here the same state machine as we have in the ALSA MIDI implementation
// That is, we look at each MIDI byte separately
//

static enum {
        STATE_SKIP,		// skip bytes until command bit is set
        STATE_ARG1,             // one arg byte to come
        STATE_ARG2,             // two arg bytes to come
} state=STATE_SKIP;

static enum {
        CMD_NOTEON,
        CMD_NOTEOFF,
        CMD_CTRL,
        CMD_PITCH,
} command;

static void ReadMIDIdevice(const MIDIPacketList *pktlist, void *refCon, void *connRefCon) {
    int i,j,k,byte,chan,arg1,arg2;
    MIDIPacket *packet = (MIDIPacket *)pktlist->packet;
	
	
    // loop through all packets in the current list
    for (j=0; j < pktlist->numPackets; ++j) {
	for (i=0; i<packet->length; i++) {
	    byte=packet->data[i];
            switch (state) {
                case STATE_SKIP:
                    chan=byte & 0x0F;
                    switch (byte & 0xF0) {
                        case 0x80:      // Note-OFF command
                            command=CMD_NOTEOFF;
                            state=STATE_ARG2;
                            break;
                        case 0x90:      // Note-ON command
                            command=CMD_NOTEON;
                            state=STATE_ARG2;
                            break;
                        case 0xB0:      // Controller Change
                            command=CMD_CTRL;
                            state=STATE_ARG2;
                            break;
                        case 0xE0:      // Pitch Bend
                            command=CMD_PITCH;
                            state=STATE_ARG2;
                            break;
                        case 0xA0:      // Polyphonic Pressure: skip args
                        case 0xC0:      // Program change: skip args
                        case 0xD0:      // Channel pressure: skip args
                        case 0xF0:      // System Message: skip args
                        default:	// Remain in STATE_SKIP until "interesting" command seen
                            break;
                    }
                    break;
                case STATE_ARG2:	// store byte as first argument
                    arg1=byte;
                    state=STATE_ARG1;
                    break;
                case STATE_ARG1:	// store byte as second argument, process command
                    arg2=byte;
                    // We have a command!
                    switch (command) {
                        case CMD_NOTEON:
                           NewMidiEvent(MIDI_NOTE, chan, arg1, 1);
                           break;
                        case CMD_NOTEOFF:
                           NewMidiEvent(MIDI_NOTE, chan, arg1, 0);
                           break;
                        case CMD_CTRL:
                           NewMidiEvent(MIDI_CTRL, chan, arg1, arg2);
                           break;
                        case CMD_PITCH:
                           NewMidiEvent(MIDI_PITCH, chan, 0, arg1+128*arg2);
                           break;
                    }
                    state=STATE_SKIP;
                    break;
            }
	} // i-loop through the packet
	packet = MIDIPacketNext(packet);
    } // j-loop through the list of packets
}


void register_midi_device(char *myname) {
    unsigned long nDevices;
    int i;
    CFStringRef pname;
    char name[100];
    int FoundMIDIref=-1;
    int mylen=strlen(myname);


//
// Go through the list of MIDI devices and
// look whether the one we are looking for is there
//

    nDevices=MIDIGetNumberOfSources();
    for (i=0; i<nDevices; i++) {
	MIDIEndpointRef dev = MIDIGetSource(i);
	if (dev != 0) {
	    MIDIObjectGetStringProperty(dev, kMIDIPropertyName, &pname);
	    CFStringGetCString(pname, name, sizeof(name), 0);
	    CFRelease(pname);
	    fprintf(stderr,"MIDI device found: >>>%s<<<\n", name);
	    if (!strncmp(name, myname, mylen)) {
		FoundMIDIref=i;
		fprintf(stderr,"MIDI device found and selected: >>>%s<<<\n", name);
	    } else {
		fprintf(stderr,"MIDI device found BUT NOT SELECTED: >>>%s<<<\n", name);
	    }
	}
    }

//
// If we found "our" device, register a callback routine
//

    if (FoundMIDIref >= 0) {
        MIDIClientRef client = 0;
        MIDIPortRef myMIDIport = 0;
        //Create client
        MIDIClientCreate(CFSTR("piHPSDR"),NULL,NULL, &client);
        MIDIInputPortCreate(client, CFSTR("FromMIDI"), ReadMIDIdevice, NULL, &myMIDIport);
        MIDIPortConnectSource(myMIDIport,MIDIGetSource(FoundMIDIref), NULL);
    }
}
#endif
