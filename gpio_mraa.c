#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <pthread.h>
#include <mraa.h>

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
#include "wdsp.h"


int ENABLE_VFO_ENCODER=1;
int ENABLE_VFO_PULLUP=1;
int VFO_ENCODER_A=11;
int VFO_ENCODER_B=12;
int ENABLE_AF_ENCODER=1;
int ENABLE_AF_PULLUP=0;
int AF_ENCODER_A=38;
int AF_ENCODER_B=37;
int ENABLE_RF_ENCODER=1;
int ENABLE_RF_PULLUP=0;
int RF_ENCODER_A=36;
int RF_ENCODER_B=35;
int ENABLE_AGC_ENCODER=1;
int ENABLE_AGC_PULLUP=0;
int AGC_ENCODER_A=7;
int AGC_ENCODER_B=40;
int ENABLE_BAND_BUTTON=1;
int BAND_BUTTON=33;
int ENABLE_BANDSTACK_BUTTON=1;
int BANDSTACK_BUTTON=32;
int ENABLE_MODE_BUTTON=1;
int MODE_BUTTON=31;
int ENABLE_FILTER_BUTTON=1;
int FILTER_BUTTON=29;
int ENABLE_NOISE_BUTTON=1;
int NOISE_BUTTON=18;
int ENABLE_AGC_BUTTON=1;
int AGC_BUTTON=16;
int ENABLE_MOX_BUTTON=1;
int MOX_BUTTON=13;
int ENABLE_FUNCTION_BUTTON=1;
int FUNCTION_BUTTON=15;
int ENABLE_LOCK_BUTTON=1;
int LOCK_BUTTON=22;
int SPARE_RF_BUTTON=24;
int SPARE_AGC_BUTTON=26;

static mraa_gpio_context vfo_a_context;
static mraa_gpio_context vfo_b_context;
static mraa_gpio_context af_a_context;
static mraa_gpio_context af_b_context;
static mraa_gpio_context rf_a_context;
static mraa_gpio_context rf_b_context;
static mraa_gpio_context agc_a_context;
static mraa_gpio_context agc_b_context;
static mraa_gpio_context band_context;
static mraa_gpio_context bandstack_context;
static mraa_gpio_context mode_context;
static mraa_gpio_context filter_context;
static mraa_gpio_context noise_context;
static mraa_gpio_context agc_context;
static mraa_gpio_context mox_context;
static mraa_gpio_context function_context;
static mraa_gpio_context lock_context;
static mraa_gpio_context rf_sw_context;
static mraa_gpio_context agc_sw_context;

static volatile int vfoEncoderPos;
static volatile int afEncoderPos;
static volatile int rfEncoderPos;
static volatile int agcEncoderPos;
static volatile int function_state;
static volatile int band_state;
static volatile int bandstack_state;
static volatile int mode_state;
static volatile int filter_state;
static volatile int noise_state;
static volatile int agc_state;
static volatile int mox_state;
static volatile int lock_state;

static void* rotary_encoder_thread(void *arg);
static pthread_t rotary_encoder_thread_id;
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
static int lock_button=0;
static int previous_lock_button=0;


static void function_interrupt(void *args) {
    function_state=(mraa_gpio_read(function_context)==0);
}

static void band_interrupt(void *args) {
    band_state=(mraa_gpio_read(band_context)==0);
}

static void bandstack_interrupt(void *args) {
    bandstack_state=(mraa_gpio_read(bandstack_context)==0);
}

static void mode_interrupt(void *args) {
    mode_state=(mraa_gpio_read(mode_context)==0);
}

static void filter_interrupt(void *args) {
    filter_state=(mraa_gpio_read(filter_context)==0);
}

static void noise_interrupt(void *args) {
    noise_state=(mraa_gpio_read(noise_context)==0);
}

static void agc_interrupt(void *args) {
    agc_state=(mraa_gpio_read(agc_context)==0);
}

static void mox_interrupt(void *args) {
    mox_state=(mraa_gpio_read(mox_context)==0);
}

static void lock_interrupt(void *args) {
    lock_state=(mraa_gpio_read(lock_context)==0);
}

static void vfo_interrupt(void *args) {
fprintf(stderr,"vfo_interrupt\n");
  if(mraa_gpio_read(vfo_a_context)) {
    if(mraa_gpio_read(vfo_b_context)) {
      ++vfoEncoderPos;
    } else {
      --vfoEncoderPos;
    }
    fprintf(stderr,"vfo pos=%d\n",vfoEncoderPos);
  }
}

static void af_interrupt(void *args) {
fprintf(stderr,"af_interrupt\n");
  if(mraa_gpio_read(af_a_context)) {
    if(mraa_gpio_read(af_b_context)) {
      ++afEncoderPos;
    } else {
      --afEncoderPos;
    }
    fprintf(stderr,"af pos=%d\n",afEncoderPos);
  }
}

static void rf_interrupt(void *args) {
fprintf(stderr,"rf_interrupt\n");
  if(mraa_gpio_read(rf_a_context)) {
    if(mraa_gpio_read(rf_b_context)) {
      ++rfEncoderPos;
    } else {
      --rfEncoderPos;
    }
    fprintf(stderr,"rf pos=%d\n",rfEncoderPos);
  }
}

static void agc_encoder_interrupt(void *args) {
fprintf(stderr,"agc_interrupt\n");
  if(mraa_gpio_read(agc_a_context)) {
    if(mraa_gpio_read(agc_b_context)) {
      ++agcEncoderPos;
    } else {
      --agcEncoderPos;
    }
    fprintf(stderr,"agc pos=%d\n",agcEncoderPos);
  }
}


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
  value=getProperty("ENABLE_AF_ENCODER");
  if(value) ENABLE_AF_ENCODER=atoi(value);
  value=getProperty("ENABLE_AF_PULLUP");
  if(value) ENABLE_AF_PULLUP=atoi(value);
  value=getProperty("AF_ENCODER_A");
  if(value) AF_ENCODER_A=atoi(value);
  value=getProperty("AF_ENCODER_B");
  if(value) AF_ENCODER_B=atoi(value);
  value=getProperty("ENABLE_RF_ENCODER");
  if(value) ENABLE_RF_ENCODER=atoi(value);
  value=getProperty("ENABLE_RF_PULLUP");
  if(value) ENABLE_RF_PULLUP=atoi(value);
  value=getProperty("RF_ENCODER_A");
  if(value) RF_ENCODER_A=atoi(value);
  value=getProperty("RF_ENCODER_B");
  if(value) RF_ENCODER_B=atoi(value);
  value=getProperty("ENABLE_AGC_ENCODER");
  if(value) ENABLE_AGC_ENCODER=atoi(value);
  value=getProperty("ENABLE_AGC_PULLUP");
  if(value) ENABLE_AGC_PULLUP=atoi(value);
  value=getProperty("AGC_ENCODER_A");
  if(value) AGC_ENCODER_A=atoi(value);
  value=getProperty("AGC_ENCODER_B");
  if(value) AGC_ENCODER_B=atoi(value);
  value=getProperty("ENABLE_BAND_BUTTON");
  if(value) ENABLE_BAND_BUTTON=atoi(value);
  value=getProperty("BAND_BUTTON");
  if(value) BAND_BUTTON=atoi(value);
  value=getProperty("ENABLE_BANDSTACK_BUTTON");
  if(value) ENABLE_BANDSTACK_BUTTON=atoi(value);
  value=getProperty("BANDSTACK_BUTTON");
  if(value) BANDSTACK_BUTTON=atoi(value);
  value=getProperty("ENABLE_MODE_BUTTON");
  if(value) ENABLE_MODE_BUTTON=atoi(value);
  value=getProperty("MODE_BUTTON");
  if(value) MODE_BUTTON=atoi(value);
  value=getProperty("ENABLE_FILTER_BUTTON");
  if(value) ENABLE_FILTER_BUTTON=atoi(value);
  value=getProperty("FILTER_BUTTON");
  if(value) FILTER_BUTTON=atoi(value);
  value=getProperty("ENABLE_NOISE_BUTTON");
  if(value) ENABLE_NOISE_BUTTON=atoi(value);
  value=getProperty("NOISE_BUTTON");
  if(value) NOISE_BUTTON=atoi(value);
  value=getProperty("ENABLE_AGC_BUTTON");
  if(value) ENABLE_AGC_BUTTON=atoi(value);
  value=getProperty("AGC_BUTTON");
  if(value) AGC_BUTTON=atoi(value);
  value=getProperty("ENABLE_FUNCTION_BUTTON");
  if(value) ENABLE_FUNCTION_BUTTON=atoi(value);
  value=getProperty("FUNCTION_BUTTON");
  if(value) FUNCTION_BUTTON=atoi(value);
  value=getProperty("ENABLE_MOX_BUTTON");
  if(value) ENABLE_MOX_BUTTON=atoi(value);
  value=getProperty("MOX_BUTTON");
  if(value) MOX_BUTTON=atoi(value);
  value=getProperty("ENABLE_LOCK_BUTTON");
  if(value) ENABLE_LOCK_BUTTON=atoi(value);
  value=getProperty("LOCK_BUTTON");
  if(value) LOCK_BUTTON=atoi(value);
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
  sprintf(value,"%d",ENABLE_AF_ENCODER);
  setProperty("ENABLE_AF_ENCODER",value);
  sprintf(value,"%d",ENABLE_AF_PULLUP);
  setProperty("ENABLE_AF_PULLUP",value);
  sprintf(value,"%d",AF_ENCODER_A);
  setProperty("AF_ENCODER_A",value);
  sprintf(value,"%d",AF_ENCODER_B);
  setProperty("AF_ENCODER_B",value);
  sprintf(value,"%d",ENABLE_RF_ENCODER);
  setProperty("ENABLE_RF_ENCODER",value);
  sprintf(value,"%d",ENABLE_RF_PULLUP);
  setProperty("ENABLE_RF_PULLUP",value);
  sprintf(value,"%d",RF_ENCODER_A);
  setProperty("RF_ENCODER_A",value);
  sprintf(value,"%d",RF_ENCODER_B);
  setProperty("RF_ENCODER_B",value);
  sprintf(value,"%d",ENABLE_AGC_ENCODER);
  setProperty("ENABLE_AGC_ENCODER",value);
  sprintf(value,"%d",ENABLE_AGC_PULLUP);
  setProperty("ENABLE_AGC_PULLUP",value);
  sprintf(value,"%d",AGC_ENCODER_A);
  setProperty("AGC_ENCODER_A",value);
  sprintf(value,"%d",AGC_ENCODER_B);
  setProperty("AGC_ENCODER_B",value);
  sprintf(value,"%d",ENABLE_BAND_BUTTON);
  setProperty("ENABLE_BAND_BUTTON",value);
  sprintf(value,"%d",BAND_BUTTON);
  setProperty("BAND_BUTTON",value);
  sprintf(value,"%d",ENABLE_BANDSTACK_BUTTON);
  setProperty("ENABLE_BANDSTACK_BUTTON",value);
  sprintf(value,"%d",BANDSTACK_BUTTON);
  setProperty("BANDSTACK_BUTTON",value);
  sprintf(value,"%d",ENABLE_MODE_BUTTON);
  setProperty("ENABLE_MODE_BUTTON",value);
  sprintf(value,"%d",MODE_BUTTON);
  setProperty("MODE_BUTTON",value);
  sprintf(value,"%d",ENABLE_FILTER_BUTTON);
  setProperty("ENABLE_FILTER_BUTTON",value);
  sprintf(value,"%d",FILTER_BUTTON);
  setProperty("FILTER_BUTTON",value);
  sprintf(value,"%d",ENABLE_NOISE_BUTTON);
  setProperty("ENABLE_NOISE_BUTTON",value);
  sprintf(value,"%d",NOISE_BUTTON);
  setProperty("NOISE_BUTTON",value);
  sprintf(value,"%d",ENABLE_AGC_BUTTON);
  setProperty("ENABLE_AGC_BUTTON",value);
  sprintf(value,"%d",AGC_BUTTON);
  setProperty("AGC_BUTTON",value);
  sprintf(value,"%d",ENABLE_FUNCTION_BUTTON);
  setProperty("ENABLE_FUNCTION_BUTTON",value);
  sprintf(value,"%d",FUNCTION_BUTTON);
  setProperty("FUNCTION_BUTTON",value);
  sprintf(value,"%d",ENABLE_MOX_BUTTON);
  setProperty("ENABLE_MOX_BUTTON",value);
  sprintf(value,"%d",MOX_BUTTON);
  setProperty("MOX_BUTTON",value);
  sprintf(value,"%d",ENABLE_LOCK_BUTTON);
  setProperty("ENABLE_LOCK_BUTTON",value);
  sprintf(value,"%d",LOCK_BUTTON);
  setProperty("LOCK_BUTTON",value);

  saveProperties("gpio.props");
}


int gpio_init() {
  mraa_result_t res;
fprintf(stderr,"MRAA: gpio_init\n");

  gpio_restore_state();

  mraa_init();

  const char* board_name = mraa_get_platform_name();
  fprintf(stderr,"MRAA: gpio_init: board=%s\n",board_name);

if(ENABLE_VFO_ENCODER) {
  vfo_a_context=mraa_gpio_init(VFO_ENCODER_A);
  if(vfo_a_context!=NULL) {
    res=mraa_gpio_dir(vfo_a_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(VFO_ENCODER_A,MRAA_GPIO_IN) returned %d\n",res);
    }
    if(ENABLE_VFO_PULLUP) {
      res=mraa_gpio_mode(vfo_a_context,MRAA_GPIO_PULLUP);
      if(res!=MRAA_SUCCESS) {
        fprintf(stderr,"mra_gpio_mode(VFO_ENCODER_A,MRAA_GPIO_PULLUP) returned %d\n",res);
      }
    }
    res=mraa_gpio_isr(vfo_a_context,MRAA_GPIO_EDGE_RISING,vfo_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(VFO_ENCODER_A,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",VFO_ENCODER_A);
  }

  vfo_b_context=mraa_gpio_init(VFO_ENCODER_B);
  if(vfo_b_context!=NULL) {
    res=mraa_gpio_dir(vfo_b_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(VFO_ENCODER_B,MRAA_GPIO_IN) returned %d\n",res);
    }
    if(ENABLE_VFO_PULLUP) {
      res=mraa_gpio_mode(vfo_b_context,MRAA_GPIO_PULLUP);
      if(res!=MRAA_SUCCESS) {
        fprintf(stderr,"mra_gpio_mode(VFO_ENCODER_B,MRAA_GPIO_PULLUP) returned %d\n",res);
      }
    }
    res=mraa_gpio_isr(vfo_b_context,MRAA_GPIO_EDGE_RISING,vfo_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(VFO_ENCODER_B,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",VFO_ENCODER_B);
  }
}

if(ENABLE_AF_ENCODER) {
  af_a_context=mraa_gpio_init(AF_ENCODER_A);
  if(af_a_context!=NULL) {
    res=mraa_gpio_dir(af_a_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(AF_ENCODER_A,MRAA_GPIO_IN) returned %d\n",res);
    }
    if(ENABLE_AF_PULLUP) {
      res=mraa_gpio_mode(af_a_context,MRAA_GPIO_PULLUP);
      if(res!=MRAA_SUCCESS) {
        fprintf(stderr,"mra_gpio_mode(AF_ENCODER_A,MRAA_GPIO_PULLUP) returned %d\n",res);
      }
    }
    res=mraa_gpio_isr(af_a_context,MRAA_GPIO_EDGE_RISING,af_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(AF_ENCODER_A,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",AF_ENCODER_A);
  }

  af_b_context=mraa_gpio_init(AF_ENCODER_B);
  if(af_b_context!=NULL) {
    res=mraa_gpio_dir(af_b_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(AF_ENCODER_B,MRAA_GPIO_IN) returned %d\n",res);
    }
    if(ENABLE_AF_PULLUP) {
      res=mraa_gpio_mode(af_b_context,MRAA_GPIO_PULLUP);
      if(res!=MRAA_SUCCESS) {
        fprintf(stderr,"mra_gpio_mode(AF_ENCODER_B,MRAA_GPIO_PULLUP) returned %d\n",res);
      }
    }
    res=mraa_gpio_isr(af_b_context,MRAA_GPIO_EDGE_RISING,af_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(AF_ENCODER_B,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",AF_ENCODER_B);
  }
}

if(ENABLE_RF_ENCODER) {
  rf_a_context=mraa_gpio_init(RF_ENCODER_A);
  if(rf_a_context!=NULL) {
    res=mraa_gpio_dir(rf_a_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(RF_ENCODER_A,MRAA_GPIO_IN) returned %d\n",res);
    }
    if(ENABLE_RF_PULLUP) {
      res=mraa_gpio_mode(rf_a_context,MRAA_GPIO_PULLUP);
      if(res!=MRAA_SUCCESS) {
        fprintf(stderr,"mra_gpio_mode(RF_ENCODER_A,MRAA_GPIO_PULLUP) returned %d\n",res);
      }
    }
    res=mraa_gpio_isr(rf_a_context,MRAA_GPIO_EDGE_RISING,rf_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(RF_ENCODER_A,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",RF_ENCODER_A);
  }

  rf_b_context=mraa_gpio_init(RF_ENCODER_B);
  if(rf_b_context!=NULL) {
    res=mraa_gpio_dir(rf_b_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(RF_ENCODER_B,MRAA_GPIO_IN) returned %d\n",res);
    }
    if(ENABLE_RF_PULLUP) {
      res=mraa_gpio_mode(rf_b_context,MRAA_GPIO_PULLUP);
      if(res!=MRAA_SUCCESS) {
        fprintf(stderr,"mra_gpio_mode(RF_ENCODER_B,MRAA_GPIO_PULLUP) returned %d\n",res);
      }
    }
    res=mraa_gpio_isr(rf_b_context,MRAA_GPIO_EDGE_RISING,rf_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(RF_ENCODER_B,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",RF_ENCODER_B);
  }
}

if(ENABLE_AGC_ENCODER) {
  agc_a_context=mraa_gpio_init(AGC_ENCODER_A);
  if(agc_a_context!=NULL) {
    res=mraa_gpio_dir(agc_a_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(AGC_ENCODER_A,MRAA_GPIO_IN) returned %d\n",res);
    }
    if(ENABLE_AGC_PULLUP) {
      res=mraa_gpio_mode(agc_a_context,MRAA_GPIO_PULLUP);
      if(res!=MRAA_SUCCESS) {
        fprintf(stderr,"mra_gpio_mode(AGC_ENCODER_A,MRAA_GPIO_PULLUP) returned %d\n",res);
      }
    }
    res=mraa_gpio_isr(agc_a_context,MRAA_GPIO_EDGE_RISING,agc_encoder_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(AGC_ENCODER_A,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",AGC_ENCODER_A);
  }

  agc_b_context=mraa_gpio_init(AGC_ENCODER_B);
  if(agc_b_context!=NULL) {
    res=mraa_gpio_dir(agc_b_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(AGC_ENCODER_B,MRAA_GPIO_IN) returned %d\n",res);
    }
    if(ENABLE_AGC_PULLUP) {
      res=mraa_gpio_mode(agc_b_context,MRAA_GPIO_PULLUP);
      if(res!=MRAA_SUCCESS) {
        fprintf(stderr,"mra_gpio_mode(AGC_ENCODER_B,MRAA_GPIO_PULLUP) returned %d\n",res);
      }
    }
    res=mraa_gpio_isr(agc_b_context,MRAA_GPIO_EDGE_RISING,agc_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(AGC_ENCODER_B,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",AGC_ENCODER_B);
  }
}

if(ENABLE_FUNCTION_BUTTON) {
  function_context=mraa_gpio_init(FUNCTION_BUTTON);
  if(function_context!=NULL) {
    res=mraa_gpio_dir(function_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(FUNCTION_BUTTON,MRAA_GPIO_IN) returned %d\n",res);
    }
    res=mraa_gpio_isr(function_context,MRAA_GPIO_EDGE_RISING,function_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(FUNCTION_BUTTON,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",FUNCTION_BUTTON);
  }
}

if(ENABLE_BAND_BUTTON) {
  band_context=mraa_gpio_init(BAND_BUTTON);
  if(band_context!=NULL) {
    res=mraa_gpio_dir(band_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(BAND_BUTTON,MRAA_GPIO_IN) returned %d\n",res);
    }
    res=mraa_gpio_isr(band_context,MRAA_GPIO_EDGE_RISING,function_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(BAND_BUTTON,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",BAND_BUTTON);
  }
}

if(ENABLE_BANDSTACK_BUTTON) {
  bandstack_context=mraa_gpio_init(BANDSTACK_BUTTON);
  if(bandstack_context!=NULL) {
    res=mraa_gpio_dir(bandstack_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(BANDSTACK_BUTTON,MRAA_GPIO_IN) returned %d\n",res);
    }
    res=mraa_gpio_isr(bandstack_context,MRAA_GPIO_EDGE_RISING,function_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(BANDSTACK_BUTTON,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",BANDSTACK_BUTTON);
  }
}

if(ENABLE_MODE_BUTTON) {
  mode_context=mraa_gpio_init(MODE_BUTTON);
  if(mode_context!=NULL) {
    res=mraa_gpio_dir(mode_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(MODE_BUTTON,MRAA_GPIO_IN) returned %d\n",res);
    }
    res=mraa_gpio_isr(mode_context,MRAA_GPIO_EDGE_RISING,function_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(MODE_BUTTON,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",MODE_BUTTON);
  }
}

if(ENABLE_FILTER_BUTTON) {
  filter_context=mraa_gpio_init(FILTER_BUTTON);
  if(filter_context!=NULL) {
    res=mraa_gpio_dir(filter_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(FILTER_BUTTON,MRAA_GPIO_IN) returned %d\n",res);
    }
    res=mraa_gpio_isr(filter_context,MRAA_GPIO_EDGE_RISING,function_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(FILTER_BUTTON,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",FILTER_BUTTON);
  }
}

if(ENABLE_NOISE_BUTTON) {
  noise_context=mraa_gpio_init(NOISE_BUTTON);
  if(noise_context!=NULL) {
    res=mraa_gpio_dir(noise_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(NOISE_BUTTON,MRAA_GPIO_IN) returned %d\n",res);
    }
    res=mraa_gpio_isr(noise_context,MRAA_GPIO_EDGE_RISING,function_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(NOISE_BUTTON,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",NOISE_BUTTON);
  }
}

if(ENABLE_AGC_BUTTON) {
  agc_context=mraa_gpio_init(AGC_BUTTON);
  if(agc_context!=NULL) {
    res=mraa_gpio_dir(agc_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(AGC_BUTTON,MRAA_GPIO_IN) returned %d\n",res);
    }
    res=mraa_gpio_isr(agc_context,MRAA_GPIO_EDGE_RISING,function_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(AGC_BUTTON,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",AGC_BUTTON);
  }
}

if(ENABLE_MOX_BUTTON) {
  mox_context=mraa_gpio_init(MOX_BUTTON);
  if(mox_context!=NULL) {
    res=mraa_gpio_dir(mox_context,MRAA_GPIO_IN);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_dir(MOX_BUTTON,MRAA_GPIO_IN) returned %d\n",res);
    }
    res=mraa_gpio_isr(mox_context,MRAA_GPIO_EDGE_RISING,function_interrupt,NULL);
    if(res!=MRAA_SUCCESS) {
      fprintf(stderr,"mra_gpio_isr(MOX_BUTTON,MRAA_GPIO_EDGE_BOTH) returned %d\n", res);
    }
  } else {
    fprintf(stderr,"mraa_gpio_init(%d) returned NULL\n",MOX_BUTTON);
  }
}

  int rc=pthread_create(&rotary_encoder_thread_id, NULL, rotary_encoder_thread, NULL);
  if(rc<0) {
    fprintf(stderr,"pthread_create for rotary_encoder_thread failed %d\n",rc);
  }

  return 0;
}

void gpio_close() {
  mraa_result_t res;

  res=mraa_gpio_close(function_context);
  res=mraa_gpio_close(band_context);
  res=mraa_gpio_close(bandstack_context);
  res=mraa_gpio_close(mode_context);
  res=mraa_gpio_close(filter_context);
  res=mraa_gpio_close(noise_context);
  res=mraa_gpio_close(agc_context);
  res=mraa_gpio_close(mox_context);
  res=mraa_gpio_close(agc_b_context);
  res=mraa_gpio_close(agc_a_context);
  res=mraa_gpio_close(rf_b_context);
  res=mraa_gpio_close(rf_a_context);
  res=mraa_gpio_close(af_b_context);
  res=mraa_gpio_close(af_a_context);
  res=mraa_gpio_close(vfo_b_context);
  res=mraa_gpio_close(vfo_a_context);
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

int af_encoder_get_pos() {
    int pos=afEncoderPos;
    afEncoderPos=0;
    return pos;
}

//int af_function_get_state() {
//    return afFunction;
//}

int rf_encoder_get_pos() {
    int pos=rfEncoderPos;
    rfEncoderPos=0;
    return pos;
}

int agc_encoder_get_pos() {
    int pos=agcEncoderPos;
    agcEncoderPos=0;
    return pos;
}

//int rf_function_get_state() {
//    return rfFunction;
//}

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
    int pos=*(int*)data;
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    //entry->frequency=entry->frequency+(pos*step);
    //setFrequency(entry->frequency);
    setFrequency(entry->frequency+ddsOffset+(pos*step));
    vfo_update(NULL);
  }
  free(data);
  return 0;
}

static int af_encoder_changed(void *data) {
  int pos=*(int*)data;
  if(pos!=0) {
    if(function || isTransmitting()) {
      // mic gain
      double gain=mic_gain;
      gain+=(double)pos/100.0;
      if(gain<0.0) {
        gain=0.0;
      } else if(gain>4.0) {
        gain=4.0;
      }
      set_mic_gain(gain);
    } else {
      // af gain
      double gain=volume;
      gain+=(double)pos/100.0;
      if(gain<0.0) {
        gain=0.0;
      } else if(gain>1.0) {
        gain=1.0;
      }
      set_af_gain(gain);
    }
  }
  free(data);
  return 0;
}

static int rf_encoder_changed(void *data) {
  int pos=*(int*)data;
  if(pos!=0) {
    if(function || tune) {
      // tune drive
      double d=getTuneDrive();
      d+=(double)pos/100.0;
      if(d<0.0) {
        d=0.0;
      } else if(d>1.0) {
        d=1.0;
      }
      set_tune(d);
    } else {
      // drive
      double d=getDrive();
      d+=(double)pos/100.0;
      if(d<0.0) {
        d=0.0;
      } else if(d>1.0) {
        d=1.0;
      }
      set_drive(d);
    }
  }
  free(data);
  return 0;
}

static int agc_encoder_changed(void *data) {
  int pos=*(int*)data;
  if(pos!=0) {
    if(function) {
      int att=attenuation;
      att+=pos;
      if(att<0) {
        att=0;
      } else if (att>31) {
        att=31;
      }
      set_attenuation_value((double)att);
    } else {
      double gain=agc_gain;
      gain+=(double)pos;
      if(gain<-20.0) {
        gain=-20.0;
      } else if(gain>120.0) {
        gain=120.0;
      }
      set_agc_gain(gain);
    }
  }
  return 0;
}

static int band_pressed(void *data) {
  sim_band_cb(NULL,NULL);
  return 0;
}

static int bandstack_pressed(void *data) {
  sim_bandstack_cb(NULL,NULL);
  return 0;
}

static int function_pressed(void *data) {
  sim_function_cb(NULL,NULL);
  return 0;
}

static int mox_pressed(void *data) {
  sim_mox_cb(NULL,NULL);
  return 0;
}

static int lock_pressed(void *data) {
fprintf(stderr,"lock_pressed\n");
  lock_cb((GtkWidget *)NULL, (gpointer)NULL);
  return 0;
}

static int mode_pressed(void *data) {
  sim_mode_cb(NULL,NULL);
  return 0;
}

static int filter_pressed(void *data) {
  sim_filter_cb(NULL,NULL);
  return 0;
}

static int noise_pressed(void *data) {
  sim_noise_cb(NULL,NULL);
  return 0;
}

static int agc_pressed(void *data) {
  sim_agc_cb(NULL,NULL);
  return 0;
}

static void* rotary_encoder_thread(void *arg) {
    int pos;
    while(1) {

        int function_button=function_get_state();
        if(function_button!=previous_function_button) {
            previous_function_button=function_button;
            if(function_button) {
                g_idle_add(function_pressed,(gpointer)NULL);
            }
        }

        pos=vfo_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(vfo_encoder_changed,(gpointer)p);
        }

/*
        af_function=af_function_get_state();
        if(af_function!=previous_af_function) {
            previous_af_function=af_function;
        }
*/
        pos=af_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(af_encoder_changed,(gpointer)p);
        }

/*
        rf_function=rf_function_get_state();
        if(rf_function!=previous_rf_function) {
            previous_rf_function=rf_function;
        }
*/
        pos=rf_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(rf_encoder_changed,(gpointer)p);
        }

        pos=agc_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(agc_encoder_changed,(gpointer)p);
        }


        int band_button=band_get_state();
        if(band_button!=previous_band_button) {
            previous_band_button=band_button;
            if(band_button) {
                g_idle_add(band_pressed,(gpointer)NULL);
            }
        }

        int bandstack_button=bandstack_get_state();
        if(bandstack_button!=previous_bandstack_button) {
            previous_bandstack_button=bandstack_button;
            if(bandstack_button) {
                g_idle_add(bandstack_pressed,(gpointer)NULL);
            }
        }

        int mode_button=mode_get_state();
        if(mode_button!=previous_mode_button) {
            previous_mode_button=mode_button;
            if(mode_button) {
                g_idle_add(mode_pressed,(gpointer)NULL);
            }
        }

        int filter_button=filter_get_state();
        if(filter_button!=previous_filter_button) {
            previous_filter_button=filter_button;
            if(filter_button) {
                g_idle_add(filter_pressed,(gpointer)NULL);
            }
        }

        int noise_button=noise_get_state();
        if(noise_button!=previous_noise_button) {
            previous_noise_button=noise_button;
            if(noise_button) {
                g_idle_add(noise_pressed,(gpointer)NULL);
            }
        }

        int agc_button=agc_get_state();
        if(agc_button!=previous_agc_button) {
            previous_agc_button=agc_button;
            if(agc_button) {
                g_idle_add(agc_pressed,(gpointer)NULL);
            }
        }

        int mox_button=mox_get_state();
        if(mox_button!=previous_mox_button) {
            previous_mox_button=mox_button;
            if(mox_button) {
                g_idle_add(mox_pressed,(gpointer)NULL);
            }
        }

        int lock_button=lock_get_state();
        if(lock_button!=previous_lock_button) {
            previous_lock_button=lock_button;
            if(lock_button) {
                g_idle_add(lock_pressed,(gpointer)NULL);
            }
        }

        usleep(100000);
    }
}
