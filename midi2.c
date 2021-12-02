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
#include "receiver.h"
#include "discovered.h"
#include "adc.h"
#include "dac.h"
#include "transmitter.h"
#include "radio.h"
#include "main.h"
#include "actions.h"
#include "midi.h"


struct cmdtable MidiCommandsTable;

void NewMidiEvent(enum MIDIevent event, int channel, int note, int val) {

    struct desc *desc;
    int new;
    static int last_wheel_action=NO_ACTION ;
    static struct timespec tp, last_wheel_tp={0,0};
    long delta;

g_print("%s: EVENT=%d CHAN=%d NOTE=%d VAL=%d\n",__FUNCTION__,event,channel,note,val);
    if (event == MIDI_PITCH) {
	desc=MidiCommandsTable.pitch;
    } else {
	desc=MidiCommandsTable.desc[note];
    }
//g_print("%s: init DESC=%p\n",__FUNCTION__,desc);
    while (desc) {
//g_print("%s: DESC=%p next=%p CHAN=%d EVENT=%d\n",__FUNCTION__,desc,desc->next,desc->channel,desc->event);
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
			new=val-64;
			/*
			if ((val >= desc->vfl1) && (val <= desc->vfl2)) new=-100;
			if ((val >= desc-> fl1) && (val <= desc-> fl2)) new=-10;
			if ((val >= desc->lft1) && (val <= desc->lft2)) new=-1;
			if ((val >= desc->rgt1) && (val <= desc->rgt2)) new= 1;
			if ((val >= desc-> fr1) && (val <= desc-> fr2)) new= 10;
			if ((val >= desc->vfr1) && (val <= desc->vfr2)) new= 100;
			*/
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
        if (event == MIDI_PITCH) g_print("%s: Unassigned PitchBend Value=%d\n",__FUNCTION__, val);
        if (event == MIDI_NOTE ) g_print("%s: Unassigned Key Note=%d Val=%d\n",__FUNCTION__, note, val);
        if (event == MIDI_CTRL ) g_print("%s: Unassigned Controller Ctl=%d Val=%d\n",__FUNCTION__, note, val);
    }
}

gchar *midi_types[] = {"NONE","KEY","KNOB/SLIDER","*INVALID*","WHEEL"};
gchar *midi_events[] = {"NONE","NOTE","CTRL","PITCH"};

//
// maintained so old midi configurations can be loaded
//
OLD_ACTION_TABLE OLD_ActionTable[] = {
	{ NO_ACTION,		"NONE",		TYPE_NONE},
	{ A_TO_B,		"A2B",		MIDI_KEY},
        { AF_GAIN,      	"AFGAIN",	MIDI_KNOB|MIDI_WHEEL},
	{ AGC,   		"AGCATTACK",	MIDI_KEY},
        { AGC_GAIN, 	    	"AGCVAL",	MIDI_KNOB|MIDI_WHEEL},
        { ANF,     		"ANF",		MIDI_KEY},
        { ATTENUATION, 		"ATT",		MIDI_KNOB|MIDI_WHEEL},
	{ B_TO_A,			"B2A",		MIDI_KEY},
	{ BAND_10,         "BAND10",	MIDI_KEY},
        { BAND_12,         "BAND12",	MIDI_KEY},
        { BAND_1240,       "BAND1240",	MIDI_KEY},
        { BAND_144,        "BAND144",	MIDI_KEY},
        { BAND_15,         "BAND15",	MIDI_KEY},
        { BAND_160,        "BAND160",	MIDI_KEY},
        { BAND_17,         "BAND17",	MIDI_KEY},
        { BAND_20,         "BAND20",	MIDI_KEY},
        { BAND_220,        "BAND220",	MIDI_KEY},
        { BAND_2300,       "BAND2300",	MIDI_KEY},
        { BAND_30,         "BAND30",	MIDI_KEY},
        { BAND_3400,       "BAND3400",	MIDI_KEY},
        { BAND_40,         "BAND40",	MIDI_KEY},
        { BAND_430,        "BAND430",	MIDI_KEY},
        { BAND_6,          "BAND6",	MIDI_KEY},
        { BAND_60,         "BAND60",	MIDI_KEY},
        { BAND_70,         "BAND70",	MIDI_KEY},
        { BAND_80,         "BAND80",	MIDI_KEY},
        { BAND_902,        "BAND902",	MIDI_KEY},
        { BAND_AIR,        "BANDAIR",	MIDI_KEY},
        { BAND_MINUS,      "BANDDOWN",	MIDI_KEY},
        { BAND_GEN,        "BANDGEN",	MIDI_KEY},
        { BAND_PLUS,       "BANDUP",	MIDI_KEY},
        { BAND_WWV,        "BANDWWV",	MIDI_KEY},
        { COMPRESSION,     	"COMPRESS",	MIDI_KEY},
	{ CTUN,  		"CTUN",		MIDI_KEY},
	{ VFO,			"CURRVFO",	MIDI_WHEEL},
	{ CW_LEFT,		"CWL",		MIDI_KEY},
	{ CW_RIGHT,		"CWR",		MIDI_KEY},
	{ CW_SPEED,		"CWSPEED",	MIDI_KNOB|MIDI_WHEEL},
	{ DIV_GAIN_COARSE,	"DIVCOARSEGAIN",	MIDI_KNOB|MIDI_WHEEL},
	{ DIV_PHASE_COARSE,	"DIVCOARSEPHASE",	MIDI_KNOB|MIDI_WHEEL},
	{ DIV_GAIN_FINE,	"DIVFINEGAIN",	MIDI_KNOB|MIDI_WHEEL},
	{ DIV_PHASE_FINE,	"DIVFINEPHASE",	MIDI_KNOB|MIDI_WHEEL},
	{ DIV_GAIN,		"DIVGAIN",	MIDI_KNOB|MIDI_WHEEL},
	{ DIV_PHASE,		"DIVPHASE",	MIDI_KNOB|MIDI_WHEEL},
	{ DIV,			"DIVTOGGLE",	MIDI_KEY},
	{ DUPLEX,  		"DUP",		MIDI_KEY},
        { FILTER_MINUS,  	"FILTERDOWN",	MIDI_KEY},
        { FILTER_PLUS,    	"FILTERUP",	MIDI_KEY},
	{ MENU_FILTER,		"MENU_FILTER",	MIDI_KEY},
	{ MENU_MODE,		"MENU_MODE",	MIDI_KEY},
	{ LOCK,	    		"LOCK",		MIDI_KEY},
        { MIC_GAIN,   		"MICGAIN",	MIDI_KNOB|MIDI_WHEEL},
	{ MODE_MINUS,		"MODEDOWN",	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL},
	{ MODE_PLUS,		"MODEUP",	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL},
        { MOX, 		    	"MOX",	MIDI_KEY},
	{ MUTE,			"MUTE",	MIDI_KEY},
	{ NB,    		"NOISEBLANKER",	MIDI_KEY},
	{ NR,    		"NOISEREDUCTION",	MIDI_KEY},
	{ NUMPAD_0,		"NUMPAD0",	MIDI_KEY},
	{ NUMPAD_1,		"NUMPAD1",	MIDI_KEY},
	{ NUMPAD_2,		"NUMPAD2",	MIDI_KEY},
	{ NUMPAD_3,		"NUMPAD3",	MIDI_KEY},
	{ NUMPAD_4,		"NUMPAD4",	MIDI_KEY},
	{ NUMPAD_5,		"NUMPAD5",	MIDI_KEY},
	{ NUMPAD_6,		"NUMPAD6",	MIDI_KEY},
	{ NUMPAD_7,		"NUMPAD7",	MIDI_KEY},
	{ NUMPAD_8,		"NUMPAD8",	MIDI_KEY},
	{ NUMPAD_9,		"NUMPAD9",	MIDI_KEY},
	{ NUMPAD_CL,		"NUMPADCL",	MIDI_KEY},
	{ NUMPAD_ENTER,		"NUMPADENTER",	MIDI_KEY},
        { PAN,			"PAN",	MIDI_KNOB|MIDI_WHEEL},
        { PANADAPTER_HIGH,     	"PANHIGH",	MIDI_KNOB|MIDI_WHEEL},
        { PANADAPTER_LOW,      	"PANLOW",	MIDI_KNOB|MIDI_WHEEL},
        { PREAMP,          	"PREAMP",	MIDI_KEY},
	{ PS,    		"PURESIGNAL",	MIDI_KEY},
	{ RF_GAIN,	 	"RFGAIN",	MIDI_KNOB|MIDI_WHEEL},
        { DRIVE, 	    	"RFPOWER",	MIDI_KNOB|MIDI_WHEEL},
	{ RIT_CLEAR,		"RITCLEAR",	MIDI_KEY},
	{ RIT_STEP, 		"RITSTEP",	MIDI_KNOB|MIDI_WHEEL},
        { RIT_ENABLE,   	"RITTOGGLE",	MIDI_KEY},
        { RIT, 	     	"RITVAL",	MIDI_KNOB|MIDI_WHEEL},
        { SAT,     		"SAT",	MIDI_KEY},
        { SNB, 		    	"SNB",	MIDI_KEY},
	{ SPLIT,  		"SPLIT",	MIDI_KEY},
	{ SWAP_RX,		"SWAPRX",	MIDI_KEY},
	{ A_SWAP_B,		"SWAPVFO",	MIDI_KEY},
        { TUNE, 	   	"TUNE",	MIDI_KEY},
        { VFOA,         	"VFOA",	MIDI_WHEEL},
        { VFOB,         	"VFOB",	MIDI_WHEEL},
	{ VFO_STEP_MINUS,	"VFOSTEPDOWN",	MIDI_KEY},
	{ VFO_STEP_PLUS,  	"VFOSTEPUP",	MIDI_KEY},
	{ VOX,   		"VOX",	MIDI_KEY},
	{ VOXLEVEL,   		"VOXLEVEL",	MIDI_KNOB|MIDI_WHEEL},
	{ XIT_CLEAR, 	 	"XITCLEAR",	MIDI_KEY},
	{ XIT,  		"XITVAL",	MIDI_KNOB|MIDI_WHEEL},
	{ ZOOM,			"ZOOM",	MIDI_KNOB|MIDI_WHEEL},
	{ ZOOM_MINUS,		"ZOOMDOWN",	MIDI_KEY},
	{ ZOOM_PLUS,		"ZOOMUP",	MIDI_KEY},
        { NO_ACTION,  	"NONE",	TYPE_NONE}
};

/*
 * Translation from keyword in midi.props file to MIDIaction
 */

static int keyword2action(char *s) {
    int i=0;

    for (i=0; i< (sizeof(OLD_ActionTable) / sizeof(OLD_ActionTable[0])); i++) {
	if (!strcmp(s, OLD_ActionTable[i].str)) return OLD_ActionTable[i].action;
    }
    fprintf(stderr,"MIDI: action keyword %s NOT FOUND.\n", s);
    return NO_ACTION;
}


int MIDIstop() {
  midi_enabled=FALSE;
  close_midi_device();
  return 0;
}

/*
 * Here we read in a MIDI description file and fill the MidiCommandsTable
 * data structure
 */

int MIDIstartup(char *filename) {
    FILE *fpin;
    char zeile[255];
    char *cp,*cq;
    int key;
    int action;
    int chan;
    int t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12;
    int onoff, delay;
    struct desc *desc,*dp;
    enum ACTIONtype type;
    enum MIDIevent event;
    int i;
    char c;

    for (i=0; i<128; i++) MidiCommandsTable.desc[i]=NULL;
    MidiCommandsTable.pitch=NULL;

    g_print("%s: %s\n",__FUNCTION__,filename);
    fpin=fopen(filename, "r");

    g_print("%s: fpin=%p\n",__FUNCTION__,fpin);
    if (!fpin) {
      g_print("%s: failed to open MIDI device\n",__FUNCTION__);
      return -1;
    }

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
      
g_print("\n%s:INP:%s\n",__FUNCTION__,zeile);

      if ((cp = strstr(zeile, "DEVICE="))) {
        // Delete comments and trailing blanks
	cq=cp+7;
	while (*cq != 0 && *cq != '#') cq++;
	*cq--=0;
	while (cq > cp+7 && (*cq == ' ' || *cq == '\t')) cq--;
	*(cq+1)=0;
//fprintf(stderr,"MIDI:REG:>>>%s<<<\n",cp+7);
	int result=register_midi_device(cp+7);
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
g_print("%s: MIDI:KEY:%d\n",__FUNCTION__, key);
      }
      if ((cp = strstr(zeile, "CTRL="))) {
        sscanf(cp+5, "%d", &key);
	event=MIDI_CTRL;
	type=MIDI_KNOB;
g_print("%s: MIDI:CTL:%d\n",__FUNCTION__, key);
      }
      if ((cp = strstr(zeile, "PITCH "))) {
        event=MIDI_PITCH;
	type=MIDI_KNOB;
g_print("%s: MIDI:PITCH\n",__FUNCTION__);
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
g_print("%s:CHAN:%d\n",__FUNCTION__,chan);
      }
      if ((cp = strstr(zeile, "WHEEL")) && (type == MIDI_KNOB)) {
	// change type from MIDI_KNOB to MIDI_WHEEL
        type=MIDI_WHEEL;
g_print("%s:WHEEL\n",__FUNCTION__);
      }
      if ((cp = strstr(zeile, "ONOFF"))) {
        onoff=1;
g_print("%s:ONOFF\n",__FUNCTION__);
      }
      if ((cp = strstr(zeile, "DELAY="))) {
        sscanf(cp+6, "%d", &delay);
g_print("%s:DELAY:%d\n",__FUNCTION__,delay);
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
g_print("%s: MIDI:ACTION:%s (%d)\n",__FUNCTION__,cp+7, action);
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
g_print("%s:TAB:Insert desc=%p in CMDS[%d] table\n",__FUNCTION__,desc,key);
	dp = MidiCommandsTable.desc[key];
	if (dp == NULL) {
	  MidiCommandsTable.desc[key]=desc;
	} else {
	  while (dp->next != NULL) dp=dp->next;
	  dp->next=desc;
	}
      }
    }

    return 0;
}
