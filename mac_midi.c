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
static void ReadMIDIdevice(const MIDIPacketList *pktlist, void *refCon, void *connRefCon) {
    int i,j,k,command,chan;
    MIDIPacket *packet = (MIDIPacket *)pktlist->packet;
	
	
    // loop through all packets in the current list
    for (j=0; j < pktlist->numPackets; ++j) {
        if (packet->length > 0) { 
	    for ( i = 0; i<(packet->length); ) {
		command=packet->data[i];
                if ((command & 128) != 128) continue;
                
                chan = command & 0x0F;

                switch (command & 0xF0) {
                  case 0x80:  // Note off
			NewMidiEvent(MIDI_NOTE, chan, packet->data[i+1], 0);
			//fprintf(stderr,"NOTE OFF: Note=%d Chan=%d Vel=%d\n", packet->data[i+1], chan, packet->data[i+2]);
			i +=3;
			break;
                  case 0x90:  // Note on
			NewMidiEvent(MIDI_NOTE, chan, packet->data[i+1], 1);
			//fprintf(stderr,"NOTE ON : Note=%d Chan=%d Vel=%d\n", packet->data[i+1], chan, packet->data[i+2]);
			i +=3;
			break;
                  case 0xA0:  // Polyph. Press.
			fprintf(stderr,"PolPress: Note=%d Chan=%d Prs=%d\n", packet->data[i+1], chan, packet->data[i+2]);
			i +=3;
			break;
                  case 0xB0:  // Control change
			NewMidiEvent(MIDI_CTRL, chan, packet->data[i+1], packet->data[i+2]);
			//fprintf(stderr,"CtlChang: Ctrl=%d Chan=%d Val=%d\n", packet->data[i+1], chan, packet->data[i+2]);
			i +=3;
			break;
                  case 0xC0:  // Program change
			fprintf(stderr,"PgmChang: Prog=%d Chan=%d\n", packet->data[i+1], chan);
			i +=2;
			break;
                  case 0xD0:  // Channel Pressure
			fprintf(stderr, "ChanPres: Pres=%d Chan=%d\n", packet->data[i+1], chan);
			i +=2;
			break;
                  case 0xE0:  // Pitch Bend
			NewMidiEvent(MIDI_PITCH, chan, 0, packet->data[i+1] + 128*packet->data[i+2]);
			//fprintf(stderr,"Pitch   : val =%d Chan=%d\n", packet->data[i+1] + 128*packet->data[i+2], chan);
			i +=3;
			break;
                  case 0xF0:  
			fprintf(stderr, "System  : %x", command);
                        while ((command = (packet->data[++i]) & 128) != 128) {
                          fprintf(stderr," %x",command);
                        }
                        fprintf(stderr,"\n");
			break;
                }
	    } // i-loop through the packet
	    packet = MIDIPacketNext(packet);
        } // if packet length > 1
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
