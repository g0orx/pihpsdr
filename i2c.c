#ifdef GPIO
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include "i2c.h"
#include "actions.h"
#include "gpio.h"
#include "band.h"
#include "band_menu.h"
#include "bandstack.h"
#include "radio.h"
#include "toolbar.h"
#include "vfo.h"
#include "ext.h"

char *i2c_device="/dev/i2c-1";
unsigned int i2c_address_1=0X20;
unsigned int i2c_address_2=0X23;

static int fd;

#define SW_2  0X8000
#define SW_3  0X4000
#define SW_4  0X2000
#define SW_5  0X1000
#define SW_6  0X0008
#define SW_7  0X0004
#define SW_8  0X0002
#define SW_9  0X0001
#define SW_10 0X0010
#define SW_11 0X0020
#define SW_12 0X0040
#define SW_13 0X0080
#define SW_14 0X0800
#define SW_15 0X0400
#define SW_16 0X0200
#define SW_17 0X0100

unsigned int i2c_sw[16]=
    { SW_2,SW_3,SW_4,SW_5,SW_6,SW_14,SW_15,SW_13,
      SW_12,SW_11,SW_10,SW_9,SW_7,SW_8,SW_16,SW_17 };

static int write_byte_data(unsigned char reg, unsigned char data) {
  int rc;

  if(i2c_smbus_write_byte_data(fd,reg,data&0xFF)<0) {
    g_print("%s: write REG_GCONF config failed: addr=%02X %s\n",__FUNCTION__,i2c_address_1,g_strerror(errno));
  }
  
  return rc;
}

static unsigned char read_byte_data(unsigned char reg) {
  __s32 data;

  data=i2c_smbus_read_byte_data(fd,reg);
  return data&0xFF;
}

static unsigned int read_word_data(unsigned char reg) {
  __s32 data;

  data=i2c_smbus_read_word_data(fd,reg);
  return data&0xFFFF;
}


static void frequencyStep(int pos) {
  vfo_step(pos);
}

static uint64_t epochMilli;

static void initialiseEpoch() {
  struct timespec ts ;

  clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
  epochMilli = (uint64_t)ts.tv_sec * (uint64_t)1000    + (uint64_t)(ts.tv_nsec / 1000000L) ;
}

static uint32_t millis () {
  uint64_t now ;
  struct  timespec ts ;
  clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
  now  = (uint64_t)ts.tv_sec * (uint64_t)1000 + (uint64_t)(ts.tv_nsec / 1000000L) ;
  return (uint32_t)(now - epochMilli) ;
}

static uint32_t debounce_time=50; // 50ms
static uint32_t t;
static uint32_t debounce=0;

void i2c_interrupt() {
  unsigned int flags;
  unsigned int ints;

  t=millis();
  do {
    flags=read_word_data(0x0E);
    if(flags) {
      ints=read_word_data(0x10);
g_print("%s: flags=%04X ints=%04X\n",__FUNCTION__,flags,ints);
      if(t<debounce) return;
      if(ints) {
        int i;
        for(i=0;i<16;i++) {
          if(i2c_sw[i]==ints) break;
        }
        if(i<16) {
g_print("%s: switches=%p sw=%d action=%d\n",__FUNCTION__,switches,i,switches[i].switch_function);
          PROCESS_ACTION *a=g_new(PROCESS_ACTION,1);
          a->action=switches[i].switch_function;
          a->mode=PRESSED;
          g_idle_add(process_action,a);
        }
      }
    }
  } while(flags!=0);
  debounce=t+debounce_time;
}

void i2c_init() {

  int flags, ints;

  g_print("%s: open i2c device %s\n",__FUNCTION__,i2c_device);
  fd=open(i2c_device, O_RDWR);
  if(fd<0) {
    g_print("%s: open i2c device %s failed: %s\n",__FUNCTION__,i2c_device,g_strerror(errno));
    return;
  }
  g_print("%s: open i2c device %s fd=%d\n",__FUNCTION__,i2c_device,fd);

  if (ioctl(fd, I2C_SLAVE, i2c_address_1) < 0) {
    g_print("%s: ioctl i2c slave %d failed: %s\n",__FUNCTION__,i2c_address_1,g_strerror(errno));
    return;
  }

  // setup i2c
  if(write_byte_data(0x0A,0x44)<0) return;
  if(write_byte_data(0x0B,0x44)<0) return;

  // disable interrupt
  if(write_byte_data(0x04,0x00)<0) return;
  if(write_byte_data(0x05,0x00)<0) return;

  // clear defaults
  if(write_byte_data(0x06,0x00)<0) return;
  if(write_byte_data(0x07,0x00)<0) return;

  // OLAT
  if(write_byte_data(0x14,0x00)<0) return;
  if(write_byte_data(0x15,0x00)<0) return;

  // set GPIOA for pullups
  if(write_byte_data(0x0C,0xFF)<0) return;
  if(write_byte_data(0x0D,0xFF)<0) return;

  // reverse polarity
  if(write_byte_data(0x02,0xFF)<0) return;
  if(write_byte_data(0x03,0xFF)<0) return;

  // set GPIOA/B for input
  if(write_byte_data(0x00,0xFF)<0) return;
  if(write_byte_data(0x01,0xFF)<0) return;

  // INTCON
  if(write_byte_data(0x08,0x00)<0) return;
  if(write_byte_data(0x09,0x00)<0) return;

  // setup for an MCP23017 interrupt
  if(write_byte_data(0x04,0xFF)<0) return;
  if(write_byte_data(0x05,0xFF)<0) return;

  // flush any interrupts
  int count=0;
  do {
    flags=read_word_data(0x0E);
    if(flags) {
      ints=read_word_data(0x10);
      fprintf(stderr,"flush interrupt: flags=%04X ints=%04X\n",flags,ints);
      count++;
      if(count==10) {
        return;
      }
    }
  } while(flags!=0);

  initialiseEpoch();
  
}
#endif
