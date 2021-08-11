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

#include "receiver.h"
#include "discovered.h"
#include "adc.h"
#include "dac.h"
#include "transmitter.h"
#include "radio.h"
#include "main.h"
#include "actions.h"
#include "midi.h"
#include "alsa_midi.h"

struct desc *MidiCommandsTable[129];

void NewMidiEvent(enum MIDIevent event, int channel, int note, int val) {

    struct desc *desc;
    int new;
    static int last_wheel_action=NO_ACTION ;
    static struct timespec tp, last_wheel_tp={0,0};
    long delta;
    struct timespec ts;  // used in debug code
    double now;          // used in debug code

//Un-comment next three lines to get a log of incoming MIDI messages
//clock_gettime(CLOCK_MONOTONIC, &ts);
//now=ts.tv_sec + 1E-9*ts.tv_nsec;
//g_print("%s:%12.3f:EVENT=%d CHAN=%d NOTE=%d VAL=%d\n",__FUNCTION__,now,event,channel,note,val);

    if (event == MIDI_PITCH) {
	desc=MidiCommandsTable[128];
    } else {
	desc=MidiCommandsTable[note];
    }
//g_print("%s: init DESC=%p\n",__FUNCTION__,desc);
    while (desc) {
//g_print("%s: DESC=%p next=%p CHAN=%d EVENT=%d\n",__FUNCTION__,desc,desc->next,desc->channel,desc->event);
	if ((desc->channel == channel || desc->channel == -1) && (desc->event == event)) {
	    // Found matching entry
	    switch (desc->event) {
		case MIDI_NOTE:
                    if (desc->type == MIDI_KEY) {
                      switch (desc->action) {
                        case CW_LEFT:
                        case CW_RIGHT:
                        case CW_KEYER:
                        case PTT:
                          // deliver message for note-on and note-off
                          DoTheMidi(desc->action, desc->type, val);
                          break;
                        default:
                          // deliver only note-on messages
                          if (val == 1) DoTheMidi(desc->action, desc->type, val);
                          break;
                      }
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
			// translate value to direction/speed
			new=0;
			if ((val >= desc->vfl1) && (val <= desc->vfl2)) new=-16;
			if ((val >= desc-> fl1) && (val <= desc-> fl2)) new=-4;
			if ((val >= desc->lft1) && (val <= desc->lft2)) new=-1;
			if ((val >= desc->rgt1) && (val <= desc->rgt2)) new= 1;
			if ((val >= desc-> fr1) && (val <= desc-> fr2)) new= 4;
			if ((val >= desc->vfr1) && (val <= desc->vfr2)) new= 16;
//			g_print("%s: WHEEL PARAMS: val=%d new=%d thrs=%d/%d, %d/%d, %d/%d, %d/%d, %d/%d, %d/%d\n",
//                               __FUNCTION__,
//                               val, new, desc->vfl1, desc->vfl2, desc->fl1, desc->fl2, desc->lft1, desc->lft2,
//				 desc->rgt1, desc->rgt2, desc->fr1, desc->fr2, desc->vfr1, desc->vfr2);
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
        if (event == MIDI_PITCH) g_print("Unassigned PitchBend Value=%d\n", val);
        if (event == MIDI_NOTE ) g_print("Unassigned Key Note=%d Val=%d\n", note, val);
        if (event == MIDI_CTRL ) g_print("Unassigned Controller Ctl=%d Val=%d\n", note, val);
    }
}

gchar *midi_types[] = {"NONE","KEY","KNOB/SLIDER","*INVALID*","WHEEL"};
gchar *midi_events[] = {"NONE","NOTE","CTRL","PITCH"};

int MIDIstop() {
  for (int i=0; i<n_midi_devices; i++) {
    if (midi_devices[i].active) {
      close_midi_device(i);
    }
  }
  return 0;
}

/*
 * Release data from MidiCommandsTable
 */

void MidiReleaseCommands() {
  int i;
  struct desc *loop, *new;
  for (i=0; i<129; i++) {
    loop = MidiCommandsTable[i];
    while (loop != NULL) {
      new=loop->next;
      free(loop);
      loop = new;
    }
    MidiCommandsTable[i]=NULL;
  }
}

/*
 * Add a command to MidiCommandsTable
 */

void MidiAddCommand(int note, struct desc *desc) {
  struct desc *loop;

  if (note < 0 || note > 128) return;

  //
  // Actions with channel == -1 (ANY) must go to the end of the list
  //
  if (MidiCommandsTable[note] == NULL) {
    // initialize linked list
    MidiCommandsTable[note]=desc;
  } else if (desc->channel >= 0) {
    // add to top of the list
    desc->next = MidiCommandsTable[note];
    MidiCommandsTable[note]=desc;
  } else {
    // add to tail of the list
    loop = MidiCommandsTable[note];
    while (loop->next != NULL) {
      loop = loop->next;
    }
    loop->next=desc;
  }
}

//
// maintained so old midi configurations can be loaded
// the sole purpose of this table is to map names in
// the midi.props file to actions
//
// THIS TABLE IS ONLY USED IN keyword2action()
//
typedef struct _old_mapping {
  enum ACTION action;
  const char *str;
} OLD_MAPPING;


static OLD_MAPPING OLD_Mapping[] = {
	{ NO_ACTION,		"NONE"			},
	{ A_TO_B,		"A2B"			},
        { AF_GAIN,      	"AFGAIN"		},
	{ AGC,   		"AGCATTACK"		},
        { AGC_GAIN, 		"AGCVAL"		},
        { ANF,     		"ANF"			},
        { ATTENUATION, 		"ATT"			},
	{ B_TO_A,		"B2A"			},
	{ BAND_10,      	"BAND10"		},
        { BAND_12,      	"BAND12"		},
        { BAND_1240,    	"BAND1240"		},
        { BAND_144,     	"BAND144"		},
        { BAND_15,      	"BAND15"		},
        { BAND_160,     	"BAND160"		},
        { BAND_17,      	"BAND17"		},
        { BAND_20,      	"BAND20"		},
        { BAND_220,     	"BAND220"		},
        { BAND_2300,    	"BAND2300"		},
        { BAND_30,      	"BAND30"		},
        { BAND_3400,    	"BAND3400"		},
        { BAND_40,      	"BAND40"		},
        { BAND_430,     	"BAND430"		},
        { BAND_6,       	"BAND6"			},
        { BAND_60,      	"BAND60"		},
        { BAND_70,      	"BAND70"		},
        { BAND_80,      	"BAND80"		},
        { BAND_902,     	"BAND902"		},
        { BAND_AIR,     	"BANDAIR"		},
        { BAND_MINUS,   	"BANDDOWN"		},
        { BAND_GEN,     	"BANDGEN"		},
        { BAND_PLUS,    	"BANDUP"		},
        { BAND_WWV,     	"BANDWWV"		},
        { COMPRESSION,  	"COMPRESS"		},
	{ CTUN,  		"CTUN"			},
	{ VFO,			"CURRVFO"		},
	{ CW_LEFT,		"CWL"			},
	{ CW_KEYER,		"CW(Keyer)"		},
	{ CW_RIGHT,		"CWR"			},
	{ CW_SPEED,		"CWSPEED"		},
	{ DIV_GAIN_COARSE,	"DIVCOARSEGAIN"		},
	{ DIV_PHASE_COARSE,	"DIVCOARSEPHASE"	},
	{ DIV_GAIN_FINE,	"DIVFINEGAIN"		},
	{ DIV_PHASE_FINE,	"DIVFINEPHASE"		},
	{ DIV_GAIN,		"DIVGAIN"		},
	{ DIV_PHASE,		"DIVPHASE"		},
	{ DIV,			"DIVTOGGLE"		},
	{ DUPLEX,  		"DUP"			},
        { FILTER_MINUS,  	"FILTERDOWN"		},
        { FILTER_PLUS,    	"FILTERUP"		},
	{ MENU_FILTER,		"MENU_FILTER"		},
	{ MENU_MODE,		"MENU_MODE"		},
	{ LOCK,	    		"LOCK"			},
        { MIC_GAIN,   		"MICGAIN"		},
	{ MODE_MINUS,		"MODEDOWN"		},
	{ MODE_PLUS,		"MODEUP"		},
        { MOX, 		    	"MOX"			},
	{ MUTE,			"MUTE"			},
	{ NB,    		"NOISEBLANKER"		},
	{ NR,    		"NOISEREDUCTION"	},
	{ NUMPAD_0,		"NUMPAD0"		},
	{ NUMPAD_1,		"NUMPAD1"		},
	{ NUMPAD_2,		"NUMPAD2"		},
	{ NUMPAD_3,		"NUMPAD3"		},
	{ NUMPAD_4,		"NUMPAD4"		},
	{ NUMPAD_5,		"NUMPAD5"		},
	{ NUMPAD_6,		"NUMPAD6"		},
	{ NUMPAD_7,		"NUMPAD7"		},
	{ NUMPAD_8,		"NUMPAD8"		},
	{ NUMPAD_9,		"NUMPAD9"		},
	{ NUMPAD_CL,		"NUMPADCL"		},
	{ NUMPAD_ENTER,		"NUMPADENTER"		},
        { PAN,			"PAN"			},
        { PANADAPTER_HIGH,     	"PANHIGH"		},
        { PANADAPTER_LOW,      	"PANLOW"		},
        { PREAMP,          	"PREAMP"		},
	{ PTT,    		"PTT"			},
	{ PS,    		"PURESIGNAL"		},
	{ RF_GAIN,	 	"RFGAIN"		},
        { DRIVE, 	    	"RFPOWER"		},
	{ RIT_CLEAR,		"RITCLEAR"		},
	{ RIT_STEP, 		"RITSTEP"		},
        { RIT_ENABLE,   	"RITTOGGLE"		},
        { RIT, 	     		"RITVAL"		},
        { SAT,     		"SAT"			},
        { SNB, 		    	"SNB"			},
	{ SPLIT,  		"SPLIT"			},
	{ SWAP_RX,		"SWAPRX"		},
	{ A_SWAP_B,		"SWAPVFO"		},
        { TUNE, 	   	"TUNE"			},
        { VFOA,         	"VFOA"			},
        { VFOB,         	"VFOB"			},
	{ VFO_STEP_MINUS,	"VFOSTEPDOWN"		},
	{ VFO_STEP_PLUS,  	"VFOSTEPUP"		},
	{ VOX,   		"VOX"			},
	{ VOXLEVEL,   		"VOXLEVEL"		},
	{ XIT_CLEAR, 	 	"XITCLEAR"		},
	{ XIT,  		"XITVAL"		},
	{ ZOOM,			"ZOOM"			},
	{ ZOOM_MINUS,		"ZOOMDOWN"		},
	{ ZOOM_PLUS,		"ZOOMUP"		},
        { NO_ACTION,  		"NONE"			}
};

/*
 * Translation from keyword in midi.props file to MIDIaction
 */

static int keyword2action(char *s) {
    int i=0;

    for (i=0; i< (sizeof(OLD_Mapping) / sizeof(OLD_Mapping[0])); i++) {
	if (!strcmp(s, OLD_Mapping[i].str)) return OLD_Mapping[i].action;
    }
    fprintf(stderr,"MIDI: action keyword %s NOT FOUND.\n", s);
    return NO_ACTION;
}

int MIDIstartup(char *filename) {
    FILE *fpin;
    char zeile[255];
    char *cp,*cq;
    int key;
    int action;
    int chan;
    int t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12;
    int delay;
    struct desc *desc;
    enum ACTIONtype type;
    enum MIDIevent event;
    char c;

    MidiReleaseCommands();

    g_print("%s: %s\n",__FUNCTION__,filename);
    fpin=fopen(filename, "r");

    //g_print("%s: fpin=%p\n",__FUNCTION__,fpin);
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
      
//g_print("\n%s:INP:%s\n",__FUNCTION__,zeile);

      chan=-1;                  // default: any channel
      t1 = t2 = t3 = t4 = -1;   // default threshold values
      t5= 0; t6= 63;
      t7=65; t8=127;
      t9 = t10 = t11 = t12 = -1;
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
//g_print("%s: MIDI:KEY:%d\n",__FUNCTION__, key);
      }
      if ((cp = strstr(zeile, "CTRL="))) {
        sscanf(cp+5, "%d", &key);
	event=MIDI_CTRL;
	type=MIDI_KNOB;
//g_print("%s: MIDI:CTL:%d\n",__FUNCTION__, key);
      }
      if ((cp = strstr(zeile, "PITCH "))) {
        event=MIDI_PITCH;
	type=MIDI_KNOB;
//g_print("%s: MIDI:PITCH\n",__FUNCTION__);
      }
      //
      // If event is still undefined, skip line
      //
      if (event == EVENT_NONE) {
//g_print("%s: no event found: %s\n", __FUNCTION__, zeile);
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
//g_print("%s:CHAN:%d\n",__FUNCTION__,chan);
      }
      if ((cp = strstr(zeile, "WHEEL")) && (type == MIDI_KNOB)) {
	// change type from MIDI_KNOB to MIDI_WHEEL
        type=MIDI_WHEEL;
//g_print("%s:WHEEL\n",__FUNCTION__);
      }
      if ((cp = strstr(zeile, "DELAY="))) {
        sscanf(cp+6, "%d", &delay);
//g_print("%s:DELAY:%d\n",__FUNCTION__,delay);
      }
      if ((cp = strstr(zeile, "THR="))) {
        sscanf(cp+4, "%d %d %d %d %d %d %d %d %d %d %d %d",
               &t1,&t2,&t3,&t4,&t5,&t6,&t7,&t8,&t9,&t10,&t11,&t12);
//g_print("%s: THR:%d/%d, %d/%d, %d/%d, %d/%d, %d/%d, %d/%d\n",__FUNCTION__,t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12);
      }
      if ((cp = strstr(zeile, "ACTION="))) {
        // cut zeile at the first blank character following
        cq=cp+7;
        while (*cq != 0 && *cq != '\n' && *cq != ' ' && *cq != '\t') cq++;
	*cq=0;
        action=keyword2action(cp+7);
//g_print("MIDI:ACTION:%s (%d)\n",cp+7, action);
      }
      //
      // All data for a descriptor has been read. Construct it!
      //
      desc = (struct desc *) malloc(sizeof(struct desc));
      desc->next = NULL;
      desc->action = action;
      desc->type = type;
      desc->event = event;
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
//g_print("%s: Insert desc=%p in CMDS[128] table\n",__FUNCTION__,desc);
        MidiAddCommand(128, desc);
      }
      if (event == MIDI_NOTE || event == MIDI_CTRL) {
//g_print("%s: Insert desc=%p in CMDS[%d] table\n",__FUNCTION__,desc,key);
        MidiAddCommand(key, desc);
      }
    }
    return 0;
}
