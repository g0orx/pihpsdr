
enum ACTION {
  NO_ACTION=0,
  A_SWAP_B,
  A_TO_B,
  AF_GAIN,
  AF_GAIN_RX1,
  AF_GAIN_RX2,
  AGC,
  AGC_GAIN,
  AGC_GAIN_RX1,
  AGC_GAIN_RX2,
  ANF,
  ATTENUATION,
  B_TO_A,
  BAND_10,
  BAND_12,
  BAND_1240,
  BAND_144,
  BAND_15,
  BAND_160,
  BAND_17,
  BAND_20,
  BAND_220,
  BAND_2300,
  BAND_30,
  BAND_3400,
  BAND_40,
  BAND_430,
  BAND_6,
  BAND_60,
  BAND_70,
  BAND_80,
  BAND_902,
  BAND_AIR,
  BAND_GEN,
  BAND_MINUS,
  BAND_PLUS,
  BAND_WWV,
  BANDSTACK_MINUS,
  BANDSTACK_PLUS,
  COMP_ENABLE,
  COMPRESSION,
  CTUN,
  CW_FREQUENCY,
  CW_LEFT,
  CW_RIGHT,
  CW_SPEED,
  DIV,
  DIV_GAIN,
  DIV_GAIN_COARSE,
  DIV_GAIN_FINE,
  DIV_PHASE,
  DIV_PHASE_COARSE,
  DIV_PHASE_FINE,
  DRIVE,
  DUPLEX,
  FILTER_MINUS,
  FILTER_PLUS,
  FUNCTION,
  IF_SHIFT,
  IF_SHIFT_RX1,
  IF_SHIFT_RX2,
  IF_WIDTH,
  IF_WIDTH_RX1,
  IF_WIDTH_RX2,
  LINEIN_GAIN,
  LOCK,
  MENU_AGC,
  MENU_BAND,
  MENU_BANDSTACK,
  MENU_DIVERSITY,
  MENU_FILTER,
  MENU_FREQUENCY,
  MENU_MEMORY,
  MENU_MODE,
  MENU_NOISE,
  MENU_PS,
  MIC_GAIN,
  MODE_MINUS,
  MODE_PLUS,
  MOX,
  MUTE,
  NB,
  NR,
  NUMPAD_0,
  NUMPAD_1,
  NUMPAD_2,
  NUMPAD_3,
  NUMPAD_4,
  NUMPAD_5,
  NUMPAD_6,
  NUMPAD_7,
  NUMPAD_8,
  NUMPAD_9,
  NUMPAD_CL,
  NUMPAD_ENTER,
  PAN,
  PAN_MINUS,
  PAN_PLUS,
  PANADAPTER_HIGH,
  PANADAPTER_LOW,
  PANADAPTER_STEP,
  PREAMP,
  PS,
  PTT,
  RF_GAIN,
  RF_GAIN_RX1,
  RF_GAIN_RX2,
  RIT,
  RIT_CLEAR,
  RIT_ENABLE,
  RIT_MINUS,
  RIT_PLUS,
  RIT_RX1,
  RIT_RX2,
  RIT_STEP,
  RSAT,
  SAT,
  SNB,
  SPLIT,
  SQUELCH,
  SQUELCH_RX1,
  SQUELCH_RX2,
  SWAP_RX,
  TUNE,
  TUNE_DRIVE,
  TUNE_FULL,
  TUNE_MEMORY,
  TWO_TONE,
  VFO,
  VFO_STEP_MINUS,
  VFO_STEP_PLUS,
  VFOA,
  VFOB,
  VOX,
  VOXLEVEL,
  WATERFALL_HIGH,
  WATERFALL_LOW,
  XIT,
  XIT_CLEAR,
  XIT_ENABLE,
  XIT_MINUS,
  XIT_PLUS,
  ZOOM,
  ZOOM_MINUS,
  ZOOM_PLUS,
//
// Support for external CW keyers
//
  CW_KEYER_KEYDOWN,
  CW_KEYER_SPEED,
  CW_KEYER_SIDETONE,
  ACTIONS
};

enum ACTIONtype {
  TYPE_NONE=0,
  MIDI_KEY=1,           // MIDI Button (press event)
  MIDI_KNOB=2,          // MIDI Knob   (value between 0 and 100)
  MIDI_WHEEL=4,         // MIDI Wheel  (direction and speed)
  CONTROLLER_SWITCH=8,  // Controller Button
  CONTROLLER_ENCODER=16 // Controller Encoder
};

typedef struct _action_table {
  enum ACTION action;
  const char *str;		// desciptive text
  const char *button_str;	// short button text
  enum ACTIONtype type;
} ACTION_TABLE;

enum ACTION_MODE {
  RELATIVE,
  ABSOLUTE,
  PRESSED,
  RELEASED
};

typedef struct process_action {
  enum ACTION action;
  enum ACTION_MODE mode;
  gint val;
} PROCESS_ACTION;

extern ACTION_TABLE ActionTable[ACTIONS+1];

extern int process_action(void *data);
extern void schedule_action(enum ACTION action, enum ACTION_MODE mode, gint val);

