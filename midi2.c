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
			fprintf(stderr,"PITCH calc: val=%d new=%d\n", val,new);
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
}

/*
 * This data structre connects names as used in the midi.inp file with
 * our MIDIaction enum values
 */

static struct {
  enum MIDIaction action;
  const char *str;
} ActionTable[] = {
        { VFO,          "VFO"},
        { TUNE,         "TUNE"},
        { MOX,          "MOX"},
        { AF_GAIN,      "AFGAIN"},
        { MIC_VOLUME,   "MICGAIN"},
        { TX_DRIVE,     "RFPOWER"},
        { ATT,          "ATT"},
        { PRE,          "PREAMP"},
        { AGC,          "AGC"},
        { COMPRESS,     "COMPRESS"},
        { RIT_ONOFF,    "RITTOGGLE"},
        { RIT_VAL,      "RITVAL"},
        { PAN_HIGH,     "PANHIGH"},
        { PAN_LOW,      "PANLOW"},
        { BAND_UP,      "BANDUP"},
        { BAND_DOWN,    "BANDDOWN"},
        { FILTER_UP,    "FILTERUP"},
        { FILTER_DOWN,  "FILTERDOWN"},
	{ MODE_UP,	"MODEUP"},
	{ MODE_DOWN,	"MODEDOWN"},
	{ SWAP_VFO,	"SWAPVFO"},
        { ACTION_NONE,  NULL}
};

/*
 * Translation from keyword in midi.inp file to MIDIaction
 */

static enum MIDIaction keyword2action(char *s) {
    int i=0;

    for (i=0; 1; i++) {
	if (ActionTable[i].str == NULL) return ACTION_NONE;
	if (!strcmp(s, ActionTable[i].str)) return ActionTable[i].action;
   }
   /* NOTREACHED */
}

/*
 * Here we read in a MIDI description file "midi.def" and fill the MidiCommandsTable
 * data structure
 */

void MIDIstartup() {
    FILE *fpin,*fpout;
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
    fpout=stderr;
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
      lt3=lt2=lt1=0;
      ut3=ut2=ut1=127;
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
      fprintf(fpout,"K=%d C=%d T=%d E=%d A=%d OnOff=%d THRs=%d %d %d %d %d %d\n",
	 key,chan,type, event, action, onoff, lt3,lt2,lt1,ut1,ut2,ut3);
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
