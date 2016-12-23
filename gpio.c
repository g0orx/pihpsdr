
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
#include <semaphore.h>
#ifdef raspberrypi
#include <pigpio.h>
#endif
#ifdef sx1509
#include <SparkFunSX1509_C.h>
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
#if defined odroid && !defined sx1509
int VFO_ENCODER_A_PIN=0;
int VFO_ENCODER_B_PIN=1;
#endif
int ENABLE_E1_ENCODER=1;
int ENABLE_E1_PULLUP=0;
int E1_ENCODER_A=20;
int E1_ENCODER_B=26;
#ifdef sx1509
int E1_FUNCTION=25;
#else
int E1_FUNCTION=2; //RRK, was 25 now taken by waveshare LCD TS, disable i2c
int LOCK_BUTTON=2; //temporarily in flux upstream
#endif
int ENABLE_E2_ENCODER=1;
int ENABLE_E2_PULLUP=0;
int E2_ENCODER_A=16;
int E2_ENCODER_B=19;
int E2_FUNCTION=8;
int ENABLE_E3_ENCODER=1;
int ENABLE_E3_PULLUP=0;
int E3_ENCODER_A=4;
int E3_ENCODER_B=21;
#if defined sx1509
int E3_FUNCTION=7;
#else
int E3_FUNCTION=3; //RRK, was 7 now taken by waveshare LCD TS, disable i2c
#endif
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
int ENABLE_LOCK_BUTTON=1;
int ENABLE_CW_BUTTONS=1;
// make sure to disable UART0 for next 2 gpios
int CWL_BUTTON=9;
int CWR_BUTTON=10;

#ifdef sx1509
/* Hardware Hookup:

Leaves a spare gpio and an extra unused button (x1)

SX1509 Breakout ------ Odroid ------------ Component
      GND -------------- GND (1)
      3V3 -------------- 3.3V(6)
      SDA -------------- SDA (3)
      SCL -------------- SCL (5)
      INT -------------- #88 (11)
       0 --------------------------------- TN S1 E1 (row 1)
       1 --------------------------------- S2 S3 E2 (row 2)
       2 --------------------------------- S4 S5 E3 (row 3)
       3 --------------------------------- S6 FN x1 (row 4)
       4 --------------------------------- VFO_ENCODER_A
       5 --------------------------------- VFO_ENCODER_B
       6 --------------------------------- E1_ENCODER_A
       7 --------------------------------- E1_ENCODER_B
       8 --------------------------------- TN S2 S4 S6 (col 1)
       9 --------------------------------- S1 S3 S5 FN (col 2)
       10 -------------------------------- E1 E2 E3 x1 (col 3)
       11 -------------------------------- E2_ENCODER_A
       12 -------------------------------- E2_ENCODER_B
       13 -------------------------------- E3_ENCODER_A
       14 -------------------------------- E3_ENCODER_B
       15 -------------------------------- spare_gpio

Alternate to allow 5 extra buttons

       0 --------------------------------- TN S1 x1 x2 (row 1)
       1 --------------------------------- S2 S3 E1 x4 (row 2)
       2 --------------------------------- S4 S5 E2 x5 (row 3)
       3 --------------------------------- S6 x3 E3 FN (row 4)
       4 --------------------------------- VFO_ENCODER_A
       5 --------------------------------- VFO_ENCODER_B
       6 --------------------------------- E1_ENCODER_A
       7 --------------------------------- E1_ENCODER_B
       8 --------------------------------- TN S2 S4 S6 (col 1)
       9 --------------------------------- S1 S3 S5 x3 (col 2)
       10 -------------------------------- x1 E1 E2 E3 (col 3)
       11 -------------------------------- x2 x4 x5 FN (col 4)
       12 -------------------------------- E2_ENCODER_A
       13 -------------------------------- E2_ENCODER_B
       14 -------------------------------- E3_ENCODER_A
       15 -------------------------------- E3_ENCODER_B

x1-x5 (spare buttons)

*/
const uint8_t SX1509_ADDRESS=0x3E;
struct SX1509* pSX1509;

//#ifdef odroid
int SX1509_INT_PIN=0;
//#endif
#endif

static volatile int vfoEncoderPos;
static volatile int e1EncoderPos;
static volatile int e1Function;
static volatile int e2EncoderPos;
static volatile int e2Function;
static volatile int e3EncoderPos;
static volatile int e3Function;
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
static int e1_function=0;
static int previous_e1_function=0;
static int e2_function=0;
static int previous_e2_function=0;
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

static void e1FunctionAlert(int gpio, int level, uint32_t tick) {
    e1Function=(level==0);
}

static void e3FunctionAlert(int gpio, int level, uint32_t tick) {
    if(level==0) {
      e3Function=e3Function==0?1:0;
    }
}

static void e2FunctionAlert(int gpio, int level, uint32_t tick) {
    e2Function=(level==0);
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

static void cwAlert(int gpio, int level, uint32_t tick) {
fprintf(stderr,"cwAlert: gpio=%d level=%d internal=%d\n",gpio,level,cw_keyer_internal);
#ifdef LOCALCW
    if (cw_keyer_internal == 0 && (mode==modeCWL || mode==modeCWU))
       keyer_event(gpio, cw_active_level == 0 ? level : (level==0));
#endif
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

static void e1EncoderPulse(int gpio, int level, uint32_t tick)
{
   static int levA=0, levB=0, lastGpio = -1;

   if (gpio == E1_ENCODER_A) levA = level; else levB = level;

   if (gpio != lastGpio) /* debounce */
   {
      lastGpio = gpio;

      if ((gpio == E1_ENCODER_A) && (level == 0))
      {
         if (!levB) ++e1EncoderPos;
      }
      else if ((gpio == E1_ENCODER_B) && (level == 1))
      {
         if (levA) --e1EncoderPos;
      }
   }
}

static void e2EncoderPulse(int gpio, int level, uint32_t tick)
{
   static int levA=0, levB=0, lastGpio = -1;

   if (gpio == E2_ENCODER_A) levA = level; else levB = level;

   if (gpio != lastGpio) /* debounce */
   {
      lastGpio = gpio;

      if ((gpio == E2_ENCODER_A) && (level == 0))
      {
         if (!levB) ++e2EncoderPos;
      }
      else if ((gpio == E2_ENCODER_B) && (level == 1))
      {
         if (levA) --e2EncoderPos;
      }
   }
}

static void e3EncoderPulse(int gpio, int level, uint32_t tick)
{
   static int levA=0, levB=0, lastGpio = -1;

   if (gpio == E3_ENCODER_A) levA = level; else levB = level;

   if (gpio != lastGpio) /* debounce */
   {
      lastGpio = gpio;

      if ((gpio == E3_ENCODER_A) && (level == 0))
      {
         if (!levB) ++e3EncoderPos;
      }
      else if ((gpio == E3_ENCODER_B) && (level == 1))
      {
         if (levA) --e3EncoderPos;
      }
   }
}

#ifdef sx1509
#define SX1509_ENCODER_MASK 0xF0F0

#define BTN_ROWS 4 // Number of rows in the button matrix
#define BTN_COLS 4 // Number of columns in the button matrix

// btnMap maps row/column combinations to button states:
volatile int *btnArray[BTN_ROWS][BTN_COLS] = {
  { &mox_state, &band_state, NULL, NULL},
  { &bandstack_state, &mode_state, &e1Function, NULL},
  { &filter_state, &noise_state, &e2Function, NULL},
  { &agc_state, NULL, &e3Function, &function_state}
};

void sx1509_interrupt(void) {

   static int lastBtnPress = 255;
   static uint64_t lastBtnPressTime = 0;

   // read and clear encoder interrupts
   uint16_t encInterrupt = SX1509_interruptSource(pSX1509, true);


   if (encInterrupt & SX1509_ENCODER_MASK) {
      if (encInterrupt & (1<<VFO_ENCODER_A))
        vfoEncoderPulse(VFO_ENCODER_A, SX1509_digitalRead(pSX1509, VFO_ENCODER_A), 0);
      if (encInterrupt & (1<<VFO_ENCODER_B))
        vfoEncoderPulse(VFO_ENCODER_B, SX1509_digitalRead(pSX1509, VFO_ENCODER_B), 0);
      if (encInterrupt & (1<<E1_ENCODER_A))
        e1EncoderPulse(E1_ENCODER_A, SX1509_digitalRead(pSX1509, E1_ENCODER_A), 0);
      if (encInterrupt & (1<<E1_ENCODER_B))
        e1EncoderPulse(E1_ENCODER_B, SX1509_digitalRead(pSX1509, E1_ENCODER_B), 0);
      if (encInterrupt & (1<<E2_ENCODER_A))
        e2EncoderPulse(E2_ENCODER_A, SX1509_digitalRead(pSX1509, E2_ENCODER_A), 0);
      if (encInterrupt & (1<<E2_ENCODER_B))
        e2EncoderPulse(E2_ENCODER_B, SX1509_digitalRead(pSX1509, E2_ENCODER_B), 0);
      if (encInterrupt & (1<<E3_ENCODER_A))
        e3EncoderPulse(E3_ENCODER_A, SX1509_digitalRead(pSX1509, E3_ENCODER_A), 0);
      if (encInterrupt & (1<<E3_ENCODER_B))
        e3EncoderPulse(E3_ENCODER_B, SX1509_digitalRead(pSX1509, E3_ENCODER_B), 0);
   }

   uint16_t btnData = SX1509_readKeypad(pSX1509);

   if (btnData) {
      uint8_t row = SX1509_getRow(pSX1509, btnData);
      uint8_t col = SX1509_getCol(pSX1509, btnData);

      if ((btnData != lastBtnPress) ||
          (lastBtnPressTime < millis() - 100)) //100ms
      {
         lastBtnPress = btnData;
         lastBtnPressTime = millis();
         if (btnArray[row][col] != NULL)
            *btnArray[row][col] = 1;
      }
   }
}
#endif

#if defined odroid && !defined sx1509
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
#if defined odroid && !defined sx1509
  value=getProperty("VFO_ENCODER_A_PIN");
  if(value) VFO_ENCODER_A_PIN=atoi(value);
  value=getProperty("VFO_ENCODER_B_PIN");
  if(value) VFO_ENCODER_B_PIN=atoi(value);
#endif
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
  value=getProperty("ENABLE_LOCK_BUTTON");
  if(value) ENABLE_LOCK_BUTTON=atoi(value);
#ifndef sx1509
  value=getProperty("LOCK_BUTTON");
  if(value) LOCK_BUTTON=atoi(value);
#endif
  value=getProperty("ENABLE_CW_BUTTONS");
  if(value) ENABLE_CW_BUTTONS=atoi(value);
  value=getProperty("CWL_BUTTON");
  if(value) CWL_BUTTON=atoi(value);
  value=getProperty("CWR_BUTTON");
  if(value) CWR_BUTTON=atoi(value);
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
#if defined odroid && !defined sx1509
  sprintf(value,"%d",VFO_ENCODER_A_PIN);
  setProperty("VFO_ENCODER_A_PIN",value);
  sprintf(value,"%d",VFO_ENCODER_B_PIN);
  setProperty("VFO_ENCODER_B_PIN",value);
#endif
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
  sprintf(value,"%d",ENABLE_LOCK_BUTTON);
  setProperty("ENABLE_LOCK_BUTTON",value);
#ifndef sx1509
  sprintf(value,"%d",LOCK_BUTTON);
  setProperty("LOCK_BUTTON",value);
#endif
  sprintf(value,"%d",ENABLE_CW_BUTTONS);
  setProperty("ENABLE_CW_BUTTONS",value);
  sprintf(value,"%d",CWL_BUTTON);
  setProperty("CWL_BUTTON",value);
  sprintf(value,"%d",CWR_BUTTON);
  setProperty("CWR_BUTTON",value);

  saveProperties("gpio.props");
}


int gpio_init() {
fprintf(stderr,"encoder_init\n");

#if defined odroid && !defined sx1509
  VFO_ENCODER_A=88;
  VFO_ENCODER_B=87;
#endif

  gpio_restore_state();
#ifdef raspberrypi

#define BUTTON_STEADY_TIME_US 5000

  void setup_button(int button, gpioAlertFunc_t pAlert) {
    gpioSetMode(button, PI_INPUT);
    gpioSetPullUpDown(button,PI_PUD_UP);
    // give time to settle to avoid false triggers
    usleep(10000);
    gpioSetAlertFunc(button, pAlert);
    gpioGlitchFilter(button, BUTTON_STEADY_TIME_US);
  }

    fprintf(stderr,"encoder_init: VFO_ENCODER_A=%d VFO_ENCODER_B=%d\n",VFO_ENCODER_A,VFO_ENCODER_B);

    fprintf(stderr,"gpioInitialize\n");
    if(gpioInitialise()<0) {
        fprintf(stderr,"Cannot initialize GPIO\n");
        return -1;
    }

  if(ENABLE_FUNCTION_BUTTON) {
    setup_button(FUNCTION_BUTTON, functionAlert);
  }

  if(ENABLE_VFO_ENCODER) {
    if(gpioSetMode(VFO_ENCODER_A, PI_INPUT)!=0) {
      fprintf(stderr,"gpioSetMode for VFO_ENCODER_A failed\n");
    }
    if(gpioSetMode(VFO_ENCODER_B, PI_INPUT)!=0) {
      fprintf(stderr,"gpioSetMode for VFO_ENCODER_B failed\n");
    }
    if(ENABLE_VFO_PULLUP) {
      gpioSetPullUpDown(VFO_ENCODER_A, PI_PUD_UP);
      gpioSetPullUpDown(VFO_ENCODER_B, PI_PUD_UP);
    } else {
      gpioSetPullUpDown(VFO_ENCODER_A, PI_PUD_OFF);
      gpioSetPullUpDown(VFO_ENCODER_B, PI_PUD_OFF);
    }
    if(gpioSetAlertFunc(VFO_ENCODER_A, vfoEncoderPulse)!=0) {
      fprintf(stderr,"gpioSetAlertFunc for VFO_ENCODER_A failed\n");
    }
    if(gpioSetAlertFunc(VFO_ENCODER_B, vfoEncoderPulse)!=0) {
      fprintf(stderr,"gpioSetAlertFunc for VFO_ENCODER_B failed\n");
    }
    vfoEncoderPos=0;
  }


  setup_button(E1_FUNCTION, e1FunctionAlert);
  e1Function=0;

  if(ENABLE_E1_ENCODER) {
    gpioSetMode(E1_ENCODER_A, PI_INPUT);
    gpioSetMode(E1_ENCODER_B, PI_INPUT);
    if(ENABLE_E1_PULLUP) {
      gpioSetPullUpDown(E1_ENCODER_A, PI_PUD_UP);
      gpioSetPullUpDown(E1_ENCODER_B, PI_PUD_UP);
    } else {
      gpioSetPullUpDown(E1_ENCODER_A, PI_PUD_OFF);
      gpioSetPullUpDown(E1_ENCODER_B, PI_PUD_OFF);
    }
    gpioSetAlertFunc(E1_ENCODER_A, e1EncoderPulse);
    gpioSetAlertFunc(E1_ENCODER_B, e1EncoderPulse);
    e1EncoderPos=0;
  }

    setup_button(E2_FUNCTION, e2FunctionAlert);
    e2Function=0;

  if(ENABLE_E2_ENCODER) {
    gpioSetMode(E2_ENCODER_A, PI_INPUT);
    gpioSetMode(E2_ENCODER_B, PI_INPUT);
    if(ENABLE_E1_PULLUP) {
      gpioSetPullUpDown(E2_ENCODER_A, PI_PUD_UP);
      gpioSetPullUpDown(E2_ENCODER_B, PI_PUD_UP);
    } else {
      gpioSetPullUpDown(E2_ENCODER_A, PI_PUD_OFF);
      gpioSetPullUpDown(E2_ENCODER_B, PI_PUD_OFF);
    }
    gpioSetAlertFunc(E2_ENCODER_A, e2EncoderPulse);
    gpioSetAlertFunc(E2_ENCODER_B, e2EncoderPulse);
    e2EncoderPos=0;
  }

  setup_button(E3_FUNCTION, e3FunctionAlert);
  e3Function=0;

  if(ENABLE_E3_ENCODER) {
    gpioSetMode(E3_ENCODER_A, PI_INPUT);
    gpioSetMode(E3_ENCODER_B, PI_INPUT);
    if(ENABLE_E1_PULLUP) {
      gpioSetPullUpDown(E3_ENCODER_A, PI_PUD_UP);
      gpioSetPullUpDown(E3_ENCODER_B, PI_PUD_UP);
    } else {
      gpioSetPullUpDown(E3_ENCODER_A, PI_PUD_OFF);
      gpioSetPullUpDown(E3_ENCODER_B, PI_PUD_OFF);
    }
    gpioSetAlertFunc(E3_ENCODER_A, e3EncoderPulse);
    gpioSetAlertFunc(E3_ENCODER_B, e3EncoderPulse);
    e3EncoderPos=0;
  }


  if(ENABLE_S1_BUTTON) {
    setup_button(S1_BUTTON, bandAlert);
  }
 
  if(ENABLE_S2_BUTTON) {
    setup_button(S2_BUTTON, bandstackAlert);
  }
 
  if(ENABLE_S3_BUTTON) {
    setup_button(S3_BUTTON, modeAlert);
  }
 
  if(ENABLE_S4_BUTTON) {
    setup_button(S4_BUTTON, filterAlert);
  }
 
  if(ENABLE_S5_BUTTON) {
    setup_button(S5_BUTTON, noiseAlert);
  }
 
  if(ENABLE_S6_BUTTON) {
    setup_button(S6_BUTTON, agcAlert);
  }
 
  if(ENABLE_MOX_BUTTON) {
    setup_button(MOX_BUTTON, moxAlert);
  }

#ifndef sx1509
  if(ENABLE_LOCK_BUTTON) {
    setup_button(LOCK_BUTTON, lockAlert);
  }
#endif

fprintf(stderr,"GPIO: ENABLE_CW_BUTTONS=%d  CWL_BUTTON=%d CWR_BUTTON=%d\n",ENABLE_CW_BUTTONS, CWL_BUTTON, CWR_BUTTON);
  if(ENABLE_CW_BUTTONS) {
    setup_button(CWL_BUTTON, cwAlert);
    setup_button(CWR_BUTTON, cwAlert);
/*
    gpioSetMode(CWL_BUTTON, PI_INPUT);
    gpioSetAlertFunc(CWL_BUTTON, cwAlert);
    gpioSetMode(CWR_BUTTON, PI_INPUT);
    gpioSetAlertFunc(CWR_BUTTON, cwAlert);
    gpioGlitchFilter(CWL_BUTTON, 5000);
    gpioGlitchFilter(CWR_BUTTON, 5000);
*/
  }
 
#endif

#ifdef sx1509
  // override default (PI) values
  VFO_ENCODER_A=4;
  VFO_ENCODER_B=5;
  E1_ENCODER_A=6;
  E1_ENCODER_B=7;
  E2_ENCODER_A=12;
  E2_ENCODER_B=13;
  E3_ENCODER_A=14;
  E3_ENCODER_B=15;

  fprintf(stderr,"sx1509 encoder_init: VFO_ENCODER_A=%d VFO_ENCODER_B=%d\n",VFO_ENCODER_A,VFO_ENCODER_B);

  pSX1509 = newSX1509();

  // Call SX1509_begin(<address>) to initialize the SX1509. If it
  // successfully communicates, it'll return 1. 255 for soft reset
  if (!SX1509_begin(pSX1509, SX1509_ADDRESS, 255))
  {
    printf("Failed to communicate to sx1509 at %x.\n", SX1509_ADDRESS);
    return 1;
  }

  fprintf(stderr,"wiringPiSetup\n");
  if (wiringPiSetup () < 0) {
    printf ("Unable to setup wiringPi: %s\n", strerror (errno));
    return 1;
  }

  // Initialize the buttons
  // Sleep time off (0). 16ms scan time, 8ms debounce:
  SX1509_keypad(pSX1509, BTN_ROWS, BTN_COLS, 0, 16, 8);

  // Initialize the encoders
  SX1509_pinMode(pSX1509, VFO_ENCODER_A, INPUT_PULLUP);
  SX1509_pinMode(pSX1509, VFO_ENCODER_B, INPUT_PULLUP);
  SX1509_enableInterrupt(pSX1509, VFO_ENCODER_A, CHANGE);
  SX1509_enableInterrupt(pSX1509, VFO_ENCODER_B, CHANGE);
  vfoEncoderPos=0;
  SX1509_pinMode(pSX1509, E1_ENCODER_A, INPUT_PULLUP);
  SX1509_pinMode(pSX1509, E1_ENCODER_B, INPUT_PULLUP);
  SX1509_enableInterrupt(pSX1509, E1_ENCODER_A, CHANGE);
  SX1509_enableInterrupt(pSX1509, E1_ENCODER_B, CHANGE);
  e1EncoderPos=0;
  SX1509_pinMode(pSX1509, E2_ENCODER_A, INPUT_PULLUP);
  SX1509_pinMode(pSX1509, E2_ENCODER_B, INPUT_PULLUP);
  SX1509_enableInterrupt(pSX1509, E2_ENCODER_A, CHANGE);
  SX1509_enableInterrupt(pSX1509, E2_ENCODER_B, CHANGE);
  e2EncoderPos=0;
  SX1509_pinMode(pSX1509, E3_ENCODER_A, INPUT_PULLUP);
  SX1509_pinMode(pSX1509, E3_ENCODER_B, INPUT_PULLUP);
  SX1509_enableInterrupt(pSX1509, E3_ENCODER_A, CHANGE);
  SX1509_enableInterrupt(pSX1509, E3_ENCODER_B, CHANGE);
  e3EncoderPos=0;

  e1Function=0;
  e2Function=0;
  e3Function=0;

  pinMode(SX1509_INT_PIN, INPUT);
  pullUpDnControl(SX1509_INT_PIN, PUD_UP);

  if ( wiringPiISR (SX1509_INT_PIN, INT_EDGE_FALLING, &sx1509_interrupt) < 0 ) {
    printf ("Unable to setup ISR: %s\n", strerror (errno));
    return 1;
  }
#endif

#if defined odroid && !defined sx1509

    //VFO_ENCODER_A=ODROID_VFO_ENCODER_A;
    //VFO_ENCODER_B=ODROID_VFO_ENCODER_B;
    //VFO_ENCODER_A_PIN=ODROID_VFO_ENCODER_A_PIN;
    //VFO_ENCODER_B_PIN=ODROID_VFO_ENCODER_B_PIN;
 
    fprintf(stderr,"encoder_init: VFO_ENCODER_A=%d VFO_ENCODER_B=%d\n",VFO_ENCODER_A,VFO_ENCODER_B);

    fprintf(stderr,"wiringPiSetup\n");
    if (wiringPiSetup () < 0) {
      printf ("Unable to setup wiringPi: %s\n", strerror (errno));
      return -1;
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
      return -1;
    }

    if ( wiringPiISR (1, INT_EDGE_BOTH, &interruptA) < 0 ) {
      printf ("Unable to setup ISR: %s\n", strerror (errno));
      return -1;
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
#if defined odroid && !defined sx1509
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

int e1_encoder_get_pos() {
    int pos=e1EncoderPos;
    e1EncoderPos=0;
    return pos;
}

int e1_function_get_state() {
    return e1Function;
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

int agc_function_get_state() {
    return e3Function;
}

int e2_function_get_state() {
    return e2Function;
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

    // VFO
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    setFrequency(entry->frequencyA+ddsOffset+(pos*step));
    vfo_update(NULL);
  }
  free(data);
  return 0;
}

static int e1_encoder_changed(void *data) {
  int pos=*(int*)data;
  if(pos!=0) {
    if(function || isTransmitting()) {
      // mic gain
      double gain=mic_gain;
      //gain+=(double)pos/100.0;
      gain+=(double)pos;
      if(gain<-10.0) {
        gain=-10.0;
      } else if(gain>50.0) {
        gain=50.0;
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

static int e2_encoder_changed(void *data) {
  int pos=*(int*)data;
  if(pos!=0) {
    if(function || tune) {
      // tune drive
      double d=getTuneDrive();
      d+=(double)pos;
      if(d<0.0) {
        d=0.0;
      } else if(d>100.0) {
        d=100.0;
      }
      set_tune(d);
    } else {
      // drive
      double d=getDrive();
      d+=(double)pos;
      if(d<0.0) {
        d=0.0;
      } else if(d>100.0) {
        d=100.0;
      }
      set_drive(d);
    }
  }
  free(data);
  return 0;
}

static int e3_encoder_changed(void *data) {
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
  sim_s1_pressed_cb(NULL,NULL);
  return 0;
}

static int band_released(void *data) {
  sim_s1_released_cb(NULL,NULL);
  return 0;
}

static int bandstack_pressed(void *data) {
  sim_s2_pressed_cb(NULL,NULL);
  return 0;
}

static int bandstack_released(void *data) {
  sim_s2_released_cb(NULL,NULL);
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
  lock_cb((GtkWidget *)NULL, (gpointer)NULL);
  return 0;
}

static int mode_pressed(void *data) {
  sim_s3_cb(NULL,NULL);
  return 0;
}

static int filter_pressed(void *data) {
  sim_s4_cb(NULL,NULL);
  return 0;
}

static int noise_pressed(void *data) {
  sim_s5_cb(NULL,NULL);
  return 0;
}

static int agc_pressed(void *data) {
  sim_s6_cb(NULL,NULL);
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
        e1_function=e1_function_get_state();
        if(e1_function!=previous_e1_function) {
            previous_e1_function=e1_function;
        }
*/
        pos=e1_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(e1_encoder_changed,(gpointer)p);
        }

/*
        e2_function=e2_function_get_state();
        if(e2_function!=previous_e2_function) {
            previous_e2_function=e2_function;
        }
*/
        pos=e2_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(e2_encoder_changed,(gpointer)p);
        }

        pos=e3_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(e3_encoder_changed,(gpointer)p);
        }


        int band_button=band_get_state();
        if(band_button!=previous_band_button) {
            previous_band_button=band_button;
            if(band_button) {
                g_idle_add(band_pressed,(gpointer)NULL);
            } else {
                g_idle_add(band_released,(gpointer)NULL);
            }
        }

        int bandstack_button=bandstack_get_state();
        if(bandstack_button!=previous_bandstack_button) {
            previous_bandstack_button=bandstack_button;
            if(bandstack_button) {
                g_idle_add(bandstack_pressed,(gpointer)NULL);
            } else {
                g_idle_add(bandstack_released,(gpointer)NULL);
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

#ifdef sx1509
        // buttons only generate interrupt when
        // pushed on, so clear them now
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

#ifdef raspberrypi
          if(running) gpioDelay(100000); // 10 per second
#endif
#ifdef odroid
          if(running) usleep(100000);
#endif
    }
#ifdef raspberrypi
    gpioTerminate();
#endif
}
#endif
