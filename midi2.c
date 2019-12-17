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
    static enum MIDIaction last_wheel_action=ACTION_NONE ;
    static struct timespec tp, last_wheel_tp={0,0};
    long delta;

//fprintf(stderr,"MIDI:EVENT=%d CHAN=%d NOTE=%d VAL=%d\n",event,channel,note,val);
    if (event == MIDI_PITCH) {
	desc=MidiCommandsTable.pitch;
    } else {
	desc=MidiCommandsTable.desc[note];
    }
//fprintf(stderr,"MIDI:init DESC=%p\n",desc);
    while (desc) {
//fprintf(stderr,"DESC=%p next=%p CHAN=%d EVENT=%d\n", desc,desc->next,desc->channel,desc->event);
	if ((desc->channel == channel || desc->channel == -1) && (desc->event == event)) {
	    // Found matching entry
	    switch (desc->event) {
		case MIDI_NOTE:
		    if ((val == 1 || (desc->onoff == 1)) && desc->type == MIDI_KEY) {
			DoTheMidi(desc->action, desc->type, val);
		    }
		    break;
		case MIDI_CTRL:
		    if (desc->type == MIDI_KNOB) {
			// normalize value to range 0 - 100
			new = (val*100)/127;
			DoTheMidi(desc->action, desc->type, new);
		    } else if (desc->type == MIDI_WHEEL) {
			if (desc->delay > 0 && last_wheel_action == desc->action) {
			  clock_gettime(CLOCK_MONOTONIC, &tp);
			  delta=1000*(tp.tv_sec - last_wheel_tp.tv_sec);
			  delta += (tp.tv_nsec - last_wheel_tp.tv_nsec)/1000000;
			  if (delta < desc->delay) break;
			  last_wheel_tp = tp;
			}
			// translate value to direction
			new=0;
			if ((val >= desc->vfl1) && (val <= desc->vfl2)) new=-100;
			if ((val >= desc-> fl1) && (val <= desc-> fl2)) new=-10;
			if ((val >= desc->lft1) && (val <= desc->lft2)) new=-1;
			if ((val >= desc->rgt1) && (val <= desc->rgt2)) new= 1;
			if ((val >= desc-> fr1) && (val <= desc-> fr2)) new= 10;
			if ((val >= desc->vfr1) && (val <= desc->vfr2)) new= 100;
//			fprintf(stderr,"WHEEL: val=%d new=%d thrs=%d/%d, %d/%d, %d/%d, %d/%d, %d/%d, %d/%d\n",
//                                  val, new, desc->vfl1, desc->vfl2, desc->fl1, desc->fl2, desc->lft1, desc->lft2,
//				          desc->rgt1, desc->rgt2, desc->fr1, desc->fr2, desc->vfr1, desc->vfr2);
			if (new != 0) DoTheMidi(desc->action, desc->type, new);
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
 * our MIDIaction enum values.
 * Take care that no key word is contained in another one!
 * Example: use "CURRVFO" not "VFO" otherwise there is possibly
 * a match for "VFO" when the key word is "VFOA".
 */

static struct {
  enum MIDIaction action;
  const char *str;
} ActionTable[] = {
	{ VFO_A2B,		"A2B"},
        { AF_GAIN,      	"AFGAIN"},
	{ AGCATTACK,   		"AGCATTACK"},
        { MIDI_AGC,     	"AGCVAL"},
        { ATT,          	"ATT"},
	{ VFO_B2A,		"B2A"},
        { BAND_DOWN,    	"BANDDOWN"},
        { BAND_UP,      	"BANDUP"},
        { COMPRESS,     	"COMPRESS"},
	{ MIDI_CTUN,  		"CTUN"},
	{ VFO,			"CURRVFO"},
	{ CWL,			"CWL"},
	{ CWR,			"CWR"},
	{ CWSPEED,		"CWSPEED"},
	{ MIDI_DUP,  		"DUP"},
        { FILTER_DOWN,  	"FILTERDOWN"},
        { FILTER_UP,    	"FILTERUP"},
	{ MIDI_LOCK,    	"LOCK"},
        { MIC_VOLUME,   	"MICGAIN"},
	{ MODE_DOWN,		"MODEDOWN"},
	{ MODE_UP,		"MODEUP"},
        { MIDI_MOX,     	"MOX"},
	{ MIDI_NB,    		"NOISEBLANKER"},
	{ MIDI_NR,    		"NOISEREDUCTION"},
        { PAN_HIGH,     	"PANHIGH"},
        { PAN_LOW,      	"PANLOW"},
        { PRE,          	"PREAMP"},
	{ MIDI_PS,    		"PURESIGNAL"},
	{ MIDI_RF_GAIN, 	"RFGAIN"},
        { TX_DRIVE,     	"RFPOWER"},
	{ MIDI_RIT_CLEAR,	"RITCLEAR"},
	{ RIT_STEP, 		"RITSTEP"},
        { RIT_TOGGLE,   	"RITTOGGLE"},
        { RIT_VAL,      	"RITVAL"},
        { MIDI_SAT,     	"SAT"},
	{ MIDI_SPLIT,  		"SPLIT"},
	{ SWAP_RX,		"SWAPRX"},
	{ SWAP_VFO,		"SWAPVFO"},
        { MIDI_TUNE,    	"TUNE"},
        { VFOA,         	"VFOA"},
        { VFOB,         	"VFOB"},
	{ VFO_STEP_UP,  	"VFOSTEPUP"},
	{ VFO_STEP_DOWN,	"VFOSTEPDOWN"},
	{ VOX,   		"VOX"},
	{ VOXLEVEL,   		"VOXLEVEL"},
	{ MIDI_XIT_CLEAR,  	"XITCLEAR"},
	{ XIT_VAL,  		"XITVAL"},
        { ACTION_NONE,  	"NONE"}
};

/*
 * Translation from keyword in midi.inp file to MIDIaction
 */

static enum MIDIaction keyword2action(char *s) {
    int i=0;

    for (i=0; i< (sizeof(ActionTable) / sizeof(ActionTable[0])); i++) {
	if (!strcmp(s, ActionTable[i].str)) return ActionTable[i].action;
    }
    fprintf(stderr,"MIDI: action keyword %s NOT FOUND.\n", s);
    return ACTION_NONE;
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
    int t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12;
    int onoff, delay;
    struct desc *desc,*dp;
    enum MIDItype type;
    enum MIDIevent event;
    int i;
    char c;

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

      // change newline, comma, slash etc. to blanks
      cp=zeile;
      while ((c=*cp)) {
        switch (c) {
	  case '\n':
	  case '\r':
	  case '\t':
	  case ',':
	  case '/':
	    *cp=' ';
	    break;
        }
	cp++;
      }
      
//fprintf(stderr,"\nMIDI:INP:%s\n",zeile);

      if ((cp = strstr(zeile, "DEVICE="))) {
        // Delete comments and trailing blanks
	cq=cp+7;
	while (*cq != 0 && *cq != '#') cq++;
	*cq--=0;
	while (cq > cp+7 && (*cq == ' ' || *cq == '\t')) cq--;
	*(cq+1)=0;
//fprintf(stderr,"MIDI:REG:>>>%s<<<\n",cp+7);
	register_midi_device(cp+7);
        continue; // nothing more in this line
      }
      chan=-1;  // default: any channel
      t1=t3=t5=t7= t9=t11=128;  // range that never occurs
      t2=t4=t6=t8=t10=t12=-1;   // range that never occurs
      onoff=0;
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
//fprintf(stderr,"MIDI:KEY:%d\n", key);
      }
      if ((cp = strstr(zeile, "CTRL="))) {
        sscanf(cp+5, "%d", &key);
	event=MIDI_CTRL;
	type=MIDI_KNOB;
//fprintf(stderr,"MIDI:CTL:%d\n", key);
      }
      if ((cp = strstr(zeile, "PITCH "))) {
        event=MIDI_PITCH;
	type=MIDI_KNOB;
//fprintf(stderr,"MIDI:PITCH\n");
      }
      //
      // If event is still undefined, skip line
      //
      if (event == EVENT_NONE) {
//fprintf(stderr,"MIDI:ERR:NO_EVENT\n");
	continue;
      }

      //
      // beware of illegal key values
      //
      if (key < 0  ) key=0;
      if (key > 127) key=127;

      if ((cp = strstr(zeile, "CHAN="))) {
        sscanf(cp+5, "%d", &chan);
	chan--;
        if (chan<0 || chan>15) chan=-1;
//fprintf(stderr,"MIDI:CHA:%d\n",chan);
      }
      if ((cp = strstr(zeile, "WHEEL")) && (type == MIDI_KNOB)) {
	// change type from MIDI_KNOB to MIDI_WHEEL
        type=MIDI_WHEEL;
//fprintf(stderr,"MIDI:WHEEL\n");
      }
      if ((cp = strstr(zeile, "ONOFF"))) {
        onoff=1;
//fprintf(stderr,"MIDI:ONOFF\n");
      }
      if ((cp = strstr(zeile, "DELAY="))) {
        sscanf(cp+6, "%d", &delay);
//fprintf(stderr,"MIDI:DELAY:%d\n",delay);
      }
      if ((cp = strstr(zeile, "THR="))) {
        sscanf(cp+4, "%d %d %d %d %d %d %d %d %d %d %d %d",
               &t1,&t2,&t3,&t4,&t5,&t6,&t7,&t8,&t9,&t10,&t11,&t12);
//fprintf(stderr,"MIDI:THR:%d/%d, %d/%d, %d/%d, %d/%d, %d/%d, %d/%d\n",t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12);
      }
      if ((cp = strstr(zeile, "ACTION="))) {
        // cut zeile at the first blank character following
        cq=cp+7;
        while (*cq != 0 && *cq != '\n' && *cq != ' ' && *cq != '\t') cq++;
	*cq=0;
        action=keyword2action(cp+7);
//fprintf(stderr,"MIDI:ACTION:%s (%d)\n",cp+7, action);
      }
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
//fprintf(stderr,"MIDI:TAB:Insert desc=%p in PITCH table\n",desc);
	dp = MidiCommandsTable.pitch;
	if (dp == NULL) {
	  MidiCommandsTable.pitch = desc;
	} else {
	  while (dp->next != NULL) dp=dp->next;
	  dp->next=desc;
	}
      }
      if (event == MIDI_KEY || event == MIDI_CTRL) {
//fprintf(stderr,"MIDI:TAB:Insert desc=%p in CMDS[%d] table\n",desc,key);
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
