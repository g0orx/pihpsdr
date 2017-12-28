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
#include "gpio.h"
#ifdef CONTROLLER2
#include "i2c.h"
#endif
#include "ext.h"
#include "sliders.h"

// debounce settle time in ms
#define SETTLE_TIME 100

#ifdef CONTROLLER2

int ENABLE_VFO_ENCODER=1;
int ENABLE_VFO_PULLUP=0;
int VFO_ENCODER_A=1;
int VFO_ENCODER_B=0;
#ifdef VFO_HAS_FUNCTION
int VFO_FUNCTION=12;
#endif
int ENABLE_E1_ENCODER=1;
int ENABLE_E1_PULLUP=0;
int E1_ENCODER_A=28;
int E1_ENCODER_B=25;
int E1_FUNCTION=3;
int ENABLE_E2_ENCODER=1;
int ENABLE_E2_PULLUP=0;
int E2_ENCODER_A=7;
int E2_ENCODER_B=29;
int E2_FUNCTION=2;
int ENABLE_E3_ENCODER=1;
int ENABLE_E3_PULLUP=0;
int E3_ENCODER_A=27;
int E3_ENCODER_B=24;
int E3_FUNCTION=4;
int ENABLE_E4_ENCODER=1;
int ENABLE_E4_PULLUP=0;
int E4_ENCODER_A=6;
int E4_ENCODER_B=10;
int E4_FUNCTION=5;

int ENABLE_E1_BUTTON=1;
int ENABLE_E2_BUTTON=1;
int ENABLE_E3_BUTTON=1;
int ENABLE_E4_BUTTON=1;

int I2C_INTERRUPT=16;

#else
// uses wiringpi pin numbers
int ENABLE_VFO_ENCODER=1;
int ENABLE_VFO_PULLUP=1;
int VFO_ENCODER_A=1;
int VFO_ENCODER_B=0;
int ENABLE_E1_ENCODER=1;
int ENABLE_E1_PULLUP=0;
int E1_ENCODER_A=28;
int E1_ENCODER_B=25;
int E1_FUNCTION=6;
int ENABLE_E2_ENCODER=1;
int ENABLE_E2_PULLUP=0;
int E2_ENCODER_A=27;
int E2_ENCODER_B=24;
int E2_FUNCTION=10;
int ENABLE_E3_ENCODER=1;
int ENABLE_E3_PULLUP=0;
int E3_ENCODER_A=7;
int E3_ENCODER_B=29;
int E3_FUNCTION=11;
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
int ENABLE_E1_BUTTON=1;
int ENABLE_E2_BUTTON=1;
int ENABLE_E3_BUTTON=1;
int ENABLE_CW_BUTTONS=1;
#endif


static volatile int vfoEncoderPos;
static volatile int vfo_A;
static volatile int vfo_B;
static volatile int vfoFunction;
static volatile int e1EncoderPos;
static volatile int e1Function;
int e1_encoder_action=ENCODER_AF_GAIN;
static volatile int e2EncoderPos;
static volatile int e2Function;
int e2_encoder_action=ENCODER_DRIVE;
static volatile int e3EncoderPos;
static volatile int e3Function;
int e3_encoder_action=ENCODER_ATTENUATION;
#ifdef CONTROLLER2
static volatile int e4EncoderPos;
static volatile int e4Function;
int e4_encoder_action=ENCODER_MIC_GAIN;
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
static int e1_function=0;
static int previous_e1_function=0;
static int e2_function=0;
static int previous_e2_function=0;
static int e3_function=0;
static int previous_e3_function=0;
#ifdef CONTROLLER2
static int e4_function=0;
static int previous_e4_function=0;
#endif
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

char *encoder_string[] = {
"AF GAIN",
"AGC GAIN",
"ATTENUATION",
"MIC GAIN",
"DRIVE",
"RIT",
"CW SPEED",
"CW FREQUENCY",
"PANADAPTER HIGH",
"PANADAPTER LOW",
"SQUELCH",
"COMP"
};

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
  int encoder=(int)data;
  start_encoder(encoder);
  return 0;
}

static unsigned long e1debounce=0;

static void e1FunctionAlert() {
    int level=digitalRead(E1_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)1);
    }
}

static unsigned long e2debounce=0;

static void e2FunctionAlert() {
    int level=digitalRead(E2_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)2);
    }
}

static unsigned long e3debounce=0;

static void e3FunctionAlert() {
    int level=digitalRead(E3_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)3);
    }
}

#ifdef CONTROLLER2
static unsigned long e4debounce=0;

static void e4FunctionAlert() {
    int level=digitalRead(E4_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)4);
    }
}
#endif

#ifndef CONTROLLER2
static unsigned long function_debounce=0;

static void functionAlert() {
    int t=millis();
    if(t-function_debounce > SETTLE_TIME) {
      int level=digitalRead(FUNCTION_BUTTON);
      if(level==0) {
        if(running) g_idle_add(function_pressed,NULL);
      }
      function_debounce=t;
    }
}

static unsigned long s1_debounce=0;

static void s1Alert() {
    int t=millis();
    if(t-s1_debounce > SETTLE_TIME) {
      int level=digitalRead(S1_BUTTON);
      if(level==0) {
        g_idle_add(s1_pressed,NULL);
      } else {
        g_idle_add(s1_released,NULL);
      }
      s1_debounce=t;
    }
}

static unsigned long s2_debounce=0;

static void s2Alert() {
    int t=millis();
    if(t-s2_debounce > SETTLE_TIME) {
      int level=digitalRead(S2_BUTTON);
      if(level==0) {
        g_idle_add(s2_pressed,NULL);
      } else {
        g_idle_add(s2_released,NULL);
      }
      s2_debounce=t;
    }
}

static unsigned long s3_debounce=0;

static void s3Alert() {
    int t=millis();
    if(t-s3_debounce > SETTLE_TIME) {
      int level=digitalRead(S3_BUTTON);
      if(level==0) {
        g_idle_add(s3_pressed,NULL);
      } else {
        g_idle_add(s3_released,NULL);
      }
      s3_debounce=t;
    }
}

static unsigned long s4_debounce=0;

static void s4Alert() {
    int t=millis();
    if(t-s4_debounce > SETTLE_TIME) {
      int level=digitalRead(S4_BUTTON);
      if(level==0) {
        g_idle_add(s4_pressed,NULL);
      } else {
        g_idle_add(s4_released,NULL);
      }
      s4_debounce=t;
    }
}

static unsigned long s5_debounce=0;

static void s5Alert() {
    int t=millis();
    if(t-s5_debounce > SETTLE_TIME) {
      int level=digitalRead(S5_BUTTON);
      if(level==0) {
        g_idle_add(s5_pressed,NULL);
      } else {
        g_idle_add(s5_released,NULL);
      }
      s5_debounce=t;
    }
}

static unsigned long s6_debounce=0;

static void s6Alert() {
    int t=millis();
    if(t-s6_debounce > SETTLE_TIME) {
      int level=digitalRead(S6_BUTTON);
      if(level==0) {
        g_idle_add(s6_pressed,NULL);
      } else {
        g_idle_add(s6_released,NULL);
      }
      s6_debounce=t;
    }
}

static unsigned long mox_debounce=0;

static void moxAlert() {
    int t=millis();
    if(t-mox_debounce > SETTLE_TIME) {
      int level=digitalRead(MOX_BUTTON);
      if(level==0) {
        g_idle_add(mox_pressed,(gpointer)NULL);
      }
      mox_debounce=t;
    }
}
#endif


#ifdef VFO_HAS_FUNCTION
static unsigned long vfo_debounce=0;

static void vfoFunctionAlert() {
    int t=millis();
    if(t-vfo_debounce > SETTLE_TIME) {
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
    int switch_A=digitalRead(VFO_ENCODER_A);
    int switch_B=digitalRead(VFO_ENCODER_B);
    if(vfo_A==switch_A && vfo_B==switch_B) {
      return; // same as last
    }
    vfo_A=switch_A;
    vfo_B=switch_B;
    if(switch_A && switch_B) {
      if(A_or_B==VFO_ENCODER_B) {
        vfoEncoderPos++;
      } else {
        vfoEncoderPos--;
      }
    }
}

static void vfoEncoderA() {
    vfoEncoderInt(VFO_ENCODER_A);
}

static void vfoEncoderB() {
    vfoEncoderInt(VFO_ENCODER_B);
}

static void e1EncoderInterrupt(int gpio) {
  static int e1CurrentA=1, e1CurrentB=1;

  int levelA=digitalRead(E1_ENCODER_A);
  int levelB=digitalRead(E1_ENCODER_B);

  if(e1CurrentA==levelA && e1CurrentB==levelB) {
    return;
  }

  e1CurrentA=levelA;
  e1CurrentB=levelB;

  if(levelA && levelB) {
    if(gpio==E1_ENCODER_B) {
      --e1EncoderPos;
    } else {
      ++e1EncoderPos;
    }
  }
}

static void e1EncoderA() {
  e1EncoderInterrupt(E1_ENCODER_A);
}

static void e1EncoderB() {
  e1EncoderInterrupt(E1_ENCODER_B);
}

static void e2EncoderInterrupt(int gpio) {
  static int e2CurrentA=1, e2CurrentB=1;

  int levelA=digitalRead(E2_ENCODER_A);
  int levelB=digitalRead(E2_ENCODER_B);

  if(e2CurrentA==levelA && e2CurrentB==levelB) {
    return;
  }

  e2CurrentA=levelA;
  e2CurrentB=levelB;

  if(levelA && levelB) {
    if(gpio==E2_ENCODER_B) {
      --e2EncoderPos;
    } else {
      ++e2EncoderPos;
    }
  }
}

static void e2EncoderA() {
  e2EncoderInterrupt(E2_ENCODER_A);
}

static void e2EncoderB() {
  e2EncoderInterrupt(E2_ENCODER_B);
}

static void e3EncoderInterrupt(int gpio) {
  static int e3CurrentA=1, e3CurrentB=1;

  int levelA=digitalRead(E3_ENCODER_A);
  int levelB=digitalRead(E3_ENCODER_B);

  if(e3CurrentA==levelA && e3CurrentB==levelB) {
    return;
  }

  e3CurrentA=levelA;
  e3CurrentB=levelB;

  if(levelA && levelB) {
    if(gpio==E3_ENCODER_B) {
      --e3EncoderPos;
    } else {
      ++e3EncoderPos;
    }
  }
}

static void e3EncoderA() {
  e3EncoderInterrupt(E3_ENCODER_A);
}

static void e3EncoderB() {
  e3EncoderInterrupt(E3_ENCODER_B);
}

#ifdef CONTROLLER2
static void e4EncoderInterrupt(int gpio) {
  static int e4CurrentA=1, e4CurrentB=1;

  int levelA=digitalRead(E4_ENCODER_A);
  int levelB=digitalRead(E4_ENCODER_B);

  if(e4CurrentA==levelA && e4CurrentB==levelB) {
    return;
  }

  e4CurrentA=levelA;
  e4CurrentB=levelB;

  if(levelA && levelB) {
    if(gpio==E4_ENCODER_B) {
      --e4EncoderPos;
    } else {
      ++e4EncoderPos;
    }
  }
}

static void e4EncoderA() {
  e4EncoderInterrupt(E4_ENCODER_A);
}

static void e4EncoderB() {
  e4EncoderInterrupt(E4_ENCODER_B);
}
#endif

#ifdef CONTROLLER2
static void pI2CInterrupt() {
    int level=digitalRead(I2C_INTERRUPT);
    if(level==0) {
      i2c_interrupt();
    }
}
#endif

void gpio_restore_state() {
  char* value;
  loadProperties("gpio.props");
  value=getProperty("ENABLE_VFO_ENCODER");
  if(value) ENABLE_VFO_ENCODER=atoi(value);
  value=getProperty("ENABLE_VFO_PULLUP");
  if(value) ENABLE_VFO_PULLUP=atoi(value);
  value=getProperty("VFO_ENCODER_A");
  if(value) VFO_ENCODER_A=atoi(value);
  value=getProperty("VFO_ENCODER_B");
  if(value) VFO_ENCODER_B=atoi(value);
  value=getProperty("ENABLE_E1_ENCODER");
  if(value) ENABLE_E1_ENCODER=atoi(value);
  value=getProperty("ENABLE_E1_PULLUP");
  if(value) ENABLE_E1_PULLUP=atoi(value);
  value=getProperty("E1_ENCODER_A");
  if(value) E1_ENCODER_A=atoi(value);
  value=getProperty("E1_ENCODER_B");
  if(value) E1_ENCODER_B=atoi(value);
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
#ifdef CONTROLLER2
  value=getProperty("ENABLE_E4_ENCODER");
  if(value) ENABLE_E4_ENCODER=atoi(value);
  value=getProperty("ENABLE_E4_PULLUP");
  if(value) ENABLE_E4_PULLUP=atoi(value);
  value=getProperty("E4_ENCODER_A");
  if(value) E4_ENCODER_A=atoi(value);
  value=getProperty("E4_ENCODER_B");
  if(value) E4_ENCODER_B=atoi(value);
#endif
#ifndef CONTROLLER2
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

  value=getProperty("ENABLE_E1_BUTTON");
  if(value) ENABLE_E1_BUTTON=atoi(value);
  value=getProperty("ENABLE_E2_BUTTON");
  if(value) ENABLE_E2_BUTTON=atoi(value);
  value=getProperty("ENABLE_E3_BUTTON");
  if(value) ENABLE_E3_BUTTON=atoi(value);
#ifdef CONTROLLER2
  value=getProperty("ENABLE_E4_BUTTON");
  if(value) ENABLE_E4_BUTTON=atoi(value);
#endif

}

void gpio_save_state() {
   char value[80];

  sprintf(value,"%d",ENABLE_VFO_ENCODER);
  setProperty("ENABLE_VFO_ENCODER",value);
  sprintf(value,"%d",ENABLE_VFO_PULLUP);
  setProperty("ENABLE_VFO_PULLUP",value);
  sprintf(value,"%d",VFO_ENCODER_A);
  setProperty("VFO_ENCODER_A",value);
  sprintf(value,"%d",VFO_ENCODER_B);
  setProperty("VFO_ENCODER_B",value);
  sprintf(value,"%d",ENABLE_E1_ENCODER);
  setProperty("ENABLE_E1_ENCODER",value);
  sprintf(value,"%d",ENABLE_E1_PULLUP);
  setProperty("ENABLE_E1_PULLUP",value);
  sprintf(value,"%d",E1_ENCODER_A);
  setProperty("E1_ENCODER_A",value);
  sprintf(value,"%d",E1_ENCODER_B);
  setProperty("E1_ENCODER_B",value);
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
#ifdef CONTROLLER2
  sprintf(value,"%d",ENABLE_E4_ENCODER);
  setProperty("ENABLE_E4_ENCODER",value);
  sprintf(value,"%d",ENABLE_E4_PULLUP);
  setProperty("ENABLE_E4_PULLUP",value);
  sprintf(value,"%d",E4_ENCODER_A);
  setProperty("E4_ENCODER_A",value);
  sprintf(value,"%d",E4_ENCODER_B);
  setProperty("E4_ENCODER_B",value);
#endif
#ifndef CONTROLLER2
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

  sprintf(value,"%d",ENABLE_E1_BUTTON);
  setProperty("ENABLE_E1_BUTTON",value);
  sprintf(value,"%d",ENABLE_E2_BUTTON);
  setProperty("ENABLE_E2_BUTTON",value);
  sprintf(value,"%d",ENABLE_E3_BUTTON);
  setProperty("ENABLE_E3_BUTTON",value);
#ifdef CONTROLLER2
  sprintf(value,"%d",ENABLE_E4_BUTTON);
  setProperty("ENABLE_E4_BUTTON",value);
#endif
  saveProperties("gpio.props");
}

static void setup_pin(int pin, int up_down, void(*pAlert)(void)) {
fprintf(stderr,"setup_pin: pin=%d mode=%d updown=%d\n",pin,INPUT,up_down);
  pinMode(pin,INPUT);
  pullUpDnControl(pin,up_down);
  usleep(10000);
  wiringPiISR(pin,INT_EDGE_BOTH,pAlert);
}


static void setup_encoder_pin(int pin, int up_down, void(*pAlert)(void)) {
fprintf(stderr,"setup_encoder_pin: pin=%d updown=%d\n",pin,up_down);
    pinMode(pin,INPUT);
    pullUpDnControl(pin,up_down);
    usleep(10000);
    wiringPiISR(pin,INT_EDGE_RISING,pAlert);
}

int gpio_init() {
  int i;

  fprintf(stderr,"gpio_wiringpi: gpio_init\n");

  gpio_restore_state();

  wiringPiSetup(); // use WiringPi pin numbers

  if(ENABLE_VFO_ENCODER) {
#ifdef CONTROLLER2
#ifdef VFO_HAS_FUNCTION
    setup_pin(VFO_FUNCTION, PUD_UP, &vfoFunctionAlert);
    vfoFunction=0;
#endif
    vfo_A=1;
    vfo_B=1;
    setup_encoder_pin(VFO_ENCODER_A,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,&vfoEncoderA);
    setup_encoder_pin(VFO_ENCODER_B,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,&vfoEncoderB);
#else
    vfo_A=1;
    vfo_B=1;
    setup_encoder_pin(VFO_ENCODER_A,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,&vfoEncoderA);
    setup_encoder_pin(VFO_ENCODER_B,ENABLE_VFO_PULLUP?PUD_UP:PUD_DOWN,&vfoEncoderB);
    //setup_pin(VFO_ENCODER_A,ENABLE_VFO_PULLUP?PUD_UP:PUD_OFF,&vfoEncoderA);
    //setup_pin(VFO_ENCODER_B,ENABLE_VFO_PULLUP?PUD_UP:PUD_OFF,&vfoEncoderB);
#endif
    vfoEncoderPos=0;
  }

  setup_pin(E1_FUNCTION, PUD_UP, &e1FunctionAlert);
  e1Function=0;

  if(ENABLE_E1_ENCODER) {
    setup_encoder_pin(E1_ENCODER_A,ENABLE_E1_PULLUP?PUD_UP:PUD_OFF,&e1EncoderA);
    setup_encoder_pin(E1_ENCODER_B,ENABLE_E1_PULLUP?PUD_UP:PUD_OFF,&e1EncoderB);
    e1EncoderPos=0;
  }

  setup_pin(E2_FUNCTION, PUD_UP, &e2FunctionAlert);
  e2Function=0;

  if(ENABLE_E2_ENCODER) {
    setup_encoder_pin(E2_ENCODER_A,ENABLE_E2_PULLUP?PUD_UP:PUD_OFF,&e2EncoderA);
    setup_encoder_pin(E2_ENCODER_B,ENABLE_E2_PULLUP?PUD_UP:PUD_OFF,&e2EncoderB);
    e2EncoderPos=0;
  }

  setup_pin(E3_FUNCTION, PUD_UP, &e3FunctionAlert);
  e3Function=0;

  if(ENABLE_E3_ENCODER) {
    setup_encoder_pin(E3_ENCODER_A,ENABLE_E3_PULLUP?PUD_UP:PUD_OFF,&e3EncoderA);
    setup_encoder_pin(E3_ENCODER_B,ENABLE_E3_PULLUP?PUD_UP:PUD_OFF,&e3EncoderB);
    e3EncoderPos=0;
  }

#ifdef CONTROLLER2
  setup_pin(E4_FUNCTION, PUD_UP, &e4FunctionAlert);
  e4Function=0;

  if(ENABLE_E4_ENCODER) {
    setup_encoder_pin(E4_ENCODER_A,ENABLE_E4_PULLUP?PUD_UP:PUD_OFF,&e4EncoderA);
    setup_encoder_pin(E4_ENCODER_B,ENABLE_E4_PULLUP?PUD_UP:PUD_OFF,&e4EncoderB);
    e4EncoderPos=0;
  }
#endif

#ifndef CONTROLLER2
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

#ifdef CONTROLLER2
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

int e1_encoder_get_pos() {
    int pos=e1EncoderPos;
    e1EncoderPos=0;
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

int e3_function_get_state() {
    return e3Function;
}

#ifdef CONTROLLER2
int e4_encoder_get_pos() {
    int pos=e4EncoderPos;
    e4EncoderPos=0;
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

static encoder_changed(int action,int pos) {
  double value;
  int mode;
  int id;
  FILTER * band_filters=filters[vfo[active_receiver->id].mode];
  FILTER *band_filter;
  FILTER *filter;
  int new_val;

  switch(action) {
    case ENCODER_AF_GAIN:
      value=active_receiver->volume;
      value+=(double)pos/100.0;
      if(value<0.0) {
        value=0.0;
      } else if(value>1.0) {
        value=1.0;
      }
      set_af_gain(value);
      break;
    case ENCODER_AGC_GAIN:
      value=active_receiver->agc_gain;
      value+=(double)pos;
      if(value<-20.0) {
        value=-20.0;
      } else if(value>120.0) {
        value=120.0;
      }
      set_agc_gain(value);
      break;
    case ENCODER_ATTENUATION:
      value=(double)adc_attenuation[active_receiver->adc];
      value+=(double)pos;
      if(value<0.0) {
        value=0.0;
      } else if (value>31.0) {
        value=31.0;
      }
      set_attenuation_value(value);
      break;
    case ENCODER_MIC_GAIN:
      value=mic_gain;
      //gain+=(double)pos/100.0;
      value+=(double)pos;
      if(value<-10.0) {
        value=-10.0;
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
    case ENCODER_RIT:
      value=(double)vfo[active_receiver->id].rit;
      value+=(double)(pos*rit_increment);
      if(value<-1000.0) {
        value=-1000.0;
      } else if(value>1000.0) {
        value=1000.0;
      }
      vfo[active_receiver->id].rit=(int)value;
      if(protocol==NEW_PROTOCOL) {
        schedule_high_priority();
      }
      vfo_update();
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
      vfo_update();
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
      vfo_update();
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
      set_squelch(active_receiver);
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
  }
}

static int e1_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E1_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e1_encoder_action,pos);
  }
  //free(data);
  return 0;
}

static int e2_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E2_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e2_encoder_action,pos);
  }
  //free(data);
  return 0;
}

static int e3_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E3_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e3_encoder_action,pos);
  }
  //free(data);
  return 0;
}

#ifdef CONTROLLER2
static int e4_encoder_changed(void *data) {
  int pos=(int)data;
  if(active_menu==E4_MENU) {
    encoder_select(pos);
  } else {
    encoder_changed(e4_encoder_action,pos);
  }
  //free(data);
  return 0;
}
#endif

static gpointer rotary_encoder_thread(gpointer data) {
    int pos;

    // ignore startup glitches
    sleep(2);

    //g_mutex_lock(&m_running);
    running=1;
    //g_mutex_unlock(&m_running);
    while(1) {

        pos=vfo_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(vfo_encoder_changed,(gpointer)pos);
        }

        pos=e1_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e1_encoder_changed,(gpointer)pos);
        }

        pos=e2_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e2_encoder_changed,(gpointer)pos);
        }

        pos=e3_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e3_encoder_changed,(gpointer)pos);
        }

#ifdef CONTROLLER2
        pos=e4_encoder_get_pos();
        if(pos!=0) {
            g_idle_add(e4_encoder_changed,(gpointer)pos);
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
//fprintf(stderr,"gpio_thread: lock\n");
        //g_mutex_lock(&m_running);
        if(running==0) {
fprintf(stderr,"gpio_thread: unlock (running==0)\n");
          //g_mutex_unlock(&m_running);
          g_thread_exit(NULL);
        }
        usleep(100000);

//fprintf(stderr,"gpio_thread: unlock (running==1)\n");
        //g_mutex_unlock(&m_running);
    }
    return NULL;
}
