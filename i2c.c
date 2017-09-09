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

static GThread *i2c_thread_id;

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

static void *i2c_thread(void *arg) {
  int rc_1_a;
  int rc_1_b;
  int prev_rc_1_a=0;
  int prev_rc_1_b=0;
  int rc_2_a;
  int rc_2_b;
  int prev_rc_2_a=0;
  int prev_rc_2_b=0;

  while(1) {
    rc_1_a=read_byte_data(ADDRESS_1,0x12);
    if(rc_1_a!=0 && rc_1_a!=prev_rc_1_a) {
      fprintf(stderr,"Dev 1: GPIOA: 0x%02X\n",rc_1_a);
      switch(rc_1_a) {
        case 0x01:
          g_idle_add(ext_band_update,(void *)band160);
          break;
        case 0x02:
          g_idle_add(ext_band_update,(void *)band80);
          break;
        case 0x04:
          g_idle_add(ext_band_update,(void *)band60);
          break;
        case 0x08:
          g_idle_add(ext_band_update,(void *)band40);
          break;
        case 0x10:
          g_idle_add(ext_band_update,(void *)band30);
          break;
        case 0x20:
          g_idle_add(ext_band_update,(void *)band20);
          break;
        case 0x40:
          g_idle_add(ext_band_update,(void *)band17);
          break;
        case 0x80:
          g_idle_add(ext_band_update,(void *)band15);
          break;
      }
    }
    prev_rc_1_a=rc_1_a;

    rc_1_b=read_byte_data(ADDRESS_1,0x13);
    if(rc_1_b!=0 && rc_1_b!=prev_rc_1_b) {
      fprintf(stderr,"Dev 1: GPIOB: 0x%02X\n",rc_1_b);
      switch(rc_1_b) {
        case 0x01:
          g_idle_add(ext_band_update,(void *)band12);
          break;
        case 0x02:
          g_idle_add(ext_band_update,(void *)band10);
          break;
        case 0x04:
          g_idle_add(ext_band_update,(void *)band6);
          break;
        case 0x08:
          g_idle_add(ext_band_update,(void *)bandGen);
          break;
        case 0x10:
          frequencyStep(+1);
          break;
        case 0x20:
          break;
        case 0x40:
          break;
        case 0x80:
          frequencyStep(-1);
          break;
      }
    }
    prev_rc_1_b=rc_1_b;

    rc_2_a=read_byte_data(ADDRESS_2,0x12);
    if(rc_2_a!=0 && rc_2_a!=prev_rc_2_a) {
      fprintf(stderr,"Dev 2: GPIOA: 0x%02X\n",rc_2_a);
      switch(rc_2_a) {
        case 0x01:
          break;
        case 0x02:
          mox_cb(NULL,NULL);
          break;
        case 0x04:
          tune_cb(NULL,NULL);
          break;
        case 0x08:
          break;
        case 0x10:
          break;
        case 0x20:
          break;
        case 0x40:
          break;
        case 0x80:
          break;
      }
    }
    prev_rc_2_a=rc_2_a;

    rc_2_b=read_byte_data(ADDRESS_2,0x13);
    if(rc_2_b!=0 && rc_2_b!=prev_rc_2_b) {
      fprintf(stderr,"Dev 2: GPIOB: 0x%02X\n",rc_2_b);
      switch(rc_2_b) {
        case 0x01:
          break;
        case 0x02:
          break;
        case 0x04:
          break;
        case 0x08:
          break;
        case 0x10:
          break;
        case 0x20:
          break;
        case 0x40:
          break;
        case 0x80:
          break;
      }
    }
    prev_rc_2_b=rc_2_b;

    usleep(200);
  }

}

void i2c_interrupt() {
  int flags;
  int ints;

  do {
    flags=read_word_data(ADDRESS_1,0x0E);
    if(flags) {
      ints=read_word_data(ADDRESS_1,0x10);
      fprintf(stderr,"i2c_interrupt: flags=%04X,ints=%04X\n",flags,ints);
      if(ints) {
        switch(ints) {
          case 0x0001: // 160
            g_idle_add(ext_band_update,(void *)band160);
            break;
          case 0x0002: // 80
            g_idle_add(ext_band_update,(void *)band80);
            break;
          case 0x0004: // 60
            g_idle_add(ext_band_update,(void *)band60);
            break;
          case 0x0008: // 40
            g_idle_add(ext_band_update,(void *)band40);
            break;
          case 0x0010: // 30
            g_idle_add(ext_band_update,(void *)band30);
            break;
          case 0x0020: // 20
            g_idle_add(ext_band_update,(void *)band20);
            break;
          case 0x0040: // 17
            g_idle_add(ext_band_update,(void *)band17);
            break;
          case 0x0080: // 15
            g_idle_add(ext_band_update,(void *)band15);
            break;
          case 0x8000: // 12
            g_idle_add(ext_band_update,(void *)band12);
            break;
          case 0x4000: // 10
            g_idle_add(ext_band_update,(void *)band10);
            break;
          case 0x2000: // 6
            g_idle_add(ext_band_update,(void *)band6);
            break;
          case 0x1000: // Gen
            g_idle_add(ext_band_update,(void *)bandGen);
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
