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

//
// MIDIaction encodes the "action" to be taken in Layer3
// (sorted alphabetically by the keyword).
// All MIDIaction entries should begin with "MIDI_ACTION"
//
enum MIDIaction {
  MIDI_ACTION_NONE=0,		// NONE:		No-Op (unassigned key)
  MIDI_ACTION_VFO_A2B,		// A2B:			VFO A -> B
  MIDI_ACTION_AF_GAIN,		// AFGAIN:		AF gain
  MIDI_ACTION_AGCATTACK,	// AGCATTACK:		AGC ATTACK (cycle fast/med/slow etc.)
  MIDI_ACTION_AGC,		// AGCVAL:		AGC level
  MIDI_ACTION_ANF,		// ANF:			toggel ANF on/off
  MIDI_ACTION_ATT,		// ATT:			Step attenuator or Programmable attenuator
  MIDI_ACTION_VFO_B2A,		// B2A:			VFO B -> A
  MIDI_ACTION_BAND_10,          // BAND10
  MIDI_ACTION_BAND_12,          // BAND12
  MIDI_ACTION_BAND_1240,        // BAND1240
  MIDI_ACTION_BAND_144,         // BAND144
  MIDI_ACTION_BAND_15,          // BAND15
  MIDI_ACTION_BAND_160,         // BAND160
  MIDI_ACTION_BAND_17,          // BAND17
  MIDI_ACTION_BAND_20,          // BAND20
  MIDI_ACTION_BAND_220,         // BAND220
  MIDI_ACTION_BAND_2300,        // BAND2300
  MIDI_ACTION_BAND_30,          // BAND30
  MIDI_ACTION_BAND_3400,        // BAND3400
  MIDI_ACTION_BAND_40,          // BAND40
  MIDI_ACTION_BAND_430,         // BAND430
  MIDI_ACTION_BAND_6,           // BAND6
  MIDI_ACTION_BAND_60,          // BAND60
  MIDI_ACTION_BAND_70,          // BAND70
  MIDI_ACTION_BAND_80,          // BAND80
  MIDI_ACTION_BAND_902,         // BAND902
  MIDI_ACTION_BAND_AIR,         // BANDAIR
  MIDI_ACTION_BAND_DOWN,	// BANDDOWN:		cycle through bands downwards
  MIDI_ACTION_BAND_GEN,		// BANDGEN
  MIDI_ACTION_BAND_UP,		// BANDUP:		cycle through bands upwards
  MIDI_ACTION_BAND_WWV,		// BANDWWVUP:		cycle through bands upwards
  MIDI_ACTION_COMPRESS,		// COMPRESS:		TX compressor value
  MIDI_ACTION_CTUN,		// CTUN:		toggle CTUN on/off
  MIDI_ACTION_VFO,		// CURRVFO:		change VFO frequency
  MIDI_ACTION_CWKEYER,		// CW(Keyer):		Unconditional CW key-down/up (outside keyer)
  MIDI_ACTION_CWLEFT,		// CWLEFT:		Left paddle pressed (use with ONOFF)
  MIDI_ACTION_CWRIGHT,		// CWRIGHT:		Right paddle pressed (use with ONOFF)
  MIDI_ACTION_CWSPEED,		// CWSPEED:		Set speed of (iambic) CW keyer
  MIDI_ACTION_DIV_COARSEGAIN,	// DIVCOARSEGAIN:	change DIVERSITY gain in large increments
  MIDI_ACTION_DIV_COARSEPHASE,	// DIVCOARSEPHASE:	change DIVERSITY phase in large increments
  MIDI_ACTION_DIV_FINEGAIN,	// DIVFINEGAIN:		change DIVERSITY gain in small increments
  MIDI_ACTION_DIV_FINEPHASE,	// DIVFINEPHASE:	change DIVERSITY phase in small increments
  MIDI_ACTION_DIV_GAIN,		// DIVGAIN:		change DIVERSITY gain in medium increments
  MIDI_ACTION_DIV_PHASE,	// DIVPHASE:		change DIVERSITY phase in medium increments
  MIDI_ACTION_DIV_TOGGLE,	// DIVTOGGLE:		DIVERSITY on/off
  MIDI_ACTION_DUP,		// DUP:			toggle duplex on/off
  MIDI_ACTION_FILTER_DOWN,	// FILTERDOWN:		cycle through filters downwards
  MIDI_ACTION_FILTER_UP,	// FILTERUP:		cycle through filters upwards
  MIDI_ACTION_LOCK,		// LOCK:		lock VFOs, disable frequency changes
  MIDI_ACTION_MEM_RECALL_M0,    // RECALLM0:		load current freq/mode/filter from memory slot #0
  MIDI_ACTION_MEM_RECALL_M1,    // RECALLM1:		load current freq/mode/filter from memory slot #1
  MIDI_ACTION_MEM_RECALL_M2,    // RECALLM2:		load current freq/mode/filter from memory slot #2
  MIDI_ACTION_MEM_RECALL_M3,    // RECALLM3:		load current freq/mode/filter from memory slot #3
  MIDI_ACTION_MEM_RECALL_M4,    // RECALLM4:		load current freq/mode/filter from memory slot #4
  MIDI_ACTION_MENU_FILTER,      // MENU_FILTER
  MIDI_ACTION_MENU_MODE,        // MENU_MODE
  MIDI_ACTION_MIC_VOLUME,	// MICGAIN:		MIC gain
  MIDI_ACTION_MODE_DOWN,	// MODEDOWN:		cycle through modes downwards
  MIDI_ACTION_MODE_UP,		// MODEUP:		cycle through modes upwards
  MIDI_ACTION_MOX,		// MOX:			toggle "mox" state
  MIDI_ACTION_MUTE,		// MUTE:		toggle mute on/off
  MIDI_ACTION_NB,		// NOISEBLANKER:	cycle through NoiseBlanker states (none, NB, NB2)
  MIDI_ACTION_NR,		// NOISEREDUCTION:	cycle through NoiseReduction states (none, NR, NR2)
  MIDI_ACTION_NUMPAD_0,         // NUMPAD0
  MIDI_ACTION_NUMPAD_1,         // NUMPAD1
  MIDI_ACTION_NUMPAD_2,         // NUMPAD2
  MIDI_ACTION_NUMPAD_3,         // NUMPAD3
  MIDI_ACTION_NUMPAD_4,         // NUMPAD4
  MIDI_ACTION_NUMPAD_5,         // NUMPAD5
  MIDI_ACTION_NUMPAD_6,         // NUMPAD6
  MIDI_ACTION_NUMPAD_7,         // NUMPAD7
  MIDI_ACTION_NUMPAD_8,         // NUMPAD8
  MIDI_ACTION_NUMPAD_9,         // NUMPAD9
  MIDI_ACTION_NUMPAD_CL,        // NUMPADCL
  MIDI_ACTION_NUMPAD_ENTER,     // NUMPADENTER
  MIDI_ACTION_PAN,		// PAN:			change panning of panadater/waterfall when zoomed
  MIDI_ACTION_PAN_HIGH,		// PANHIGH:		"high" value of current panadapter
  MIDI_ACTION_PAN_LOW,		// PANLOW:		"low" value of current panadapter
  MIDI_ACTION_PRE,		// PREAMP:		preamp on/off
  MIDI_ACTION_PTTKEYER,		// PTT(Keyer):			set PTT state to "on" or "off"
  MIDI_ACTION_PS,		// PURESIGNAL:		toggle PURESIGNAL on/off
  MIDI_ACTION_RF_GAIN,		// RFGAIN:		receiver RF gain
  MIDI_ACTION_TX_DRIVE,		// RFPOWER:		adjust TX RF output power
  MIDI_ACTION_RIT_CLEAR,	// RITCLEAR:		clear RIT and XIT value
  MIDI_ACTION_RIT_STEP,		// RITSTEP:		cycle through RIT/XIT step size values
  MIDI_ACTION_RIT_TOGGLE,  	// RITTOGGLE:		toggle RIT on/off
  MIDI_ACTION_RIT_VAL,		// RITVAL:		change RIT value
  MIDI_ACTION_SAT,		// SAT:			cycle through SAT modes off/SAT/RSAT
  MIDI_ACTION_SNB,		// SNB:			toggle SNB on/off
  MIDI_ACTION_SPLIT,		// SPLIT:		Split on/off
  MIDI_ACTION_MEM_STORE_M0,     // STOREM0:		store current freq/mode/filter in memory slot #0
  MIDI_ACTION_MEM_STORE_M1,     // STOREM1:		store current freq/mode/filter in memory slot #1
  MIDI_ACTION_MEM_STORE_M2,     // STOREM2:		store current freq/mode/filter in memory slot #2
  MIDI_ACTION_MEM_STORE_M3,     // STOREM3:		store current freq/mode/filter in memory slot #3
  MIDI_ACTION_MEM_STORE_M4,     // STOREM4:		store current freq/mode/filter in memory slot #4
  MIDI_ACTION_SWAP_RX, 		// SWAPRX:		swap active receiver (if there are two receivers)
  MIDI_ACTION_SWAP_VFO,		// SWAPVFO:		swap VFO A/B frequency
  MIDI_ACTION_TUNE,		// TUNE:		toggle "tune" state
  MIDI_ACTION_VFOA,		// VFOA:		change VFO-A frequency
  MIDI_ACTION_VFOB,		// VFOB:		change VFO-B frequency
  MIDI_ACTION_VFO_STEP_UP,	// VFOSTEPUP:		cycle through vfo steps upwards;
  MIDI_ACTION_VFO_STEP_DOWN,	// VFOSTEPDOWN:		cycle through vfo steps downwards;
  MIDI_ACTION_VOX, 		// VOX:			toggle VOX on/off
  MIDI_ACTION_VOXLEVEL, 	// VOXLEVEL:		adjust VOX threshold
  MIDI_ACTION_XIT_CLEAR,	// XITCLEAR:		clear XIT value
  MIDI_ACTION_XIT_VAL,		// XITVAL:		change XIT value
  MIDI_ACTION_ZOOM,		// ZOOM:		change zoom factor
  MIDI_ACTION_ZOOM_UP,		// ZOOMUP:		change zoom factor
  MIDI_ACTION_ZOOM_DOWN,	// ZOOMDOWN:		change zoom factor
  MIDI_ACTION_LAST,             // flag for end of list
};

//
// MIDItype encodes the type of MIDI control. This info
// is passed from Layer-2 to Layer-3
//
// MIDI_TYPE_KEY has no parameters and indicates that some
// button has been pressed.
//
// MIDI_TYPE_KNOB has a "value" parameter (between 0 and 100)
// and indicates that some knob has been set to a specific
// position.
//
// MIDI_TYPE_WHEEL has a "direction" parameter and indicates that
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

enum MIDItype {
 MIDI_TYPE_NONE =0,
 MIDI_TYPE_KEY  =1,      // Button (press event)
 MIDI_TYPE_KNOB =2,      // Knob   (value between 0 and 100)
 MIDI_TYPE_WHEEL=4       // Wheel  (direction and speed)
};

extern gchar *midi_types[];
extern gchar *midi_events[];

//
// MIDIevent encodes the actual MIDI event "seen" in Layer-1 and
// passed to Layer-2. MIDI_NOTE events end up as MIDI_KEY and
// MIDI_PITCH as MIDI_KNOB, while MIDI_CTRL can end up both as
// MIDI_KNOB or MIDI_WHEEL, depending on the device description.
//
enum MIDIevent {
 MIDI_EVENT_NONE=0,
 MIDI_EVENT_NOTE,
 MIDI_EVENT_CTRL,
 MIDI_EVENT_PITCH
};

typedef struct _action_table {
  enum MIDIaction action;
  const char *str;
  enum MIDItype type;
  int onoff;
} ACTION_TABLE;

extern ACTION_TABLE ActionTable[];

//
// Data structure for Layer-2
//

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
// If an action is generated both on NOTE_ON and NOTE_OFF,
// then MOX is engaged when pressing the key and disengaged
// when releasing it. For MOX this makes little send but you
// might want to configure the TUNE button this way.
// The latter behaviour is triggered when the line assigning the key
// or "NOTE OFF". The table speficying the behaviour of layer-2 thus
// contains the key word "ONOFF". This is stored in the field "onoff"
// in struct desc.

struct desc {
   int               channel;     // -1 for ANY channel
   enum MIDIevent    event;	  // type of event (NOTE on/off, Controller change, Pitch value)
   int               onoff;       // 1: generate upstream event both for Note-on and Note-off
   enum MIDItype     type;        // Key, Knob, or Wheel
   int               vfl1,vfl2;   // Wheel only: range of controller values for "very fast left"
   int               fl1,fl2;     // Wheel only: range of controller values for "fast left"
   int               lft1,lft2;   // Wheel only: range of controller values for "slow left"
   int               vfr1,vfr2;   // Wheel only: range of controller values for "very fast right"
   int               fr1,fr2;     // Wheel only: range of controller values for "fast right"
   int               rgt1,rgt2;   // Wheel only: range of controller values for "slow right"
   int		     delay;       // Wheel only: delay (msec) before next message is given upstream
   enum MIDIaction   action;	  // SDR "action" to generate
   struct desc       *next;       // Next defined action for a controller/key with that note value (NULL for end of list)
};

extern struct desc *MidiCommandsTable[129];

extern int midi_debug;

//
// Layer-1 entry point, called once for all the MIDI devices
// that have been defined. This is called upon startup by
// Layer-2 through the function MIDIstartup.
//
void register_midi_device(int index);
void close_midi_device(int index);
void configure_midi_device(gboolean state);

//
// Layer-2 entry point (called by Layer1)
// 
// When Layer-1 has received a MIDI message, it calls
// NewMidiEvent.
//
// MIDIstartup looks for files containing descriptions for MIDI
// devices and calls the Layer-1 function register_midi_device
// for each device description that was successfully read.

void NewMidiEvent(enum MIDIevent event, int channel, int note, int val);
int MIDIstartup(char *filename);
void MidiAddCommand(int note, struct desc *desc);
void MidiReleaseCommands();

//
// Layer-3 entry point (called by Layer2). In Layer-3, all the pihpsdr
// actions (such as changing the VFO frequency) are performed.
// The implementation of DoTheMIDI is tightly bound to pihpsr and contains
// tons of invocations of g_idle_add with routines from ext.c
//

void DoTheMidi(enum MIDIaction code, enum MIDItype type, int val);
