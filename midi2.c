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
    static enum MIDIaction last_wheel_action=MIDI_ACTION_NONE ;
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
                        case MIDI_ACTION_CWLEFT:
                        case MIDI_ACTION_CWRIGHT:
                        case MIDI_ACTION_CWKEYER:
                        case MIDI_ACTION_PTTKEYER:
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
//			g_print("%s: WHEEL: val=%d new=%d thrs=%d/%d, %d/%d, %d/%d, %d/%d, %d/%d, %d/%d\n",
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

/*
 * This data structre connects names as used in the midi.props file with
 * our MIDIaction enum values.
 *
 * At some places in the code, it is assumes that ActionTable[i].action == i
 * so keep the entries strictly in the order the enum is defined, and
 * add one entry with ACTION_NONE at the end.
 */

ACTION_TABLE ActionTable[] = {
        { MIDI_ACTION_NONE,     	"NONE",         	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL   },
        { MIDI_ACTION_VFO_A2B,         	"A2B",          	MIDI_KEY 			},
        { MIDI_ACTION_AF_GAIN,         	"AFGAIN",       	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_AGCATTACK,       	"AGCATTACK",    	MIDI_KEY 			},
        { MIDI_ACTION_AGC,             	"AGCVAL",       	MIDI_KNOB|MIDI_WHEEL 		},
	{ MIDI_ACTION_ANF,             	"ANF",          	MIDI_KEY 			},
        { MIDI_ACTION_ATT,             	"ATT",          	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL   },
        { MIDI_ACTION_VFO_B2A,        	"B2A",          	MIDI_KEY 			},
        { MIDI_ACTION_BAND_10,         	"BAND10",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_12,         	"BAND12",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_1240,       	"BAND1240",     	MIDI_KEY 			},
        { MIDI_ACTION_BAND_144,        	"BAND144",      	MIDI_KEY 			},
        { MIDI_ACTION_BAND_15,         	"BAND15",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_160,        	"BAND160",      	MIDI_KEY 			},
        { MIDI_ACTION_BAND_17,         	"BAND17",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_20,         	"BAND20",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_220,        	"BAND220",      	MIDI_KEY 			},
        { MIDI_ACTION_BAND_2300,       	"BAND2300",     	MIDI_KEY 			},
        { MIDI_ACTION_BAND_30,         	"BAND30",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_3400,       	"BAND3400",     	MIDI_KEY 			},
        { MIDI_ACTION_BAND_40,         	"BAND40",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_430,        	"BAND430",      	MIDI_KEY 			},
        { MIDI_ACTION_BAND_6,          	"BAND6",        	MIDI_KEY 			},
        { MIDI_ACTION_BAND_60,         	"BAND60",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_70,         	"BAND70",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_80,         	"BAND80",       	MIDI_KEY 			},
        { MIDI_ACTION_BAND_902,        	"BAND902",      	MIDI_KEY 			},
        { MIDI_ACTION_BAND_AIR,        	"BANDAIR",      	MIDI_KEY 			},
        { MIDI_ACTION_BAND_DOWN,	"BANDDOWN",     	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL 	},
        { MIDI_ACTION_BAND_GEN,      	"BANDGEN",      	MIDI_KEY 			},
        { MIDI_ACTION_BAND_UP,         	"BANDUP",       	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL 	},
        { MIDI_ACTION_BAND_WWV,        	"BANDWWV",      	MIDI_KEY 			},
        { MIDI_ACTION_COMPRESS,        	"COMPRESS",     	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_CTUN,            	"CTUN",         	MIDI_KEY 			},
        { MIDI_ACTION_VFO,             	"CURRVFO",      	MIDI_WHEEL 			},
        { MIDI_ACTION_CWKEYER,         	"CW(Keyer)",        	MIDI_KEY 			},
        { MIDI_ACTION_CWLEFT,         	"CWLEFT",          	MIDI_KEY 			},
        { MIDI_ACTION_CWRIGHT,         	"CWRIGHT",          	MIDI_KEY 			},
        { MIDI_ACTION_CWSPEED,         	"CWSPEED",      	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_DIV_COARSEGAIN,  	"DIVCOARSEGAIN",        MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_DIV_COARSEPHASE,  "DIVCOARSEPHASE",       MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_DIV_FINEGAIN,     "DIVFINEGAIN",  	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_DIV_FINEPHASE,    "DIVFINEPHASE", 	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_DIV_GAIN,         "DIVGAIN",      	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_DIV_PHASE,        "DIVPHASE",     	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_DIV_TOGGLE,       "DIVTOGGLE",    	MIDI_KEY 			},
        { MIDI_ACTION_DUP,             	"DUP",          	MIDI_KEY 			},
        { MIDI_ACTION_FILTER_DOWN,      "FILTERDOWN",   	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL 	},
        { MIDI_ACTION_FILTER_UP,        "FILTERUP",     	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL 	},
        { MIDI_ACTION_LOCK,            	"LOCK",         	MIDI_KEY 			},
        { MIDI_ACTION_MEM_RECALL_M0,    "RECALLM0",  		MIDI_KEY           		},
        { MIDI_ACTION_MEM_RECALL_M1,    "RECALLM1",  		MIDI_KEY           		},
        { MIDI_ACTION_MEM_RECALL_M2,    "RECALLM2",  		MIDI_KEY           		},
        { MIDI_ACTION_MEM_RECALL_M3,    "RECALLM3",  		MIDI_KEY           		},
        { MIDI_ACTION_MEM_RECALL_M4,    "RECALLM4",  		MIDI_KEY           		},
        { MIDI_ACTION_MENU_FILTER,      "MENU_FILTER",  	MIDI_KEY 			},
        { MIDI_ACTION_MENU_MODE,        "MENU_MODE",    	MIDI_KEY 			},
        { MIDI_ACTION_MIC_VOLUME,       "MICGAIN",      	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_MODE_DOWN,        "MODEDOWN",     	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL   },
        { MIDI_ACTION_MODE_UP,          "MODEUP",       	MIDI_KEY|MIDI_KNOB|MIDI_WHEEL 	},
        { MIDI_ACTION_MOX,             	"MOX",  		MIDI_KEY 			},
        { MIDI_ACTION_MUTE,            	"MUTE", 		MIDI_KEY 			},
        { MIDI_ACTION_NB,             	"NOISEBLANKER", 	MIDI_KEY 			},
        { MIDI_ACTION_NR,              	"NOISEREDUCTION",       MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_0,        	"NUMPAD0",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_1,        	"NUMPAD1",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_2,        	"NUMPAD2",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_3,        	"NUMPAD3",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_4,        	"NUMPAD4",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_5,        	"NUMPAD5",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_6,        	"NUMPAD6",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_7,        	"NUMPAD7",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_8,        	"NUMPAD8",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_9,        	"NUMPAD9",      	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_CL,       	"NUMPADCL",     	MIDI_KEY 			},
        { MIDI_ACTION_NUMPAD_ENTER,    	"NUMPADENTER",  	MIDI_KEY 			},
        { MIDI_ACTION_PAN,             	"PAN",  		MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_PAN_HIGH,        	"PANHIGH",      	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_PAN_LOW,         	"PANLOW",       	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_PRE,             	"PREAMP",       	MIDI_KEY 			},
        { MIDI_ACTION_PTTKEYER,        	"PTT(Keyer)",     	MIDI_KEY 			},
        { MIDI_ACTION_PS,               "PURESIGNAL",   	MIDI_KEY 			},
        { MIDI_ACTION_RF_GAIN,         	"RFGAIN",       	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_TX_DRIVE,         "RFPOWER",      	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_RIT_CLEAR,       	"RITCLEAR",     	MIDI_KEY 			},
        { MIDI_ACTION_RIT_STEP,         "RITSTEP",      	MIDI_KEY|MIDI_WHEEL 		},
        { MIDI_ACTION_RIT_TOGGLE,       "RITTOGGLE",    	MIDI_KEY 			},
        { MIDI_ACTION_RIT_VAL,          "RITVAL",       	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_SAT,             	"SAT",  		MIDI_KEY 			},
        { MIDI_ACTION_SNB,              "SNB",  		MIDI_KEY 			},
        { MIDI_ACTION_SPLIT,           	"SPLIT",        	MIDI_KEY 			},
        { MIDI_ACTION_MEM_STORE_M0,     "STOREM0",             	MIDI_KEY 			},
        { MIDI_ACTION_MEM_STORE_M1,     "STOREM1",             	MIDI_KEY 			},
        { MIDI_ACTION_MEM_STORE_M2,     "STOREM2",             	MIDI_KEY 			},
        { MIDI_ACTION_MEM_STORE_M3,     "STOREM3",              MIDI_KEY 			},
        { MIDI_ACTION_MEM_STORE_M4,     "STOREM4",              MIDI_KEY 			},
        { MIDI_ACTION_SWAP_RX,          "SWAPRX",       	MIDI_KEY 			},
        { MIDI_ACTION_SWAP_VFO,         "SWAPVFO",      	MIDI_KEY 			},
        { MIDI_ACTION_TUNE,            	"TUNE", 		MIDI_KEY 			},
        { MIDI_ACTION_VFOA,             "VFOA", 		MIDI_WHEEL 			},
        { MIDI_ACTION_VFOB,             "VFOB", 		MIDI_WHEEL 			},
        { MIDI_ACTION_VFO_STEP_UP,      "VFOSTEPUP",    	MIDI_KEY|MIDI_WHEEL 		},
        { MIDI_ACTION_VFO_STEP_DOWN,    "VFOSTEPDOWN",  	MIDI_KEY|MIDI_WHEEL 		},
        { MIDI_ACTION_VOX,              "VOX",  		MIDI_KEY 			},
        { MIDI_ACTION_VOXLEVEL,         "VOXLEVEL",     	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_XIT_CLEAR,       	"XITCLEAR",     	MIDI_KEY 			},
        { MIDI_ACTION_XIT_VAL,          "XITVAL",       	MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_ZOOM,            	"ZOOM", 		MIDI_KNOB|MIDI_WHEEL 		},
        { MIDI_ACTION_ZOOM_UP,          "ZOOMUP",       	MIDI_KEY|MIDI_WHEEL 		},
        { MIDI_ACTION_ZOOM_DOWN,        "ZOOMDOWN",     	MIDI_KEY|MIDI_WHEEL 		},
        { MIDI_ACTION_LAST,		"NONE", 		TYPE_NONE       		},
};


/*
 * Translation from keyword in midi.props file to MIDIaction
 */

static int keyword2action(char *s) {
    int i=0;

    for (i=0; i< (sizeof(ActionTable) / sizeof(ActionTable[0])); i++) {
	if (!strcmp(s, ActionTable[i].str)) return ActionTable[i].action;
    }
    g_print("%s: action keyword %s NOT FOUND.\n", __FUNCTION__, s);
    return MIDI_ACTION_NONE;
}

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

/*
 * Here we read in a MIDI description file and fill the MidiCommandsTable
 * data structure
 */

int MIDIstartup(char *filename) {
    FILE *fpin;
    char zeile[255];
    char *cp,*cq;
    int key;
    enum MIDIaction action;
    int chan;
    int t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12;
    int delay;
    struct desc *desc;
    enum MIDItype type;
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
         action = keyword2action(cp+7);
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
