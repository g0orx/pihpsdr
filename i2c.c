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
      fprintf(stderr,"i2c_interrupt: flags=%04X,ints=%04X\n",flags,ints);
      if(ints) {
        switch(ints) {
          case 0x0001:
            g_idle_add(ext_mox_update,NULL);
            break;
          case 0x0002:
            g_idle_add(ext_tune_update,NULL);
            break;
          case 0x0004:
            g_idle_add(ext_band_update,NULL);
            break;
          case 0x0008:
            g_idle_add(ext_band_update,(void *)band40);
            break;
          case 0x0010:
            g_idle_add(ext_band_update,(void *)band30);
            break;
          case 0x0020:
            g_idle_add(ext_band_update,(void *)band20);
            break;
          case 0x0040:
            g_idle_add(ext_band_update,(void *)band17);
            break;
          case 0x0080:
            g_idle_add(ext_band_update,(void *)band15);
            break;
          case 0x0100:
            g_idle_add(ext_band_update,(void *)band12);
            break;
          case 0x0200:
            g_idle_add(ext_band_update,(void *)band10);
            break;
          case 0x0400:
            g_idle_add(ext_band_update,(void *)band6);
            break;
          case 0x0800:
            g_idle_add(ext_band_update,(void *)bandGen);
            break;
          case 0x1000:
            g_idle_add(ext_band_update,(void *)band12);
            break;
          case 0x2000:
            g_idle_add(ext_band_update,(void *)band10);
            break;
          case 0x4000:
            g_idle_add(ext_band_update,(void *)band6);
            break;
          case 0x8000:
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
