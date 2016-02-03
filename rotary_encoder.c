#include <stdio.h>
#include <pigpio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <pthread.h>
#include <wiringPi.h>

#include "rotary_encoder.h"
#include "main.h"

#define SYSFS_GPIO_DIR  "/sys/class/gpio"

#define RASPBERRYPI_VFO_ENCODER_A 14
#define RASPBERRYPI_VFO_ENCODER_B 15

#define ODROID_VFO_ENCODER_A 108
#define ODROID_VFO_ENCODER_B 97
#define ODROID_VFO_ENCODER_A_PIN 23
#define ODROID_VFO_ENCODER_B_PIN 24

#define AF_ENCODER_A 20
#define AF_ENCODER_B 26
#define AF_FUNCTION 21

#define RF_ENCODER_A 16
#define RF_ENCODER_B 19
#define RF_FUNCTION 13

#define FUNCTION 12
#define BAND 6

static int VFO_ENCODER_A=14;
static int VFO_ENCODER_B=15;

static int VFO_ENCODER_A_PIN=0;
static int VFO_ENCODER_B_PIN=0;

static volatile int vfoEncoderPos;
static volatile int afEncoderPos;
static volatile int afFunction;
static volatile int rfEncoderPos;
static volatile int rfFunction;
static volatile int function;
static volatile int band;

static void afFunctionAlert(int gpio, int level, uint32_t tick) {
    afFunction=(level==0);
}

static void rfFunctionAlert(int gpio, int level, uint32_t tick) {
    rfFunction=(level==0);
}

static void functionAlert(int gpio, int level, uint32_t tick) {
    function=(level==0);
}

static void bandAlert(int gpio, int level, uint32_t tick) {
    band=(level==0);
}

static void vfoEncoderPulse(int gpio, int level, unsigned int tick) {
   static int levA=0, levB=0, lastGpio = -1;

//fprintf(stderr,"vfoEncoderPulse:%d=%d\n",gpio,level);
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

void interruptB(void) {
   vfoEncoderPulse(VFO_ENCODER_B,digitalRead(VFO_ENCODER_B_PIN),0);
}

void interruptA(void) {
   vfoEncoderPulse(VFO_ENCODER_A,digitalRead(VFO_ENCODER_A_PIN),0);
}

int encoder_init() {

    if(strcmp(unameData.nodename,"raspberrypi")==0) {

    VFO_ENCODER_A=RASPBERRYPI_VFO_ENCODER_A;
    VFO_ENCODER_B=RASPBERRYPI_VFO_ENCODER_B;
    VFO_ENCODER_A_PIN=0;
    VFO_ENCODER_B_PIN=0;

    fprintf(stderr,"encoder_init: VFO_ENCODER_A=%d VFO_ENCODER_B=%d\n",VFO_ENCODER_A,VFO_ENCODER_B);

    if(gpioInitialise()<0) {
        fprintf(stderr,"Cannot initialize GPIO\n");
        return -1;
    }

   gpioSetMode(VFO_ENCODER_A, PI_INPUT);
   gpioSetMode(VFO_ENCODER_B, PI_INPUT);
   gpioSetPullUpDown(VFO_ENCODER_A, PI_PUD_UP);
   gpioSetPullUpDown(VFO_ENCODER_B, PI_PUD_UP);
   gpioSetAlertFunc(VFO_ENCODER_A, vfoEncoderPulse);
   gpioSetAlertFunc(VFO_ENCODER_B, vfoEncoderPulse);
   vfoEncoderPos=0;


   gpioSetMode(AF_FUNCTION, PI_INPUT);
   gpioSetPullUpDown(AF_FUNCTION,PI_PUD_UP);
   gpioSetAlertFunc(AF_FUNCTION, afFunctionAlert);
   afFunction=0;

   gpioSetMode(AF_ENCODER_A, PI_INPUT);
   gpioSetMode(AF_ENCODER_B, PI_INPUT);
   gpioSetPullUpDown(AF_ENCODER_A, PI_PUD_OFF);
   gpioSetPullUpDown(AF_ENCODER_B, PI_PUD_OFF);
   gpioSetAlertFunc(AF_ENCODER_A, afEncoderPulse);
   gpioSetAlertFunc(AF_ENCODER_B, afEncoderPulse);
   afEncoderPos=0;

   gpioSetMode(RF_FUNCTION, PI_INPUT);
   gpioSetPullUpDown(RF_FUNCTION,PI_PUD_UP);
   gpioSetAlertFunc(RF_FUNCTION, rfFunctionAlert);
   rfFunction=0;

   gpioSetMode(RF_ENCODER_A, PI_INPUT);
   gpioSetMode(RF_ENCODER_B, PI_INPUT);
   gpioSetPullUpDown(RF_ENCODER_A, PI_PUD_OFF);
   gpioSetPullUpDown(RF_ENCODER_B, PI_PUD_OFF);
   gpioSetAlertFunc(RF_ENCODER_A, rfEncoderPulse);
   gpioSetAlertFunc(RF_ENCODER_B, rfEncoderPulse);
   rfEncoderPos=0;

   gpioSetMode(FUNCTION, PI_INPUT);
   gpioSetPullUpDown(FUNCTION,PI_PUD_UP);
   gpioSetAlertFunc(FUNCTION, functionAlert);

   gpioSetMode(BAND, PI_INPUT);
   gpioSetPullUpDown(BAND,PI_PUD_UP);
   gpioSetAlertFunc(BAND, bandAlert);

   } else if(strcmp(unameData.nodename,"odroid")==0) {

    VFO_ENCODER_A=ODROID_VFO_ENCODER_A;
    VFO_ENCODER_B=ODROID_VFO_ENCODER_B;
    VFO_ENCODER_A_PIN=ODROID_VFO_ENCODER_A_PIN;
    VFO_ENCODER_B_PIN=ODROID_VFO_ENCODER_B_PIN;

    fprintf(stderr,"encoder_init: VFO_ENCODER_A=%d VFO_ENCODER_B=%d\n",VFO_ENCODER_A,VFO_ENCODER_B);

  if (wiringPiSetup () < 0) {
      printf ("Unable to setup wiringPi: %s\n", strerror (errno));
      return 1;
  }

  FILE *fp;

  fp = popen("echo 97 > /sys/class/gpio/export\n", "r");
  pclose(fp);
  fp = popen("echo \"in\" > /sys/class/gpio/gpio97/direction\n", "r");
  pclose(fp);
  fp = popen("chmod 0666 /sys/class/gpio/gpio97/value\n", "r");
  pclose(fp);

  fp = popen("echo 108 > /sys/class/gpio/export\n", "r");
  pclose(fp);
  fp = popen("echo \"in\" > /sys/class/gpio/gpio108/direction\n", "r");
  pclose(fp);
  fp = popen("chmod 0666 /sys/class/gpio/gpio108/value\n", "r");
  pclose(fp);

  if ( wiringPiISR (24, INT_EDGE_BOTH, &interruptB) < 0 ) {
      printf ("Unable to setup ISR: %s\n", strerror (errno));
      return 1;
  }

  if ( wiringPiISR (23, INT_EDGE_BOTH, &interruptA) < 0 ) {
      printf ("Unable to setup ISR: %s\n", strerror (errno));
      return 1;
  }
  } else {
     fprintf(stderr,"Unknown nodename: %s. Rotary Encoder not enabled.\n",unameData.nodename);
     return 1;
  }


   return 0;
}

void encoder_close() {
  if(strcmp(unameData.nodename,"odroid")==0) {
    FILE *fp;
    fp = popen("echo 97 > /sys/class/gpio/unexport\n", "r");
    pclose(fp);
    fp = popen("echo 108 > /sys/class/gpio/unexport\n", "r");
    pclose(fp);
  }
}

int vfo_encoder_get_pos() {
    int pos=vfoEncoderPos;

  if(strcmp(unameData.nodename,"raspberrypi")==0) {
    if(pos<0 && pos>-12) {
        pos=0;
    } else if(pos>0 && pos<12) {
        pos=0;
    }
    pos=pos/12;
    vfoEncoderPos=vfoEncoderPos-(pos*12);
  } else if(strcmp(unameData.nodename,"odroid")==0) {
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

int rf_function_get_state() {
    return rfFunction;
}

int function_get_state() {
    return function;
}

int band_get_state() {
    return band;
}
