#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>

#include <gtk/gtk.h>
#include "i2c.h"
#include "band.h"
#include "band_menu.h"
#include "bandstack.h"
#include "radio.h"
#include "toolbar.h"
#include "vfo.h"

#define I2C_DEVICE "/dev/i2c-1"
#define ADDRESS_1 0X20
#define ADDRESS_2 0X23

//static pthread_t i2c_thread_id;
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
    fprintf(stderr,"c$cannot open %s: %s\n",I2C_DEVICE,strerror(errno));
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

static void frequencyStep(int pos) {
  BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
  setFrequency(entry->frequency+ddsOffset+(pos*step));
  vfo_update(NULL);
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
          g_idle_add(band_update,(void *)band160);
          break;
        case 0x02:
          g_idle_add(band_update,(void *)band80);
          break;
        case 0x04:
          g_idle_add(band_update,(void *)band60);
          break;
        case 0x08:
          g_idle_add(band_update,(void *)band40);
          break;
        case 0x10:
          g_idle_add(band_update,(void *)band30);
          break;
        case 0x20:
          g_idle_add(band_update,(void *)band20);
          break;
        case 0x40:
          g_idle_add(band_update,(void *)band17);
          break;
        case 0x80:
          g_idle_add(band_update,(void *)band15);
          break;
      }
    }
    prev_rc_1_a=rc_1_a;

    rc_1_b=read_byte_data(ADDRESS_1,0x13);
    if(rc_1_b!=0 && rc_1_b!=prev_rc_1_b) {
      fprintf(stderr,"Dev 1: GPIOB: 0x%02X\n",rc_1_b);
      switch(rc_1_b) {
        case 0x01:
          g_idle_add(band_update,(void *)band12);
          break;
        case 0x02:
          g_idle_add(band_update,(void *)band10);
          break;
        case 0x04:
          g_idle_add(band_update,(void *)band6);
          break;
        case 0x08:
          g_idle_add(band_update,(void *)bandGen);
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

void i2c_init() {

fprintf(stderr,"i2c_init\n");
  // setup i2c
  if(write_byte_data(ADDRESS_1,0x0A,0x22)<0) return;

  // set GPIOA/B for input
  if(write_byte_data(ADDRESS_1,0x00,0xFF)<0) return;
  if(write_byte_data(ADDRESS_1,0x01,0xFF)<0) return;

  // set GPIOA for pullups
  if(write_byte_data(ADDRESS_1,0x0C,0xFF)<0) return;
  if(write_byte_data(ADDRESS_1,0x0D,0xFF)<0) return;

  // reverse polarity
  if(write_byte_data(ADDRESS_1,0x02,0xFF)<0) return;
  if(write_byte_data(ADDRESS_1,0x03,0xFF)<0) return;

  // setup i2c
  if(write_byte_data(ADDRESS_2,0x0A,0x22)<0) return;

  // set GPIOA/B for input
  if(write_byte_data(ADDRESS_2,0x00,0xFF)<0) return;
  if(write_byte_data(ADDRESS_2,0x01,0xFF)<0) return;

  // set GPIOA for pullups
  if(write_byte_data(ADDRESS_2,0x0C,0xFF)<0) return;
  if(write_byte_data(ADDRESS_2,0x0D,0xFF)<0) return;

  // reverse polarity
  if(write_byte_data(ADDRESS_2,0x02,0xFF)<0) return;
  if(write_byte_data(ADDRESS_2,0x03,0xFF)<0) return;

/*
  int rc;
  rc=pthread_create(&i2c_thread_id,NULL,i2c_thread,NULL);
  if(rc != 0) {
    fprintf(stderr,"i2c_init: pthread_create failed on i2c_thread: rc=%d\n", rc);
  }
*/
  i2c_thread_id = g_thread_new( "i2c", i2c_thread, NULL);
  if( ! i2c_thread_id )
  {
    fprintf(stderr,"g_thread_new failed on i2c_thread\n");
  }


}
