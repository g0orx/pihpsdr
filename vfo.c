/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
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
#include <gdk/gdk.h>
#include <math.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "discovered.h"
#include "main.h"
#include "agc.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "frequency.h"
#include "new_protocol.h"
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "channel.h"
#include "toolbar.h"
#include "wdsp.h"
#include "new_menu.h"
#include "rigctl.h"
#include "ext.h"
#include "noise_menu.h"
#ifdef FREEDV
#include "freedv.h"
#endif

static GtkWidget *parent_window;
static int my_width;
static int my_height;

static GtkWidget *vfo_panel;
static cairo_surface_t *vfo_surface = NULL;

int steps[]={1,10,25,50,100,250,500,1000,2500,5000,6250,9000,10000,12500,15000,20000,25000,30000,50000,100000,0};
char *step_labels[]={"1Hz","10Hz","25Hz","50Hz","100Hz","250Hz","500Hz","1kHz","2.5kHz","5kHz","6.25kHz","9kHz","10kHz","12.5kHz","15kHz","20kHz","25kHz","30kHz","50kHz","100kHz",0};

static GtkWidget* menu=NULL;
static GtkWidget* band_menu=NULL;


static void vfo_save_bandstack() {
  BANDSTACK *bandstack=bandstack_get_bandstack(vfo[0].band);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[0].bandstack];
  entry->frequency=vfo[0].frequency;
  entry->mode=vfo[0].mode;
  entry->filter=vfo[0].filter;
}

void modesettings_save_state() {
  int i;
  char name[80];
  char value[80];

  for (i=0; i<MODES; i++) {
    sprintf(name,"modeset.%d.filter", i);
    sprintf(value,"%d", mode_settings[i].filter);
    setProperty(name,value);
    sprintf(name,"modeset.%d.nr", i);
    sprintf(value,"%d", mode_settings[i].nr);
    setProperty(name,value);
    sprintf(name,"modeset.%d.nr2", i);
    sprintf(value,"%d", mode_settings[i].nr2);
    setProperty(name,value);
    sprintf(name,"modeset.%d.nb", i);
    sprintf(value,"%d", mode_settings[i].nb);
    setProperty(name,value);
    sprintf(name,"modeset.%d.nb2", i);
    sprintf(value,"%d", mode_settings[i].nb2);
    setProperty(name,value);
    sprintf(name,"modeset.%d.anf", i);
    sprintf(value,"%d", mode_settings[i].anf);
    setProperty(name,value);
    sprintf(name,"modeset.%d.snb", i);
    sprintf(value,"%d", mode_settings[i].snb);
    setProperty(name,value);
  }
}

void modesettings_restore_state() {
  int i;
  char name[80];
  char *value;

  // set some reasonable defaults for the filters

  for (i=0; i<MODES; i++) {
    mode_settings[i].filter=filterF6;
    mode_settings[i].nr=0;
    mode_settings[i].nr2=0;
    mode_settings[i].nb=0;
    mode_settings[i].nb2=0;
    mode_settings[i].anf=0;
    mode_settings[i].snb=0;

    sprintf(name,"modeset.%d.filter",i);
    value=getProperty(name);
    if(value) mode_settings[i].filter=atoi(value);
    sprintf(name,"modeset.%d.nr",i);
    value=getProperty(name);
    if(value) mode_settings[i].nr=atoi(value);
    sprintf(name,"modeset.%d.nr2",i);
    value=getProperty(name);
    if(value) mode_settings[i].nr2=atoi(value);
    sprintf(name,"modeset.%d.nb",i);
    value=getProperty(name);
    if(value) mode_settings[i].nb=atoi(value);
    sprintf(name,"modeset.%d.nb2",i);
    value=getProperty(name);
    if(value) mode_settings[i].nb2=atoi(value);
    sprintf(name,"modeset.%d.anf",i);
    value=getProperty(name);
    if(value) mode_settings[i].anf=atoi(value);
    sprintf(name,"modeset.%d.snb",i);
    value=getProperty(name);
    if(value) mode_settings[i].snb=atoi(value);
  }
}

void vfo_save_state() {
  int i;
  char name[80];
  char value[80];

  vfo_save_bandstack();

  for(i=0;i<MAX_VFOS;i++) {
    sprintf(name,"vfo.%d.band",i);
    sprintf(value,"%d",vfo[i].band);
    setProperty(name,value);
    sprintf(name,"vfo.%d.frequency",i);
    sprintf(value,"%lld",vfo[i].frequency);
    setProperty(name,value);
    sprintf(name,"vfo.%d.ctun",i);
    sprintf(value,"%d",vfo[i].ctun);
    setProperty(name,value);
    sprintf(name,"vfo.%d.rit_enabled",i);
    sprintf(value,"%d",vfo[i].rit_enabled);
    setProperty(name,value);
    sprintf(name,"vfo.%d.rit",i);
    sprintf(value,"%lld",vfo[i].rit);
    setProperty(name,value);
    sprintf(name,"vfo.%d.lo",i);
    sprintf(value,"%lld",vfo[i].lo);
    setProperty(name,value);
    sprintf(name,"vfo.%d.lo_tx",i);
    sprintf(value,"%lld",vfo[i].lo_tx);
    setProperty(name,value);
    sprintf(name,"vfo.%d.ctun_frequency",i);
    sprintf(value,"%lld",vfo[i].ctun_frequency);
    setProperty(name,value);
    sprintf(name,"vfo.%d.offset",i);
    sprintf(value,"%lld",vfo[i].offset);
    setProperty(name,value);
    sprintf(name,"vfo.%d.mode",i);
    sprintf(value,"%d",vfo[i].mode);
    setProperty(name,value);
    sprintf(name,"vfo.%d.filter",i);
    sprintf(value,"%d",vfo[i].filter);
    setProperty(name,value);
  }
}

void vfo_restore_state() {
  int i;
  char name[80];
  char *value;

  for(i=0;i<MAX_VFOS;i++) {

    vfo[i].band=band20;
    vfo[i].bandstack=0;
    vfo[i].frequency=14010000;
#ifdef SOAPYSDR
    if(radio->protocol==SOAPYSDR_PROTOCOL) {
      vfo[i].frequency=144010000;
    }
#endif
    vfo[i].mode=modeCWU;
    vfo[i].filter=6;
    vfo[i].lo=0;
    vfo[i].offset=0;
    vfo[i].rit_enabled=0;
    vfo[i].rit=0;
    vfo[i].ctun=0;

    sprintf(name,"vfo.%d.band",i);
    value=getProperty(name);
    if(value) vfo[i].band=atoi(value);
    sprintf(name,"vfo.%d.frequency",i);
    value=getProperty(name);
    if(value) vfo[i].frequency=atoll(value);
    sprintf(name,"vfo.%d.ctun",i);
    value=getProperty(name);
    if(value) vfo[i].ctun=atoi(value);
    sprintf(name,"vfo.%d.ctun_frequency",i);
    value=getProperty(name);
    if(value) vfo[i].ctun_frequency=atoll(value);
    sprintf(name,"vfo.%d.rit",i);
    value=getProperty(name);
    if(value) vfo[i].rit=atoll(value);
    sprintf(name,"vfo.%d.rit_enabled",i);
    value=getProperty(name);
    if(value) vfo[i].rit_enabled=atoi(value);
    sprintf(name,"vfo.%d.lo",i);
    value=getProperty(name);
    if(value) vfo[i].lo=atoll(value);
    sprintf(name,"vfo.%d.offset",i);
    value=getProperty(name);
    if(value) vfo[i].offset=atoll(value);
    sprintf(name,"vfo.%d.mode",i);
    value=getProperty(name);
    if(value) vfo[i].mode=atoi(value);
    sprintf(name,"vfo.%d.filter",i);
    value=getProperty(name);
    if(value) vfo[i].filter=atoi(value);
    sprintf(name,"vfo.%d.lo_tx",i);
    value=getProperty(name);
    if(value) vfo[i].lo_tx=atoll(value);
    // Sanity check: if !ctun, offset must be zero
    if (!vfo[i].ctun) {
	vfo[i].offset=0;
    }
  }
}

void vfo_xvtr_changed() {
  if(vfo[0].band>=BANDS) {
    BAND *band=band_get_band(vfo[0].band);
    vfo[0].lo=band->frequencyLO+band->errorLO;
  }
  if(vfo[1].band>=BANDS) {
    BAND *band=band_get_band(vfo[1].band);
    vfo[1].lo=band->frequencyLO+band->errorLO;
  }
}

void vfo_band_changed(int b) {
  BANDSTACK *bandstack;

  int id=active_receiver->id;

  if(id==0) {
    vfo_save_bandstack();
  }
  if(b==vfo[id].band) {
    // same band selected - step to the next band stack
    bandstack=bandstack_get_bandstack(b);
    vfo[id].bandstack++;
    if(vfo[id].bandstack>=bandstack->entries) {
      vfo[id].bandstack=0;
    }
  } else {
    // new band - get band stack entry
    bandstack=bandstack_get_bandstack(b);
    vfo[id].bandstack=bandstack->current_entry;
  }

  BAND *band=band_get_band(b);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[id].bandstack];
  vfo[id].band=b;
  vfo[id].frequency=entry->frequency;
  vfo[id].mode=entry->mode;
  vfo[id].filter=entry->filter;
  vfo[id].lo=band->frequencyLO+band->errorLO;
  vfo[id].lo_tx=band->txFrequencyLO+band->txErrorLO;

  // turn off ctun
  vfo[id].ctun=0;

  switch(id) {
    case 0:
      bandstack->current_entry=vfo[id].bandstack;
      receiver_vfo_changed(receiver[id]);
      BAND *band=band_get_band(vfo[id].band);
      set_alex_rx_antenna(band->alexRxAntenna);
      if(can_transmit) {
        set_alex_tx_antenna(band->alexTxAntenna);
      }
      set_alex_attenuation(band->alexAttenuation);
      receiver_vfo_changed(receiver[0]);
      break;
   case 1:
      if(receivers==2) {
        receiver_vfo_changed(receiver[1]);
      }
      break;
  }

  if(can_transmit) {
    if(split) {
      tx_set_mode(transmitter,vfo[VFO_B].mode);
    } else {
      tx_set_mode(transmitter,vfo[VFO_A].mode);
    }
    //
    // If the band has changed, it is necessary to re-calculate
    // the drive level. Furthermore, possibly the "PA disable"
    // status has changed.
    //
    calcDriveLevel();  // sends HighPrio packet if in new protocol
  }
  if (protocol == NEW_PROTOCOL) {
    schedule_general();
  }
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_bandstack_changed(int b) {
  int id=active_receiver->id;
  if(id==0) {
    vfo_save_bandstack();
  }
  vfo[id].bandstack=b;

  BANDSTACK *bandstack=bandstack_get_bandstack(vfo[id].band);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[id].bandstack];
  vfo[id].frequency=entry->frequency;
  vfo[id].mode=entry->mode;
  vfo[id].filter=entry->filter;

  switch(id) {
    case 0:
      bandstack->current_entry=vfo[id].bandstack;
      receiver_vfo_changed(receiver[id]);
      BAND *band=band_get_band(vfo[id].band);
      set_alex_rx_antenna(band->alexRxAntenna);
      set_alex_tx_antenna(band->alexTxAntenna);
      set_alex_attenuation(band->alexAttenuation);
      receiver_vfo_changed(receiver[0]);
      break;
   case 1:
      if(receivers==2) {
        receiver_vfo_changed(receiver[1]);
      }
      break;
  }

  if(can_transmit) {
    if(split) {
      tx_set_mode(transmitter,vfo[VFO_B].mode);
    } else {
      tx_set_mode(transmitter,vfo[VFO_A].mode);
    }
  }
  //
  // I do not think the band can change within this function.
  // But out of paranoia, I consider this possiblity here
  //
  calcDriveLevel();  // sends HighPrio packet if in new protocol
  if (protocol == NEW_PROTOCOL) {
    schedule_general();		// for PA disable
  }
  g_idle_add(ext_vfo_update,NULL);

}

void vfo_mode_changed(int m) {
  int id=active_receiver->id;
  vfo[id].mode=m;
//
// Change to the filter/NR combination stored for this mode
//
  vfo[id].filter      =mode_settings[m].filter;
  active_receiver->nr =mode_settings[m].nr;
  active_receiver->nr2=mode_settings[m].nr2;
  active_receiver->nb =mode_settings[m].nb;
  active_receiver->nb2=mode_settings[m].nb2;
  active_receiver->anf=mode_settings[m].anf;
  active_receiver->snb=mode_settings[m].snb;

  // make changes effective
  update_noise();
  switch(id) {
    case 0:
      receiver_mode_changed(receiver[0]);
      receiver_filter_changed(receiver[0]);
      break;
    case 1:
      if(receivers==2) {
        receiver_mode_changed(receiver[1]);
        receiver_filter_changed(receiver[1]);
      }
      break;
  }
  if(can_transmit) {
    if(split) {
      tx_set_mode(transmitter,vfo[VFO_B].mode);
    } else {
      tx_set_mode(transmitter,vfo[VFO_A].mode);
    }
  }
  //
  // changing modes may change BFO frequency
  // and SDR need to be informed about "CW or not CW"
  //
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();		// update frequencies
    schedule_transmit_specific();	// update "CW" flag
  }
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_filter_changed(int f) {
  int id=active_receiver->id;

  // store changed filter in the mode settings
  mode_settings[vfo[id].mode].filter = f;

  vfo[id].filter=f;
  switch(id) {
    case 0:
      receiver_filter_changed(receiver[0]);
      break;
    case 1:
      if(receivers==2) {
        receiver_filter_changed(receiver[1]);
      }
      break;
  }

  g_idle_add(ext_vfo_update,NULL);
}

void vfo_a_to_b() {
  vfo[VFO_B].band=vfo[VFO_A].band;
  vfo[VFO_B].bandstack=vfo[VFO_A].bandstack;
  vfo[VFO_B].frequency=vfo[VFO_A].frequency;
  vfo[VFO_B].mode=vfo[VFO_A].mode;
  vfo[VFO_B].filter=vfo[VFO_A].filter;
  vfo[VFO_B].lo=vfo[VFO_A].lo;
  vfo[VFO_B].offset=vfo[VFO_A].offset;
  vfo[VFO_B].rit_enabled=vfo[VFO_A].rit_enabled;
  vfo[VFO_B].rit=vfo[VFO_A].rit;

  if(receivers==2) {
    receiver_vfo_changed(receiver[1]);
  }
  if(can_transmit) {
    if(split) {
      tx_set_mode(transmitter,vfo[VFO_B].mode);
    }
  }
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_b_to_a() {
  vfo[VFO_A].band=vfo[VFO_B].band;
  vfo[VFO_A].bandstack=vfo[VFO_B].bandstack;
  vfo[VFO_A].frequency=vfo[VFO_B].frequency;
  vfo[VFO_A].mode=vfo[VFO_B].mode;
  vfo[VFO_A].filter=vfo[VFO_B].filter;
  vfo[VFO_A].lo=vfo[VFO_B].lo;
  vfo[VFO_A].offset=vfo[VFO_B].offset;
  vfo[VFO_A].rit_enabled=vfo[VFO_B].rit_enabled;
  vfo[VFO_A].rit=vfo[VFO_B].rit;
  receiver_vfo_changed(receiver[0]);
  if(can_transmit) {
    if(!split) {
      tx_set_mode(transmitter,vfo[VFO_B].mode);
    }
  }
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_a_swap_b() {
  int temp_band;
  int temp_bandstack;
  long long temp_frequency;
  int temp_mode;
  int temp_filter;
  int temp_lo;
  int temp_offset;
  int temp_rit_enabled;
  int temp_rit;

  temp_band=vfo[VFO_A].band;
  temp_bandstack=vfo[VFO_A].bandstack;
  temp_frequency=vfo[VFO_A].frequency;
  temp_mode=vfo[VFO_A].mode;
  temp_filter=vfo[VFO_A].filter;
  temp_lo=vfo[VFO_A].lo;
  temp_offset=vfo[VFO_A].offset;
  temp_rit_enabled=vfo[VFO_A].rit_enabled;
  temp_rit=vfo[VFO_A].rit;

  vfo[VFO_A].band=vfo[VFO_B].band;
  vfo[VFO_A].bandstack=vfo[VFO_B].bandstack;
  vfo[VFO_A].frequency=vfo[VFO_B].frequency;
  vfo[VFO_A].mode=vfo[VFO_B].mode;
  vfo[VFO_A].filter=vfo[VFO_B].filter;
  vfo[VFO_A].lo=vfo[VFO_B].lo;
  vfo[VFO_A].offset=vfo[VFO_B].offset;
  vfo[VFO_A].rit_enabled=vfo[VFO_B].rit_enabled;
  vfo[VFO_A].rit=vfo[VFO_B].rit;

  vfo[VFO_B].band=temp_band;
  vfo[VFO_B].bandstack=temp_bandstack;
  vfo[VFO_B].frequency=temp_frequency;
  vfo[VFO_B].mode=temp_mode;
  vfo[VFO_B].filter=temp_filter;
  vfo[VFO_B].lo=temp_lo;
  vfo[VFO_B].offset=temp_offset;
  vfo[VFO_B].rit_enabled=temp_rit_enabled;
  vfo[VFO_B].rit=temp_rit;

  receiver_vfo_changed(receiver[0]);
  if(receivers==2) {
    receiver_vfo_changed(receiver[1]);
  }
  if(can_transmit) {
    if(split) {
      tx_set_mode(transmitter,vfo[VFO_B].mode);
    } else {
      tx_set_mode(transmitter,vfo[VFO_A].mode);
    }
  }
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_step(int steps) {
  int id=active_receiver->id;
  if(!locked) {
    if(vfo[id].ctun) {
      vfo[id].ctun_frequency=(vfo[id].ctun_frequency/step + steps)*step;
    } else {
      vfo[id].frequency=(vfo[id].frequency/step +steps)*step;
    }
    receiver_frequency_changed(active_receiver);
#ifdef INCLUDED
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    setFrequency(active_receiver->frequency+(steps*step));
#endif
    g_idle_add(ext_vfo_update,NULL);
  }
}
//
// DL1YCF: essentially a duplicate of vfo_step but
//         changing a specific VFO freq instead of
//         changing the VFO of the active receiver
//
void vfo_id_step(int id, int steps) {
  if(!locked) {
    if(vfo[id].ctun) {
      vfo[id].ctun_frequency=(vfo[id].ctun_frequency/step+steps)*step;
    } else {
      vfo[id].frequency=(vfo[id].frequency/step+steps)*step;
    }

    int sid=id==0?1:0;
    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if(vfo[sid].ctun) {
          vfo[sid].ctun_frequency=vfo[sid].ctun_frequency+(steps*step);
        } else {
          vfo[sid].frequency=vfo[sid].frequency+(steps*step);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrments and B increments
        if(vfo[sid].ctun) {
          vfo[sid].ctun_frequency=vfo[sid].ctun_frequency-(steps*step);
        } else {
          vfo[sid].frequency=vfo[sid].frequency-(steps*step);
        }
        break;
    }

    receiver_frequency_changed(active_receiver);
#ifdef INCLUDED
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    setFrequency(active_receiver->frequency+(steps*step));
#endif
    g_idle_add(ext_vfo_update,NULL);
  }
}

void vfo_move(long long hz,int round) {
  int id=active_receiver->id;
g_print("vfo_move: id=%d hz=%lld\n",id,hz);
  if(!locked) {
    if(vfo[id].ctun) {
      vfo[id].ctun_frequency=vfo[id].ctun_frequency+hz;
      if(round) {
         vfo[id].ctun_frequency=(vfo[id].ctun_frequency/step)*step;
      }
    } else {
      vfo[id].frequency=vfo[id].frequency-hz;
      if(round) {
         vfo[id].frequency=(vfo[id].frequency/step)*step;
      }
    }
    int sid=id==0?1:0;
    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if(vfo[sid].ctun) {
          vfo[sid].ctun_frequency=vfo[sid].ctun_frequency+hz;
          if(round) {
             vfo[sid].ctun_frequency=(vfo[sid].ctun_frequency/step)*step;
          }
        } else {
          vfo[sid].frequency=vfo[sid].frequency-hz;
          if(round) {
             vfo[sid].frequency=(vfo[sid].frequency/step)*step;
          }
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrments and B increments
        if(vfo[sid].ctun) {
          vfo[sid].ctun_frequency=((vfo[sid].ctun_frequency-hz)/step)*step;
          if(round) {
             vfo[sid].ctun_frequency=(vfo[sid].ctun_frequency/step)*step;
          }
        } else {
          vfo[sid].frequency=((vfo[sid].frequency+hz)/step)*step;
          if(round) {
             vfo[sid].ctun_frequency=(vfo[sid].ctun_frequency/step)*step;
          }
        }
        break;
    }
    receiver_frequency_changed(active_receiver);
    g_idle_add(ext_vfo_update,NULL);
  }
}

void vfo_move_to(long long hz) {
  // hz is the offset from the min frequency
  int id=active_receiver->id;
  long long offset=(hz/step)*step;
  long long half=(long long)(active_receiver->sample_rate/2);
  long long f=vfo[id].frequency-half+offset;
  long long diff; 

g_print("vfo_move_to: id=%d hz=%lld f=%lld\n",id,hz,f);

  if(!locked) {
    if(vfo[id].ctun) {
      diff=f-vfo[id].ctun_frequency;
      vfo[id].ctun_frequency=f;
      if(vfo[id].mode==modeCWL) {
        vfo[id].ctun_frequency+=cw_keyer_sidetone_frequency;
      } else if(vfo[id].mode==modeCWU) {
        vfo[id].ctun_frequency-=cw_keyer_sidetone_frequency;
      }
g_print("vfo_move_to: vfo=%d ctun_frequency=%lld diff=%lld\n",id,vfo[id].ctun_frequency,diff);
    } else {
      diff=f-vfo[id].frequency;
      vfo[id].frequency=f;
      if(vfo[id].mode==modeCWL) {
        vfo[id].frequency+=cw_keyer_sidetone_frequency;
      } else if(vfo[id].mode==modeCWU) {
        vfo[id].frequency-=cw_keyer_sidetone_frequency;
      }
g_print("vfo_move_to: vfo=%d frequency=%lld diff==%lld\n",id,vfo[id].frequency,diff);
    }

    int sid=id==0?1:0;
    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        f=vfo[sid].frequency-half+offset;
        // A and B increment and decrement together
        if(vfo[sid].ctun) {
          vfo[sid].ctun_frequency+=diff;
g_print("vfo_move_to: SAT vfo=%d ctun_frequency=%lld\n",sid,vfo[sid].ctun_frequency);
        } else {
          vfo[sid].frequency+=diff;
g_print("vfo_move_to: SAT vfo=%d frequency=%lld\n",sid,vfo[sid].frequency);
        }
        break;
      case RSAT_MODE:
        f=vfo[sid].frequency+half-offset;
        // A increments and B decrements or A decrements and B increments
        if(vfo[sid].ctun) {
          vfo[sid].ctun_frequency-=diff;
g_print("vfo_move_to: RSAT vfo=%d ctun_frequency=%lld\n",sid,vfo[sid].ctun_frequency);
        } else {
          vfo[sid].frequency-=diff;
g_print("vfo_move_to: RSAT vfo=%d frequency=%lld\n",sid,vfo[sid].frequency);
        }
        break;
    }

    receiver_vfo_changed(active_receiver);

#ifdef INCLUDED

    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();

#ifdef SOAPYSDR
    if(protocol==SOAPYSDR_PROTOCOL) {
      setFrequency((entry->frequency+active_receiver->dds_offset-hz)/step*step);
    } else {
#endif
      if(vfo[id].ctun) {
        setFrequency((active_receiver->frequency+hz)/step*step);
      } else {
        long long f=(active_receiver->frequency+active_receiver->dds_offset+hz)/step*step;
        if(vfo[active_receiver->id].mode==modeCWL) {
          f+=cw_keyer_sidetone_frequency;
        } else if(vfo[active_receiver->id].mode==modeCWU) {
          f-=cw_keyer_sidetone_frequency;
        }
        setFrequency(f);
      }
#ifdef SOAPYSDR
    }
#endif
#endif
    g_idle_add(ext_vfo_update,NULL);
  }
}

static gboolean
vfo_scroll_event_cb (GtkWidget      *widget,
               GdkEventScroll *event,
               gpointer        data)
{
  int i;
  if(event->direction==GDK_SCROLL_UP) {
    vfo_move(step,TRUE);
  } else {
    vfo_move(-step,TRUE);
  }
  return FALSE;
}


static gboolean vfo_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  if (vfo_surface)
    cairo_surface_destroy (vfo_surface);

  vfo_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));

  /* Initialize the surface to black */
  cairo_t *cr;
  cr = cairo_create (vfo_surface);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_paint (cr);
  cairo_destroy(cr);
  g_idle_add(ext_vfo_update,NULL);
  return TRUE;
}

static gboolean vfo_draw_cb (GtkWidget *widget,
 cairo_t   *cr,
 gpointer   data)
{
  cairo_set_source_surface (cr, vfo_surface, 0.0, 0.0);
  cairo_paint (cr);
  return FALSE;
}

void vfo_update() {
    
    int id=active_receiver->id;
    FILTER* band_filters=filters[vfo[id].mode];
    FILTER* band_filter=&band_filters[vfo[id].filter];
    if(vfo_surface) {
        char temp_text[32];
        cairo_t *cr;
        cr = cairo_create (vfo_surface);
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        cairo_paint (cr);

        cairo_select_font_face(cr, "FreeMono",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);

        char dv[32];
        strcpy(dv,"");
#ifdef FREEDV
        if(active_receiver->freedv) {
          sprintf(dv,"FreeDV %s", freedv_get_mode_string());
        }
#endif
        cairo_set_font_size(cr, 12);
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        cairo_move_to(cr, 5, 15);
        
        switch(vfo[id].mode) {
          case modeFMN:
            if(active_receiver->deviation==2500) {
              sprintf(temp_text,"%s 8k %s",mode_string[vfo[id].mode],dv);
            } else {
              sprintf(temp_text,"%s 16k %s",mode_string[vfo[id].mode],dv);
            }
            break;
          case modeCWL:
          case modeCWU:
            sprintf(temp_text,"%s %s %d wpm %d Hz",mode_string[vfo[id].mode],band_filter->title,cw_keyer_speed,cw_keyer_sidetone_frequency);
            break;
          case modeLSB:
          case modeUSB:
          case modeDSB:
          case modeAM:
            sprintf(temp_text,"%s %s %s",mode_string[vfo[id].mode],band_filter->title,dv);
            break;
          default:
            sprintf(temp_text,"%s %s",mode_string[vfo[id].mode],band_filter->title);
            break;
        }
        cairo_show_text(cr, temp_text);

	// In what follows, we want to display the VFO frequency
	// on which we currently transmit a signal with red colour.
	// If it is out-of-band, we display "Out of band" in red.
        // Frequencies we are not transmitting on are displayed in green
	// (dimmed if the freq. does not belong to the active receiver).
        // Depending on which receiver is the active one, and if we use split,
        // the following frequencies are used for transmitting (see old_protocol.c):
	// id == 0, split == 0 : TX freq = VFO_A
	// id == 0, split == 1 : TX freq = VFO_B
	// id == 1, split == 0 : TX freq = VFO_B
	// id == 1, split == 1 : TX freq = VFO_A


        long long af;
        if(isTransmitting() && !split) {
          if(vfo[0].ctun) {
            af=(double)(vfo[0].ctun_frequency-vfo[0].lo_tx);
          } else {
            af=(double)(vfo[0].frequency-vfo[0].lo_tx);
          }
        } else {
          if(vfo[0].ctun) {
            af=(double)(vfo[0].ctun_frequency);
          } else {
            af=(double)(vfo[0].frequency);
          }
        }

        sprintf(temp_text,"VFO A: %0lld.%06lld",af/(long long)1000000,af%(long long)1000000);
        if(isTransmitting() && ((id  == 0 && !split) || (id == 1 && split))) {
	    if (transmitter->out_of_band) sprintf(temp_text,"VFO A: Out of band");
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
            if(id==0) {
              cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
            } else {
              cairo_set_source_rgb(cr, 0.0, 0.65, 0.0);
            }
        }
        cairo_move_to(cr, 5, 38);  
        cairo_set_font_size(cr, 22); 
        cairo_show_text(cr, temp_text);


        long long bf;
        if(isTransmitting() && split) {
          if(vfo[1].ctun) {
            bf=(double)(vfo[1].ctun_frequency-vfo[1].lo_tx);
          } else {
            bf=(double)(vfo[1].frequency-vfo[1].lo_tx);
          }
        } else {
          if(vfo[1].ctun) {
            bf=(double)(vfo[1].ctun_frequency);
          } else {
            bf=(double)(vfo[1].frequency);
          }
        }
        sprintf(temp_text,"VFO B: %0lld.%06lld",bf/(long long)1000000,bf%(long long)1000000);
        if(isTransmitting() && ((id == 0 && split) || (id == 1 && !split))) {
	    if (transmitter->out_of_band) sprintf(temp_text,"VFO B: Out of band");
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
            if(id==1) {
              cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
            } else {
              cairo_set_source_rgb(cr, 0.0, 0.65, 0.0);
            }
        }
        cairo_move_to(cr, 300, 38);  
        cairo_show_text(cr, temp_text);

#ifdef PURESIGNAL
        if(can_transmit) {
          //cairo_move_to(cr, 180, 15);
          cairo_move_to(cr, 55, 50);
          if(transmitter->puresignal) {
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
          } else {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
          }
          cairo_set_font_size(cr, 12);
          cairo_show_text(cr, "PS");
        }
#endif


        if(vfo[id].rit_enabled==0) {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        } else {
            cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
        }
        sprintf(temp_text,"RIT: %lldHz",vfo[id].rit);
        //cairo_move_to(cr, 210, 15);
        cairo_move_to(cr, 170, 15);
        cairo_set_font_size(cr, 12);
        cairo_show_text(cr, temp_text);


        if(can_transmit) {
          if(transmitter->xit_enabled==0) {
              cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
          } else {
              cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
          }
          sprintf(temp_text,"XIT: %lldHz",transmitter->xit);
          cairo_move_to(cr, 310, 15);
          cairo_set_font_size(cr, 12);
          cairo_show_text(cr, temp_text);
        }

	// NB and NB2 are mutually exclusive, therefore
	// they are put to the same place in order to save
	// some space
        cairo_move_to(cr, 155, 50);
        if(active_receiver->nb) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
          cairo_show_text(cr, "NB");
        } else if (active_receiver->nb2) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
          cairo_show_text(cr, "NB2");
	} else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
          cairo_show_text(cr, "NB");
        }

	// NR and NR2 are mutually exclusive
        cairo_move_to(cr, 180, 50);  
        if(active_receiver->nr) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
          cairo_show_text(cr, "NR");
        } else if (active_receiver->nr2) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
          cairo_show_text(cr, "NR2");
	} else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
          cairo_show_text(cr, "NR");
        }

        cairo_move_to(cr, 210, 50);  
        if(active_receiver->anf) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "ANF");

        cairo_move_to(cr, 240, 50);  
        if(active_receiver->snb) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "SNB");

        //cairo_move_to(cr, 300, 50);  
        cairo_move_to(cr, 270, 50);  
        switch(active_receiver->agc) {
          case AGC_OFF:
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
            cairo_show_text(cr, "AGC OFF");
            break;
          case AGC_LONG:
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
            cairo_show_text(cr, "AGC LONG");
            break;
          case AGC_SLOW:
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
            cairo_show_text(cr, "AGC SLOW");
            break;
          case AGC_MEDIUM:
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
            cairo_show_text(cr, "AGC MED");
            break;
          case AGC_FAST:
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
            cairo_show_text(cr, "AGC FAST");
            break;
        }

	//
	// Since we can now change it by a MIDI controller,
	// we should display the compressor (level)
	//
        if(can_transmit) {
          //cairo_move_to(cr, 400, 50);  
          cairo_move_to(cr, 330, 50);  
  	  if (transmitter->compressor) {
  	      sprintf(temp_text,"CMPR %d dB",(int) transmitter->compressor_level);
              cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
              cairo_show_text(cr, temp_text);
	  } else {
              cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
              cairo_show_text(cr, "CMPR OFF");
	  }
        }

        cairo_move_to(cr, 500, 50);  
        if(diversity_enabled) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "DIV");

        int s=0;
        while(steps[s]!=step && steps[s]!=0) {
          s++;
        }
        sprintf(temp_text,"Step %s",step_labels[s]);
        //cairo_move_to(cr, 300, 15);
        cairo_move_to(cr, 400, 15);
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        cairo_show_text(cr, temp_text);

        char *info=getFrequencyInfo(af,active_receiver->filter_low,active_receiver->filter_high);
/*
        cairo_move_to(cr, (my_width/4)*3, 50);
        cairo_show_text(cr, getFrequencyInfo(af));
*/
         
        //cairo_move_to(cr, 400, 15);  
        cairo_move_to(cr, 430, 50);  
        if(vfo[id].ctun) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "CTUN");

        //cairo_move_to(cr, 450, 15);  
        cairo_move_to(cr, 470, 50);  
        if(cat_control>0) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "CAT");

        cairo_move_to(cr, 500, 15);  
        if(vox_enabled) {
          cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "VOX");

        cairo_move_to(cr, 5, 50);
        if(locked) {
          cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "Locked");

        //cairo_move_to(cr, 55, 50);
        cairo_move_to(cr, 260, 18);
        if(split) {
          cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "Split");

        //cairo_move_to(cr, 95, 50);
        cairo_move_to(cr, 260, 28);
        if(sat_mode!=SAT_NONE) {
          cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        if(sat_mode==SAT_NONE || sat_mode==SAT_MODE) {
          cairo_show_text(cr, "SAT");
        } else {
          cairo_show_text(cr, "RSAT");
        }


        if(duplex) {
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        sprintf(temp_text,"DUP");
        //cairo_move_to(cr, 130, 50);
        cairo_move_to(cr, 260, 38);
        cairo_set_font_size(cr, 12);
        cairo_show_text(cr, temp_text);

        cairo_destroy (cr);
        gtk_widget_queue_draw (vfo_panel);
    } else {
fprintf(stderr,"vfo_update: no surface!\n");
    }
}

/*
static gboolean
vfo_step_select_cb (GtkWidget *widget,
               gpointer        data)
{
  step=steps[(int)data];
  g_idle_add(ext_vfo_update,NULL);
}
*/

static gboolean
vfo_press_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
  start_vfo();
  return TRUE;
}

GtkWidget* vfo_init(int width,int height,GtkWidget *parent) {
  int i;

fprintf(stderr,"vfo_init: width=%d height=%d\n", width, height);

  parent_window=parent;
  my_width=width;
  my_height=height;

  vfo_panel = gtk_drawing_area_new ();
  gtk_widget_set_size_request (vfo_panel, width, height);

  g_signal_connect (vfo_panel,"configure-event",
            G_CALLBACK (vfo_configure_event_cb), NULL);
  g_signal_connect (vfo_panel, "draw",
            G_CALLBACK (vfo_draw_cb), NULL);

  /* Event signals */
  g_signal_connect (vfo_panel, "button-press-event",
            G_CALLBACK (vfo_press_event_cb), NULL);
  g_signal_connect(vfo_panel,"scroll_event",
            G_CALLBACK(vfo_scroll_event_cb),NULL);
  gtk_widget_set_events (vfo_panel, gtk_widget_get_events (vfo_panel)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_SCROLL_MASK);

  return vfo_panel;
}
