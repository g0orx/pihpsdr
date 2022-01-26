/*
 * MIDI support for pihpsdr
 *
 * (C) Christoph van Wullen, DL1YCF.
 *
 * Midi support works in three layers
 *
 * Layer-1: hardware specific
 * --------------------------
 *
 * Layer1 either implements a callback function (if the operating system
 * supports MIDI) or a separate thread polling MIDI data. Whenever a
 * MIDI command arrives, such as Note on/off or Midi-Controller value
 * changed, it calls Layer 2.
 *
 * Layer-2: MIDI device specific
 * ----------------------------- 
 *
 * Layer2 translates MIDI commands into pihpsdr actions. This is done with
 * a table-driven algorithm, such that the same translator can be used for
 * any MIDI device provided the tables have been set up correctly.
 * It seems overly complicated to create a user interface for setting up
 * these tables, instead a standard text file describing the MIDI device
 * is read and the tables are set up.
 * Layer-2 has SDR applications in mind, but is not necessarily specific
 * to pihpsr. It calls the Layer-3 function.
 *
 * Layer-3: pihpsdr specific
 * -------------------------
 *
 * Layer 3, finally, implements all the "actions" we can make, such as TUNE
 * or VFO. This Layer calls pihpsdr functions.
 *
 * One word to MIDI channels. Usually, a MIDI device can be configured to use
 * a specific channel, such that different keyboards use different channels.
 * The Layer-2 tables can either specify that the MIDI command has to come from
 * a specific channel, or can specify that the action will be taken not matter which
 * channel the MIDI message comes from. The latter case should be the default, but
 * if we want to connect more than one MIDI device, we need to speficy the channel.
 *
 * In principle this supports more than one MIDI device, but in this case they
 * must generate MIDI events on different channels
 */

#ifndef _MIDI_H
#define _MIDI_H
//
// MIDIaction encodes the "action" to be taken in Layer3
// (sorted alphabetically by the keyword)
//
/*
enum MIDIaction {
  ACTION_NONE=0,	// NONE:		No-Op (unassigned key)
  VFO_A2B,		// A2B:			VFO A -> B
  MIDI_AF_GAIN,		// AFGAIN:		AF gain
  AGCATTACK,		// AGCATTACK:		AGC ATTACK (cycle fast/med/slow etc.)
  MIDI_AGC,		// AGCVAL:		AGC level
  ANF,			// ANF:			toggel ANF on/off
  ATT,			// ATT:			Step attenuator or Programmable attenuator
  VFO_B2A,		// B2A:			VFO B -> A
  MIDI_BAND_10,         // BAND10
  MIDI_BAND_12,         // BAND12
  MIDI_BAND_1240,       // BAND1240
  MIDI_BAND_144,        // BAND144
  MIDI_BAND_15,         // BAND15
  MIDI_BAND_160,        // BAND160
  MIDI_BAND_17,         // BAND17
  MIDI_BAND_20,         // BAND20
  MIDI_BAND_220,        // BAND220
  MIDI_BAND_2300,       // BAND2300
  MIDI_BAND_30,         // BAND30
  MIDI_BAND_3400,       // BAND3400
  MIDI_BAND_40,         // BAND40
  MIDI_BAND_430,        // BAND430
  MIDI_BAND_6,          // BAND6
  MIDI_BAND_60,         // BAND60
  MIDI_BAND_70,         // BAND70
  MIDI_BAND_80,         // BAND80
  MIDI_BAND_902,        // BAND902
  MIDI_BAND_AIR,        // BANDAIR
  BAND_DOWN,            // BANDDOWN
  MIDI_BAND_GEN,        // BANDGEN
  BAND_UP,              // BANDUP
  MIDI_BAND_WWV,        // BANDWWV
  COMPRESS,		// COMPRESS:		TX compressor value
  MIDI_CTUN,		// CTUN:		toggle CTUN on/off
  VFO,			// CURRVFO:		change VFO frequency
  CWLEFT,		// CWL:			Left paddle pressed
  CWRIGHT,		// CWR:			Right paddle pressed
  CWSPEED,		// CWSPEED:		Set speed of (iambic) CW keyer
  DIV_COARSEGAIN,	// DIVCOARSEGAIN:	change DIVERSITY gain in large increments
  DIV_COARSEPHASE,	// DIVPHASE:		change DIVERSITY phase in large increments
  DIV_FINEGAIN,		// DIVFINEGAIN:		change DIVERSITY gain in small increments
  DIV_FINEPHASE,	// DIVFINEPHASE:	change DIVERSITY phase in small increments
  DIV_GAIN,		// DIVGAIN:		change DIVERSITY gain in medium increments
  DIV_PHASE,		// DIVPHASE:		change DIVERSITY phase in medium increments
  DIV_TOGGLE,		// DIVTOGGLE:		DIVERSITY on/off
  MIDI_DUP,		// DUP:			toggle duplex on/off
  FILTER_DOWN,		// FILTERDOWN:		cycle through filters downwards
  FILTER_UP,		// FILTERUP:		cycle through filters upwards
  MENU_FILTER,
  MENU_MODE,
  MIDI_LOCK,		// LOCK:		lock VFOs, disable frequency changes
  MIC_VOLUME,		// MICGAIN:		MIC gain
  MODE_DOWN,		// MODEDOWN:		cycle through modes downwards
  MODE_UP,		// MODEUP:		cycle through modes upwards
  MIDI_MOX,		// MOX:			toggle "mox" state
  MIDI_MUTE,		// MUTE:		toggle mute on/off
  MIDI_NB,		// NOISEBLANKER:	cycle through NoiseBlanker states (none, NB, NB2)
  MIDI_NR,		// NOISEREDUCTION:	cycle through NoiseReduction states (none, NR, NR2)
  NUMPAD_0,		// NUMPAD0
  NUMPAD_1,		// NUMPAD1
  NUMPAD_2,		// NUMPAD2
  NUMPAD_3,		// NUMPAD3
  NUMPAD_4,		// NUMPAD4
  NUMPAD_5,		// NUMPAD5
  NUMPAD_6,		// NUMPAD6
  NUMPAD_7,		// NUMPAD7
  NUMPAD_8,		// NUMPAD8
  NUMPAD_9,		// NUMPAD9
  NUMPAD_CL,		// NUMPADCL
  NUMPAD_ENTER,		// NUMPADENTER
  MIDI_PAN,		// PAN:			change panning of panadater/waterfall when zoomed
  PAN_HIGH,		// PANHIGH:		"high" value of current panadapter
  PAN_LOW,		// PANLOW:		"low" value of current panadapter
  PRE,			// PREAMP:		preamp on/off
  MIDI_PS,		// PURESIGNAL:		toggle PURESIGNAL on/off
  MIDI_RF_GAIN,		// RFGAIN:		receiver RF gain
  TX_DRIVE,		// RFPOWER:		adjust TX RF output power
  MIDI_RIT_CLEAR,	// RITCLEAR:		clear RIT and XIT value
  RIT_STEP,		// RITSTEP:		cycle through RIT/XIT step size values
  RIT_TOGGLE,  		// RITTOGGLE:		toggle RIT on/off
  RIT_VAL,		// RITVAL:		change RIT value
  MIDI_SAT,		// SAT:			cycle through SAT modes off/SAT/RSAT
  SNB,			// SNB:			toggle SNB on/off
  MIDI_SPLIT,		// SPLIT:		Split on/off
  SWAP_RX, 		// SWAPRX:		swap active receiver (if there are two receivers)
  SWAP_VFO,		// SWAPVFO:		swap VFO A/B frequency
  MIDI_TUNE,		// TUNE:		toggle "tune" state
  VFOA,			// VFOA:		change VFO-A frequency
  VFOB,			// VFOB:		change VFO-B frequency
  VFO_STEP_UP,		// VFOSTEPUP:		cycle through vfo steps upwards;
  VFO_STEP_DOWN,	// VFOSTEPDOWN:		cycle through vfo steps downwards;
  VOX, 			// VOX:			toggle VOX on/off
  VOXLEVEL, 		// VOXLEVEL:		adjust VOX threshold
  MIDI_XIT_CLEAR,	// XITCLEAR:		clear XIT value
  XIT_VAL,		// XITVAL:		change XIT value
  MIDI_ZOOM,		// ZOOM:		change zoom factor
  ZOOM_UP,		// ZOOMUP:		change zoom factor
  ZOOM_DOWN,		// ZOOMDOWN:		change zoom factor
};
*/

//
// MIDItype encodes the type of MIDI control. This info
// is passed from Layer-2 to Layer-3
//
// MIDI_KEY has no parameters and indicates that some
// button has been pressed.
//
// MIDI_KNOB has a "value" parameter (between 0 and 100)
// and indicates that some knob has been set to a specific
// position.
//
// MIDI_WHEEL has a "direction" parameter and indicates that
// some knob has been turned left/down or right/ip. The  value
// can be
//
// -100 very fast going down
//  -10 fast going down
//   -1 going down
//    1 going up
//   10 fast going up
//  100 very fast going up
//

/*
enum MIDItype {
 TYPE_NONE=0,
 MIDI_KEY=1,          // Button (press event)
 MIDI_KNOB=2,         // Knob   (value between 0 and 100)
 MIDI_WHEEL=4         // Wheel  (direction and speed)
};
*/
extern gchar *midi_types[];
extern gchar *midi_events[];

//
// MIDIevent encodes the actual MIDI event "seen" in Layer-1 and
// passed to Layer-2. MIDI_NOTE events end up as MIDI_KEY and
// MIDI_PITCH as MIDI_KNOB, while MIDI_CTRL can end up both as
// MIDI_KNOB or MIDI_WHEEL, depending on the device description.
//
enum MIDIevent {
 EVENT_NONE=0,
 MIDI_NOTE,
 MIDI_CTRL,
 MIDI_PITCH
};

//
// Data structure for Layer-2
//
typedef struct _old_action_table {
  enum ACTION action;
  const char *str;
  enum ACTIONtype type;
} OLD_ACTION_TABLE;

extern ACTION_TABLE ActionTable[];

//
// There is linked list of all specified MIDI events for a given "Note" value,
// which contains the defined actions for all MIDI_NOTE and MIDI_CTRL events
// with that given note and for all channels
// Note on wheel delay:
// If using a wheel for cycling through a menu, it is difficult to "hit" the correct
// menu item if wheel events are generated at a very high rate. Therefore we can define
// a delay: once a wheel event is reported upstream, any such events are suppressed during
// the delay.
// 
// Note that with a MIDI KEY, you can choose that an action is
// generated only for a NOTE_ON event or both for NOTE_ON and
// NOTE_OFF. In the first case, if the key is associated to MOX,
// then MOX is toggled each time the key is pressed. This behaves
// very much like point-and-clicking the MOX buttion in the GUI.
//

struct desc {
   int               channel;     // -1 for ANY channel
   enum MIDIevent    event;	  // type of event (NOTE on/off, Controller change, Pitch value)
   enum ACTIONtype   type;        // Key, Knob, or Wheel
   int               vfl1,vfl2;   // Wheel only: range of controller values for "very fast left"
   int               fl1,fl2;     // Wheel only: range of controller values for "fast left"
   int               lft1,lft2;   // Wheel only: range of controller values for "slow left"
   int               vfr1,vfr2;   // Wheel only: range of controller values for "very fast right"
   int               fr1,fr2;     // Wheel only: range of controller values for "fast right"
   int               rgt1,rgt2;   // Wheel only: range of controller values for "slow right"
   int		     delay;       // Wheel only: delay (msec) before next message is given upstream
   int   action;	  // SDR "action" to generate
   struct desc       *next;       // Next defined action for a controller/key with that note value (NULL for end of list)
};

struct cmdtable {
   struct desc *desc[128];    // description for Note On/Off and ControllerChange
   struct desc *pitch;        // description for PitchChanges
};

extern struct cmdtable MidiCommandsTable;

extern int midi_debug;

//
// Layer-1 entry point, called once for all the MIDI devices
// that have been defined.
//
int register_midi_device(char *name);
void close_midi_device(void);
void configure_midi_device(gboolean state);

//
// Layer-2 entry point (called by Layer1)
// 
// When Layer-1 has received a MIDI message, it calls
// NewMidiEvent.
//

void NewMidiEvent(enum MIDIevent event, int channel, int note, int val);
int ReadLegacyMidiFile(char *filename);
int MIDIstop(void);

//
// Layer-3 entry point (called by Layer2). In Layer-3, all the pihpsdr
// actions (such as changing the VFO frequency) are performed.
// The implementation of DoTheMIDI is tightly bound to pihpsr and contains
// tons of invocations of g_idle_add with routines from ext.c
//

void DoTheMidi(int code, enum ACTIONtype type, int val);
#endif
