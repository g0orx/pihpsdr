/* Copyright (C)
* 2020 - John Melton, G0ORX/N6LYT
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

// Rewrite to use gpiod rather than wiringPi
// Note that all pin numbers are now the Broadcom GPIO


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

#ifdef GPIO
#include <gpiod.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/ioctl.h>
#endif

#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "toolbar.h"
#include "radio.h"
#include "toolbar.h"
#include "main.h"
#include "property.h"
#include "vfo.h"
#include "wdsp.h"
#include "new_menu.h"
#include "encoder_menu.h"
#include "diversity_menu.h"
#include "actions.h"
#include "gpio.h"
#include "i2c.h"
#include "ext.h"
#include "sliders.h"
#include "new_protocol.h"
#include "zoompan.h"
#ifdef LOCALCW
#include "iambic.h"

//
// Broadcom pins #9, 10, 11 are not used
// by Controller1 and Controller2_V1
// (and keep #2,3 reserved for I2C extensions)
//
int CWL_BUTTON=9;
int CWR_BUTTON=11;
int SIDETONE_GPIO=10;
int ENABLE_GPIO_SIDETONE=0;
int ENABLE_CW_BUTTONS=1;
int CW_ACTIVE_LOW=1;
#endif

#ifdef PTT
int ENABLE_PTT_GPIO=1;
int PTT_GPIO=14;
int PTT_ACTIVE_LOW=1;
#endif

enum {
  TOP_ENCODER,
  BOTTOM_ENCODER
};

enum {
  A,
  B
};

// encoder state table
#define R_START 0x0
#define R_CW_FINAL 0x1
#define R_CW_BEGIN 0x2
#define R_CW_NEXT 0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT 0x6

#define DIR_NONE 0x0
// Clockwise step.
#define DIR_CW 0x10
// Anti-clockwise step.
#define DIR_CCW 0x20

guchar encoder_state_table[7][4] = {
  // R_START
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
  // R_CW_FINAL
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
  // R_CW_BEGIN
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
  // R_CW_NEXT
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
  // R_CCW_BEGIN
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
  // R_CCW_FINAL
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
  // R_CCW_NEXT
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};

#ifdef GPIO
char *consumer="pihpsdr";

char *gpio_device="/dev/gpiochip0";

static struct gpiod_chip *chip=NULL;
#endif

static GMutex encoder_mutex;
static GThread *monitor_thread_id;

int I2C_INTERRUPT=15;

#define MAX_LINES 32
int monitor_lines[MAX_LINES];
int lines=0;

long settle_time=50;  // ms

// VFO Encoder is always last

ENCODER encoders_no_controller[MAX_ENCODERS]={
  {FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  {FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  {FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  {FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  {FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  };

ENCODER encoders_controller1[MAX_ENCODERS]={
  {TRUE,TRUE,20,1,26,1,0,ENCODER_AF_GAIN,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,TRUE,TRUE,25,MENU_BAND,0L},
  {TRUE,TRUE,16,1,19,1,0,ENCODER_AGC_GAIN,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,TRUE,TRUE,8,MENU_BANDSTACK,0L},
  {TRUE,TRUE,4,1,21,1,0,ENCODER_DRIVE,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,TRUE,TRUE,7,MENU_MODE,0L},
  {TRUE,TRUE,18,1,17,1,0,ENCODER_VFO,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  {FALSE,TRUE,0,1,0,0,1,0,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  };

ENCODER encoders_controller2_v1[MAX_ENCODERS]={
  {TRUE,TRUE,20,1,26,1,0,ENCODER_AF_GAIN,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,TRUE,TRUE,22,MENU_BAND,0L},
  {TRUE,TRUE,4,1,21,1,0,ENCODER_AGC_GAIN,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,TRUE,TRUE,27,MENU_BANDSTACK,0L},
  {TRUE,TRUE,16,1,19,1,0,ENCODER_IF_WIDTH,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,TRUE,TRUE,23,MENU_MODE,0L},
  {TRUE,TRUE,25,1,8,1,0,ENCODER_RIT,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,TRUE,TRUE,24,MENU_FREQUENCY,0L},
  {TRUE,TRUE,18,1,17,1,0,ENCODER_VFO,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  };

ENCODER encoders_controller2_v2[MAX_ENCODERS]={
  {TRUE,TRUE,5,1,6,1,0,ENCODER_RF_GAIN,R_START,TRUE,TRUE,26,1,20,1,0,ENCODER_AF_GAIN,R_START,TRUE,TRUE,22,MENU_BAND,0L},
  {TRUE,TRUE,9,1,7,1,0,ENCODER_ATTENUATION,R_START,TRUE,TRUE,21,1,4,1,0,ENCODER_AGC_GAIN,R_START,TRUE,TRUE,27,MENU_MODE,0L},
  {TRUE,TRUE,11,1,10,1,0,ENCODER_IF_WIDTH,R_START,TRUE,TRUE,19,1,16,1,0,ENCODER_IF_SHIFT,R_START,TRUE,TRUE,23,MENU_FILTER,0L},
  {TRUE,TRUE,13,1,12,1,0,ENCODER_XIT,R_START,TRUE,TRUE,8,1,25,1,0,ENCODER_RIT,R_START,TRUE,TRUE,24,MENU_FREQUENCY,0L},
  {TRUE,TRUE,18,1,17,1,0,ENCODER_VFO,R_START,FALSE,TRUE,0,0,0,0,0,0,R_START,FALSE,TRUE,0,0,0L},
  };

ENCODER *encoders=encoders_no_controller;

//
// in switches_cw,only the switch address will be used
//
SWITCH switches_cw[3]={
  {FALSE,FALSE, 9,NO_ACTION,0L},
  {FALSE,FALSE,11,NO_ACTION,0L},
  {FALSE,FALSE,10,NO_ACTION,0L}
};

SWITCH switches_no_controller[MAX_SWITCHES]={
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L},
  {FALSE,FALSE,0,NO_ACTION,0L}
  };

SWITCH switches_controller1[MAX_FUNCTIONS][MAX_SWITCHES]={
  {{TRUE,TRUE,27,MOX,0L},
   {TRUE,TRUE,13,MENU_BAND,0L},
   {TRUE,TRUE,12,MENU_BANDSTACK,0L},
   {TRUE,TRUE,6,MENU_MODE,0L},
   {TRUE,TRUE,5,MENU_FILTER,0L},
   {TRUE,TRUE,24,NR,0L},
   {TRUE,TRUE,23,AGC,0L},
   {TRUE,TRUE,22,FUNCTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L}},
  {{TRUE,TRUE,27,MOX,0L},
   {TRUE,TRUE,13,LOCK,0L},
   {TRUE,TRUE,12,CTUN,0L},
   {TRUE,TRUE,6,A_TO_B,0L},
   {TRUE,TRUE,5,B_TO_A,0L},
   {TRUE,TRUE,24,A_SWAP_B,0L},
   {TRUE,TRUE,23,SPLIT,0L},
   {TRUE,TRUE,22,FUNCTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L}},
  {{TRUE,TRUE,27,MOX,0L},
   {TRUE,TRUE,13,MENU_FREQUENCY,0L},
   {TRUE,TRUE,12,MENU_MEMORY,0L},
   {TRUE,TRUE,6,RIT,0L},
   {TRUE,TRUE,5,RIT_PLUS,0L},
   {TRUE,TRUE,24,RIT_MINUS,0L},
   {TRUE,TRUE,23,RIT_CLEAR,0L},
   {TRUE,TRUE,22,FUNCTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L}},
  {{TRUE,TRUE,27,MOX,0L},
   {TRUE,TRUE,13,MENU_FREQUENCY,0L},
   {TRUE,TRUE,12,MENU_MEMORY,0L},
   {TRUE,TRUE,6,XIT,0L},
   {TRUE,TRUE,5,XIT_PLUS,0L},
   {TRUE,TRUE,24,XIT_MINUS,0L},
   {TRUE,TRUE,23,XIT_CLEAR,0L},
   {TRUE,TRUE,22,FUNCTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L}},
  {{TRUE,TRUE,27,MOX,0L},
   {TRUE,TRUE,13,MENU_FREQUENCY,0L},
   {TRUE,TRUE,12,SPLIT,0L},
   {TRUE,TRUE,6,DUPLEX,0L},
   {TRUE,TRUE,5,SAT,0L},
   {TRUE,TRUE,24,RSAT,0L},
   {TRUE,TRUE,23,NO_ACTION,0L},
   {TRUE,TRUE,22,FUNCTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L}},
  {{TRUE,TRUE,27,MOX,0L},
   {TRUE,TRUE,13,TUNE,0L},
   {TRUE,TRUE,12,TUNE_FULL,0L},
   {TRUE,TRUE,6,TUNE_MEMORY,0L},
   {TRUE,TRUE,5,MENU_BAND,0L},
   {TRUE,TRUE,24,MENU_MODE,0L},
   {TRUE,TRUE,23,MENU_FILTER,0L},
   {TRUE,TRUE,22,FUNCTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L},
   {FALSE,FALSE,0,NO_ACTION,0L}},

  };

SWITCH switches_controller2_v1[MAX_SWITCHES]={
  {FALSE,FALSE,0,MOX,0L},
  {FALSE,FALSE,0,TUNE,0L},
  {FALSE,FALSE,0,PS,0L},
  {FALSE,FALSE,0,TWO_TONE,0L},
  {FALSE,FALSE,0,NR,0L},
  {FALSE,FALSE,0,A_TO_B,0L},
  {FALSE,FALSE,0,B_TO_A,0L},
  {FALSE,FALSE,0,MODE_MINUS,0L},
  {FALSE,FALSE,0,BAND_MINUS,0L},
  {FALSE,FALSE,0,MODE_PLUS,0L},
  {FALSE,FALSE,0,BAND_PLUS,0L},
  {FALSE,FALSE,0,XIT,0L},
  {FALSE,FALSE,0,NB,0L},
  {FALSE,FALSE,0,SNB,0L},
  {FALSE,FALSE,0,LOCK,0L},
  {FALSE,FALSE,0,CTUN,0L}
  };

SWITCH switches_controller2_v2[MAX_SWITCHES]={
  {FALSE,FALSE,0,MOX,0L},
  {FALSE,FALSE,0,TUNE,0L},
  {FALSE,FALSE,0,PS,0L},
  {FALSE,FALSE,0,TWO_TONE,0L},
  {FALSE,FALSE,0,NR,0L},
  {FALSE,FALSE,0,A_TO_B,0L},
  {FALSE,FALSE,0,B_TO_A,0L},
  {FALSE,FALSE,0,MODE_MINUS,0L},
  {FALSE,FALSE,0,BAND_MINUS,0L},
  {FALSE,FALSE,0,MODE_PLUS,0L},
  {FALSE,FALSE,0,BAND_PLUS,0L},
  {FALSE,FALSE,0,XIT,0L},
  {FALSE,FALSE,0,NB,0L},
  {FALSE,FALSE,0,SNB,0L},
  {FALSE,FALSE,0,LOCK,0L},
  {FALSE,FALSE,0,CTUN,0L}
  };

SWITCH *switches=switches_controller1[0];

static int running=0;

static GThread *rotary_encoder_thread_id;

static uint64_t epochMilli;

static void initialiseEpoch() {
  struct timespec ts ;

  clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
  epochMilli = (uint64_t)ts.tv_sec * (uint64_t)1000    + (uint64_t)(ts.tv_nsec / 1000000L) ;
}

static unsigned int millis () {
  uint64_t now ;
  struct  timespec ts ;
  clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
  now  = (uint64_t)ts.tv_sec * (uint64_t)1000 + (uint64_t)(ts.tv_nsec / 1000000L) ;
  return (uint32_t)(now - epochMilli) ;
}

static gpointer rotary_encoder_thread(gpointer data) {
  ENCODER_ACTION *a;
  int i;

  usleep(250000);
  g_print("%s\n",__FUNCTION__);
  while(TRUE) {
    g_mutex_lock(&encoder_mutex);
    for(i=0;i<MAX_ENCODERS;i++) {
      if(encoders[i].bottom_encoder_enabled && encoders[i].bottom_encoder_pos!=0) {
        //g_print("%s: BOTTOM encoder %d pos=%d\n",__FUNCTION__,i,encoders[i].bottom_encoder_pos);
        a=g_new(ENCODER_ACTION,1);
        a->action=encoders[i].bottom_encoder_function;
        a->mode=RELATIVE;
        a->val=encoders[i].bottom_encoder_pos;
        g_idle_add(encoder_action,a);
        encoders[i].bottom_encoder_pos=0;
      }
      if(encoders[i].top_encoder_enabled && encoders[i].top_encoder_pos!=0) {
        //g_print("%s: TOP encoder %d pos=%d\n",__FUNCTION__,i,encoders[i].top_encoder_pos);
        a=g_new(ENCODER_ACTION,1);
        a->action=encoders[i].top_encoder_function;
        a->mode=RELATIVE;
        a->val=encoders[i].top_encoder_pos;
        g_idle_add(encoder_action,a);
        encoders[i].top_encoder_pos=0;
      }
    }
    g_mutex_unlock(&encoder_mutex);
    usleep(100000); // sleep for 100ms
  }
}

int process_function_switch(void *data) {
  function++;
  if(function>=MAX_FUNCTIONS) {
    function=0;
  }
  switches=switches_controller1[function];
  update_toolbar_labels();
  return 0;
}

#ifdef GPIO
static unsigned long switch_debounce;

static void process_encoder(int e,int l,int addr,int val) {
  guchar pinstate;
  //g_print("%s: encoder=%d level=%d addr=0x%02X val=%d\n",__FUNCTION__,e,l,addr,val);
  g_mutex_lock(&encoder_mutex);
  switch(l) {
    case BOTTOM_ENCODER:
      switch(addr) {
        case A:
          encoders[e].bottom_encoder_a_value=val;
          pinstate=(encoders[e].bottom_encoder_b_value<<1) | encoders[e].bottom_encoder_a_value;
          encoders[e].bottom_encoder_state=encoder_state_table[encoders[e].bottom_encoder_state&0xf][pinstate];
          //g_print("%s: state=%02X\n",__FUNCTION__,encoders[e].bottom_encoder_state);
          switch(encoders[e].bottom_encoder_state&0x30) {
            case DIR_NONE:
              break;
            case DIR_CW:
              encoders[e].bottom_encoder_pos++;
              break;
            case DIR_CCW:
              encoders[e].bottom_encoder_pos--;
              break;
            default:
              break;
          }

          //g_print("%s: %s BOTTOM pos=%d\n",__FUNCTION__,encoder_string[encoders[e].bottom_encoder_function],encoders[e].bottom_encoder_pos);
          break;
        case B:
          encoders[e].bottom_encoder_b_value=val;
          pinstate=(encoders[e].bottom_encoder_b_value<<1) | encoders[e].bottom_encoder_a_value;
          encoders[e].bottom_encoder_state=encoder_state_table[encoders[e].bottom_encoder_state&0xf][pinstate];
          //g_print("%s: state=%02X\n",__FUNCTION__,encoders[e].bottom_encoder_state);
          switch(encoders[e].bottom_encoder_state&0x30) {
            case DIR_NONE:
              break;
            case DIR_CW:
              encoders[e].bottom_encoder_pos++;
              break;
            case DIR_CCW:
              encoders[e].bottom_encoder_pos--;
              break;
            default:
              break;
          }

          //g_print("%s: %s BOTTOM pos=%d\n",__FUNCTION__,encoder_string[encoders[e].bottom_encoder_function],encoders[e].bottom_encoder_pos);

          break;
      }
      break;
    case TOP_ENCODER:
      switch(addr) {
        case A:
          encoders[e].top_encoder_a_value=val;
          pinstate=(encoders[e].top_encoder_b_value<<1) | encoders[e].top_encoder_a_value;
          encoders[e].top_encoder_state=encoder_state_table[encoders[e].top_encoder_state&0xf][pinstate];
          //g_print("%s: state=%02X\n",__FUNCTION__,encoders[e].top_encoder_state);
          switch(encoders[e].top_encoder_state&0x30) {
            case DIR_NONE:
              break;
            case DIR_CW:
              encoders[e].top_encoder_pos++;
              break;
            case DIR_CCW:
              encoders[e].top_encoder_pos--;
              break;
            default:
              break;
          }
          //g_print("%s: %s TOP pos=%d\n",__FUNCTION__,encoder_string[encoders[e].top_encoder_function],encoders[e].top_encoder_pos);
          break;
        case B:
          encoders[e].top_encoder_b_value=val;
          pinstate=(encoders[e].top_encoder_b_value<<1) | encoders[e].top_encoder_a_value;
          encoders[e].top_encoder_state=encoder_state_table[encoders[e].top_encoder_state&0xf][pinstate];
          //g_print("%s: state=%02X\n",__FUNCTION__,encoders[e].top_encoder_state);
          switch(encoders[e].top_encoder_state&0x30) {
            case DIR_NONE:
              break;
            case DIR_CW:
              encoders[e].top_encoder_pos++;
              break;
            case DIR_CCW:
              encoders[e].top_encoder_pos--;
              break;
            default:
              break;
          }
          //g_print("%s: %s TOP pos=%d\n",__FUNCTION__,encoder_string[encoders[e].top_encoder_function],encoders[e].top_encoder_pos);

          break;
      }
      break;
  }
  g_mutex_unlock(&encoder_mutex);
}

static void process_edge(int offset,int value) {
  gint i;
  gint t;
  gboolean found;

  //g_print("%s: offset=%d value=%d\n",__FUNCTION__,offset,value);
  found=FALSE;
#ifdef LOCALCW
  if(ENABLE_CW_BUTTONS) {
    if(offset==CWL_BUTTON) {
      keyer_event(1, CW_ACTIVE_LOW ? (value==PRESSED) : value);
      found=TRUE;
    } else if(offset==CWR_BUTTON) {
      keyer_event(2, CW_ACTIVE_LOW ? (value==PRESSED) : value);
      found=TRUE;
    }
  }
  if(found) return;
#endif
  // check encoders
  for(i=0;i<MAX_ENCODERS;i++) {
    if(encoders[i].bottom_encoder_enabled && encoders[i].bottom_encoder_address_a==offset) {
      //g_print("%s: found %d encoder %d bottom A\n",__FUNCTION__,offset,i);
      process_encoder(i,BOTTOM_ENCODER,A,value);
      found=TRUE;
      break;
    } else if(encoders[i].bottom_encoder_enabled && encoders[i].bottom_encoder_address_b==offset) {
      //g_print("%s: found %d encoder %d bottom B\n",__FUNCTION__,offset,i);
      process_encoder(i,BOTTOM_ENCODER,B,value);
      found=TRUE;
      break;
    } else if(encoders[i].top_encoder_enabled && encoders[i].top_encoder_address_a==offset) {
      //g_print("%s: found %d encoder %d top A\n",__FUNCTION__,offset,i);
      process_encoder(i,TOP_ENCODER,A,value);
      found=TRUE;
      break;
    } else if(encoders[i].top_encoder_enabled && encoders[i].top_encoder_address_b==offset) {
      //g_print("%s: found %d encoder %d top B\n",__FUNCTION__,offset,i);
      process_encoder(i,TOP_ENCODER,B,value);
      found=TRUE;
      break;
    } else if(encoders[i].switch_enabled && encoders[i].switch_address==offset) {
      //g_print("%s: found %d encoder %d switch\n",__FUNCTION__,offset,i);
      SWITCH_ACTION *a=g_new(SWITCH_ACTION,1);
      a->action=encoders[i].switch_function;
      a->state=value;
      g_idle_add(switch_action,a);
      found=TRUE;
      break;
    }
  }

  if(controller==CONTROLLER2_V1 || controller==CONTROLLER2_V2) {
    if(I2C_INTERRUPT==offset) {
      if(value==PRESSED) {
        i2c_interrupt();
      }
      found=TRUE;
    }
  }

  if(!found) {
    for(i=0;i<MAX_SWITCHES;i++) {
      if(switches[i].switch_enabled && switches[i].switch_address==offset) {
        t=millis();
        //g_print("%s: found %d switch %d value=%d t=%d\n",__FUNCTION__,offset,i,value,t);
        found=TRUE;
        if(t<switches[i].switch_debounce) {
          return;
        }
g_print("%s: switches=%p function=%d (%s)\n",__FUNCTION__,switches,switches[i].switch_function,sw_string[switches[i].switch_function]);
        switches[i].switch_debounce=t+settle_time;
        SWITCH_ACTION *a=g_new(SWITCH_ACTION,1);
        a->action=switches[i].switch_function;
        a->state=value;
        g_idle_add(switch_action,a);
        break;
      }
    }
  }


  if(!found) {
    g_print("%s: could not find %d\n",__FUNCTION__,offset);
  }
}

static int interrupt_cb(int event_type, unsigned int line, const struct timespec *timeout, void* data) {
  //g_print("%s: event=%d line=%d\n",__FUNCTION__,event_type,line);
  switch(event_type) {
    case GPIOD_CTXLESS_EVENT_CB_TIMEOUT:
      // timeout - ignore
      //g_print("%s: Ignore timeout\n",__FUNCTION__);
      break;
    case GPIOD_CTXLESS_EVENT_CB_RISING_EDGE:
      //g_print("%s: Ignore RISING EDGE\n",__FUNCTION__);
      process_edge(line,RELEASED);
      break;
    case GPIOD_CTXLESS_EVENT_CB_FALLING_EDGE:
      //g_print("%s: Process FALLING EDGE\n",__FUNCTION__);
      process_edge(line,PRESSED);
      break;
  }
  return GPIOD_CTXLESS_EVENT_CB_RET_OK;
}
#endif

void gpio_set_defaults(int ctrlr) {
  int i;
  g_print("%s: %d\n",__FUNCTION__,ctrlr);
  switch(ctrlr) {
    case NO_CONTROLLER:
      encoders=encoders_no_controller;
      switches=switches_controller1[0];
      break;
    case CONTROLLER1:
      encoders=encoders_controller1;
      switches=switches_controller1[0];
      break;
    case CONTROLLER2_V1:
      encoders=encoders_controller2_v1;
      switches=switches_controller2_v1;
      break;
    case CONTROLLER2_V2:
      encoders=encoders_controller2_v2;
      switches=switches_controller2_v2;
      break;
  }
}

void gpio_restore_state() {
  char* value;
  char name[80];

  loadProperties("gpio.props");
 
  controller=NO_CONTROLLER;
  value=getProperty("controller");
  if(value) controller=atoi(value);
  gpio_set_defaults(controller);

  for(int i=0;i<MAX_ENCODERS;i++) {
    sprintf(name,"encoders[%d].bottom_encoder_enabled",i);
    value=getProperty(name);
    if(value) encoders[i].bottom_encoder_enabled=atoi(value);
    sprintf(name,"encoders[%d].bottom_encoder_pullup",i);
    value=getProperty(name);
    if(value) encoders[i].bottom_encoder_pullup=atoi(value);
    sprintf(name,"encoders[%d].bottom_encoder_address_a",i);
    value=getProperty(name);
    if(value) encoders[i].bottom_encoder_address_a=atoi(value);
    sprintf(name,"encoders[%d].bottom_encoder_address_b",i);
    value=getProperty(name);
    if(value) encoders[i].bottom_encoder_address_b=atoi(value);
    sprintf(name,"encoders[%d].bottom_encoder_address_b",i);
    value=getProperty(name);
    sprintf(name,"encoders[%d].top_encoder_enabled",i);
    value=getProperty(name);
    if(value) encoders[i].top_encoder_enabled=atoi(value);
    sprintf(name,"encoders[%d].top_encoder_pullup",i);
    value=getProperty(name);
    if(value) encoders[i].top_encoder_pullup=atoi(value);
    sprintf(name,"encoders[%d].top_encoder_address_a",i);
    value=getProperty(name);
    if(value) encoders[i].top_encoder_address_a=atoi(value);
    sprintf(name,"encoders[%d].top_encoder_address_b",i);
    value=getProperty(name);
    if(value) encoders[i].top_encoder_address_b=atoi(value);
    sprintf(name,"encoders[%d].switch_enabled",i);
    value=getProperty(name);
    if(value) encoders[i].switch_enabled=atoi(value);
    sprintf(name,"encoders[%d].switch_pullup",i);
    value=getProperty(name);
    if(value) encoders[i].switch_pullup=atoi(value);
    sprintf(name,"encoders[%d].switch_address",i);
    value=getProperty(name);
    if(value) encoders[i].switch_address=atoi(value);
  }

  for(int f=0;f<MAX_FUNCTIONS;f++) {
    for(int i=0;i<MAX_SWITCHES;i++) {
      sprintf(name,"switches[%d,%d].switch_enabled",f,i);
      value=getProperty(name);
      if(value) switches_controller1[f][i].switch_enabled=atoi(value);
      sprintf(name,"switches[%d,%d].switch_pullup",f,i);
      value=getProperty(name);
      if(value) switches_controller1[f][i].switch_pullup=atoi(value);
      sprintf(name,"switches[%d,%d].switch_address",f,i);
      value=getProperty(name);
      if(value) switches_controller1[f][i].switch_address=atoi(value);
    }
  }

  if(controller!=CONTROLLER1) {
    for(int i=0;i<MAX_SWITCHES;i++) {
      sprintf(name,"switches[%d].switch_enabled",i);
      value=getProperty(name);
      if(value) switches[i].switch_enabled=atoi(value);
      sprintf(name,"switches[%d].switch_pullup",i);
      value=getProperty(name);
      if(value) switches[i].switch_pullup=atoi(value);
      sprintf(name,"switches[%d].switch_address",i);
      value=getProperty(name);
      if(value) switches[i].switch_address=atoi(value);
    }
  }

}

void gpio_save_state() {
  char value[80];
  char name[80];

  clearProperties();
  sprintf(value,"%d",controller);
  setProperty("controller",value);

  for(int i=0;i<MAX_ENCODERS;i++) {
    sprintf(name,"encoders[%d].bottom_encoder_enabled",i);
    sprintf(value,"%d",encoders[i].bottom_encoder_enabled);
    setProperty(name,value);
    sprintf(name,"encoders[%d].bottom_encoder_pullup",i);
    sprintf(value,"%d",encoders[i].bottom_encoder_pullup);
    setProperty(name,value);
    sprintf(name,"encoders[%d].bottom_encoder_address_a",i);
    sprintf(value,"%d",encoders[i].bottom_encoder_address_a);
    setProperty(name,value);
    sprintf(name,"encoders[%d].bottom_encoder_address_b",i);
    sprintf(value,"%d",encoders[i].bottom_encoder_address_b);
    setProperty(name,value);
    sprintf(name,"encoders[%d].bottom_encoder_address_b",i);
    sprintf(value,"%d",encoders[i].bottom_encoder_address_b);
    setProperty(name,value);
    sprintf(name,"encoders[%d].top_encoder_enabled",i);
    sprintf(value,"%d",encoders[i].top_encoder_enabled);
    setProperty(name,value);
    sprintf(name,"encoders[%d].top_encoder_pullup",i);
    sprintf(value,"%d",encoders[i].top_encoder_pullup);
    setProperty(name,value);
    sprintf(name,"encoders[%d].top_encoder_address_a",i);
    sprintf(value,"%d",encoders[i].top_encoder_address_a);
    setProperty(name,value);
    sprintf(name,"encoders[%d].top_encoder_address_b",i);
    sprintf(value,"%d",encoders[i].top_encoder_address_b);
    setProperty(name,value);
    sprintf(name,"encoders[%d].top_encoder_address_b",i);
    sprintf(value,"%d",encoders[i].top_encoder_address_b);
    setProperty(name,value);
    sprintf(name,"encoders[%d].switch_enabled",i);
    sprintf(value,"%d",encoders[i].switch_enabled);
    setProperty(name,value);
    sprintf(name,"encoders[%d].switch_pullup",i);
    sprintf(value,"%d",encoders[i].switch_pullup);
    setProperty(name,value);
    sprintf(name,"encoders[%d].switch_address",i);
    sprintf(value,"%d",encoders[i].switch_address);
    setProperty(name,value);
  }

  for(int f=0;f<MAX_FUNCTIONS;f++) {
    for(int i=0;i<MAX_SWITCHES;i++) {
      sprintf(name,"switches[%d,%d].switch_enabled",f,i);
      sprintf(value,"%d",switches_controller1[f][i].switch_enabled);
      setProperty(name,value);
      sprintf(name,"switches[%d,%d].switch_pullup",f,i);
      sprintf(value,"%d",switches_controller1[f][i].switch_pullup);
      setProperty(name,value);
      sprintf(name,"switches[%d,%d].switch_address",f,i);
      sprintf(value,"%d",switches_controller1[f][i].switch_address);
      setProperty(name,value);
    }
  }

  if(controller==CONTROLLER2_V1 || controller==CONTROLLER2_V2) {
    for(int i=0;i<MAX_SWITCHES;i++) {
      sprintf(name,"switches[%d].switch_enabled",i);
      sprintf(value,"%d",switches[i].switch_enabled);
      setProperty(name,value);
      sprintf(name,"switches[%d].switch_pullup",i);
      sprintf(value,"%d",switches[i].switch_pullup);
      setProperty(name,value);
      sprintf(name,"switches[%d].switch_address",i);
      sprintf(value,"%d",switches[i].switch_address);
      setProperty(name,value);
    }
  }

  saveProperties("gpio.props");
}

void gpio_restore_actions() {
  char name[80];
  char *value;
  if(controller!=NO_CONTROLLER) {
    for(int i=0;i<MAX_ENCODERS;i++) {
      sprintf(name,"encoders[%d].bottom_encoder_function",i);
      value=getProperty(name);
      if(value) encoders[i].bottom_encoder_function=atoi(value);
      sprintf(name,"encoders[%d].top_encoder_function",i);
      value=getProperty(name);
      if(value) encoders[i].top_encoder_function=atoi(value);
      sprintf(name,"encoders[%d].switch_function",i);
      value=getProperty(name);
      if(value) encoders[i].switch_function=atoi(value);
    }
  }

  for(int f=0;f<MAX_FUNCTIONS;f++) {
    for(int i=0;i<MAX_SWITCHES;i++) {
      sprintf(name,"switches[%d,%d].switch_function",f,i);
      value=getProperty(name);
      if(value) switches_controller1[f][i].switch_function=atoi(value);
    }
  }
  if(controller==CONTROLLER2_V1 || controller==CONTROLLER2_V2) {
    for(int i=0;i<MAX_SWITCHES;i++) {
      sprintf(name,"switches[%d].switch_function",i);
      value=getProperty(name);
      if(value) switches[i].switch_function=atoi(value);
    }
  }
}

void gpio_save_actions() {
  char value[80];
  char name[80];
  if(controller!=NO_CONTROLLER) {
    for(int i=0;i<MAX_ENCODERS;i++) {
      sprintf(name,"encoders[%d].bottom_encoder_function",i);
      sprintf(value,"%d",encoders[i].bottom_encoder_function);
      setProperty(name,value);
      sprintf(name,"encoders[%d].top_encoder_function",i);
      sprintf(value,"%d",encoders[i].top_encoder_function);
      setProperty(name,value);
      sprintf(name,"encoders[%d].switch_function",i);
      sprintf(value,"%d",encoders[i].switch_function);
      setProperty(name,value);
    }
  }

  for(int f=0;f<MAX_FUNCTIONS;f++) {
    for(int i=0;i<MAX_SWITCHES;i++) {
      sprintf(name,"switches[%d,%d].switch_function",f,i);
      sprintf(value,"%d",switches_controller1[f][i].switch_function);
      setProperty(name,value);
    }
  }
  if(controller==CONTROLLER2_V1 || controller==CONTROLLER2_V2) {
    for(int i=0;i<MAX_SWITCHES;i++) {
      sprintf(name,"switches[%d].switch_function",i);
      sprintf(value,"%d",switches[i].switch_function);
      setProperty(name,value);
    }
  }
}

#ifdef GPIO
static gpointer monitor_thread(gpointer arg) {
  struct timespec t;

  // thread to monitor gpio events
  g_print("%s: start event monitor lines=%d\n",__FUNCTION__,lines);
  g_print("%s:",__FUNCTION__);
  for(int i=0;i<lines;i++) {
    g_print(" %d",monitor_lines[i]);
  }
  g_print("\n");
  t.tv_sec=60;
  t.tv_nsec=0;

  int ret=gpiod_ctxless_event_monitor_multiple(
			gpio_device, GPIOD_CTXLESS_EVENT_BOTH_EDGES,
			monitor_lines, lines, FALSE,
			consumer, &t, NULL, interrupt_cb,NULL);
  if (ret<0) {
    g_print("%s: ctxless event monitor failed: %s\n",__FUNCTION__,g_strerror(errno));
  }

  g_print("%s: exit\n",__FUNCTION__);
  return NULL;
}

static int setup_line(struct gpiod_chip *chip, int offset, gboolean pullup) {
  int ret;
  struct gpiod_line_request_config config;

  g_print("%s: %d\n",__FUNCTION__,offset);
  struct gpiod_line *line=gpiod_chip_get_line(chip, offset);
  if (!line) {
    g_print("%s: get line %d failed: %s\n",__FUNCTION__,offset,g_strerror(errno));
    return -1;
  }

  config.consumer=consumer;
  config.request_type=GPIOD_LINE_REQUEST_DIRECTION_INPUT | GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES;
#ifdef OLD_GPIOD
  config.flags=pullup?GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW:0;
#else
  config.flags=pullup?GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP:GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN;
#endif
  ret=gpiod_line_request(line,&config,1);
  if (ret<0) {
    g_print("%s: line %d gpiod_line_request failed: %s\n",__FUNCTION__,offset,g_strerror(errno));
    return ret;
  }

  gpiod_line_release(line);

  monitor_lines[lines]=offset;
  lines++;
  return 0;
}

static int setup_output_line(struct gpiod_chip *chip, int offset, int _initial_value) {
  int ret;
  struct gpiod_line_request_config config;

  g_print("%s: %d\n",__FUNCTION__,offset);
  struct gpiod_line *line=gpiod_chip_get_line(chip, offset);
  if (!line) {
    g_print("%s: get line %d failed: %s\n",__FUNCTION__,offset,g_strerror(errno));
    return -1;
  }

  config.consumer=consumer;
  config.request_type=GPIOD_LINE_REQUEST_DIRECTION_OUTPUT;
  ret=gpiod_line_request(line,&config,1);
  if (ret<0) {
    g_print("%s: line %d gpiod_line_request failed: %s\n",__FUNCTION__,offset,g_strerror(errno));
    return ret;
  }

  // write initial value
  

  gpiod_line_release(line);

  return 0;
}
#endif

int gpio_init() {
  int ret=0;

#ifdef GPIO
  initialiseEpoch();
  switch_debounce=millis();

  g_mutex_init(&encoder_mutex);

  gpio_set_defaults(controller);

  chip=NULL;

//g_print("%s: open gpio 0\n",__FUNCTION__);
  chip=gpiod_chip_open_by_number(0);
  if(chip==NULL) {
    g_print("%s: open chip failed: %s\n",__FUNCTION__,g_strerror(errno));
    ret=-1;
    goto err;
  }

  // setup encoders
  g_print("%s: setup encoders\n",__FUNCTION__);
  for(int i=0;i<MAX_ENCODERS;i++) {
    if(encoders[i].bottom_encoder_enabled) {
      if(setup_line(chip,encoders[i].bottom_encoder_address_a,encoders[i].bottom_encoder_pullup)<0) {
        continue;
      }
      if(setup_line(chip,encoders[i].bottom_encoder_address_b,encoders[i].bottom_encoder_pullup)<0) {
        continue;
      }
    }

    if(encoders[i].top_encoder_enabled) {
      if(setup_line(chip,encoders[i].top_encoder_address_a,encoders[i].top_encoder_pullup)<0) {
        continue;
      }
      if(setup_line(chip,encoders[i].top_encoder_address_b,encoders[i].top_encoder_pullup)<0) {
        continue;
      }
    }

    if(encoders[i].switch_enabled) {
      if(setup_line(chip,encoders[i].switch_address,encoders[i].switch_pullup)<0) {
        continue;
      }
    }
  }

  // setup switches
  g_print("%s: setup switches\n",__FUNCTION__);
  for(int i=0;i<MAX_SWITCHES;i++) {
    if(switches[i].switch_enabled) {
      if(setup_line(chip,switches[i].switch_address,switches[i].switch_pullup)<0) {
        continue;
      }
    }
  }

  if(controller==CONTROLLER2_V1 || controller==CONTROLLER2_V2) {
    i2c_init();
    g_print("%s: setup i2c interrupt %d\n",__FUNCTION__,I2C_INTERRUPT);
    if((ret=setup_line(chip,I2C_INTERRUPT,TRUE))<0) {
      goto err;
    }
  }

#ifdef LOCALCW
  g_print("%s: ENABLE_CW_BUTTONS=%d  CWL_BUTTON=%d CWR_BUTTON=%d\n", __FUNCTION__, ENABLE_CW_BUTTONS, CWL_BUTTON, CWR_BUTTON);
  if(ENABLE_CW_BUTTONS) {
    if((ret=setup_line(chip,CWL_BUTTON,CW_ACTIVE_LOW==1))<0) {
      goto err;
    }
    monitor_lines[lines]=CWL_BUTTON;
    lines++;
    if((ret=setup_line(chip,CWR_BUTTON,CW_ACTIVE_LOW==1))<0) {
      goto err;
    }
    monitor_lines[lines]=CWR_BUTTON;
    lines++;

  }
  if (ENABLE_GPIO_SIDETONE) {
//
//  use this pin as an output pin and
//  set its value to LOW
//
    if((ret=setup_output_line(chip,SIDETONE_GPIO,0))<0) {
      goto err;
    }
  }
#endif

  if(controller!=NO_CONTROLLER
#ifdef LOCALCW
    || ENABLE_CW_BUTTONS
#endif
    ) {
    monitor_thread_id = g_thread_new( "gpiod monitor", monitor_thread, NULL);
    if(!monitor_thread_id ) {
      g_print("%s: g_thread_new failed for monitor_thread\n",__FUNCTION__);
    }

    if(controller!=NO_CONTROLLER) {
      rotary_encoder_thread_id = g_thread_new( "encoders", rotary_encoder_thread, NULL);
      if(!rotary_encoder_thread_id ) {
        g_print("%s: g_thread_new failed on rotary_encoder_thread\n",__FUNCTION__);
        exit( -1 );
      }
      g_print("%s: rotary_encoder_thread: id=%p\n",__FUNCTION__,rotary_encoder_thread_id);
    }
  }

#endif
  return 0;

err:
g_print("%s: err\n",__FUNCTION__);
#ifdef GPIO
  if(chip!=NULL) {
    gpiod_chip_close(chip);
    chip=NULL;
  }
#endif
  return ret;
}

void gpio_close() {
#ifdef GPIO
  if(chip!=NULL) gpiod_chip_close(chip);
#endif
}

#ifdef LOCALCW
void gpio_cw_sidetone_set(int level) {
  int rc;
#ifdef GPIO
  if (ENABLE_GPIO_SIDETONE) {
    if((rc=gpiod_ctxless_set_value(gpio_device,SIDETONE_GPIO,level,FALSE,consumer,NULL,NULL))<0) {
      g_print("%s: err=%d\n",__FUNCTION__,rc);
    }
  }
#endif
}

int  gpio_cw_sidetone_enabled() {
  return ENABLE_GPIO_SIDETONE;
}

#endif
