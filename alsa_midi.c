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

#include "midi.h"

#ifndef __APPLE__

#include <pthread.h>
#include <alsa/asoundlib.h>

static pthread_t midi_thread_id;
static void* midi_thread(void *);

static char portname[64];

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

static void *midi_thread(void *arg) {
    int ret;
    snd_rawmidi_t *input;
    int npfds;
    struct pollfd *pfds;
    unsigned char buf[32];
    unsigned char byte;
    unsigned short revents;
    int i;
    int chan,arg1,arg2;

    if ((ret = snd_rawmidi_open(&input, NULL, portname, SND_RAWMIDI_NONBLOCK)) < 0) {
        fprintf(stderr,"cannot open port \"%s\": %s\n", portname, snd_strerror(ret));
        return NULL;
    }
    snd_rawmidi_read(input, NULL, 0); /* trigger reading */

    npfds = snd_rawmidi_poll_descriptors_count(input);
    pfds = alloca(npfds * sizeof(struct pollfd));
    snd_rawmidi_poll_descriptors(input, pfds, npfds);
    for (;;) {
	ret = poll(pfds, npfds, 250);
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
            fprintf(stderr,"cannot read from port \"%s\": %s\n", portname, snd_strerror(ret));
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
        }
    }
}

void register_midi_device(char *myname) {

    int mylen=strlen(myname);
    snd_ctl_t *ctl;
    snd_rawmidi_info_t *info;
    int card, device, subs, sub, ret;
    const char *devnam, *subnam;
    int found=0;
    char name[64];

    card=-1;
    if ((ret = snd_card_next(&card)) < 0) {
        fprintf(stderr,"cannot determine card number: %s\n", snd_strerror(ret));
        return;
    }
    while (card >= 0) {
	fprintf(stderr,"Found Sound Card=%d\n",card);
	sprintf(name,"hw:%d", card);
        if ((ret = snd_ctl_open(&ctl, name, 0)) < 0) {
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
	    fprintf(stderr,"Found Device=%d on Card=%d\n", device, card);
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
	    fprintf(stderr,"Number of MIDI input devices: %d\n", subs);
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
		if (!strncmp(myname, devnam, mylen)) {
		    found=1;
		    fprintf(stderr,"MIDI device %s selected (PortName=%s)\n", devnam, portname);
		} else {
                    fprintf(stderr,"MIDI device found BUT NOT SELECTED: %s\n", devnam);
		}
	    }
	}
	snd_ctl_close(ctl);
	// next card
        if ((ret = snd_card_next(&card)) < 0) {
            fprintf(stderr,"cannot determine card number: %s\n", snd_strerror(ret));
            break;
        }
    }
    if (!found) {
	fprintf(stderr,"MIDI device %s NOT FOUND!\n", myname);
    }
    // Found our MIDI input device. Spawn off a thread reading data
    ret = pthread_create(&midi_thread_id, NULL, midi_thread, NULL);
    if (ret < 0) {
	fprintf(stderr,"Failed to create MIDI read thread\n");
    }
}
#endif
