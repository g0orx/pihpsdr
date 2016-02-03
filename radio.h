
#define MIC_IN 0x00
#define LINE_IN 0x01
#define MIC_BOOST 0x02
#define ORION_MIC_PTT_ENABLED 0x00
#define ORION_MIC_PTT_DISABLED 0x04
#define ORION_MIC_PTT_RING_BIAS_TIP 0x00
#define ORION_MIC_PTT_TIP_BIAS_RING 0x08
#define ORION_MIC_BIAS_DISABLED 0x00
#define ORION_MIC_BIAS_ENABLED 0x10

extern char property_path[];

#define NONE 0

#define ALEX 1
#define APOLLO 2

#define PA_DISABLED 0
#define PA_ENABLED 1

#define APOLLO_TUNER 1

#define KEYER_STRAIGHT 0
#define KEYER_MODE_A 1
#define KEYER_MODE_B 2

extern int sample_rate;
extern int filter_board;
extern int pa;
extern int apollo_tuner;

extern int panadapter_high;
extern int panadapter_low;

extern int waterfall_high;
extern int waterfall_low;
extern int waterfall_automatic;

extern double volume;
extern double mic_gain;
extern int agc;
extern double agc_gain;

extern int nr;
extern int nb;
extern int anf;
extern int snb;

extern int cwPitch;

extern int orion_mic;

extern int tune_drive;
extern int drive;

int receivers;
int adc[2];

int locked;

extern int step;

extern int byte_swap;

extern int cw_keys_reversed;
extern int cw_keyer_speed;
extern int cw_keyer_mode;
extern int cw_keyer_weight;
extern int cw_keyer_spacing;
extern int cw_keyer_internal;
extern int cw_keyer_sidetone_volume;
extern int cw_keyer_ptt_delay;
extern int cw_keyer_hang_time;
extern int cw_keyer_sidetone_frequency;
extern int cw_breakin;

extern void radioRestoreState();
extern void radioSaveState();

