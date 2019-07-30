/*
 * Layer-2 of MIDI support
 *
 * (C) Christoph van Wullen, DL1YCF
 *
 * Using the data in MIDICommandsTable, this subroutine translates the low-level
 * MIDI events into MIDI actions in the SDR console.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "midi.h"

void NewMidiEvent(enum MIDIevent event, int channel, int note, int val) {

    struct desc *desc;
    int new;
    static enum MIDIaction last_wheel_action;
    static struct timespec tp, last_wheel_tp={0,0};
    long delta;

    if (event == MIDI_PITCH) {
	desc=MidiCommandsTable.pitch;
    } else {
	desc=MidiCommandsTable.desc[note];
    }
    while (desc) {
	if ((desc->channel == channel || desc->channel == -1) && (desc->event == event)) {
	    // Found matching entry
	    switch (desc->event) {
		case MIDI_NOTE:
		    if ((val == 1 || (val == 0 && desc->onoff == 1)) && desc->type == MIDI_KEY) {
			DoTheMidi(desc->action, desc->type, 0);
		    }
		    break;
		case MIDI_CTRL:
		    if (desc->type == MIDI_KNOB) {
			// normalize value to range 0 - 100
			new = (val*100)/127;
			DoTheMidi(desc->action, desc->type, new);
		    } else if (desc->type == MIDI_WHEEL) {
			if (desc->delay > 0) {
			  clock_gettime(CLOCK_MONOTONIC, &tp);
			  delta=1000*(tp.tv_sec - last_wheel_tp.tv_sec);
			  delta += (tp.tv_nsec - last_wheel_tp.tv_nsec)/1000000;
			  if (delta < desc->delay) break;
			  last_wheel_tp = tp;
			}
			// translate value to direction
			if (val <= desc->low_thr1) new=-1;
			if (val <= desc->low_thr2) new=-10;
			if (val <= desc->low_thr3) new=-100;
			if (val >= desc->up_thr1 ) new=1;
			if (val >= desc->up_thr2 ) new=10;
			if (val >= desc->up_thr3 ) new=100;
			DoTheMidi(desc->action, desc->type, new);
			last_wheel_action=desc->action;
		    }
		    break;
		case MIDI_PITCH:
		    if (desc->type == MIDI_KNOB) {
			// normalize value to 0 - 100
			new = (val*100)/16383;
			DoTheMidi(desc->action, desc->type, new);
		    }
		    break;
		case EVENT_NONE:
		    break;
	    }
	    break;
	} else {
	    desc=desc->next;
	}
    }
    if (!desc) {
      // Nothing found. This is nothing to worry about, but log the key to stderr
      if (event == MIDI_PITCH) fprintf(stderr, "Unassigned PitchBend Value=%d\n", val);
      if (event == MIDI_NOTE ) fprintf(stderr, "Unassigned Key Note=%d Val=%d\n", note, val);
      if (event == MIDI_CTRL ) fprintf(stderr, "Unassigned Controller Ctl=%d Val=%d\n", note, val);
    }
}

/*
 * This data structre connects names as used in the midi.inp file with
 * our MIDIaction enum values
 */

static struct {
  enum MIDIaction action;
  const char *str;
} ActionTable[] = {
        { AF_GAIN,      "AFGAIN"},
        { AGC,          "AGC"},
	{ AGCATTACK,   	"AGCATTACK"},
        { ATT,          "ATT"},
        { BAND_DOWN,    "BANDDOWN"},
        { BAND_UP,      "BANDUP"},
        { COMPRESS,     "COMPRESS"},
	{ CTUN,  	"CTUN"},
        { FILTER_DOWN,  "FILTERDOWN"},
        { FILTER_UP,    "FILTERUP"},
	{ LOCK,    	"LOCK"},
        { MIC_VOLUME,   "MICGAIN"},
	{ MODE_DOWN,	"MODEDOWN"},
	{ MODE_UP,	"MODEUP"},
        { MOX,          "MOX"},
	{ NB,    	"NOISEBLANKER"},
	{ NR,    	"NOISEREDUCTION"},
        { PAN_HIGH,     "PANHIGH"},
        { PAN_LOW,      "PANLOW"},
        { PRE,          "PREAMP"},
	{ PS,    	"PURESIGNAL"},
        { RIT_CLEAR,    "RITCLEAR"},
        { RIT_VAL,      "RITVAL"},
	{ SPLIT,  	"SPLIT"},
	{ SWAP_VFO,	"SWAPVFO"},
        { TUNE,         "TUNE"},
        { TX_DRIVE,     "RFPOWER"},
        { VFO,          "VFO"},
	{ VFO_A2B,	"VFOA2B"},
	{ VFO_B2A,	"VFOB2A"},
	{ VOX,   	"VOX"},
        { ACTION_NONE,  "NONE"},
        { ACTION_NONE,  NULL}
};

/*
 * Translation from keyword in midi.inp file to MIDIaction
 */

static enum MIDIaction keyword2action(char *s) {
    int i=0;

    for (i=0; 1; i++) {
	if (ActionTable[i].str == NULL) {
	  fprintf(stderr,"MIDI: action keyword %s NOT FOUND.\n", s);
	  return ACTION_NONE;
	}
	if (!strcmp(s, ActionTable[i].str)) return ActionTable[i].action;
   }
   /* NOTREACHED */
}

/*
 * Here we read in a MIDI description file "midi.def" and fill the MidiCommandsTable
 * data structure
 */

void MIDIstartup() {
    FILE *fpin;
    char zeile[255];
    char *cp,*cq;
    int key;
    enum MIDIaction action;
    int chan;
    int is_wheel;
    int lt3,lt2,lt1,ut1,ut2,ut3;
    int onoff, delay;
    struct desc *desc,*dp;
    enum MIDItype type;
    enum MIDIevent event;
    int i;

    for (i=0; i<128; i++) MidiCommandsTable.desc[i]=NULL;
    MidiCommandsTable.pitch=NULL;

    fpin=fopen("midi.inp", "r");
    if (!fpin) return;

    for (;;) {
      if (fgets(zeile, 255, fpin) == NULL) break;

      // ignore comments
      cp=index(zeile,'#');
      if (cp == zeile) continue;   // comment line
      if (cp) *cp=0;               // ignore trailing comment

      if ((cp = strstr(zeile, "DEVICE="))) {
        // Delete comments and trailing blanks
	cq=cp+7;
	while (*cq != 0 && *cq != '#') cq++;
	*cq--=0;
	while (cq > cp+7 && (*cq == ' ' || *cq == '\t')) cq--;
	*cq=0;
	register_midi_device(cp+7);
        continue; // nothing more in this line
      }
      chan=-1;  // default: any channel
      lt3=lt2=lt1=-1;
      ut3=ut2=ut1=128;
      onoff=0;
      event=EVENT_NONE;
      type=TYPE_NONE;
      key=0;
      delay=0;

      if ((cp = strstr(zeile, "KEY="))) {
        sscanf(cp+4, "%d", &key);
        event=MIDI_NOTE;
	type=MIDI_KEY;
      }
      if ((cp = strstr(zeile, "CTRL="))) {
        sscanf(cp+5, "%d", &key);
	event=MIDI_CTRL;
	type=MIDI_KNOB;
      }
      if ((cp = strstr(zeile, "PITCH "))) {
        event=MIDI_PITCH;
	type=MIDI_KNOB;
      }
      if ((cp = strstr(zeile, "CHAN="))) {
        sscanf(cp+5, "%d", &chan);
	chan--;
        if (chan<0 || chan>15) chan=-1;
      }
      if ((cp = strstr(zeile, "WHEEL"))) {
	// change type from MIDI_KNOB to MIDI_WHEEL
        type=MIDI_WHEEL;
      }
      if ((cp = strstr(zeile, "ONOFF"))) {
        onoff=1;
      }
      if ((cp = strstr(zeile, "DELAY="))) {
        sscanf(cp+6, "%d", &delay);
      }
      if ((cp = strstr(zeile, "THR="))) {
        sscanf(cp+4, "%d %d %d %d %d %d", &lt3, &lt2, &lt1, &ut1, &ut2, &ut3);
        is_wheel=1;
      }
      if ((cp = strstr(zeile, "ACTION="))) {
        // cut zeile at the first blank character following
        cq=cp+7;
        while (*cq != 0 && *cq != '\n' && *cq != ' ' && *cq != '\t') cq++;
	*cq=0;
        action=keyword2action(cp+7);
      }
      if (event == EVENT_NONE || type == TYPE_NONE || key < 0 || key > 127) continue;
      // Now all entries of the line have been read. Construct descriptor
//fprintf(stderr,"K=%d C=%d T=%d E=%d A=%d OnOff=%d THRs=%d %d %d %d %d %d\n", key,chan,type, event, action, onoff, lt3,lt2,lt1,ut1,ut2,ut3);
      desc = (struct desc *) malloc(sizeof(struct desc));
      desc->next = NULL;
      desc->action = action;
      desc->type = type;
      desc->event = event;
      desc->onoff = onoff;
      desc->delay = delay;
      desc->low_thr3 = lt3;
      desc->low_thr2 = lt2;
      desc->low_thr1 = lt1;
      desc->up_thr1  = ut1;
      desc->up_thr2  = ut2;
      desc->up_thr3  = ut3;
      desc->channel  = chan;
      // insert descriptor
      if (event == MIDI_PITCH) {
	dp = MidiCommandsTable.pitch;
	if (dp == NULL) {
	  MidiCommandsTable.pitch = desc;
	} else {
	  while (dp->next != NULL) dp=dp->next;
	  dp->next=desc;
	}
      }
      if (event == MIDI_KEY || event == MIDI_CTRL) {
	dp = MidiCommandsTable.desc[key];
	if (dp == NULL) {
	  MidiCommandsTable.desc[key]=desc;
	} else {
	  while (dp->next != NULL) dp=dp->next;
	  dp->next=desc;
	}
      }
    }
}
