/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>
#include "main.h"
#include "discovery.h"
#include "receiver.h"
#include "sliders.h"
#include "toolbar.h"
#include "band_menu.h"
#include "diversity_menu.h"
#include "vfo.h"
#include "radio.h"
#include "radio_menu.h"
#include "new_menu.h"
#include "new_protocol.h"
#ifdef PURESIGNAL
#include "ps_menu.h"
#endif
#include "agc.h"
#include "filter.h"
#include "mode.h"
#include "band.h"
#include "bandstack.h"
#include "noise_menu.h"
#include "wdsp.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "ext.h"
#include "zoompan.h"
#include "equalizer_menu.h"


// The following calls functions can be called usig g_idle_add

int ext_vfo_mode_changed(void * data)
{
  int mode=GPOINTER_TO_INT(data);
  vfo_mode_changed(mode);
  return 0;
}

int ext_discovery(void *data) {
  discovery();
  return 0;
}

void local_set_frequency(int v,long long f) {
  int b=get_band_from_frequency(f);
  if(active_receiver->id==v) {
    if (b != vfo[v].band) {
      vfo_band_changed(active_receiver->id,b);
    }
    setFrequency(f);
  } else if(v==VFO_B) {
    // just  changing VFO-B frequency
    vfo[v].frequency=f;
    vfo[v].band=b;
    if(receivers==2) {
      // need to change the receiver frequency
    }
  }
}

int ext_set_frequency(void *data) {
  //
  // If new frequency is outside of current band,
  // behave as if the user had chosen the new band
  // via the menu prior to changing the frequency
  //
  SET_FREQUENCY *set_frequency=(SET_FREQUENCY *)data;
g_print("ext_set_frequency: vfo=%d freq=%lld\n",set_frequency->vfo,set_frequency->frequency);
  local_set_frequency(set_frequency->vfo,set_frequency->frequency);
  free(data);
  return 0;
}

static guint vfo_timeout=0;

static int vfo_timeout_cb(void * data) {
  vfo_timeout=0;
  vfo_update();
  return 0;
}

//
// ALL calls to vfo_update should go through g_idle_add(ext_vfo_update)
// such that they can be filtered out if they come at high rate
//
int ext_vfo_update(void *data) {
  if (vfo_timeout ==0) {
    vfo_timeout=g_timeout_add(100, vfo_timeout_cb, NULL);
  }
  return 0;
}

int ext_vfo_filter_changed(void *data) {
  vfo_filter_changed(GPOINTER_TO_INT(data));
  return 0;
}

int ext_band_update(void *data) {
  if(data==NULL) {
    start_band();
  } else {
    band_select_cb(NULL,data);
  }
  return 0;
}

int ext_bandstack_update(void *data) {
  start_bandstack();
  return 0;
}

int ext_mode_update(void *data) {
  start_mode();
  return 0;
}

int ext_filter_update(void *data) {
  start_filter();
  return 0;
}

int ext_frequency_update(void *data) {
  start_vfo(active_receiver->id);
  return 0;
}

int ext_memory_update(void *data) {
  start_store();
  return 0;
}

int ext_noise_update(void *data) {
  start_noise();
  return 0;
}

int ext_mox_update(void *data) {
  mox_update(GPOINTER_TO_INT(data));
  return 0;
}

int ext_tune_update(void *data) {
  tune_update(GPOINTER_TO_INT(data));
  return 0;
}

int ext_vox_changed(void *data) {
  vox_changed(GPOINTER_TO_INT(data));
  return 0;
}

int ext_update_agc_gain(void *data) {
  update_agc_gain(GPOINTER_TO_INT(data));
  free(data);
  return 0;
}

int ext_update_af_gain(void *data) {
  update_af_gain();
  return 0;
}

int ext_calc_drive_level(void *data) {
  calcDriveLevel();
  return 0;
}

int ext_vfo_band_changed(void *data) {
  int b=GPOINTER_TO_INT(data);
  vfo_band_changed(active_receiver->id,b);
  return 0;
}

int ext_radio_change_sample_rate(void *data) {
  radio_change_sample_rate(GPOINTER_TO_INT(data));
  return 0;
}

int ext_update_squelch(void *data) {
  set_squelch();
  return 0;
}

int ext_sliders_update(void *data) {
  sliders_update();
  return 0;
}

#ifdef PURESIGNAL
int ext_tx_set_ps(void *data) {
  if(can_transmit) {
    int state=GPOINTER_TO_INT(data);
    tx_set_ps(transmitter, state);
  }
  return 0;
}
#endif

int ext_update_vfo_step(void *data) {
  int direction=GPOINTER_TO_INT(data);
  int i=0;
  while(steps[i]!=step && steps[i]!=0) {
    i++;
  }

  if(steps[i]!=0) {
    if(direction>0) {
      i++;
      if(steps[i]!=0) {
        step=steps[i];
      }
    } else {
      i--;
      if(i>=0) {
        step=steps[i];
      }
    }
  }
  g_idle_add(ext_vfo_update, NULL);
  return 0;
}

int ext_vfo_step(void *data) {
  int step=GPOINTER_TO_INT(data);
  vfo_step(step);
  return 0;
}

int ext_vfo_id_step(void *data) {
  int *ip=(int *) data;
  int id=ip[0];
  int step=ip[1];
  vfo_id_step(id,step);
  free(data);
  return 0;
}

int ext_set_mic_gain(void * data) {
  double d=*(double *)data;
  set_mic_gain(d);
  free(data);
  return 0;
}

int ext_set_agc_gain(void *data) {
  double d=*(double *)data;
  set_agc_gain(active_receiver->id,d);
  free(data);
  return 0;
}
 
int ext_set_drive(void *data) {
  double d=*(double *)data;
  set_drive(d);
  free(data);
  return 0;
}

int ext_set_compression(void *data) {
  if(can_transmit) {
    set_compression(transmitter);
  }
  return 0;
}

int ext_vfo_a_swap_b(void *data) {
  vfo_a_swap_b();
  return 0;
}

int ext_vfo_a_to_b(void *data) {
  vfo_a_to_b();
  return 0;
}

int ext_vfo_b_to_a(void *data) {
  vfo_b_to_a();
  return 0;
}

int ext_update_att_preamp(void *data) {
  update_att_preamp();
  return 0;
}

int ext_set_alex_attenuation(void *data) {
  int val=GPOINTER_TO_INT(data);
  BAND *band=band_get_band(vfo[VFO_A].band);
  // store changed attenuation in "band" info
  band->alexAttenuation=val;
  set_alex_attenuation();
  return 0;
}

int ext_set_attenuation_value(void *data) {
  double d=*(double *)data;
  set_attenuation_value(d);
  free(data);
  return 0;
}


#ifdef PURESIGNAL
int ext_ps_update(void *data) {
  if(can_transmit) {
    if(transmitter->puresignal==0) {
      tx_set_ps(transmitter,1);
    } else {
      tx_set_ps(transmitter,0);
    }
  }
  return 0;
}
#endif

int ext_two_tone(void *data) {
  if(can_transmit) {
    int state=transmitter->twotone?0:1;
    tx_set_twotone(transmitter,state);
  }
  return 0;
}

int ext_nr_update(void *data) {
  if(active_receiver->nr==0 && active_receiver->nr2==0) {
    active_receiver->nr=1;
    active_receiver->nr2=0;
    mode_settings[vfo[active_receiver->id].mode].nr=1;
    mode_settings[vfo[active_receiver->id].mode].nr2=0;
  } else if(active_receiver->nr==1 && active_receiver->nr2==0) {
    active_receiver->nr=0;
    active_receiver->nr2=1;
    mode_settings[vfo[active_receiver->id].mode].nr=0;
    mode_settings[vfo[active_receiver->id].mode].nr2=1;
  } else if(active_receiver->nr==0 && active_receiver->nr2==1) {
    active_receiver->nr=0;
    active_receiver->nr2=0;
    mode_settings[vfo[active_receiver->id].mode].nr=0;
    mode_settings[vfo[active_receiver->id].mode].nr2=0;
  }
  update_noise();
  return 0;
}

int ext_nb_update(void *data) {
  if(active_receiver->nb==0 && active_receiver->nb2==0) {
    active_receiver->nb=1;
    active_receiver->nb2=0;
    mode_settings[vfo[active_receiver->id].mode].nb=1;
    mode_settings[vfo[active_receiver->id].mode].nb2=0;
  } else if(active_receiver->nb==1 && active_receiver->nb2==0) {
    active_receiver->nb=0;
    active_receiver->nb2=1;
    mode_settings[vfo[active_receiver->id].mode].nb=0;
    mode_settings[vfo[active_receiver->id].mode].nb2=1;
  } else if(active_receiver->nb==0 && active_receiver->nb2==1) {
    active_receiver->nb=0;
    active_receiver->nb2=0;
    mode_settings[vfo[active_receiver->id].mode].nb=0;
    mode_settings[vfo[active_receiver->id].mode].nb2=0;
  }
  update_noise();
  return 0;
}

int ext_snb_update(void *data) {
  if(active_receiver->snb==0) {
    active_receiver->snb=1;
    mode_settings[vfo[active_receiver->id].mode].snb=1;
  } else {
    active_receiver->snb=0;
    mode_settings[vfo[active_receiver->id].mode].snb=0;
  }
  update_noise();
  return 0;
}

void band_plus(int id) {
  long long frequency_min=radio->frequency_min;
  long long frequency_max=radio->frequency_max;
  int b=vfo[id].band;
  BAND *band;
  int found=0;
  while(!found) {
    b++;
    if(b>=BANDS+XVTRS) b=0;
    band=(BAND*)band_get_band(b);
    if(strlen(band->title)>0) {
      if(b<BANDS) {
        if(!(band->frequencyMin==0.0 && band->frequencyMax==0.0)) {
          if(band->frequencyMin<frequency_min || band->frequencyMax>frequency_max) {
            continue;
          }
        }
      }
      vfo_band_changed(id,b);
      found=1;
    }
  }
}

int ext_band_plus(void *data) {
  band_plus(active_receiver->id);
  return 0;
}


void band_minus(int id) {
  long long frequency_min=radio->frequency_min;
  long long frequency_max=radio->frequency_max;
  int b=vfo[id].band;
  BAND *band;
  int found=0;
  while(!found) {
    b--;
    if(b<0) b=BANDS+XVTRS-1;
    band=(BAND*)band_get_band(b);
    if(strlen(band->title)>0) {
      if(b<BANDS) {
        if(band->frequencyMin<frequency_min || band->frequencyMax>frequency_max) {
          continue;
        }
      }
      vfo_band_changed(id,b);
      found=1;
    }
  }
}

int ext_band_minus(void *data) {
  band_minus(active_receiver->id);
  return 0;
}

int ext_bandstack_plus(void *data) {
  BAND *band=band_get_band(vfo[active_receiver->id].band);
  BANDSTACK *bandstack=band->bandstack;
  int b=vfo[active_receiver->id].bandstack+1;
  if(b>=bandstack->entries) b=0;
  vfo_bandstack_changed(b);
  return 0;
}

int ext_bandstack_minus(void *data) {
  BAND *band=band_get_band(vfo[active_receiver->id].band);
  BANDSTACK *bandstack=band->bandstack;
  int b=vfo[active_receiver->id].bandstack-1;
  if(b<0) b=bandstack->entries-1;;
  vfo_bandstack_changed(b);
  return 0;
}

int ext_lock_update(void *data) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_lock(client_socket,locked==1?0:1);
  } else {
#endif
    locked=locked==1?0:1;
    g_idle_add(ext_vfo_update, NULL);
#ifdef CLIENT_SERVER
  }
#endif
  return 0;
}

int ext_rit_update(void *data) {
  vfo_rit_update(active_receiver->id);
  return 0;
}

int ext_rit_clear(void *data) {
  vfo_rit_clear(active_receiver->id);
  return 0;
}

int ext_xit_update(void *data) {
  if(can_transmit) {
    transmitter->xit_enabled=transmitter->xit_enabled==1?0:1;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
  }
  g_idle_add(ext_vfo_update, NULL);
  return 0;
}

int ext_xit_clear(void *data) {
  if(can_transmit) {
    transmitter->xit=0;
    g_idle_add(ext_vfo_update, NULL);
  }
  return 0;
}

int ext_filter_plus(void *data) {
  int f=vfo[active_receiver->id].filter-1;
  if(f<0) f=FILTERS-1;
  vfo_filter_changed(f);
  return 0;
}

int ext_filter_minus(void *data) {
  int f=vfo[active_receiver->id].filter+1;
  if(f>=FILTERS) f=0;
  vfo_filter_changed(f);
  return 0;
}

int ext_mode_plus(void *data) {
  int mode=vfo[active_receiver->id].mode;
  mode++;
  if(mode>=MODES) mode=0;
  vfo_mode_changed(mode);
  return 0;
}

int ext_mode_minus(void *data) {
  int mode=vfo[active_receiver->id].mode;
  mode--;
  if(mode<0) mode=MODES-1;
  vfo_mode_changed(mode);
  return 0;
}

void ctun_update(int id,int state) {
  vfo[id].ctun=state;
  if(!vfo[id].ctun) {
    vfo[id].offset=0;
  }
  vfo[id].ctun_frequency=vfo[id].frequency;
  set_offset(receiver[id],vfo[id].offset);
}

int ext_ctun_update(void *data) {
  ctun_update(active_receiver->id,vfo[active_receiver->id].ctun==1?0:1);
  g_idle_add(ext_vfo_update, NULL);
  return 0;
}

int ext_agc_update(void *data) {
  active_receiver->agc++;
  if(active_receiver->agc>+AGC_LAST) {
    active_receiver->agc=0;
  }
  set_agc(active_receiver, active_receiver->agc);
  g_idle_add(ext_vfo_update, NULL);
  return 0;
}

int ext_set_split(void *data) {
  if(can_transmit) {
    split=GPOINTER_TO_INT(data),
    tx_set_mode(transmitter,get_tx_mode());
    set_alex_tx_antenna();
    calcDriveLevel();
    g_idle_add(ext_vfo_update, NULL);
  }
  return 0;
}

int ext_split_toggle(void *data) {
  if(can_transmit) {
    split=split==1?0:1;
    tx_set_mode(transmitter,get_tx_mode());
    set_alex_tx_antenna();
    calcDriveLevel();
    g_idle_add(ext_vfo_update, NULL);
  }
  return 0;
}

int ext_start_rx(void *data) {
  start_rx();
  return 0;
}

int ext_start_tx(void *data) {
  start_tx();
  return 0;
}

int ext_diversity_update(void *data) {
  int menu=GPOINTER_TO_INT(data);
  if(menu) {
    start_diversity();
  } else {
    diversity_enabled=diversity_enabled==1?0:1;
    if (protocol == NEW_PROTOCOL) {
      schedule_high_priority();
      schedule_receive_specific();
    }
    g_idle_add(ext_vfo_update, NULL);
  }
  return 0;
}

int ext_diversity_change_gain(void *data) {
  double *dp = (double *) data;
  update_diversity_gain(*dp);
  free(dp);
  return 0;
}

int ext_diversity_change_phase(void *data) {
  double *dp = (double *) data;
  update_diversity_phase(*dp);
  free(dp);
  return 0;
}

#ifdef PURESIGNAL
int ext_start_ps(void *data) {
  start_ps();
  return 0;
}
#endif

int ext_sat_update(void *data) {
  switch(sat_mode) {
    case SAT_NONE:
      sat_mode=SAT_MODE;
      break;
    case SAT_MODE:
      sat_mode=RSAT_MODE;
      break;
    case RSAT_MODE:
      sat_mode=SAT_NONE;
      break;
  }
  g_idle_add(ext_vfo_update, NULL);
  return 0;
}

int ext_function_update(void *data) {
  function++;
  if(function>MAX_FUNCTION) {
    function=0;
  }
  update_toolbar_labels();
  g_idle_add(ext_vfo_update, NULL);
  return 0;
}

int ext_set_rf_gain(void *data) {
  int pos=GPOINTER_TO_INT(data);
  double value;
  value=(double)pos;
  if(value<-12.0) {
    value=-12.0;
  } else if(value>48.0) {
    value=48.0;
  }
  set_rf_gain(active_receiver->id,value);
  return 0;
}

int ext_update_noise(void *data) {
  update_noise();
  return 0;
}

int ext_update_eq(void *data) {
  update_eq();
  return 0;
}

int ext_set_duplex(void *data) {
  setDuplex();
  return 0;
}

#ifdef CLIENT_SERVER
int ext_remote_command(void *data) {
  HEADER *header=(HEADER *)data;
  REMOTE_CLIENT *client=header->context.client;
  int temp;
  switch(ntohs(header->data_type)) {
    case CMD_RESP_RX_FREQ:
      {
      FREQ_COMMAND *freq_command=(FREQ_COMMAND *)data;
      temp=active_receiver->pan;
      int vfo=freq_command->id;
      long long f=ntohll(freq_command->hz);
      local_set_frequency(vfo,f);
      vfo_update();
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      if(temp!=active_receiver->pan) {
        send_pan(client->socket,active_receiver->id,active_receiver->pan);
      }
      }
      break;
    case CMD_RESP_RX_STEP:
      {
      STEP_COMMAND *step_command=(STEP_COMMAND *)data;
      temp=active_receiver->pan;
      short steps=ntohs(step_command->steps);
      vfo_step(steps);
      //send_vfo_data(client,VFO_A);
      //send_vfo_data(client,VFO_B);
      if(temp!=active_receiver->pan) {
        send_pan(client->socket,active_receiver->id,active_receiver->pan);
      }
      }
      break;
    case CMD_RESP_RX_MOVE:
      {
      MOVE_COMMAND *move_command=(MOVE_COMMAND *)data;
      temp=active_receiver->pan;
      long long hz=ntohll(move_command->hz);
      vfo_move(hz,move_command->round);
      //send_vfo_data(client,VFO_A);
      //send_vfo_data(client,VFO_B);
      if(temp!=active_receiver->pan) {
        send_pan(client->socket,active_receiver->id,active_receiver->pan);
      }
      }
      break;
    case CMD_RESP_RX_MOVETO:
      {
      MOVE_TO_COMMAND *move_to_command=(MOVE_TO_COMMAND *)data;
      temp=active_receiver->pan;
      long long hz=ntohll(move_to_command->hz);
      vfo_move_to(hz);
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      if(temp!=active_receiver->pan) {
        send_pan(client->socket,active_receiver->id,active_receiver->pan);
      }
      }
      break;
    case CMD_RESP_RX_ZOOM:
      {
      ZOOM_COMMAND *zoom_command=(ZOOM_COMMAND *)data;
      temp=ntohs(zoom_command->zoom);
      set_zoom(zoom_command->id,(double)temp);
      send_zoom(client->socket,active_receiver->id,active_receiver->zoom);
      send_pan(client->socket,active_receiver->id,active_receiver->pan);
      }
      break;
    case CMD_RESP_RX_PAN:
      {
      PAN_COMMAND *pan_command=(PAN_COMMAND *)data;
      temp=ntohs(pan_command->pan);
      set_pan(pan_command->id,(double)temp);
      send_pan(client->socket,active_receiver->id,active_receiver->pan);
      }
      break;
    case CMD_RESP_RX_VOLUME:
      {
      VOLUME_COMMAND *volume_command=(VOLUME_COMMAND *)data;
      temp=ntohs(volume_command->volume);
      set_af_gain(volume_command->id,(double)temp/100.0);
      }
      break;
    case CMD_RESP_RX_AGC:
      {
      AGC_COMMAND *agc_command=(AGC_COMMAND *)data;
      RECEIVER *rx=receiver[agc_command->id];
      rx->agc=ntohs(agc_command->agc);
g_print("AGC_COMMAND: set_agc id=%d agc=%d\n",rx->id,rx->agc);
      set_agc(rx,rx->agc);
      send_agc(client->socket,rx->id,rx->agc);
      g_idle_add(ext_vfo_update, NULL);
      }
      break;
    case CMD_RESP_RX_AGC_GAIN:
      {
      AGC_GAIN_COMMAND *agc_gain_command=(AGC_GAIN_COMMAND *)data;
      temp=ntohs(agc_gain_command->gain);
      set_agc_gain(agc_gain_command->id,(double)temp);
      RECEIVER *rx=receiver[agc_gain_command->id];
      send_agc_gain(client->socket,rx->id,(int)rx->agc_gain,(int)rx->agc_hang,(int)rx->agc_thresh);
      }
      break;
    case CMD_RESP_RX_ATTENUATION:
      {
      ATTENUATION_COMMAND *attenuation_command=(ATTENUATION_COMMAND *)data;
      temp=ntohs(attenuation_command->attenuation);
      set_attenuation(temp);
      }
      break;
    case CMD_RESP_RX_SQUELCH:
      {
      SQUELCH_COMMAND *squelch_command=(SQUELCH_COMMAND *)data;
      receiver[squelch_command->id]->squelch_enable=squelch_command->enable;
      temp=ntohs(squelch_command->squelch);
      receiver[squelch_command->id]->squelch=(double)temp;
      set_squelch(receiver[squelch_command->id]);
      }
      break;
    case CMD_RESP_RX_NOISE:
      {
      NOISE_COMMAND *noise_command=(NOISE_COMMAND *)data;
      RECEIVER *rx=receiver[noise_command->id];
      rx->nb=noise_command->nb;
      rx->nb2=noise_command->nb2;
      mode_settings[vfo[rx->id].mode].nb=rx->nb;
      mode_settings[vfo[rx->id].mode].nb2=rx->nb2;
      rx->nr=noise_command->nr;
      rx->nr2=noise_command->nr2;
      mode_settings[vfo[rx->id].mode].nr=rx->nr;
      mode_settings[vfo[rx->id].mode].nr2=rx->nr2;
      rx->anf=noise_command->anf;
      mode_settings[vfo[rx->id].mode].anf=rx->anf;
      rx->snb=noise_command->snb;
      mode_settings[vfo[rx->id].mode].snb=rx->snb;
      set_noise();
      send_noise(client->socket,rx->id,rx->nb,rx->nb2,rx->nr,rx->nr2,rx->anf,rx->snb);
      }
      break;
    case CMD_RESP_RX_BAND:
      {
      BAND_COMMAND *band_command=(BAND_COMMAND *)data;
      RECEIVER *rx=receiver[band_command->id];
      short b=htons(band_command->band);
g_print("BAND_COMMAND: id=%d band=%d\n",rx->id,b);
      vfo_band_changed(rx->id,b);
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      }
      break;
    case CMD_RESP_RX_MODE:
      {
      MODE_COMMAND *mode_command=(MODE_COMMAND *)data;
      RECEIVER *rx=receiver[mode_command->id];
      short m=htons(mode_command->mode);
g_print("MODE_COMMAND: id=%d mode=%d\n",rx->id,m);
      vfo_mode_changed(m);
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      send_filter(client->socket,rx->id,m);
      }
      break;
    case CMD_RESP_RX_FILTER:
      {
      FILTER_COMMAND *filter_command=(FILTER_COMMAND *)data;
      RECEIVER *rx=receiver[filter_command->id];
      short f=htons(filter_command->filter);
g_print("FILTER_COMMAND: id=%d filter=%d\n",rx->id,f);
      vfo_filter_changed(f);
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      send_filter(client->socket,rx->id,f);
      }
      break;
    case CMD_RESP_SPLIT:
      {
      SPLIT_COMMAND *split_command=(SPLIT_COMMAND *)data;
g_print("SPLIT_COMMAND: split=%d\n",split);
      if(can_transmit) {
        split=split_command->split;
        tx_set_mode(transmitter,get_tx_mode());
        g_idle_add(ext_vfo_update, NULL);
      }
      send_split(client->socket,split);
      }
      break;
    case CMD_RESP_SAT:
      {
      SAT_COMMAND *sat_command=(SAT_COMMAND *)data;
      sat_mode=sat_command->sat;
g_print("SAT_COMMAND: sat_mode=%d\n",sat_mode);
      g_idle_add(ext_vfo_update, NULL);
      send_sat(client->socket,sat_mode);
      }
      break;
    case CMD_RESP_DUP:
      {
      DUP_COMMAND *dup_command=(DUP_COMMAND *)data;
      duplex=dup_command->dup;
g_print("DUP: duplex=%d\n",duplex);
      g_idle_add(ext_vfo_update, NULL);
      send_dup(client->socket,duplex);
      }
      break;
    case CMD_RESP_LOCK:
      {
      LOCK_COMMAND *lock_command=(LOCK_COMMAND *)data;
      locked=lock_command->lock;
g_print("LOCK: locked=%d\n",locked);
      g_idle_add(ext_vfo_update, NULL);
      send_lock(client->socket,locked);
      }
      break;
    case CMD_RESP_CTUN:
      {
      CTUN_COMMAND *ctun_command=(CTUN_COMMAND *)data;
g_print("CTUN: vfo=%d ctun=%d\n",ctun_command->id,ctun_command->ctun);
      int v=ctun_command->id;
      vfo[v].ctun=ctun_command->ctun;
      if(!vfo[v].ctun) {
        vfo[v].offset=0;
      }
      vfo[v].ctun_frequency=vfo[v].frequency;
      set_offset(active_receiver,vfo[v].offset);
      g_idle_add(ext_vfo_update, NULL);
      send_ctun(client->socket,v,vfo[v].ctun);
      send_vfo_data(client,v);
      }
      break;
    case CMD_RESP_RX_FPS:
      {
      FPS_COMMAND *fps_command=(FPS_COMMAND *)data;
      int rx=fps_command->id;
g_print("FPS: rx=%d fps=%d\n",rx,fps_command->fps);
      receiver[rx]->fps=fps_command->fps;
      calculate_display_average(receiver[rx]);
      set_displaying(receiver[rx],1);
      send_fps(client->socket,rx,receiver[rx]->fps);
      }
      break;
    case CMD_RESP_RX_SELECT:
      {
      RX_SELECT_COMMAND *rx_select_command=(RX_SELECT_COMMAND *)data;
      int rx=rx_select_command->id;
g_print("RX_SELECT: rx=%d\n",rx);
      receiver_set_active(receiver[rx]);
      send_rx_select(client->socket,rx);
      }
      break;
    case CMD_RESP_VFO:
      {
      VFO_COMMAND *vfo_command=(VFO_COMMAND *)data;
      int action=vfo_command->id;
g_print("VFO: action=%d\n",action);
      switch(action) {
        case VFO_A_TO_B:
          vfo_a_to_b();
          break;
        case VFO_B_TO_A:
          vfo_b_to_a();
          break;
        case VFO_A_SWAP_B:
          vfo_a_swap_b();
          break;
      }
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      }
      break;
    case CMD_RESP_RIT_UPDATE:
      {
      RIT_UPDATE_COMMAND *rit_update_command=(RIT_UPDATE_COMMAND *)data;
      int rx=rit_update_command->id;
g_print("RIT_UPDATE: rx=%d\n",rx);
      vfo_rit_update(rx);
      send_vfo_data(client,rx);
      }
      break;
    case CMD_RESP_RIT_CLEAR:
      {
      RIT_CLEAR_COMMAND *rit_clear_command=(RIT_CLEAR_COMMAND *)data;
      int rx=rit_clear_command->id;
g_print("RIT_CLEAR: rx=%d\n",rx);
      vfo_rit_clear(rx);
      send_vfo_data(client,rx);
      }
      break;
    case CMD_RESP_RIT:
      {
      RIT_COMMAND *rit_command=(RIT_COMMAND *)data;
      int rx=rit_command->id;
      short rit=ntohs(rit_command->rit);
g_print("RIT: rx=%d rit=%d\n",rx,(int)rit);
      vfo_rit(rx,(int)rit);
      send_vfo_data(client,rx);
      }
      break;
    case CMD_RESP_XIT_UPDATE:
      {
      XIT_UPDATE_COMMAND *xit_update_command=(XIT_UPDATE_COMMAND *)data;
g_print("XIT_UPDATE\n");
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      }
      break;
    case CMD_RESP_XIT_CLEAR:
      {
      XIT_CLEAR_COMMAND *xit_clear_command=(XIT_CLEAR_COMMAND *)data;
g_print("XIT_CLEAR\n");
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      }
      break;
    case CMD_RESP_XIT:
      {
      XIT_COMMAND *xit_command=(XIT_COMMAND *)data;
      short xit=ntohs(xit_command->xit);
g_print("XIT_CLEAR: xit=%d\n",xit);
      send_vfo_data(client,VFO_A);
      send_vfo_data(client,VFO_B);
      }
      break;
    case CMD_RESP_SAMPLE_RATE:
      {
      SAMPLE_RATE_COMMAND *sample_rate_command=(SAMPLE_RATE_COMMAND *)data;
      int rx=(int)sample_rate_command->id;
      long long rate=ntohll(sample_rate_command->sample_rate);
g_print("SAMPLE_RATE: rx=%d rate=%d\n",rx,(int)rate);
      if(rx==-1) {
        radio_change_sample_rate((int)rate);
        send_sample_rate(client->socket,-1,radio_sample_rate);
      } else {
        receiver_change_sample_rate(receiver[rx],(int)rate);
        send_sample_rate(client->socket,rx,receiver[rx]->sample_rate);
      }
      }
      break;
    case CMD_RESP_RECEIVERS:
      {
      RECEIVERS_COMMAND *receivers_command=(RECEIVERS_COMMAND *)data;
      int r=receivers_command->receivers;
g_print("RECEIVERS: receivers=%d\n",r);
      radio_change_receivers(r);
      send_receivers(client->socket,receivers);
      }
      break;
    case CMD_RESP_RIT_INCREMENT:
      {
      RIT_INCREMENT_COMMAND *rit_increment_command=(RIT_INCREMENT_COMMAND *)data;
      short increment=ntohs(rit_increment_command->increment);
g_print("RIT_INCREMENT: increment=%d\n",increment);
      rit_increment=(int)increment;
      send_rit_increment(client->socket,rit_increment);
      }
      break;
    case CMD_RESP_FILTER_BOARD:
      {
      FILTER_BOARD_COMMAND *filter_board_command=(FILTER_BOARD_COMMAND *)data;
g_print("FILTER_BOARD: board=%d\n",(int)filter_board_command->filter_board);
      filter_board=(int)filter_board_command->filter_board;
      load_filters();
      send_filter_board(client->socket,filter_board);
      }
      break;
    case CMD_RESP_SWAP_IQ:
      {
      SWAP_IQ_COMMAND *swap_iq_command=(SWAP_IQ_COMMAND *)data;
g_print("SWAP_IQ: board=%d\n",(int)swap_iq_command->iqswap);
      iqswap=(int)swap_iq_command->iqswap;
      send_swap_iq(client->socket,iqswap);
      }
      break;
    case CMD_RESP_REGION:
      {
      REGION_COMMAND *region_command=(REGION_COMMAND *)data;
g_print("REGION: region=%d\n",(int)region_command->region);
      iqswap=(int)region_command->region;
      send_region(client->socket,region);
      }
      break;




  }
  g_free(data);
  return 0;
}

int ext_receiver_remote_update_display(void *data) {
  RECEIVER *rx=(RECEIVER *)data;
  receiver_remote_update_display(rx);
  return 0;
}
#endif

int ext_anf_update(void *data) {
  if(active_receiver->anf==0) {
    active_receiver->anf=1;
    mode_settings[vfo[active_receiver->id].mode].anf=1;
  } else {
    active_receiver->snb=0;
    mode_settings[vfo[active_receiver->id].mode].anf=0;
  }
  SetRXAANFRun(active_receiver->id, active_receiver->anf);
  g_idle_add(ext_vfo_update, NULL);
  return 0;
}

int ext_mute_update(void *data) {
  active_receiver->mute_radio=!active_receiver->mute_radio;
  return 0;
}

int ext_zoom_update(void *data) {
  update_zoom((double)GPOINTER_TO_INT(data));
  return 0;
}

int ext_zoom_set(void *data) {
  int pos=GPOINTER_TO_INT(data);
  double zoom=((double)pos/(100.0/7.0))+1.0;
  if((int)zoom!=active_receiver->zoom) {
    set_zoom(active_receiver->id,(double)zoom);
  }
  return 0;
}

int ext_pan_update(void *data) {
  update_pan((double)GPOINTER_TO_INT(data));
  return 0;
}

int ext_pan_set(void *data) {
  if(active_receiver->zoom>1) {
    int pos=GPOINTER_TO_INT(data);
    double pan=(double)((active_receiver->zoom-1)*active_receiver->width)*((double)pos/100.0);
    set_pan(active_receiver->id,(double)pan);
  }
  return 0;
}

int ext_remote_set_zoom(void *data) {
  int zoom=GPOINTER_TO_INT(data);
g_print("ext_remote_set_zoom: %d\n",zoom);
  remote_set_zoom(active_receiver->id,(double)zoom);
  return 0;
}

int ext_remote_set_pan(void *data) {
  int pan=GPOINTER_TO_INT(data);
g_print("ext_remote_set_pan: %d\n",pan);
  remote_set_pan(active_receiver->id,(double)pan);
  return 0;
}

int ext_set_title(void *data) {
  gtk_window_set_title(GTK_WINDOW(top_window),(char *)data);
  return 0;
}
