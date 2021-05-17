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

#include <gtk/gtk.h>
#include "discovered.h"
#include "receiver.h"
#include "transmitter.h"
#include "receiver.h"
#include "adc.h"
#include "dac.h"
#include "radio.h"
#include "midi.h"
#include "midi_menu.h"
#include "alsa_midi.h"

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

MIDI_DEVICE midi_devices[MAX_MIDI_DEVICES];
int n_midi_devices;
int running;

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

static gboolean configure=FALSE;

static void ReadMIDIdevice(const MIDIPacketList *pktlist, void *refCon, void *connRefCon) {
    int i,j,byte,chan,arg1,arg2;
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
                           // Hercules MIDI controllers generate NoteOn
                           // messages with velocity == 0 when releasing
                           // a push-button.
                           if (arg2 == 0) {
                             if(configure) {
                               NewMidiConfigureEvent(MIDI_EVENT_NOTE, chan, arg1, 0);
                             } else {
                               NewMidiEvent(MIDI_EVENT_NOTE, chan, arg1, 0);
                             }
                           } else {
                             if(configure) {
                               NewMidiConfigureEvent(MIDI_EVENT_NOTE, chan, arg1, 1);
                             } else {
                               NewMidiEvent(MIDI_EVENT_NOTE, chan, arg1, 1);
                             }
                           }
                           break;
                        case CMD_NOTEOFF:
                           if(configure) {
                             NewMidiConfigureEvent(MIDI_EVENT_NOTE, chan, arg1, 0);
                           } else {
                             NewMidiEvent(MIDI_EVENT_NOTE, chan, arg1, 0);
                           }
                           break;
                        case CMD_CTRL:
                           if(configure) {
                             NewMidiConfigureEvent(MIDI_EVENT_CTRL, chan, arg1, arg2);
                           } else {
                             NewMidiEvent(MIDI_EVENT_CTRL, chan, arg1, arg2);
                           }
                           break;
                        case CMD_PITCH:
                           if(configure) {
                             NewMidiConfigureEvent(MIDI_EVENT_PITCH, chan, 0, arg1+128*arg2);
                           } else {
                             NewMidiEvent(MIDI_EVENT_PITCH, chan, 0, arg1+128*arg2);
                           }
                           break;
                    }
                    state=STATE_SKIP;
                    break;
            }
	} // i-loop through the packet
	packet = MIDIPacketNext(packet);
    } // j-loop through the list of packets
}

//
// store the ports and clients locally such that we
// can properly close a MIDI connection.
// This can be local static data, no one outside this file
// needs it.
//
static MIDIPortRef myMIDIports[MAX_MIDI_DEVICES];
static MIDIClientRef myClients[MAX_MIDI_DEVICES];

void close_midi_device(index) {
    fprintf(stderr,"%s index=%d\n",__FUNCTION__, index);
    if (index < 0 || index > n_midi_devices) return;
    //
    // This should release the resources associated with the pending connection
    //
    MIDIPortDisconnectSource(myMIDIports[index], MIDIGetSource(index));
    midi_devices[index].active=0;
}

void register_midi_device(int index) {
    OSStatus osret;

    g_print("%s: index=%d\n",__FUNCTION__,index);
//
//  Register a callback routine for the device
//
    if (index < 0 || index > MAX_MIDI_DEVICES) return;

     myClients[index]=0;
     myMIDIports[index] = 0;
     //Create client and port, and connect
     osret=MIDIClientCreate(CFSTR("piHPSDR"),NULL,NULL, &myClients[index]);
     if (osret !=0) {
       g_print("%s: MIDIClientCreate failed with ret=%d\n", __FUNCTION__, (int) osret);
       return;
     }
     osret=MIDIInputPortCreate(myClients[index], CFSTR("FromMIDI"), ReadMIDIdevice, NULL, &myMIDIports[index]);
     if (osret !=0) {
        g_print("%s: MIDIInputPortCreate failed with ret=%d\n", __FUNCTION__, (int) osret);
        return;
     }
     osret=MIDIPortConnectSource(myMIDIports[index] ,MIDIGetSource(index), NULL);
     if (osret != 0) {
        g_print("%s: MIDIPortConnectSource failed with ret=%d\n", __FUNCTION__, (int) osret);
        return;
     }
     //
     // Now we have successfully opened the device.
     //
     midi_devices[index].active=1;
     return;
}

void get_midi_devices() {
    int n;
    int i;
    CFStringRef pname;   // MacOS name of the device
    char name[100];      // C name of the device
    OSStatus osret;
    static int first=1;

    if (first) {
      //
      // perhaps not necessary in C, but good programming practise:
      // initialize the table upon the first call
      //
      first=0;
      for (i=0; i<MAX_MIDI_DEVICES; i++) {
        midi_devices[i].name=NULL;
        midi_devices[i].active=0;
      }
    }

//
//  This is called at startup (via midi_restore) and each time
//  the MIDI menu is opened. So we have to take care that this
//  function is essentially a no-op if the device list has not
//  changed.
//  If the device list has changed because of hot-plugging etc.
//  close any MIDI device which changed position and mark
//  it as inactive. Note that upon a hot-plug, MIDI devices that were
//  there before may change its position in the device list and will then
//  be closed.
//
    n=MIDIGetNumberOfSources();
    n_midi_devices=0;
    for (i=0; i<n; i++) {
        MIDIEndpointRef dev = MIDIGetSource(i);
        if (dev != 0) {
            osret=MIDIObjectGetStringProperty(dev, kMIDIPropertyName, &pname);
            if (osret !=0) break; // in this case pname is invalid
            CFStringGetCString(pname, name, sizeof(name), 0);
            CFRelease(pname);
            //
            // Some users have reported that MacOS reports a string of length zero
            // for some MIDI devices. In this case, we replace the name by
            // "NoPort<n>"
            //
            if (strlen(name) == 0) sprintf(name,"NoPort%d",n_midi_devices);
            g_print("%s: %s\n",__FUNCTION__,name);
            if (midi_devices[n_midi_devices].name != NULL) {
              if (strncmp(name, midi_devices[n_midi_devices].name,sizeof(name))) {
                //
                // This slot was occupied and the names do not match:
                // Close device (if active), insert new name
                //
                if (midi_devices[n_midi_devices].active) {
                  close_midi_device(n_midi_devices);
                }
                g_free(midi_devices[n_midi_devices].name);
                midi_devices[n_midi_devices].name=g_new(gchar,strlen(name)+1);
                strcpy(midi_devices[n_midi_devices].name, name);
              } else {
                //
                // This slot was occupied and the names match: do nothing!
                // If there was no hot-plug or hot-unplug, we should always
                // arrive here!
                //
              }
            } else {
	      //
              // This slot was unoccupied. Insert name and mark inactive
              //
              midi_devices[n_midi_devices].name=g_new(gchar,strlen(name)+1);
              strcpy(midi_devices[n_midi_devices].name, name);
              midi_devices[n_midi_devices].active=0;
            }
            n_midi_devices++;
        }
        //
        // If there are more devices than we have slots in our Table
        // just stop processing.
        //
        if (n_midi_devices >= MAX_MIDI_DEVICES) break;
    }
    g_print("%s: number of devices=%d\n",__FUNCTION__,n_midi_devices);
    //
    // Get rid of all devices lingering around above the high-water mark
    // (this happens in the case of hot-unplugging)
    //
    for (i=n_midi_devices; i<MAX_MIDI_DEVICES; i++) {
      if (midi_devices[i].active) {
        close_midi_device(i);
      }
      if (midi_devices[i].name != NULL) {
        g_free(midi_devices[i].name);
        midi_devices[i].name=NULL;
      }
    }
}

void configure_midi_device(gboolean state) {
  configure=state;
}

#endif
