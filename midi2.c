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
			if ((val >= desc->vfl1) && (val <= desc->vfl2)) new=-100;
			if ((val >= desc-> fl1) && (val <= desc-> fl2)) new=-10;
			if ((val >= desc->lft1) && (val <= desc->lft2)) new=-1;
			if ((val >= desc->rgt1) && (val <= desc->rgt2)) new= 1;
			if ((val >= desc-> fr1) && (val <= desc-> fr2)) new= 10;
			if ((val >= desc->vfr1) && (val <= desc->vfr2)) new= 100;
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
        { MIDI_AGC,     "AGC"},
	{ AGCATTACK,   	"AGCATTACK"},
        { ATT,          "ATT"},
        { BAND_DOWN,    "BANDDOWN"},
        { BAND_UP,      "BANDUP"},
        { COMPRESS,     "COMPRESS"},
	{ MIDI_CTUN,  	"CTUN"},
        { FILTER_DOWN,  "FILTERDOWN"},
        { FILTER_UP,    "FILTERUP"},
	{ MIDI_LOCK,    "LOCK"},
        { MIC_VOLUME,   "MICGAIN"},
	{ MODE_DOWN,	"MODEDOWN"},
	{ MODE_UP,	"MODEUP"},
        { MIDI_MOX,     "MOX"},
	{ MIDI_NB,    	"NOISEBLANKER"},
	{ MIDI_NR,    	"NOISEREDUCTION"},
        { PAN_HIGH,     "PANHIGH"},
        { PAN_LOW,      "PANLOW"},
        { PRE,          "PREAMP"},
	{ MIDI_PS,    	"PURESIGNAL"},
        { MIDI_RIT_CLEAR,"RITCLEAR"},
        { RIT_VAL,      "RITVAL"},
	{ MIDI_SPLIT,  	"SPLIT"},
	{ SWAP_VFO,	"SWAPVFO"},
        { MIDI_TUNE,    "TUNE"},
        { TX_DRIVE,     "RFPOWER"},
        { VFO,          "VFO"},
        { VFOA,         "VFOA"},
        { VFOB,         "VFOB"},
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
    int swap_lr;
    int t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12;
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
      t1=t3=t5=t7= t9=t11=128;  // range that never occurs
      t2=t4=t6=t8=t10=t12=-1;   // range that never occurs
      onoff=0;
      swap_lr=0;
      event=EVENT_NONE;
      type=TYPE_NONE;
      key=0;
      delay=0;

      //
      // The KEY=, CTRL=, and PITCH= cases are mutually exclusive
      // If more than one keyword is in the line, PITCH wins over CTRL
      // wins over KEY.
      //
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
      //
      // If event is still undefined, skip line
      //
      if (event == EVENT_NONE) continue;

      //
      // beware of illegal key values
      //
      if (key < 0  ) key=0;
      if (key > 127) key=127;

      if ((cp = strstr(zeile, "CHAN="))) {
        sscanf(cp+5, "%d", &chan);
	chan--;
        if (chan<0 || chan>15) chan=-1;
      }
      if ((cp = strstr(zeile, "WHEEL")) && (type == MIDI_KNOB)) {
	// change type from MIDI_KNOB to MIDI_WHEEL
        type=MIDI_WHEEL;
      }
      if ((cp = strstr(zeile, "LEFTRIGHT"))) {
	swap_lr=1;
      }
      if ((cp = strstr(zeile, "ONOFF"))) {
        onoff=1;
      }
      if ((cp = strstr(zeile, "DELAY="))) {
        sscanf(cp+6, "%d", &delay);
      }
      if ((cp = strstr(zeile, "THR="))) {
        sscanf(cp+4, "%d %d %d %d %d %d %d %d %d %d %d %d",
               &t1,&t2,&t3,&t4,&t5,&t6,&t7,&t8,&t9,&t10,&t11,&t12);
      }
      if ((cp = strstr(zeile, "ACTION="))) {
        // cut zeile at the first blank character following
        cq=cp+7;
        while (*cq != 0 && *cq != '\n' && *cq != ' ' && *cq != '\t') cq++;
	*cq=0;
        action=keyword2action(cp+7);
      }
#if 0
  fprintf(stderr,"K=%d C=%d T=%d E=%d A=%d OnOff=%d\n",key,chan,type, event, action, onoff);
  if (t1 <= t2 ) fprintf(stderr,"Range for very fast  left: %d -- %d\n",t1,t2);
  if (t3 <= t4 ) fprintf(stderr,"Range for      fast  left: %d -- %d\n",t3,t4);
  if (t5 <= t6 ) fprintf(stderr,"Range for    normal  left: %d -- %d\n",t5,t6);
  if (t7 <= t8 ) fprintf(stderr,"Range for    normal right: %d -- %d\n",t7,t8);
  if (t9 <= t10) fprintf(stderr,"Range for      fast right: %d -- %d\n",t9,t10);
  if (t11<= t12) fprintf(stderr,"Range for very fast right: %d -- %d\n",t11,t12);
#endif
      //
      // All data for a descriptor has been read. Construct it!
      //
      desc = (struct desc *) malloc(sizeof(struct desc));
      desc->next = NULL;
      desc->action = action;
      desc->type = type;
      desc->event = event;
      desc->onoff = onoff;
      desc->delay = delay;
      desc->vfl1  = t1;
      desc->vfl2  = t2;
      desc->fl1   = t3;
      desc->fl2   = t4;
      desc->lft1  = t5;
      desc->lft2  = t6;
      desc->rgt1  = t7;
      desc->rgt2  = t8;
      desc->fr1   = t9;
      desc->fr2   = t10;
      desc->vfr1  = t11;
      desc->vfr2  = t12;
      desc->channel  = chan;
      //
      // insert descriptor into linked list.
      // We have a linked list for each key value to speed up searches
      //
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
