/*
 * Layer-2 of MIDI support
 *
 * (C) Christoph van Wullen, DL1YCF
 *
 * Using the data in MIDICommandsTable, this subroutine translates the low-level
 * MIDI events into MIDI actions in the SDR console.
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef __APPLE__
#include "MacOS.h"  // emulate clock_gettime on old MacOS systems
#endif

#include "midi.h"

struct cmdtable MidiCommandsTable;

void NewMidiEvent(enum MIDIevent event, int channel, int note, int val) {

    struct desc *desc;
    int new;
    static enum MIDIaction last_wheel_action=MIDI_ACTION_NONE ;
    static struct timespec tp, last_wheel_tp={0,0};
    long delta;
    struct timespec ts;  // used in debug code
    double now;          // used in debug code

//Un-comment the next three lines to get a log of incoming MIDI events with milli-second time stamps
//clock_gettime(CLOCK_MONOTONIC, &ts);
//now=ts.tv_sec + 1E-9*ts.tv_nsec;
//g_print("%s:%12.3f:EVENT=%d CHAN=%d NOTE=%d VAL=%d\n",__FUNCTION__,now,event,channel,note,val);
    if (event == MIDI_EVENT_PITCH) {
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
		case MIDI_EVENT_NOTE:
		    if ((val == 1 || (desc->onoff == 1)) && desc->type == MIDI_TYPE_KEY) {
			DoTheMidi(desc->action, desc->type, val);
		    }
		    break;
		case MIDI_EVENT_CTRL:
		    if (desc->type == MIDI_TYPE_KNOB) {
			// normalize value to range 0 - 100
			new = (val*100)/127;
			DoTheMidi(desc->action, desc->type, new);
		    } else if (desc->type == MIDI_TYPE_WHEEL) {
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
		case MIDI_EVENT_PITCH:
		    if (desc->type == MIDI_TYPE_KNOB) {
			// normalize value to 0 - 100
			new = (val*100)/16383;
			DoTheMidi(desc->action, desc->type, new);
		    }
		    break;
		case MIDI_EVENT_NONE:
		    break;
	    }
	    break;
	} else {
	    desc=desc->next;
	}
    }
    if (!desc) {
      // Nothing found. This is nothing to worry about, but log the key to stderr
      if (event == MIDI_EVENT_PITCH) fprintf(stderr, "Unassigned PitchBend Value=%d\n", val);
      if (event == MIDI_EVENT_NOTE ) fprintf(stderr, "Unassigned Key Note=%d Val=%d\n", note, val);
      if (event == MIDI_EVENT_CTRL ) fprintf(stderr, "Unassigned Controller Ctl=%d Val=%d\n", note, val);
    }
}

/*
 * This data structre connects names as used in the midi.props file with
 * our MIDIaction enum values.
 * Take care that no key word is contained in another one!
 * Example: use "CURRVFO" not "VFO" otherwise there is possibly
 * a match for "VFO" when the key word is "VFOA".
 */

static struct {
  enum MIDIaction action;
  const char *str;
} ActionTable[] = {
	{ MIDI_ACTION_VFO_A2B,		"A2B"},
        { MIDI_ACTION_AF_GAIN,      	"AFGAIN"},
	{ MIDI_ACTION_AGCATTACK,   	"AGCATTACK"},
        { MIDI_ACTION_AGC,     		"AGCVAL"},
        { MIDI_ACTION_ANF,     		"ANF"},
        { MIDI_ACTION_ATT,          	"ATT"},
	{ MIDI_ACTION_VFO_B2A,		"B2A"},
        { MIDI_ACTION_BAND_DOWN,    	"BANDDOWN"},
        { MIDI_ACTION_BAND_UP,      	"BANDUP"},
        { MIDI_ACTION_COMPRESS,     	"COMPRESS"},
	{ MIDI_ACTION_CTUN,  		"CTUN"},
	{ MIDI_ACTION_VFO,		"CURRVFO"},
	{ MIDI_ACTION_CWL,		"CWL"},
	{ MIDI_ACTION_CWR,		"CWR"},
	{ MIDI_ACTION_CWSPEED,		"CWSPEED"},
	{ MIDI_ACTION_DIV_COARSEGAIN,	"DIVCOARSEGAIN"},
	{ MIDI_ACTION_DIV_COARSEPHASE,	"DIVCOARSEPHASE"},
	{ MIDI_ACTION_DIV_FINEGAIN,	"DIVFINEGAIN"},
	{ MIDI_ACTION_DIV_FINEPHASE,	"DIVFINEPHASE"},
	{ MIDI_ACTION_DIV_GAIN,		"DIVGAIN"},
	{ MIDI_ACTION_DIV_PHASE,	"DIVPHASE"},
	{ MIDI_ACTION_DIV_TOGGLE,	"DIVTOGGLE"},
	{ MIDI_ACTION_DUP,  		"DUP"},
        { MIDI_ACTION_FILTER_DOWN,  	"FILTERDOWN"},
        { MIDI_ACTION_FILTER_UP,    	"FILTERUP"},
	{ MIDI_ACTION_LOCK,    		"LOCK"},
        { MIDI_ACTION_MIC_VOLUME,   	"MICGAIN"},
	{ MIDI_ACTION_MODE_DOWN,	"MODEDOWN"},
	{ MIDI_ACTION_MODE_UP,		"MODEUP"},
        { MIDI_ACTION_MOX,     		"MOX"},
	{ MIDI_ACTION_MUTE,		"MUTE"},
	{ MIDI_ACTION_NB,    		"NOISEBLANKER"},
	{ MIDI_ACTION_NR,    		"NOISEREDUCTION"},
        { MIDI_ACTION_PAN,		"PAN"},
        { MIDI_ACTION_PAN_HIGH,     	"PANHIGH"},
        { MIDI_ACTION_PAN_LOW,      	"PANLOW"},
        { MIDI_ACTION_PRE,          	"PREAMP"},
	{ MIDI_ACTION_PTTONOFF,		"PTT"},
	{ MIDI_ACTION_PS,    		"PURESIGNAL"},
        { MIDI_ACTION_MEM_RECALL_M0,	"RECALLM0"},
        { MIDI_ACTION_MEM_RECALL_M1,	"RECALLM1"},
        { MIDI_ACTION_MEM_RECALL_M2,	"RECALLM2"},
        { MIDI_ACTION_MEM_RECALL_M3,	"RECALLM3"},
        { MIDI_ACTION_MEM_RECALL_M4,	"RECALLM4"},
	{ MIDI_ACTION_RF_GAIN, 		"RFGAIN"},
        { MIDI_ACTION_TX_DRIVE,     	"RFPOWER"},
	{ MIDI_ACTION_RIT_CLEAR,	"RITCLEAR"},
	{ MIDI_ACTION_RIT_STEP, 	"RITSTEP"},
        { MIDI_ACTION_RIT_TOGGLE,   	"RITTOGGLE"},
        { MIDI_ACTION_RIT_VAL,      	"RITVAL"},
        { MIDI_ACTION_SAT,     		"SAT"},
        { MIDI_ACTION_SNB, 		"SNB"},
	{ MIDI_ACTION_SPLIT,  		"SPLIT"},
        { MIDI_ACTION_MEM_STORE_M0,     "STOREM0"},
        { MIDI_ACTION_MEM_STORE_M1,     "STOREM1"},
        { MIDI_ACTION_MEM_STORE_M2,     "STOREM2"},
        { MIDI_ACTION_MEM_STORE_M3,     "STOREM3"},
        { MIDI_ACTION_MEM_STORE_M4,     "STOREM4"},
	{ MIDI_ACTION_SWAP_RX,		"SWAPRX"},
	{ MIDI_ACTION_SWAP_VFO,		"SWAPVFO"},
        { MIDI_ACTION_TUNE,    		"TUNE"},
        { MIDI_ACTION_VFOA,         	"VFOA"},
        { MIDI_ACTION_VFOB,         	"VFOB"},
	{ MIDI_ACTION_VFO_STEP_UP,  	"VFOSTEPUP"},
	{ MIDI_ACTION_VFO_STEP_DOWN,	"VFOSTEPDOWN"},
	{ MIDI_ACTION_VOX,   		"VOX"},
	{ MIDI_ACTION_VOXLEVEL,   	"VOXLEVEL"},
	{ MIDI_ACTION_XIT_CLEAR,  	"XITCLEAR"},
	{ MIDI_ACTION_XIT_VAL,  	"XITVAL"},
	{ MIDI_ACTION_ZOOM,		"ZOOM"},
	{ MIDI_ACTION_ZOOM_UP,		"ZOOMUP"},
	{ MIDI_ACTION_ZOOM_DOWN,	"ZOOMDOWN"},
        { MIDI_ACTION_NONE,  		"NONE"}
};

/*
 * Translation from keyword in midi.props file to MIDIaction
 */

static enum MIDIaction keyword2action(char *s) {
    int i=0;

    for (i=0; i< (sizeof(ActionTable) / sizeof(ActionTable[0])); i++) {
	if (!strcmp(s, ActionTable[i].str)) return ActionTable[i].action;
    }
    fprintf(stderr,"MIDI: action keyword %s NOT FOUND.\n", s);
    return MIDI_ACTION_NONE;
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

    fpin=fopen("midi.props", "r");
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
      event=MIDI_EVENT_NONE;
      type=MIDI_TYPE_NONE;
      key=0;
      delay=0;

      //
      // The KEY=, CTRL=, and PITCH= cases are mutually exclusive
      // If more than one keyword is in the line, PITCH wins over CTRL
      // wins over KEY.
      //
      if ((cp = strstr(zeile, "KEY="))) {
        sscanf(cp+4, "%d", &key);
        event=MIDI_EVENT_NOTE;
	type=MIDI_TYPE_KEY;
//fprintf(stderr,"MIDI:KEY:%d\n", key);
      }
      if ((cp = strstr(zeile, "CTRL="))) {
        sscanf(cp+5, "%d", &key);
	event=MIDI_EVENT_CTRL;
	type=MIDI_TYPE_KNOB;
//fprintf(stderr,"MIDI:CTL:%d\n", key);
      }
      if ((cp = strstr(zeile, "PITCH "))) {
        event=MIDI_EVENT_PITCH;
	type=MIDI_TYPE_KNOB;
//fprintf(stderr,"MIDI:PITCH\n");
      }
      //
      // If event is still undefined, skip line
      //
      if (event == MIDI_EVENT_NONE) {
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
      if ((cp = strstr(zeile, "WHEEL")) && (type == MIDI_TYPE_KNOB)) {
	// change type from MIDI_TYPE_KNOB to MIDI_TYPE_WHEEL
        type=MIDI_TYPE_WHEEL;
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
      if (event == MIDI_EVENT_PITCH) {
//fprintf(stderr,"MIDI:TAB:Insert desc=%p in PITCH table\n",desc);
	dp = MidiCommandsTable.pitch;
	if (dp == NULL) {
	  MidiCommandsTable.pitch = desc;
	} else {
	  while (dp->next != NULL) dp=dp->next;
	  dp->next=desc;
	}
      }
      if (event == MIDI_EVENT_NOTE || event == MIDI_EVENT_CTRL) {
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
