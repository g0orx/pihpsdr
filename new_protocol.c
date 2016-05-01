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
#include <pthread.h>
#include <semaphore.h>
#include <math.h>

#include "alex.h"
#include "band.h"
#include "new_protocol.h"
#include "channel.h"
#include "discovered.h"
#include "wdsp.h"
#include "mode.h"
#include "filter.h"
#include "radio.h"
#include "vfo.h"
#include "toolbar.h"
#include "wdsp_init.h"

#define PI 3.1415926535897932F

int data_socket;

static int receiver;
static int running;

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

static struct sockaddr_in data_addr;
static int data_addr_length;

static pthread_t new_protocol_thread_id;
static pthread_t new_protocol_timer_thread_id;

sem_t response_sem;

static long rx_sequence = 0;

static long high_priority_sequence = 0;
static long general_sequence = 0;
static long rx_specific_sequence = 0;
static long tx_specific_sequence = 0;

static int buffer_size=BUFFER_SIZE;
static int fft_size=4096;
static int dspRate=48000;
static int outputRate=48000;

static int micSampleRate=48000;
static int micDspRate=48000;
static int micOutputRate=192000;

static int spectrumWIDTH=800;
static int SPECTRUM_UPDATES_PER_SECOND=10;

static float phase = 0.0F;

long response_sequence;
int response;

sem_t send_high_priority_sem;
int send_high_priority=0;
sem_t send_general_sem;
int send_general=0;

static void* new_protocol_thread(void* arg);
static void* new_protocol_timer_thread(void* arg);

void schedule_high_priority(int source) {
fprintf(stderr,"new_protocol: schedule_high_priority: source=%d\n",source);
    sem_wait(&send_high_priority_sem);
    send_high_priority=1;
    sem_post(&send_high_priority_sem);
}

void schedule_general() {
fprintf(stderr,"new_protocol: schedule_general\n");
    sem_wait(&send_general_sem);
    send_general=1;
    sem_post(&send_general_sem);
}

void filter_board_changed() {
    schedule_general();
}

void pa_changed() {
    schedule_general();
}

void tuner_changed() {
    schedule_general();
}

void cw_changed() {
}

void new_protocol_init(int rx,int pixels) {
    int rc;
    receiver=rx;
    spectrumWIDTH=pixels;

    fprintf(stderr,"new_protocol_init: %d\n",rx);

    rc=sem_init(&response_sem, 0, 0);
    rc=sem_init(&send_high_priority_sem, 0, 1);
    rc=sem_init(&send_general_sem, 0, 1);

    rc=pthread_create(&new_protocol_thread_id,NULL,new_protocol_thread,NULL);
    if(rc != 0) {
        fprintf(stderr,"pthread_create failed on new_protocol_thread for receiver %d: rc=%d\n", receiver, rc);
        exit(-1);
    }

}

static void new_protocol_general() {
    unsigned char buffer[60];

fprintf(stderr,"new_protocol_general: receiver=%d\n", receiver);

    memset(buffer, 0, sizeof(buffer));

    buffer[0]=general_sequence>>24;
    buffer[1]=general_sequence>>16;
    buffer[2]=general_sequence>>8;
    buffer[3]=general_sequence;

    // use defaults apart from
    buffer[37]=0x08;  //  phase word (not frequency)
    buffer[58]=pa;  // enable PA 0x01
    if((filter_board==APOLLO) && tune && apollo_tuner) {
        buffer[58]|=0x02;
    }

    if(filter_board==ALEX) {
        buffer[59]=0x01;  // enable Alex 0
    }

    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&base_addr,base_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for general\n");
        exit(1);
    }

    general_sequence++;
}

static void new_protocol_high_priority(int run,int tx,int drive) {
    unsigned char buffer[1444];
    BAND *band=band_get_current_band();

//fprintf(stderr,"new_protocol_high_priority: run=%d tx=%d drive=%d tx_ant=0x%08x rx_ant=0x%08x\n", run, tx, drive, alex_tx_antenna, alex_rx_antenna);
    memset(buffer, 0, sizeof(buffer));

    buffer[0]=high_priority_sequence>>24;
    buffer[1]=high_priority_sequence>>16;
    buffer[2]=high_priority_sequence>>8;
    buffer[3]=high_priority_sequence;

    buffer[4]=run|(tx<<1);

    long phase=(long)((4294967296.0*(double)ddsFrequency)/122880000.0);

// rx
    buffer[9]=phase>>24;
    buffer[10]=phase>>16;
    buffer[11]=phase>>8;
    buffer[12]=phase;

// tx (no split yet)
    buffer[329]=phase>>24;
    buffer[330]=phase>>16;
    buffer[331]=phase>>8;
    buffer[332]=phase;


    buffer[345]=drive;


    if(isTransmitting()) {
      buffer[1401]=band->OCtx;
      if(tune) {
        if(OCmemory_tune_time!=0) {
          struct timeval te;
          gettimeofday(&te,NULL);
          long long now=te.tv_sec*1000LL+te.tv_usec/1000;
          if(tune_timeout>now) {
            buffer[1401]|=OCtune;
          }
        } else {
          buffer[1401]|=OCtune;
        }
      }
    } else {
      buffer[1401]=band->OCrx;
    }
// alex HPF filters
/*
if              (frequency <  1800000) HPF <= 6'b100000;        // bypass
else if (frequency <  6500000) HPF <= 6'b010000;        // 1.5MHz HPF   
else if (frequency <  9500000) HPF <= 6'b001000;        // 6.5MHz HPF
else if (frequency < 13000000) HPF <= 6'b000100;        // 9.5MHz HPF
else if (frequency < 20000000) HPF <= 6'b000001;        // 13MHz HPF
else                                               HPF <= 6'b000010;    // 20MHz HPF
*/



    long filters=0x00000000;
// set HPF
    if(ddsFrequency<1800000L) {
        filters|=ALEX_BYPASS_HPF;
    } else if(ddsFrequency<6500000L) {
        filters|=ALEX_1_5MHZ_HPF;
    } else if(ddsFrequency<9500000L) {
        filters|=ALEX_6_5MHZ_HPF;
    } else if(ddsFrequency<13000000L) {
        filters|=ALEX_9_5MHZ_HPF;
    } else if(ddsFrequency<20000000L) {
        filters|=ALEX_13MHZ_HPF;
    } else {
        filters|=ALEX_20MHZ_HPF;
    }
// alex LPF filters
/*
if (frequency > 32000000)   LPF <= 7'b0010000;             // > 10m so use 6m LPF^M
else if (frequency > 22000000) LPF <= 7'b0100000;       // > 15m so use 12/10m LPF^M
else if (frequency > 15000000) LPF <= 7'b1000000;       // > 20m so use 17/15m LPF^M
else if (frequency > 8000000)  LPF <= 7'b0000001;       // > 40m so use 30/20m LPF  ^M
else if (frequency > 4500000)  LPF <= 7'b0000010;       // > 80m so use 60/40m LPF^M
else if (frequency > 2400000)  LPF <= 7'b0000100;       // > 160m so use 80m LPF  ^M
else LPF <= 7'b0001000;             // < 2.4MHz so use 160m LPF^M
*/

    if(ddsFrequency>32000000) {
        filters|=ALEX_6_BYPASS_LPF;
    } else if(ddsFrequency>22000000) {
        filters|=ALEX_12_10_LPF;
    } else if(ddsFrequency>15000000) {
        filters|=ALEX_17_15_LPF;
    } else if(ddsFrequency>8000000) {
        filters|=ALEX_30_20_LPF;
    } else if(ddsFrequency>4500000) {
        filters|=ALEX_60_40_LPF;
    } else if(ddsFrequency>2400000) {
        filters|=ALEX_80_LPF;
    } else {
        filters|=ALEX_160_LPF;
    }


    switch(band->alexRxAntenna) {
        case 0:  // ANT 1
          break;
        case 1:  // ANT 2
          break;
        case 2:  // ANT 3
          break;
        case 3:  // EXT 1
          //filters|=ALEX_RX_ANTENNA_EXT1;
          filters|=ALEX_RX_ANTENNA_EXT2;
          break;
        case 4:  // EXT 2
          //filters|=ALEX_RX_ANTENNA_EXT2;
          filters|=ALEX_RX_ANTENNA_EXT1;
          break;
        case 5:  // XVTR
          filters|=ALEX_RX_ANTENNA_XVTR;
          break;
    }

    if(isTransmitting()) {
      switch(band->alexTxAntenna) {
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
    } else {
      switch(band->alexRxAntenna) {
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
          switch(band->alexTxAntenna) {
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

    //filters|=alex_attenuation;

    if(tx) {
        filters|=0x08000000;
    }

/*
    buffer[1420]=filters>>24;
    buffer[1421]=filters>>16;
    buffer[1422]=filters>>8;
    buffer[1423]=filters;
*/

    buffer[1432]=filters>>24;
    buffer[1433]=filters>>16;
    buffer[1434]=filters>>8;
    buffer[1435]=filters;

    //buffer[1442]=attenuation;
    buffer[1443]=attenuation;

//fprintf(stderr,"[4]=%02X\n", buffer[4]);
//fprintf(stderr,"filters=%04X\n", filters);

    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&high_priority_addr,high_priority_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for high priority\n");
        exit(1);
    }

    high_priority_sequence++;
}

static void new_protocol_transmit_specific() {
    unsigned char buffer[60];

    memset(buffer, 0, sizeof(buffer));

    buffer[0]=tx_specific_sequence>>24;
    buffer[1]=tx_specific_sequence>>16;
    buffer[2]=tx_specific_sequence>>8;
    buffer[3]=tx_specific_sequence;

    buffer[4]=1; // 1 DAC
    buffer[5]=0; // no CW
    if(cw_keyer_internal) {
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
    if(cw_keyer_spacing) {
        buffer[5]|=0x40;
    }
    if(cw_breakin) {
        buffer[5]|=0x80;
    }

    buffer[6]=cw_keyer_sidetone_volume; // sidetone off
    buffer[7]=cw_keyer_sidetone_frequency>>8; buffer[8]=cw_keyer_sidetone_frequency; // sidetone frequency
    buffer[9]=cw_keyer_speed; // cw keyer speed
    buffer[10]=cw_keyer_weight; // cw weight
    buffer[11]=cw_keyer_hang_time>>8; buffer[12]=cw_keyer_hang_time; // cw hang delay
    buffer[13]=0; // rf delay
    buffer[50]=0;
    if(mic_linein) {
      buffer[50]|=0x01;
    }
    if(mic_boost) {
      buffer[50]|=0x02;
    }
    if(mic_ptt_enabled==0) {
      buffer[50]|=0x04;
    }
    if(mic_bias_enabled) {
      buffer[50]|=0x10;
    }
    if(mic_ptt_tip_bias_ring) {
      buffer[50]|=0x08;
    }
    buffer[51]=0x7F; // Line in gain

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
    buffer[5]=0; // dither off
    if(lt2208Dither) {
        buffer[5]=0x01<<receiver;
    }
    buffer[6]=0; // random off
    if(lt2208Random) {
        buffer[6]=0x01<<receiver;
    }
    buffer[7]=0x00;
    for(i=0;i<receivers;i++) {
      buffer[7]|=(1<<i);
    }
       
    for(i=0;i<receivers;i++) {
      buffer[17+(i*6)]=adc[i];
    }

    buffer[18]=((sample_rate/1000)>>8)&0xFF;
    buffer[19]=(sample_rate/1000)&0xFF;
    buffer[22]=24;

    buffer[24]=((sample_rate/1000)>>8)&0xFF;
    buffer[25]=(sample_rate/1000)&0xFF;
    buffer[28]=24;

    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&receiver_addr,receiver_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for start: receiver=%d\n",receiver);
        exit(1);
    }

    rx_specific_sequence++;
}

static void new_protocol_start() {
    running=1;
    new_protocol_transmit_specific();
    new_protocol_receive_specific();
    int rc=pthread_create(&new_protocol_timer_thread_id,NULL,new_protocol_timer_thread,NULL);
    if(rc != 0) {
        fprintf(stderr,"pthread_create failed on new_protocol_timer_thread: %d\n", rc);
        exit(-1);
    }
}

void new_protocol_stop() {
    new_protocol_high_priority(0,0,0);
    running=0;
    sleep(1);
}

/*
float sineWave(float* buf, int samples, float phase, float freq) {
    float phase_step = 2 * PI * freq / 192000.0F;
    int i;
    for (i = 0; i < samples; i++) {
        buf[i] = (float) sin(phase);
        phase += phase_step;
    }
    return phase;
}
*/
float sineWave(double* buf, int samples, float phase, float freq) {
    float phase_step = 2 * PI * freq / 192000.0F;
    int i;
    for (i = 0; i < samples; i++) {
        buf[i*2] = (double) sin(phase);
        phase += phase_step;
    }
    return phase;
}

double calibrate(int v) {
    // Angelia
    double v1;
    v1=(double)v/4095.0*3.3;

    return (v1*v1)/0.095;
}

void* new_protocol_thread(void* arg) {

    DISCOVERED* d=&discovered[selected_device];

    struct sockaddr_in addr;
    int length;
    unsigned char buffer[2048];
    int bytesread;
    short sourceport;

    long sequence;
    long long timestamp;
    int bitspersample;
    int samplesperframe;

    int b;
    int leftsample;
    int rightsample;
    float leftsamplefloat;
    float rightsamplefloat;

    int previous_ptt;
    int previous_dot;
    int previous_dash;

    int samples;
    //float leftinputbuffer[BUFFER_SIZE];
    //float rightinputbuffer[BUFFER_SIZE];
    double iqinputbuffer[BUFFER_SIZE*2];

    int outputsamples;
    //float leftoutputbuffer[BUFFER_SIZE];
    //float rightoutputbuffer[BUFFER_SIZE];
    double audiooutputbuffer[BUFFER_SIZE*2];

    short leftaudiosample;
    short rightaudiosample;
    long audiosequence;
    unsigned char audiobuffer[1444];
    int audioindex;

    int micsample;
    float micsamplefloat;

    int micsamples;
/*
    float micleftinputbuffer[BUFFER_SIZE];  // 48000
    float micrightinputbuffer[BUFFER_SIZE];
*/
    double micinputbuffer[BUFFER_SIZE*2];

    int micoutputsamples;
/*
    float micleftoutputbuffer[BUFFER_SIZE*4]; // 192000
    float micrightoutputbuffer[BUFFER_SIZE*4];
*/
    double micoutputbuffer[BUFFER_SIZE*4*2];

    double gain;

    int isample;
    int qsample;
    long tx_iq_sequence;
    unsigned char iqbuffer[1444];
    int iqindex;

    int i, j;
fprintf(stderr,"new_protocol_thread: receiver=%d\n", receiver);

    micsamples=0;
    iqindex=4;

    switch(sample_rate) {
        case 48000:
            outputsamples=BUFFER_SIZE;
            break;
        case 96000:
            outputsamples=BUFFER_SIZE/2;
            break;
        case 192000:
            outputsamples=BUFFER_SIZE/4;
            break;
        case 384000:
            outputsamples=BUFFER_SIZE/8;
            break;
        case 768000:
            outputsamples=BUFFER_SIZE/16;
            break;
        case 1536000:
            outputsamples=BUFFER_SIZE/32;
            break;
    }

    micoutputsamples=BUFFER_SIZE*4;  // 48000 in, 192000 out

fprintf(stderr,"outputsamples=%d\n", outputsamples);
    data_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(data_socket<0) {
        fprintf(stderr,"metis: create socket failed for data_socket: receiver=%d\n",receiver);
        exit(-1);
    }

    int optval = 1;
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // bind to the interface
    if(bind(data_socket,(struct sockaddr*)&d->interface_address,d->interface_length)<0) {
        fprintf(stderr,"metis: bind socket failed for data_socket: receiver=%d\n",receiver);
        exit(-1);
    }

    memcpy(&base_addr,&d->address,d->address_length);
    base_addr_length=d->address_length;
    base_addr.sin_port=htons(GENERAL_REGISTERS_FROM_HOST_PORT);

    memcpy(&receiver_addr,&d->address,d->address_length);
    receiver_addr_length=d->address_length;
    receiver_addr.sin_port=htons(RECEIVER_SPECIFIC_REGISTERS_FROM_HOST_PORT);

    memcpy(&transmitter_addr,&d->address,d->address_length);
    transmitter_addr_length=d->address_length;
    transmitter_addr.sin_port=htons(TRANSMITTER_SPECIFIC_REGISTERS_FROM_HOST_PORT);

    memcpy(&high_priority_addr,&d->address,d->address_length);
    high_priority_addr_length=d->address_length;
    high_priority_addr.sin_port=htons(HIGH_PRIORITY_FROM_HOST_PORT);

    memcpy(&audio_addr,&d->address,d->address_length);
    audio_addr_length=d->address_length;
    audio_addr.sin_port=htons(AUDIO_FROM_HOST_PORT);

    memcpy(&iq_addr,&d->address,d->address_length);
    iq_addr_length=d->address_length;
    iq_addr.sin_port=htons(TX_IQ_FROM_HOST_PORT);

    memcpy(&data_addr,&d->address,d->address_length);
    data_addr_length=d->address_length;
    data_addr.sin_port=htons(RX_IQ_TO_HOST_PORT+receiver);

    samples=0;
    audioindex=4; // leave space for sequence
    audiosequence=0L;

    new_protocol_general();
    new_protocol_start();
    new_protocol_high_priority(1,0,drive);

    while(running) {
        bytesread=recvfrom(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&addr,&length);
        if(bytesread<0) {
            fprintf(stderr,"recvfrom socket failed for new_protocol_thread: receiver=%d", receiver);
            exit(1);
        }

        short sourceport=ntohs(addr.sin_port);

//fprintf(stderr,"received packet length %d from port %d\n",bytesread,sourceport);

        if(sourceport==RX_IQ_TO_HOST_PORT) {

          sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);
          timestamp=((long long)(buffer[4]&0xFF)<<56)+((long long)(buffer[5]&0xFF)<<48)+((long long)(buffer[6]&0xFF)<<40)+((long long)(buffer[7]&0xFF)<<32);
                    ((long long)(buffer[8]&0xFF)<<24)+((long long)(buffer[9]&0xFF)<<16)+((long long)(buffer[10]&0xFF)<<8)+(long long)(buffer[11]&0xFF);
          bitspersample=((buffer[12]&0xFF)<<8)+(buffer[13]&0xFF);
          samplesperframe=((buffer[14]&0xFF)<<8)+(buffer[15]&0xFF);

//fprintf(stderr,"samples per frame %d\n",samplesperframe);

          if(!isTransmitting()) {
              b=16;
              for(i=0;i<samplesperframe;i++) {
                  leftsample   = (int)((signed char) buffer[b++]) << 16;
                  leftsample  += (int)((unsigned char)buffer[b++]) << 8;
                  leftsample  += (int)((unsigned char)buffer[b++]);
                  rightsample  = (int)((signed char) buffer[b++]) << 16;
                  rightsample += (int)((unsigned char)buffer[b++]) << 8;
                  rightsample += (int)((unsigned char)buffer[b++]);

                  leftsamplefloat=(float)leftsample/8388607.0; // for 24 bits
                  rightsamplefloat=(float)rightsample/8388607.0; // for 24 bits
            
                  //leftinputbuffer[samples]=leftsamplefloat;
                  //rightinputbuffer[samples]=rightsamplefloat;
                  iqinputbuffer[samples*2]=(double)leftsamplefloat;
                  iqinputbuffer[(samples*2)+1]=(double)rightsamplefloat;

                  samples++;
                  if(samples==BUFFER_SIZE) {
                      int error;
                      fexchange0(CHANNEL_RX0+receiver, iqinputbuffer, audiooutputbuffer, &error);
                      if(error!=0) {
                          fprintf(stderr,"fexchange0 returned error: %d for receiver %d\n", error,receiver);
                      }

                      Spectrum0(1, CHANNEL_RX0+receiver, 0, 0, iqinputbuffer);

                      for(j=0;j<outputsamples;j++) {
                        leftaudiosample=(short)(audiooutputbuffer[j*2]*32767.0*volume);
                        rightaudiosample=(short)(audiooutputbuffer[(j*2)+1]*32767.0*volume);

                        audiobuffer[audioindex++]=leftaudiosample>>8;
                        audiobuffer[audioindex++]=leftaudiosample;
                        audiobuffer[audioindex++]=rightaudiosample>>8;
                        audiobuffer[audioindex++]=rightaudiosample;

                        if(audioindex>=sizeof(audiobuffer)) {
                            // insert the sequence
                            audiobuffer[0]=audiosequence>>24;
                            audiobuffer[1]=audiosequence>>16;
                            audiobuffer[2]=audiosequence>>8;
                            audiobuffer[3]=audiosequence;
                            // send the buffer
                            if(sendto(data_socket,audiobuffer,sizeof(audiobuffer),0,(struct sockaddr*)&audio_addr,audio_addr_length)<0) {
                                fprintf(stderr,"sendto socket failed for audio\n");
                                exit(1);
                            }
                            audioindex=4;
                            audiosequence++;
                        }
                      }
                      samples=0;
                  }
              }
          }

       } else if(sourceport==COMMAND_RESPONCE_TO_HOST_PORT) {
          // command/response
          response_sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);
          response=buffer[4]&0xFF;
          fprintf(stderr,"response_sequence=%ld response=%d\n",response_sequence,response);
          sem_post(&response_sem);

       } else if(sourceport==HIGH_PRIORITY_TO_HOST_PORT) {
          // high priority
          sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);

          previous_ptt=ptt;
          previous_dot=dot;
          previous_dash=dash;

          ptt=buffer[4]&0x01;
          dot=(buffer[4]>>1)&0x01;
          dash=(buffer[4]>>2)&0x01;
          pll_locked=(buffer[4]>>3)&0x01;
          adc_overload=buffer[5]&0x01;
          exciter_power=((buffer[6]&0xFF)<<8)|(buffer[7]&0xFF);
          alex_forward_power=((buffer[14]&0xFF)<<8)|(buffer[15]&0xFF);
          alex_reverse_power=((buffer[22]&0xFF)<<8)|(buffer[23]&0xFF);
          supply_volts=((buffer[49]&0xFF)<<8)|(buffer[50]&0xFF);

          if(previous_ptt!=ptt) {
              //send_high_priority=1;
              previous_ptt=ptt;
              g_idle_add(ptt_update,(gpointer)ptt);
          }

       } else if(sourceport==MIC_LINE_TO_HOST_PORT) {
          // Mic/Line data
          sequence=((buffer[0]&0xFF)<<24)+((buffer[1]&0xFF)<<16)+((buffer[2]&0xFF)<<8)+(buffer[3]&0xFF);
          if(isTransmitting()) {
              b=4;

              for(i=0;i<720;i++) {
                  micsample  = (int)((signed char) buffer[b++]) << 8;
                  micsample  |= (int)((unsigned char)buffer[b++] & 0xFF);
                  micsamplefloat = (float)micsample/32767.0F; // 16 bit sample

                  micinputbuffer[micsamples*2]=(double)(micsamplefloat*mic_gain);
                  micinputbuffer[(micsamples*2)+1]=(double)(micsamplefloat*mic_gain);

                  micsamples++;

                  if(micsamples==BUFFER_SIZE) {
                      int error;

                      if(tune==1) {
                          float tunefrequency = (float)((filterHigh - filterLow) / 2);
                          phase=sineWave(micinputbuffer, BUFFER_SIZE, phase, tunefrequency);
                      }

                      fexchange0(CHANNEL_TX, micinputbuffer, micoutputbuffer, &error);
                      if(error!=0) {
                          fprintf(stderr,"fexchange0 returned error: %d for transmitter\n", error);
                      }

                      if(d->device!=DEVICE_METIS || atlas_penelope) {
                          if(tune) {
                              gain=8388607.0*255.0/(double)tune_drive;
                          } else {
                              gain=8388607.0*255.0/(double)drive;
                          }
                      } else {
                          gain=65535.0;
                      }


                      for(j=0;j<micoutputsamples;j++) {
                        isample=(int)(micoutputbuffer[j*2]*gain*2); // 24 bit
                        qsample=(int)(micoutputbuffer[(j*2)+1]*gain*2); // 24 bit

                        iqbuffer[iqindex++]=isample>>16;
                        iqbuffer[iqindex++]=isample>>8;
                        iqbuffer[iqindex++]=isample;
                        iqbuffer[iqindex++]=qsample>>16;
                        iqbuffer[iqindex++]=qsample>>8;
                        iqbuffer[iqindex++]=qsample;

                        if(iqindex>=sizeof(iqbuffer)) {
                            // insert the sequence
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
                      //Spectrum(CHANNEL_TX, 0, 0, micrightoutputbuffer, micleftoutputbuffer);
                      Spectrum0(1, CHANNEL_TX, 0, 0, micoutputbuffer);
                      micsamples=0;
                  }
#ifdef ECHO_MIC
                  audiobuffer[audioindex++]=micsample>>8;
                  audiobuffer[audioindex++]=micsample;
                  audiobuffer[audioindex++]=micsample>>8;
                  audiobuffer[audioindex++]=micsample;

                  if(audioindex>=sizeof(audiobuffer)) {
                      // insert the sequence
                      audiobuffer[0]=audiosequence>>24;
                      audiobuffer[1]=audiosequence>>16;
                      audiobuffer[2]=audiosequence>>8;
                      audiobuffer[3]=audiosequence;
                      // send the buffer
                      if(sendto(data_socket,audiobuffer,sizeof(audiobuffer),0,(struct sockaddr*)&audio_addr,audio_addr_length)<0) {
                          fprintf(stderr,"sendto socket failed for audio\n");
                          exit(1);
                      }
                      audioindex=4;
                      audiosequence++;
                  }
#endif
              }
          }
       } else {
       }

       if(running) {
           sem_wait(&send_general_sem);
           if(send_general==1) {
               new_protocol_general();
               send_general=0;
           }
           sem_post(&send_general_sem);

           sem_wait(&send_high_priority_sem);
           if(send_high_priority==1) {
               new_protocol_high_priority(1,isTransmitting(),tune==0?drive:tune_drive);
               send_high_priority=0;
           }
           sem_post(&send_high_priority_sem);
       }
        
    }

    close(data_socket);
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
