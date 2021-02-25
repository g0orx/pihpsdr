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
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "new_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "vfo.h"
#include "channel.h"
#include "toolbar.h"
#include "wdsp.h"
#include "new_menu.h"
#include "rigctl.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "ext.h"

static GtkWidget *parent_window;
static int my_width;
static int my_height;

static GtkWidget *vfo_panel;
static cairo_surface_t *vfo_surface = NULL;

int steps[]={1,10,25,50,100,250,500,1000,5000,9000,10000,100000,250000,500000,1000000,0};
char *step_labels[]={"1Hz","10Hz","25Hz","50Hz","100Hz","250Hz","500Hz","1kHz","5kHz","9kHz","10kHz","100kHz","250KHz","500KHz","1MHz",0};

static GtkWidget* menu=NULL;
static GtkWidget* band_menu=NULL;

struct _vfo vfo[MAX_VFOS];
struct _mode_settings mode_settings[MODES];

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
g_print("vfo_restore_state: %d\n",i);

    vfo[i].band=band20;
    vfo[i].bandstack=0;
    vfo[i].frequency=14010000;
#ifdef SOAPYSDR
    if(radio->protocol==SOAPYSDR_PROTOCOL) {
      vfo[i].band=band144;
      vfo[i].bandstack=0;
      vfo[i].frequency=144010000;
    }
#endif
    vfo[i].mode=modeCWU;
    vfo[i].filter=filterF6;
    vfo[i].lo=0;
    vfo[i].offset=0;
    vfo[i].rit_enabled=0;
    vfo[i].rit=0;
    vfo[i].ctun=0;

g_print("vfo_restore_state: band=%d frequency=%lld\n",vfo[i].band,vfo[i].frequency);

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

void vfo_band_changed(int id,int b) {
  BANDSTACK *bandstack;

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_band(client_socket,id,b);
    return;
  }
#endif

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

  // turn off ctun
  vfo[id].ctun=0;
  vfo[id].ctun_frequency=0LL;
  vfo[id].offset=0;
  // tell WDSP about the offset
  set_offset(active_receiver, vfo[id].offset);

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
    tx_set_mode(transmitter,get_tx_mode());
    //
    // If the band has changed, it is necessary to re-calculate
    // the drive level. Furthermore, possibly the "PA disable"
    // status has changed.
    //
    calcDriveLevel();  // sends HighPrio packet if in new protocol
  }

  switch(protocol) {
    case NEW_PROTOCOL:
      schedule_general();
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      soapy_protocol_set_rx_frequency(active_receiver,id);
      break;
#endif
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
    tx_set_mode(transmitter,get_tx_mode());
    //
    // I do not think the band can change within this function.
    // But out of paranoia, I consider this possiblity here
    //
    calcDriveLevel();  // sends HighPrio packet if in new protocol
    if (protocol == NEW_PROTOCOL) {
      schedule_general();		// for PA disable
    }
  }
  g_idle_add(ext_vfo_update,NULL);

}

void vfo_mode_changed(int m) {
  int id=active_receiver->id;
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_mode(client_socket,id,m);
    return;
  }
#endif

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
  g_idle_add(ext_update_noise, NULL);
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
    tx_set_mode(transmitter,get_tx_mode());
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
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_filter(client_socket,id,f);
    return;
  }
#endif

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
    tx_set_mode(transmitter,get_tx_mode());
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
    tx_set_mode(transmitter,get_tx_mode());
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
    tx_set_mode(transmitter,get_tx_mode());
  }
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_step(int steps) {
  int id=active_receiver->id;
  long long delta;
  int sid;
  RECEIVER *other_receiver;

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    //send_vfo_step(client_socket,id,steps);
    update_vfo_step(id,steps);
    return;
  }
#endif

  if(!locked) {

    if(vfo[id].ctun) {
      // don't let ctun go beyond end of passband
      long long frequency=vfo[id].frequency;
      long long rx_low=((vfo[id].ctun_frequency/step + steps)*step)+active_receiver->filter_low;
      long long rx_high=((vfo[id].ctun_frequency/step + steps)*step)+active_receiver->filter_high;
      long long half=(long long)active_receiver->sample_rate/2LL;
      long long min_freq=frequency-half;
      long long max_freq=frequency+half;

      if(rx_low<=min_freq) {
        return;
      } else if(rx_high>=max_freq) {
        return;
      }

      delta=vfo[id].ctun_frequency;
      vfo[id].ctun_frequency=(vfo[id].ctun_frequency/step + steps)*step;
      delta=vfo[id].ctun_frequency - delta;
    } else {
      delta=vfo[id].frequency;
      vfo[id].frequency=(vfo[id].frequency/step + steps)*step;
      delta = vfo[id].frequency - delta;
    }

    sid=id==0?1:0;
    other_receiver=receiver[sid];
 
    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency += delta;
        } else {
          vfo[sid].frequency      += delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrments and B increments
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency -= delta;
        } else {
          vfo[sid].frequency      -= delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
    }
    receiver_frequency_changed(active_receiver);
    g_idle_add(ext_vfo_update,NULL);
  }
}
//
// DL1YCF: essentially a duplicate of vfo_step but
//         changing a specific VFO freq instead of
//         changing the VFO of the active receiver
//
void vfo_id_step(int id, int steps) {
  long long delta;
  int sid;
  RECEIVER *other_receiver;

  if(!locked) {
    if(vfo[id].ctun) {
      delta=vfo[id].ctun_frequency;
      vfo[id].ctun_frequency=(vfo[id].ctun_frequency/step+steps)*step;
      delta=vfo[id].ctun_frequency - delta;
    } else {
      delta=vfo[id].frequency;
      vfo[id].frequency=(vfo[id].frequency/step+steps)*step;
      delta = vfo[id].frequency - delta;
    }

    sid=id==0?1:0;
    other_receiver=receiver[sid];

    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency += delta;
        } else {
          vfo[sid].frequency      += delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrments and B increments
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency -= delta;
        } else {
          vfo[sid].frequency      -= delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
    }

    receiver_frequency_changed(active_receiver);
    g_idle_add(ext_vfo_update,NULL);
  }
}

void vfo_id_move(int id,long long hz,int round) {
  long long delta;
  int sid;
  RECEIVER *other_receiver;

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    //send_vfo_move(client_socket,id,hz,round);
    update_vfo_move(id,hz,round);
    return;
  }
#endif

  if(!locked) {
    if(vfo[id].ctun) {
      // don't let ctun go beyond end of passband
      long long frequency=vfo[id].frequency;
      long long rx_low=vfo[id].ctun_frequency+hz+active_receiver->filter_low;
      long long rx_high=vfo[id].ctun_frequency+hz+active_receiver->filter_high;
      long long half=(long long)active_receiver->sample_rate/2LL;
      long long min_freq=frequency-half;
      long long max_freq=frequency+half;

      if(rx_low<=min_freq) {
        return;
      } else if(rx_high>=max_freq) {
        return;
      }

      delta=vfo[id].ctun_frequency;
      vfo[id].ctun_frequency=vfo[id].ctun_frequency+hz;
      if(round && (vfo[id].mode!=modeCWL && vfo[id].mode!=modeCWU)) {
         vfo[id].ctun_frequency=(vfo[id].ctun_frequency/step)*step;
      }
      delta=vfo[id].ctun_frequency - delta;
    } else {
      delta=vfo[id].frequency;
      vfo[id].frequency=vfo[id].frequency-hz;
      if(round && (vfo[id].mode!=modeCWL && vfo[id].mode!=modeCWU)) {
         vfo[id].frequency=(vfo[id].frequency/step)*step;
      }
      delta = vfo[id].frequency - delta;
    }

    sid=id==0?1:0;
    other_receiver=receiver[sid];
 
    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency += delta;
        } else {
          vfo[sid].frequency      += delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrments and B increments
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency -= delta;
        } else {
          vfo[sid].frequency      -= delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
    }
    receiver_frequency_changed(receiver[id]);
    g_idle_add(ext_vfo_update,NULL);
  }
}

void vfo_move(long long hz,int round) {
  vfo_id_move(active_receiver->id,hz,round);
}

void vfo_move_to(long long hz) {
  // hz is the offset from the min displayed frequency
  int id=active_receiver->id;
  long long offset=hz;
  long long half=(long long)(active_receiver->sample_rate/2);
  long long f;
  long long delta;
  int sid;
  RECEIVER *other_receiver;

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_vfo_move_to(client_socket,id,hz);
    return;
  }
#endif

  if(vfo[id].mode!=modeCWL && vfo[id].mode!=modeCWU) {
    offset=(hz/step)*step;
  }
  f=(vfo[id].frequency-half)+offset+((double)active_receiver->pan*active_receiver->hz_per_pixel);

  if(!locked) {
    if(vfo[id].ctun) {
      delta=vfo[id].ctun_frequency;
      vfo[id].ctun_frequency=f;
      if(vfo[id].mode==modeCWL) {
        vfo[id].ctun_frequency+=cw_keyer_sidetone_frequency;
      } else if(vfo[id].mode==modeCWU) {
        vfo[id].ctun_frequency-=cw_keyer_sidetone_frequency;
      }
      delta=vfo[id].ctun_frequency - delta;
    } else {
      delta=vfo[id].frequency;
      vfo[id].frequency=f;
      if(vfo[id].mode==modeCWL) {
        vfo[id].frequency+=cw_keyer_sidetone_frequency;
      } else if(vfo[id].mode==modeCWU) {
        vfo[id].frequency-=cw_keyer_sidetone_frequency;
      }
      delta = vfo[id].frequency - delta;
    }

    sid=id==0?1:0;
    other_receiver=receiver[sid];

    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency += delta;
        } else {
          vfo[sid].frequency      += delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrements and B increments
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency -= delta;
        } else {
          vfo[sid].frequency      -= delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
    }

    receiver_vfo_changed(active_receiver);

    g_idle_add(ext_vfo_update,NULL);
  }
}

static gboolean
vfo_scroll_event_cb (GtkWidget      *widget,
               GdkEventScroll *event,
               gpointer        data)
{
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
    int txvfo=get_tx_vfo();

    FILTER* band_filters=filters[vfo[id].mode];
    FILTER* band_filter=&band_filters[vfo[id].filter];
    if(vfo_surface) {
        char temp_text[32];
        cairo_t *cr;
        cr = cairo_create (vfo_surface);
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        cairo_paint (cr);

        cairo_select_font_face(cr, "FreeSans",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);

        switch(vfo[id].mode) {
          case modeFMN:
            if(active_receiver->deviation==2500) {
              sprintf(temp_text,"%s 8k",mode_string[vfo[id].mode]);
            } else {
              sprintf(temp_text,"%s 16k",mode_string[vfo[id].mode]);
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
            sprintf(temp_text,"%s %s",mode_string[vfo[id].mode],band_filter->title);
            break;
          default:
            sprintf(temp_text,"%s %s",mode_string[vfo[id].mode],band_filter->title);
            break;
        }
        cairo_set_font_size(cr, 12);
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        cairo_move_to(cr, 5, 15);
        cairo_show_text(cr, temp_text);

	// In what follows, we want to display the VFO frequency
	// on which we currently transmit a signal with red colour.
	// If it is out-of-band, we display "Out of band" in red.
	// Frequencies we are not transmitting on are displayed in green
	// (dimmed if the freq. does not belong to the active receiver).

        // Frequencies of VFO A and B

        long long af = vfo[0].ctun ? vfo[0].ctun_frequency : vfo[0].frequency;
        long long bf = vfo[1].ctun ? vfo[1].ctun_frequency : vfo[1].frequency;

	if(vfo[0].entering_frequency) {
	    af=vfo[0].entered_frequency;
	}
	if(vfo[1].entering_frequency) {
	    bf=vfo[1].entered_frequency;
	}
        int oob=0;
        if (can_transmit) oob=transmitter->out_of_band;

        sprintf(temp_text,"VFO A: %0lld.%06lld",af/(long long)1000000,af%(long long)1000000);
        if(txvfo == 0 && (isTransmitting() || oob)) {
            if (oob) sprintf(temp_text,"VFO A: Out of band");
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
            if(vfo[0].entering_frequency) {
              cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
            } else if(id==0) {
              cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
            } else {
              cairo_set_source_rgb(cr, 0.0, 0.65, 0.0);
            }
        }
        cairo_move_to(cr, 5, 38);  
        cairo_set_font_size(cr, 22); 
        cairo_show_text(cr, temp_text);

        sprintf(temp_text,"VFO B: %0lld.%06lld",bf/(long long)1000000,bf%(long long)1000000);
        if(txvfo == 1 && (isTransmitting() || oob)) {
            if (oob) sprintf(temp_text,"VFO B: Out of band");
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
            if(vfo[1].entering_frequency) {
              cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
	    } else if(id==1) {
              cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
            } else {
              cairo_set_source_rgb(cr, 0.0, 0.65, 0.0);
            }
        }
        cairo_move_to(cr, 300, 38);  
        cairo_show_text(cr, temp_text);

#ifdef PURESIGNAL
        if(can_transmit) {
          cairo_move_to(cr, 130, 50);
          if(transmitter->puresignal) {
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
          } else {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
          }
          cairo_set_font_size(cr, 12);
          cairo_show_text(cr, "PS");
        }
#endif
        
        cairo_move_to(cr, 55, 50);
        if(active_receiver->zoom>1) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_set_font_size(cr, 12);
        sprintf(temp_text,"Zoom x%d",active_receiver->zoom);
        cairo_show_text(cr, temp_text);

        if(vfo[id].rit_enabled==0) {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        } else {
            cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
        }
        sprintf(temp_text,"RIT: %lldHz",vfo[id].rit);
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
        cairo_move_to(cr, 400, 15);
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        cairo_show_text(cr, temp_text);

        cairo_move_to(cr, 430, 50);  
        if(vfo[id].ctun) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "CTUN");

        cairo_move_to(cr, 470, 50);  
        if(cat_control>0) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "CAT");

        if(can_transmit) {
          cairo_move_to(cr, 500, 15);  
          if(vox_enabled) {
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
          } else {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
          }
          cairo_show_text(cr, "VOX");
        }

        cairo_move_to(cr, 5, 50);
        if(locked) {
          cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "Locked");

        cairo_move_to(cr, 260, 18);
        if(split) {
          cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "SPLIT");

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
  start_vfo(event->x<300?VFO_A:VFO_B);
  return TRUE;
}

GtkWidget* vfo_init(int width,int height,GtkWidget *parent) {

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

//
// Some utility functions to get characteristics of the current
// transmitter. These functions can be used even if there is no
// transmitter (transmitter->mode may segfault).
//

int get_tx_vfo() {
  int txvfo=active_receiver->id;
  if (split) txvfo = 1 - txvfo;
  return txvfo;
}

int get_tx_mode() {
  int txvfo=active_receiver->id;
  if (split) txvfo = 1 - txvfo;
  if (can_transmit) {
    return vfo[txvfo].mode;
  } else {
    return modeUSB;
  }
}

long long get_tx_freq() {
  int txvfo=active_receiver->id;
  if (split) txvfo = 1 - txvfo;
  if (vfo[txvfo].ctun) {
    return  vfo[txvfo].ctun_frequency;
  } else {
    return vfo[txvfo].frequency;
  }
}
void vfo_rit_update(int rx) {
  vfo[receiver[rx]->id].rit_enabled=vfo[receiver[rx]->id].rit_enabled==1?0:1;
  receiver_frequency_changed(receiver[rx]);
  g_idle_add(ext_vfo_update, NULL);
}

void vfo_rit_clear(int rx) {
  vfo[receiver[rx]->id].rit=0;
  receiver_frequency_changed(receiver[rx]);
  g_idle_add(ext_vfo_update, NULL);
}

void vfo_rit(int rx,int i) {
  double value=(double)vfo[receiver[rx]->id].rit;
  value+=(double)(i*rit_increment);
  if(value<-10000.0) {
    value=-10000.0;
  } else if(value>10000.0) {
    value=10000.0;
  }
  vfo[receiver[rx]->id].rit=(int)value;
  receiver_frequency_changed(receiver[rx]);
  g_idle_add(ext_vfo_update,NULL);
}

