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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#ifndef __APPLE__
#include <endian.h>
#endif
#include <semaphore.h>

#include "discovered.h"
#include "adc.h"
#include "dac.h"
#include "receiver.h"
#include "transmitter.h"
#include "radio.h"
#include "main.h"
#include "vfo.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "ext.h"
#include "audio.h"

#define DISCOVERY_PORT 4992
#define LISTEN_PORT 50000

gint listen_port=LISTEN_PORT;

REMOTE_CLIENT *clients=NULL;

GMutex client_mutex;

#define MAX_COMMAND 256

enum {
  NO_ACTION,
  SET,
  MOVE,
  MOVETO,
  GET,
  START,
  STOP,
};

static char title[128];

gboolean hpsdr_server=FALSE;

gint client_socket=-1;
GThread *client_thread_id;
gint start_spectrum(void *data);
gboolean remote_started=FALSE;

static GThread *listen_thread_id;
static char status[1024];
static int n_status=0;
static char reply[64];
static gboolean running;
static gint listen_socket;

static int audio_buffer_index=0;
AUDIO_DATA audio_data;


GMutex accumulated_mutex;
static int accumulated_steps=0;
static long long accumulated_hz=0LL;
static gboolean accumulated_round=FALSE;
gint check_vfo_timer_id=-1;

REMOTE_CLIENT *add_client(REMOTE_CLIENT *client) {
g_print("add_client: %p\n",client);
// add to front of queue
  g_mutex_lock(&client_mutex);
  client->next=clients;
  clients=client;
  g_mutex_unlock(&client_mutex);
g_print("add_client: clients=%p\n",clients);
  return client;
}

void delete_client(REMOTE_CLIENT *client) {
g_print("delete_client: %p\n",client);
  g_mutex_lock(&client_mutex);
  if(clients==client) {
    clients=client->next;
    g_free(client);
  } else {
    REMOTE_CLIENT* c=clients;
    REMOTE_CLIENT* last_c=NULL;
    while(c!=NULL && c!=client) {
      last_c=c;
      c=c->next;
    }
    if(c!=NULL) {
      last_c->next=c->next;
      g_free(c);
    }
  }
g_print("delete_client: clients=%p\n",clients);
  g_mutex_unlock(&client_mutex);
}

static int recv_bytes(int s,char *buffer,int bytes) {
  int bytes_read=0;
  int rc;
  while(bytes_read!=bytes) {
    rc=recv(s,&buffer[bytes_read],bytes-bytes_read,0);
    if(rc<0) {
      bytes_read=rc;
      perror("recv_bytes");
      break;
    } else {
      bytes_read+=rc;
    }
  }
  return bytes_read;
}

static int send_bytes(int s,char *buffer,int bytes) {
  int bytes_sent=0;
  int rc;
  if(s<0) return -1;
  while(bytes_sent!=bytes) {
    rc=send(s,&buffer[bytes_sent],bytes-bytes_sent,0);
    if(rc<0) {
      bytes_sent=rc;
      perror("send_bytes");
      break;
    } else {
      bytes_sent+=rc;
    }
  }
  return bytes_sent;
}

void remote_audio(RECEIVER *rx,short left_sample,short right_sample) {
  int i=audio_buffer_index*2;
  audio_data.sample[i]=htons(left_sample);
  audio_data.sample[i+1]=htons(right_sample);
  audio_buffer_index++;
  if(audio_buffer_index>=AUDIO_DATA_SIZE) {
    g_mutex_lock(&client_mutex);
    REMOTE_CLIENT *c=clients;
    while(c!=NULL && c->socket!=-1) {
      audio_data.header.sync=REMOTE_SYNC;
      audio_data.header.data_type=htons(INFO_AUDIO);
      audio_data.header.version=htonll(CLIENT_SERVER_VERSION);
      audio_data.rx=rx->id;
      audio_data.samples=ntohs(audio_buffer_index);
      int bytes_sent=send_bytes(c->socket,(char *)&audio_data,sizeof(audio_data));
      if(bytes_sent<0) {
        perror("remote_audio");
        if(c->socket!=-1) {
          close(c->socket);
        }
      }
      c=c->next;
    }
    g_mutex_unlock(&client_mutex);
    audio_buffer_index=0;
  }
}

static gint send_spectrum(void *arg) {
  REMOTE_CLIENT *client=(REMOTE_CLIENT *)arg;
  float *samples;
  double sample;
  short s;
  SPECTRUM_DATA spectrum_data;
  gint result;

  result=TRUE;

  if(!(client->receiver[0].send_spectrum || client->receiver[1].send_spectrum) || !client->running) {
    client->spectrum_update_timer_id=-1;
g_print("send_spectrum: no more receivers\n");
    return FALSE;
  }

  for(int r=0;r<receivers;r++) {
    RECEIVER *rx=receiver[r];
    if(client->receiver[r].send_spectrum) {
      if(rx->displaying && (rx->pixels>0) && (rx->pixel_samples!=NULL)) {
        g_mutex_lock(&rx->display_mutex);
        spectrum_data.header.sync=REMOTE_SYNC;
        spectrum_data.header.data_type=htons(INFO_SPECTRUM);
        spectrum_data.header.version=htonll(CLIENT_SERVER_VERSION);
        spectrum_data.rx=r;
        spectrum_data.vfo_a_freq=htonll(vfo[VFO_A].frequency);
        spectrum_data.vfo_b_freq=htonll(vfo[VFO_B].frequency);
        spectrum_data.vfo_a_ctun_freq=htonll(vfo[VFO_A].ctun_frequency);
        spectrum_data.vfo_b_ctun_freq=htonll(vfo[VFO_B].ctun_frequency);
        spectrum_data.vfo_a_offset=htonll(vfo[VFO_A].offset);
        spectrum_data.vfo_b_offset=htonll(vfo[VFO_B].offset);
        s=(short)receiver[r]->meter;
        spectrum_data.meter=htons(s);
        spectrum_data.samples=htons(rx->width);
        samples=rx->pixel_samples;
        for(int i=0;i<rx->width;i++) {
          s=(short)samples[i+rx->pan];
          spectrum_data.sample[i]=htons(s);
        }
        // send the buffer
        int bytes_sent=send_bytes(client->socket,(char *)&spectrum_data,sizeof(spectrum_data));
        if(bytes_sent<0) {
          result=FALSE;
        }
        g_mutex_unlock(&rx->display_mutex);
      }
    }
  }
  return result;
}

void send_radio_data(REMOTE_CLIENT *client) {
  RADIO_DATA radio_data;
  radio_data.header.sync=REMOTE_SYNC;
  radio_data.header.data_type=htons(INFO_RADIO);
  radio_data.header.version=htonl(CLIENT_SERVER_VERSION);
  strcpy(radio_data.name,radio->name);
  radio_data.protocol=htons(radio->protocol);
  radio_data.device=htons(radio->device);
  uint64_t temp=(uint64_t)radio->frequency_min;
  radio_data.frequency_min=htonll(temp);
  temp=(uint64_t)radio->frequency_max;
  radio_data.frequency_max=htonll(temp);
  long long rate=(long long)radio_sample_rate;
  radio_data.sample_rate=htonll(rate);
  radio_data.display_filled=display_filled;
  radio_data.locked=locked;
  radio_data.supported_receivers=htons(radio->supported_receivers);
  radio_data.receivers=htons(receivers);
  radio_data.can_transmit=can_transmit;
  radio_data.step=htonll(step);
  radio_data.split=split;
  radio_data.sat_mode=sat_mode;
  radio_data.duplex=duplex;
  radio_data.have_rx_gain=have_rx_gain;
  radio_data.rx_gain_calibration=htons(rx_gain_calibration);
  radio_data.filter_board=htons(filter_board);

  int bytes_sent=send_bytes(client->socket,(char *)&radio_data,sizeof(radio_data));
g_print("send_radio_data: %d\n",bytes_sent);
  if(bytes_sent<0) {
    perror("send_radio_data");
  } else {
    //g_print("send_radio_data: %d\n",bytes_sent);
  }
}

void send_adc_data(REMOTE_CLIENT *client,int i) {
  ADC_DATA adc_data;
  adc_data.header.sync=REMOTE_SYNC;
  adc_data.header.data_type=htons(INFO_ADC);
  adc_data.header.version=htonl(CLIENT_SERVER_VERSION);
  adc_data.adc=i;
  adc_data.filters=htons(adc[i].filters);
  adc_data.hpf=htons(adc[i].hpf);
  adc_data.lpf=htons(adc[i].lpf);
  adc_data.antenna=htons(adc[i].antenna);
  adc_data.dither=adc[i].dither;
  adc_data.random=adc[i].random;
  adc_data.preamp=adc[i].preamp;
  adc_data.attenuation=htons(adc[i].attenuation);
  adc_data.adc_attenuation=htons(adc_attenuation[i]);
  int bytes_sent=send_bytes(client->socket,(char *)&adc_data,sizeof(adc_data));
  if(bytes_sent<0) {
    perror("send_adc_data");
  } else {
    //g_print("send_adc_data: %d\n",bytes_sent);
  }
}

void send_receiver_data(REMOTE_CLIENT *client,int rx) {
  RECEIVER_DATA receiver_data;
  receiver_data.header.sync=REMOTE_SYNC;
  receiver_data.header.data_type=htons(INFO_RECEIVER);
  receiver_data.header.version=htonl(CLIENT_SERVER_VERSION);
  receiver_data.rx=rx;
  receiver_data.adc=htons(receiver[rx]->adc);
  long long rate=(long long)receiver[rx]->sample_rate;
  receiver_data.sample_rate=htonll(rate);
  receiver_data.displaying=receiver[rx]->displaying;
  receiver_data.display_panadapter=receiver[rx]->display_panadapter;
  receiver_data.display_waterfall=receiver[rx]->display_waterfall;
  receiver_data.fps=htons(receiver[rx]->fps);
  receiver_data.agc=receiver[rx]->agc;
  short s=(short)receiver[rx]->agc_hang;
  receiver_data.agc_hang=htons(s);
  s=(short)receiver[rx]->agc_thresh;
  receiver_data.agc_thresh=htons(s);
  receiver_data.nb=receiver[rx]->nb;
  receiver_data.nb2=receiver[rx]->nb2;
  receiver_data.nr=receiver[rx]->nr;
  receiver_data.nr2=receiver[rx]->nr2;
  receiver_data.anf=receiver[rx]->anf;
  receiver_data.snb=receiver[rx]->snb;
  receiver_data.filter_low=htons(receiver[rx]->filter_low);
  receiver_data.filter_high=htons(receiver[rx]->filter_high);
  receiver_data.panadapter_low=htons(receiver[rx]->panadapter_low);
  receiver_data.panadapter_high=htons(receiver[rx]->panadapter_high);
  receiver_data.panadapter_step=htons(receiver[rx]->panadapter_step);
  receiver_data.waterfall_low=htons(receiver[rx]->waterfall_low);
  receiver_data.waterfall_high=htons(receiver[rx]->waterfall_high);
  receiver_data.waterfall_automatic=receiver[rx]->waterfall_automatic;
  receiver_data.pixels=htons(receiver[rx]->pixels);
  receiver_data.zoom=htons(receiver[rx]->zoom);
  receiver_data.pan=htons(receiver[rx]->pan);
  receiver_data.width=htons(receiver[rx]->width);
  receiver_data.height=htons(receiver[rx]->height);
  receiver_data.x=htons(receiver[rx]->x);
  receiver_data.y=htons(receiver[rx]->y);
  s=(short)(receiver[rx]->volume*100.0);
  receiver_data.volume=htons(s);
  s=(short)receiver[rx]->rf_gain;
  receiver_data.rf_gain=htons(s);
  s=(short)receiver[rx]->agc_gain;
  receiver_data.agc_gain=htons(s);

  int bytes_sent=send_bytes(client->socket,(char *)&receiver_data,sizeof(receiver_data));
  if(bytes_sent<0) {
    perror("send_receiver_data");
  } else {
    //g_print("send_receiver_data: bytes sent %d\n",bytes_sent);
  }
}

void send_vfo_data(REMOTE_CLIENT *client,int v) {
  VFO_DATA vfo_data;
  vfo_data.header.sync=REMOTE_SYNC;
  vfo_data.header.data_type=htons(INFO_VFO);
  vfo_data.header.version=htonl(CLIENT_SERVER_VERSION);
  vfo_data.vfo=v;
  vfo_data.band=htons(vfo[v].band);
  vfo_data.bandstack=htons(vfo[v].bandstack);
  vfo_data.frequency=htonll(vfo[v].frequency);
  vfo_data.mode=htons(vfo[v].mode);
  vfo_data.filter=htons(vfo[v].filter);
  vfo_data.ctun=vfo[v].ctun;
  vfo_data.ctun_frequency=htonll(vfo[v].ctun_frequency);
  vfo_data.rit_enabled=vfo[v].rit_enabled;
  vfo_data.rit=htonll(vfo[v].rit);
  vfo_data.lo=htonll(vfo[v].lo);
  vfo_data.offset=htonll(vfo[v].offset);

  int bytes_sent=send_bytes(client->socket,(char *)&vfo_data,sizeof(vfo_data));
  if(bytes_sent<0) {
    perror("send_vfo_data");
  } else {
    //g_print("send_vfo_data: bytes sent %d\n",bytes_sent);
  }
}

static void *server_client_thread(void *arg) {
  REMOTE_CLIENT *client=(REMOTE_CLIENT *)arg;
  int bytes_read;
  HEADER header;

g_print("Client connected on port %d\n",client->address.sin_port);
  send_radio_data(client);
  send_adc_data(client,0);
  send_adc_data(client,1);
  for(int i=0;i<RECEIVERS;i++) {
    send_receiver_data(client,i);
  }
  send_vfo_data(client,VFO_A);
  send_vfo_data(client,VFO_B);

  // get and parse client commands
  while(client->running) {
    bytes_read=recv_bytes(client->socket,(char *)&header.sync,sizeof(header.sync));
    if(bytes_read<=0) {
      g_print("server_client_thread: read %d bytes for HEADER SYNC\n",bytes_read);
      perror("server_client_thread");
      client->running=FALSE;
      continue;
    }

g_print("header.sync is %x\n",header.sync);

    if(header.sync!=REMOTE_SYNC) {
g_print("header.sync is %x wanted %x\n",header.sync,REMOTE_SYNC);
      int syncs=0;
      char c;
      while(syncs!=sizeof(header.sync) && client->running) {
        // try to resync on 2 0xFA bytes
        bytes_read=recv_bytes(client->socket,(char *)&c,1);
        if(bytes_read<=0) {
          g_print("server_client_thread: read %d bytes for HEADER RESYNC\n",bytes_read);
          perror("server_client_thread");
          client->running=FALSE;
          continue;
        }
        if(c==(char)0xFA) {
          syncs++;
        } else {
          syncs=0;
        }
      }
    }

    bytes_read=recv_bytes(client->socket,(char *)&header.data_type,sizeof(header)-sizeof(header.sync));
    if(bytes_read<=0) {
      g_print("server_client_thread: read %d bytes for HEADER\n",bytes_read);
      perror("server_client_thread");
      client->running=FALSE;
      continue;
    }
g_print("header remaining bytes %d\n",bytes_read);

g_print("server_client_thread: received header: type=%d\n",ntohs(header.data_type));

    switch(ntohs(header.data_type)) {
       case CMD_RESP_SPECTRUM:
         {
         SPECTRUM_COMMAND spectrum_command;
         bytes_read=recv_bytes(client->socket,(char *)&spectrum_command.id,sizeof(SPECTRUM_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for SPECTRUM_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }

         int rx=spectrum_command.id;
         int state=spectrum_command.start_stop;
g_print("server_client_thread: CMD_RESP_SPECTRUM rx=%d state=%d timer_id=%d\n",rx,state,client->spectrum_update_timer_id);
         if(state) {
           client->receiver[rx].receiver=rx;
           client->receiver[rx].spectrum_fps=receiver[rx]->fps;
           client->receiver[rx].spectrum_port=0;
           client->receiver[rx].send_spectrum=TRUE;
           if(client->spectrum_update_timer_id==-1) {
g_print("start send_spectrum thread: fps=%d\n",client->receiver[rx].spectrum_fps);
             client->spectrum_update_timer_id=gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE,1000/client->receiver[rx].spectrum_fps, send_spectrum, client, NULL);
g_print("spectrum_update_timer_id=%d\n",client->spectrum_update_timer_id);
           } else {
g_print("send_spectrum thread already running\n");
           }
         } else {
           client->receiver[rx].send_spectrum=FALSE;
         }
         }
         break;
       case CMD_RESP_RX_FREQ:
g_print("server_client_thread: CMD_RESP_RX_FREQ\n");
         {
         FREQ_COMMAND *freq_command=g_new(FREQ_COMMAND,1);
         freq_command->header.data_type=header.data_type;
         freq_command->header.version=header.version;
         freq_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&freq_command->id,sizeof(FREQ_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for FREQ_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,freq_command);
         }
         break;
       case CMD_RESP_RX_STEP:
g_print("server_client_thread: CMD_RESP_RX_STEP\n");
         {
         STEP_COMMAND *step_command=g_new(STEP_COMMAND,1);
         step_command->header.data_type=header.data_type;
         step_command->header.version=header.version;
         step_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&step_command->id,sizeof(STEP_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for STEP_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,step_command);
         }
         break;
       case CMD_RESP_RX_MOVE:
g_print("server_client_thread: CMD_RESP_RX_MOVE\n");
         {
         MOVE_COMMAND *move_command=g_new(MOVE_COMMAND,1);
         move_command->header.data_type=header.data_type;
         move_command->header.version=header.version;
         move_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&move_command->id,sizeof(MOVE_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for MOVE_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,move_command);
         }
         break;
       case CMD_RESP_RX_MOVETO:
g_print("server_client_thread: CMD_RESP_RX_MOVETO\n");
         {
         MOVE_TO_COMMAND *move_to_command=g_new(MOVE_TO_COMMAND,1);
         move_to_command->header.data_type=header.data_type;
         move_to_command->header.version=header.version;
         move_to_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&move_to_command->id,sizeof(MOVE_TO_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for MOVE_TO_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,move_to_command);
         }
         break;
       case CMD_RESP_RX_ZOOM:
g_print("server_client_thread: CMD_RESP_RX_ZOOM\n");
         {
         ZOOM_COMMAND *zoom_command=g_new(ZOOM_COMMAND,1);
         zoom_command->header.data_type=header.data_type;
         zoom_command->header.version=header.version;
         zoom_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&zoom_command->id,sizeof(ZOOM_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for ZOOM_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,zoom_command);
         }
         break;
       case CMD_RESP_RX_PAN:
g_print("server_client_thread: CMD_RESP_RX_PAN\n");
         {
         PAN_COMMAND *pan_command=g_new(PAN_COMMAND,1);
         pan_command->header.data_type=header.data_type;
         pan_command->header.version=header.version;
         pan_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&pan_command->id,sizeof(PAN_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for PAN_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,pan_command);
         }
         break;
       case CMD_RESP_RX_VOLUME:
g_print("server_client_thread: CMD_RESP_RX_VOLUME\n");
         {
         VOLUME_COMMAND *volume_command=g_new(VOLUME_COMMAND,1);
         volume_command->header.data_type=header.data_type;
         volume_command->header.version=header.version;
         volume_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&volume_command->id,sizeof(VOLUME_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for VOLUME_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,volume_command);
         }
         break;
       case CMD_RESP_RX_AGC:
g_print("server_client_thread: CMD_RESP_RX_AGC\n");
         {
         AGC_COMMAND *agc_command=g_new(AGC_COMMAND,1);
         agc_command->header.data_type=header.data_type;
         agc_command->header.version=header.version;
         agc_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&agc_command->id,sizeof(AGC_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for AGC_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
g_print("CMD_RESP_RX_AGC: id=%d agc=%d\n",agc_command->id,ntohs(agc_command->agc));
         g_idle_add(ext_remote_command,agc_command);
         }
         break;
       case CMD_RESP_RX_AGC_GAIN:
g_print("server_client_thread: CMD_RESP_RX_AGC_GAIN\n");
         {
         AGC_GAIN_COMMAND *agc_gain_command=g_new(AGC_GAIN_COMMAND,1);
         agc_gain_command->header.data_type=header.data_type;
         agc_gain_command->header.version=header.version;
         agc_gain_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&agc_gain_command->id,sizeof(AGC_GAIN_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for AGC_GAIN_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,agc_gain_command);
         }
         break;
      case CMD_RESP_RX_ATTENUATION:
g_print("server_client_thread: CMD_RESP_RX_ATTENUATION\n");
         {
         ATTENUATION_COMMAND *attenuation_command=g_new(ATTENUATION_COMMAND,1);
         attenuation_command->header.data_type=header.data_type;
         attenuation_command->header.version=header.version;
         attenuation_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&attenuation_command->id,sizeof(ATTENUATION_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for ATTENUATION_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,attenuation_command);
         }
         break;
       case CMD_RESP_RX_SQUELCH:
g_print("server_client_thread: CMD_RESP_RX_SQUELCH\n");
         {
         SQUELCH_COMMAND *squelch_command=g_new(SQUELCH_COMMAND,1);
         squelch_command->header.data_type=header.data_type;
         squelch_command->header.version=header.version;
         squelch_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&squelch_command->id,sizeof(SQUELCH_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for SQUELCH_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,squelch_command);
         }
         break;
       case CMD_RESP_RX_NOISE:
g_print("server_client_thread: CMD_RESP_RX_NOISE\n");
         {
         NOISE_COMMAND *noise_command=g_new(NOISE_COMMAND,1);
         noise_command->header.data_type=header.data_type;
         noise_command->header.version=header.version;
         noise_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&noise_command->id,sizeof(NOISE_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for NOISE_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,noise_command);
         }
         break;
       case CMD_RESP_RX_BAND:
g_print("server_client_thread: CMD_RESP_RX_BAND\n");
         {
         BAND_COMMAND *band_command=g_new(BAND_COMMAND,1);
         band_command->header.data_type=header.data_type;
         band_command->header.version=header.version;
         band_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&band_command->id,sizeof(BAND_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for BAND_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,band_command);
         }
         break;
       case CMD_RESP_RX_MODE:
g_print("server_client_thread: CMD_RESP_RX_MODE\n");
         {
         MODE_COMMAND *mode_command=g_new(MODE_COMMAND,1);
         mode_command->header.data_type=header.data_type;
         mode_command->header.version=header.version;
         mode_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&mode_command->id,sizeof(MODE_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for MODE_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,mode_command);
         }
         break;
       case CMD_RESP_RX_FILTER:
g_print("server_client_thread: CMD_RESP_RX_FILTER\n");
         {
         FILTER_COMMAND *filter_command=g_new(FILTER_COMMAND,1);
         filter_command->header.data_type=header.data_type;
         filter_command->header.version=header.version;
         filter_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&filter_command->id,sizeof(FILTER_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for FILTER_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,filter_command);
         }
         break;
       case CMD_RESP_SPLIT:
g_print("server_client_thread: CMD_RESP_RX_SPLIT\n");
         {
         SPLIT_COMMAND *split_command=g_new(SPLIT_COMMAND,1);
         split_command->header.data_type=header.data_type;
         split_command->header.version=header.version;
         split_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&split_command->split,sizeof(SPLIT_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for SPLIT_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,split_command);
         }
         break;
       case CMD_RESP_SAT:
g_print("server_client_thread: CMD_RESP_RX_SAT\n");
         {
         SAT_COMMAND *sat_command=g_new(SAT_COMMAND,1);
         sat_command->header.data_type=header.data_type;
         sat_command->header.version=header.version;
         sat_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&sat_command->sat,sizeof(SAT_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for SAT_COMMAND\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,sat_command);
         }
         break;
       case CMD_RESP_DUP:
g_print("server_client_thread: CMD_RESP_DUP\n");
         {
         DUP_COMMAND *dup_command=g_new(DUP_COMMAND,1);
         dup_command->header.data_type=header.data_type;
         dup_command->header.version=header.version;
         dup_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&dup_command->dup,sizeof(DUP_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for DUP\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,dup_command);
         }
         break;
       case CMD_RESP_LOCK:
g_print("server_client_thread: CMD_RESP_LOCK\n");
         {
         LOCK_COMMAND *lock_command=g_new(LOCK_COMMAND,1);
         lock_command->header.data_type=header.data_type;
         lock_command->header.version=header.version;
         lock_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&lock_command->lock,sizeof(LOCK_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for LOCK\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,lock_command);
         }
         break;
       case CMD_RESP_CTUN:
g_print("server_client_thread: CMD_RESP_CTUN\n");
         {
         CTUN_COMMAND *ctun_command=g_new(CTUN_COMMAND,1);
         ctun_command->header.data_type=header.data_type;
         ctun_command->header.version=header.version;
         ctun_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&ctun_command->id,sizeof(CTUN_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for CTUN\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,ctun_command);
         }
         break;
       case CMD_RESP_RX_FPS:
g_print("server_client_thread: CMD_RESP_RX_FPS\n");
         {
         FPS_COMMAND *fps_command=g_new(FPS_COMMAND,1);
         fps_command->header.data_type=header.data_type;
         fps_command->header.version=header.version;
         fps_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&fps_command->id,sizeof(FPS_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for FPS\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,fps_command);
         }
         break;
       case CMD_RESP_RX_SELECT:
g_print("server_client_thread: CMD_RESP_RX_SELECT\n");
         {
         RX_SELECT_COMMAND *rx_select_command=g_new(RX_SELECT_COMMAND,1);
         rx_select_command->header.data_type=header.data_type;
         rx_select_command->header.version=header.version;
         rx_select_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&rx_select_command->id,sizeof(RX_SELECT_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for RX_SELECT\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,rx_select_command);
         }
         break;
       case CMD_RESP_VFO:
g_print("server_client_thread: CMD_RESP_VFO\n");
         {
         VFO_COMMAND *vfo_command=g_new(VFO_COMMAND,1);
         vfo_command->header.data_type=header.data_type;
         vfo_command->header.version=header.version;
         vfo_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&vfo_command->id,sizeof(VFO_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for VFO\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,vfo_command);
         }
         break;
       case CMD_RESP_RIT_UPDATE:
g_print("server_client_thread: CMD_RESP_RIT_UPDATE\n");
         {
         RIT_UPDATE_COMMAND *rit_update_command=g_new(RIT_UPDATE_COMMAND,1);
         rit_update_command->header.data_type=header.data_type;
         rit_update_command->header.version=header.version;
         rit_update_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&rit_update_command->id,sizeof(RIT_UPDATE_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for RIT_UPDATE\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,rit_update_command);
         }
         break;
       case CMD_RESP_RIT_CLEAR:
g_print("server_client_thread: CMD_RESP_RIT_CLEAR\n");
         {
         RIT_CLEAR_COMMAND *rit_clear_command=g_new(RIT_CLEAR_COMMAND,1);
         rit_clear_command->header.data_type=header.data_type;
         rit_clear_command->header.version=header.version;
         rit_clear_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&rit_clear_command->id,sizeof(RIT_CLEAR_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for RIT_CLEAR\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,rit_clear_command);
         }
         break;
       case CMD_RESP_RIT:
g_print("server_client_thread: CMD_RESP_RIT\n");
         {
         RIT_COMMAND *rit_command=g_new(RIT_COMMAND,1);
         rit_command->header.data_type=header.data_type;
         rit_command->header.version=header.version;
         rit_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&rit_command->id,sizeof(RIT_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for RIT\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,rit_command);
         }
         break;
       case CMD_RESP_XIT_UPDATE:
g_print("server_client_thread: CMD_RESP_XIT_UPDATE\n");
         {
         XIT_UPDATE_COMMAND *xit_update_command=g_new(XIT_UPDATE_COMMAND,1);
         xit_update_command->header.data_type=header.data_type;
         xit_update_command->header.version=header.version;
         xit_update_command->header.context.client=client;
         g_idle_add(ext_remote_command,xit_update_command);
         }
         break;
       case CMD_RESP_XIT_CLEAR:
g_print("server_client_thread: CMD_RESP_XIT_CLEAR\n");
         {
         XIT_CLEAR_COMMAND *xit_clear_command=g_new(XIT_CLEAR_COMMAND,1);
         xit_clear_command->header.data_type=header.data_type;
         xit_clear_command->header.version=header.version;
         xit_clear_command->header.context.client=client;
         g_idle_add(ext_remote_command,xit_clear_command);
         }
         break;
       case CMD_RESP_XIT:
g_print("server_client_thread: CMD_RESP_XIT\n");
         {
         XIT_COMMAND *xit_command=g_new(XIT_COMMAND,1);
         xit_command->header.data_type=header.data_type;
         xit_command->header.version=header.version;
         xit_command->header.context.client=client;
         g_idle_add(ext_remote_command,xit_command);
         }
         break;
       case CMD_RESP_SAMPLE_RATE:
g_print("server_client_thread: CMD_RESP_SAMPLE_RATE\n");
         {
         SAMPLE_RATE_COMMAND *sample_rate_command=g_new(SAMPLE_RATE_COMMAND,1);
         sample_rate_command->header.data_type=header.data_type;
         sample_rate_command->header.version=header.version;
         sample_rate_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&sample_rate_command->id,sizeof(SAMPLE_RATE_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for SAMPLE_RATE\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,sample_rate_command);
         }
         break;
       case CMD_RESP_RECEIVERS:
g_print("server_client_thread: CMD_RESP_RECEIVERS\n");
         {
         RECEIVERS_COMMAND *receivers_command=g_new(RECEIVERS_COMMAND,1);
         receivers_command->header.data_type=header.data_type;
         receivers_command->header.version=header.version;
         receivers_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&receivers_command->receivers,sizeof(RECEIVERS_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for RECEIVERS\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,receivers_command);
         }
         break;
       case CMD_RESP_RIT_INCREMENT:
g_print("server_client_thread: CMD_RESP_RIT_INCREMENT\n");
         {
         RIT_INCREMENT_COMMAND *rit_increment_command=g_new(RIT_INCREMENT_COMMAND,1);
         rit_increment_command->header.data_type=header.data_type;
         rit_increment_command->header.version=header.version;
         rit_increment_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&rit_increment_command->increment,sizeof(RIT_INCREMENT_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for RIT_INCREMENT\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,rit_increment_command);
         }
         break;
       case CMD_RESP_FILTER_BOARD:
g_print("server_client_thread: CMD_RESP_FILTER_BOARD\n");
         {
         FILTER_BOARD_COMMAND *filter_board_command=g_new(FILTER_BOARD_COMMAND,1);
         filter_board_command->header.data_type=header.data_type;
         filter_board_command->header.version=header.version;
         filter_board_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&filter_board_command->filter_board,sizeof(FILTER_BOARD_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for FILTER_BOARD\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,filter_board_command);
         }
         break;
       case CMD_RESP_SWAP_IQ:
g_print("server_client_thread: CMD_RESP_SWAP_IQ\n");
         {
         SWAP_IQ_COMMAND *swap_iq_command=g_new(SWAP_IQ_COMMAND,1);
         swap_iq_command->header.data_type=header.data_type;
         swap_iq_command->header.version=header.version;
         swap_iq_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&swap_iq_command->iqswap,sizeof(SWAP_IQ_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for SWAP_IQ\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,swap_iq_command);
         }
         break;
       case CMD_RESP_REGION:
g_print("server_client_thread: CMD_RESP_REGION\n");
         {
         REGION_COMMAND *region_command=g_new(REGION_COMMAND,1);
         region_command->header.data_type=header.data_type;
         region_command->header.version=header.version;
         region_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&region_command->region,sizeof(REGION_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for REGION\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,region_command);
         }
         break;
       case CMD_RESP_MUTE_RX:
g_print("server_client_thread: CMD_RESP_MUTE_RX\n");
         {
         MUTE_RX_COMMAND *mute_rx_command=g_new(MUTE_RX_COMMAND,1);
         mute_rx_command->header.data_type=header.data_type;
         mute_rx_command->header.version=header.version;
         mute_rx_command->header.context.client=client;
         bytes_read=recv_bytes(client->socket,(char *)&mute_rx_command->mute,sizeof(MUTE_RX_COMMAND)-sizeof(header));
         if(bytes_read<0) {
           g_print("server_client_thread: read %d bytes for MUTE_RX\n",bytes_read);
           perror("server_client_thread");
           // dialog box?
           return NULL;
         }
         g_idle_add(ext_remote_command,mute_rx_command);
         }
         break;





       default:
g_print("server_client_thread: UNKNOWN command: %d\n",ntohs(header.data_type));
         break;
    }

  }

  // close the socket to force listen to terminate
g_print("client disconnected\n");
  if(client->socket!=-1) {
    close(client->socket);
    client->socket=-1;
  }
  delete_client(client);
  return NULL;
}

void send_start_spectrum(int s,int rx) {
  SPECTRUM_COMMAND command;
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_SPECTRUM);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.start_stop=1;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_vfo_frequency(int s,int rx,long long hz) {
  FREQ_COMMAND command;

  g_print("send_vfo_frequency\n");
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_FREQ);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.hz=htonll(hz);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_vfo_move_to(int s,int rx,long long hz) {
  MOVE_TO_COMMAND command;
  g_print("send_vfo_move_to\n");
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_MOVETO);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.hz=htonll(hz);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_vfo_move(int s,int rx,long long hz,int round) {
  MOVE_COMMAND command;

  g_print("send_vfo_move\n");
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_MOVE);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.hz=htonll(hz);
  command.round=round;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void update_vfo_move(int rx,long long hz,int round) {
  g_mutex_lock(&accumulated_mutex);
  accumulated_hz+=hz;
  accumulated_round=round;
  g_mutex_unlock(&accumulated_mutex);
}

void send_vfo_step(int s,int rx,int steps) {
  STEP_COMMAND command;

  short stps=(short)steps;
  g_print("send_vfo_step rx=%d steps=%d s=%d\n",rx,steps,stps);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_STEP);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.steps=htons(stps);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void update_vfo_step(int rx,int steps) {
  g_mutex_lock(&accumulated_mutex);
  accumulated_steps+=steps;
  g_mutex_unlock(&accumulated_mutex);
}

void send_zoom(int s,int rx,int zoom) {
  ZOOM_COMMAND command;
  short z=(short)zoom;
g_print("send_zoom rx=%d zoom=%d\n",rx,zoom);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_ZOOM);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.zoom=htons(z);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_pan(int s,int rx,int pan) {
  PAN_COMMAND command;
  short p=(short)pan;
g_print("send_pan rx=%d pan=%d\n",rx,pan);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_PAN);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.pan=htons(p);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_volume(int s,int rx,int volume) {
  VOLUME_COMMAND command;
  short v=(short)volume;
g_print("send_volume rx=%d volume=%d\n",rx,volume);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_VOLUME);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.volume=htons(v);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_agc(int s,int rx,int agc) {
  AGC_COMMAND command;
  short a=(short)agc;
g_print("send_agc rx=%d agc=%d\n",rx,agc);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_AGC);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.agc=htons(a);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_agc_gain(int s,int rx,int gain,int hang,int thresh) {
  AGC_GAIN_COMMAND command;
  short g=(short)gain;
  short h=(short)hang;
  short t=(short)thresh;
g_print("send_agc_gain rx=%d gain=%d\n",rx,gain);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_AGC_GAIN);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.gain=htons(g);
  command.hang=htons(h);
  command.thresh=htons(t);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_attenuation(int s,int rx,int attenuation) {
  ATTENUATION_COMMAND command;
  short a=(short)attenuation;
g_print("send_attenuation rx=%d attenuation=%d\n",rx,attenuation);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_ATTENUATION);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.attenuation=htons(a);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_squelch(int s,int rx,int enable,int squelch) {
  SQUELCH_COMMAND command;
  short sq=(short)squelch;
g_print("send_squelch rx=%d enable=%d squelch=%d\n",rx,enable,squelch);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_SQUELCH);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.enable=enable;
  command.squelch=htons(sq);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_noise(int s,int rx,int nb,int nb2,int nr,int nr2,int anf,int snb) {
  NOISE_COMMAND command;
g_print("send_noise rx=%d nb=%d nb2=%d nr=%d nr2=%d anf=%d snb=%d\n",rx,nb,nb2,nr,nr2,anf,snb);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_NOISE);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.nb=nb;
  command.nb2=nb2;
  command.nr=nr;
  command.nr2=nr2;
  command.anf=anf;
  command.snb=snb;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_band(int s,int rx,int band) {
  BAND_COMMAND command;
g_print("send_band rx=%d band=%d\n",rx,band);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_BAND);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.band=htons(band);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_mode(int s,int rx,int mode) {
  MODE_COMMAND command;
g_print("send_mode rx=%d mode=%d\n",rx,mode);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_MODE);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.mode=htons(mode);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_filter(int s,int rx,int filter) {
  FILTER_COMMAND command;
g_print("send_filter rx=%d filter=%d\n",rx,filter);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_FILTER);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.filter=htons(filter);
  command.filter_low=htons(receiver[rx]->filter_low);
  command.filter_high=htons(receiver[rx]->filter_high);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_split(int s,int split) {
  SPLIT_COMMAND command;
g_print("send_split split=%d\n",split);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_SPLIT);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.split=split;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_sat(int s,int sat) {
  SAT_COMMAND command;
g_print("send_sat sat=%d\n",sat);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_SAT);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.sat=sat;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_dup(int s,int dup) {
  DUP_COMMAND command;
g_print("send_dup dup=%d\n",dup);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_DUP);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.dup=dup;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_fps(int s,int rx,int fps) {
  FPS_COMMAND command;
g_print("send_fps rx=%d fps=%d\n",rx,fps);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_FPS);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.fps=htons(fps);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_lock(int s,int lock) {
  LOCK_COMMAND command;
g_print("send_lock lock=%d\n",lock);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_LOCK);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.lock=lock;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_ctun(int s,int vfo,int ctun) {
  CTUN_COMMAND command;
g_print("send_ctun vfo=%d ctun=%d\n",vfo,ctun);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_CTUN);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=vfo;
  command.ctun=ctun;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_rx_select(int s,int rx) {
  RX_SELECT_COMMAND command;
g_print("send_rx_select rx=%d\n",rx);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RX_SELECT);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_vfo(int s,int action) {
  VFO_COMMAND command;
g_print("send_vfo action=%d\n",action);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_VFO);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=action;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_rit_update(int s,int rx) {
  RIT_UPDATE_COMMAND command;
g_print("send_rit_enable rx=%d\n",rx);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RIT_UPDATE);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_rit_clear(int s,int rx) {
  RIT_CLEAR_COMMAND command;
g_print("send_rit_clear rx=%d\n",rx);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RIT_CLEAR);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_rit(int s,int rx,int rit) {
  RIT_COMMAND command;
g_print("send_rit rx=%d rit=%d\n",rx,rit);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RIT);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.rit=htons(rit);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_xit_update(int s) {
  XIT_UPDATE_COMMAND command;
g_print("send_xit_update\n");
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RIT_UPDATE);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_xit_clear(int s) {
  XIT_CLEAR_COMMAND command;
g_print("send_xit_clear\n");
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_XIT_CLEAR);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_xit(int s,int xit) {
  XIT_COMMAND command;
g_print("send_xit xit=%d\n",xit);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_XIT);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.xit=htons(xit);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_sample_rate(int s,int rx,int sample_rate) {
  SAMPLE_RATE_COMMAND command;

  long long rate=(long long)sample_rate;
  g_print("send_sample_rate rx=%d rate=%lld\n",rx,rate);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_SAMPLE_RATE);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.id=rx;
  command.sample_rate=htonll(rate);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_receivers(int s,int receivers) {
  RECEIVERS_COMMAND command;

  g_print("send_receivers receivers=%d\n",receivers);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RECEIVERS);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.receivers=receivers;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }
}

void send_rit_increment(int s,int increment) {
  RIT_INCREMENT_COMMAND command;

g_print("send_rit_increment increment=%d\n",increment);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_RIT_INCREMENT);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.increment=htons(increment);
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }

}

void send_filter_board(int s,int filter_board) {
  FILTER_BOARD_COMMAND command;

g_print("send_filter_board filter_board=%d\n",filter_board);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_FILTER_BOARD);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.filter_board=filter_board;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }

}

void send_swap_iq(int s,int iqswap) {
  SWAP_IQ_COMMAND command;

g_print("send_swap_iq iqswap=%d\n",iqswap);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_SWAP_IQ);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.iqswap=iqswap;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }

}

void send_region(int s,int region) {
  REGION_COMMAND command;

g_print("send_region region=%d\n",region);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_REGION);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.region=region;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }

}

void send_mute_rx(int s,int mute) {
  MUTE_RX_COMMAND command;

g_print("send_mute_rx mute=%d\n",mute);
  command.header.sync=REMOTE_SYNC;
  command.header.data_type=htons(CMD_RESP_MUTE_RX);
  command.header.version=htonl(CLIENT_SERVER_VERSION);
  command.mute=mute;
  int bytes_sent=send_bytes(s,(char *)&command,sizeof(command));
  if(bytes_sent<0) {
    perror("send_command");
  } else {
    //g_print("send_command: %d\n",bytes_sent);
  }

}


static void *listen_thread(void *arg) {
  int address_length;
  struct sockaddr_in address;
  REMOTE_CLIENT* client;
  int rc;
  int on=1;

g_print("hpsdr_server: listening on port %d\n",listen_port);
  while(running) {
    // create TCP socket to listen on
    listen_socket=socket(AF_INET,SOCK_STREAM,0);
    if(listen_socket<0) {
      g_print("listen_thread: socket failed\n");
      return NULL;
    }

    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

    // bind to listening port
    memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(listen_port);
    if(bind(listen_socket,(struct sockaddr*)&address,sizeof(address))<0) {
      g_print("listen_thread: bind failed\n");
      return NULL;
    }

  // listen for connections
    if(listen(listen_socket,5)<0) {
      g_print("listen_thread: listen failed\n");
      break;
    }
    REMOTE_CLIENT* client=g_new(REMOTE_CLIENT,1);
    client->spectrum_update_timer_id=-1;
    client->address_length=sizeof(client->address);
    client->running=TRUE;
g_print("hpsdr_server: accept\n");
    if((client->socket=accept(listen_socket,(struct sockaddr*)&client->address,&client->address_length))<0) {
      g_print("listen_thread: accept failed\n");
      g_free(client);
      continue;
    }
 char s[128];
 inet_ntop(AF_INET, &(((struct sockaddr_in *)&client->address)->sin_addr),s,128);
g_print("Client_connected from %s\n",s);
    client->thread_id=g_thread_new("SSDR_client",server_client_thread,client);
    add_client(client);
    close(listen_socket);
    gpointer thread_result=g_thread_join(client->thread_id);
  }
  return NULL;
}

int create_hpsdr_server() {
  g_print("create_hpsdr_server\n");

  g_mutex_init(&client_mutex);
  clients=NULL;
  running=TRUE;
  listen_thread_id = g_thread_new( "HPSDR_listen", listen_thread, NULL);
  return 0;
}

int destroy_hpsdr_server() {
g_print("destroy_hpsdr_server\n");
  running=FALSE;
  return 0;
}

// CLIENT Code

static gint check_vfo(void *arg) {
  if(!running) return FALSE;

  g_mutex_lock(&accumulated_mutex);

  if(accumulated_steps!=0) {
    send_vfo_step(client_socket,active_receiver->id,accumulated_steps);
    accumulated_steps=0;
  }

  if(accumulated_hz!=0LL || accumulated_round) {
    send_vfo_move(client_socket,active_receiver->id,accumulated_hz,accumulated_round);
    accumulated_hz=0LL;
    accumulated_round=FALSE;
  }

  g_mutex_unlock(&accumulated_mutex);

  return TRUE;
}

static char server_host[128];
static int delay=0;

gint start_spectrum(void *data) {
  RECEIVER *rx=(RECEIVER *)data;

  if(delay!=3) {
    delay++;
g_print("start_spectrum: delay %d\n",delay);
    return TRUE;
  }
  send_start_spectrum(client_socket,rx->id);
  return FALSE;
}

void start_vfo_timer() {
  g_mutex_init(&accumulated_mutex);
  check_vfo_timer_id=gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE,100,check_vfo, NULL, NULL);
g_print("check_vfo_timer_id %d\n",check_vfo_timer_id);
}

static void *client_thread(void* arg) {
  gint bytes_read;
#define MAX_BUFFER 2400
  char buffer[MAX_BUFFER];
  char *token;
  HEADER header;
  char *server=(char *)arg;

  running=TRUE;

  while(running) {
    bytes_read=recv_bytes(client_socket,(char *)&header,sizeof(header));
    if(bytes_read<0) {
      g_print("client_thread: read %d bytes for HEADER\n",bytes_read);
      perror("client_thread");
      // dialog box?
      return NULL;
    }

    switch(ntohs(header.data_type)) {
      case INFO_RADIO:
        {
        RADIO_DATA radio_data;
        bytes_read=recv_bytes(client_socket,(char *)&radio_data.name,sizeof(radio_data)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for RADIO_DATA\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }

g_print("INFO_RADIO: %d\n",bytes_read);
        // build a radio (discovered) structure
        radio=g_new(DISCOVERED,1);
        strcpy(radio->name,radio_data.name);
        radio->protocol=ntohs(radio_data.protocol);
        radio->device=ntohs(radio_data.device);
        uint64_t temp=ntohll(radio_data.frequency_min);
        radio->frequency_min=(double)temp;
        temp=ntohll(radio_data.frequency_max);
        radio->frequency_max=(double)temp;
        radio->supported_receivers=ntohs(radio_data.supported_receivers);
        temp=ntohll(radio_data.sample_rate);
        radio_sample_rate=(int)temp;
#ifdef SOAPYSDR
        if(protocol==SOAPYSDR_PROTOCOL) {
          radio->info.soapy.sample_rate=(int)temp;
        }
#endif
        display_filled=radio_data.display_filled;
        locked=radio_data.locked;
        receivers=ntohs(radio_data.receivers);
        can_transmit=radio_data.can_transmit;
//TEMP
can_transmit=0;
        step=ntohll(radio_data.step);
        split=radio_data.split;
        sat_mode=radio_data.sat_mode;
        duplex=radio_data.duplex;
        protocol=radio->protocol;
        have_rx_gain=radio_data.have_rx_gain;
        short s=ntohs(radio_data.rx_gain_calibration);
        rx_gain_calibration=(int)s;
        filter_board=ntohs(radio_data.filter_board);
g_print("have_rx_gain=%d rx_gain_calibration=%d filter_board=%d\n",have_rx_gain,rx_gain_calibration,filter_board);
//
// A semaphore for safely writing to the props file
//
        g_mutex_init(&property_mutex);

        sprintf(title,"piHPSDR: %s remote at %s",radio->name,server);
        g_idle_add(ext_set_title,(void *)title);

        }
        break;
      case INFO_ADC:
        {
        ADC_DATA adc_data;
        bytes_read=recv_bytes(client_socket,(char *)&adc_data.adc,sizeof(adc_data)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for ADC_DATA\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
g_print("INFO_ADC: %d\n",bytes_read);
        int i=adc_data.adc;
        adc[i].filters=ntohs(adc_data.filters);
        adc[i].hpf=ntohs(adc_data.hpf);
        adc[i].lpf=ntohs(adc_data.lpf);
        adc[i].antenna=ntohs(adc_data.antenna);
        adc[i].dither=adc_data.dither;
        adc[i].random=adc_data.random;
        adc[i].preamp=adc_data.preamp;
        adc[i].attenuation=ntohs(adc_data.attenuation);
        adc_attenuation[i]=ntohs(adc_data.adc_attenuation);
        }
        break;
      case INFO_RECEIVER:
        {
        RECEIVER_DATA receiver_data;
        bytes_read=recv_bytes(client_socket,(char *)&receiver_data.rx,sizeof(receiver_data)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for RECEIVER_DATA\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }

g_print("INFO_RECEIVER: %d\n",bytes_read);
        int rx=receiver_data.rx;
        receiver[rx]=g_new(RECEIVER,1);
        receiver[rx]->id=rx;
        receiver[rx]->adc=ntohs(receiver_data.adc);;
        long long rate=ntohll(receiver_data.sample_rate);
        receiver[rx]->sample_rate=(int)rate;
        receiver[rx]->displaying=receiver_data.displaying;
        receiver[rx]->display_panadapter=receiver_data.display_panadapter;
        receiver[rx]->display_waterfall=receiver_data.display_waterfall;
        receiver[rx]->fps=ntohs(receiver_data.fps);
        receiver[rx]->agc=receiver_data.agc;
        short s=ntohs(receiver_data.agc_hang);
        receiver[rx]->agc_hang=(double)s;
        s=ntohs(receiver_data.agc_thresh);
        receiver[rx]->agc_thresh=(double)s;
        receiver[rx]->nb=receiver_data.nb;
        receiver[rx]->nb2=receiver_data.nb2;
        receiver[rx]->nr=receiver_data.nr;
        receiver[rx]->nr2=receiver_data.nr2;
        receiver[rx]->anf=receiver_data.anf;
        receiver[rx]->snb=receiver_data.snb;
        s=ntohs(receiver_data.filter_low);
        receiver[rx]->filter_low=(int)s;
        s=ntohs(receiver_data.filter_high);
        receiver[rx]->filter_high=(int)s;
        s=ntohs(receiver_data.panadapter_low);
        receiver[rx]->panadapter_low=(int)s;
        s=ntohs(receiver_data.panadapter_high);
        receiver[rx]->panadapter_high=(int)s;
        s=ntohs(receiver_data.panadapter_step);
        receiver[rx]->panadapter_step=s;
        s=ntohs(receiver_data.waterfall_low);
        receiver[rx]->waterfall_low=(int)s;
        s=ntohs(receiver_data.waterfall_high);
        receiver[rx]->waterfall_high=s;
        receiver[rx]->waterfall_automatic=receiver_data.waterfall_automatic;
        receiver[rx]->pixels=ntohs(receiver_data.pixels);
        receiver[rx]->zoom=ntohs(receiver_data.zoom);
        receiver[rx]->pan=ntohs(receiver_data.pan);
        receiver[rx]->width=ntohs(receiver_data.width);
        receiver[rx]->height=ntohs(receiver_data.height);
        receiver[rx]->x=ntohs(receiver_data.x);
        receiver[rx]->y=ntohs(receiver_data.y);
        s=ntohs(receiver_data.volume);
        receiver[rx]->volume=(double)s/100.0;
        s=ntohs(receiver_data.rf_gain);
        receiver[rx]->rf_gain=(double)s;
        s=ntohs(receiver_data.agc_gain);
        receiver[rx]->agc_gain=(double)s;
//
        receiver[rx]->pixel_samples=NULL;
        g_mutex_init(&receiver[rx]->display_mutex);
        receiver[rx]->hz_per_pixel=(double)receiver[rx]->sample_rate/(double)receiver[rx]->pixels;

        receiver[rx]->playback_handle=NULL;
        receiver[rx]->local_audio_buffer=NULL;
        receiver[rx]->local_audio_buffer_size=2048;
        receiver[rx]->local_audio=0;
        g_mutex_init(&receiver[rx]->local_audio_mutex);
        receiver[rx]->audio_name=NULL;
        receiver[rx]->mute_when_not_active=0;
        receiver[rx]->audio_channel=STEREO;
        receiver[rx]->audio_device=-1;
        receiver[rx]->mute_radio=0;

g_print("rx=%d width=%d sample_rate=%d hz_per_pixel=%f pan=%d zoom=%d\n",rx,receiver[rx]->width,receiver[rx]->sample_rate,receiver[rx]->hz_per_pixel,receiver[rx]->pan,receiver[rx]->zoom);
        }
        break;
      case INFO_TRANSMITTER:
        {
g_print("INFO_TRANSMITTER\n");
        }
        break;
      case INFO_VFO:
        {
        VFO_DATA vfo_data;
        bytes_read=recv_bytes(client_socket,(char *)&vfo_data.vfo,sizeof(vfo_data)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for VFO_DATA\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }

g_print("INFO_VFO: %d\n",bytes_read);

        int v=vfo_data.vfo;
        vfo[v].band=ntohs(vfo_data.band);
        vfo[v].bandstack=ntohs(vfo_data.bandstack);
        vfo[v].frequency=ntohll(vfo_data.frequency);
        vfo[v].mode=ntohs(vfo_data.mode);
        vfo[v].filter=ntohs(vfo_data.filter);
        vfo[v].ctun=vfo_data.ctun;
        vfo[v].ctun_frequency=ntohll(vfo_data.ctun_frequency);
        vfo[v].rit_enabled=vfo_data.rit_enabled;
        vfo[v].rit=ntohll(vfo_data.rit);
        vfo[v].lo=ntohll(vfo_data.lo);
        vfo[v].offset=ntohll(vfo_data.offset);

        // when VFO-B is initialized we can create the visual. start the MIDI interface and start the data flowing
        if(v==VFO_B && !remote_started) {
g_print("g_idle_add: remote_start\n");
          g_idle_add(remote_start,(gpointer)server);
        } else if(remote_started) {
g_print("g_idle_add: ext_vfo_update\n");
          g_idle_add(ext_vfo_update,(gpointer)NULL);
        }
        }
        break;
      case INFO_SPECTRUM:
        {
        SPECTRUM_DATA spectrum_data;
        bytes_read=recv_bytes(client_socket,(char *)&spectrum_data.rx,sizeof(spectrum_data)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for SPECTRUM_DATA\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int r=spectrum_data.rx;
        long long frequency_a=ntohll(spectrum_data.vfo_a_freq);
        long long frequency_b=ntohll(spectrum_data.vfo_b_freq);
        long long ctun_frequency_a=ntohll(spectrum_data.vfo_a_ctun_freq);
        long long ctun_frequency_b=ntohll(spectrum_data.vfo_b_ctun_freq);
        long long offset_a=ntohll(spectrum_data.vfo_a_offset);
        long long offset_b=ntohll(spectrum_data.vfo_b_offset);
        short meter=ntohs(spectrum_data.meter);
        receiver[r]->meter=(double)meter;
        short samples=ntohs(spectrum_data.samples);
        if(receiver[r]->pixel_samples==NULL) {
          receiver[r]->pixel_samples=g_new(float,(int)samples);
        }

        short sample;
        for(int i=0;i<samples;i++) {
          sample=ntohs(spectrum_data.sample[i]);
          receiver[r]->pixel_samples[i]=(float)sample;
        }
        if(vfo[VFO_A].frequency!=frequency_a || vfo[VFO_B].frequency!=frequency_b || vfo[VFO_A].ctun_frequency!=ctun_frequency_a || vfo[VFO_B].ctun_frequency!=ctun_frequency_b || vfo[VFO_A].offset!=offset_a || vfo[VFO_B].offset!=offset_b) {
          vfo[VFO_A].frequency=frequency_a;
          vfo[VFO_B].frequency=frequency_b;
          vfo[VFO_A].ctun_frequency=ctun_frequency_a;
          vfo[VFO_B].ctun_frequency=ctun_frequency_b;
          vfo[VFO_A].offset=offset_a;
          vfo[VFO_B].offset=offset_b;
          g_idle_add(ext_vfo_update,(gpointer)NULL);
        }
        g_idle_add(ext_receiver_remote_update_display,receiver[r]);
        }
        break;
      case INFO_AUDIO:
        {
        AUDIO_DATA audio_data;
        bytes_read=recv_bytes(client_socket,(char *)&audio_data.rx,sizeof(audio_data)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for AUDIO_DATA\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        RECEIVER *rx=receiver[audio_data.rx];
        short left_sample;
        short right_sample;
        int samples=ntohs(audio_data.samples);
        if(rx->local_audio) {
          for(int i=0;i<samples;i++) {
            left_sample=ntohs(audio_data.sample[(i*2)]);
            right_sample=ntohs(audio_data.sample[(i*2)+1]);
            audio_write(rx,(float)left_sample/32767.0,(float)right_sample/32767.0);
          }
        }
        }
        break;
      case CMD_RESP_RX_ZOOM:
        {
        ZOOM_COMMAND zoom_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&zoom_cmd.id,sizeof(zoom_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for ZOOM_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=zoom_cmd.id;
        short zoom=ntohs(zoom_cmd.zoom);
g_print("CMD_RESP_RX_ZOOM: zoom=%d rx[%d]->zoom=%d\n",zoom,rx,receiver[rx]->zoom);
        if(receiver[rx]->zoom!=zoom) {
          g_idle_add(ext_remote_set_zoom,GINT_TO_POINTER(zoom));
        } else {
          receiver_change_zoom(receiver[rx],(double)zoom);
        }
        }
        break;
      case CMD_RESP_RX_PAN:
        {
        PAN_COMMAND pan_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&pan_cmd.id,sizeof(pan_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for PAN_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=pan_cmd.id;
        short pan=ntohs(pan_cmd.pan);
g_print("CMD_RESP_RX_PAN: pan=%d rx[%d]->pan=%d\n",pan,rx,receiver[rx]->pan);
        g_idle_add(ext_remote_set_pan,GINT_TO_POINTER(pan));
        }
        break;
      case CMD_RESP_RX_VOLUME:
        {
        VOLUME_COMMAND volume_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&volume_cmd.id,sizeof(volume_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for VOLUME_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=volume_cmd.id;
        short volume=ntohs(volume_cmd.volume);
g_print("CMD_RESP_RX_VOLUME: volume=%d rx[%d]->volume=%f\n",volume,rx,receiver[rx]->volume);
        receiver[rx]->volume=(double)volume/100.0;
        }
        break;
      case CMD_RESP_RX_AGC:
        {
        AGC_COMMAND agc_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&agc_cmd.id,sizeof(agc_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for AGC_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=agc_cmd.id;
        short a=ntohs(agc_cmd.agc);
g_print("AGC_COMMAND: rx=%d agc=%d\n",rx,a);
        receiver[rx]->agc=(int)a;
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        }
        break;
      case CMD_RESP_RX_AGC_GAIN:
        {
        AGC_GAIN_COMMAND agc_gain_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&agc_gain_cmd.id,sizeof(agc_gain_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for AGC_GAIN_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=agc_gain_cmd.id;
        short gain=ntohs(agc_gain_cmd.gain);
        short hang=ntohs(agc_gain_cmd.hang);
        short thresh=ntohs(agc_gain_cmd.thresh);
        receiver[rx]->agc_gain=(double)gain;
        receiver[rx]->agc_hang=(double)hang;
        receiver[rx]->agc_thresh=(double)thresh;
        }
        break;
      case CMD_RESP_RX_ATTENUATION:
        {
        ATTENUATION_COMMAND attenuation_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&attenuation_cmd.id,sizeof(attenuation_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for ATTENUATION_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=attenuation_cmd.id;
        short attenuation=ntohs(attenuation_cmd.attenuation);
g_print("CMD_RESP_RX_ATTENUATION: attenuation=%d adc_attenuation[rx[%d]->adc]=%d\n",attenuation,rx,adc_attenuation[receiver[rx]->adc]);
        adc_attenuation[receiver[rx]->adc]=attenuation;
        }
        break;
      case CMD_RESP_RX_NOISE:
        {
        NOISE_COMMAND noise_command;
        bytes_read=recv_bytes(client_socket,(char *)&noise_command.id,sizeof(noise_command)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for NOISE_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        RECEIVER *rx=receiver[noise_command.id];
        rx->nb=noise_command.nb;
        rx->nb2=noise_command.nb2;
        mode_settings[vfo[rx->id].mode].nb=rx->nb;
        mode_settings[vfo[rx->id].mode].nb2=rx->nb2;
        rx->nr=noise_command.nr;
        rx->nr2=noise_command.nr2;
        mode_settings[vfo[rx->id].mode].nr=rx->nr;
        mode_settings[vfo[rx->id].mode].nr2=rx->nr2;
        rx->snb=noise_command.snb;
        mode_settings[vfo[rx->id].mode].snb=rx->snb;
        rx->anf=noise_command.anf;
        mode_settings[vfo[rx->id].mode].anf=rx->anf;
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        }
        break;
      case CMD_RESP_RX_MODE:
        {
        MODE_COMMAND mode_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&mode_cmd.id,sizeof(mode_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for MODE_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=mode_cmd.id;
        short m=ntohs(mode_cmd.mode);
        vfo[rx].mode=m;
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        }
        break;
      case CMD_RESP_RX_FILTER:
        {
        FILTER_COMMAND filter_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&filter_cmd.id,sizeof(filter_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for FILTER_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=filter_cmd.id;
        short low=ntohs(filter_cmd.filter_low);
        short high=ntohs(filter_cmd.filter_high);
        receiver[rx]->filter_low=(int)low;
        receiver[rx]->filter_high=(int)high;
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        }
        break;
      case CMD_RESP_SPLIT:
        {
        SPLIT_COMMAND split_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&split_cmd.split,sizeof(split_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for SPLIT_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        split=split_cmd.split;
        }
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        break;
      case CMD_RESP_SAT:
        {
        SAT_COMMAND sat_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&sat_cmd.sat,sizeof(sat_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for SAT_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        sat_mode=sat_cmd.sat;
        }
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        break;
      case CMD_RESP_DUP:
        {
        DUP_COMMAND dup_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&dup_cmd.dup,sizeof(dup_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for DUP_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        duplex=dup_cmd.dup;
        }
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        break;
      case CMD_RESP_LOCK:
        {
        LOCK_COMMAND lock_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&lock_cmd.lock,sizeof(lock_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for LOCK_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        locked=lock_cmd.lock;
        }
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        break;
      case CMD_RESP_RX_FPS:
        {
        FPS_COMMAND fps_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&fps_cmd.id,sizeof(fps_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for FPS_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=fps_cmd.id;
        receiver[rx]->fps=(int)fps_cmd.fps;
        }
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        break;
      case CMD_RESP_RX_SELECT:
        {
        RX_SELECT_COMMAND rx_select_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&rx_select_cmd.id,sizeof(rx_select_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for RX_SELECT_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=rx_select_cmd.id;
        active_receiver=receiver[rx];
        }
        g_idle_add(ext_vfo_update,(gpointer)NULL);
        break;
     case CMD_RESP_SAMPLE_RATE:
        {
        SAMPLE_RATE_COMMAND sample_rate_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&sample_rate_cmd.id,sizeof(sample_rate_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for SAMPLE_RATE_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int rx=(int)sample_rate_cmd.id;
        long long rate=ntohll(sample_rate_cmd.sample_rate);
g_print("CMD_RESP_SAMPLE_RATE: rx=%d rate=%lld\n",rx,rate);
        if(rx==-1) {
          radio_sample_rate=(int)rate;
          for(rx=0;rx<receivers;rx++) {
            receiver[rx]->sample_rate=(int)rate;
            receiver[rx]->hz_per_pixel=(double)receiver[rx]->sample_rate/(double)receiver[rx]->pixels;
          }
        } else {
          receiver[rx]->sample_rate=(int)rate;
          receiver[rx]->hz_per_pixel=(double)receiver[rx]->sample_rate/(double)receiver[rx]->pixels;
        }
        }
        break;
     case CMD_RESP_RECEIVERS:
        {
        RECEIVERS_COMMAND receivers_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&receivers_cmd.receivers,sizeof(receivers_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for RECEIVERS_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int r=(int)receivers_cmd.receivers;
g_print("CMD_RESP_RECEIVERS: receivers=%d\n",r);
        radio_change_receivers(r);
        }
        break;
      case CMD_RESP_RIT_INCREMENT:
        {
        RIT_INCREMENT_COMMAND rit_increment_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&rit_increment_cmd.increment,sizeof(rit_increment_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for RIT_INCREMENT_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        int increment=ntohs(rit_increment_cmd.increment);
g_print("CMD_RESP_RIT_INCREMENT: increment=%d\n",increment);
        rit_increment=increment;
        }
        break;
      case CMD_RESP_FILTER_BOARD:
        {
        FILTER_BOARD_COMMAND filter_board_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&filter_board_cmd.filter_board,sizeof(filter_board_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for FILTER_BOARD_CMD\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        filter_board=(int)filter_board_cmd.filter_board;
g_print("CMD_RESP_FILTER_BOARD: board=%d\n",filter_board);
        }
        break;
      case CMD_RESP_SWAP_IQ:
        {
        SWAP_IQ_COMMAND swap_iq_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&swap_iq_cmd.iqswap,sizeof(swap_iq_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for SWAP_IQ_COMMAND\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        iqswap=(int)swap_iq_cmd.iqswap;
g_print("CMD_RESP_IQ_SWAP: iqswap=%d\n",iqswap);
        }
        break;
      case CMD_RESP_REGION:
        {
        REGION_COMMAND region_cmd;
        bytes_read=recv_bytes(client_socket,(char *)&region_cmd.region,sizeof(region_cmd)-sizeof(header));
        if(bytes_read<0) {
          g_print("client_thread: read %d bytes for REGION_COMMAND\n",bytes_read);
          perror("client_thread");
          // dialog box?
          return NULL;
        }
        region=(int)region_cmd.region;
g_print("CMD_RESP_REGION: region=%d\n",region);
        }
        break;

      default:
g_print("client_thread: Unknown type=%d\n",ntohs(header.data_type));
        break;
    }
  }
  return NULL;
}

int radio_connect_remote(char *host, int port) {
  struct sockaddr_in server_address;
  gint on=1;

g_print("radio_connect_remote: %s:%d\n",host,port);
  client_socket=socket(AF_INET, SOCK_STREAM, 0); 
  if(client_socket==-1) { 
    g_print("radio_connect_remote: socket creation failed...\n"); 
    return -1;
  } 

  setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  setsockopt(client_socket, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

  struct hostent *server=gethostbyname(host);

  if(server == NULL) {
    g_print("radio_connect_remote: no such host: %s\n",host);
    return -1;
  }

  // assign IP, PORT and bind to address
  memset(&server_address,0,sizeof(server_address));
  server_address.sin_family = AF_INET; 
  bcopy((char *)server->h_addr,(char *)&server_address.sin_addr.s_addr,server->h_length);
  server_address.sin_port = htons((short)port); 

  if(connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) { 
    g_print("client_thread: connect failed\n");
    perror("client_thread");
    return -1;
  }

g_print("radio_connect_remote: socket %d bound to %s:%d\n",client_socket,host,port);
  sprintf(server_host,"%s:%d",host,port);
  client_thread_id=g_thread_new("remote_client",client_thread,&server_host);
  return 0;
}
