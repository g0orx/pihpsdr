#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include "i2c.h"
#include "gpio.h"
#include "band.h"
#include "band_menu.h"
#include "bandstack.h"
#include "radio.h"
#include "toolbar.h"
#include "vfo.h"
#include "ext.h"

#define I2C_DEVICE "/dev/i2c-1"
#define ADDRESS_1 0X20
#define ADDRESS_2 0X23

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

static int write_byte_data(unsigned char addr,unsigned char reg, unsigned char data) {
  int fd;
  int rc;

  if((fd=open(I2C_DEVICE, O_RDWR))<0) {
    fprintf(stderr,"cannot open %s: %s\n",I2C_DEVICE,strerror(errno));
    return(-1);
  }

  if(ioctl(fd,I2C_SLAVE,addr)<0) {
    fprintf(stderr,"cannot aquire access to I2C device at 0x%02X\n",addr);
    return(-1);
  }

  rc=i2c_smbus_write_byte_data(fd,reg,data);
  if(rc<0) {
    fprintf(stderr,"i2c_smbus_write_byte_data failed: device=%02X 0x%02X to 0x%02X: %s\n",addr,data,reg,strerror(errno));
    return(-1);
  }

  close(fd);
  
  return 0;
}

static unsigned char read_byte_data(unsigned char addr,unsigned char reg) {
  int fd;
  int rc;

  if((fd=open(I2C_DEVICE, O_RDWR))<0) {
    fprintf(stderr,"cannot open %s: %s\n",I2C_DEVICE,strerror(errno));
    exit(1);
  }

  if(ioctl(fd,I2C_SLAVE,addr)<0) {
    fprintf(stderr,"cannot aquire access to I2C device at 0x%x\n",addr);
    exit(1);
  }

  rc=i2c_smbus_read_byte_data(fd,reg);
  if(rc<0) {
    fprintf(stderr,"i2c_smbus_read_byte_data failed: 0x%2X: %s\n",reg,strerror(errno));
    exit(1);
  }

  close(fd);

  return rc;
}

static unsigned int read_word_data(unsigned char addr,unsigned char reg) {
  int fd;
  int rc;

  if((fd=open(I2C_DEVICE, O_RDWR))<0) {
    fprintf(stderr,"c$cannot open %s: %s\n",I2C_DEVICE,strerror(errno));
    exit(1);
  }

  if(ioctl(fd,I2C_SLAVE,addr)<0) {
    fprintf(stderr,"cannot aquire access to I2C device at 0x%x\n",addr);
    exit(1);
  }

  rc=i2c_smbus_read_word_data(fd,reg);
  if(rc<0) {
    fprintf(stderr,"i2c_smbus_read_word_data failed: 0x%2X: %s\n",reg,strerror(errno));
    exit(1);
  }

  close(fd);

  return rc;
}


static void frequencyStep(int pos) {
  vfo_step(pos);
}

void i2c_interrupt() {
  int flags;
  int ints;

  do {
    flags=read_word_data(ADDRESS_1,0x0E);
    if(flags) {
      ints=read_word_data(ADDRESS_1,0x10);
//g_print("i2c_interrupt: flags=%04X ints=%04X\n",flags,ints);
      if(ints) {
        int i=-1;
        switch(ints) {
          case SW_2:
            i=SW2;
            break;
          case SW_3:
            i=SW3;
            break;
          case SW_4:
            i=SW4;
            break;
          case SW_5:
            i=SW5;
            break;
          case SW_6:
            i=SW6;
            break;
          case SW_7:
            i=SW7;
            break;
          case SW_8:
            i=SW8;
            break;
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
          case SW_9:
            i=SW9;
            break;
          case SW_10:
            i=SW10;
            break;
          case SW_11:
            i=SW11;
            break;
          case SW_12:
            i=SW12;
            break;
          case SW_13:
            i=SW13;
            break;
          case SW_14:
            i=SW14;
            break;
          case SW_15:
            i=SW15;
            break;
          case SW_16:
            i=SW16;
            break;
          case SW_17:
            i=SW17;
            break;
#endif
        }
//g_print("i1c_interrupt: sw=%d action=%d\n",i,sw_action[i]);
        switch(sw_action[i]) {
          case TUNE:
            {
            int tune=getTune();
            if(tune==0) tune=1; else tune=0;
            g_idle_add(ext_tune_update,GINT_TO_POINTER(tune));
            }
            break;
          case MOX:
            {
            int mox=getMox();
            if(mox==0) mox=1; else mox=0;
            g_idle_add(ext_mox_update,GINT_TO_POINTER(mox));
            }
            break;
          case PS:
#ifdef PURESIGNAL
            g_idle_add(ext_ps_update,NULL);
#endif
            break;
          case TWO_TONE:
            g_idle_add(ext_two_tone,NULL);
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
            g_idle_add(ext_xit_update,NULL);
            break;
          case XIT_CLEAR:
            g_idle_add(ext_xit_clear,NULL);
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
            g_idle_add(ext_split_update,NULL);
            break;
        }
      }
    }
  } while(flags!=0);
}

void i2c_init() {

  int flags, ints;

fprintf(stderr,"i2c_init\n");
  // setup i2c
  if(write_byte_data(ADDRESS_1,0x0A,0x44)<0) return;
  if(write_byte_data(ADDRESS_1,0x0B,0x44)<0) return;

  // disable interrupt
  if(write_byte_data(ADDRESS_1,0x04,0x00)<0) return;
  if(write_byte_data(ADDRESS_1,0x05,0x00)<0) return;

  // clear defaults
  if(write_byte_data(ADDRESS_1,0x06,0x00)<0) return;
  if(write_byte_data(ADDRESS_1,0x07,0x00)<0) return;

  // OLAT
  if(write_byte_data(ADDRESS_1,0x14,0x00)<0) return;
  if(write_byte_data(ADDRESS_1,0x15,0x00)<0) return;

  // set GPIOA for pullups
  if(write_byte_data(ADDRESS_1,0x0C,0xFF)<0) return;
  if(write_byte_data(ADDRESS_1,0x0D,0xFF)<0) return;

  // reverse polarity
  if(write_byte_data(ADDRESS_1,0x02,0xFF)<0) return;
  if(write_byte_data(ADDRESS_1,0x03,0xFF)<0) return;

  // set GPIOA/B for input
  if(write_byte_data(ADDRESS_1,0x00,0xFF)<0) return;
  if(write_byte_data(ADDRESS_1,0x01,0xFF)<0) return;

  // INTCON
  if(write_byte_data(ADDRESS_1,0x08,0x00)<0) return;
  if(write_byte_data(ADDRESS_1,0x09,0x00)<0) return;

  // setup for an MCP23017 interrupt
  if(write_byte_data(ADDRESS_1,0x04,0xFF)<0) return;
  if(write_byte_data(ADDRESS_1,0x05,0xFF)<0) return;

  // flush any interrupts
  do {
    flags=read_word_data(ADDRESS_1,0x0E);
    if(flags) {
      ints=read_word_data(ADDRESS_1,0x10);
      fprintf(stderr,"flush interrupt: flags=%04X ints=%04X\n",flags,ints);
    }
  } while(flags!=0);
  
}
