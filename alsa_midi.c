/*
 * MIDI support for pihpsdr
 * (C) Christoph van Wullen, DL1YCF.
 *
 * This is the "Layer-1" for ALSA-MIDI (Linux)
 * For further comments see file mac_midi.c
 */

/*
 * ALSA: MIDI devices are sub-devices to sound cards.
 *       Therefore we have to loop through the sound cards
 *       and then, for each sound card, through the
 *       sub-devices until we have found "our" MIDI
 *       input device.
 *
 *       The procedure how to find and talk with
 *       a MIDI device is taken from the sample
 *       program amidi.c in alsautils.
 */

#include <gtk/gtk.h>
#include "midi.h"
#include "midi_menu.h"
#include "alsa_midi.h"

#ifndef __APPLE__

#include <pthread.h>
#include <alsa/asoundlib.h>

MIDI_DEVICE midi_devices[MAX_MIDI_DEVICES];
int n_midi_devices;

static void* midi_thread(void *);

static enum {
        STATE_SKIP,		// skip bytes
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

static snd_rawmidi_t *input;

void configure_midi_device(gboolean state) {
  configure=state;
}

static void *midi_thread(void *arg) {
    int ret;
    MIDI_DEVICE *midi_device=(MIDI_DEVICE *)arg;
    int npfds;
    struct pollfd *pfds;
    unsigned char buf[32];
    unsigned char byte;
    unsigned short revents;
    int i;
    int chan,arg1,arg2;
    snd_rawmidi_t *input;

    if ((ret = snd_rawmidi_open(&input, NULL, midi_device->port, SND_RAWMIDI_NONBLOCK)) < 0) {
        fprintf(stderr,"cannot open port \"%s\": %s\n", midi_device->port, snd_strerror(ret));
        return NULL;
    }
    snd_rawmidi_read(input, NULL, 0); /* trigger reading */

    npfds = snd_rawmidi_poll_descriptors_count(input);
    pfds = alloca(npfds * sizeof(struct pollfd));
    snd_rawmidi_poll_descriptors(input, pfds, npfds);
    for (;;) {
	ret = poll(pfds, npfds, 250);
        if (!midi_device->active) break;
	if (ret < 0) {
            fprintf(stderr,"poll failed: %s\n", strerror(errno));
	    // Do not give up, but also do not fire too rapidly
	    usleep(250000);
	}
	if (ret <= 0) continue;  // nothing arrived, do next poll()
	if ((ret = snd_rawmidi_poll_descriptors_revents(input, pfds, npfds, &revents)) < 0) {
            fprintf(stderr,"cannot get poll events: %s\n", snd_strerror(errno));
            continue;
        }
        if (revents & (POLLERR | POLLHUP)) continue;
        if (!(revents & POLLIN)) continue;
	// something has arrived
	ret = snd_rawmidi_read(input, buf, 64);
        if (ret == 0) continue;
        if (ret < 0) {
            fprintf(stderr,"cannot read from port \"%s\": %s\n", midi_device->port, snd_strerror(ret));
            continue;
        }
        // process bytes in buffer. Since they no not necessarily form complete messages
        // we need a state machine here. 
        for (i=0; i< ret; i++) {
	    byte=buf[i];
	    switch (state) {
		case STATE_SKIP:
		    chan=byte & 0x0F;
		    switch (byte & 0xF0) {
			case 0x80:	// Note-OFF command
			    command=CMD_NOTEOFF;
			    state=STATE_ARG2;
			    break;
			case 0x90:	// Note-ON command
			    command=CMD_NOTEON;
			    state=STATE_ARG2;
			    break;    
			case 0xB0:	// Controller Change
			    command=CMD_CTRL;
			    state=STATE_ARG2;
			    break;
			case 0xE0:	// Pitch Bend
			    command=CMD_PITCH;
			    state=STATE_ARG2;
			    break;
			case 0xA0:	// Polyphonic Pressure
			case 0xC0:	// Program change
			case 0xD0:	// Channel pressure
			case 0xF0:	// System Message: continue waiting for bit7 set
			default: 	// Remain in STATE_SKIP until bit7 is set
			    break;
		    }
		    break;
		case STATE_ARG2:
		    arg1=byte;
                    state=STATE_ARG1;
                    break;
		case STATE_ARG1:
		    arg2=byte;
		    // We have a command!
		    switch (command) {
			case CMD_NOTEON:
			   // Hercules MIDI controllers generate NoteOn
			   // messages with velocity == 0 when releasing
			   // a push-button
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
        }
    }
    // no longer active: close and quit
    if((ret = snd_rawmidi_close(input)) < 0) {
       g_print("%s: cannot close port: %s\n",__FUNCTION__, snd_strerror(ret));
    }
}

int register_midi_device(int index) {
    int i;
    int ret=0;
    pthread_t midi_thread_id;

    if (index < 0 || index > MAX_MIDI_DEVICES) return -1;;

    ret = pthread_create(&midi_thread_id, NULL, midi_thread, &midi_devices[index]);
    if (ret < 0) {
      g_print("%s: Failed to create MIDI read thread\n",__FUNCTION__);
      return -1;
    }
    midi_devices[index].active=1;
    return 0;
}

void close_midi_device(int index) {
  g_print("%s\n",__FUNCTION__);
  midi_devices[index].active=0;
  usleep(500000L);  // make sure the reading thread has noticed 
}

void get_midi_devices() {

    snd_ctl_t *ctl;
    snd_rawmidi_info_t *info;
    int card, device, subs, sub, ret;
    const char *devnam, *subnam;
    int found=0;
    char portname[64];

    n_midi_devices=0;
    card=-1;
    if ((ret = snd_card_next(&card)) < 0) {
        fprintf(stderr,"cannot determine card number: %s\n", snd_strerror(ret));
        return;
    }
    while (card >= 0) {
        //fprintf(stderr,"Found Sound Card=%d\n",card);
        sprintf(portname,"hw:%d", card);
        if ((ret = snd_ctl_open(&ctl, portname, 0)) < 0) {
                fprintf(stderr,"cannot open control for card %d: %s\n", card, snd_strerror(ret));
                return;
        }
        device = -1;
        // loop through devices of the card
        for (;;) {
            if ((ret = snd_ctl_rawmidi_next_device(ctl, &device)) < 0) {
                fprintf(stderr,"cannot determine device number: %s\n", snd_strerror(ret));
                break;
            }
            if (device < 0) break;
            //fprintf(stderr,"Found Device=%d on Card=%d\n", device, card);
            // found sub-device
            snd_rawmidi_info_alloca(&info);
            snd_rawmidi_info_set_device(info, device);
            snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
            ret = snd_ctl_rawmidi_info(ctl, info);
            if (ret >= 0) {
                subs = snd_rawmidi_info_get_subdevices_count(info);
            } else {
                subs = 0;
            }
            //fprintf(stderr,"Number of MIDI input devices: %d\n", subs);
            if (!subs) break;
            // subs: number of sub-devices to device on card
            for (sub = 0; sub < subs; ++sub) {
                snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
                snd_rawmidi_info_set_subdevice(info, sub);
                ret = snd_ctl_rawmidi_info(ctl, info);
                if (ret < 0) {
                    fprintf(stderr,"cannot get rawmidi information %d:%d:%d: %s\n",
                                   card, device, sub, snd_strerror(ret));
                    break;
                }
                if (found) break;
                devnam = snd_rawmidi_info_get_name(info);
                subnam = snd_rawmidi_info_get_subdevice_name(info);
                // If there is only one sub-device and it has no name, we  use
                // devnam for comparison and make a portname of form "hw:x,y",
                // else we use subnam for comparison and make a portname of form "hw:x,y,z".
                if (sub == 0 && subnam[0] == '\0') {
                    sprintf(portname,"hw:%d,%d", card, device);
                } else {
                    sprintf(portname,"hw:%d,%d,%d", card, device, sub);
                    devnam=subnam;
                }

                if (midi_devices[n_midi_devices].name != NULL) {
                  g_free(midi_devices[n_midi_devices].name);
                }
                midi_devices[n_midi_devices].name=g_new(gchar,strlen(devnam)+1);
                strcpy(midi_devices[n_midi_devices].name,devnam);

                if (midi_devices[n_midi_devices].port != NULL) {
                  g_free(midi_devices[n_midi_devices].port);
                }
                midi_devices[n_midi_devices].port=g_new(gchar,strlen(portname)+1);
                strcpy(midi_devices[n_midi_devices].port,portname);
                n_midi_devices++;
            }
        }
        snd_ctl_close(ctl);
        // next card
        if ((ret = snd_card_next(&card)) < 0) {
            fprintf(stderr,"cannot determine card number: %s\n", snd_strerror(ret));
            break;
        }
    }

    for(int i=0;i<n_midi_devices;i++) {
        g_print("%s: %d: %s %s\n",__FUNCTION__,i,midi_devices[i].name,midi_devices[i].port);
    }
}
#endif
