
#ifdef GPIO
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
#include <wiringPi.h>
#ifdef raspberrypi
#include <pigpio.h>
#endif

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

#define SYSFS_GPIO_DIR  "/sys/class/gpio"

int ENABLE_VFO_ENCODER=1;
int ENABLE_VFO_PULLUP=1;
int VFO_ENCODER_A=17;
int VFO_ENCODER_B=18;
#ifdef odroid
int VFO_ENCODER_A_PIN=0;
int VFO_ENCODER_B_PIN=1;
#endif
int ENABLE_AF_ENCODER=1;
int ENABLE_AF_PULLUP=0;
int AF_ENCODER_A=20;
int AF_ENCODER_B=26;
int AF_FUNCTION=25;
int ENABLE_RF_ENCODER=1;
int ENABLE_RF_PULLUP=0;
int RF_ENCODER_A=16;
int RF_ENCODER_B=19;
int RF_FUNCTION=8;
int ENABLE_AGC_ENCODER=1;
int ENABLE_AGC_PULLUP=0;
int AGC_ENCODER_A=4;
int AGC_ENCODER_B=21;
int AGC_FUNCTION=7;
int ENABLE_BAND_BUTTON=1;
int BAND_BUTTON=13;
int ENABLE_BANDSTACK_BUTTON=1;
int BANDSTACK_BUTTON=12;
int ENABLE_MODE_BUTTON=1;
int MODE_BUTTON=6;
int ENABLE_FILTER_BUTTON=1;
int FILTER_BUTTON=5;
int ENABLE_NOISE_BUTTON=1;
int NOISE_BUTTON=24;
int ENABLE_AGC_BUTTON=1;
int AGC_BUTTON=23;
int ENABLE_MOX_BUTTON=1;
int MOX_BUTTON=27;
int ENABLE_FUNCTION_BUTTON=1;
int FUNCTION_BUTTON=22;
int ENABLE_LOCK_BUTTON=1;
int LOCK_BUTTON=25;

static volatile int vfoEncoderPos;
static volatile int afEncoderPos;
static volatile int afFunction;
static volatile int rfEncoderPos;
static volatile int rfFunction;
static volatile int agcEncoderPos;
static volatile int agcFunction;
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
static int af_function=0;
static int previous_af_function=0;
static int rf_function=0;
static int previous_rf_function=0;
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

static int running=1;

static void afFunctionAlert(int gpio, int level, uint32_t tick) {
    afFunction=(level==0);
}

static void agcFunctionAlert(int gpio, int level, uint32_t tick) {
    if(level==0) {
      agcFunction=agcFunction==0?1:0;
    }
}

static void rfFunctionAlert(int gpio, int level, uint32_t tick) {
    rfFunction=(level==0);
}

static void functionAlert(int gpio, int level, uint32_t tick) {
    function_state=(level==0);
}

static void bandAlert(int gpio, int level, uint32_t tick) {
    band_state=(level==0);
}

static void bandstackAlert(int gpio, int level, uint32_t tick) {
    bandstack_state=(level==0);
}

static void modeAlert(int gpio, int level, uint32_t tick) {
    mode_state=(level==0);
}

static void filterAlert(int gpio, int level, uint32_t tick) {
    filter_state=(level==0);
}

static void noiseAlert(int gpio, int level, uint32_t tick) {
    noise_state=(level==0);
}

static void agcAlert(int gpio, int level, uint32_t tick) {
    agc_state=(level==0);
}

static void moxAlert(int gpio, int level, uint32_t tick) {
    mox_state=(level==0);
}

static void lockAlert(int gpio, int level, uint32_t tick) {
    lock_state=(level==0);
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

static void afEncoderPulse(int gpio, int level, uint32_t tick)
{
   static int levA=0, levB=0, lastGpio = -1;

   if (gpio == AF_ENCODER_A) levA = level; else levB = level;

   if (gpio != lastGpio) /* debounce */
   {
      lastGpio = gpio;

      if ((gpio == AF_ENCODER_A) && (level == 0))
      {
         if (!levB) ++afEncoderPos;
      }
      else if ((gpio == AF_ENCODER_B) && (level == 1))
      {
         if (levA) --afEncoderPos;
      }
   }
}

static void rfEncoderPulse(int gpio, int level, uint32_t tick)
{
   static int levA=0, levB=0, lastGpio = -1;

   if (gpio == RF_ENCODER_A) levA = level; else levB = level;

   if (gpio != lastGpio) /* debounce */
   {
      lastGpio = gpio;

      if ((gpio == RF_ENCODER_A) && (level == 0))
      {
         if (!levB) ++rfEncoderPos;
      }
      else if ((gpio == RF_ENCODER_B) && (level == 1))
      {
         if (levA) --rfEncoderPos;
      }
   }
}

static void agcEncoderPulse(int gpio, int level, uint32_t tick)
{
   static int levA=0, levB=0, lastGpio = -1;

   if (gpio == AGC_ENCODER_A) levA = level; else levB = level;

   if (gpio != lastGpio) /* debounce */
   {
      lastGpio = gpio;

      if ((gpio == AGC_ENCODER_A) && (level == 0))
      {
         if (!levB) ++agcEncoderPos;
      }
      else if ((gpio == AGC_ENCODER_B) && (level == 1))
      {
         if (levA) --agcEncoderPos;
      }
   }
}

#ifdef odroid
void interruptB(void) {
   vfoEncoderPulse(VFO_ENCODER_B,digitalRead(VFO_ENCODER_B_PIN),0);
}

void interruptA(void) {
   vfoEncoderPulse(VFO_ENCODER_A,digitalRead(VFO_ENCODER_A_PIN),0);
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
#ifdef odroid
  value=getProperty("VFO_ENCODER_A_PIN");
  if(value) VFO_ENCODER_A_PIN=atoi(value);
  value=getProperty("VFO_ENCODER_B_PIN");
  if(value) VFO_ENCODER_B_PIN=atoi(value);
#endif
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
#ifdef odroid
  sprintf(value,"%d",VFO_ENCODER_A_PIN);
  setProperty("VFO_ENCODER_A_PIN",value);
  sprintf(value,"%d",VFO_ENCODER_B_PIN);
  setProperty("VFO_ENCODER_B_PIN",value);
#endif
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
fprintf(stderr,"encoder_init\n");

#ifdef odroid
  VFO_ENCODER_A=88;
  VFO_ENCODER_B=87;
#endif

  gpio_restore_state();
#ifdef raspberrypi

    fprintf(stderr,"encoder_init: VFO_ENCODER_A=%d VFO_ENCODER_B=%d\n",VFO_ENCODER_A,VFO_ENCODER_B);

    fprintf(stderr,"gpioInitialize\n");
    if(gpioInitialise()<0) {
        fprintf(stderr,"Cannot initialize GPIO\n");
        return -1;
    }

  if(ENABLE_FUNCTION_BUTTON) {
    gpioSetMode(FUNCTION_BUTTON, PI_INPUT);
    gpioSetPullUpDown(FUNCTION_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(FUNCTION_BUTTON, functionAlert);
  }

  if(ENABLE_VFO_ENCODER) {
    gpioSetMode(VFO_ENCODER_A, PI_INPUT);
    gpioSetMode(VFO_ENCODER_B, PI_INPUT);
    if(ENABLE_VFO_PULLUP) {
      gpioSetPullUpDown(VFO_ENCODER_A, PI_PUD_UP);
      gpioSetPullUpDown(VFO_ENCODER_B, PI_PUD_UP);
    } else {
      gpioSetPullUpDown(VFO_ENCODER_A, PI_PUD_OFF);
      gpioSetPullUpDown(VFO_ENCODER_B, PI_PUD_OFF);
    }
    gpioSetAlertFunc(VFO_ENCODER_A, vfoEncoderPulse);
    gpioSetAlertFunc(VFO_ENCODER_B, vfoEncoderPulse);
    vfoEncoderPos=0;
  }


    gpioSetMode(AF_FUNCTION, PI_INPUT);
    gpioSetPullUpDown(AF_FUNCTION,PI_PUD_UP);
    gpioSetAlertFunc(AF_FUNCTION, afFunctionAlert);
    afFunction=0;

  if(ENABLE_AF_ENCODER) {
    gpioSetMode(AF_ENCODER_A, PI_INPUT);
    gpioSetMode(AF_ENCODER_B, PI_INPUT);
    if(ENABLE_AF_PULLUP) {
      gpioSetPullUpDown(AF_ENCODER_A, PI_PUD_UP);
      gpioSetPullUpDown(AF_ENCODER_B, PI_PUD_UP);
    } else {
      gpioSetPullUpDown(AF_ENCODER_A, PI_PUD_OFF);
      gpioSetPullUpDown(AF_ENCODER_B, PI_PUD_OFF);
    }
    gpioSetAlertFunc(AF_ENCODER_A, afEncoderPulse);
    gpioSetAlertFunc(AF_ENCODER_B, afEncoderPulse);
    afEncoderPos=0;
  }

    gpioSetMode(RF_FUNCTION, PI_INPUT);
    gpioSetPullUpDown(RF_FUNCTION,PI_PUD_UP);
    gpioSetAlertFunc(RF_FUNCTION, rfFunctionAlert);
    rfFunction=0;

  if(ENABLE_RF_ENCODER) {
    gpioSetMode(RF_ENCODER_A, PI_INPUT);
    gpioSetMode(RF_ENCODER_B, PI_INPUT);
    if(ENABLE_AF_PULLUP) {
      gpioSetPullUpDown(RF_ENCODER_A, PI_PUD_UP);
      gpioSetPullUpDown(RF_ENCODER_B, PI_PUD_UP);
    } else {
      gpioSetPullUpDown(RF_ENCODER_A, PI_PUD_OFF);
      gpioSetPullUpDown(RF_ENCODER_B, PI_PUD_OFF);
    }
    gpioSetAlertFunc(RF_ENCODER_A, rfEncoderPulse);
    gpioSetAlertFunc(RF_ENCODER_B, rfEncoderPulse);
    rfEncoderPos=0;
  }

  gpioSetMode(AGC_FUNCTION, PI_INPUT);
  gpioSetPullUpDown(AGC_FUNCTION,PI_PUD_UP);
  gpioSetAlertFunc(AGC_FUNCTION, agcFunctionAlert);
  agcFunction=0;

  if(ENABLE_AGC_ENCODER) {
    gpioSetMode(AGC_ENCODER_A, PI_INPUT);
    gpioSetMode(AGC_ENCODER_B, PI_INPUT);
    if(ENABLE_AF_PULLUP) {
      gpioSetPullUpDown(AGC_ENCODER_A, PI_PUD_UP);
      gpioSetPullUpDown(AGC_ENCODER_B, PI_PUD_UP);
    } else {
      gpioSetPullUpDown(AGC_ENCODER_A, PI_PUD_OFF);
      gpioSetPullUpDown(AGC_ENCODER_B, PI_PUD_OFF);
    }
    gpioSetAlertFunc(AGC_ENCODER_A, agcEncoderPulse);
    gpioSetAlertFunc(AGC_ENCODER_B, agcEncoderPulse);
    agcEncoderPos=0;
  }


  if(ENABLE_BAND_BUTTON) {
    gpioSetMode(BAND_BUTTON, PI_INPUT);
    gpioSetPullUpDown(BAND_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(BAND_BUTTON, bandAlert);
  }
 
  if(ENABLE_BANDSTACK_BUTTON) {
    gpioSetMode(BANDSTACK_BUTTON, PI_INPUT);
    gpioSetPullUpDown(BANDSTACK_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(BANDSTACK_BUTTON, bandstackAlert);
  }
 
  if(ENABLE_MODE_BUTTON) {
    gpioSetMode(MODE_BUTTON, PI_INPUT);
    gpioSetPullUpDown(MODE_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(MODE_BUTTON, modeAlert);
  }
 
  if(ENABLE_FILTER_BUTTON) {
    gpioSetMode(FILTER_BUTTON, PI_INPUT);
    gpioSetPullUpDown(FILTER_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(FILTER_BUTTON, filterAlert);
  }
 
  if(ENABLE_NOISE_BUTTON) {
    gpioSetMode(NOISE_BUTTON, PI_INPUT);
    gpioSetPullUpDown(NOISE_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(NOISE_BUTTON, noiseAlert);
  }
 
  if(ENABLE_AGC_BUTTON) {
    gpioSetMode(AGC_BUTTON, PI_INPUT);
    gpioSetPullUpDown(AGC_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(AGC_BUTTON, agcAlert);
  }
 
  if(ENABLE_MOX_BUTTON) {
    gpioSetMode(MOX_BUTTON, PI_INPUT);
    gpioSetPullUpDown(MOX_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(MOX_BUTTON, moxAlert);
  }

  if(ENABLE_LOCK_BUTTON) {
    gpioSetMode(LOCK_BUTTON, PI_INPUT);
    gpioSetPullUpDown(LOCK_BUTTON,PI_PUD_UP);
    gpioSetAlertFunc(LOCK_BUTTON, lockAlert);
  }
 
#endif

#ifdef odroid

    //VFO_ENCODER_A=ODROID_VFO_ENCODER_A;
    //VFO_ENCODER_B=ODROID_VFO_ENCODER_B;
    //VFO_ENCODER_A_PIN=ODROID_VFO_ENCODER_A_PIN;
    //VFO_ENCODER_B_PIN=ODROID_VFO_ENCODER_B_PIN;
 
    fprintf(stderr,"encoder_init: VFO_ENCODER_A=%d VFO_ENCODER_B=%d\n",VFO_ENCODER_A,VFO_ENCODER_B);

    fprintf(stderr,"wiringPiSetup\n");
    if (wiringPiSetup () < 0) {
      printf ("Unable to setup wiringPi: %s\n", strerror (errno));
      return 1;
    }

    FILE *fp;

    fp = popen("echo 88 > /sys/class/gpio/export\n", "r");
    pclose(fp);
    fp = popen("echo \"in\" > /sys/class/gpio/gpio88/direction\n", "r");
    pclose(fp);
    fp = popen("chmod 0666 /sys/class/gpio/gpio88/value\n", "r");
    pclose(fp);

    fp = popen("echo 87 > /sys/class/gpio/export\n", "r");
    pclose(fp);
    fp = popen("echo \"in\" > /sys/class/gpio/gpio87/direction\n", "r");
    pclose(fp);
    fp = popen("chmod 0666 /sys/class/gpio/gpio87/value\n", "r");
    pclose(fp);

    if ( wiringPiISR (0, INT_EDGE_BOTH, &interruptB) < 0 ) {
      printf ("Unable to setup ISR: %s\n", strerror (errno));
      return 1;
    }

    if ( wiringPiISR (1, INT_EDGE_BOTH, &interruptA) < 0 ) {
      printf ("Unable to setup ISR: %s\n", strerror (errno));
      return 1;
    }
#endif

  int rc=pthread_create(&rotary_encoder_thread_id, NULL, rotary_encoder_thread, NULL);
  if(rc<0) {
    fprintf(stderr,"pthread_create for rotary_encoder_thread failed %d\n",rc);
  }


  return 0;
}

void gpio_close() {
    running=0;
#ifdef odroid
    FILE *fp;
    fp = popen("echo 97 > /sys/class/gpio/unexport\n", "r");
    pclose(fp);
    fp = popen("echo 108 > /sys/class/gpio/unexport\n", "r");
    pclose(fp);
#endif
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

int af_function_get_state() {
    return afFunction;
}

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

int agc_function_get_state() {
    return agcFunction;
}

int rf_function_get_state() {
    return rfFunction;
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
    int pos=*(int*)data;

    if(function) {
#ifdef PSK
      if(mode==modePSK) {
        psk_set_frequency(psk_get_frequency()-(pos*10));
      }
#endif
    } else {
      // VFO
      BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
      setFrequency(entry->frequencyA+ddsOffset+(pos*step));
    }
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
      } else if(gain>1.0) {
        gain=1.0;
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
    if(agcFunction) {
      rit-=pos;
      if(rit>1000) rit=1000;
      if(rit<-1000) rit=-1000;
      vfo_update(NULL);
    } else {
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
    running=1;
    while(running) {

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

#ifdef raspberrypi
          if(running) gpioDelay(100000); // 10 per second
#endif
#ifdef odroid
          if(runnig) usleep(100000);
#endif
    }
#ifdef raspberrypi
    gpioTerminate();
#endif
}
#endif
