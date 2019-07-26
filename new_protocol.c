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


//#define ECHO_MIC

#include <gtk/gtk.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <semaphore.h>
#include <math.h>

#include <wdsp.h>

#include "alex.h"
#include "audio.h"
#include "band.h"
#include "new_protocol.h"
#include "channel.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "signal.h"
#include "vfo.h"
#include "toolbar.h"
#ifdef FREEDV
#include "freedv.h"
#endif
#include "vox.h"
#include "ext.h"

#define min(x,y) (x<y?x:y)

#define PI 3.1415926535897932F

/*
 * A new 'action table' defines what to to
 * with a sample packet received from a DDC
 */

#define RXACTION_SKIP   0    // skip samples
#define RXACTION_NORMAL 1    // deliver 238 samples to a receiver
#define RXACTION_PS     2    // deliver 2*119 samples to PS engine
#define RXACTION_DIV    3    // take 2*119 samples, mix them, deliver to a receiver

static int rxcase[MAX_DDC];
static int rxid  [MAX_DDC];

int data_socket=-1;

static int running;

#ifdef __APPLE__
sem_t *response_sem;
#else
sem_t response_sem;
#endif

static struct sockaddr_in base_addr;
static int base_addr_length;

static struct sockaddr_in receiver_addr;
static int receiver_addr_length;

static struct sockaddr_in transmitter_addr;
static int transmitter_addr_length;

static struct sockaddr_in high_priority_addr;
static int high_priority_addr_length;

static struct sockaddr_in audio_addr;
static int audio_addr_length;

static struct sockaddr_in iq_addr;
static int iq_addr_length;

static struct sockaddr_in data_addr[MAX_DDC];
static int data_addr_length[MAX_DDC];

static GThread *new_protocol_thread_id;
static GThread *new_protocol_timer_thread_id;

static long high_priority_sequence = 0;
static long general_sequence = 0;
static long rx_specific_sequence = 0;
static long tx_specific_sequence = 0;
static long ddc_sequence[MAX_DDC];

//static int buffer_size=BUFFER_SIZE;
//static int fft_size=4096;
static int dspRate=48000;
static int outputRate=48000;

static int micSampleRate=48000;
static int micDspRate=48000;
static int micOutputRate=192000;
static int micoutputsamples;  // 48000 in, 192000 out

static double micinputbuffer[MAX_BUFFER_SIZE*2]; // 48000
static double iqoutputbuffer[MAX_BUFFER_SIZE*4*2]; //192000

static long tx_iq_sequence;
static unsigned char iqbuffer[1444];
static int iqindex;
static int micsamples;

static int spectrumWIDTH=800;
static int SPECTRUM_UPDATES_PER_SECOND=10;

static float phase = 0.0F;

static long response_sequence=0;
static long highprio_rcvd_sequence=0;
static long micsamples_sequence=0;

static int response;

//static sem_t send_high_priority_sem;
//static int send_high_priority=0;
//static sem_t send_general_sem;
//static int send_general=0;

#ifdef __APPLE__
static sem_t *command_response_sem_ready;
static sem_t *command_response_sem_buffer;
#else
static sem_t command_response_sem_ready;
static sem_t command_response_sem_buffer;
#endif
static GThread *command_response_thread_id;
#ifdef __APPLE__
static sem_t *high_priority_sem_ready;
static sem_t *high_priority_sem_buffer;
#else
static sem_t high_priority_sem_ready;
static sem_t high_priority_sem_buffer;
#endif
static GThread *high_priority_thread_id;
#ifdef __APPLE__
static sem_t *mic_line_sem_ready;
static sem_t *mic_line_sem_buffer;
#else
static sem_t mic_line_sem_ready;
static sem_t mic_line_sem_buffer;
#endif
static GThread *mic_line_thread_id;
#ifdef __APPLE__
static sem_t *iq_sem_ready[MAX_DDC];
static sem_t *iq_sem_buffer[MAX_DDC];
#else
static sem_t iq_sem_ready[MAX_DDC];
static sem_t iq_sem_buffer[MAX_DDC];
#endif
static GThread *iq_thread_id[MAX_DDC];

#ifdef INCLUDED
static int outputsamples;
#endif

static int leftaudiosample;
static int rightaudiosample;
static long audiosequence;
static unsigned char audiobuffer[260]; // was 1444
static int audioindex;

#ifdef PSK
static int psk_samples=0;
static int psk_resample=6;  // convert from 48000 to 8000
#endif

// Use this to determine the source port of messages received
static struct sockaddr_in addr;
static socklen_t length=sizeof(addr);

// Network buffers
#define NET_BUFFER_SIZE 2048
static unsigned char *iq_buffer[MAX_DDC];
static unsigned char *command_response_buffer;
static unsigned char *high_priority_buffer;
static unsigned char *mic_line_buffer;
static int mic_bytes_read;

static unsigned char general_buffer[60];
static unsigned char high_priority_buffer_to_radio[1444];
static unsigned char transmit_specific_buffer[60];
static unsigned char receive_specific_buffer[1444];

// DL1YCF
// new_protocol_receive_specific and friends are not thread-safe, but called
// periodically from  timer thread and asynchronously from everywhere else
// therefore we need to implement a critical section for each of these functions.
// It seems that this is not necessary for the audio and TX-IQ buffers.

static pthread_mutex_t rx_spec_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tx_spec_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t hi_prio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t general_mutex = PTHREAD_MUTEX_INITIALIZER;

static int local_ptt=0;

static void new_protocol_high_priority();
static void new_protocol_general();
static void new_protocol_receive_specific();
static void new_protocol_transmit_specific();
static gpointer new_protocol_thread(gpointer data);
static gpointer new_protocol_timer_thread(gpointer data);
static gpointer command_response_thread(gpointer data);
static gpointer high_priority_thread(gpointer data);
static gpointer mic_line_thread(gpointer data);
static gpointer iq_thread(gpointer data);
static void  process_iq_data(unsigned char *buffer, RECEIVER *rx);
static void  process_ps_iq_data(unsigned char *buffer);
static void process_div_iq_data(unsigned char *buffer);
static void  process_command_response();
static void  process_high_priority();
static void  process_mic_data(int bytes);

#ifdef INCLUDED
static void new_protocol_calc_buffers() {
  switch(sample_rate) {
    case 48000:
      outputsamples=buffer_size;
      break;
    case 96000:
      outputsamples=buffer_size/2;
      break;
    case 192000:
      outputsamples=buffer_size/4;
      break;
    case 384000:
      outputsamples=buffer_size/8;
      break;
    case 768000:
      outputsamples=buffer_size/16;
      break;
    case 1536000:
      outputsamples=buffer_size/32;
      break;
  }
}
#endif

void schedule_high_priority() {
    new_protocol_high_priority();
}

void schedule_general() {
    new_protocol_general();
}

void schedule_receive_specific() {
    new_protocol_receive_specific();
}

void schedule_transmit_specific() {
    new_protocol_transmit_specific();
}

void filter_board_changed() {
    schedule_general();
}

/*
void pa_changed() {
    schedule_general();
}

void tuner_changed() {
    schedule_general();
}
*/

void update_action_table() {
  //
  // Depending on the values of mox, puresignal, and diversity,
  // determine the actions to be taken when a DDC packet arrives
  //
  int flag;
  flag=0;
  if (device==NEW_DEVICE_ANGELIA || device==NEW_DEVICE_ORION || device == NEW_DEVICE_ORION2) flag +=1000;
  if (isTransmitting())                                                                      flag +=100;
  if (transmitter->puresignal)                                                               flag +=10;
  if (diversity_enabled)                                                                     flag +=1;

  //
  // Set up rxcase and rxid for each of the 16 possible cases
  // note that rxid[i] can be left unspecified if rxcase[i] == RXACTION_SKIP
  //
  rxcase[0] = RXACTION_SKIP;
  rxcase[1] = RXACTION_SKIP;
  rxcase[2] = RXACTION_SKIP;
  rxcase[3] = RXACTION_SKIP;
  switch (flag) {
    case    0:								// HERMES, RX, no DIVERSITY
    case   10:
	rxid[0]=0;
	rxcase[0] = RXACTION_NORMAL;
        if (receivers > 1) {
	  rxid[1]=1;
	  rxcase[1] = RXACTION_NORMAL;
        }
	break;
    case    1:								// HERMES, RX, DIVERSITY
    case   11:								// ORION, RX, DIVERSITY
    case 1001:
    case 1011:
	rxid[0]=0;
	rxcase[0] = RXACTION_DIV;
	break;
    case  100:								// HERMES, TX, no PURESIGNAL
    case  101:
    case 1100:								// ORION, TX, no PURESIGNAL
    case 1101:								// ORION, TX, no PURESIGNAL
	// just skip samples
	break;
    case  110:								// HERMES, TX, PURESIGNAL
    case  111:
    case 1110:								// ORION, TX, PURESIGNAL
    case 1111:
	rxcase[0] = RXACTION_PS;
	break;
    case 1000:								// ORION, RX, no DIVERSITY
    case 1010:
	rxid[2]=0;
	rxcase[2] = RXACTION_NORMAL;
        if (receivers > 1) {
	  rxid[3]=1;
	  rxcase[3] = RXACTION_NORMAL;
        }
	break;
  }
}

void new_protocol_init(int pixels) {
    int i;
    int rc;
    spectrumWIDTH=pixels;

    fprintf(stderr,"new_protocol_init: MIC_SAMPLES=%d\n",MIC_SAMPLES);

    memset(rxcase      , 0, MAX_DDC*sizeof(int));
    memset(rxid        , 0, MAX_DDC*sizeof(int));
    memset(ddc_sequence, 0, MAX_DDC*sizeof(long));
    update_action_table();

#ifdef INCLUDED
    outputsamples=buffer_size;
#endif
    micoutputsamples=buffer_size*4;

#ifdef OLD_AUDIO
    if(local_audio) {
      if(audio_open_output()!=0) {
        fprintf(stderr,"audio_open_output failed\n");
        local_audio=0;
      }
    }
#endif

    if(transmitter->local_microphone) {
      if(audio_open_input()!=0) {
        fprintf(stderr,"audio_open_input failed\n");
        transmitter->local_microphone=0;
      }
    }

#ifdef INCLUDED
    new_protocol_calc_buffers();
#endif

#ifdef __APPLE__
    sem_unlink("RESPONSE");
    response_sem=sem_open("RESPONSE", O_CREAT | O_EXCL, 0700, 0);
    if (response_sem == SEM_FAILED) perror("ResponseSemaphore");
#else
    rc=sem_init(&response_sem, 0, 0);
#endif
    //rc=sem_init(&send_high_priority_sem, 0, 1);
    //rc=sem_init(&send_general_sem, 0, 1);

#ifdef __APPLE__
    sem_unlink("COMMRESREADY");
    command_response_sem_ready=sem_open("COMMRESREADY", O_CREAT | O_EXCL, 0700, 0);
    if (command_response_sem_ready == SEM_FAILED) perror("CommandResponseReadySemaphore");
    sem_unlink("COMMRESBUF");
    command_response_sem_buffer=sem_open("COMMRESBUF", O_CREAT | O_EXCL, 0700, 0);
    if (command_response_sem_buffer == SEM_FAILED) perror("CommandResponseBufferSemaphore");
#else
    rc=sem_init(&command_response_sem_ready, 0, 0);
    rc=sem_init(&command_response_sem_buffer, 0, 0);
#endif
    command_response_thread_id = g_thread_new( "command_response thread",command_response_thread, NULL);
    if( ! command_response_thread_id ) {
      fprintf(stderr,"g_thread_new failed on command_response_thread\n");
      exit( -1 );
    }
    fprintf(stderr, "command_response_thread: id=%p\n",command_response_thread_id);
#ifdef __APPLE__
    sem_unlink("HIGHREADY");
    high_priority_sem_ready=sem_open("HIGHREADY", O_CREAT | O_EXCL, 0700, 0);
    if (high_priority_sem_ready == SEM_FAILED) perror("HighPriorityReadySemaphore");
    sem_unlink("HIGHBUF");
    high_priority_sem_buffer=sem_open("HIGHBUF",   O_CREAT | O_EXCL, 0700, 0);
    if (high_priority_sem_buffer == SEM_FAILED) perror("HIGHPriorityBufferSemaphore");
#else
    rc=sem_init(&high_priority_sem_ready, 0, 0);
    rc=sem_init(&high_priority_sem_buffer, 0, 0);
#endif
    high_priority_thread_id = g_thread_new( "high_priority thread", high_priority_thread, NULL);
    if( ! high_priority_thread_id ) {
      fprintf(stderr,"g_thread_new failed on high_priority_thread\n");
      exit( -1 );
    }
    fprintf(stderr, "high_priority_thread: id=%p\n",high_priority_thread_id);
#ifdef __APPLE__
    sem_unlink("MICREADY");
    mic_line_sem_ready=sem_open("MICREADY", O_CREAT | O_EXCL, 0700, 0);
    if (mic_line_sem_ready == SEM_FAILED) perror("MicLineReadySemaphore");
    sem_unlink("MICBUF");
    mic_line_sem_buffer=sem_open("MICBUF",   O_CREAT | O_EXCL, 0700, 0);
    if (mic_line_sem_buffer == SEM_FAILED) perror("MicLineBufferSemaphore");
#else
    rc=sem_init(&mic_line_sem_ready, 0, 0);
    rc=sem_init(&mic_line_sem_buffer, 0, 0);
#endif
    mic_line_thread_id = g_thread_new( "mic_line thread", mic_line_thread, NULL);
    if( ! mic_line_thread_id ) {
      fprintf(stderr,"g_thread_new failed on mic_line_thread\n");
      exit( -1 );
    }
    fprintf(stderr, "mic_line_thread: id=%p\n",mic_line_thread_id);

//
//  Spawn off one IQ reading thread for each DDC to be used
//  Note that IQ reading threads are associated with DDCs and
//  not with RECEIVERs.
//
    for(i=0;i<MAX_DDC;i++) {
#ifdef __APPLE__
      char sname[12];
      sprintf(sname,"IQREADY%03d", i);
      sem_unlink(sname);
      iq_sem_ready[i]=sem_open(sname, O_CREAT | O_EXCL, 0700, 0);
      if (iq_sem_ready[i] == SEM_FAILED) {
        fprintf(stderr,"SEM=%s, ",sname);
        perror("IQreadySemaphore");
      }
      sprintf(sname,"IQBUF%03d", i);
      sem_unlink(sname);
      iq_sem_buffer[i]=sem_open(sname, O_CREAT| O_EXCL, 0700, 0);
      if (iq_sem_buffer[i] == SEM_FAILED) {
        fprintf(stderr,"SEM=%s, ",sname);
        perror("IQbufferSemaphore");
      }
#else
      rc=sem_init(&iq_sem_ready[i], 0, 0);
      rc=sem_init(&iq_sem_buffer[i], 0, 0);
#endif
      iq_thread_id[i] = g_thread_new( "iq thread", iq_thread, (gpointer)(long)i);
    }

data_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(data_socket<0) {
        fprintf(stderr,"NewProtocol: create socket failed for data_socket\n");
        exit(-1);
    }

    int optval = 1;
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // bind to the interface
    if(bind(data_socket,(struct sockaddr*)&radio->info.network.interface_address,radio->info.network.interface_length)<0) {
        fprintf(stderr,"metis: bind socket failed for data_socket\n");
        exit(-1);
    }

fprintf(stderr,"new_protocol_thread: date_socket %d bound to interface\n",data_socket);

    memcpy(&base_addr,&radio->info.network.address,radio->info.network.address_length);
    base_addr_length=radio->info.network.address_length;
    base_addr.sin_port=htons(GENERAL_REGISTERS_FROM_HOST_PORT);

    memcpy(&receiver_addr,&radio->info.network.address,radio->info.network.address_length);
    receiver_addr_length=radio->info.network.address_length;
    receiver_addr.sin_port=htons(RECEIVER_SPECIFIC_REGISTERS_FROM_HOST_PORT);

    memcpy(&transmitter_addr,&radio->info.network.address,radio->info.network.address_length);
    transmitter_addr_length=radio->info.network.address_length;
    transmitter_addr.sin_port=htons(TRANSMITTER_SPECIFIC_REGISTERS_FROM_HOST_PORT);

    memcpy(&high_priority_addr,&radio->info.network.address,radio->info.network.address_length);
    high_priority_addr_length=radio->info.network.address_length;
    high_priority_addr.sin_port=htons(HIGH_PRIORITY_FROM_HOST_PORT);

fprintf(stderr,"new_protocol_thread: high_priority_addr setup for port %d\n",HIGH_PRIORITY_FROM_HOST_PORT);

    memcpy(&audio_addr,&radio->info.network.address,radio->info.network.address_length);
    audio_addr_length=radio->info.network.address_length;
    audio_addr.sin_port=htons(AUDIO_FROM_HOST_PORT);

    memcpy(&iq_addr,&radio->info.network.address,radio->info.network.address_length);
    iq_addr_length=radio->info.network.address_length;
    iq_addr.sin_port=htons(TX_IQ_FROM_HOST_PORT);


    for(i=0;i<MAX_DDC;i++) {
        memcpy(&data_addr[i],&radio->info.network.address,radio->info.network.address_length);
        data_addr_length[i]=radio->info.network.address_length;
        data_addr[i].sin_port=htons(RX_IQ_TO_HOST_PORT_0+i);
    }

    new_protocol_thread_id = g_thread_new( "new protocol", new_protocol_thread, NULL);
    if( ! new_protocol_thread_id )
    {
        fprintf(stderr,"g_thread_new failed on new_protocol_thread\n");
        exit( -1 );
    }
    fprintf(stderr, "new_protocol_thread: id=%p\n",new_protocol_thread_id);

}

static void new_protocol_general() {
    BAND *band;

    pthread_mutex_lock(&general_mutex);
    if(split) {
      band=band_get_band(vfo[VFO_B].band);
    } else {
      band=band_get_band(vfo[VFO_A].band);
    }
    memset(general_buffer, 0, sizeof(general_buffer));

    general_buffer[0]=general_sequence>>24;
    general_buffer[1]=general_sequence>>16;
    general_buffer[2]=general_sequence>>8;
    general_buffer[3]=general_sequence;

    // use defaults apart from
    general_buffer[37]=0x08;  //  phase word (not frequency)
    general_buffer[38]=0x01;  //  enable hardware timer

    if(band->disablePA) {
      general_buffer[58]=0x00;
    } else {
      general_buffer[58]=0x01;  // enable PA
    }

// fprintf(stderr,"new_protocol_general: PA Enable=%02X\n",general_buffer[58]);

    if(filter_board==APOLLO) {
      general_buffer[58]|=0x02; // enable APOLLO tuner
    }

    if(filter_board==ALEX) {
      if(device==NEW_DEVICE_ORION2) {
        general_buffer[59]=0x03;  // enable Alex 0 and 1
      } else {
        general_buffer[59]=0x01;  // enable Alex 0
      }
    }

//fprintf(stderr,"Alex Enable=%02X\n",general_buffer[59]);

    if(sendto(data_socket,general_buffer,sizeof(general_buffer),0,(struct sockaddr*)&base_addr,base_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for general\n");
        exit(1);
    }

    general_sequence++;
    pthread_mutex_unlock(&general_mutex);
}

static void new_protocol_high_priority() {
    int i;
    BAND *band;
    long long rxFrequency;
    long long txFrequency;
    long phase;
    int mode;
    int ddc;

    if(data_socket==-1) {
      return;
    }

    pthread_mutex_lock(&hi_prio_mutex);
    memset(high_priority_buffer_to_radio, 0, sizeof(high_priority_buffer_to_radio));

    high_priority_buffer_to_radio[0]=high_priority_sequence>>24;
    high_priority_buffer_to_radio[1]=high_priority_sequence>>16;
    high_priority_buffer_to_radio[2]=high_priority_sequence>>8;
    high_priority_buffer_to_radio[3]=high_priority_sequence;

    if(split) {
      mode=vfo[1].mode;
    } else {
      mode=vfo[0].mode;
    }
    high_priority_buffer_to_radio[4]=running;
    if(mode==modeCWU || mode==modeCWL) {
      if(tune || CAT_cw_is_active) {
        high_priority_buffer_to_radio[4]|=0x02;
      }
#ifdef LOCALCW
      if (cw_keyer_internal == 0) {
        // set the ptt if we are not in breakin mode and mox is on
        if(cw_breakin == 0 && getMox()) high_priority_buffer_to_radio[4]|=0x02;
      }
#endif
      if (cw_key_state) high_priority_buffer_to_radio[5]|= 0x01;
    } else {
      if(isTransmitting()) {
        high_priority_buffer_to_radio[4]|=0x02;
      }
    }

//
//  Set DDC frequencies
//

    if (diversity_enabled) {
	//
	// Use frequency of RX1 for both DDC0 and DDC1
	// This is overridden later if we do PURESIGNAL TX
	//
        rxFrequency=vfo[0].frequency-vfo[0].lo;
        if(vfo[0].rit_enabled) {
          rxFrequency+=vfo[0].rit;
        }

        switch(vfo[0].mode) {
          case modeCWU:
            rxFrequency-=cw_keyer_sidetone_frequency;
            break;
          case modeCWL:
            rxFrequency+=cw_keyer_sidetone_frequency;
            break;
          default:
            break;
        }
 
        phase=(long)((4294967296.0*(double)rxFrequency)/122880000.0);
        high_priority_buffer_to_radio[ 9]=phase>>24;
        high_priority_buffer_to_radio[10]=phase>>16;
        high_priority_buffer_to_radio[11]=phase>>8;
        high_priority_buffer_to_radio[12]=phase;
        high_priority_buffer_to_radio[13]=phase>>24;
        high_priority_buffer_to_radio[14]=phase>>16;
        high_priority_buffer_to_radio[15]=phase>>8;
        high_priority_buffer_to_radio[16]=phase;
    } else {
	//
	// Set frequencies for all receivers
	//
	for(i=0;i<receivers;i++) {
          // note that for HERMES, receiver[i] is associated with DDC(i) but beyond
          // (that is, ANGELIA, ORION, ORION2) receiver[i] is associated with DDC(i+2)
          ddc=i;
          if (device==NEW_DEVICE_ANGELIA || device==NEW_DEVICE_ORION || device == NEW_DEVICE_ORION2) ddc=2+i;
          int v=receiver[i]->id;
          rxFrequency=vfo[v].frequency-vfo[v].lo;
          if(vfo[v].rit_enabled) {
            rxFrequency+=vfo[v].rit;
          }

          switch(vfo[v].mode) {
            case modeCWU:
              rxFrequency-=cw_keyer_sidetone_frequency;
              break;
            case modeCWL:
              rxFrequency+=cw_keyer_sidetone_frequency;
              break;
            default:
              break;
	}

	phase=(long)((4294967296.0*(double)rxFrequency)/122880000.0);
	high_priority_buffer_to_radio[9+(ddc*4)]=phase>>24;
	high_priority_buffer_to_radio[10+(ddc*4)]=phase>>16;
	high_priority_buffer_to_radio[11+(ddc*4)]=phase>>8;
	high_priority_buffer_to_radio[12+(ddc*4)]=phase;
      }
    }

//
//  Set DUC frequency
//

    if(active_receiver->id==VFO_A) {
      txFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo;
      if(split) {
        txFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo;
      }
    } else {
      txFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo;
      if(split) {
        txFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo;
      }
    }

/*
    switch(vfo[active_receiver->id].mode) {
        case modeCWU:
          txFrequency+=cw_keyer_sidetone_frequency;
          break;
        case modeCWL:
          txFrequency-=cw_keyer_sidetone_frequency;
          break;
        default:
          break;
      }
*/

    phase=(long)((4294967296.0*(double)txFrequency)/122880000.0);

    if(isTransmitting() && transmitter->puresignal) {
      //
      // Set DDC0 and DDC1 (synchronized) to the transmit frequency
      //
      high_priority_buffer_to_radio[9]=phase>>24;
      high_priority_buffer_to_radio[10]=phase>>16;
      high_priority_buffer_to_radio[11]=phase>>8;
      high_priority_buffer_to_radio[12]=phase;

      high_priority_buffer_to_radio[13]=phase>>24;
      high_priority_buffer_to_radio[14]=phase>>16;
      high_priority_buffer_to_radio[15]=phase>>8;
      high_priority_buffer_to_radio[16]=phase;
    }

    high_priority_buffer_to_radio[329]=phase>>24;
    high_priority_buffer_to_radio[330]=phase>>16;
    high_priority_buffer_to_radio[331]=phase>>8;
    high_priority_buffer_to_radio[332]=phase;

    int power=0;
    if(isTransmitting()) {
      if(tune && !transmitter->tune_use_drive) {
        power=(int)((double)transmitter->drive_level/100.0*(double)transmitter->tune_percent);
      } else {
        power=transmitter->drive_level;
      }
    }

    high_priority_buffer_to_radio[345]=power&0xFF;

    if(isTransmitting()) {

      if(split) {
        band=band_get_band(vfo[VFO_B].band);
      } else {
        band=band_get_band(vfo[VFO_A].band);
      }
      high_priority_buffer_to_radio[1401]=band->OCtx<<1;
      if(tune) {
        if(OCmemory_tune_time!=0) {
          struct timeval te;
          gettimeofday(&te,NULL);
          long long now=te.tv_sec*1000LL+te.tv_usec/1000;
          if(tune_timeout>now) {
            high_priority_buffer_to_radio[1401]|=OCtune<<1;
          }
        } else {
          high_priority_buffer_to_radio[1401]|=OCtune<<1;
        }
      }
    } else {
      band=band_get_band(vfo[VFO_A].band);
      high_priority_buffer_to_radio[1401]=band->OCrx<<1;
    }

//
//  ANAN-7000/8000: route TXout to XvtrOut out when not using PA
//
    if ((device==NEW_DEVICE_ORION2) && band->disablePA) {
      high_priority_buffer_to_radio[1400] |= ANAN7000_XVTR_OUT;
    }

//
//  ALEX bits
//
    long alex0=0x00000000;
    long alex1=0x00000000;

    if (device != NEW_DEVICE_ORION2) {
      //
      // ANAN7000 and 8000 do not have ALEX attenuators.
      // Even worse, ALEX0(14) bit used to control these attenuators
      // on ANAN-10/100/200 is now used differently.
      //
      // Note: ALEX attenuators are not much used anyway since we
      //       have step attenuators on most boards.
      //
      switch (receiver[0]->alex_attenuation) {
	case 0:
	  alex0 |= ALEX_ATTENUATION_0dB;
	  break;
	case 1:
	  alex0 |= ALEX_ATTENUATION_10dB;
	  break;
	case 2:
	  alex0 |= ALEX_ATTENUATION_20dB;
	  break;
	case 3:
	  alex0 |= ALEX_ATTENUATION_30dB;
	  break;
      }
    }

    if(isTransmitting()) {
      alex0 |= ALEX_TX_RELAY;
      if(transmitter->puresignal) {
        alex0 |= ALEX_PS_BIT;            // Bit 18
      }
    }

//
//  The following code is based upon the assumption that
//  the frequency of VFO_A is used with ADC0, and that the
//  frequency of VFO_B can safely be used to control the
//  filters of ADC1 (if there are any).
//
    rxFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo;
    switch(device) {
      case NEW_DEVICE_ORION2:
//
//	new ANAN-7000/8000 band-pass RX filters
//      This info comes from file bpf2_select.v in the
//      P1 firmware
//
        if(rxFrequency<1500000L) {
          alex0|=ALEX_ANAN7000_RX_BYPASS_BPF;
        } else if(rxFrequency<2100000L) {
          alex0|=ALEX_ANAN7000_RX_160_BPF;
        } else if(rxFrequency<5500000L) {
          alex0|=ALEX_ANAN7000_RX_80_60_BPF;
        } else if(rxFrequency<11000000L) {
          alex0|=ALEX_ANAN7000_RX_40_30_BPF;
        } else if(rxFrequency<20900000L) {
          alex0|=ALEX_ANAN7000_RX_30_20_17_BPF;
        } else if(rxFrequency<35000000L) {
          alex0|=ALEX_ANAN7000_RX_15_12_BPF;
        } else {
          alex0|=ALEX_ANAN7000_RX_10_6_PRE_BPF;
        }
        break;
      default:
//
//	Old (ANAN-100/200) high-pass filters
//      Bypass HPFs while using EXT1 for PURESIGNAL feedback!
//
	i=0;  // flag used here for "filter bypass"
	if (rxFrequency<1800000L) i=1;
#ifdef PURESIGNAL
	if (isTransmitting() && transmitter->puresignal && receiver[PS_RX_FEEDBACK]->alex_antenna == 6) i=1;
#endif
        if (i) {
          alex0|=ALEX_BYPASS_HPF;
        } else if(rxFrequency<6500000L) {
          alex0|=ALEX_1_5MHZ_HPF;
        } else if(rxFrequency<9500000L) {
          alex0|=ALEX_6_5MHZ_HPF;
        } else if(rxFrequency<13000000L) {
          alex0|=ALEX_9_5MHZ_HPF;
        } else if(rxFrequency<20000000L) {
          alex0|=ALEX_13MHZ_HPF;
        } else if(rxFrequency<50000000L) {
          alex0|=ALEX_20MHZ_HPF;
        } else {
          alex0|=ALEX_6M_PREAMP;
        }
        break;
    }

//
//   Pre-Orion2 boards: If using Ant1/2/3, the RX signal goes through the TX low-pass
//                      filters. Therefore we must set these according to the ADC0
//			(receive) frequency while RXing.
//
    if (!isTransmitting() && device != NEW_DEVICE_ORION2 && receiver[0]->alex_antenna < 3) {
	txFrequency = rxFrequency;
    }
    switch(device) {
      case NEW_DEVICE_ORION2:
        if(txFrequency>32000000) {
          alex0|=ALEX_6_BYPASS_LPF;
        } else if(txFrequency>22000000) {
          alex0|=ALEX_12_10_LPF;
        } else if(txFrequency>15000000) {
          alex0|=ALEX_17_15_LPF;
        } else if(txFrequency>8000000) {
          alex0|=ALEX_30_20_LPF;
        } else if(txFrequency>4500000) {
          alex0|=ALEX_60_40_LPF;
        } else if(txFrequency>2400000) {
          alex0|=ALEX_80_LPF;
        } else {
          alex0|=ALEX_160_LPF;
        }
        break;
      default:
        if(txFrequency>35600000) {
          alex0|=ALEX_6_BYPASS_LPF;
        } else if(txFrequency>24000000) {
          alex0|=ALEX_12_10_LPF;
        } else if(txFrequency>16500000) {
          alex0|=ALEX_17_15_LPF;
        } else if(txFrequency>8000000) {
          alex0|=ALEX_30_20_LPF;
        } else if(txFrequency>5000000) {
          alex0|=ALEX_60_40_LPF;
        } else if(txFrequency>2500000) {
          alex0|=ALEX_80_LPF;
        } else {
          alex0|=ALEX_160_LPF;
        }
        break;
    }

//
//  Set bits that route Ext1/Ext2/XVRTin to the RX
//
//  If transmitting with PURESIGNAL, we must use the alex_antenna
//  settings of the PS_RX_FEEDBACK receiver
//
//  ANAN-7000 routes signals differently (these bits have no function on ANAN-80000)
//            and uses ALEX0(14) to connnect Ext/XvrtIn to the RX.
//
    i=receiver[0]->alex_antenna;			// 0,1,2  or 3,4,5
#ifdef PURESIGNAL
    if (isTransmitting() && transmitter->puresignal) {
	i=receiver[PS_RX_FEEDBACK]->alex_antenna;   	// 0, 6, or 7
    }
#endif
    if (device == NEW_DEVICE_ORION2) i +=100;
    switch(i) {
	case 6:  // EXT 1 for PS feedback
        case 3:  // EXT 1
          alex0|=ALEX_RX_ANTENNA_EXT1;
          break;
        case 4:  // EXT 2
          alex0|=ALEX_RX_ANTENNA_EXT2;
          break;
        case 5:  // XVTR
            alex0|=ALEX_RX_ANTENNA_XVTR;
          break;
	case 7: // RX_Bypass_In for PS feedback
          alex0|=ALEX_RX_ANTENNA_BYPASS;
          break;
	case 103:	// EXT1 on ANAN-7000
	case 106:	// EXT1 on ANAN-7000 for PS feedback
	  alex0|=ALEX_ANAN7000_RX_ANT_EXT1;
	  break;
	case 104:
	  // no EXT2 jacket on ANAN7000!
	  break;
	case 105:
	  alex0|=ALEX_ANAN7000_RX_ANT_XVTR;
	  break;
	case 107:	// RxBypassIn on ANAN-7000
	  alex0|=ALEX_ANAN7000_RX_ANT_BYPASS;
	  break;
    }

//
//  Now we set the bits for Ant1/2/3 (RX and TX may be different)
//
    if(isTransmitting()) {
      i=transmitter->alex_antenna;
    } else {
      i=receiver[0]->alex_antenna;
    }
    switch(i) {
      case 0:  // ANT 1
        alex0|=ALEX_TX_ANTENNA_1;
        break;
      case 1:  // ANT 2
         alex0|=ALEX_TX_ANTENNA_2;
         break;
      case 2:  // ANT 3
        alex0|=ALEX_TX_ANTENNA_3;
        break;
      default:
	// this should not happen in TX case. Out of paranoia,
        // connect ANT1 in this case
	if (isTransmitting()) {
	  fprintf(stderr,"WARNING: illegal TX antenna chosen, using ANT1\n");
	  transmitter->alex_antenna=0;
          alex0|=ALEX_TX_ANTENNA_1;
	}
	break;
    }

    high_priority_buffer_to_radio[1432]=(alex0>>24)&0xFF;
    high_priority_buffer_to_radio[1433]=(alex0>>16)&0xFF;
    high_priority_buffer_to_radio[1434]=(alex0>>8)&0xFF;
    high_priority_buffer_to_radio[1435]=alex0&0xFF;

//fprintf(stderr,"ALEX0 bits:  %02X %02X %02X %02X for rx=%lld tx=%lld\n",high_priority_buffer_to_radio[1432],high_priority_buffer_to_radio[1433],high_priority_buffer_to_radio[1434],high_priority_buffer_to_radio[1435],rxFrequency,txFrequency);

//
//  Orion2 boards: set RX2 filters according ro VFOB frequency
//
    if (device == NEW_DEVICE_ORION2) {
	//
	// Note that while using DIVERSITY, the second RX filter settings must match
	// those of the first RX
	//
	if (diversity_enabled) {
          rxFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo;
	} else {
          rxFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo;
	}
//
//      new ANAN-7000/8000 band-pass RX filters
//      This info comes from file bpf2_select.v in the
//      P1 firmware
//
        if(rxFrequency<1500000L) {
          alex1|=ALEX_ANAN7000_RX_BYPASS_BPF;
        } else if(rxFrequency<2100000L) {
          alex1|=ALEX_ANAN7000_RX_160_BPF;
        } else if(rxFrequency<5500000L) {
          alex1|=ALEX_ANAN7000_RX_80_60_BPF;
        } else if(rxFrequency<11000000L) {
          alex1|=ALEX_ANAN7000_RX_40_30_BPF;
        } else if(rxFrequency<20900000L) {
          alex1|=ALEX_ANAN7000_RX_30_20_17_BPF;
        } else if(rxFrequency<35000000L) {
          alex1|=ALEX_ANAN7000_RX_15_12_BPF;
        } else {
          alex1|=ALEX_ANAN7000_RX_10_6_PRE_BPF;
        }

//
//      The main purpose of RX2 is DIVERSITY. Therefore,
//      ground RX2 upon TX *always*
//
	if (isTransmitting()) {
	  alex1|=ALEX1_ANAN7000_RX_GNDonTX;
	}

        high_priority_buffer_to_radio[1430]=(alex1>>8)&0xFF;
        high_priority_buffer_to_radio[1431]=alex1&0xFF;
//fprintf(stderr,"ALEX1 bits: rx1: %02X %02X for rx=%lld\n",high_priority_buffer_to_radio[1430],high_priority_buffer_to_radio[1431],rxFrequency);
    }


//
//  Upon transmitting, set the attenuator of ADC0 to the "transmitter attenuation"
//  (used in PURESIGNAL signal strength adjustment) and the attenuator of ADC1
//  to the maximum value (to protect RX2 in DIVERSITY setups).
//

    if(isTransmitting()) {
      high_priority_buffer_to_radio[1443]=transmitter->attenuation;
      high_priority_buffer_to_radio[1442]=31;
    } else {
      high_priority_buffer_to_radio[1443]=adc_attenuation[0];
      high_priority_buffer_to_radio[1442]=adc_attenuation[1];
    }

//
//  Voila mes amis. Envoyons les 1444 octets "high priority" au radio
//
    int rc;
    if((rc=sendto(data_socket,high_priority_buffer_to_radio,sizeof(high_priority_buffer_to_radio),0,(struct sockaddr*)&high_priority_addr,high_priority_addr_length))<0) {
        fprintf(stderr,"sendto socket failed for high priority: rc=%d errno=%d\n",rc,errno);
        abort();
        //exit(1);
    }

    high_priority_sequence++;
    update_action_table();
    pthread_mutex_unlock(&hi_prio_mutex);
}

static unsigned char last_50=0;

static void new_protocol_transmit_specific() {
    int mode;

    pthread_mutex_lock(&tx_spec_mutex);
    memset(transmit_specific_buffer, 0, sizeof(transmit_specific_buffer));

    transmit_specific_buffer[0]=tx_specific_sequence>>24;
    transmit_specific_buffer[1]=tx_specific_sequence>>16;
    transmit_specific_buffer[2]=tx_specific_sequence>>8;
    transmit_specific_buffer[3]=tx_specific_sequence;

    if(split) {
      mode=vfo[1].mode;
    } else {
      mode=vfo[0].mode;
    }
    transmit_specific_buffer[4]=1; // 1 DAC
    transmit_specific_buffer[5]=0; //  default no CW
    // may be using local pihpsdr OR hpsdr CW
    if(mode==modeCWU || mode==modeCWL) {
        transmit_specific_buffer[5]|=0x02;
    }
    if(cw_keys_reversed) {
        transmit_specific_buffer[5]|=0x04;
    }
    if(cw_keyer_mode==KEYER_MODE_A) {
        transmit_specific_buffer[5]|=0x08;
    }
    if(cw_keyer_mode==KEYER_MODE_B) {
        transmit_specific_buffer[5]|=0x28;
    }
    if(cw_keyer_sidetone_volume!=0) {
        transmit_specific_buffer[5]|=0x10;
    }
    if(cw_keyer_spacing) {
        transmit_specific_buffer[5]|=0x40;
    }
    if(cw_breakin) {
        transmit_specific_buffer[5]|=0x80;
    }

    transmit_specific_buffer[6]=cw_keyer_sidetone_volume; // sidetone off
    transmit_specific_buffer[7]=cw_keyer_sidetone_frequency>>8;
    transmit_specific_buffer[8]=cw_keyer_sidetone_frequency; // sidetone frequency
    transmit_specific_buffer[9]=cw_keyer_speed; // cw keyer speed
    transmit_specific_buffer[10]=cw_keyer_weight; // cw weight
    transmit_specific_buffer[11]=cw_keyer_hang_time>>8;
    transmit_specific_buffer[12]=cw_keyer_hang_time; // cw hang delay
    transmit_specific_buffer[13]=0; // rf delay
    transmit_specific_buffer[50]=0;
    if(mic_linein) {
      transmit_specific_buffer[50]|=0x01;
    }
    if(mic_boost) {
      transmit_specific_buffer[50]|=0x02;
    }
    if(mic_ptt_enabled==0) {  // set if disabled
      transmit_specific_buffer[50]|=0x04;
    }
    if(mic_ptt_tip_bias_ring) {
      transmit_specific_buffer[50]|=0x08;
    }
    if(mic_bias_enabled) {
      transmit_specific_buffer[50]|=0x10;
    }

    if(last_50!=transmit_specific_buffer[50]) {
      last_50=transmit_specific_buffer[50];
fprintf(stderr,"tx_specific: 50=%02X\n",transmit_specific_buffer[50]);
    }

    // 0..31
    transmit_specific_buffer[51]=linein_gain;

    // Attenuator for ADC0 upon TX
    transmit_specific_buffer[59]=transmitter->attenuation;

    if(sendto(data_socket,transmit_specific_buffer,sizeof(transmit_specific_buffer),0,(struct sockaddr*)&transmitter_addr,transmitter_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for tx specific\n");
        exit(1);
    }

    tx_specific_sequence++;
    pthread_mutex_unlock(&tx_spec_mutex);

}

static void new_protocol_receive_specific() {
    int i;
    int ddc;

    pthread_mutex_lock(&rx_spec_mutex);
    memset(receive_specific_buffer, 0, sizeof(receive_specific_buffer));

    receive_specific_buffer[0]=rx_specific_sequence>>24;
    receive_specific_buffer[1]=rx_specific_sequence>>16;
    receive_specific_buffer[2]=rx_specific_sequence>>8;
    receive_specific_buffer[3]=rx_specific_sequence;

    receive_specific_buffer[4]=n_adc; 	// number of ADCs

    for(i=0;i<receivers;i++) {
	// note that for HERMES, receiver[i] is associated with DDC(i) but beyond
	// (that is, ANGELIA, ORION, ORION2) receiver[i] is associated with DDC(i+2)
        ddc=i;
        if (device==NEW_DEVICE_ANGELIA || device==NEW_DEVICE_ORION || device == NEW_DEVICE_ORION2) ddc=2+i;
        receive_specific_buffer[5]|=receiver[i]->dither<<ddc; // dither enable
        receive_specific_buffer[6]|=receiver[i]->random<<ddc; // random enable
	if (!isTransmitting() && !diversity_enabled) {
	  // Upon TX (with and without PURESIGNAL), and upon diversity reception, deactivate DDCs
          receive_specific_buffer[7]|=(1<<ddc); // DDC enable
	}
        receive_specific_buffer[17+(ddc*6)]=receiver[i]->adc;
        receive_specific_buffer[18+(ddc*6)]=((receiver[i]->sample_rate/1000)>>8)&0xFF;
        receive_specific_buffer[19+(ddc*6)]=(receiver[i]->sample_rate/1000)&0xFF;
        receive_specific_buffer[22+(ddc*6)]=24;
    }

    if(transmitter->puresignal && isTransmitting()) {
//
//    Some things are fixed.
//    the sample rate is always 192.
//    the DDC for PS_RX_FEEDBACK is always DDC0, and the ADC is ADC0
//    the DDC for PS_TX_FEEDBACK is always DDC1, and the ADC is nadc (ADC1 for HERMES, ADC2 beyond)
//    dither and random are always off
//    there are 24 bits per sample
//
      receive_specific_buffer[17]=0;		// ADC0 associated with DDC0
      receive_specific_buffer[18]=0;		// sample rate MSB
      receive_specific_buffer[19]=192;		// sample rate LSB
      receive_specific_buffer[22]=24;		// bits per sample

      receive_specific_buffer[23]=n_adc;	// TX-DAC associated with DDC1
      receive_specific_buffer[24]=0;		// sample rate MSB
      receive_specific_buffer[25]=192;		// sample rate LSB
      receive_specific_buffer[26]=24;		// bits per sample
      receive_specific_buffer[1363]=0x02;       // sync DDC1 to DDC0

      receive_specific_buffer[7]=1; 		// enable  DDC0 but disable all others
    }
    if (diversity_enabled && ! isTransmitting()) {
//
//    Some things are fixed.
//    We always use DDC0 for the signals from ADC0, and DDC1 for the signals from ADC1
//    The sample rate of both DDCs is that of receiver[0].
//    Boths ADCs take the dither/random setting from receiver[0]
//
      receive_specific_buffer[5]|=receiver[0]->dither;		// dither DDC0: take value from RX1
      receive_specific_buffer[5]|=(receiver[0]->dither) << 1;	// dither DDC1: take value from RX1
      receive_specific_buffer[6]|=receiver[0]->random;		// random DDC0: take value from RX1
      receive_specific_buffer[6]|=(receiver[0]->random) << 1;	// random DDC1: take value from RX1

      receive_specific_buffer[17]=0;						// ADC0 associated with DDC0
      receive_specific_buffer[18]=((receiver[0]->sample_rate/1000)>>8)&0xFF;	// sample rate MSB
      receive_specific_buffer[19]=(receiver[0]->sample_rate/1000)&0xFF;		// sample rate LSB
      receive_specific_buffer[22]=24;						// bits per sample

      receive_specific_buffer[23]=1;						// ADC1 associated with DDC1
      receive_specific_buffer[24]=((receiver[0]->sample_rate/1000)>>8)&0xFF;	// sample rate MSB
      receive_specific_buffer[25]=(receiver[0]->sample_rate/1000)&0xFF;;	// sample rate LSB
      receive_specific_buffer[26]=24;						// bits per sample
      receive_specific_buffer[1363]=0x02;       				// sync DDC1 to DDC0

      receive_specific_buffer[7]=1; 						// enable  DDC0 but disable all others
    }

//fprintf(stderr,"new_protocol_receive_specific: enable=%02X\n",receive_specific_buffer[7]);
    if(sendto(data_socket,receive_specific_buffer,sizeof(receive_specific_buffer),0,(struct sockaddr*)&receiver_addr,receiver_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for start\n");
        exit(1);
    }

    rx_specific_sequence++;
    update_action_table();
    pthread_mutex_unlock(&rx_spec_mutex);
}

static void new_protocol_start() {
    new_protocol_transmit_specific();
    new_protocol_receive_specific();
    new_protocol_timer_thread_id = g_thread_new( "new protocol timer", new_protocol_timer_thread, NULL);
    if( ! new_protocol_timer_thread_id )
    {
        fprintf(stderr,"g_thread_new failed on new_protocol_timer_thread\n");
        exit( -1 );
    }
    fprintf(stderr, "new_protocol_timer_thread: id=%p\n",new_protocol_timer_thread_id);

}

void new_protocol_stop() {
    running=0;
    new_protocol_high_priority();
    usleep(100000); // 100 ms
    close (data_socket);
}

void new_protocol_run() {
    new_protocol_high_priority();
}

double calibrate(int v) {
    // Angelia
    double v1;
    v1=(double)v/4095.0*3.3;

    return (v1*v1)/0.095;
}

static gpointer new_protocol_thread(gpointer data) {

    int i;
    int ddc;
    short sourceport;
    unsigned char *buffer;
    int bytesread;

fprintf(stderr,"new_protocol_thread\n");

    micsamples=0;
    iqindex=4;


/*
    data_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(data_socket<0) {
        fprintf(stderr,"metis: create socket failed for data_socket\n");
        exit(-1);
    }

    int optval = 1;
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // bind to the interface
    if(bind(data_socket,(struct sockaddr*)&radio->info.network.interface_address,radio->info.network.interface_length)<0) {
        fprintf(stderr,"metis: bind socket failed for data_socket\n");
        exit(-1);
    }

fprintf(stderr,"new_protocol_thread: date_socket %d bound to interface\n");

    memcpy(&base_addr,&radio->info.network.address,radio->info.network.address_length);
    base_addr_length=radio->info.network.address_length;
    base_addr.sin_port=htons(GENERAL_REGISTERS_FROM_HOST_PORT);

    memcpy(&receiver_addr,&radio->info.network.address,radio->info.network.address_length);
    receiver_addr_length=radio->info.network.address_length;
    receiver_addr.sin_port=htons(RECEIVER_SPECIFIC_REGISTERS_FROM_HOST_PORT);

    memcpy(&transmitter_addr,&radio->info.network.address,radio->info.network.address_length);
    transmitter_addr_length=radio->info.network.address_length;
    transmitter_addr.sin_port=htons(TRANSMITTER_SPECIFIC_REGISTERS_FROM_HOST_PORT);

    memcpy(&high_priority_addr,&radio->info.network.address,radio->info.network.address_length);
    high_priority_addr_length=radio->info.network.address_length;
    high_priority_addr.sin_port=htons(HIGH_PRIORITY_FROM_HOST_PORT);

fprintf(stderr,"new_protocol_thread: high_priority_addr setup for port %d\n",HIGH_PRIORITY_FROM_HOST_PORT);

    memcpy(&audio_addr,&radio->info.network.address,radio->info.network.address_length);
    audio_addr_length=radio->info.network.address_length;
    audio_addr.sin_port=htons(AUDIO_FROM_HOST_PORT);

    memcpy(&iq_addr,&radio->info.network.address,radio->info.network.address_length);
    iq_addr_length=radio->info.network.address_length;
    iq_addr.sin_port=htons(TX_IQ_FROM_HOST_PORT);

   
    for(i=0;i<MAX_DDC;i++) {
        memcpy(&data_addr[i],&radio->info.network.address,radio->info.network.address_length);
        data_addr_length[i]=radio->info.network.address_length;
        data_addr[i].sin_port=htons(RX_IQ_TO_HOST_PORT_0+i);
    }
*/
    audioindex=4; // leave space for sequence
    audiosequence=0L;

    running=1;
    new_protocol_general();
    new_protocol_start();
    new_protocol_high_priority();

    while(running) {

        buffer=malloc(NET_BUFFER_SIZE);
        bytesread=recvfrom(data_socket,buffer,NET_BUFFER_SIZE,0,(struct sockaddr*)&addr,&length);
        if(bytesread<0) {
            fprintf(stderr,"recvfrom socket failed for new_protocol_thread");
            exit(-1);
        }

        short sourceport=ntohs(addr.sin_port);

        switch(sourceport) {
            case RX_IQ_TO_HOST_PORT_0:
            case RX_IQ_TO_HOST_PORT_1:
            case RX_IQ_TO_HOST_PORT_2:
            case RX_IQ_TO_HOST_PORT_3:
            case RX_IQ_TO_HOST_PORT_4:
            case RX_IQ_TO_HOST_PORT_5:
            case RX_IQ_TO_HOST_PORT_6:
            case RX_IQ_TO_HOST_PORT_7:
              ddc=sourceport-RX_IQ_TO_HOST_PORT_0;
//fprintf(stderr,"iq packet from port=%d ddc=%d\n",sourceport,ddc);
              if(ddc>=MAX_DDC)  {
                fprintf(stderr,"unexpected iq data from ddc %d\n",ddc);
              } else {
#ifdef __APPLE__
                sem_wait(iq_sem_ready[ddc]);
#else
                sem_wait(&iq_sem_ready[ddc]);
#endif
                iq_buffer[ddc]=buffer;
#ifdef __APPLE__
                sem_post(iq_sem_buffer[ddc]);
#else
                sem_post(&iq_sem_buffer[ddc]);
#endif
              }
              break;
            case COMMAND_RESPONCE_TO_HOST_PORT:
#ifdef __APPLE__
              sem_wait(command_response_sem_ready);
#else
              sem_wait(&command_response_sem_ready);
#endif
              command_response_buffer=buffer;
#ifdef __APPLE__
              sem_post(command_response_sem_buffer);
#else
              sem_post(&command_response_sem_buffer);
#endif
              //process_command_response();
              break;
            case HIGH_PRIORITY_TO_HOST_PORT:
#ifdef __APPLE__
              sem_wait(high_priority_sem_ready);
#else
              sem_wait(&high_priority_sem_ready);
#endif
              high_priority_buffer=buffer;
#ifdef __APPLE__
              sem_post(high_priority_sem_buffer);
#else
              sem_post(&high_priority_sem_buffer);
#endif
              //process_high_priority();
              break;
            case MIC_LINE_TO_HOST_PORT:
#ifdef __APPLE__
              sem_wait(mic_line_sem_ready);
#else
              sem_wait(&mic_line_sem_ready);
#endif
              mic_line_buffer=buffer;
              mic_bytes_read=bytesread;
#ifdef __APPLE__
              sem_post(mic_line_sem_buffer);
#else
              sem_post(&mic_line_sem_buffer);
#endif
              break;
            default:
fprintf(stderr,"new_protocol_thread: Unknown port %d\n",sourceport);
              free(buffer);
              break;
        }
    }

    return NULL;
}

static gpointer command_response_thread(gpointer data) {
  fprintf(stderr,"command_response_thread\n");
  while(1) {
#ifdef __APPLE__
    sem_post(command_response_sem_ready);
    sem_wait(command_response_sem_buffer);
#else
    sem_post(&command_response_sem_ready);
    sem_wait(&command_response_sem_buffer);
#endif
    process_command_response();
    free(command_response_buffer);
  }
}

static gpointer high_priority_thread(gpointer data) {
fprintf(stderr,"high_priority_thread\n");
  while(1) {
#ifdef __APPLE__
    sem_post(high_priority_sem_ready);
    sem_wait(high_priority_sem_buffer);
#else
    sem_post(&high_priority_sem_ready);
    sem_wait(&high_priority_sem_buffer);
#endif
    process_high_priority();
    free(high_priority_buffer);
  }
}

static gpointer mic_line_thread(gpointer data) {
fprintf(stderr,"mic_line_thread\n");
  while(1) {
#ifdef __APPLE__
    sem_post(mic_line_sem_ready);
    sem_wait(mic_line_sem_buffer);
#else
    sem_post(&mic_line_sem_ready);
    sem_wait(&mic_line_sem_buffer);
#endif
    if(!transmitter->local_microphone) {
      process_mic_data(mic_bytes_read);
    }
    free(mic_line_buffer);
  }
}

static gpointer iq_thread(gpointer data) {
  int ddc=(uintptr_t)data;
  long sequence;
  unsigned char *buffer;
  fprintf(stderr,"iq_thread: ddc=%d\n",ddc);
  while(1) {
#ifdef __APPLE__
    sem_post(iq_sem_ready[ddc]);
    sem_wait(iq_sem_buffer[ddc]);
#else
    sem_post(&iq_sem_ready[ddc]);
    sem_wait(&iq_sem_buffer[ddc]);
#endif
    buffer=iq_buffer[ddc];
    if (buffer == NULL) continue;
//
//  Perform sequence check HERE for all cases
//
    sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);
    if(ddc_sequence[ddc] !=sequence) {
      fprintf(stderr,"DDC %d sequence error: expected %ld got %ld\n",ddc,ddc_sequence[ddc],sequence);
      ddc_sequence[ddc]=sequence;
    }
    ddc_sequence[ddc]++;
//
//  Now comes the action table:
//  for each DDC we have set up which action to be taken
//  (and, possibly, for which receiver)
//
    switch (rxcase[ddc]) {
	case RXACTION_SKIP:
	  break;
	case RXACTION_NORMAL:
          process_iq_data(buffer,receiver[rxid[ddc]]);
	  break;
	case RXACTION_PS:
	  process_ps_iq_data(buffer);
	  break;
	case RXACTION_DIV:
	  process_div_iq_data(buffer);
	  break;
    }
    free(buffer);
  }
}

static void process_iq_data(unsigned char *buffer, RECEIVER *rx) {
  long long timestamp;
  int bitspersample;
  int samplesperframe;
  int b;
  int leftsample;
  int rightsample;
  double leftsampledouble;
  double rightsampledouble;

  timestamp=((long long)(buffer[4]&0xFF)<<56)
           +((long long)(buffer[5]&0xFF)<<48)
           +((long long)(buffer[6]&0xFF)<<40)
           +((long long)(buffer[7]&0xFF)<<32)
           +((long long)(buffer[8]&0xFF)<<24)
           +((long long)(buffer[9]&0xFF)<<16)
           +((long long)(buffer[10]&0xFF)<<8)
           +((long long)(buffer[11]&0xFF)   );
  bitspersample=((buffer[12]&0xFF)<<8)+(buffer[13]&0xFF);
  samplesperframe=((buffer[14]&0xFF)<<8)+(buffer[15]&0xFF);

//fprintf(stderr,"process_iq_data: rx=%d bitspersample=%d samplesperframe=%d\n",rx->id, bitspersample,samplesperframe);
  b=16;
  int i;
  for(i=0;i<samplesperframe;i++) {
    leftsample   = (int)((signed char) buffer[b++])<<16;
    leftsample  |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    leftsample  |= (int)((unsigned char)buffer[b++]&0xFF);
    rightsample  = (int)((signed char)buffer[b++]) << 16;
    rightsample |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    rightsample |= (int)((unsigned char)buffer[b++]&0xFF);

    leftsampledouble=(double)leftsample/8388608.0; // for 24 bits
    rightsampledouble=(double)rightsample/8388608.0; // for 24 bits
    //leftsampledouble=(double)leftsample/16777215.0; // for 24 bits
    //rightsampledouble=(double)rightsample/16777215.0; // for 24 bits

    add_iq_samples(rx, leftsampledouble,rightsampledouble);
  }
}

//
// This is the same as process_ps_iq_data except that add_div_iq_samples is called
// at the end
//
static void process_div_iq_data(unsigned char*buffer) {
  long sequence;
  long long timestamp;
  int bitspersample;
  int samplesperframe;
  int b;
  int leftsample0;
  int rightsample0;
  double leftsampledouble0;
  double rightsampledouble0;
  int leftsample1;
  int rightsample1;
  double leftsampledouble1;
  double rightsampledouble1;
  
  timestamp=((long long)(buffer[ 4]&0xFF)<<56)
           +((long long)(buffer[ 5]&0xFF)<<48)
           +((long long)(buffer[ 6]&0xFF)<<40)
           +((long long)(buffer[ 7]&0xFF)<<32)
           +((long long)(buffer[ 8]&0xFF)<<24)
           +((long long)(buffer[ 9]&0xFF)<<16)
           +((long long)(buffer[10]&0xFF)<< 8)
           +((long long)(buffer[11]&0xFF)    );

  bitspersample=((buffer[12]&0xFF)<<8)+(buffer[13]&0xFF);
  samplesperframe=((buffer[14]&0xFF)<<8)+(buffer[15]&0xFF);

  b=16;
  int i;
  for(i=0;i<samplesperframe;i+=2) {
    leftsample0   = (int)((signed char) buffer[b++])<<16;
    leftsample0  |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    leftsample0  |= (int)((unsigned char)buffer[b++]&0xFF);
    rightsample0  = (int)((signed char)buffer[b++]) << 16;
    rightsample0 |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    rightsample0 |= (int)((unsigned char)buffer[b++]&0xFF);
    
    leftsampledouble0=(double)leftsample0/8388608.0; 
    rightsampledouble0=(double)rightsample0/8388608.0; 
    
    leftsample1   = (int)((signed char) buffer[b++])<<16;
    leftsample1  |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    leftsample1  |= (int)((unsigned char)buffer[b++]&0xFF);
    rightsample1  = (int)((signed char)buffer[b++]) << 16;
    rightsample1 |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    rightsample1 |= (int)((unsigned char)buffer[b++]&0xFF);

    leftsampledouble1=(double)leftsample1/8388608.0; // for 24 bits
    rightsampledouble1=(double)rightsample1/8388608.0; // for 24 bits

    add_div_iq_samples(receiver[0], leftsampledouble0,rightsampledouble0,leftsampledouble1,rightsampledouble1);
    //
    // if both receivers share the sample rate, we can feed data to RX2
    //
    if (receivers > 1 && (receiver[0]->sample_rate == receiver[1]->sample_rate)) {
      add_iq_samples(receiver[1], leftsampledouble1,rightsampledouble1);
    }
  }
}

static void process_ps_iq_data(unsigned char *buffer) {
  long sequence;
  long long timestamp;
  int bitspersample;
  int samplesperframe;
  int b;
  int leftsample0;
  int rightsample0;
  double leftsampledouble0;
  double rightsampledouble0;
  int leftsample1;
  int rightsample1;
  double leftsampledouble1;
  double rightsampledouble1;

  timestamp=((long long)(buffer[ 4]&0xFF)<<56)
           +((long long)(buffer[ 5]&0xFF)<<48)
           +((long long)(buffer[ 6]&0xFF)<<40)
           +((long long)(buffer[ 7]&0xFF)<<32)
           +((long long)(buffer[ 8]&0xFF)<<24)
           +((long long)(buffer[ 9]&0xFF)<<16)
           +((long long)(buffer[10]&0xFF)<< 8)
           +((long long)(buffer[11]&0xFF)    );

  bitspersample=((buffer[12]&0xFF)<<8)+(buffer[13]&0xFF);
  samplesperframe=((buffer[14]&0xFF)<<8)+(buffer[15]&0xFF);

//fprintf(stderr,"process_ps_iq_data: bitspersample=%d samplesperframe=%d\n", bitspersample,samplesperframe);
  b=16;
  int i;
  for(i=0;i<samplesperframe;i+=2) {
    leftsample0   = (int)((signed char) buffer[b++])<<16;
    leftsample0  |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    leftsample0  |= (int)((unsigned char)buffer[b++]&0xFF);
    rightsample0  = (int)((signed char)buffer[b++]) << 16;
    rightsample0 |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    rightsample0 |= (int)((unsigned char)buffer[b++]&0xFF);

    leftsampledouble0=(double)leftsample0/8388608.0;
    rightsampledouble0=(double)rightsample0/8388608.0;

    leftsample1   = (int)((signed char) buffer[b++])<<16;
    leftsample1  |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    leftsample1  |= (int)((unsigned char)buffer[b++]&0xFF);
    rightsample1  = (int)((signed char)buffer[b++]) << 16;
    rightsample1 |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    rightsample1 |= (int)((unsigned char)buffer[b++]&0xFF);

    leftsampledouble1=(double)leftsample1/8388608.0; // for 24 bits
    rightsampledouble1=(double)rightsample1/8388608.0; // for 24 bits

    add_ps_iq_samples(transmitter, leftsampledouble1,rightsampledouble1,leftsampledouble0,rightsampledouble0);

//fprintf(stderr,"%06x,%06x %06x,%06x\n",leftsample0,rightsample0,leftsample1,rightsample1);
  }
}


static void process_command_response() {
    long sequence;
    sequence=((command_response_buffer[0]&0xFF)<<24)+((command_response_buffer[1]&0xFF)<<16)+((command_response_buffer[2]&0xFF)<<8)+(command_response_buffer[3]&0xFF);
    if (sequence != response_sequence) {
	fprintf(stderr,"CommRes SeqErr: expected=%ld seen=%ld\n", response_sequence, sequence);
	response_sequence=sequence;
    }
    response_sequence++;
    response=command_response_buffer[4]&0xFF;
    fprintf(stderr,"CommandResponse with seq=%ld and command=%d\n",sequence,response);
#ifdef __APPLE__
    sem_post(response_sem);
#else
    sem_post(&response_sem);
#endif
}

static void process_high_priority(unsigned char *buffer) {
    long sequence;
    int previous_ptt;
    int previous_dot;
    int previous_dash;

    int id=active_receiver->id;

    sequence=((high_priority_buffer[0]&0xFF)<<24)+((high_priority_buffer[1]&0xFF)<<16)+((high_priority_buffer[2]&0xFF)<<8)+(high_priority_buffer[3]&0xFF);
    if (sequence != highprio_rcvd_sequence) {
	fprintf(stderr,"HighPrio SeqErr Expected=%ld Seen=%ld\n", highprio_rcvd_sequence, sequence);
	highprio_rcvd_sequence=sequence;
    }
    highprio_rcvd_sequence++;

    previous_ptt=local_ptt;
    previous_dot=dot;
    previous_dash=dash;

    local_ptt=high_priority_buffer[4]&0x01;
    dot=(high_priority_buffer[4]>>1)&0x01;
    dash=(high_priority_buffer[4]>>2)&0x01;
    pll_locked=(high_priority_buffer[4]>>4)&0x01;
    adc_overload=high_priority_buffer[5]&0x01;
    exciter_power=((high_priority_buffer[6]&0xFF)<<8)|(high_priority_buffer[7]&0xFF);
    alex_forward_power=((high_priority_buffer[14]&0xFF)<<8)|(high_priority_buffer[15]&0xFF);
    alex_reverse_power=((high_priority_buffer[22]&0xFF)<<8)|(high_priority_buffer[23]&0xFF);
    supply_volts=((high_priority_buffer[49]&0xFF)<<8)|(high_priority_buffer[50]&0xFF);

    if (dash || dot) cw_key_hit=1;

    int tx_vfo=split?VFO_B:VFO_A;
    if(vfo[tx_vfo].mode==modeCWL || vfo[tx_vfo].mode==modeCWU) {
      local_ptt=local_ptt|dot|dash;
    }
    if(previous_ptt!=local_ptt && !CAT_cw_is_active) {
      g_idle_add(ext_mox_update,(gpointer)(long)(local_ptt));
    }
}

static void process_mic_data(int bytes) {
  long sequence;
  int b;
  int i;
  short sample;

  sequence=((mic_line_buffer[0]&0xFF)<<24)+((mic_line_buffer[1]&0xFF)<<16)+((mic_line_buffer[2]&0xFF)<<8)+(mic_line_buffer[3]&0xFF);
  if (sequence != micsamples_sequence) {
    fprintf(stderr,"MicSample SeqErr Expected=%ld Seen=%ld\n", micsamples_sequence, sequence);
    micsamples_sequence=sequence;
  }
  micsamples_sequence++;
  b=4;
  for(i=0;i<MIC_SAMPLES;i++) {
    sample=(short)(mic_line_buffer[b++]<<8);
    sample |= (short) (mic_line_buffer[b++]&0xFF);
#ifdef FREEDV
    if(active_receiver->freedv) {
      add_freedv_mic_sample(transmitter,sample);
    } else {
#endif
      add_mic_sample(transmitter,sample);
#ifdef FREEDV
    }
#endif
  }
}

void new_protocol_process_local_mic(unsigned char *buffer,int le) {
  int b;
  int i;
  short sample;

  b=0;
  for(i=0;i<MIC_SAMPLES;i++) {
    if(le) {
      sample = (short)(buffer[b++]&0xFF);
      sample |= (short) (buffer[b++]<<8);
    } else {
      sample = (short)(buffer[b++]<<8);
      sample |= (short) (buffer[b++]&0xFF);
    }
#ifdef FREEDV
    if(active_receiver->freedv) {
      add_freedv_mic_sample(transmitter,sample);
    } else {
#endif
      add_mic_sample(transmitter,sample);
#ifdef FREEDV
    }
#endif
  }

}


void new_protocol_audio_samples(RECEIVER *rx,short left_audio_sample,short right_audio_sample) {
  int rc;

  // insert the samples
  audiobuffer[audioindex++]=left_audio_sample>>8;
  audiobuffer[audioindex++]=left_audio_sample;
  audiobuffer[audioindex++]=right_audio_sample>>8;
  audiobuffer[audioindex++]=right_audio_sample;
            
  if(audioindex>=sizeof(audiobuffer)) {

    // insert the sequence
    audiobuffer[0]=audiosequence>>24;
    audiobuffer[1]=audiosequence>>16;
    audiobuffer[2]=audiosequence>>8;
    audiobuffer[3]=audiosequence;

    // send the buffer

    rc=sendto(data_socket,audiobuffer,sizeof(audiobuffer),0,(struct sockaddr*)&audio_addr,audio_addr_length);
    if(rc!=sizeof(audiobuffer)) {
      fprintf(stderr,"sendto socket failed for %ld bytes of audio: %d\n",sizeof(audiobuffer),rc);
    }
    audioindex=4;
    audiosequence++;
  }
}

void new_protocol_flush_iq_samples() {
//
// this is called at the end of a TX phase:
// zero out "rest" of TX IQ buffer and send it
//
  while (iqindex < sizeof(iqbuffer)) {
    iqbuffer[iqindex++]=0;
  }

  iqbuffer[0]=tx_iq_sequence>>24;
  iqbuffer[1]=tx_iq_sequence>>16;
  iqbuffer[2]=tx_iq_sequence>>8;
  iqbuffer[3]=tx_iq_sequence;

  // send the buffer
  if(sendto(data_socket,iqbuffer,sizeof(iqbuffer),0,(struct sockaddr*)&iq_addr,iq_addr_length)<0) {
    fprintf(stderr,"sendto socket failed for iq\n");
    exit(1);
  }
  iqindex=4;
  tx_iq_sequence++;
}

void new_protocol_iq_samples(int isample,int qsample) {
  iqbuffer[iqindex++]=isample>>16;
  iqbuffer[iqindex++]=isample>>8;
  iqbuffer[iqindex++]=isample;
  iqbuffer[iqindex++]=qsample>>16;
  iqbuffer[iqindex++]=qsample>>8;
  iqbuffer[iqindex++]=qsample;

  if(iqindex==sizeof(iqbuffer)) {
    iqbuffer[0]=tx_iq_sequence>>24;
    iqbuffer[1]=tx_iq_sequence>>16;
    iqbuffer[2]=tx_iq_sequence>>8;
    iqbuffer[3]=tx_iq_sequence;

    // send the buffer
    if(sendto(data_socket,iqbuffer,sizeof(iqbuffer),0,(struct sockaddr*)&iq_addr,iq_addr_length)<0) {
      fprintf(stderr,"sendto socket failed for iq\n");
      exit(1);
    }
    iqindex=4;
    tx_iq_sequence++;
  }
}

void* new_protocol_timer_thread(void* arg) {
  int specific=0;
fprintf(stderr,"new_protocol_timer_thread\n");
  while(running) {
    usleep(100000); // 100ms
//    if(running) {
//      switch(specific) {
//        case 0:
          new_protocol_transmit_specific();
//          specific=1;
//          break;
//        case 1:
          new_protocol_receive_specific();
//          specific=0;
//          break;
//      }
//    }
  }
  return NULL;
}
