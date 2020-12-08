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

#include <gtk/gtk.h>
#include <errno.h>
#include <gpiod.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "i2c_controller.h"
#include "vfo.h"
#include "radio.h"
#include "ext.h"
#include "sliders.h"
#include "new_menu.h"


char *encoder_string[ENCODER_ACTIONS] = {
  "NO ACTION",
  "AF GAIN",
  "AF GAIN RX1",
  "AF GAIN RX2",
  "AGC GAIN",
  "AGC GAIN RX1",
  "AGC GAIN RX2",
  "ATTENUATION/RX GAIN",
  "COMP",
  "CW FREQUENCY",
  "CW SPEED",
  "DIVERSITY GAIN",
  "DIVERSITY GAIN (coarse)",
  "DIVERSITY GAIN (fine)",
  "DIVERSITY PHASE",
  "DIVERSITY PHASE (coarse)",
  "DIVERSITY PHASE (fine)",
  "DRIVE",
  "IF SHIFT",
  "IF SHIFT RX1",
  "IF SHIFT RX2",
  "IF WIDTH",
  "IF WIDTH RX1",
  "IF WIDTH RX2",
  "MIC GAIN",
  "PAN",
  "PANADAPTER HIGH",
  "PANADAPTER LOW",
  "PANADAPTER STEP",
  "RF GAIN",
  "RF GAIN RX1",
  "RF GAIN RX2",
  "RIT",
  "RIT RX1",
  "RIT RX2",
  "SQUELCH",
  "SQUELCH RX1",
  "SQUELCH RX2",
  "TUNE DRIVE",
  "VFO",
  "WATERFALL HIGH",
  "WATERFALL LOW",
  "XIT",
  "ZOOM",
};

char *sw_string[SWITCH_ACTIONS] = {
  "NO ACTION",
  "A TO B",
  "A SWAP B",
  "AGC",
  "ANF",
  "B TO A",
  "BAND -",
  "BAND +",
  "BSTACK -",
  "BSTACK +",
  "CTUN",
  "DIV",
  "FILTER -",
  "FILTER +",
  "FUNCTION",
  "LOCK",
  "MENU BAND",
  "MENU BSTACK",
  "MENU DIV",
  "MENU FILTER",
  "MENU FREQUENCY",
  "MENU MEMORY",
  "MENU MODE",
  "MENU PS",
  "MODE -",
  "MODE +",
  "MOX",
  "MUTE",
  "NB",
  "NR",
  "PAN -",
  "PAN +",
  "PS",
  "RIT",
  "RIT CL",
  "SAT",
  "SNB",
  "SPLIT",
  "TUNE",
  "TWO TONE",
  "XIT",
  "XIT CL",
  "ZOOM -",
  "ZOOM +",
};

ENCODER encoder[MAX_ENCODERS]=
{
  {TRUE,0x11,0,ENCODER_AF_GAIN,TRUE,MODE_MENU,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION},
  {TRUE,0x12,0,ENCODER_AGC_GAIN,TRUE,FILTER_MENU,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION},
  {TRUE,0x13,0,ENCODER_RIT,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION},
  {TRUE,0x14,0,ENCODER_DRIVE,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION},
  {TRUE,0x15,0,ENCODER_IF_SHIFT,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION},
  {TRUE,0x16,0,ENCODER_IF_WIDTH,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION},
  {TRUE,0x10,0,ENCODER_VFO,TRUE,MENU_BAND,FALSE,NO_ACTION,FALSE,NO_ACTION,FALSE,NO_ACTION},
};

char *gpio_device="/dev/gpiochip0";
int new_i2c_interrupt=4;

static struct gpiod_chip *chip=NULL;
static struct gpiod_line *line=NULL;

static GMutex encoder_mutex;
static GThread *monitor_thread_id;

char *i2c_device="/dev/i2c-1";

static int fd=-1;

static GThread *rotary_encoder_thread_id;

static int vfo_encoder_changed(void *data) {
  if(!locked) {
    gint pos=(gint)data;
    vfo_step(pos);
  }
  return 0;
}

static gpointer rotary_encoder_thread(gpointer data) {
  while(1) {
    g_mutex_lock(&encoder_mutex);
    for(int i=0;i<MAX_ENCODERS;i++) {
      if(encoder[i].enabled) {
        if(encoder[i].pos!=0) {
          switch(encoder[i].encoder_function) {
            case ENCODER_VFO:
              if(active_menu==BAND_MENU) {
                // move to next/previous band
              } else {
                g_idle_add(vfo_encoder_changed,(gpointer)encoder[i].pos);
              }
              break;
            case ENCODER_AF_GAIN:
              {
              double af_gain=active_receiver->volume;
              af_gain+=((double)encoder[i].pos/100.0);
              if(af_gain<0.0) af_gain=0.0;
              if(af_gain>1.0) af_gain=1.0;
              double *dp=g_new(double,1);
              *dp=af_gain;
              g_idle_add(ext_set_af_gain,(gpointer)dp);
              }
              break;
            case ENCODER_AGC_GAIN:
              {
              double agc_gain=active_receiver->agc_gain;
              agc_gain+=(double)encoder[i].pos;
              if(agc_gain<-20.0) agc_gain=-20.0;
              if(agc_gain>120.0) agc_gain=120.0;
              double *dp=g_new(double,1);
              *dp=agc_gain;
              g_idle_add(ext_set_agc_gain,(gpointer)dp);
              }
              break;
            case ENCODER_DRIVE:
              {
              double drive=getDrive();
              drive+=(double)encoder[i].pos;
              if(drive<0.0) drive=0.0;
              if(drive>drive_max) drive=drive_max;
              double *dp=g_new(double,1);
              *dp=drive;
              g_idle_add(ext_set_drive,(gpointer)dp);
              }
              break;
          }
        }
        encoder[i].pos=0;
      }
    }
    g_mutex_unlock(&encoder_mutex);
    usleep(100000);
  }
}

int i2c_init() {
  int ret=0;

  g_mutex_init(&encoder_mutex);

  g_print("%s: open i2c device %s\n",__FUNCTION__,i2c_device);
  fd=open(i2c_device, O_RDWR);
  if(fd<0) {
    g_print("%s: open i2c device %s failed: %s\n",__FUNCTION__,i2c_device,g_strerror(errno));
    goto end;
  }
  g_print("%s: open i2c device %s fd=%d\n",__FUNCTION__,i2c_device,fd);

  for(int i=0;i<MAX_ENCODERS;i++) {
    if(encoder[i].enabled) {
 
      if (ioctl(fd, I2C_SLAVE, encoder[i].address) < 0) {
        g_print("%s: ioctl i2c slave %d failed: %s\n",__FUNCTION__,encoder[i].address,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }
  
      //g_print("%s: write config\n",__FUNCTION__);
      unsigned int config=INT_DATA | WRAP_DISABLE | DIRE_LEFT | IPUP_ENABLE | REL_MODE_DISABLE | RMOD_X1;
      if(i2c_smbus_write_byte_data(fd,REG_GCONF,config&0xFF)<0) {
        g_print("%s: write REG_GCONF config failed: addr=%02X %s\n",__FUNCTION__,encoder[i].address,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }
      if(i2c_smbus_write_byte_data(fd,REG_GCONF2,(config>>8)&0xFF)<0) {
        g_print("%s: write REG_GCONF2 config failed: addr=%02X %s\n",__FUNCTION__,encoder[i].address,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      int v=0; // initial value

      //g_print("%s: write counter value\n",__FUNCTION__);
      if(i2c_smbus_write_i2c_block_data(fd, REG_CVALB4, 4, (const __u8 *)&v)<0) {
        g_print("%s: counter CVALB1 config failed: %s\n",__FUNCTION__,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      v=1024; // max value

      //g_print("%s: write max value\n",__FUNCTION__);
      if(i2c_smbus_write_i2c_block_data(fd, REG_CMAXB4, 4, (const __u8 *)&v)<0) {
        g_print("%s: counter CMAXB1 config failed: %s\n",__FUNCTION__,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      v=-1024; // min value

      //g_print("%s: write min value\n",__FUNCTION__);
      if(i2c_smbus_write_i2c_block_data(fd, REG_CMINB4, 4, (const __u8 *)&v)<0) {
        g_print("%s: counter CMINB1 config failed: %s\n",__FUNCTION__,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      v=1; // step value

      //g_print("%s: write step value\n",__FUNCTION__);
      if(i2c_smbus_write_i2c_block_data(fd, REG_ISTEPB4, 4, (const __u8 *)&v)<0) {
        g_print("%s: counter CISTEPB4 config failed: %s\n",__FUNCTION__,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      v=8;

      //g_print("%s: write antibounce value\n",__FUNCTION__);
      if(i2c_smbus_write_byte_data(fd, REG_ANTBOUNC, v&0xFF)<0) {
        g_print("%s: counter REG_ANTBOUNC config failed: %s\n",__FUNCTION__,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      v=30;

      //g_print("%s: write double push value\n",__FUNCTION__);
      if(i2c_smbus_write_byte_data(fd, REG_DPPERIOD, v&0xFF)<0) {
        g_print("%s: counter REG_DPPERIOD config failed: %s\n",__FUNCTION__,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      v=PUSHR | PUSHP | PUSHD | RINC | RDEC /*| RMAX | RMIN | INT_2*/; 
      //g_print("%s: write interrupt config %02X\n",__FUNCTION__, v&0xFF);
      if(i2c_smbus_write_byte_data(fd,REG_INTCONF,v&0xFF)<0) {
        g_print("%s: counter CMINB1 config failed: %s\n",__FUNCTION__,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      //g_print("%s: write interrupt config %02X\n",__FUNCTION__, v&0xFF);
      if(i2c_smbus_write_byte_data(fd,REG_INTCONF,v&0xFF)<0) {
        g_print("%s: counter CMINB1 config failed: %s\n",__FUNCTION__,g_strerror(errno));
        encoder[i].enabled=FALSE;
        continue;
      }

      __s32 id=i2c_smbus_read_byte_data(fd,REG_IDCODE);
      g_print("%s: board addr=%02X id=%02X\n",__FUNCTION__,encoder[i].address,id&0xFF);

      __s32 ver=i2c_smbus_read_byte_data(fd,REG_VERSION);
      g_print("%s: board addr=%02X version=%02X\n",__FUNCTION__,encoder[i].address,ver&0xFF);
    }
  }

  rotary_encoder_thread_id = g_thread_new( "encoders", rotary_encoder_thread, NULL);
  if(!rotary_encoder_thread_id ) {
    g_print("%s: g_thread_new failed on rotary_encoder_thread\n",__FUNCTION__);
    ret=-1;
    goto error;
  }

  goto end;

error:
  if(fd>=0) {
    close(fd);
    fd=-1;
  }

end:
  return ret;
}

void i2c_close() {
  if(fd>0) {
    close(fd);
    fd=-1;
  }
}

static void encoder_switch_pushed(int i) {
  switch(encoder[i].push_sw_function) {
    case MENU_BAND:
      g_idle_add(ext_band_update,NULL);
      break;
  }
}

void i2c_interrupt(int line) {
  int length;

  g_print("%s: line=%d fd=%d\n",__FUNCTION__,line,fd);
  if(fd!=-1) {
    g_mutex_lock(&encoder_mutex);
    for(int i=0;i<MAX_ENCODERS;i++) {
      if(encoder[i].enabled) {
        if (ioctl(fd, I2C_SLAVE, encoder[i].address) < 0) {
          g_print("%s: ioctl i2c slave %d failed: %s\n",__FUNCTION__,encoder[i].address,g_strerror(errno));
          continue;
        }
        
        __s32 status=i2c_smbus_read_byte_data(fd,REG_ESTATUS);
        g_print("%s: address=%02X status=%02X\n",__FUNCTION__,encoder[i].address,status&0xFF);


        if(status&PUSHR) {
          //g_print("%s: PUSHR\n",__FUNCTION__);
          if(encoder[i].push_sw_enabled) {
            encoder_switch_pushed(i);
          }
        }
        if(status&PUSHP) {
          //g_print("%s: PUSHP\n",__FUNCTION__);
        }
        if(status&PUSHD) {
          //g_print("%s: PUSHD\n",__FUNCTION__);
        }
        if(status&RINC) {
          g_print("%s: RINC from %02X\n",__FUNCTION__,encoder[i].address);
          encoder[i].pos++;
/*
          __s32 v;
          __s32 bytes=i2c_smbus_read_i2c_block_data(fd, REG_CVALB4, 4, (__u8 *)&v);
          g_print("%s: val=%d\n",__FUNCTION__,v);
*/
        }
        if(status&RDEC) {
          g_print("%s: RDEC from %02X\n",__FUNCTION__,encoder[i].address);
          encoder[i].pos--;
/*
          __s32 v;
          __s32 bytes=i2c_smbus_read_i2c_block_data(fd, REG_CVALB4, 4, (__u8 *)&v);
          g_print("%s: val=%d\n",__FUNCTION__,v);
*/
        }
        if(status&RMAX) {
          //g_print("%s: RMAX\n",__FUNCTION__);
        }
        if(status&RMIN) {
          //g_print("%s: RMIN\n",__FUNCTION__);
        }
        if(status&INT_2) {
          //g_print("%s: INT_2\n",__FUNCTION__);
          __s32 i2status=i2c_smbus_read_byte_data(fd,REG_I2STATUS);
          //g_print("%s: i2status=%02X\n",__FUNCTION__,status&0xFF);
        }
      }
    }
    g_mutex_unlock(&encoder_mutex);
  }
  //g_print("%s: exit\n",__FUNCTION__);
}

static int interrupt_cb(int event_type, unsigned int line, const struct timespec *timeout, void* data) {
  g_print("%s: event=%d line=%d\n",__FUNCTION__,event_type,line);
  switch(event_type) {
    case GPIOD_CTXLESS_EVENT_CB_TIMEOUT:
      // timeout - ignore
      //g_print("%s: Ignore timeout\n",__FUNCTION__);
      break;
    case GPIOD_CTXLESS_EVENT_CB_RISING_EDGE:
      // not expected
      //g_print("%s: Ignore RISING EDGE\n",__FUNCTION__);
      break;
    case GPIOD_CTXLESS_EVENT_CB_FALLING_EDGE:
      // process
      //g_print("%s: Process FALLING EDGE\n",__FUNCTION__);
      i2c_interrupt(line);
      break;
  }
  return GPIOD_CTXLESS_EVENT_CB_RET_OK;
}

static gpointer monitor_thread(gpointer arg) {
  struct timespec t;

  // thread to monitor gpio events
  g_print("%s: start event monitor\n",__FUNCTION__);
  t.tv_sec=60;
  t.tv_nsec=0;

  int ret=gpiod_ctxless_event_monitor(gpio_device,GPIOD_CTXLESS_EVENT_FALLING_EDGE,new_i2c_interrupt,FALSE,"encoder",&t,NULL,interrupt_cb,NULL);
  if (ret<0) {
    g_print("%s: ctxless event monitor failed: %s\n",__FUNCTION__,g_strerror(errno));
  }

  g_print("%s: exit\n",__FUNCTION__);
  return NULL;
}

int gpio_init() {
  int ret=0;


  chip=NULL;
  line=NULL;

//g_print("%s: open gpio 0\n",__FUNCTION__);
  chip=gpiod_chip_open_by_number(0);
  if(chip==NULL) {
    g_print("%s: open chip failed: %s\n",__FUNCTION__,g_strerror(errno));
    ret=1;
    goto end;
  }

//g_print("%s: get line %d\n",__FUNCTION__,new_i2c_interrupt);
  line = gpiod_chip_get_line(chip, new_i2c_interrupt);
  if (!line) {
    g_print("%s: get line failed: %s\n",__FUNCTION__,g_strerror(errno));
    ret = -1;
    goto end;
  }

//g_print("%s: line request falling edge\n",__FUNCTION__);
  ret=gpiod_line_request_falling_edge_events(line,"encoder");
  if (ret<0) {
    g_print("%s: line request falling edge events failed: %s\n",__FUNCTION__,g_strerror(errno));
    ret = -1;
    goto end;
  }
  if(line!=NULL) {
    gpiod_line_release(line);
    line=NULL;
  }
  if(chip!=NULL) {
    gpiod_chip_close(chip);
    chip=NULL;
  }

  monitor_thread_id = g_thread_new( "gpiod monitor", monitor_thread, NULL);
  if(!monitor_thread_id ) {
    g_print("%s: g_thread_new failed for monitor_thread\n",__FUNCTION__);
  }
  return 0;

g_print("%s: end\n",__FUNCTION__);
end:
  if(line!=NULL) {
    gpiod_line_release(line);
    line=NULL;
   }
  if(chip!=NULL) {
    gpiod_chip_close(chip);
    chip=NULL;
  }
  return ret;
}

void gpio_close() {
  if(line!=NULL) gpiod_line_release(line);
  if(chip!=NULL) gpiod_chip_close(chip);
}



int i2c_controller_init() {
  int rc=0;

  rc=gpio_init();
  if(rc<0) goto end;

  rc=i2c_init();

end:
  return rc;
}
