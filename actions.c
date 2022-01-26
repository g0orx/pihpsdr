#include <gtk/gtk.h>
#include <math.h>

#include "main.h"
#include "discovery.h"
#include "receiver.h"
#include "sliders.h"
#include "band_menu.h"
#include "diversity_menu.h"
#include "vfo.h"
#include "radio.h"
#include "radio_menu.h"
#include "new_menu.h"
#include "new_protocol.h"
#ifdef PURESIGNAL
#include "ps_menu.h"
#endif
#include "agc.h"
#include "filter.h"
#include "mode.h"
#include "band.h"
#include "bandstack.h"
#include "noise_menu.h"
#include "wdsp.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "ext.h"
#include "zoompan.h"
#include "actions.h"
#include "gpio.h"
#include "toolbar.h"
#ifdef LOCALCW
#include "iambic.h"
#endif

ACTION_TABLE ActionTable[] = {
  {NO_ACTION,		"NONE",			NULL,		TYPE_NONE},
  {A_SWAP_B,		"A<>B",			"A<>B",		MIDI_KEY | CONTROLLER_SWITCH},
  {A_TO_B,		"A>B",			"A>B",		MIDI_KEY | CONTROLLER_SWITCH},
  {AF_GAIN,		"AF GAIN",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {AF_GAIN_RX1,		"AF GAIN\nRX1",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {AF_GAIN_RX2,		"AF GAIN\nRX2",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {AGC,			"AGC",			"AGC",		MIDI_KEY | CONTROLLER_SWITCH},
  {AGC_GAIN,		"AGC GAIN",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {AGC_GAIN_RX1,	"AGC GAIN\nRX1",	NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {AGC_GAIN_RX2,	"AGC GAIN\nRX2",	NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {ANF,			"ANF",			"ANF",		MIDI_KEY | CONTROLLER_SWITCH},
  {ATTENUATION,		"ATTEN",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {B_TO_A,		"A<B",			"A<B",		MIDI_KEY | CONTROLLER_SWITCH},
  {BAND_10,		"BAND 10",		"10",		MIDI_KEY},
  {BAND_12,		"BAND 12",		"12",		MIDI_KEY},
  {BAND_1240,		"BAND 1240",		"1240",		MIDI_KEY},
  {BAND_144,		"BAND 144",		"144",		MIDI_KEY},
  {BAND_15,		"BAND 15",		"15",		MIDI_KEY},
  {BAND_160,		"BAND 160",		"160",		MIDI_KEY},
  {BAND_17,		"BAND 17",		"17",		MIDI_KEY},
  {BAND_20,		"BAND 20",		"20",		MIDI_KEY},
  {BAND_220,		"BAND 220",		"220",		MIDI_KEY},
  {BAND_2300,		"BAND 2300",		"2300",		MIDI_KEY},
  {BAND_30,		"BAND 30",		"30",		MIDI_KEY},
  {BAND_3400,		"BAND 3400",		"3400",		MIDI_KEY},
  {BAND_40,		"BAND 40",		"40",		MIDI_KEY},
  {BAND_430,		"BAND 430",		"430",		MIDI_KEY},
  {BAND_6,		"BAND 6",		"6",		MIDI_KEY},
  {BAND_60,		"BAND 60",		"60",		MIDI_KEY},
  {BAND_70,		"BAND 70",		"70",		MIDI_KEY},
  {BAND_80,		"BAND 80",		"80",		MIDI_KEY},
  {BAND_902,		"BAND 902",		"902",		MIDI_KEY},
  {BAND_AIR,		"BAND AIR",		"AIR",		MIDI_KEY},
  {BAND_GEN,		"BAND GEN",		"GEN",		MIDI_KEY},
  {BAND_MINUS,		"BAND -",		"BND-",		MIDI_KEY | CONTROLLER_SWITCH},
  {BAND_PLUS,		"BAND +",		"BND+",		MIDI_KEY | CONTROLLER_SWITCH},
  {BAND_WWV,		"BAND WWV",		"WWV",		MIDI_KEY},
  {BANDSTACK_MINUS,	"BANDSTACK -",		"BSTK-",	MIDI_KEY | CONTROLLER_SWITCH},
  {BANDSTACK_PLUS,	"BANDSTACK +",		"BSTK+",	MIDI_KEY | CONTROLLER_SWITCH},
  {COMP_ENABLE,		"COMP ON/OFF",		"COMP",		MIDI_KEY | CONTROLLER_SWITCH},
  {COMPRESSION,		"COMPRESSION",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {CTUN,		"CTUN",			"CTUN",		MIDI_KEY | CONTROLLER_SWITCH},
  {CW_FREQUENCY,	"CW FREQUENCY",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {CW_LEFT,		"CW LEFT",		"CWL",		MIDI_KEY | CONTROLLER_SWITCH},
  {CW_RIGHT,		"CW RIGHT",		"CWR",		MIDI_KEY | CONTROLLER_SWITCH},
  {CW_SPEED,		"CW SPEED",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {DIV,			"DIV ON/OFF",		"DIV",		MIDI_KEY | CONTROLLER_SWITCH},
  {DIV_GAIN,		"DIV GAIN",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {DIV_GAIN_COARSE,	"DIV GAIN\nCOARSE",	NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {DIV_GAIN_FINE,	"DIV GAIN\nFINE",	NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {DIV_PHASE,		"DIV PHASE",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {DIV_PHASE_COARSE,	"DIV PHASE\nCOARSE",	NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {DIV_PHASE_FINE,	"DIV PHASE\nFINE",	NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {DRIVE,		"TX DRIVE",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {DUPLEX,		"DUPLEX",		"DUP",		MIDI_KEY | CONTROLLER_SWITCH},
  {FILTER_MINUS,	"FILTER -",		"FL-",		MIDI_KEY | CONTROLLER_SWITCH},
  {FILTER_PLUS,		"FILTER +",		"FL+",		MIDI_KEY | CONTROLLER_SWITCH},
  {FUNCTION,		"FUNC",			"FUNC",		CONTROLLER_SWITCH},
  {IF_SHIFT,		"IF SHIFT",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {IF_SHIFT_RX1,	"IF SHIFT\nRX1",	NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {IF_SHIFT_RX2,	"IF SHIFT\nRX2",	NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {IF_WIDTH,		"IF WIDTH",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {IF_WIDTH_RX1,	"IF WIDTH\nRX1",	NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {IF_WIDTH_RX2,	"IF WIDTH\nRX2",	NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {LINEIN_GAIN,		"LINEIN\nGAIN",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {LOCK,		"LOCK",			"LOCK",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_AGC,		"AGC\nMENU",		"AGC",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_BAND,		"BAND\nMENU",		"BAND",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_BANDSTACK,	"BSTK\nMENU",		"BSTK",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_DIVERSITY,	"DIV\nMENU",		"DIV",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_FILTER,		"FILT\nMENU",		"FILT",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_FREQUENCY,	"FREQ\nMENU",		"FREQ",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_MEMORY,		"MEM\nMENU",		"MEM",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_MODE,		"MODE\nMENU",		"MODE",		MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_NOISE,		"NOISE\nMENU",		"NOISE",	MIDI_KEY | CONTROLLER_SWITCH},
  {MENU_PS,		"PS MENU",		"PS",		MIDI_KEY | CONTROLLER_SWITCH},
  {MIC_GAIN,		"MIC GAIN",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {MODE_MINUS,		"MODE -",		"MD-",		MIDI_KEY | CONTROLLER_SWITCH},
  {MODE_PLUS,		"MODE +",		"MD+",		MIDI_KEY | CONTROLLER_SWITCH},
  {MOX,			"MOX",			"MOX",		MIDI_KEY | CONTROLLER_SWITCH},
  {MUTE,		"MUTE",			"MUTE",		MIDI_KEY | CONTROLLER_SWITCH},
  {NB,			"NB",			"NB",		MIDI_KEY | CONTROLLER_SWITCH},
  {NR,			"NR",			"NR",		MIDI_KEY | CONTROLLER_SWITCH},
  {NUMPAD_0,		"NUMPAD 0",		"0",		MIDI_KEY},
  {NUMPAD_1,		"NUMPAD 1",		"1",		MIDI_KEY},
  {NUMPAD_2,		"NUMPAD 2",		"2",		MIDI_KEY},
  {NUMPAD_3,		"NUMPAD 3",		"3",		MIDI_KEY},
  {NUMPAD_4,		"NUMPAD 4",		"4",		MIDI_KEY},
  {NUMPAD_5,		"NUMPAD 5",		"5",		MIDI_KEY},
  {NUMPAD_6,		"NUMPAD 6",		"6",		MIDI_KEY},
  {NUMPAD_7,		"NUMPAD 7",		"7",		MIDI_KEY},
  {NUMPAD_8,		"NUMPAD 8",		"8",		MIDI_KEY},
  {NUMPAD_9,		"NUMPAD 9",		"9",		MIDI_KEY},
  {NUMPAD_CL,		"NUMPAD\nCL",		"CL",		MIDI_KEY},
  {NUMPAD_ENTER,	"NUMPAD\nENTER",	"EN",		MIDI_KEY},
  {PAN,			"PAN",			NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {PAN_MINUS,		"PAN -",		"PAN-",		MIDI_KEY | CONTROLLER_SWITCH},
  {PAN_PLUS,		"PAN +",		"PAN+",		MIDI_KEY | CONTROLLER_SWITCH},
  {PANADAPTER_HIGH,	"PAN HIGH",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {PANADAPTER_LOW,	"PAN LOW",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {PANADAPTER_STEP,	"PAN STEP",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {PREAMP,		"PREAMP\nON/OFF",	"PRE",		MIDI_KEY | CONTROLLER_SWITCH},
  {PS,			"PS ON/OFF",		"PS",		MIDI_KEY | CONTROLLER_SWITCH},
  {PTT,			"PTT",			"PTT",		MIDI_KEY | CONTROLLER_SWITCH},
  {RF_GAIN,		"RF GAIN",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {RF_GAIN_RX1,		"RF GAIN\nRX1",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {RF_GAIN_RX2,		"RF GAIN\nRX2",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {RIT,			"RIT",			NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {RIT_CLEAR,		"RIT\nCLEAR",		"RITCL",	MIDI_KEY | CONTROLLER_SWITCH},
  {RIT_ENABLE,		"RIT\nON/OFF",		"RIT",		MIDI_KEY | CONTROLLER_SWITCH},
  {RIT_MINUS,		"RIT +",		"RIT-",		MIDI_KEY | CONTROLLER_SWITCH},
  {RIT_PLUS,		"RIT -",		"RIT+",		MIDI_KEY | CONTROLLER_SWITCH},
  {RIT_RX1,		"RIT\nRX1",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {RIT_RX2,		"RIT\nRX2",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {RIT_STEP,		"RIT\nSTEP",		"RITST",	MIDI_KEY | CONTROLLER_SWITCH},
  {RSAT,		"RSAT",			"RSAT",		MIDI_KEY | CONTROLLER_SWITCH},
  {SAT,			"SAT",			"SAT",		MIDI_KEY | CONTROLLER_SWITCH},
  {SNB,			"SNB",			"SNB",		MIDI_KEY | CONTROLLER_SWITCH},
  {SPLIT,		"SPLIT",		"SPLIT",	MIDI_KEY | CONTROLLER_SWITCH},
  {SQUELCH,		"SQUELCH",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {SQUELCH_RX1,		"SQUELCH\nRX1",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {SQUELCH_RX2,		"SQUELCH\nRX1",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {SWAP_RX,		"SWAP RX",		"SWAPRX",	MIDI_KEY | CONTROLLER_SWITCH},
  {TUNE,		"TUNE",			"TUNE",		MIDI_KEY | CONTROLLER_SWITCH},
  {TUNE_DRIVE,		"TUNE\nDRV",		NULL,		MIDI_KNOB | MIDI_WHEEL | CONTROLLER_ENCODER},
  {TUNE_FULL,		"TUNE\nFUL",		"TUNF",		MIDI_KEY | CONTROLLER_SWITCH},
  {TUNE_MEMORY,		"TUNE\nMEM",		"TUNM",		MIDI_KEY | CONTROLLER_SWITCH},
  {TWO_TONE,		"TWO TONE",		"2TONE",	MIDI_KEY | CONTROLLER_SWITCH},
  {VFO,			"VFO",			NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {VFO_STEP_MINUS,	"VFO STEP -",		"STEP-",	MIDI_KEY | CONTROLLER_SWITCH},
  {VFO_STEP_PLUS,	"VFO STEP +",		"STEP+",	MIDI_KEY | CONTROLLER_SWITCH},
  {VFOA,		"VFO A",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {VFOB,		"VFO B",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {VOX,			"VOX\nON/OFF",		"VOX",		MIDI_KEY | CONTROLLER_SWITCH},
  {VOXLEVEL,		"VOX\nLEVEL",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {WATERFALL_HIGH,	"WFALL\nHIGH",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {WATERFALL_LOW,	"WFALL\nLOW",		NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {XIT	,		"XIT",			NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {XIT_CLEAR,		"XIT\nCLEAR",		"XITCL",	MIDI_KEY | CONTROLLER_SWITCH},
  {XIT_ENABLE,		"XIT\nON/OFF",		"XIT",		MIDI_KEY | CONTROLLER_SWITCH},
  {XIT_MINUS,		"XIT -",		"XIT-",		MIDI_KEY | CONTROLLER_SWITCH},
  {XIT_PLUS,		"XIT +",		"XIT+",		MIDI_KEY | CONTROLLER_SWITCH},
  {ZOOM,		"ZOOM",			NULL,		MIDI_WHEEL | CONTROLLER_ENCODER},
  {ZOOM_MINUS,		"ZOOM -",		"ZOOM-",	MIDI_KEY | CONTROLLER_SWITCH},
  {ZOOM_PLUS,		"ZOOM +",		"ZOOM+",	MIDI_KEY | CONTROLLER_SWITCH},
//
// The following actions support external CW keyers generating
// the following messages:
//
// CW Keydown (MIDI and GPIO)
// CW speed   (only MIDI)
// CW side tone frequency (only MIDI)
//
  {CW_KEYER_KEYDOWN,    "CW Key\n(keyer)",     NULL,           MIDI_KEY | CONTROLLER_SWITCH},
  {CW_KEYER_SPEED,      "CW Speed\n(keyer)",       NULL,           MIDI_KNOB},
  {CW_KEYER_SIDETONE,   "CW pitch\n(keyer)",     NULL,           MIDI_KNOB},
  {ACTIONS,		"",			NULL,		TYPE_NONE}
};

static gint timer=0;
static gboolean timer_released;

static int timeout_cb(gpointer data) {
  if(timer_released) {
    g_free(data);
    timer=0;
    return FALSE;
  }
  // process the action;
  process_action(data);
  return TRUE;
}

static inline double KnobOrWheel(PROCESS_ACTION *a, double oldval, double minval, double maxval, double inc) {
  //
  // Knob ("Potentiometer"):  set value
  // Wheel("Rotary Encoder"): increment/decrement the value (by "inc" per tick)
  //
  // In both cases, the returned value is
  //  - in the range minval...maxval
  //  - rounded to a multiple of inc
  //
  switch (a->mode) {
    case RELATIVE:
      oldval += a->val * inc;
      break;
    case ABSOLUTE:
      oldval = minval + a->val*(maxval-minval)*0.01;
      break;
    default:
      // do nothing
      break;
  }
  //
  // Round and check range
  //
  oldval=inc*round(oldval/inc);
  if (oldval > maxval) oldval=maxval;
  if (oldval < minval) oldval=minval;
  return oldval;
}

//
// This interface puts an "action" into the GTK idle queue,
// but CW actions are processed immediately
//
void schedule_action(enum ACTION action, enum ACTION_MODE mode, gint val) {
  PROCESS_ACTION *a;
  switch (action) {
    case CW_LEFT:
    case CW_RIGHT:
#ifdef LOCALCW
      keyer_event(action==CW_LEFT,val);
#else
      g_print("CW_Left/Right but compiled without LOCALCW\n");
#endif
      break;
    case CW_KEYER_KEYDOWN:
      //
      // hard "key-up/down" action WITHOUT break-in
      // intended for external keyers (MIDI or GPIO connected)
      // which take care of PTT themselves
      //
      if (val != 0 && cw_keyer_internal == 0) {
        cw_key_down=960000;  // max. 20 sec to protect hardware
        cw_key_up=0;
        cw_key_hit=1;
      } else {
        cw_key_down=0;
        cw_key_up=0;
      }
      break;
    default:
      //
      // schedule action through GTK idle queue
      //
      a=g_new(PROCESS_ACTION, 1);
      a->action=action;
      a->mode=mode;
      a->val=val;
      g_idle_add(process_action, a);
      break;
  }
}

int process_action(void *data) {
  PROCESS_ACTION *a=(PROCESS_ACTION *)data;
  double value;
  int i;
  gboolean free_action=TRUE;

  //g_print("%s: action=%d mode=%d value=%d\n",__FUNCTION__,a->action,a->mode,a->val);
  switch(a->action) {

    case A_SWAP_B:
      if(a->mode==PRESSED) {
        vfo_a_swap_b();
      }
      break;
    case A_TO_B:
      if(a->mode==PRESSED) {
        vfo_a_to_b();
      }
      break;
    case AF_GAIN:
      value=KnobOrWheel(a, active_receiver->volume, 0.0, 1.0, 0.01);
      set_af_gain(active_receiver->id,value);
      break;
    case AF_GAIN_RX1:
      value=KnobOrWheel(a, receiver[0]->volume, 0.0, 1.0, 0.01);
      set_af_gain(0,value);
      break;
    case AF_GAIN_RX2:
      value=KnobOrWheel(a, receiver[1]->volume, 0.0, 1.0, 0.01);
      set_af_gain(1,value);
      break;
    case AGC:
      if(a->mode==PRESSED) {
        active_receiver->agc++;
        if(active_receiver->agc>+AGC_LAST) {
          active_receiver->agc=0;
        }
        set_agc(active_receiver, active_receiver->agc);
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case AGC_GAIN:
      value=KnobOrWheel(a, active_receiver->agc_gain, -20.0, 120.0, 1.0);
      set_agc_gain(active_receiver->id,value);
      break;
    case AGC_GAIN_RX1:
      value=KnobOrWheel(a, receiver[0]->agc_gain, -20.0, 120.0, 1.0);
      set_agc_gain(0,value);
      break;
    case AGC_GAIN_RX2:
      value=KnobOrWheel(a, receiver[1]->agc_gain, -20.0, 120.0, 1.0);
      set_agc_gain(1,value);
      break;
    case ANF:
      if(a->mode==PRESSED) {
        if(active_receiver->anf==0) {
          active_receiver->anf=1;
          mode_settings[vfo[active_receiver->id].mode].anf=1;
        } else {
          active_receiver->anf=0;
          mode_settings[vfo[active_receiver->id].mode].anf=0;
        }
        SetRXAANFRun(active_receiver->id, active_receiver->anf);
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case ATTENUATION:
      value=KnobOrWheel(a, adc[active_receiver->adc].attenuation,   0.0, 31.0, 1.0);
      set_attenuation_value(value);
      break;
    case B_TO_A:
      if(a->mode==PRESSED) {
        vfo_b_to_a();
      }
      break;
    case BAND_10:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band160);
      }
      break;
    case BAND_12:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band12);
      }
      break;
#ifdef SOAPYSDR
    case BAND_1240:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band1240);
      }
      break;
    case BAND_144:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band144);
      }
      break;
#endif
    case BAND_15:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band15);
      }
      break;
    case BAND_160:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band160);
      }
      break;
    case BAND_17:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band17);
      }
      break;
    case BAND_20:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band20);
      }
      break;
#ifdef SOAPYSDR
    case BAND_220:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band220);
      }
      break;
    case BAND_2300:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band2300);
      }
      break;
#endif
    case BAND_30:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band30);
      }
      break;
#ifdef SOAPYSDR
    case BAND_3400:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band3400);
      }
      break;
#endif
    case BAND_40:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band40);
      }
      break;
#ifdef SOAPYSDR
    case BAND_430:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band430);
      }
      break;
#endif
    case BAND_6:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band6);
      }
      break;
    case BAND_60:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band60);
      }
      break;
#ifdef SOAPYSDR
    case BAND_70:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band70);
      }
      break;
#endif
    case BAND_80:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band80);
      }
      break;
#ifdef SOAPYSDR
    case BAND_902:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,band902);
      }
      break;
    case BAND_AIR:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,bandAIR);
      }
      break;
#endif
    case BAND_GEN:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,bandGen);
      }
      break;
    case BAND_MINUS:
      if(a->mode==PRESSED) {
        long long frequency_min=radio->frequency_min;
        long long frequency_max=radio->frequency_max;
        int b=vfo[active_receiver->id].band;
        BAND *band;
        int found=0;
        while(!found) {
          b--;
          if(b<0) b=BANDS+XVTRS-1;
          band=(BAND*)band_get_band(b);
          if(strlen(band->title)>0) {
            if(b<BANDS) {
              if(band->frequencyMin<frequency_min || band->frequencyMax>frequency_max) {
                continue;
              }
            }
            vfo_band_changed(active_receiver->id,b);
            found=1;
          }
        }
      }
      break;
    case BAND_PLUS:
      if(a->mode==PRESSED) {
        long long frequency_min=radio->frequency_min;
        long long frequency_max=radio->frequency_max;
        int b=vfo[active_receiver->id].band;
        BAND *band;
        int found=0;
        while(!found) {
          b++;
          if(b>=BANDS+XVTRS) b=0;
          band=(BAND*)band_get_band(b);
          if(strlen(band->title)>0) {
            if(b<BANDS) {
              if(!(band->frequencyMin==0.0 && band->frequencyMax==0.0)) {
                if(band->frequencyMin<frequency_min || band->frequencyMax>frequency_max) {
                  continue;
                }
              }
            }
            vfo_band_changed(active_receiver->id,b);
            found=1;
          }
        }
      }
      break;
    case BAND_WWV:
      if(a->mode==PRESSED) {
        vfo_band_changed(active_receiver->id,bandWWV);
      }
      break;
    case BANDSTACK_MINUS:
      if(a->mode==PRESSED) {
        BAND *band=band_get_band(vfo[active_receiver->id].band);
        BANDSTACK *bandstack=band->bandstack;
        int b=vfo[active_receiver->id].bandstack-1;
        if(b<0) b=bandstack->entries-1;;
        vfo_bandstack_changed(b);
      }
      break;
    case BANDSTACK_PLUS:
      if(a->mode==PRESSED) {
        BAND *band=band_get_band(vfo[active_receiver->id].band);
        BANDSTACK *bandstack=band->bandstack;
        int b=vfo[active_receiver->id].bandstack+1;
        if(b>=bandstack->entries) b=0;
        vfo_bandstack_changed(b);
      }
      break;
    case COMP_ENABLE:
      if(can_transmit && a->mode==PRESSED) {
        transmitter_set_compressor(transmitter,transmitter->compressor?FALSE:TRUE);
      }
      break;
    case COMPRESSION:
      if(can_transmit) {
        value=KnobOrWheel(a, transmitter->compressor_level, 0.0, 20.0, 1.0);
        transmitter_set_compressor_level(transmitter,value);
        transmitter_set_compressor(transmitter, value > 0.5);
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case CTUN:
      if(a->mode==PRESSED) {
        vfo[active_receiver->id].ctun=!vfo[active_receiver->id].ctun;
        if(!vfo[active_receiver->id].ctun) {
          vfo[active_receiver->id].offset=0;
        }
        vfo[active_receiver->id].ctun_frequency=vfo[active_receiver->id].frequency;
        set_offset(receiver[active_receiver->id],vfo[active_receiver->id].offset);
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case CW_FREQUENCY:
      value=KnobOrWheel(a, (double)cw_keyer_sidetone_frequency, 300.0, 1000.0, 10.0);
      cw_keyer_sidetone_frequency=(int)value;
      g_idle_add(ext_vfo_update,NULL);
      break;
    case CW_SPEED:
      value=KnobOrWheel(a, (double)cw_keyer_speed, 1.0, 60.0, 1.0);
      cw_keyer_speed=(int)value;
      g_idle_add(ext_vfo_update,NULL);
      break;
    case DIV:
      if(a->mode==PRESSED) {
        diversity_enabled=diversity_enabled==1?0:1;
        if (protocol == NEW_PROTOCOL) {
          schedule_high_priority();
          schedule_receive_specific();
        }
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case DIV_GAIN:
      update_diversity_gain((double)a->val * 0.5);
      break;
    case DIV_GAIN_COARSE:
      update_diversity_gain((double)a->val * 2.5);
      break;
    case DIV_GAIN_FINE:
      update_diversity_gain((double)a->val * 0.1);
      break;
    case DIV_PHASE:
      update_diversity_phase((double)a->val* 0.5);
      break;
    case DIV_PHASE_COARSE:
      update_diversity_phase((double)a->val*2.5);
      break;
    case DIV_PHASE_FINE:
      update_diversity_phase((double)a->val*0.1);
      break;
    case DRIVE:
      value=KnobOrWheel(a, getDrive(), 0.0, drive_max, 1.0);
      set_drive(value);
      break;
    case DUPLEX:
      if(can_transmit && !isTransmitting() && a->mode == PRESSED) {
        duplex=duplex==1?0:1;
        g_idle_add(ext_set_duplex, NULL);
      }
      break;
    case FILTER_MINUS:
      if(a->mode==PRESSED) {
        int f=vfo[active_receiver->id].filter+1;
        if(f>=FILTERS) f=0;
        vfo_filter_changed(f);
      }
      break;
    case FILTER_PLUS:
      if(a->mode==PRESSED) {
        int f=vfo[active_receiver->id].filter-1;
        if(f<0) f=FILTERS-1;
        vfo_filter_changed(f);
      }
      break;
    case FUNCTION:
      if(a->mode==PRESSED) {
        switch(controller) {
          case NO_CONTROLLER:
          case CONTROLLER1:
            function++;
            if(function>=MAX_FUNCTIONS) {
              function=0;
            }
            toolbar_switches=switches_controller1[function];
            switches=switches_controller1[function];
            update_toolbar_labels();
	    break;
	  case CONTROLLER2_V1:
	  case CONTROLLER2_V2:
            function++;
            if(function>=MAX_FUNCTIONS) {
              function=0;
            }
            toolbar_switches=switches_controller1[function];
            update_toolbar_labels();
	    break;
        }
      }
      break;
    case IF_SHIFT:
      filter_shift_changed(active_receiver->id,a->val);
      break;
    case IF_SHIFT_RX1:
      filter_shift_changed(0,a->val);
      break;
    case IF_SHIFT_RX2:
      filter_shift_changed(1,a->val);
      break;
    case IF_WIDTH:
      filter_width_changed(active_receiver->id,a->val);
      break;
    case IF_WIDTH_RX1:
      filter_width_changed(0,a->val);
      break;
    case IF_WIDTH_RX2:
      filter_width_changed(1,a->val);
      break;
    //case LINEIN_GAIN:
    case LOCK:
      if(a->mode==PRESSED) {
#ifdef CLIENT_SERVER
        if(radio_is_remote) {
          send_lock(client_socket,locked==1?0:1);
        } else {
#endif
          locked=locked==1?0:1;
          g_idle_add(ext_vfo_update, NULL);
#ifdef CLIENT_SERVER
        }
#endif
      }
      break;
    case MENU_AGC:
      if(a->mode==PRESSED) {
        start_agc();
      }
      break;
    case MENU_BAND:
      if(a->mode==PRESSED) {
        start_band();
      }
      break;
    case MENU_BANDSTACK:
      if(a->mode==PRESSED) {
        start_bandstack();
      }
      break;
    case MENU_DIVERSITY:
      if(a->mode==PRESSED) {
        start_diversity();
      }
      break;
    case MENU_FILTER:
      if(a->mode==PRESSED) {
        start_filter();
      }
      break;
    case MENU_FREQUENCY:
      if(a->mode==PRESSED) {
        start_vfo(active_receiver->id);
      }
      break;
    case MENU_MEMORY:
      if(a->mode==PRESSED) {
        start_store();
      }
      break;
    case MENU_MODE:
      if(a->mode==PRESSED) {
        start_mode();
      }
      break;
    case MENU_NOISE:
      if(a->mode==PRESSED) {
        start_noise();
      }
      break;
#ifdef PURESIGNAL
    case MENU_PS:
      if(a->mode==PRESSED) {
        start_ps();
      }
      break;
#endif
    case MIC_GAIN:
      value=KnobOrWheel(a, mic_gain, -12.0, 50.0, 1.0);
      set_mic_gain(value);
      break;
    case MODE_MINUS:
      if(a->mode==PRESSED) {
        int mode=vfo[active_receiver->id].mode;
        mode--;
        if(mode<0) mode=MODES-1;
        vfo_mode_changed(mode);
      }
      break;
    case MODE_PLUS:
      if(a->mode==PRESSED) {
        int mode=vfo[active_receiver->id].mode;
        mode++;
        if(mode>=MODES) mode=0;
        vfo_mode_changed(mode);
      }
      break;
    case MOX:
      if(a->mode==PRESSED) {
        int state=getMox();
        mox_update(!state);
      }
      break;
    case MUTE:
      if(a->mode==PRESSED) {
        active_receiver->mute_radio=!active_receiver->mute_radio;
      }
      break;
    case NB:
      if(a->mode==PRESSED) {
        if(active_receiver->nb==0 && active_receiver->nb2==0) {
          active_receiver->nb=1;
          active_receiver->nb2=0;
          mode_settings[vfo[active_receiver->id].mode].nb=1;
          mode_settings[vfo[active_receiver->id].mode].nb2=0;
        } else if(active_receiver->nb==1 && active_receiver->nb2==0) {
          active_receiver->nb=0;
          active_receiver->nb2=1;
          mode_settings[vfo[active_receiver->id].mode].nb=0;
          mode_settings[vfo[active_receiver->id].mode].nb2=1;
        } else if(active_receiver->nb==0 && active_receiver->nb2==1) {
          active_receiver->nb=0;
          active_receiver->nb2=0;
          mode_settings[vfo[active_receiver->id].mode].nb=0;
          mode_settings[vfo[active_receiver->id].mode].nb2=0;
        }
        update_noise();
      }
      break;
    case NR:
      if(a->mode==PRESSED) {
        if(active_receiver->nr==0 && active_receiver->nr2==0) {
          active_receiver->nr=1;
          active_receiver->nr2=0;
          mode_settings[vfo[active_receiver->id].mode].nr=1;
          mode_settings[vfo[active_receiver->id].mode].nr2=0;
        } else if(active_receiver->nr==1 && active_receiver->nr2==0) {
          active_receiver->nr=0;
          active_receiver->nr2=1;
          mode_settings[vfo[active_receiver->id].mode].nr=0;
          mode_settings[vfo[active_receiver->id].mode].nr2=1;
        } else if(active_receiver->nr==0 && active_receiver->nr2==1) {
          active_receiver->nr=0;
          active_receiver->nr2=0;
          mode_settings[vfo[active_receiver->id].mode].nr=0;
          mode_settings[vfo[active_receiver->id].mode].nr2=0;
        }
        update_noise();
      }
      break;
    case NUMPAD_0:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(0));
      }
      break;
    case NUMPAD_1:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(1));
      }
      break;
    case NUMPAD_2:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(2));
      }
      break;
    case NUMPAD_3:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(3));
      }
      break;
    case NUMPAD_4:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(4));
      }
      break;
    case NUMPAD_5:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(5));
      }
      break;
    case NUMPAD_6:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(6));
      }
      break;
    case NUMPAD_7:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(7));
      }
      break;
    case NUMPAD_8:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(8));
      }
      break;
    case NUMPAD_9:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(9));
      }
      break;
    case NUMPAD_CL:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(-1));
      }
      break;
    case NUMPAD_ENTER:
      if(a->mode==PRESSED) {
        g_idle_add(ext_num_pad,GINT_TO_POINTER(-2));
      }
      break;
    case PAN:
      update_pan((double)a->val*100);
      break;
    case PAN_MINUS:
      if(a->mode==PRESSED) {
        update_pan(-100.0);
      }
      break;
    case PAN_PLUS:
       if(a->mode==PRESSED) {
         update_pan(+100.0);
       }
       break;
    case PANADAPTER_HIGH:
      value=KnobOrWheel(a, active_receiver->panadapter_high, -60.0, 20.0, 1.0);
      active_receiver->panadapter_high=(int)value;
      break;
    case PANADAPTER_LOW:
      value=KnobOrWheel(a, active_receiver->panadapter_low, -160.0, -60.0, 1.0);
      active_receiver->panadapter_low=(int)value;
      break;
    case PANADAPTER_STEP:
      value=KnobOrWheel(a, active_receiver->panadapter_step, 5.0, 30.0, 1.0);
      active_receiver->panadapter_step=(int)value;
      break;
    case PREAMP:
      break;
    case PS:
#ifdef PURESIGNAL
      if(a->mode==PRESSED) {
        if(can_transmit) {
          if(transmitter->puresignal==0) {
            tx_set_ps(transmitter,1);
          } else {
            tx_set_ps(transmitter,0);
          }
        }
      }
#endif
      break;
    case PTT:
      if(a->mode==PRESSED || a->mode==RELEASED) {
	mox_update(a->mode==PRESSED);
      }
      break;
    case RF_GAIN:
      value=KnobOrWheel(a, adc[active_receiver->adc].gain, adc[active_receiver->adc].min_gain, adc[active_receiver->adc].max_gain, 1.0);
      set_rf_gain(active_receiver->id,value);
      break;
    case RF_GAIN_RX1:
      value=KnobOrWheel(a, adc[receiver[0]->adc].gain, adc[receiver[0]->adc].min_gain, adc[receiver[0]->adc].max_gain, 1.0);
      set_rf_gain(0,value);
      break;
    case RF_GAIN_RX2:
      value=KnobOrWheel(a, adc[receiver[1]->adc].gain, adc[receiver[1]->adc].min_gain, adc[receiver[1]->adc].max_gain, 1.0);
      set_rf_gain(1,value);
      break;
    case RIT:
      vfo_rit(active_receiver->id,a->val);
      break;
    case RIT_CLEAR:
      if(a->mode==PRESSED) {
        vfo_rit_clear(active_receiver->id);
      }
      break;
    case RIT_ENABLE:
      if(a->mode==PRESSED) {
        vfo_rit_update(active_receiver->id);
      }
      break;
    case RIT_MINUS:
      if(a->mode==PRESSED) {
        vfo_rit(active_receiver->id,-1);
	if(timer==0) {
  	  timer=g_timeout_add(250,timeout_cb,a);
	  timer_released=FALSE;
	}
	free_action=FALSE;
      } else {
	timer_released=TRUE;
      }
      break;
    case RIT_PLUS:
      if(a->mode==PRESSED) {
        vfo_rit(active_receiver->id,1);
	if(timer==0) {
	  timer=g_timeout_add(250,timeout_cb,a);
	  timer_released=FALSE;
	}
	free_action=FALSE;
      } else {
	timer_released=TRUE;
      }
      break;
    case RIT_RX1:
      vfo_rit(receiver[0]->id,a->val);
      break;
    case RIT_RX2:
      vfo_rit(receiver[1]->id,a->val);
      break;
    case RIT_STEP:
      switch(a->mode) {
        case PRESSED:
	  a->val=1;
	  // fall through
	case RELATIVE:
	  if(a->val>0) {
            rit_increment=10*rit_increment;
	  } else {
            rit_increment=rit_increment/10;
	  }
	  if(rit_increment<1) rit_increment=100;
	  if(rit_increment>100) rit_increment=1;
	  break;
	default:
	  // ignore other types
	  break;
      }
      g_idle_add(ext_vfo_update,NULL);
      break;
    case RSAT:
      if(a->mode==PRESSED) {
        if(sat_mode==RSAT_MODE) {
          sat_mode=SAT_NONE;
        } else {
          sat_mode=RSAT_MODE;
        }
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case SAT:
      if(a->mode==PRESSED) {
	if(sat_mode==SAT_MODE) {
	  sat_mode=SAT_NONE;
	} else {
	  sat_mode=SAT_MODE;
	}
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case SNB:
      if(a->mode==PRESSED) {
        if(active_receiver->snb==0) {
          active_receiver->snb=1;
          mode_settings[vfo[active_receiver->id].mode].snb=1;
        } else {
          active_receiver->snb=0;
        mode_settings[vfo[active_receiver->id].mode].snb=0;
        }
        update_noise();
      }
      break;
    case SPLIT:
      if(a->mode==PRESSED) {
        if(can_transmit) {
          split=split==1?0:1;
          tx_set_mode(transmitter,get_tx_mode());
          g_idle_add(ext_vfo_update, NULL);
        }
      }
      break;
    case SQUELCH:
      value=KnobOrWheel(a, active_receiver->squelch, 0.0, 100.0, 1.0);
      active_receiver->squelch=value;
      set_squelch(active_receiver);
      break;
    case SQUELCH_RX1:
      value=KnobOrWheel(a, receiver[0]->squelch, 0.0, 100.0, 1.0);
      receiver[0]->squelch=value;
      set_squelch(receiver[0]);
      break;
    case SQUELCH_RX2:
      value=KnobOrWheel(a, receiver[1]->squelch, 0.0, 100.0, 1.0);
      receiver[1]->squelch=value;
      set_squelch(receiver[1]);
      break;
    case SWAP_RX:
      if(a->mode==PRESSED) {
        if(receivers==2) {
          active_receiver=receiver[active_receiver->id==1?0:1];
          g_idle_add(menu_active_receiver_changed,NULL);
          g_idle_add(ext_vfo_update,NULL);
          g_idle_add(sliders_active_receiver_changed,NULL);
        }
      }
      break;
    case TUNE:
      if(a->mode==PRESSED) {
        int state=getTune();
        tune_update(!state);
      }
      break;
    case TUNE_DRIVE:
      if(can_transmit) {
        value=KnobOrWheel(a, (double) transmitter->tune_percent, 0.0, 100.0, 1.0);
        transmitter->tune_percent=(int) value;
      }
      break;
    case TUNE_FULL:
      if(a->mode==PRESSED) {
        if(can_transmit) {
	  full_tune=full_tune?FALSE:TRUE;
	  memory_tune=FALSE;
	}
      }
      break;
    case TUNE_MEMORY:
      if(a->mode==PRESSED) {
        if(can_transmit) {
	  memory_tune=memory_tune?FALSE:TRUE;
	  full_tune=FALSE;
	}
      }
      break;
    case TWO_TONE:
      if(a->mode==PRESSED) {
        if(can_transmit) {
          int state=transmitter->twotone?0:1;
          tx_set_twotone(transmitter,state);
        }
      }
      break;
    case VFO:
      if(a->mode==RELATIVE && !locked) {
        vfo_step(a->val);
      }
      break;
    case VFO_STEP_MINUS:
      if(a->mode==PRESSED) {
        i=vfo_get_stepindex();
        vfo_set_step_from_index(--i);
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case VFO_STEP_PLUS:
      if(a->mode==PRESSED) {
        i=vfo_get_stepindex();
        vfo_set_step_from_index(++i);
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case VFOA:
      if(a->mode==RELATIVE && !locked) {
	vfo_id_step(0,(int)a->val);
      }
      break;
    case VFOB:
      if(a->mode==RELATIVE && !locked) {
	vfo_id_step(1,(int)a->val);
      }
      break;
    case VOX:
      if(a->mode==PRESSED) {
        vox_enabled = !vox_enabled;
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case VOXLEVEL:
      vox_threshold=KnobOrWheel(a, vox_threshold, 0.0, 1.0, 0.01);
      break;
    case WATERFALL_HIGH:
      value=KnobOrWheel(a, active_receiver->waterfall_high, -100.0, 0.0, 1.0);
      active_receiver->waterfall_high=(int)value;
      break;
    case WATERFALL_LOW:
      value=KnobOrWheel(a, active_receiver->waterfall_low, -150.0, -50.0, 1.0);
      active_receiver->waterfall_low=(int)value;
      break;
    case XIT:
      value=KnobOrWheel(a, (double)transmitter->xit, -10000.0, 10000.0, (double) rit_increment);
      transmitter->xit=(int)value;
      transmitter->xit_enabled=(value!=0);
      if(protocol==NEW_PROTOCOL) {
        schedule_high_priority();
      }
      g_idle_add(ext_vfo_update,NULL);
      break;
    case XIT_CLEAR:
      if(a->mode==PRESSED) {
        if(can_transmit) {
          transmitter->xit=0;
          transmitter->xit_enabled=0;
          if(protocol==NEW_PROTOCOL) {
            schedule_high_priority();
          }
          g_idle_add(ext_vfo_update, NULL);
        }
      }
      break;
    case XIT_ENABLE:
      if(a->mode==PRESSED) {
        if(can_transmit) {
          transmitter->xit_enabled=transmitter->xit_enabled==1?0:1;
          if(protocol==NEW_PROTOCOL) {
            schedule_high_priority();
          }
        }
        g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case XIT_MINUS:
      if(a->mode==PRESSED) {
        if(can_transmit) {
          value=(double)transmitter->xit;
          value-=(double)rit_increment;
          if(value<-10000.0) {
            value=-10000.0;
          } else if(value>10000.0) {
            value=10000.0;
          }
          transmitter->xit=(int)value;
	  transmitter->xit_enabled=(transmitter->xit!=0);
          if(protocol==NEW_PROTOCOL) {
            schedule_high_priority();
          }
          g_idle_add(ext_vfo_update,NULL);
        }
      }
      break;
    case XIT_PLUS:
      if(a->mode==PRESSED) {
        if(can_transmit) {
          value=(double)transmitter->xit;
          value+=(double)rit_increment;
          if(value<-10000.0) {
            value=-10000.0;
          } else if(value>10000.0) {
            value=10000.0;
          }
          transmitter->xit=(int)value;
	  transmitter->xit_enabled=(transmitter->xit!=0);
          if(protocol==NEW_PROTOCOL) {
            schedule_high_priority();
          }
          g_idle_add(ext_vfo_update,NULL);
        }
      }
      break;
    case ZOOM:
      update_zoom((double)a->val);
      break;
    case ZOOM_MINUS:
      if(a->mode==PRESSED) {
        update_zoom(-1);
      }
      break;
    case ZOOM_PLUS:
      if(a->mode==PRESSED) {
        update_zoom(+1);
      }
      break;

    case CW_KEYER_SPEED:
      if (a->mode==ABSOLUTE) {
        //
        // The MIDI keyer reports the speed as a value between 1 and 127,
        // however the range 0-127 is internally converted to 0-100 upstream
        //
        cw_keyer_speed=(127*a->val + 50)/100;
        if (cw_keyer_speed <  1) cw_keyer_speed=1;
        if (cw_keyer_speed > 99) cw_keyer_speed=99;
        g_idle_add(ext_vfo_update,NULL);
      }
      break;

    case CW_KEYER_SIDETONE:
      if (a->mode==ABSOLUTE) {
        //
        // The MIDI keyer encodes the frequency as a value between 0 and 127,
        // freq = 250 + 8*val
        // however the range 0-127 is internally converted to 0-100 upstream
        //
        cw_keyer_sidetone_frequency=250 + (254*a->val + 12)/25;
        g_idle_add(ext_vfo_update,NULL);
      }
      break;

    default:
      if(a->action>=0 && a->action<ACTIONS) {
        g_print("%s: UNKNOWN PRESSED SWITCH ACTION %d (%s)\n",__FUNCTION__,a->action,ActionTable[a->action].str);
      } else {
        g_print("%s: INVALID PRESSED SWITCH ACTION %d\n",__FUNCTION__,a->action);
      }
      break;
  }
  if(free_action) {
    g_free(data);
  }
  return 0;
}

