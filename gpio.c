/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <wiringPi.h>
#include <semaphore.h>

#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "toolbar.h"
#include "gpio.h"
#include "radio.h"
#include "toolbar.h"
#include "main.h"
#include "property.h"
#include "vfo.h"
#include "wdsp.h"
#ifdef PSK
#include "psk.h"
#endif
#include "new_menu.h"
#include "encoder_menu.h"
#include "diversity_menu.h"
#include "gpio.h"
#if defined (CONTROLLER2_V2) || defined(CONTROLLER2_V1)
#include "i2c.h"
#endif
#include "ext.h"
#include "sliders.h"
#include "new_protocol.h"
#ifdef LOCALCW
#include "iambic.h"
#endif

// debounce settle time in ms
#define DEFAULT_SETTLE_TIME 50

int settle_time=DEFAULT_SETTLE_TIME;
static gint release_timer=-1;

#if defined (CONTROLLER2_V1)
// uses wiringpi pin numbers
int ENABLE_VFO_ENCODER=1;
int ENABLE_VFO_PULLUP=1;
int VFO_ENCODER_A=1;
int VFO_ENCODER_B=0;
int ENABLE_E2_ENCODER=1;
int ENABLE_E2_PULLUP=1;
int E2_ENCODER_A=28;
int E2_ENCODER_B=25;
int E2_FUNCTION=3;
int ENABLE_E3_ENCODER=1;
int ENABLE_E3_PULLUP=1;
int E3_ENCODER_A=7;
int E3_ENCODER_B=29;
int E3_FUNCTION=2;
int ENABLE_E4_ENCODER=1;
int ENABLE_E4_PULLUP=1;
int E4_ENCODER_A=27;
int E4_ENCODER_B=24;
int E4_FUNCTION=4;
int ENABLE_E5_ENCODER=1;
int ENABLE_E5_PULLUP=1;
int E5_ENCODER_A=6;
int E5_ENCODER_B=10;
int E5_FUNCTION=5;

int ENABLE_E2_BUTTON=1;
int ENABLE_E3_BUTTON=1;
int ENABLE_E4_BUTTON=1;
int ENABLE_E5_BUTTON=1;

int I2C_INTERRUPT=16;
#endif
#if defined (CONTROLLER2_V2)
// uses wiringpi pin numbers
int ENABLE_VFO_ENCODER=1;
int ENABLE_VFO_PULLUP=0;
int VFO_ENCODER_A=1;
int VFO_ENCODER_B=0;
int ENABLE_E2_ENCODER=1;
int ENABLE_E2_PULLUP=1;
int E2_ENCODER_A=21;
int E2_ENCODER_B=22;
int E2_TOP_ENCODER_A=25;
int E2_TOP_ENCODER_B=28;
int E2_FUNCTION=3;
int ENABLE_E3_ENCODER=1;
int ENABLE_E3_PULLUP=1;
int E3_ENCODER_A=13;
int E3_ENCODER_B=11;
int E3_TOP_ENCODER_A=29;
int E3_TOP_ENCODER_B=7;
int E3_FUNCTION=2;
int ENABLE_E4_ENCODER=1;
int ENABLE_E4_PULLUP=1;
int E4_ENCODER_A=14;
int E4_ENCODER_B=12;
int E4_TOP_ENCODER_A=24;
int E4_TOP_ENCODER_B=27;
int E4_FUNCTION=4;
int ENABLE_E5_ENCODER=1;
int ENABLE_E5_PULLUP=1;
int E5_ENCODER_A=23;
int E5_ENCODER_B=26;
int E5_TOP_ENCODER_A=10;
int E5_TOP_ENCODER_B=6;
int E5_FUNCTION=5;

int ENABLE_E2_BUTTON=1;
int ENABLE_E3_BUTTON=1;
int ENABLE_E4_BUTTON=1;
int ENABLE_E5_BUTTON=1;

int I2C_INTERRUPT=16;
#endif

#if !defined (CONTROLLER2_V2) && !defined (CONTROLLER2_V1)
// uses wiringpi pin numbers
int ENABLE_VFO_ENCODER=1;
int ENABLE_VFO_PULLUP=1;
int VFO_ENCODER_A=1;
int VFO_ENCODER_B=0;
int ENABLE_E2_ENCODER=1;
int ENABLE_E2_PULLUP=0;
int E2_ENCODER_A=28;
int E2_ENCODER_B=25;
int E2_FUNCTION=6;
int ENABLE_E3_ENCODER=1;
int ENABLE_E3_PULLUP=0;
int E3_ENCODER_A=27;
int E3_ENCODER_B=24;
int E3_FUNCTION=10;
int ENABLE_E4_ENCODER=1;
int ENABLE_E4_PULLUP=0;
int E4_ENCODER_A=7;
int E4_ENCODER_B=29;
int E4_FUNCTION=11;
int ENABLE_S1_BUTTON=1;
int S1_BUTTON=23;
int ENABLE_S2_BUTTON=1;
int S2_BUTTON=26;
int ENABLE_S3_BUTTON=1;
int S3_BUTTON=22;
int ENABLE_S4_BUTTON=1;
int S4_BUTTON=21;
int ENABLE_S5_BUTTON=1;
int S5_BUTTON=5;
int ENABLE_S6_BUTTON=1;
int S6_BUTTON=4;
int ENABLE_MOX_BUTTON=1;
int MOX_BUTTON=2;
int ENABLE_FUNCTION_BUTTON=1;
int FUNCTION_BUTTON=3;
int ENABLE_E2_BUTTON=1;
int ENABLE_E3_BUTTON=1;
int ENABLE_E4_BUTTON=1;
#endif

#ifdef LOCALCW
int CWL_BUTTON=14;
int CWR_BUTTON=15;
int SIDETONE_GPIO=8;
int ENABLE_GPIO_SIDETONE=0;
int ENABLE_CW_BUTTONS=1;
int CW_ACTIVE_LOW=1;
#endif

int vfoEncoderPos;
int vfoFunction;

#if defined (CONTROLLER2_V1)
int e2EncoderPos;
int e2_sw_action=MENU_BAND;
int e2_encoder_action=ENCODER_AF_GAIN_RX1;
int e3EncoderPos;
int e3_sw_action=MENU_MODE;
int e3_encoder_action=ENCODER_AGC_GAIN_RX1;
int e4EncoderPos;
int e4_sw_action=MENU_FILTER;
int e4_encoder_action=ENCODER_IF_WIDTH_RX1;
int e5EncoderPos;
int e5_sw_action=MENU_FREQUENCY;
int e5_encoder_action=ENCODER_DRIVE;
#endif

#if defined (CONTROLLER2_V2)
int e2EncoderPos;
int e2_sw_action=MENU_BAND;
int e2_encoder_action=ENCODER_AF_GAIN_RX2;
int e3EncoderPos;
int e3_sw_action=MENU_MODE;
int e3_encoder_action=ENCODER_AGC_GAIN_RX2;
int e4EncoderPos;
int e4_sw_action=MENU_FILTER;
int e4_encoder_action=ENCODER_IF_WIDTH_RX2;
int e5EncoderPos;
int e5_sw_action=MENU_FREQUENCY;
int e5_encoder_action=ENCODER_DRIVE;
#endif

#if defined (CONTROLLER2_V2)
int e2_top_encoder_action=ENCODER_AF_GAIN_RX1;
int e3_top_encoder_action=ENCODER_AGC_GAIN_RX1;
int e4_top_encoder_action=ENCODER_IF_WIDTH_RX1;
int e5_top_encoder_action=ENCODER_TUNE_DRIVE;
int e2TopEncoderPos;
int e3TopEncoderPos;
int e4TopEncoderPos;
int e5TopEncoderPos;
#endif

#if !defined (CONTROLLER2_V2) && !defined (CONTROLLER2_V1)
int e2EncoderPos;
int e2_sw_action=RIT;
int e2_encoder_action=ENCODER_AF_GAIN_RX1;
int e3EncoderPos;
int e3_sw_action=AGC;
int e3_encoder_action=ENCODER_AGC_GAIN_RX1;
int e4EncoderPos;
int e4_sw_action=BAND_PLUS;
int e4_encoder_action=ENCODER_DRIVE;
#endif

static volatile int function_state;
static volatile int band_state;
static volatile int bandstack_state;
static volatile int mode_state;
static volatile int filter_state;
static volatile int noise_state;
static volatile int agc_state;
static volatile int mox_state;
static volatile int lock_state;

static gpointer rotary_encoder_thread(gpointer data);
static GThread *rotary_encoder_thread_id;

static int previous_function_button=0;
static int band_button=0;
static int previous_band_button=0;
static int bandstack_button=0;
static int previous_bandstack_button=0;
static int mode_button=0;
static int previous_mode_button=0;
static int filter_button=0;
static int previous_filter_button=0;
static int noise_button=0;
static int previous_noise_button=0;
static int agc_button=0;
static int previous_agc_button=0;
static int mox_button=0;
static int previous_mox_button=0;

static int running=0;

char *encoder_string[ENCODER_ACTIONS] = {
  "AF GAIN RX1",
  "RF GAIN RX1",
  "AF GAIN RX2",
  "RF GAIN RX2",
  "AGC GAIN RX1",
  "AGC GAIN RX2",
  "IF WIDTH RX1",
  "IF WIDTH RX2",
  "ATTENUATION",
  "MIC GAIN",
  "DRIVE",
  "TUNE DRIVE",
  "RIT RX1",
  "RIT RX2",
  "XIT",
  "CW SPEED",
  "CW FREQUENCY",
  "PANADAPTER HIGH",
  "PANADAPTER LOW",
  "SQUELCH",
  "COMP",
  "DIVERSITY GAIN",
  "DIVERSITY PHASE"};

char *sw_string[SWITCH_ACTIONS] = {
  "NO ACTION",
  "TUNE",
  "MOX",
  "PS",
  "TWO TONE",
  "NR",
  "NB",
  "SNB",
  "RIT",
  "RIT Clear",
  "XIT",
  "XIT Clear",
  "BAND PLUS",
  "BAND MINUS",
  "BANDSTACK PLUS",
  "BANDSTACK MINUS",
  "MODE PLUS",
  "MODE MINUS",
  "FILTER PLUS",
  "FILTER MINUS",
  "A TO B",
  "B TO A",
  "A SWAP B",
  "LOCK",
  "CTUN",
  "AGC",
  "SPLIT",
  "DIVERSITY",
  "SAT",
  "BAND MENU",
  "BANDSTACK MENU",
  "MODE MENU",
  "FILTER MENU",
  "FREQUENCY MENU",
  "MEMORY MENU",
  "DIVERSITY MENU",
#if !defined (CONTROLLER2_V2) && !defined (CONTROLLER2_V1)
  "FUNCTION",
#endif
};

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
int sw_action[SWITCHES] = {TUNE,MOX,PS,TWO_TONE,NR,A_TO_B,B_TO_A,MODE_MINUS,BAND_MINUS,MODE_PLUS,BAND_PLUS,XIT,NB,SNB,LOCK,CTUN};
#endif

static int mox_pressed(void *data) {
  if(running) sim_mox_cb(NULL,NULL);
  return 0;
}

static int s1_pressed(void *data) {
  if(running) sim_s1_pressed_cb(NULL,NULL);
  return 0;
}

static int s1_released(void *data) {
  if(running) sim_s1_released_cb(NULL,NULL);
  return 0;
}

static int s2_pressed(void *data) {
  if(running) sim_s2_pressed_cb(NULL,NULL);
  return 0;
}

static int s2_released(void *data) {
  if(running) sim_s2_released_cb(NULL,NULL);
  return 0;
}

static int s3_pressed(void *data) {
  if(running) sim_s3_pressed_cb(NULL,NULL);
  return 0;
}

static int s3_released(void *data) {
  if(running) sim_s3_released_cb(NULL,NULL);
  return 0;
}

static int s4_pressed(void *data) {
  if(running) sim_s4_pressed_cb(NULL,NULL);
  return 0;
}

static int s4_released(void *data) {
  if(running) sim_s4_released_cb(NULL,NULL);
  return 0;
}

static int s5_pressed(void *data) {
  if(running) sim_s5_pressed_cb(NULL,NULL);
  return 0;
}

static int s5_released(void *data) {
  if(running) sim_s5_released_cb(NULL,NULL);
  return 0;
}

static int s6_pressed(void *data) {
  if(running) sim_s6_pressed_cb(NULL,NULL);
  return 0;
}

static int s6_released(void *data) {
  if(running) sim_s6_released_cb(NULL,NULL);
  return 0;
}

static int function_pressed(void *data) {
  if(running) sim_function_cb(NULL,NULL);
  return 0;
}

static int vfo_function_pressed(void *data) {
  RECEIVER *rx;
  if(receivers==2) {
    if(active_receiver==receiver[0]) {
      rx=receiver[1];
    } else {
      rx=receiver[0];
    }
    active_receiver=rx;
    g_idle_add(menu_active_receiver_changed,NULL);
    g_idle_add(ext_vfo_update,NULL);
    g_idle_add(sliders_active_receiver_changed,NULL);
  }
  return 0;
}

static int vfo_function_released(void *data) {
  return 0;
}

static int e_function_pressed(void *data) {
  int action=(int)data;
fprintf(stderr,"e_function_pressed: %d\n",action);
  switch(action) {
    case TUNE:
      if(can_transmit) g_idle_add(ext_tune_update,NULL);
      break;
    case MOX:
      if(can_transmit) g_idle_add(ext_mox_update,NULL);
      break;
    case PS:
#ifdef PURESIGNAL
      if(can_transmit) g_idle_add(ext_ps_update,NULL);
#endif
      break;
    case TWO_TONE:
      if(can_transmit) g_idle_add(ext_two_tone,NULL);
      break;
    case NR:
      g_idle_add(ext_nr_update,NULL);
      break;
    case NB:
      g_idle_add(ext_nb_update,NULL);
      break;
    case SNB:
      g_idle_add(ext_snb_update,NULL);
      break;
    case RIT:
      g_idle_add(ext_rit_update,NULL);
      break;
    case RIT_CLEAR:
      g_idle_add(ext_rit_clear,NULL);
      break;
    case XIT:
      if(can_transmit) g_idle_add(ext_xit_update,NULL);
      break;
    case XIT_CLEAR:
      if(can_transmit) g_idle_add(ext_xit_clear,NULL);
      break;
    case BAND_PLUS:
      g_idle_add(ext_band_plus,NULL);
      break;
    case BAND_MINUS:
      g_idle_add(ext_band_minus,NULL);
      break;
    case BANDSTACK_PLUS:
      g_idle_add(ext_bandstack_plus,NULL);
      break;
    case BANDSTACK_MINUS:
      g_idle_add(ext_bandstack_minus,NULL);
      break;
    case MODE_PLUS:
      g_idle_add(ext_mode_plus,NULL);
      break;
    case MODE_MINUS:
      g_idle_add(ext_mode_minus,NULL);
      break;
    case FILTER_PLUS:
      g_idle_add(ext_filter_plus,NULL);
      break;
    case FILTER_MINUS:
      g_idle_add(ext_filter_minus,NULL);
      break;
    case A_TO_B:
      g_idle_add(ext_vfo_a_to_b,NULL);
      break;
    case B_TO_A:
      g_idle_add(ext_vfo_b_to_a,NULL);
      break;
    case A_SWAP_B:
      g_idle_add(ext_vfo_a_swap_b,NULL);
      break;
    case LOCK:
      g_idle_add(ext_lock_update,NULL);
      break;
    case CTUN:
      g_idle_add(ext_ctun_update,NULL);
      break;
    case AGC:
      g_idle_add(ext_agc_update,NULL);
      break;
    case SPLIT:
      if(can_transmit) g_idle_add(ext_split_update,NULL);
      break;
    case DIVERSITY:
      g_idle_add(ext_diversity_update,GINT_TO_POINTER(0));
      break;
    case SAT:
      if(can_transmit) g_idle_add(ext_sat_update,NULL);
      break;
    case MENU_BAND:
      g_idle_add(ext_band_update,NULL);
      break;
    case MENU_BANDSTACK:
      g_idle_add(ext_bandstack_update,NULL);
      break;
    case MENU_MODE:
      g_idle_add(ext_mode_update,NULL);
      break;
    case MENU_FILTER:
      g_idle_add(ext_filter_update,NULL);
      break;
    case MENU_FREQUENCY:
      g_idle_add(ext_frequency_update,NULL);
      break;
    case MENU_MEMORY:
      g_idle_add(ext_memory_update,NULL);
      break;
    case MENU_DIVERSITY:
      g_idle_add(ext_diversity_update,GINT_TO_POINTER(1));
      break;
#if !defined (CONTROLLER2_V1) && !defined (CONTROLLER2_V2)
    case FUNCTION:
      g_idle_add(ext_function_update,NULL);
      break;
#endif
  }
  return 0;
}

static unsigned long e2debounce=0;

static void e2FunctionAlert() {
    int level=digitalRead(E2_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)e2_sw_action);
    }
}

static unsigned long e3debounce=0;

static void e3FunctionAlert() {
    int level=digitalRead(E3_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)e3_sw_action);
    }
}

static unsigned long e4debounce=0;

static void e4FunctionAlert() {
    int level=digitalRead(E4_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)e4_sw_action);
    }
}

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
static unsigned long e5debounce=0;

static void e5FunctionAlert() {
    int level=digitalRead(E5_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)e5_sw_action);
    }
}
#endif

#if !defined (CONTROLLER2_V2) && !defined (CONTROLLER2_V1)
static int function_level=1;
static unsigned long function_debounce=0;

static void functionAlert() {
    int t=millis();
    if(millis()<function_debounce) {
      return;
    }
    int level=digitalRead(FUNCTION_BUTTON);
    if(level!=function_level) {
      if(level==0) {
        if(running) g_idle_add(function_pressed,NULL);
      }
      function_level=level;
      function_debounce=t+settle_time;
    }
}

static int s1_level=1;
static unsigned long s1_debounce=0;

static void s1Alert() {
    int t=millis();
    if(millis()<s1_debounce) {
      return;
    }
    int level=digitalRead(S1_BUTTON);
    if(level!=s1_level) {
      if(level==0) {
        if(running) g_idle_add(s1_pressed,NULL);
      } else {
        if(running) g_idle_add(s1_released,NULL);
      }
      s1_level=level;
      s1_debounce=t+settle_time;
    }
}

static int s2_level=1;
static unsigned long s2_debounce=0;

static void s2Alert() {
    int t=millis();
    if(millis()<s2_debounce) {
      return;
    }
    int level=digitalRead(S2_BUTTON);
    if(level!=s2_level) {
      if(level==0) {
        if(running) g_idle_add(s2_pressed,NULL);
      } else {
        if(running) g_idle_add(s2_released,NULL);
      }
      s2_level=level;
      s2_debounce=t+settle_time;
    }
}

static int s3_level=1;
static unsigned long s3_debounce=0;

static void s3Alert() {
    int t=millis();
    if(millis()<s3_debounce) {
      return;
    }
    int level=digitalRead(S3_BUTTON);
    if(level!=s3_level) {
      if(level==0) {
        if(running) g_idle_add(s3_pressed,NULL);
      } else {
        if(running) g_idle_add(s3_released,NULL);
      }
      s3_level=level;
      s3_debounce=t+settle_time;
    }
}

static int s4_level=1;
static unsigned long s4_debounce=0;

static void s4Alert() {
    int t=millis();
    if(millis()<s4_debounce) {
      return;
    }
    int level=digitalRead(S4_BUTTON);
    if(level!=s4_level) {
      if(level==0) {
        if(running) g_idle_add(s4_pressed,NULL);
      } else {
        if(running) g_idle_add(s4_released,NULL);
      }
      s4_level=level;
      s4_debounce=t+settle_time;
    }
}

static int s5_level=1;
static unsigned long s5_debounce=0;

static void s5Alert() {
    int t=millis();
    if(millis()<s5_debounce) {
      return;
    }
    int level=digitalRead(S5_BUTTON);
    if(level!=s5_level) {
      if(level==0) {
        if(running) g_idle_add(s5_pressed,NULL);
      } else {
        if(running) g_idle_add(s5_released,NULL);
      }
      s5_level=level;
      s5_debounce=t+settle_time;
    }
}

static int s6_level=1;
static unsigned long s6_debounce=0;

static void s6Alert() {
    int t=millis();
    if(millis()<s6_debounce) {
      return;
    }
    int level=digitalRead(S6_BUTTON);
    if(level!=s6_level) {
      if(level==0) {
        if(running) g_idle_add(s6_pressed,NULL);
      } else {
        if(running) g_idle_add(s6_released,NULL);
      }
      s6_level=level;
      s6_debounce=t+settle_time;
    }
}

static int mox_level=1;
static unsigned long mox_debounce=0;

static void moxAlert() {
    int t=millis();
    if(millis()<mox_debounce) {
      return;
    }
    int level=digitalRead(MOX_BUTTON);
    if(level!=mox_level) {
      if(level==0) {
        if(running) g_idle_add(mox_pressed,NULL);
      }
      mox_level=level;
      mox_debounce=t+settle_time;
    }
}
#endif


#ifdef VFO_HAS_FUNCTION
static unsigned long vfo_debounce=0;

static void vfoFunctionAlert() {
    int t=millis();
    if(t-vfo_debounce > settle_time) {
      int level=digitalRead(VFO_FUNCTION);
      if(level==0) {
        if(running) g_idle_add(vfo_function_pressed,NULL);
      } else {
        if(running) g_idle_add(vfo_function_released,NULL);
      }
      vfo_debounce=t;
    }
}
#endif

static void vfoEncoderInt(int A_or_B) {
    static int vfoA=1, vfoB=1;
    int levelA=digitalRead(VFO_ENCODER_A);
    int levelB=digitalRead(VFO_ENCODER_B);

  if(levelA!=vfoA) {
    if(levelA==levelB) ++vfoEncoderPos;
    if(levelA!=levelB) --vfoEncoderPos;
    vfoA=levelA;
  }
/*
  if(levelA!=e2CurrentA) {
    if(levelA==levelB) ++e2EncoderPos;
    if(levelA!=levelB) --e2EncoderPos;
    e2CurrentA=levelA;
  }

    if(vfo_A==switch_A && vfo_B==switch_B) {
      return; // same as last
    }
    vfo_A=switch_A;
    vfo_B=switch_B;
    if(switch_A && switch_B) {
      if(A_or_B==VFO_ENCODER_B) {
        vfoEncoderPos--;
      } else {
        vfoEncoderPos++;
      }
    }
*/
}

static void vfoEncoderA() {
    vfoEncoderInt(VFO_ENCODER_A);
}

static void vfoEncoderB() {
    vfoEncoderInt(VFO_ENCODER_B);
}

static void e2EncoderInterrupt(int gpio) {
  static int e2CurrentA=1, e2CurrentB=1;

  int levelA=digitalRead(E2_ENCODER_A);
  int levelB=digitalRead(E2_ENCODER_B);

  if(levelA!=e2CurrentA) {
    if(levelA==levelB) ++e2EncoderPos;
    if(levelA!=levelB) --e2EncoderPos;
    e2CurrentA=levelA;
  }
}

static void e2EncoderA() {
  e2EncoderInterrupt(E2_ENCODER_A);
}

static void e2EncoderB() {
  e2EncoderInterrupt(E2_ENCODER_B);
}

#if defined (CONTROLLER2_V2)
static void e2TopEncoderInterrupt(int gpio) {
  static int e2TopCurrentA=1, e2TopCurrentB=1;

  int levelA=digitalRead(E2_TOP_ENCODER_A);
  int levelB=digitalRead(E2_TOP_ENCODER_B);

  if(levelA!=e2TopCurrentA) {
    if(levelA==levelB) ++e2TopEncoderPos;
    if(levelA!=levelB) --e2TopEncoderPos;
    e2TopCurrentA=levelA;
  }
}

static void e2TopEncoderA() {
  e2TopEncoderInterrupt(E2_TOP_ENCODER_A);
}

static void e2TopEncoderB() {
  e2TopEncoderInterrupt(E2_TOP_ENCODER_B);
}
#endif

static void e3EncoderInterrupt(int gpio) {
  static int e3CurrentA=1, e3CurrentB=1;

  int levelA=digitalRead(E3_ENCODER_A);
  int levelB=digitalRead(E3_ENCODER_B);

  if(levelA!=e3CurrentA) {
    if(levelA==levelB) ++e3EncoderPos;
    if(levelA!=levelB) --e3EncoderPos;
    e3CurrentA=levelA;
  }
}

static void e3EncoderA() {
  e3EncoderInterrupt(E3_ENCODER_A);
}

static void e3EncoderB() {
  e3EncoderInterrupt(E3_ENCODER_B);
}

#if defined (CONTROLLER2_V2)
static void e3TopEncoderInterrupt(int gpio) {
  static int e3TopCurrentA=1, e3TopCurrentB=1;

  int levelA=digitalRead(E3_TOP_ENCODER_A);
  int levelB=digitalRead(E3_TOP_ENCODER_B);


  if(levelA!=e3TopCurrentA) {
    if(levelA==levelB) ++e3TopEncoderPos;
    if(levelA!=levelB) --e3TopEncoderPos;
    e3TopCurrentA=levelA;
  }
}

static void e3TopEncoderA() {
  e3TopEncoderInterrupt(E3_TOP_ENCODER_A);
}

static void e3TopEncoderB() {
  e3TopEncoderInterrupt(E3_TOP_ENCODER_B);
}
#endif

static void e4EncoderInterrupt(int gpio) {
  static int e4CurrentA=1, e4CurrentB=1;

  int levelA=digitalRead(E4_ENCODER_A);
  int levelB=digitalRead(E4_ENCODER_B);

  if(levelA!=e4CurrentA) {
    if(levelA==levelB) ++e4EncoderPos;
    if(levelA!=levelB) --e4EncoderPos;
    e4CurrentA=levelA;
  }
}

static void e4EncoderA() {
  e4EncoderInterrupt(E4_ENCODER_A);
}

static void e4EncoderB() {
  e4EncoderInterrupt(E4_ENCODER_B);
}

#if defined (CONTROLLER2_V2)
static void e4TopEncoderInterrupt(int gpio) {
  static int e4TopCurrentA=1, e4TopCurrentB=1;

  int levelA=digitalRead(E4_TOP_ENCODER_A);
  int levelB=digitalRead(E4_TOP_ENCODER_B);

  if(levelA!=e4TopCurrentA) {
    if(levelA==levelB) ++e4TopEncoderPos;
    if(levelA!=levelB) --e4TopEncoderPos;
    e4TopCurrentA=levelA;
  }
}

static void e4TopEncoderA() {
  e4TopEncoderInterrupt(E4_TOP_ENCODER_A);
}

static void e4TopEncoderB() {
  e4TopEncoderInterrupt(E4_TOP_ENCODER_B);
}
#endif


#if defined (CONTROLLER2_V2)  || defined (CONTROLLER2_V1)
static void e5EncoderInterrupt(int gpio) {
  static int e5CurrentA=1, e5CurrentB=1;

  int levelA=digitalRead(E5_ENCODER_A);
  int levelB=digitalRead(E5_ENCODER_B);


  if(levelA!=e5CurrentA) {
    if(levelA==levelB) ++e5EncoderPos;
    if(levelA!=levelB) --e5EncoderPos;
    e5CurrentA=levelA;
  }
}

static void e5EncoderA() {
  e5EncoderInterrupt(E5_ENCODER_A);
}

static void e5EncoderB() {
  e5EncoderInterrupt(E5_ENCODER_B);
}
#endif

#if defined (CONTROLLER2_V2)
static void e5TopEncoderInterrupt(int gpio) {
  static int e5TopCurrentA=1, e5TopCurrentB=1;

  int levelA=digitalRead(E5_TOP_ENCODER_A);
  int levelB=digitalRead(E5_TOP_ENCODER_B);

  if(levelA!=e5TopCurrentA) {
    if(levelA==levelB) ++e5TopEncoderPos;
    if(levelA!=levelB) --e5TopEncoderPos;
    e5TopCurrentA=levelA;
  }
}

static void e5TopEncoderA() {
  e5TopEncoderInterrupt(E5_TOP_ENCODER_A);
}

static void e5TopEncoderB() {
  e5TopEncoderInterrupt(E5_TOP_ENCODER_B);
}
#endif


#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
static void pI2CInterrupt() {
    int level=digitalRead(I2C_INTERRUPT);
    if(level==0) {
      i2c_interrupt();
    }
}
#endif

void gpio_restore_actions() {
  char* value;
  char name[80];
  int i;

  value=getProperty("settle_time");
  if(value) settle_time=atoi(value);
  value=getProperty("e2_encoder_action");
  if(value) e2_encoder_action=atoi(value);
  value=getProperty("e2_sw_action");
  if(value) e2_sw_action=atoi(value);
  value=getProperty("e3_encoder_action");
  if(value) e3_encoder_action=atoi(value);
  value=getProperty("e3_sw_action");
  if(value) e3_sw_action=atoi(value);
  value=getProperty("e4_encoder_action");
  if(value) e4_encoder_action=atoi(value);
  value=getProperty("e4_sw_action");
  if(value) e4_sw_action=atoi(value);

#if defined (CONTROLLER2_V2)
  value=getProperty("e2_top_encoder_action");
  if(value) e2_top_encoder_action=atoi(value);
  value=getProperty("e3_top_encoder_action");
  if(value) e3_top_encoder_action=atoi(value);
  value=getProperty("e4_top_encoder_action");
  if(value) e4_top_encoder_action=atoi(value);
#endif

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  value=getProperty("e5_encoder_action");
  if(value) e5_encoder_action=atoi(value);
#if defined (CONTROLLER2_V2)
  value=getProperty("e5_top_encoder_action");
  if(value) e5_top_encoder_action=atoi(value);
#endif
  value=getProperty("e5_sw_action");
  if(value) e5_sw_action=atoi(value);
  for(i=0;i<SWITCHES;i++) {
    sprintf(name,"sw_action[%d]",i);
    value=getProperty(name);		
    if(value) sw_action[i]=atoi(value);		
  }
#endif

}

void gpio_restore_state() {
  char* value;
  char name[80];
  int i;

  loadProperties("gpio.props");
  value=getProperty("ENABLE_VFO_ENCODER");
  if(value) ENABLE_VFO_ENCODER=atoi(value);
  value=getProperty("ENABLE_VFO_PULLUP");
  if(value) ENABLE_VFO_PULLUP=atoi(value);
  value=getProperty("VFO_ENCODER_A");
  if(value) VFO_ENCODER_A=atoi(value);
  value=getProperty("VFO_ENCODER_B");
  if(value) VFO_ENCODER_B=atoi(value);
  value=getProperty("ENABLE_E2_ENCODER");
  if(value) ENABLE_E2_ENCODER=atoi(value);
  value=getProperty("ENABLE_E2_PULLUP");
  if(value) ENABLE_E2_PULLUP=atoi(value);
  value=getProperty("E2_ENCODER_A");
  if(value) E2_ENCODER_A=atoi(value);
  value=getProperty("E2_ENCODER_B");
  if(value) E2_ENCODER_B=atoi(value);
  value=getProperty("ENABLE_E3_ENCODER");
  if(value) ENABLE_E3_ENCODER=atoi(value);
  value=getProperty("ENABLE_E3_PULLUP");
  if(value) ENABLE_E3_PULLUP=atoi(value);
  value=getProperty("E3_ENCODER_A");
  if(value) E3_ENCODER_A=atoi(value);
  value=getProperty("E3_ENCODER_B");
  if(value) E3_ENCODER_B=atoi(value);
  value=getProperty("ENABLE_E4_ENCODER");
  if(value) ENABLE_E4_ENCODER=atoi(value);
  value=getProperty("ENABLE_E4_PULLUP");
  if(value) ENABLE_E4_PULLUP=atoi(value);
  value=getProperty("E4_ENCODER_A");
  if(value) E4_ENCODER_A=atoi(value);
  value=getProperty("E4_ENCODER_B");
  if(value) E4_ENCODER_B=atoi(value);
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  value=getProperty("ENABLE_E5_ENCODER");
  if(value) ENABLE_E5_ENCODER=atoi(value);
  value=getProperty("ENABLE_E5_PULLUP");
  if(value) ENABLE_E5_PULLUP=atoi(value);
  value=getProperty("E5_ENCODER_A");
  if(value) E5_ENCODER_A=atoi(value);
  value=getProperty("E5_ENCODER_B");
  if(value) E5_ENCODER_B=atoi(value);
#endif
#if !defined (CONTROLLER2_V2) && !defined(CONTROLLER2_V1)
  value=getProperty("ENABLE_S1_BUTTON");
  if(value) ENABLE_S1_BUTTON=atoi(value);
  value=getProperty("S1_BUTTON");
  if(value) S1_BUTTON=atoi(value);
  value=getProperty("ENABLE_S2_BUTTON");
  if(value) ENABLE_S2_BUTTON=atoi(value);
  value=getProperty("S2_BUTTON");
  if(value) S2_BUTTON=atoi(value);
  value=getProperty("ENABLE_S3_BUTTON");
  if(value) ENABLE_S3_BUTTON=atoi(value);
  value=getProperty("S3_BUTTON");
  if(value) S3_BUTTON=atoi(value);
  value=getProperty("ENABLE_S4_BUTTON");
  if(value) ENABLE_S4_BUTTON=atoi(value);
  value=getProperty("S4_BUTTON");
  if(value) S4_BUTTON=atoi(value);
  value=getProperty("ENABLE_S5_BUTTON");
  if(value) ENABLE_S5_BUTTON=atoi(value);
  value=getProperty("S5_BUTTON");
  if(value) S5_BUTTON=atoi(value);
  value=getProperty("ENABLE_S6_BUTTON");
  if(value) ENABLE_S6_BUTTON=atoi(value);
  value=getProperty("S6_BUTTON");
  if(value) S6_BUTTON=atoi(value);
  value=getProperty("ENABLE_FUNCTION_BUTTON");
  if(value) ENABLE_FUNCTION_BUTTON=atoi(value);
  value=getProperty("FUNCTION_BUTTON");
  if(value) FUNCTION_BUTTON=atoi(value);
  value=getProperty("ENABLE_MOX_BUTTON");
  if(value) ENABLE_MOX_BUTTON=atoi(value);
  value=getProperty("MOX_BUTTON");
  if(value) MOX_BUTTON=atoi(value);
#endif

  value=getProperty("ENABLE_E2_BUTTON");
  if(value) ENABLE_E2_BUTTON=atoi(value);
  value=getProperty("ENABLE_E3_BUTTON");
  if(value) ENABLE_E3_BUTTON=atoi(value);
  value=getProperty("ENABLE_E4_BUTTON");
  if(value) ENABLE_E4_BUTTON=atoi(value);
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  value=getProperty("ENABLE_E5_BUTTON");
  if(value) ENABLE_E5_BUTTON=atoi(value);
#endif
#ifdef LOCALCW		
 value=getProperty("ENABLE_CW_BUTTONS");		
 if(value) ENABLE_CW_BUTTONS=atoi(value);		
 value=getProperty("CW_ACTIVE_LOW");		
 if(value) CW_ACTIVE_LOW=atoi(value);		
 value=getProperty("CWL_BUTTON");		
 if(value) CWL_BUTTON=atoi(value);		
 value=getProperty("CWR_BUTTON");		
 if(value) CWR_BUTTON=atoi(value);		
 value=getProperty("SIDETONE_GPIO");		
 if(value) SIDETONE_GPIO=atoi(value);		
 value=getProperty("ENABLE_GPIO_SIDETONE");		
 if(value) ENABLE_GPIO_SIDETONE=atoi(value);		
#endif

}

void gpio_save_actions() {
  int i;
  char name[80];
  char value[80];

  sprintf(value,"%d",settle_time);
  setProperty("settle_time",value);
  sprintf(value,"%d",e2_sw_action);
  setProperty("e2_sw_action",value);
  sprintf(value,"%d",e2_encoder_action);
  setProperty("e2_encoder_action",value);
  sprintf(value,"%d",e3_sw_action);
  setProperty("e3_sw_action",value);
  sprintf(value,"%d",e3_encoder_action);
  setProperty("e3_encoder_action",value);
  sprintf(value,"%d",e4_sw_action);
  setProperty("e4_sw_action",value);
  sprintf(value,"%d",e4_encoder_action);
  setProperty("e4_encoder_action",value);
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  sprintf(value,"%d",e5_sw_action);
  setProperty("e5_sw_action",value);
  sprintf(value,"%d",e5_encoder_action);
  setProperty("e5_encoder_action",value);
#endif
#if defined (CONTROLLER2_V2)
  sprintf(value,"%d",e2_top_encoder_action);
  setProperty("e2_top_encoder_action",value);
  sprintf(value,"%d",e3_top_encoder_action);
  setProperty("e3_top_encoder_action",value);
  sprintf(value,"%d",e4_top_encoder_action);
  setProperty("e4_top_encoder_action",value);
  sprintf(value,"%d",e5_top_encoder_action);
  setProperty("e5_top_encoder_action",value);
#endif
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  for(i=0;i<SWITCHES;i++) {
    sprintf(name,"sw_action[%d]",i);
    sprintf(value,"%d",sw_action[i]);
    setProperty(name,value);		
  }
#endif

}

void gpio_save_state() {
  int i;
  char name[80];
  char value[80];

  sprintf(value,"%d",ENABLE_VFO_ENCODER);
  setProperty("ENABLE_VFO_ENCODER",value);
  sprintf(value,"%d",ENABLE_VFO_PULLUP);
  setProperty("ENABLE_VFO_PULLUP",value);
  sprintf(value,"%d",VFO_ENCODER_A);
  setProperty("VFO_ENCODER_A",value);
  sprintf(value,"%d",VFO_ENCODER_B);
  setProperty("VFO_ENCODER_B",value);
  sprintf(value,"%d",ENABLE_E2_ENCODER);
  setProperty("ENABLE_E2_ENCODER",value);
  sprintf(value,"%d",ENABLE_E2_PULLUP);
  setProperty("ENABLE_E2_PULLUP",value);
  sprintf(value,"%d",E2_ENCODER_A);
  setProperty("E2_ENCODER_A",value);
  sprintf(value,"%d",E2_ENCODER_B);
  setProperty("E2_ENCODER_B",value);
  sprintf(value,"%d",ENABLE_E3_ENCODER);
  setProperty("ENABLE_E3_ENCODER",value);
  sprintf(value,"%d",ENABLE_E3_PULLUP);
  setProperty("ENABLE_E3_PULLUP",value);
  sprintf(value,"%d",E3_ENCODER_A);
  setProperty("E3_ENCODER_A",value);
  sprintf(value,"%d",E3_ENCODER_B);
  setProperty("E3_ENCODER_B",value);
  sprintf(value,"%d",ENABLE_E4_ENCODER);
  setProperty("ENABLE_E4_ENCODER",value);
  sprintf(value,"%d",ENABLE_E4_PULLUP);
  setProperty("ENABLE_E4_PULLUP",value);
  sprintf(value,"%d",E4_ENCODER_A);
  setProperty("E4_ENCODER_A",value);
  sprintf(value,"%d",E4_ENCODER_B);
  setProperty("E4_ENCODER_B",value);
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  sprintf(value,"%d",ENABLE_E5_ENCODER);
  setProperty("ENABLE_E5_ENCODER",value);
  sprintf(value,"%d",ENABLE_E5_PULLUP);
  setProperty("ENABLE_E5_PULLUP",value);
  sprintf(value,"%d",E5_ENCODER_A);
  setProperty("E5_ENCODER_A",value);
  sprintf(value,"%d",E5_ENCODER_B);
  setProperty("E5_ENCODER_B",value);
#endif
#if !defined(CONTROLLER2_V2) && !defined(CONTROLLER2_V1)
  sprintf(value,"%d",ENABLE_S1_BUTTON);
  setProperty("ENABLE_S1_BUTTON",value);
  sprintf(value,"%d",S1_BUTTON);
  setProperty("S1_BUTTON",value);
  sprintf(value,"%d",ENABLE_S2_BUTTON);
  setProperty("ENABLE_S2_BUTTON",value);
  sprintf(value,"%d",S2_BUTTON);
  setProperty("S2_BUTTON",value);
  sprintf(value,"%d",ENABLE_S3_BUTTON);
  setProperty("ENABLE_S3_BUTTON",value);
  sprintf(value,"%d",S3_BUTTON);
  setProperty("S3_BUTTON",value);
  sprintf(value,"%d",ENABLE_S4_BUTTON);
  setProperty("ENABLE_S4_BUTTON",value);
  sprintf(value,"%d",S4_BUTTON);
  setProperty("S4_BUTTON",value);
  sprintf(value,"%d",ENABLE_S5_BUTTON);
  setProperty("ENABLE_S5_BUTTON",value);
  sprintf(value,"%d",S5_BUTTON);
  setProperty("S5_BUTTON",value);
  sprintf(value,"%d",ENABLE_S6_BUTTON);
  setProperty("ENABLE_S6_BUTTON",value);
  sprintf(value,"%d",S6_BUTTON);
  setProperty("S6_BUTTON",value);
  sprintf(value,"%d",ENABLE_FUNCTION_BUTTON);
  setProperty("ENABLE_FUNCTION_BUTTON",value);
  sprintf(value,"%d",FUNCTION_BUTTON);
  setProperty("FUNCTION_BUTTON",value);
  sprintf(value,"%d",ENABLE_MOX_BUTTON);
  setProperty("ENABLE_MOX_BUTTON",value);
  sprintf(value,"%d",MOX_BUTTON);
  setProperty("MOX_BUTTON",value);
#endif

  sprintf(value,"%d",ENABLE_E2_BUTTON);
  setProperty("ENABLE_E2_BUTTON",value);
  sprintf(value,"%d",ENABLE_E3_BUTTON);
  setProperty("ENABLE_E3_BUTTON",value);
  sprintf(value,"%d",ENABLE_E4_BUTTON);
  setProperty("ENABLE_E4_BUTTON",value);
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  sprintf(value,"%d",ENABLE_E5_BUTTON);
  setProperty("ENABLE_E5_BUTTON",value);
#endif
#ifdef LOCALCW		
  sprintf(value,"%d",ENABLE_CW_BUTTONS);		
  setProperty("ENABLE_CW_BUTTONS",value);		
  sprintf(value,"%d",CW_ACTIVE_LOW);		
  setProperty("CW_ACTIVE_LOW",value);		
  sprintf(value,"%d",CWL_BUTTON);		
  setProperty("CWL_BUTTON",value);		
  sprintf(value,"%d",CWR_BUTTON);		
  setProperty("CWR_BUTTON",value);		
  sprintf(value,"%d",SIDETONE_GPIO);		
  setProperty("SIDETONE_GPIO",value);		
  sprintf(value,"%d",ENABLE_GPIO_SIDETONE);		
  setProperty("ENABLE_GPIO_SIDETONE",value);		
#endif

  saveProperties("gpio.props");

}

static void setup_pin(int pin, int up_down, void(*pAlert)(void)) {
  int rc;
  pinMode(pin,INPUT);
  pullUpDnControl(pin,up_down);
  usleep(10000);
  rc=wiringPiISR(pin,INT_EDGE_BOTH,pAlert);
  if(rc<0) {
    fprintf(stderr,"wirngPiISR returned %d\n",rc);
  }
}


static void setup_encoder_pin(int pin, int up_down, void(*pAlert)(void)) {
  int rc;
  pinMode(pin,INPUT);
  pullUpDnControl(pin,up_down);
  usleep(10000);
  if(pAlert!=NULL) {
    rc=wiringPiISR(pin,INT_EDGE_BOTH,pAlert);
    if(rc<0) {
      fprintf(stderr,"wirngPiISR returned %d\n",rc);
    }
  }
}

#ifdef LOCALCW

//
// We generate interrupts only on falling edge
//

static void setup_cw_pin(int pin, void(*pAlert)(void)) {
  int rc;
  pinMode(pin,INPUT);
  pullUpDnControl(pin,PUD_UP);
  usleep(10000);
  rc=wiringPiISR(pin,INT_EDGE_BOTH,pAlert);
  if(rc<0) {
    fprintf(stderr,"wirngPiISR returned %d\n",rc);
  }
}

static void cwAlert_left() {
    int level;
    if (cw_keyer_internal != 0) return; // as quickly as possible
    level=digitalRead(CWL_BUTTON);
    //fprintf(stderr,"cwl button : level=%d \n",level);
    //the second parameter of keyer_event ("state") is TRUE on key-down
    keyer_event(1, CW_ACTIVE_LOW ? (level==0) : level);
}

static void cwAlert_right() {
    int level;
    if (cw_keyer_internal != 0) return; // as quickly as possible
    level=digitalRead(CWR_BUTTON);
    //fprintf(stderr,"cwr button : level=%d \n",level);
     keyer_event(0, CW_ACTIVE_LOW ? (level==0) : level);
}

//
// The following functions are an interface for
// other parts to access CW gpio functions
// (query left and right paddle, set sidetone output)
//
int gpio_left_cw_key() {
  int val=digitalRead(CWL_BUTTON);
  return CW_ACTIVE_LOW? (val==0) : val;
}

int gpio_right_cw_key() {
  int val=digitalRead(CWR_BUTTON);
  return CW_ACTIVE_LOW? (val==0) : val;
}

int gpio_cw_sidetone_enabled() {
  return ENABLE_GPIO_SIDETONE;
}

void gpio_cw_sidetone_set(int level) {
  if (ENABLE_GPIO_SIDETONE) {
    digitalWrite(SIDETONE_GPIO, level);
  }
}
#endif

int gpio_init() {
  int i;

  fprintf(stderr,"gpio_wiringpi: gpio_init\n");

  gpio_restore_state();

  wiringPiSetup(); // use WiringPi pin numbers

  if(ENABLE_VFO_ENCODER) {
#ifdef CONTROLLER2_V2
#ifdef VFO_HAS_FUNCTION
    setup_pin(VFO_FUNCTION, PUD_UP, &vfoFunctionAlert);
    vfoFunction=0;
#endif
    setup_encoder_pin(VFO_ENCODER_A,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,&vfoEncoderA);
    setup_encoder_pin(VFO_ENCODER_B,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,NULL);
    //setup_encoder_pin(VFO_ENCODER_B,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,&vfoEncoderB);
#else
    setup_encoder_pin(VFO_ENCODER_A,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,&vfoEncoderA);
    setup_encoder_pin(VFO_ENCODER_B,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,NULL);
    //setup_encoder_pin(VFO_ENCODER_B,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,&vfoEncoderB);
#endif
    vfoEncoderPos=0;
  }

  if(ENABLE_E2_ENCODER) {
    setup_pin(E2_FUNCTION, PUD_UP, &e2FunctionAlert);
	  
    setup_encoder_pin(E2_ENCODER_A,ENABLE_E2_PULLUP?PUD_UP:PUD_OFF,&e2EncoderA);
    setup_encoder_pin(E2_ENCODER_B,ENABLE_E2_PULLUP?PUD_UP:PUD_OFF,NULL);
    e2EncoderPos=0;


#ifdef CONTROLLER2_V2
    setup_encoder_pin(E2_TOP_ENCODER_A,ENABLE_E2_PULLUP?PUD_UP:PUD_OFF,&e2TopEncoderA);
    setup_encoder_pin(E2_TOP_ENCODER_B,ENABLE_E2_PULLUP?PUD_UP:PUD_OFF,NULL);
    e2TopEncoderPos=0;
#endif
  }

  if(ENABLE_E3_ENCODER) {
    setup_pin(E3_FUNCTION, PUD_UP, &e3FunctionAlert);
	
    setup_encoder_pin(E3_ENCODER_A,ENABLE_E3_PULLUP?PUD_UP:PUD_OFF,&e3EncoderA);
    setup_encoder_pin(E3_ENCODER_B,ENABLE_E3_PULLUP?PUD_UP:PUD_OFF,NULL);
    e3EncoderPos=0;

#ifdef CONTROLLER2_V2
    setup_encoder_pin(E3_TOP_ENCODER_A,ENABLE_E3_PULLUP?PUD_UP:PUD_OFF,&e3TopEncoderA);
    setup_encoder_pin(E3_TOP_ENCODER_B,ENABLE_E3_PULLUP?PUD_UP:PUD_OFF,NULL);
    e3TopEncoderPos=0;
#endif
  }

  if(ENABLE_E4_ENCODER) {
    setup_pin(E4_FUNCTION, PUD_UP, &e4FunctionAlert);
	  
    setup_encoder_pin(E4_ENCODER_A,ENABLE_E4_PULLUP?PUD_UP:PUD_OFF,&e4EncoderA);
    setup_encoder_pin(E4_ENCODER_B,ENABLE_E4_PULLUP?PUD_UP:PUD_OFF,NULL);
    e4EncoderPos=0;
	  
#ifdef CONTROLLER2_V2
    setup_encoder_pin(E4_TOP_ENCODER_A,ENABLE_E4_PULLUP?PUD_UP:PUD_OFF,&e4TopEncoderA);
    setup_encoder_pin(E4_TOP_ENCODER_B,ENABLE_E4_PULLUP?PUD_UP:PUD_OFF,NULL);
    e4TopEncoderPos=0;
#endif
  }

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  if(ENABLE_E5_ENCODER) {
    setup_pin(E5_FUNCTION, PUD_UP, &e5FunctionAlert);

    setup_encoder_pin(E5_ENCODER_A,ENABLE_E5_PULLUP?PUD_UP:PUD_OFF,&e5EncoderA);
    setup_encoder_pin(E5_ENCODER_B,ENABLE_E5_PULLUP?PUD_UP:PUD_OFF,NULL);
    e5EncoderPos=0;

#if defined (CONTROLLER2_V2)
    setup_encoder_pin(E5_TOP_ENCODER_A,ENABLE_E5_PULLUP?PUD_UP:PUD_OFF,&e5TopEncoderA);
    setup_encoder_pin(E5_TOP_ENCODER_B,ENABLE_E5_PULLUP?PUD_UP:PUD_OFF,NULL);
    e5TopEncoderPos=0;
#endif
  }
#endif

#if !defined (CONTROLLER2_V2) && !defined(CONTROLLER2_V1)
  if(ENABLE_FUNCTION_BUTTON) {
    setup_pin(FUNCTION_BUTTON, PUD_UP, &functionAlert);
  }

  if(ENABLE_MOX_BUTTON) {
    setup_pin(MOX_BUTTON, PUD_UP, &moxAlert);
  }

  if(ENABLE_S1_BUTTON) {
    setup_pin(S1_BUTTON, PUD_UP, &s1Alert);
  }

  if(ENABLE_S2_BUTTON) {
    setup_pin(S2_BUTTON, PUD_UP, &s2Alert);
  }

  if(ENABLE_S3_BUTTON) {
    setup_pin(S3_BUTTON, PUD_UP, &s3Alert);
  }

  if(ENABLE_S4_BUTTON) {
    setup_pin(S4_BUTTON, PUD_UP, &s4Alert);
  }

  if(ENABLE_S5_BUTTON) {
    setup_pin(S5_BUTTON, PUD_UP, &s5Alert);
  }

  if(ENABLE_S6_BUTTON) {
    setup_pin(S6_BUTTON, PUD_UP, &s6Alert);
  }
#endif

  rotary_encoder_thread_id = g_thread_new( "rotary encoder", rotary_encoder_thread, NULL);
  if( ! rotary_encoder_thread_id )
  {
    fprintf(stderr,"g_thread_new failed on rotary_encoder_thread\n");
    exit( -1 );
  }
  fprintf(stderr, "rotary_encoder_thread: id=%p\n",rotary_encoder_thread_id);

#if defined(CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  // setup i2c
  i2c_init();

  // setup interrupt pin
  fprintf(stderr,"setup i2c interrupt: pin=%d\n",I2C_INTERRUPT);
  //digitalWrite(I2C_INTERRUPT,0); // clear pin
  pinMode(I2C_INTERRUPT,INPUT);
  pullUpDnControl(I2C_INTERRUPT,PUD_UP);
  usleep(10000);
  //wiringPiISR(I2C_INTERRUPT,INT_EDGE_FALLING,pI2CInterrupt);
  wiringPiISR(I2C_INTERRUPT,INT_EDGE_BOTH,pI2CInterrupt);
#endif

#ifdef LOCALCW
  fprintf(stderr,"GPIO: ENABLE_CW_BUTTONS=%d  CWL_BUTTON=%d CWR_BUTTON=%d\n", ENABLE_CW_BUTTONS, CWL_BUTTON, CWR_BUTTON);
  if(ENABLE_CW_BUTTONS) {	
    setup_cw_pin(CWL_BUTTON, cwAlert_left);
    setup_cw_pin(CWR_BUTTON, cwAlert_right);
  }
  if (ENABLE_GPIO_SIDETONE) {
//
//  use this pin as an output pin and
//  set its value to LOW
//
    pinMode(SIDETONE_GPIO, OUTPUT);
    digitalWrite(SIDETONE_GPIO, 0);
  }
#endif

  return 0;
}

void gpio_close() {
    running=0;
}

int vfo_encoder_get_pos() {
  int pos=vfoEncoderPos;

  if(vfo_encoder_divisor>1) {
    if(pos<0 && pos>-vfo_encoder_divisor) {
        pos=0;
    } else if(pos>0 && pos<vfo_encoder_divisor) {
        pos=0;
    }
    pos=pos/vfo_encoder_divisor;
    vfoEncoderPos=vfoEncoderPos-(pos*vfo_encoder_divisor);
  } else {
    vfoEncoderPos=0;
  }
  return pos;
}

int e2_encoder_get_pos() {
    int pos=e2EncoderPos;
    e2EncoderPos=0;
    return pos;
}

int e3_encoder_get_pos() {
    int pos=e3EncoderPos;
    e3EncoderPos=0;
    return pos;
}

int e4_encoder_get_pos() {
    int pos=e4EncoderPos;
    e4EncoderPos=0;
    return pos;
}

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
int e5_encoder_get_pos() {
    int pos=e5EncoderPos;
    e5EncoderPos=0;
    return pos;
}
#endif

int e4_function_get_state() {
    return e4_sw_action;
}

#ifdef CONTROLLER2_V2
int e2_top_encoder_get_pos() {
    int pos=e2TopEncoderPos;
    e2TopEncoderPos=0;
    return pos;
}

int e3_top_encoder_get_pos() {
    int pos=e3TopEncoderPos;
    e3TopEncoderPos=0;
    return pos;
}

int e4_top_encoder_get_pos() {
    int pos=e4TopEncoderPos;
    e4TopEncoderPos=0;
    return pos;
}

int e5_top_encoder_get_pos() {
    int pos=e5TopEncoderPos;
    e5TopEncoderPos=0;
    return pos;
}
#endif

int function_get_state() {
    return function_state;
}

int band_get_state() {
    return band_state;
}

int bandstack_get_state() {
    return bandstack_state;
}

int mode_get_state() {
    return mode_state;
}

int filter_get_state() {
    return filter_state;
}

int noise_get_state() {
    return noise_state;
}

int agc_get_state() {
    return agc_state;
}

int mox_get_state() {
    return mox_state;
}

int lock_get_state() {
    return lock_state;
}


static int vfo_encoder_changed(void *data) {
  if(!locked) {
    int pos=(int)data;
    vfo_step(pos);
  }
  //free(data);
  return 0;
}

static void encoder_changed(int action,int pos) {
  double value;
  int mode;
  int id;
  FILTER * band_filters=filters[vfo[active_receiver->id].mode];
  FILTER *band_filter;
  FILTER *filter;
  int new_val;

  switch(action) {
    case ENCODER_AF_GAIN_RX1:
      value=receiver[0]->volume;
      value+=(double)pos/100.0;
      if(value<0.0) {
        value=0.0;
      } else if(value>1.0) {
        value=1.0;
      }
      set_af_gain(0,value);
      break;
    case ENCODER_AF_GAIN_RX2:
      value=receiver[1]->volume;
      value+=(double)pos/100.0;
      if(value<0.0) {
        value=0.0;
      } else if(value>1.0) {
        value=1.0;
      }
      set_af_gain(1,value);
      break;
    case ENCODER_RF_GAIN_RX1:
      value=receiver[0]->rf_gain;
      value+=(double)pos;
      if(value<0.0) {
        value=0.0;
      } else if(value>100.0) {
        value=100.0;
      }
      set_rf_gain(0,value);
      break;
    case ENCODER_RF_GAIN_RX2:
      value=receiver[1]->rf_gain;
      value+=(double)pos;
      if(value<0.0) {
        value=0.0;
      } else if(value>71.0) {
        value=71.0;
      }
      set_rf_gain(1,value);
      break;
    case ENCODER_AGC_GAIN_RX1:
      value=receiver[0]->agc_gain;
      value+=(double)pos;
      if(value<-20.0) {
        value=-20.0;
      } else if(value>120.0) {
        value=120.0;
      }
      set_agc_gain(0,value);
      break;
    case ENCODER_AGC_GAIN_RX2:
      value=receiver[1]->agc_gain;
      value+=(double)pos;
      if(value<-20.0) {
        value=-20.0;
      } else if(value>120.0) {
        value=120.0;
      }
      set_agc_gain(1,value);
      break;
    case ENCODER_IF_WIDTH_RX1:
      filter_width_changed(0,pos);
      break;
    case ENCODER_IF_WIDTH_RX2:
      filter_width_changed(1,pos);
      break;
    case ENCODER_ATTENUATION:
      value=(double)adc_attenuation[active_receiver->adc];
      value+=(double)pos;
      if(value<0.0) {
        value=0.0;
#ifdef RADIOBERRY
      } else if (value>60.0) {
        value=60.0;
      }
#else
	  } else if (value>31.0) {
        value=31.0;
      }	
#endif
      set_attenuation_value(value);
      break;
    case ENCODER_MIC_GAIN:
      value=mic_gain;
      value+=(double)pos;
      if(value<-12.0) {
        value=-12.0;
      } else if(value>50.0) {
        value=50.0;
      }
      set_mic_gain(value);
      break;
    case ENCODER_DRIVE:
      value=getDrive();
      value+=(double)pos;
      if(value<0.0) {
        value=0.0;
      } else if(value>100.0) {
        value=100.0;
      }
      set_drive(value);
      break;
    case ENCODER_RIT_RX1:
      value=(double)vfo[receiver[0]->id].rit;
      value+=(double)(pos*rit_increment);
      if(value<-10000.0) {
        value=-10000.0;
      } else if(value>10000.0) {
        value=10000.0;
      }
      vfo[receiver[0]->id].rit=(int)value;
      receiver_frequency_changed(receiver[0]);
      g_idle_add(ext_vfo_update,NULL);
      break;
    case ENCODER_RIT_RX2:
      value=(double)vfo[receiver[1]->id].rit;
      value+=(double)(pos*rit_increment);
      if(value<-10000.0) {
        value=-10000.0;
      } else if(value>10000.0) {
        value=10000.0;
      }
      vfo[receiver[1]->id].rit=(int)value;
      receiver_frequency_changed(receiver[1]);
      g_idle_add(ext_vfo_update,NULL);
      break;
    case ENCODER_XIT:
      value=(double)transmitter->xit;
      value+=(double)(pos*rit_increment);
      if(value<-10000.0) {
        value=-10000.0;
      } else if(value>10000.0) {
        value=10000.0;
      }
      transmitter->xit=(int)value;
      if(protocol==NEW_PROTOCOL) {
        schedule_high_priority();
      }
      g_idle_add(ext_vfo_update,NULL);
      break;
    case ENCODER_CW_SPEED:
      value=(double)cw_keyer_speed;
      value+=(double)pos;
      if(value<1.0) {
        value=1.0;
      } else if(value>60.0) {
        value=60.0;
      }
      cw_keyer_speed=(int)value;
      g_idle_add(ext_vfo_update,NULL);
      break;
    case ENCODER_CW_FREQUENCY:
      value=(double)cw_keyer_sidetone_frequency;
      value+=(double)pos;
      if(value<0.0) {
        value=0.0;
      } else if(value>1000.0) {
        value=1000.0;
      }
      cw_keyer_sidetone_frequency=(int)value;
      g_idle_add(ext_vfo_update,NULL);
      break;
    case ENCODER_PANADAPTER_HIGH:
      value=(double)active_receiver->panadapter_high;
      value+=(double)pos;
      active_receiver->panadapter_high=(int)value;
      break;
    case ENCODER_PANADAPTER_LOW:
      value=(double)active_receiver->panadapter_low;
      value+=(double)pos;
      active_receiver->panadapter_low=(int)value;
      break;
    case ENCODER_SQUELCH:
      value=active_receiver->squelch;
      value+=(double)pos;
      if(value<0.0) {
        value=0.0;
      } else if(value>100.0) {
        value=100.0;
      }
      active_receiver->squelch=value;
      set_squelch();
      break;
    case ENCODER_COMP:
      value=(double)transmitter->compressor_level;
      value+=(double)pos;
      if(value<0.0) {
        value=0.0;
      } else if(value>20.0) {
        value=20.0;
      }
      transmitter->compressor_level=(int)value;
      set_compression(transmitter);
      break;
    case ENCODER_DIVERSITY_GAIN:
      update_diversity_gain((double)pos);
      break;
    case ENCODER_DIVERSITY_PHASE:
      update_diversity_phase((double)pos);
      break;
  }
}

static int e2_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E2_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e2_encoder_action,pos);
  }
  return 0;
}

static int e3_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E3_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e3_encoder_action,pos);
  }
  return 0;
}

static int e4_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E4_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e4_encoder_action,pos);
  }
  return 0;
}

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
static int e5_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E5_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e5_encoder_action,pos);
  }
  return 0;
}
#endif


#ifdef CONTROLLER2_V2
static int e2_top_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E2_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e2_top_encoder_action,pos);
  }
  return 0;
}

static int e3_top_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E3_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e3_top_encoder_action,pos);
  }
  return 0;
}

static int e4_top_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E4_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e4_top_encoder_action,pos);
  }
  return 0;
}

static int e5_top_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E5_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e5_top_encoder_action,pos);
  }
  return 0;
}
#endif

static gpointer rotary_encoder_thread(gpointer data) {
    int pos;

    sleep(2);

    running=1;
    while(1) {

        pos=vfo_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(vfo_encoder_changed,(gpointer)pos);
        }

        pos=e2_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e2_encoder_changed,(gpointer)pos);
        }

        pos=e3_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e3_encoder_changed,(gpointer)pos);
        }

        pos=e4_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e4_encoder_changed,(gpointer)pos);
        }

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
        pos=e5_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e5_encoder_changed,(gpointer)pos);
        }
#endif

#if defined (CONTROLLER2_V2)
        pos=e2_top_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e2_top_encoder_changed,(gpointer)pos);
        }

        pos=e3_top_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e3_top_encoder_changed,(gpointer)pos);
        }

        pos=e4_top_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e4_top_encoder_changed,(gpointer)pos);
        }

        pos=e5_top_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e5_top_encoder_changed,(gpointer)pos);
        }
#endif

#ifdef sx1509
        // buttons only generate interrupt when
        // pushed onODER_AF_GAIN = 0,
        function_state = 0;
        band_state = 0;
        bandstack_state = 0;
        mode_state = 0;
        filter_state = 0;
        noise_state = 0;
        agc_state = 0;
        mox_state = 0;
        lock_state = 0;
#endif
        if(running==0) {
          fprintf(stderr,"gpio_thread: quitting (running==0)\n");
          g_thread_exit(NULL);
        }
        usleep(100000);

    }
    return NULL;
}
