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
// (sorted alphabetically)
//
enum MIDIaction {
  ACTION_NONE=0,	// No-Op (unassigned key)
  MIDI_AGC,			// AGC level
  AGCATTACK,		// AGC ATTACK (cycle fast/med/slow etc.)
  ATT,			// Step attenuator or Programmable attenuator
  AF_GAIN,		// AF gain
  BAND_DOWN,		// cycle through bands downwards
  BAND_UP,		// cycle through bands upwards
  COMPRESS,		// TX compressor value
  MIDI_CTUN,			// CTUN on/off
  FILTER_UP,		// cycle through filters upwards
  FILTER_DOWN,		// cycle through filters downwards
  MIC_VOLUME,		// MIC gain
  MIDI_LOCK,			// disable frequency changes
  MODE_UP,		// cycle through modes upwards
  MODE_DOWN,		// cycle through modes downwards
  MIDI_MOX,			// toggle "mox" state
  MIDI_NB,			// cycle through NoiseBlanker states (none, NB, NB2)
  MIDI_NR,			// cycle through NoiseReduction states (none, NR, NR2)
  PRE,			// preamp on/off
  PAN_HIGH,		// "high" value of current panadapter
  PAN_LOW,		// "low" value of current panadapter
  MIDI_PS,			// PURESIGNAL on/off
  MIDI_RIT_CLEAR,		// clear RIT value
  RIT_VAL,		// change RIT value
  MIDI_SPLIT,		// Split on/off
  SWAP_VFO,		// swap VFO A/B frequency
  MIDI_TUNE,			// toggle "tune" state
  TX_DRIVE,		// RF output power
  VFO,			// change VFO frequency
  VFOA,			// change VFO-A frequency
  VFOB,			// change VFO-B frequency
  VFO_A2B,		// VFO A -> B
  VFO_B2A,		// VFO B -> A
  VOX 			// VOX on/off
};

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

enum MIDItype {
 TYPE_NONE=0,
 MIDI_KEY,          // Button (press event)
 MIDI_KNOB,         // Knob   (value between 0 and 100)
 MIDI_WHEEL         // Wheel  (direction and speed)
};

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
   int               low_thr3;    // Wheel only: If controller value is <= this value, generate "very fast down"
   int               low_thr2;    // Wheel only: If controller value is <= this value, generate "     fast down"
   int               low_thr1;    // Wheel only: If controller value is <= this value, generate "          down"
   int               up_thr1;     // Wheel only: If controller value is <= this value, generate "          up  "
   int               up_thr2;     // Wheel only: If controller value is <= this value, generate "     fast up  "
   int               up_thr3;     // Wheel only: If controller value is <= this value, generate "very fast up  "
   int		     delay;       // Wheel only: delay (msec) before next message is given upstream
   enum MIDIaction   action;	  // SDR "action" to generate
   struct desc       *next;       // Next defined action for a controller/key with that note value (NULL for end of list)
};

struct {
   struct desc *desc[128];    // description for Note On/Off and ControllerChange
   struct desc *pitch;        // description for PitchChanges
} MidiCommandsTable;

//
// Layer-1 entry point, called once for all the MIDI devices
// that have been defined. This is called upon startup by
// Layer-2 through the function MIDIstartup.
//
void register_midi_device(char *name);

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
void MIDIstartup();

//
// Layer-3 entry point (called by Layer2). In Layer-3, all the pihpsdr
// actions (such as changing the VFO frequency) are performed.
// The implementation of DoTheMIDI is tightly bound to pihpsr and contains
// tons of invocations of g_idle_add with routines from ext.c
//

void DoTheMidi(enum MIDIaction code, enum MIDItype type, int val);
