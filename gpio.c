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
#ifdef I2C
#include "i2c.h"
#endif

// debounce settle time in us
#define SETTLE_TIME 100000

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

#ifdef I2C
int I2C_INTERRUPT=15;
#endif

/*
// uses broadcom pin numbers -- not working on Tinker board
int ENABLE_VFO_ENCODER=1;
int ENABLE_VFO_PULLUP=1;
int VFO_ENCODER_A=18;
int VFO_ENCODER_B=17;
#if defined odroid && !defined sx1509
int VFO_ENCODER_A_PIN=0;
int VFO_ENCODER_B_PIN=1;
#endif
int ENABLE_E1_ENCODER=1;
int ENABLE_E1_PULLUP=0;
int E1_ENCODER_A=20;
int E1_ENCODER_B=26;
//#ifndef sx1509
int E1_FUNCTION=25;
//#else
//int E1_FUNCTION=2; //RRK, was 25 now taken by waveshare LCD TS, disable i2c
//#endif
int ENABLE_E2_ENCODER=1;
int ENABLE_E2_PULLUP=0;
int E2_ENCODER_A=16;
int E2_ENCODER_B=19;
int E2_FUNCTION=8;
int ENABLE_E3_ENCODER=1;
int ENABLE_E3_PULLUP=0;
int E3_ENCODER_A=4;
int E3_ENCODER_B=21;
//#if defined sx1509
int E3_FUNCTION=7;
//#else
//int E3_FUNCTION=3; //RRK, was 7 now taken by waveshare LCD TS, disable i2c
//#endif
int ENABLE_S1_BUTTON=1;
int S1_BUTTON=13;
int ENABLE_S2_BUTTON=1;
int S2_BUTTON=12;
int ENABLE_S3_BUTTON=1;
int S3_BUTTON=6;
int ENABLE_S4_BUTTON=1;
int S4_BUTTON=5;
int ENABLE_S5_BUTTON=1;
int S5_BUTTON=24;
int ENABLE_S6_BUTTON=1;
int S6_BUTTON=23;
int ENABLE_MOX_BUTTON=1;
int MOX_BUTTON=27;
int ENABLE_FUNCTION_BUTTON=1;
int FUNCTION_BUTTON=22;
int ENABLE_E1_BUTTON=1;
int ENABLE_E2_BUTTON=1;
int ENABLE_E3_BUTTON=1;
int ENABLE_CW_BUTTONS=1;
// make sure to disable UART0 for next 2 gpios
#ifdef LOCALCW
int CWL_BUTTON=9;
int CWR_BUTTON=10;
#endif
*/

static volatile int vfoEncoderPos;
static volatile int e1EncoderPos;
static volatile int e1Function;
int e1_encoder_action=ENCODER_AF_GAIN;
static volatile int e2EncoderPos;
static volatile int e2Function;
int e2_encoder_action=ENCODER_DRIVE;
static volatile int e3EncoderPos;
static volatile int e3Function;
int e3_encoder_action=ENCODER_ATTENUATION;
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
"PANADAPTER LOW"
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

static int e_function_pressed(void *data) {
  int encoder=(int)data;
  start_encoder(encoder);
  return 0;
}

static void e1FunctionAlert() {
    int level=digitalRead(E1_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)1);
    }
    usleep(SETTLE_TIME);
}

static void e2FunctionAlert() {
    int level=digitalRead(E2_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)2);
    }
    usleep(SETTLE_TIME);
}

static void e3FunctionAlert() {
    int level=digitalRead(E3_FUNCTION);
    if(level==0) {
      if(running) g_idle_add(e_function_pressed,(gpointer)3);
    }
    usleep(SETTLE_TIME);
}

static void functionAlert() {
    int level=digitalRead(FUNCTION_BUTTON);
    if(level==0) {
      if(running) g_idle_add(function_pressed,NULL);
    }
    usleep(SETTLE_TIME);
}

static void s1Alert() {
    int level=digitalRead(S1_BUTTON);
    if(level==0) {
      g_idle_add(s1_pressed,NULL);
    } else {
      g_idle_add(s1_released,NULL);
    }
    usleep(SETTLE_TIME);
}

static void s2Alert() {
    int level=digitalRead(S2_BUTTON);
    if(level==0) {
      g_idle_add(s2_pressed,NULL);
    } else {
      g_idle_add(s2_released,NULL);
    }
    usleep(SETTLE_TIME);
}

static void s3Alert() {
    int level=digitalRead(S3_BUTTON);
    if(level==0) {
      g_idle_add(s3_pressed,NULL);
    } else {
      g_idle_add(s3_released,NULL);
    }
    usleep(SETTLE_TIME);
}

static void s4Alert() {
    int level=digitalRead(S4_BUTTON);
    if(level==0) {
      g_idle_add(s4_pressed,NULL);
    } else {
      g_idle_add(s4_released,NULL);
    }
    usleep(SETTLE_TIME);
}

static void s5Alert() {
    int level=digitalRead(S5_BUTTON);
    if(level==0) {
      g_idle_add(s5_pressed,NULL);
    } else {
      g_idle_add(s5_released,NULL);
    }
    usleep(SETTLE_TIME);
}

static void s6Alert() {
    int level=digitalRead(S6_BUTTON);
    if(level==0) {
      g_idle_add(s6_pressed,NULL);
    } else {
      g_idle_add(s6_released,NULL);
    }
    usleep(SETTLE_TIME);
}

static unsigned long moxdebounce=0;

static void moxAlert() {
    int level1=digitalRead(MOX_BUTTON);
    usleep(20000);
    int level2=digitalRead(MOX_BUTTON);
fprintf(stderr,"moxAlert: level1=%d level2=%d\n",level1,level2);
    if(level1==0 && level2==0) {
      g_idle_add(mox_pressed,(gpointer)NULL);
    }
    usleep(SETTLE_TIME);
}

static void vfoEncoderPulse(int gpio, int level, unsigned int tick) {
   static int levA=0, levB=0, lastGpio = -1;

   if (gpio == VFO_ENCODER_A) levA = level; else levB = level;

   if (gpio != lastGpio) /* debounce */
   {
      lastGpio = gpio;

      if ((gpio == VFO_ENCODER_A) && (level == 0))
      {
         if (!levB) ++vfoEncoderPos;
      }
      else if ((gpio == VFO_ENCODER_B) && (level == 1))
      {
         if (levA) --vfoEncoderPos;
      }
   }
}

static void vfoEncoderA() {
    int level=digitalRead(VFO_ENCODER_A);
    vfoEncoderPulse(VFO_ENCODER_A,level,0);
}

static void vfoEncoderB() {
    int level=digitalRead(VFO_ENCODER_B);
    vfoEncoderPulse(VFO_ENCODER_B,level,0);
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

#ifdef I2C
static void pI2CInterrupt() {
    int level=digitalRead(I2C_INTERRUPT);
    if(level==0) {
      fprintf(stderr,"I2CInterrupt: %d\n",level);
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
  value=getProperty("ENABLE_E1_BUTTON");
  if(value) ENABLE_E1_BUTTON=atoi(value);

  value=getProperty("ENABLE_E2_BUTTON");
  if(value) ENABLE_E2_BUTTON=atoi(value);

  value=getProperty("ENABLE_E3_BUTTON");
  if(value) ENABLE_E3_BUTTON=atoi(value);

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

  sprintf(value,"%d",ENABLE_E1_BUTTON);
  setProperty("ENABLE_E1_BUTTON",value);

  sprintf(value,"%d",ENABLE_E2_BUTTON);
  setProperty("ENABLE_E2_BUTTON",value);

  sprintf(value,"%d",ENABLE_E3_BUTTON);
  setProperty("ENABLE_E3_BUTTON",value);

  saveProperties("gpio.props");
}

static void setup_pin(int pin, int up_down, void(*pAlert)(void)) {

fprintf(stderr,"setup_pin: pin=%d updown=%d\n",pin,up_down);
  pinMode(pin,INPUT);
  pullUpDnControl(pin,up_down);
  usleep(10000);
  wiringPiISR(pin,INT_EDGE_BOTH,pAlert);
}


static void setup_encoder_pin(int pin, int up_down, void(*pAlert)(void)) {
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
    setup_pin(VFO_ENCODER_A,ENABLE_VFO_PULLUP?PUD_UP:PUD_OFF,&vfoEncoderA);
    setup_pin(VFO_ENCODER_B,ENABLE_VFO_PULLUP?PUD_UP:PUD_OFF,&vfoEncoderB);
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

  rotary_encoder_thread_id = g_thread_new( "rotary encoder", rotary_encoder_thread, NULL);
  if( ! rotary_encoder_thread_id )
  {
    fprintf(stderr,"g_thread_new failed on rotary_encoder_thread\n");
    exit( -1 );
  }
  fprintf(stderr, "rotary_encoder_thread: id=%p\n",rotary_encoder_thread_id);

#ifdef I2C
  // setup i2c
  i2c_init();

  // setup interrupt pin
  fprintf(stderr,"setup i2c interrupt: pin=%d\n",I2C_INTERRUPT);
  digitalWrite(I2C_INTERRUPT,0); // clear pin
  pinMode(I2C_INTERRUPT,INPUT);
  pullUpDnControl(I2C_INTERRUPT,PUD_UP);
  usleep(10000);
  wiringPiISR(I2C_INTERRUPT,INT_EDGE_FALLING,pI2CInterrupt);
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
      value=active_receiver->attenuation;
      value+=pos;
      if(value<0) {
        value=0;
      } else if (value>31) {
        value=31;
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
      vfo_update(NULL);
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
      vfo_update(NULL);
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
      vfo_update(NULL);
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
