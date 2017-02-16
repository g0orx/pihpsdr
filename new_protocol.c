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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
#ifdef LOCALCW
#include "iambic.h"
#endif
#include "vox.h"

#define min(x,y) (x<y?x:y)

#define PI 3.1415926535897932F

int data_socket;

static int running;

sem_t response_sem;

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

static struct sockaddr_in data_addr[MAX_RECEIVERS];
static int data_addr_length[MAX_RECEIVERS];

static GThread *new_protocol_thread_id;
static GThread *new_protocol_timer_thread_id;

static long rx_sequence = 0;

static long high_priority_sequence = 0;
static long general_sequence = 0;
static long rx_specific_sequence = 0;
static long tx_specific_sequence = 0;

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

static long response_sequence;
static int response;

static sem_t send_high_priority_sem;
static int send_high_priority=0;
static sem_t send_general_sem;
static int send_general=0;

static int samples[MAX_RECEIVERS];
#ifdef INCLUDED
static int outputsamples;
#endif

static int leftaudiosample;
static int rightaudiosample;
static long audiosequence;
#ifdef SHORT_FRAMES
static unsigned char audiobuffer[260]; // was 1444
#else
static unsigned char audiobuffer[1444]; // was 1444
#endif
static int audioindex;

#ifdef FREEDV
static int freedv_samples=0;
static int freedv_resample=6;  // convert from 48000 to 8000
#endif
#ifdef PSK
static int psk_samples=0;
static int psk_resample=6;  // convert from 48000 to 8000
#endif

static struct sockaddr_in addr;
static int length;
static unsigned char buffer[2048];
static int bytesread;

static void new_protocol_high_priority(int run);
//static void* new_protocol_thread(void* arg);
static gpointer new_protocol_thread(gpointer data);
//static void* new_protocol_timer_thread(void* arg);
static gpointer new_protocol_timer_thread(gpointer data);
static void  process_iq_data(RECEIVER *rx,unsigned char *buffer);
static void  process_command_response(unsigned char *buffer);
static void  process_high_priority(unsigned char *buffer);
static void  process_mic_data(unsigned char *buffer,int bytes);
static void full_tx_buffer();

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
    sem_wait(&send_high_priority_sem);
    send_high_priority=1;
    sem_post(&send_high_priority_sem);
}

void schedule_general() {
    sem_wait(&send_general_sem);
    send_general=1;
    sem_post(&send_general_sem);
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

void cw_changed() {
#ifdef LOCALCW
    // update the iambic keyer params
    if (cw_keyer_internal == 0)
        keyer_update();
#endif
}

void new_protocol_init(int pixels) {
    int rc;
    spectrumWIDTH=pixels;

    fprintf(stderr,"new_protocol_init: MIC_SAMPLES=%d\n",MIC_SAMPLES);

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

    rc=sem_init(&response_sem, 0, 0);
    rc=sem_init(&send_high_priority_sem, 0, 1);
    rc=sem_init(&send_general_sem, 0, 1);

    new_protocol_thread_id = g_thread_new( "new protocol", new_protocol_thread, NULL);
    if( ! new_protocol_thread_id )
    {
        fprintf(stderr,"g_thread_new failed on new_protocol_thread\n");
        exit( -1 );
    }
    fprintf(stderr, "new_protocol_thread: id=%p\n",new_protocol_thread_id);


}

static void new_protocol_general() {
    unsigned char buffer[60];
    BAND *band=band_get_current_band();

    memset(buffer, 0, sizeof(buffer));

    buffer[0]=general_sequence>>24;
    buffer[1]=general_sequence>>16;
    buffer[2]=general_sequence>>8;
    buffer[3]=general_sequence;

    // use defaults apart from
    buffer[37]=0x08;  //  phase word (not frequency)
    buffer[38]=0x01;  //  enable hardware timer

    
    
    if(band->disablePA) {
      buffer[58]=0x00;
    } else {
      buffer[58]=0x01;  // enable PA
    }

    if(filter_board==APOLLO) {
      buffer[58]|=0x02; // enable APOLLO tuner
    }

    if(filter_board==ALEX) {
      if(device==NEW_DEVICE_ORION2) {
        buffer[59]=0x03;  // enable Alex 0 and 1
      } else {
        buffer[59]=0x01;  // enable Alex 0
      }
    }

fprintf(stderr,"Alex Enable=%02X\n",buffer[59]);

    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&base_addr,base_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for general\n");
        exit(1);
    }

    general_sequence++;
}

static void new_protocol_high_priority(int run) {
    int i, r;
    unsigned char buffer[1444];
    BAND *band;
    long long rxFrequency;
    long long txFrequency;
    long phase;
    int mode;

    memset(buffer, 0, sizeof(buffer));

    buffer[0]=high_priority_sequence>>24;
    buffer[1]=high_priority_sequence>>16;
    buffer[2]=high_priority_sequence>>8;
    buffer[3]=high_priority_sequence;

    if(split) {
      mode=vfo[1].mode;
    } else {
      mode=vfo[0].mode;
    }
    buffer[4]=run;
    if(mode==modeCWU || mode==modeCWL) {
      if(tune) {
        buffer[4]|=0x02;
      }
#ifdef LOCALCW
      if (cw_keyer_internal == 0) {
        // set the ptt if we're not in breakin mode and mox is on
        if(cw_breakin == 0 && getMox()) buffer[4]|=0x02;
        buffer[5]|=(keyer_out) ? 0x01 : 0;
        //buffer[5]|=(*kdot) ? 0x02 : 0;
        //buffer[5]|=(*kdash) ? 0x04 : 0;
        buffer[5]|=(key_state==SENDDOT) ? 0x02 : 0;
        buffer[5]|=(key_state==SENDDASH) ? 0x04 : 0;
      }
#endif
    } else {
      if(isTransmitting()) {
        buffer[4]|=0x02;
      }
    }

// rx

    for(r=0;r<receivers;r++) {
      //long long rxFrequency=ddsFrequency;
      //rxFrequency=receiver[r]->dds_frequency;
      int v=receiver[r]->id;
      rxFrequency=vfo[v].frequency-vfo[v].lo;
      if(vfo[v].rit_enabled) {
        rxFrequency+=vfo[v].rit;
      }
      if(vfo[v].mode==modeCWU) {
        rxFrequency-=cw_keyer_sidetone_frequency;
      } else if(vfo[v].mode==modeCWL) {
        rxFrequency+=cw_keyer_sidetone_frequency;
      }
      phase=(long)((4294967296.0*(double)rxFrequency)/122880000.0);

      i=r;
      //if(device==NEW_DEVICE_ORION2 && r==1) i=3;
      buffer[9+(i*4)]=phase>>24;
      buffer[10+(i*4)]=phase>>16;
      buffer[11+(i*4)]=phase>>8;
      buffer[12+(i*4)]=phase;
    }

    // tx
    band=band_get_band(vfo[VFO_A].band);
    rxFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo;
    txFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo;
    if(split) {
      band=band_get_band(vfo[VFO_B].band);
      txFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo;
    }

    phase=(long)((4294967296.0*(double)txFrequency)/122880000.0);

    buffer[329]=phase>>24;
    buffer[330]=phase>>16;
    buffer[331]=phase>>8;
    buffer[332]=phase;

    int power=0;
    if(isTransmitting()) {
      if(tune) {
        power=tune_drive_level;
      } else {
        power=drive_level;
      }
    }

    buffer[345]=power&0xFF;

    if(isTransmitting()) {

      if(split) {
        band=band_get_band(vfo[VFO_B].band);
      } else {
        band=band_get_band(vfo[VFO_A].band);
      }
      buffer[1401]=band->OCtx<<1;
      if(tune) {
        if(OCmemory_tune_time!=0) {
          struct timeval te;
          gettimeofday(&te,NULL);
          long long now=te.tv_sec*1000LL+te.tv_usec/1000;
          if(tune_timeout>now) {
            buffer[1401]|=OCtune<<1;
          }
        } else {
          buffer[1401]|=OCtune<<1;
        }
      }
    } else {
      band=band_get_band(vfo[VFO_A].band);
      buffer[1401]=band->OCrx<<1;
    }

    if((protocol==ORIGINAL_PROTOCOL && device==DEVICE_METIS) ||
#ifdef USBOZY
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_OZY) ||
#endif
       (protocol==NEW_PROTOCOL && device==NEW_DEVICE_ATLAS)) {
      for(r=0;r<receivers;r++) {
        buffer[1403]|=receiver[i]->preamp;
      }
    }


    long filters=0x00000000;

    if(isTransmitting()) {
      filters=0x08000000;
    }

    if(rxFrequency<1800000L) {
        filters|=ALEX_BYPASS_HPF;
    } else if(rxFrequency<6500000L) {
        filters|=ALEX_1_5MHZ_HPF;
    } else if(rxFrequency<9500000L) {
        filters|=ALEX_6_5MHZ_HPF;
    } else if(rxFrequency<13000000L) {
        filters|=ALEX_9_5MHZ_HPF;
    } else if(rxFrequency<20000000L) {
        filters|=ALEX_13MHZ_HPF;
    } else {
        filters|=ALEX_20MHZ_HPF;
    }

    if(rxFrequency>30000000L) {
        filters|=ALEX_6M_PREAMP;
    }

    if(txFrequency>32000000) {
        filters|=ALEX_6_BYPASS_LPF;
    } else if(txFrequency>22000000) {
        filters|=ALEX_12_10_LPF;
    } else if(txFrequency>15000000) {
        filters|=ALEX_17_15_LPF;
    } else if(txFrequency>8000000) {
        filters|=ALEX_30_20_LPF;
    } else if(txFrequency>4500000) {
        filters|=ALEX_60_40_LPF;
    } else if(txFrequency>2400000) {
        filters|=ALEX_80_LPF;
    } else {
        filters|=ALEX_160_LPF;
    }

    switch(receiver[0]->alex_antenna) {
        case 0:  // ANT 1
          break;
        case 1:  // ANT 2
          break;
        case 2:  // ANT 3
          break;
        case 3:  // EXT 1
          filters|=ALEX_RX_ANTENNA_EXT2;
          break;
        case 4:  // EXT 2
          filters|=ALEX_RX_ANTENNA_EXT1;
          break;
        case 5:  // XVTR
          filters|=ALEX_RX_ANTENNA_XVTR;
          break;
        default:
          // invalid value - set to 0
          band->alexRxAntenna=0;
          break;
    }

    if(isTransmitting()) {
      switch(transmitter->alex_antenna) {
        case 0:  // ANT 1
          filters|=ALEX_TX_ANTENNA_1;
          break;
        case 1:  // ANT 2
          filters|=ALEX_TX_ANTENNA_2;
          break;
        case 2:  // ANT 3
          filters|=ALEX_TX_ANTENNA_3;
          break;
        default:
          // invalid value - set to 0
          filters|=ALEX_TX_ANTENNA_1;
          band->alexRxAntenna=0;
          break;
         
      }
    } else {
      switch(receiver[0]->alex_antenna) {
        case 0:  // ANT 1
          filters|=ALEX_TX_ANTENNA_1;
          break;
        case 1:  // ANT 2
          filters|=ALEX_TX_ANTENNA_2;
          break;
        case 2:  // ANT 3
          filters|=ALEX_TX_ANTENNA_3;
          break;
        case 3:  // EXT 1
        case 4:  // EXT 2
        case 5:  // XVTR
          switch(transmitter->alex_antenna) {
            case 0:  // ANT 1
              filters|=ALEX_TX_ANTENNA_1;
              break;
            case 1:  // ANT 2
              filters|=ALEX_TX_ANTENNA_2;
              break;
            case 2:  // ANT 3
              filters|=ALEX_TX_ANTENNA_3;
              break;
          }
          break;
      }
    }

    buffer[1432]=(filters>>24)&0xFF;
    buffer[1433]=(filters>>16)&0xFF;
    buffer[1434]=(filters>>8)&0xFF;
    buffer[1435]=filters&0xFF;
//fprintf(stderr,"HPF: 0: %02X %02X for %lld\n",buffer[1434],buffer[1435],rxFrequency);

    filters=0x00000000;
    rxFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo;
    if(rxFrequency<1800000L) {
        filters|=ALEX_BYPASS_HPF;
    } else if(rxFrequency<6500000L) {
        filters|=ALEX_1_5MHZ_HPF;
    } else if(rxFrequency<9500000L) {
        filters|=ALEX_6_5MHZ_HPF;
    } else if(rxFrequency<13000000L) {
        filters|=ALEX_9_5MHZ_HPF;
    } else if(rxFrequency<20000000L) {
        filters|=ALEX_13MHZ_HPF;
    } else {
        filters|=ALEX_20MHZ_HPF;
    }

    if(rxFrequency>30000000L) {
        filters|=ALEX_6M_PREAMP;
    }


    buffer[1428]=(filters>>24)&0xFF;
    buffer[1429]=(filters>>16)&0xFF;
    buffer[1430]=(filters>>8)&0xFF;
    buffer[1431]=filters&0xFF;

//fprintf(stderr,"HPF: 1: %02X %02X for %lld\n",buffer[1430],buffer[1431],rxFrequency);
// rx_frequency

//fprintf(stderr,"new_protocol_high_priority: OC=%02X filters=%04X for frequency=%lld\n", buffer[1401], filters, rxFrequency);

    for(r=0;r<receivers;r++) {
      i=r;
      //if(device==NEW_DEVICE_ORION2 && r==1) i=3;
      buffer[1443-i]=receiver[r]->attenuation;
    }

    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&high_priority_addr,high_priority_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for high priority\n");
        exit(1);
    }

    high_priority_sequence++;
}

static void new_protocol_transmit_specific() {
    unsigned char buffer[60];
    int mode;

    memset(buffer, 0, sizeof(buffer));

    buffer[0]=tx_specific_sequence>>24;
    buffer[1]=tx_specific_sequence>>16;
    buffer[2]=tx_specific_sequence>>8;
    buffer[3]=tx_specific_sequence;

    if(split) {
      mode=vfo[1].mode;
    } else {
      mode=vfo[0].mode;
    }
    buffer[4]=1; // 1 DAC
    buffer[5]=0; //  default no CW
    // may be using local pihpsdr OR hpsdr CW
    if(mode==modeCWU || mode==modeCWL) {
        buffer[5]|=0x02;
    }
    if(cw_keys_reversed) {
        buffer[5]|=0x04;
    }
    if(cw_keyer_mode==KEYER_MODE_A) {
        buffer[5]|=0x08;
    }
    if(cw_keyer_mode==KEYER_MODE_B) {
        buffer[5]|=0x28;
    }
    if(cw_keyer_sidetone_volume!=0) {
        buffer[5]|=0x10;
    }
    if(cw_keyer_spacing) {
        buffer[5]|=0x40;
    }
    if(cw_breakin) {
        buffer[5]|=0x80;
    }

    buffer[6]=cw_keyer_sidetone_volume; // sidetone off
    buffer[7]=cw_keyer_sidetone_frequency>>8;
    buffer[8]=cw_keyer_sidetone_frequency; // sidetone frequency
    buffer[9]=cw_keyer_speed; // cw keyer speed
    buffer[10]=cw_keyer_weight; // cw weight
    buffer[11]=cw_keyer_hang_time>>8;
    buffer[12]=cw_keyer_hang_time; // cw hang delay
    buffer[13]=0; // rf delay
    buffer[50]=0;
    if(mic_linein) {
      buffer[50]|=0x01;
    }
    if(mic_boost) {
      buffer[50]|=0x02;
    }
    if(mic_ptt_enabled==0) {  // set if disabled
      buffer[50]|=0x04;
    }
    if(mic_ptt_tip_bias_ring) {
      buffer[50]|=0x08;
    }
    if(mic_bias_enabled) {
      buffer[50]|=0x10;
    }

    // 0..31
    buffer[51]=linein_gain;

    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&transmitter_addr,transmitter_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for tx specific\n");
        exit(1);
    }

    tx_specific_sequence++;

}

static void new_protocol_receive_specific() {
    unsigned char buffer[1444];
    int i;

    memset(buffer, 0, sizeof(buffer));

    buffer[0]=rx_specific_sequence>>24;
    buffer[1]=rx_specific_sequence>>16;
    buffer[2]=rx_specific_sequence>>8;
    buffer[3]=rx_specific_sequence;

    buffer[4]=2; // 2 ADCs

    for(i=0;i<receivers;i++) {
      int r=i;
      //if(device==NEW_DEVICE_ORION2 && r==1) r=3;
      buffer[5]|=receiver[i]->dither<<r; // dither enable
      buffer[6]|=receiver[i]->random<<r; // random enable
      buffer[7]|=(1<<r); // DDC enbale
      buffer[17+(r*6)]=receiver[i]->adc;
      buffer[18+(r*6)]=((receiver[i]->sample_rate/1000)>>8)&0xFF;
      buffer[19+(r*6)]=(receiver[i]->sample_rate/1000)&0xFF;
      buffer[22+(r*6)]=24;
    }


    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&receiver_addr,receiver_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for start\n");
        exit(1);
    }

    rx_specific_sequence++;
}

static void new_protocol_start() {
    running=1;
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
    new_protocol_high_priority(0);
    usleep(100000); // 100 ms
}

void new_protocol_run() {
    new_protocol_high_priority(1);
}

double calibrate(int v) {
    // Angelia
    double v1;
    v1=(double)v/4095.0*3.3;

    return (v1*v1)/0.095;
}

//void* new_protocol_thread(void* arg) {
static gpointer new_protocol_thread(gpointer data) {

    int i;
    short sourceport;

fprintf(stderr,"new_protocol_thread\n");

    micsamples=0;
    iqindex=4;


    data_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(data_socket<0) {
        fprintf(stderr,"metis: create socket failed for data_socket\n");
        exit(-1);
    }

    int optval = 1;
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // bind to the interface
    if(bind(data_socket,(struct sockaddr*)&radio->info.network.interface_address,radio->info.network.interface_length)<0) {
        fprintf(stderr,"metis: bind socket failed for data_socket\n");
        exit(-1);
    }

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

    memcpy(&audio_addr,&radio->info.network.address,radio->info.network.address_length);
    audio_addr_length=radio->info.network.address_length;
    audio_addr.sin_port=htons(AUDIO_FROM_HOST_PORT);

    memcpy(&iq_addr,&radio->info.network.address,radio->info.network.address_length);
    iq_addr_length=radio->info.network.address_length;
    iq_addr.sin_port=htons(TX_IQ_FROM_HOST_PORT);

   
    for(i=0;i<MAX_RECEIVERS;i++) {
        memcpy(&data_addr[i],&radio->info.network.address,radio->info.network.address_length);
        data_addr_length[i]=radio->info.network.address_length;
        data_addr[i].sin_port=htons(RX_IQ_TO_HOST_PORT_0+i);
        samples[i]=0;
    }

    audioindex=4; // leave space for sequence
    audiosequence=0L;

    new_protocol_general();
    new_protocol_start();
    new_protocol_high_priority(1);

    while(running) {
        bytesread=recvfrom(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&addr,&length);
        if(bytesread<0) {
            fprintf(stderr,"recvfrom socket failed for new_protocol_thread");
            exit(1);
        }

        short sourceport=ntohs(addr.sin_port);

//fprintf(stderr,"received packet length %d from port %d\n",bytesread,sourceport);

        switch(sourceport) {
            case RX_IQ_TO_HOST_PORT_0:
            case RX_IQ_TO_HOST_PORT_1:
            case RX_IQ_TO_HOST_PORT_2:
            case RX_IQ_TO_HOST_PORT_3:
            case RX_IQ_TO_HOST_PORT_4:
            case RX_IQ_TO_HOST_PORT_5:
            case RX_IQ_TO_HOST_PORT_6:
            case RX_IQ_TO_HOST_PORT_7:
              i=sourceport-RX_IQ_TO_HOST_PORT_0;
              //if(device==NEW_DEVICE_ORION2 && i==3) {
              //  i=1;
              //}
              process_iq_data(receiver[i],buffer);
              break;
            case COMMAND_RESPONCE_TO_HOST_PORT:
              process_command_response(buffer);
              break;
            case HIGH_PRIORITY_TO_HOST_PORT:
              process_high_priority(buffer);
              break;
            case MIC_LINE_TO_HOST_PORT:
              if(!transmitter->local_microphone) {
                process_mic_data(buffer,bytesread);
              }
              break;
            default:
              break;
        }

        if(running) {
           sem_wait(&send_high_priority_sem);
           if(send_high_priority==1) {
               new_protocol_high_priority(1);
               send_high_priority=0;
           }
           sem_post(&send_high_priority_sem);

           sem_wait(&send_general_sem);
           if(send_general==1) {
               new_protocol_general();
               send_general=0;
           }
           sem_post(&send_general_sem);
       }
        
    }

    close(data_socket);
}

static void process_iq_data(RECEIVER *rx,unsigned char *buffer) {
  long sequence;
  long long timestamp;
  int bitspersample;
  int samplesperframe;
  int b;
  int leftsample;
  int rightsample;
  double leftsampledouble;
  double rightsampledouble;

  sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);
  timestamp=((long long)(buffer[4]&0xFF)<<56)+((long long)(buffer[5]&0xFF)<<48)+((long long)(buffer[6]&0xFF)<<40)+((long long)(buffer[7]&0xFF)<<32);
  ((long long)(buffer[8]&0xFF)<<24)+((long long)(buffer[9]&0xFF)<<16)+((long long)(buffer[10]&0xFF)<<8)+(long long)(buffer[11]&0xFF);
  bitspersample=((buffer[12]&0xFF)<<8)+(buffer[13]&0xFF);
  samplesperframe=((buffer[14]&0xFF)<<8)+(buffer[15]&0xFF);

//fprintf(stderr,"process_iq_data: rx=%d seq=%ld bitspersample=%d samplesperframe=%d\n",rx->id, sequence,bitspersample,samplesperframe);
  b=16;
  int i;
  for(i=0;i<samplesperframe;i++) {
    leftsample   = (int)((signed char) buffer[b++])<<16;
    leftsample  |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    leftsample  |= (int)((unsigned char)buffer[b++]&0xFF);
    rightsample  = (int)((signed char)buffer[b++]) << 16;
    rightsample |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
    rightsample |= (int)((unsigned char)buffer[b++]&0xFF);

    leftsampledouble=(double)leftsample/8388607.0; // for 24 bits
    rightsampledouble=(double)rightsample/8388607.0; // for 24 bits

    add_iq_samples(rx, leftsampledouble,rightsampledouble);
  }
}

static void process_command_response(unsigned char *buffer) {
    response_sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);
    response=buffer[4]&0xFF;
    fprintf(stderr,"response_sequence=%ld response=%d\n",response_sequence,response);
    sem_post(&response_sem);
}

static void process_high_priority(unsigned char *buffer) {
    long sequence;
    int previous_ptt;
    int previous_dot;
    int previous_dash;

    sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);

    previous_ptt=ptt;
    previous_dot=dot;
    previous_dash=dash;

    ptt=buffer[4]&0x01;
    dot=(buffer[4]>>1)&0x01;
    dash=(buffer[4]>>2)&0x01;

if(ptt!=previous_ptt) {
  fprintf(stderr,"ptt=%d\n",ptt);
}
if(dot!=previous_dot) {
  fprintf(stderr,"dot=%d\n",dot);
}
if(dash!=previous_dash) {
  fprintf(stderr,"dash=%d\n",dash);
}
    pll_locked=(buffer[4]>>3)&0x01;


    adc_overload=buffer[5]&0x01;
    exciter_power=((buffer[6]&0xFF)<<8)|(buffer[7]&0xFF);
    alex_forward_power=((buffer[14]&0xFF)<<8)|(buffer[15]&0xFF);
    alex_reverse_power=((buffer[22]&0xFF)<<8)|(buffer[23]&0xFF);
    supply_volts=((buffer[49]&0xFF)<<8)|(buffer[50]&0xFF);

    if(previous_ptt!=ptt) {
        g_idle_add(ptt_update,(gpointer)ptt);
    }

}

static void process_mic_data(unsigned char *buffer,int bytes) {
  long sequence;
  int b;
  short sample;

  sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);
  b=4;
  int i;
  for(i=0;i<MIC_SAMPLES;i++) {
    sample=(short)((buffer[b++]<<8) | (buffer[b++]&0xFF));
    add_mic_sample(transmitter,sample);
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
      fprintf(stderr,"sendto socket failed for %d bytes of audio: %d\n",sizeof(audiobuffer),rc);
    }
    audioindex=4;
    audiosequence++;
  }
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

void new_protocol_process_local_mic(unsigned char *buffer,int le) {
  int b;
  short micsample;
  double micsampledouble;
  double gain=pow(10.0, mic_gain / 20.0);

  b=0;
  int i,j,s;
  for(i=0;i<MIC_SAMPLES;i++) {
    if(le) {
      micsample = (short)((buffer[b++]&0xFF) | (buffer[b++]<<8));
    } else {
      micsample = (short)((buffer[b++]<<8) | (buffer[b++]&0xFF));
    }
    add_mic_sample(transmitter,micsample);
  }

}

void* new_protocol_timer_thread(void* arg) {
  int count=0;
fprintf(stderr,"new_protocol_timer_thread\n");
  while(running) {
    usleep(100000); // 100ms
    if(running) {
      if(count==0) {
        new_protocol_transmit_specific();
        count=1;
      } else {
        new_protocol_receive_specific();
        count=0;
      }
    }
  }
}
