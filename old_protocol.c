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
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <wdsp.h>

#include "audio.h"
#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "old_protocol.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "signal.h"
#include "toolbar.h"
#include "vfo.h"
#ifdef FREEDV
#include "freedv.h"
#endif
#ifdef PSK
#include "psk.h"
#endif
#include "vox.h"
#include "ext.h"
#include "error_handler.h"

#define min(x,y) (x<y?x:y)

#define SYNC0 0
#define SYNC1 1
#define SYNC2 2
#define C0 3
#define C1 4
#define C2 5
#define C3 6
#define C4 7

#define DATA_PORT 1024

#define SYNC 0x7F
#define OZY_BUFFER_SIZE 512
//#define OUTPUT_BUFFER_SIZE 1024

// ozy command and control
#define MOX_DISABLED    0x00
#define MOX_ENABLED     0x01

#define MIC_SOURCE_JANUS 0x00
#define MIC_SOURCE_PENELOPE 0x80
#define CONFIG_NONE     0x00
#define CONFIG_PENELOPE 0x20
#define CONFIG_MERCURY  0x40
#define CONFIG_BOTH     0x60
#define PENELOPE_122_88MHZ_SOURCE 0x00
#define MERCURY_122_88MHZ_SOURCE  0x10
#define ATLAS_10MHZ_SOURCE        0x00
#define PENELOPE_10MHZ_SOURCE     0x04
#define MERCURY_10MHZ_SOURCE      0x08
#define SPEED_48K                 0x00
#define SPEED_96K                 0x01
#define SPEED_192K                0x02
#define SPEED_384K                0x03
#define MODE_CLASS_E              0x01
#define MODE_OTHERS               0x00
#define ALEX_ATTENUATION_0DB      0x00
#define ALEX_ATTENUATION_10DB     0x01
#define ALEX_ATTENUATION_20DB     0x02
#define ALEX_ATTENUATION_30DB     0x03
#define LT2208_GAIN_OFF           0x00
#define LT2208_GAIN_ON            0x04
#define LT2208_DITHER_OFF         0x00
#define LT2208_DITHER_ON          0x08
#define LT2208_RANDOM_OFF         0x00
#define LT2208_RANDOM_ON          0x10

//static int buffer_size=BUFFER_SIZE;

static int display_width;

static int speed;

static int dsp_rate=48000;
static int output_rate=48000;

static int data_socket;
static struct sockaddr_in data_addr;
static int data_addr_length;

static int output_buffer_size;

static unsigned char control_in[5]={0x00,0x00,0x00,0x00,0x00};

static double tuning_phase;
static double phase=0.0;

static int running;
static long ep4_sequence;

// DL1YCF added this variable for lost-package-check
static long last_seq_num=0;

static int current_rx=0;

static int samples=0;
static int mic_samples=0;
static int mic_sample_divisor=1;
#ifdef FREEDV
static int freedv_divisor=6;
#endif
#ifdef PSK
static int psk_samples=0;
static int psk_divisor=6;
#endif

static int local_ptt=0;

static double micinputbuffer[MAX_BUFFER_SIZE*2];

static int left_rx_sample;
static int right_rx_sample;
static int left_tx_sample;
static int right_tx_sample;

static unsigned char output_buffer[OZY_BUFFER_SIZE];
static int output_buffer_index=8;

static int command=1;

static GThread *receive_thread_id;
static void start_receive_thread();
static gpointer receive_thread(gpointer arg);
// DL1YCF changed buffer to uchar*
static void process_ozy_input_buffer(unsigned char  *buffer);
static void process_bandscope_buffer(char  *buffer);
void ozy_send_buffer();

static unsigned char metis_buffer[1032];
static long send_sequence=-1;
static int metis_offset=8;

// DL1YCF changed buffer to uchar*
static int metis_write(unsigned char ep,unsigned char* buffer,int length);
static void metis_start_stop(int command);
// DL1YCF changed buffer to uchar*
static void metis_send_buffer(unsigned char* buffer,int length);
static void metis_restart();

#define COMMON_MERCURY_FREQUENCY 0x80
#define PENELOPE_MIC 0x80

#ifdef USBOZY
//
// additional defines if we include USB Ozy support
//
#include "ozyio.h"

static GThread *ozy_EP4_rx_thread_id;
static GThread *ozy_EP6_rx_thread_id;
static gpointer ozy_ep4_rx_thread(gpointer arg);
static gpointer ozy_ep6_rx_thread(gpointer arg);
static void start_usb_receive_threads();
static int ozyusb_write(char* buffer,int length);
#define EP6_IN_ID  0x86                         // end point = 6, direction toward PC
#define EP2_OUT_ID  0x02                        // end point = 2, direction from PC
#define EP6_BUFFER_SIZE 2048
static unsigned char usb_output_buffer[EP6_BUFFER_SIZE];
static unsigned char ep6_inbuffer[EP6_BUFFER_SIZE];
static unsigned char usb_buffer_block = 0;
#endif

void old_protocol_stop() {
  metis_start_stop(0);
}

void old_protocol_run() {
  metis_restart();
}

void old_protocol_set_mic_sample_rate(int rate) {
  mic_sample_divisor=rate/48000;
}

void old_protocol_init(int rx,int pixels,int rate) {
  int i;

  fprintf(stderr,"old_protocol_init\n");

  old_protocol_set_mic_sample_rate(rate);

  if(transmitter->local_microphone) {
    if(audio_open_input()!=0) {
      fprintf(stderr,"audio_open_input failed\n");
      transmitter->local_microphone=0;
    }
  }

  display_width=pixels;
 
#ifdef USBOZY
//
// if we have a USB interfaced Ozy device:
//
  if (device == DEVICE_OZY) {
    fprintf(stderr,"old_protocol_init: initialise ozy on USB\n");
    ozy_initialise();
    start_usb_receive_threads();
  }
  else
#endif
  start_receive_thread();

  fprintf(stderr,"old_protocol_init: prime radio\n");
  for(i=8;i<OZY_BUFFER_SIZE;i++) {
    output_buffer[i]=0;
  }

  metis_restart();

}

#ifdef USBOZY
//
// starts the threads for USB receive
// EP4 is the bandscope endpoint (not yet used)
// EP6 is the "normal" USB frame endpoint
//
static void start_usb_receive_threads()
{
  int rc;

  fprintf(stderr,"old_protocol starting USB receive thread: buffer_size=%d\n",buffer_size);

  ozy_EP6_rx_thread_id = g_thread_new( "OZY EP6 RX", ozy_ep6_rx_thread, NULL);
  if( ! ozy_EP6_rx_thread_id )
  {
    fprintf(stderr,"g_thread_new failed for ozy_ep6_rx_thread\n");
    exit( -1 );
  }
}

//
// receive threat for USB EP4 (bandscope) not currently used.
//
static gpointer ozy_ep4_rx_thread(gpointer arg)
{
}

//
// receive threat for USB EP6 (512 byte USB Ozy frames)
// this function loops reading 4 frames at a time through USB
// then processes them one at a time.
//
static gpointer ozy_ep6_rx_thread(gpointer arg) {
  int bytes;
  unsigned char buffer[2048];

  fprintf(stderr, "old_protocol: USB EP6 receive_thread\n");
  running=1;
 
  while (running)
  {
    bytes = ozy_read(EP6_IN_ID,ep6_inbuffer,EP6_BUFFER_SIZE); // read a 2K buffer at a time

    if (bytes == 0)
    {
      fprintf(stderr,"old_protocol_ep6_read: ozy_read returned 0 bytes... retrying\n");
      continue;
    }
    else if (bytes != EP6_BUFFER_SIZE)
    {
      fprintf(stderr,"old_protocol_ep6_read: OzyBulkRead failed %d bytes\n",bytes);
      perror("ozy_read(EP6 read failed");
      //exit(1);
    }
    else
// process the received data normally
    {
      process_ozy_input_buffer(&ep6_inbuffer[0]);
      process_ozy_input_buffer(&ep6_inbuffer[512]);
      process_ozy_input_buffer(&ep6_inbuffer[1024]);
      process_ozy_input_buffer(&ep6_inbuffer[1024+512]);
    }

  }
}
#endif

static void start_receive_thread() {
  int i;
  int rc;
  struct hostent *h;

  fprintf(stderr,"old_protocol starting receive thread: buffer_size=%d output_buffer_size=%d\n",buffer_size,output_buffer_size);

  switch(device) {
#ifdef USBOZY
    case DEVICE_OZY:
      break;
#endif
    default:
      data_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
      if(data_socket<0) {
        perror("old_protocol: create socket failed for data_socket\n");
        exit(-1);
      }

      int optval = 1;
      if(setsockopt(data_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))<0) {
        perror("data_socket: SO_REUSEADDR");
      }
      if(setsockopt(data_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))<0) {
        perror("data_socket: SO_REUSEPORT");
      }

/*
      // set a timeout for receive
      struct timeval tv;
      tv.tv_sec=1;
      tv.tv_usec=0;
      if(setsockopt(data_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0) {
        perror("data_socket: SO_RCVTIMEO");
      }
*/
      // bind to the interface
      if(bind(data_socket,(struct sockaddr*)&radio->info.network.interface_address,radio->info.network.interface_length)<0) {
        perror("old_protocol: bind socket failed for data_socket\n");
        exit(-1);
      }

      memcpy(&data_addr,&radio->info.network.address,radio->info.network.address_length);
      data_addr_length=radio->info.network.address_length;
      data_addr.sin_port=htons(DATA_PORT);
      break;
  }

  receive_thread_id = g_thread_new( "old protocol", receive_thread, NULL);
  if( ! receive_thread_id )
  {
    fprintf(stderr,"g_thread_new failed on receive_thread\n");
    exit( -1 );
  }
  fprintf(stderr, "receive_thread: id=%p\n",receive_thread_id);

}

static gpointer receive_thread(gpointer arg) {
  struct sockaddr_in addr;
  // DL1YCF changed from int to socklen_t
  socklen_t length;
  unsigned char buffer[2048];
  int bytes_read;
  int ep;
  long sequence;

  fprintf(stderr, "old_protocol: receive_thread\n");
  running=1;

  length=sizeof(addr);
  while(running) {

    switch(device) {
#ifdef USBOZY
      case DEVICE_OZY:
        // should not happen
        break;
#endif

      default:
        bytes_read=recvfrom(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&addr,&length);
        if(bytes_read<0) {
          if(errno==EAGAIN) {
            error_handler("old_protocol: receiver_thread: recvfrom socket failed","Radio not sending data");
          } else {
            error_handler("old_protocol: receiver_thread: recvfrom socket failed",strerror(errno));
          }
          running=0;
          continue;
        }

        if(buffer[0]==0xEF && buffer[1]==0xFE) {
          switch(buffer[2]) {
            case 1:
              // get the end point
              ep=buffer[3]&0xFF;

              // get the sequence number
              sequence=((buffer[4]&0xFF)<<24)+((buffer[5]&0xFF)<<16)+((buffer[6]&0xFF)<<8)+(buffer[7]&0xFF);

	      // DL1YCF: added check on lost packets
              if (sequence != last_seq_num+1) {
		fprintf(stderr,"SEQ ERROR: last %ld, recvd %ld\n", last_seq_num, sequence);
	      }
	      last_seq_num=sequence;
              switch(ep) {
                case 6: // EP6
                  // process the data
                  process_ozy_input_buffer(&buffer[8]);
                  process_ozy_input_buffer(&buffer[520]);
                  break;
                case 4: // EP4
/*
                  ep4_sequence++;
                  if(sequence!=ep4_sequence) {
                    ep4_sequence=sequence;
                  } else {
                    int seq=(int)(sequence%32L);
                    if((sequence%32L)==0L) {
                      reset_bandscope_buffer_index();
                    }
                    process_bandscope_buffer(&buffer[8]);
                    process_bandscope_buffer(&buffer[520]);
                  }
*/
                  break;
                default:
                  fprintf(stderr,"unexpected EP %d length=%d\n",ep,bytes_read);
                  break;
              }
              break;
            case 2:  // response to a discovery packet
              fprintf(stderr,"unexepected discovery response when not in discovery mode\n");
              break;
            default:
              fprintf(stderr,"unexpected packet type: 0x%02X\n",buffer[2]);
              break;
          }
        } else {
          fprintf(stderr,"received bad header bytes on data port %02X,%02X\n",buffer[0],buffer[1]);
        }
        break;
    }
  }
  // DL1YCF added return statement to make compiler happy.
  return NULL;
}

// Dl1YCF changed buffer to uchar*
static void process_ozy_input_buffer(unsigned char  *buffer) {
  int i,j;
  int r;
  int b=0;
  unsigned char ozy_samples[8*8];
  int bytes;
  int previous_ptt;
  int previous_dot;
  int previous_dash;
  int left_sample;
  int right_sample;
  short mic_sample;
  double left_sample_double;
  double right_sample_double;
  double mic_sample_double;
  double gain=pow(10.0, mic_gain / 20.0);
  int left_sample_1;
  int right_sample_1;
  double left_sample_double_rx;
  double right_sample_double_rx;
  double left_sample_double_tx;
  double right_sample_double_tx;
  int nreceivers;

  int id=active_receiver->id;

  int tx_vfo=split?VFO_B:VFO_A;

  if(buffer[b++]==SYNC && buffer[b++]==SYNC && buffer[b++]==SYNC) {
    // extract control bytes
    control_in[0]=buffer[b++];
    control_in[1]=buffer[b++];
    control_in[2]=buffer[b++];
    control_in[3]=buffer[b++];
    control_in[4]=buffer[b++];

    previous_ptt=local_ptt;
    previous_dot=dot;
    previous_dash=dash;
    ptt=(control_in[0]&0x01)==0x01;
    dash=(control_in[0]&0x02)==0x02;
    dot=(control_in[0]&0x04)==0x04;

    local_ptt=ptt;
    if(vfo[tx_vfo].mode==modeCWL || vfo[tx_vfo].mode==modeCWU) {
      local_ptt=ptt|dot|dash;
    }
    if(previous_ptt!=local_ptt) {
      g_idle_add(ext_ptt_update,(gpointer)(long)(local_ptt));
    }


    switch((control_in[0]>>3)&0x1F) {
      case 0:
        adc_overload=control_in[1]&0x01;
        IO1=(control_in[1]&0x02)?0:1;
        IO2=(control_in[1]&0x04)?0:1;
        IO3=(control_in[1]&0x08)?0:1;
        if(mercury_software_version!=control_in[2]) {
          mercury_software_version=control_in[2];
          fprintf(stderr,"  Mercury Software version: %d (0x%0X)\n",mercury_software_version,mercury_software_version);
        }
        if(penelope_software_version!=control_in[3]) {
          penelope_software_version=control_in[3];
          fprintf(stderr,"  Penelope Software version: %d (0x%0X)\n",penelope_software_version,penelope_software_version);
        }
        if(ozy_software_version!=control_in[4]) {
          ozy_software_version=control_in[4];
          fprintf(stderr,"FPGA firmware version: %d.%d\n",ozy_software_version/10,ozy_software_version%10);
        }
        break;
      case 1:
        exciter_power=((control_in[1]&0xFF)<<8)|(control_in[2]&0xFF); // from Penelope or Hermes
        alex_forward_power=((control_in[3]&0xFF)<<8)|(control_in[4]&0xFF); // from Alex or Apollo
        break;
      case 2:
        alex_reverse_power=((control_in[1]&0xFF)<<8)|(control_in[2]&0xFF); // from Alex or Apollo
        AIN3=(control_in[3]<<8)+control_in[4]; // from Pennelope or Hermes
        break;
      case 3:
        AIN4=(control_in[1]<<8)+control_in[2]; // from Pennelope or Hermes
        AIN6=(control_in[3]<<8)+control_in[4]; // from Pennelope or Hermes
        break;
    }


#ifdef PURESIGNAL
    nreceivers=(RECEIVERS*2)+1;
#else
	#ifdef RADIOBERRY
		nreceivers = receivers;
	#else
		nreceivers=RECEIVERS;
	#endif
#endif

    int iq_samples=(512-8)/((nreceivers*6)+2);

    for(i=0;i<iq_samples;i++) {
      for(r=0;r<nreceivers;r++) {
        left_sample   = (int)((signed char) buffer[b++])<<16;
        left_sample  |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
        left_sample  |= (int)((unsigned char)buffer[b++]&0xFF);
        right_sample  = (int)((signed char)buffer[b++]) << 16;
        right_sample |= (int)((((unsigned char)buffer[b++])<<8)&0xFF00);
        right_sample |= (int)((unsigned char)buffer[b++]&0xFF);

        left_sample_double=(double)left_sample/8388607.0; // 24 bit sample 2^23-1
        right_sample_double=(double)right_sample/8388607.0; // 24 bit sample 2^23-1

#ifdef PURESIGNAL
        if(!isTransmitting() || (isTransmitting() && !transmitter->puresignal)) {
          switch(r) {
            case 0:
              add_iq_samples(receiver[0], left_sample_double,right_sample_double);
              break;
            case 1:
              break;
            case 2:
              add_iq_samples(receiver[1], left_sample_double,right_sample_double);
              break;
            case 3:
              break;
            case 4:
              break;
          }
        } else {
          switch(r) {
            case 0:
              if(device==DEVICE_METIS)  {
                left_sample_double_rx=left_sample_double;
                right_sample_double_rx=right_sample_double;
              }
              break;
            case 1:
              if(device==DEVICE_METIS)  {
                left_sample_double_tx=left_sample_double;
                right_sample_double_tx=right_sample_double;
                add_ps_iq_samples(transmitter, left_sample_double_rx,right_sample_double_rx,left_sample_double_tx,right_sample_double_tx);
              }
              break;
            case 2:
              if(device==DEVICE_HERMES)  {
                left_sample_double_rx=left_sample_double;
                right_sample_double_rx=right_sample_double;
              }
              break;
            case 3:
              if(device==DEVICE_METIS)  {
                left_sample_double_tx=left_sample_double;
                right_sample_double_tx=right_sample_double;
                add_ps_iq_samples(transmitter, left_sample_double_tx,right_sample_double_tx,left_sample_double_rx,right_sample_double_rx);
              } else if(device==DEVICE_ANGELIA || device==DEVICE_ORION || device==DEVICE_ORION2) {
                left_sample_double_rx=left_sample_double;
                right_sample_double_rx=right_sample_double;
              }
              break;
            case 4:
              if(device==DEVICE_ANGELIA || device==DEVICE_ORION || device==DEVICE_ORION2) {
                left_sample_double_tx=left_sample_double;
                right_sample_double_tx=right_sample_double;
                add_ps_iq_samples(transmitter, left_sample_double_tx,right_sample_double_tx,left_sample_double_rx,right_sample_double_rx);
              }
              break;
          }
        }
#else
        add_iq_samples(receiver[r], left_sample_double,right_sample_double);
#endif
      }

      mic_sample  = (short)(buffer[b++]<<8);
      mic_sample |= (short)(buffer[b++]&0xFF);
      if(!transmitter->local_microphone) {
        mic_samples++;
        if(mic_samples>=mic_sample_divisor) { // reduce to 48000
#ifdef FREEDV
          if(active_receiver->freedv) {
            add_freedv_mic_sample(transmitter,mic_sample);
          } else {
#endif
            add_mic_sample(transmitter,mic_sample);
#ifdef FREEDV
          }
#endif
          mic_samples=0;
        }
      }
    }
  } else {
    time_t t;
    struct tm* gmt;
    time(&t);
    gmt=gmtime(&t);

    fprintf(stderr,"%s: process_ozy_input_buffer: did not find sync: restarting\n",
            asctime(gmt));


    metis_start_stop(0);
    metis_restart();
  }
}

void old_protocol_audio_samples(RECEIVER *rx,short left_audio_sample,short right_audio_sample) {
  if(!isTransmitting()) {
    output_buffer[output_buffer_index++]=left_audio_sample>>8;
    output_buffer[output_buffer_index++]=left_audio_sample;
    output_buffer[output_buffer_index++]=right_audio_sample>>8;
    output_buffer[output_buffer_index++]=right_audio_sample;
    output_buffer[output_buffer_index++]=0;
    output_buffer[output_buffer_index++]=0;
    output_buffer[output_buffer_index++]=0;
    output_buffer[output_buffer_index++]=0;
    if(output_buffer_index>=OZY_BUFFER_SIZE) {
      ozy_send_buffer();
      output_buffer_index=8;
    }
  }
}

//
// This is a copy of old_protocol_iq_samples,
// but it includes the possibility to send a side tone
// We use it to provide a side-tone for CW/TUNE, in
// all other cases side==0 and this routine then is
// fully equivalent to old_protocol_iq_samples.
//
void old_protocol_iq_samples_with_sidetone(int isample, int qsample, int side) {
  if(isTransmitting()) {
    output_buffer[output_buffer_index++]=side >> 8;
    output_buffer[output_buffer_index++]=side;
    output_buffer[output_buffer_index++]=side >> 8;
    output_buffer[output_buffer_index++]=side;
    output_buffer[output_buffer_index++]=isample>>8;
    output_buffer[output_buffer_index++]=isample;
    output_buffer[output_buffer_index++]=qsample>>8;
    output_buffer[output_buffer_index++]=qsample;
    if(output_buffer_index>=OZY_BUFFER_SIZE) {
      ozy_send_buffer();
      output_buffer_index=8;
    }
  }
}

void old_protocol_iq_samples(int isample,int qsample) {
  if(isTransmitting()) {
    output_buffer[output_buffer_index++]=0;
    output_buffer[output_buffer_index++]=0;
    output_buffer[output_buffer_index++]=0;
    output_buffer[output_buffer_index++]=0;
    output_buffer[output_buffer_index++]=isample>>8;
    output_buffer[output_buffer_index++]=isample;
    output_buffer[output_buffer_index++]=qsample>>8;
    output_buffer[output_buffer_index++]=qsample;
    if(output_buffer_index>=OZY_BUFFER_SIZE) {
      ozy_send_buffer();
      output_buffer_index=8;
    }
  }
}


void old_protocol_process_local_mic(unsigned char *buffer,int le) {
  int b;
  int i;
  short sample;

// always 48000 samples per second
  b=0;
  for(i=0;i<720;i++) {
    // avoid pointer increments in logical-or constructs, as the sequence
    // is undefined
    if(le) {
      // DL1YCF: changed this to two statements such that the order of pointer increments
      //         becomes clearly defined.
      sample = (short) (buffer[b++]&0xFF);
      sample |= (short) (buffer[b++]<<8);
    } else {
      // DL1YCF: changed this to two statements such that the order of pointer increments
      //         becomes clearly defined.
      sample = (short)(buffer[b++]<<8);
      sample |=  (short) (buffer[b++]&0xFF);
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

/*
static void process_bandscope_buffer(char  *buffer) {
}
*/

#ifdef PROTOCOL_DEBUG
// DL1YCF Debug: save last values and log changes
static unsigned char last_c1[20], last_c2[20], last_c3[20], last_c4[20], last_mox;
#endif

void ozy_send_buffer() {

  int mode;
  int i;
  BAND *band;
  int nreceivers;

  output_buffer[SYNC0]=SYNC;
  output_buffer[SYNC1]=SYNC;
  output_buffer[SYNC2]=SYNC;

  if(metis_offset==8) {
    output_buffer[C0]=0x00;
    output_buffer[C1]=0x00;
    switch(active_receiver->sample_rate) {
      case 48000:
        output_buffer[C1]|=SPEED_48K;
        break;
      case 96000:
        output_buffer[C1]|=SPEED_96K;
        break;
      case 192000:
        output_buffer[C1]|=SPEED_192K;
        break;
      case 384000:
        output_buffer[C1]|=SPEED_384K;
        break;
    }

// set more bits for Atlas based device
// CONFIG_BOTH seems to be critical to getting ozy to respond
#ifdef USBOZY
    if ((device == DEVICE_OZY) || (device == DEVICE_METIS))
#else
    if (device == DEVICE_METIS)
#endif
    {
      if (atlas_mic_source)
        output_buffer[C1] |= PENELOPE_MIC;
      output_buffer[C1] |= CONFIG_BOTH;
      if (atlas_clock_source_128mhz)
        output_buffer[C1] |= MERCURY_122_88MHZ_SOURCE;
      output_buffer[C1] |= ((atlas_clock_source_10mhz & 3) << 2);
    }

    output_buffer[C2]=0x00;
    if(classE) {
      output_buffer[C2]|=0x01;
    }
    band=band_get_band(vfo[VFO_A].band);
    if(isTransmitting()) {
      if(split) {
        band=band_get_band(vfo[VFO_B].band);
      }
      output_buffer[C2]|=band->OCtx<<1;
      if(tune) {
        if(OCmemory_tune_time!=0) {
          struct timeval te;
          gettimeofday(&te,NULL);
          long long now=te.tv_sec*1000LL+te.tv_usec/1000;
          if(tune_timeout>now) {
            output_buffer[C2]|=OCtune<<1;
          }
        } else {
          output_buffer[C2]|=OCtune<<1;
        }
      }
    } else {
      output_buffer[C2]|=band->OCrx<<1;
    }

// TODO - add Alex Antenna
    output_buffer[C3]=0x00;
    output_buffer[C3] = receiver[0]->alex_attenuation;
    if(active_receiver->random) {
      output_buffer[C3]|=LT2208_RANDOM_ON;
    }
#ifdef RADIOBERRY
	if (rx_gain_slider[active_receiver->adc] > 31) 
	{
		output_buffer[C3]|=LT2208_DITHER_OFF;}
	else {
		output_buffer[C3]|=LT2208_DITHER_ON;
	}
#else
	if(active_receiver->dither) {
		output_buffer[C3]|=LT2208_DITHER_ON;
	}
#endif
    if(active_receiver->preamp) {
      output_buffer[C3]|=LT2208_GAIN_ON;
    }

    switch(receiver[0]->alex_antenna) {
      case 0:  // ANT 1
        break;
      case 1:  // ANT 2
        break;
      case 2:  // ANT 3
        break;
      case 3:  // EXT 1
        //output_buffer[C3]|=0xA0;
        output_buffer[C3]|=0xC0;
        break;
      case 4:  // EXT 2
        //output_buffer[C3]|=0xC0;
        output_buffer[C3]|=0xA0;
        break;
      case 5:  // XVTR
        output_buffer[C3]|=0xE0;
        break;
      default:
        break;
    }


// TODO - add Alex TX relay, duplex, receivers Mercury board frequency
    output_buffer[C4]=0x04;  // duplex
#ifdef PURESIGNAL
    nreceivers=(RECEIVERS*2)-1;
    nreceivers+=1; // for PS TX Feedback
#else
	#ifdef RADIOBERRY
		nreceivers = receivers-1;
	#else
		nreceivers=RECEIVERS-1;
	#endif
#endif

    output_buffer[C4]|=nreceivers<<3;
    
    if(isTransmitting()) {
      switch(transmitter->alex_antenna) {
        case 0:  // ANT 1
          output_buffer[C4]|=0x00;
          break;
        case 1:  // ANT 2
          output_buffer[C4]|=0x01;
          break;
        case 2:  // ANT 3
          output_buffer[C4]|=0x02;
          break;
        default:
          break;
      }
    } else {
      switch(receiver[0]->alex_antenna) {
        case 0:  // ANT 1
          output_buffer[C4]|=0x00;
          break;
        case 1:  // ANT 2
          output_buffer[C4]|=0x01;
          break;
        case 2:  // ANT 3
          output_buffer[C4]|=0x02;
          break;
        case 3:  // EXT 1
        case 4:  // EXT 2
        case 5:  // XVTR
          switch(transmitter->alex_antenna) {
            case 0:  // ANT 1
              output_buffer[C4]|=0x00;
              break;
            case 1:  // ANT 2
              output_buffer[C4]|=0x01;
              break;
            case 2:  // ANT 3
              output_buffer[C4]|=0x02;
              break;
          }
          break;
      }
    }
  } else {
    switch(command) {
      case 1: // tx frequency
        output_buffer[C0]=0x02;
        long long txFrequency;
        if(active_receiver->id==VFO_A) {
          if(split) {
            txFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo+vfo[VFO_B].offset;
          } else {
            txFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo+vfo[VFO_A].offset;
          }
        } else {
          if(split) {
            txFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo+vfo[VFO_A].offset;
          } else {
            txFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo+vfo[VFO_B].offset;
          }
        }
        output_buffer[C1]=txFrequency>>24;
        output_buffer[C2]=txFrequency>>16;
        output_buffer[C3]=txFrequency>>8;
        output_buffer[C4]=txFrequency;
        break;
      case 2: // rx frequency
#ifdef PURESIGNAL
        nreceivers=(RECEIVERS*2)+1;
#else
		#ifdef RADIOBERRY
			nreceivers = receivers;
		#else
			nreceivers=RECEIVERS;
		#endif
#endif

        if(current_rx<nreceivers) {
          output_buffer[C0]=0x04+(current_rx*2);
#ifdef PURESIGNAL
          int v=receiver[current_rx/2]->id;
          if(isTransmitting() && transmitter->puresignal) {
            long long txFrequency;
            if(active_receiver->id==VFO_A) {
              if(split) {
                txFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo+vfo[VFO_B].offset;
              } else {
                txFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo+vfo[VFO_A].offset;
              }
            } else {
              if(split) {
                txFrequency=vfo[VFO_A].frequency-vfo[VFO_A].lo+vfo[VFO_A].offset;
              } else {
                txFrequency=vfo[VFO_B].frequency-vfo[VFO_B].lo+vfo[VFO_B].offset;
              }
            }
            output_buffer[C1]=txFrequency>>24;
            output_buffer[C2]=txFrequency>>16;
            output_buffer[C3]=txFrequency>>8;
            output_buffer[C4]=txFrequency;
          } else {
#else
          int v=receiver[current_rx]->id;
#endif
            long long rxFrequency=vfo[v].frequency-vfo[v].lo;
            if(vfo[v].rit_enabled) {
              rxFrequency+=vfo[v].rit;
            }
            if(vfo[v].mode==modeCWU) {
              rxFrequency-=(long long)cw_keyer_sidetone_frequency;
            } else if(vfo[v].mode==modeCWL) {
              rxFrequency+=(long long)cw_keyer_sidetone_frequency;
            }
            output_buffer[C1]=rxFrequency>>24;
            output_buffer[C2]=rxFrequency>>16;
            output_buffer[C3]=rxFrequency>>8;
            output_buffer[C4]=rxFrequency;
#ifdef PURESIGNAL
          }
#endif
          current_rx++;
        }
        if(current_rx>=nreceivers) {
          current_rx=0;
        }
        break;
      case 3:
        {
        BAND *band=band_get_current_band();
        int power=0;
#ifdef STEMLAB_FIX
	//
	// DL1YCF:
	// On my HAMlab RedPitaya-based SDR transceiver, CW is generated on-board the RP.
	// However, while in CW mode, DriveLevel changes do not become effective.
	// If the CW paddle is hit, the new PTT state is sent to piHPSDR, then the TX drive
	// is sent the next time "command 3" is performed, but this often is too late and
	// CW is generated with zero DriveLevel.
	// Therefore, when in CW mode, send the TX drive level also when receiving.
	//
        if(split) {
          mode=vfo[1].mode;
        } else {
          mode=vfo[0].mode;
        }
        if(isTransmitting() || (mode == modeCWU) || (mode == modeCWL)) {
#else
        if(isTransmitting()) {
#endif
          if(tune && !transmitter->tune_use_drive) {
            power=(int)((double)transmitter->drive_level/100.0*(double)transmitter->tune_percent);
          } else {
            power=transmitter->drive_level;
          }
        }
  
        output_buffer[C0]=0x12;
        output_buffer[C1]=power&0xFF;
        output_buffer[C2]=0x00;
        if(mic_boost) {
          output_buffer[C2]|=0x01;
        }
        if(mic_linein) {
          output_buffer[C2]|=0x02;
        }
        if(filter_board==APOLLO) {
          output_buffer[C2]|=0x2C;
        }
        if((filter_board==APOLLO) && tune) {
          output_buffer[C2]|=0x10;
        }
        output_buffer[C3]=0x00;
        if(band_get_current()==band6) {
          output_buffer[C3]=output_buffer[C3]|0x40; // Alex 6M low noise amplifier
        }
        if(band->disablePA) {
          output_buffer[C3]=output_buffer[C3]|0x80; // disable PA
        }
        output_buffer[C4]=0x00;
        }
        break;
      case 4:
        output_buffer[C0]=0x14;
        output_buffer[C1]=0x00;
        for(i=0;i<receivers;i++) {
          output_buffer[C1]|=(receiver[i]->preamp<<i);
        }
        if(mic_ptt_enabled==0) {
          output_buffer[C1]|=0x40;
        }
        if(mic_bias_enabled) {
          output_buffer[C1]|=0x20;
        }
        if(mic_ptt_tip_bias_ring) {
          output_buffer[C1]|=0x10;
        }
        output_buffer[C2]=0x00;
        output_buffer[C2]|=linein_gain;
#ifdef PURESIGNAL
        if(transmitter->puresignal) {
          output_buffer[C2]|=0x40;
        }
#endif
        output_buffer[C3]=0x00;
  
        if(radio->device==DEVICE_HERMES || radio->device==DEVICE_ANGELIA || radio->device==DEVICE_ORION || radio->device==DEVICE_ORION2) {
          output_buffer[C4]=0x20|adc_attenuation[receiver[0]->adc];
        } else {
#ifdef RADIOBERRY
		  int att = 63 - rx_gain_slider[active_receiver->adc];
          output_buffer[C4]=0x20|att;
#else
		  output_buffer[C4]=0x00;
#endif
        }
        break;
      case 5:
        // need to add adc 2 and 3 attenuation
        output_buffer[C0]=0x16;
        output_buffer[C1]=0x00;
        if(receivers==2) {
          if(radio->device==DEVICE_HERMES || radio->device==DEVICE_ANGELIA || radio->device==DEVICE_ORION || radio->device==DEVICE_ORION2) {
            output_buffer[C1]=0x20|adc_attenuation[receiver[1]->adc];
          }
        }
        output_buffer[C2]=0x00;
        if(cw_keys_reversed!=0) {
          output_buffer[C2]|=0x40;
        }
        output_buffer[C3]=cw_keyer_speed | (cw_keyer_mode<<6);
        output_buffer[C4]=cw_keyer_weight | (cw_keyer_spacing<<7);
        break;
      case 6:
        // need to add tx attenuation and rx ADC selection
        output_buffer[C0]=0x1C;
        output_buffer[C1]=0x00;
#ifdef PURESIGNAL
        output_buffer[C1]|=receiver[0]->adc;
        output_buffer[C1]|=(receiver[0]->adc<<2);
        output_buffer[C1]|=receiver[1]->adc<<4;
        output_buffer[C1]|=(receiver[1]->adc<<6);
        output_buffer[C2]=0x00;
        if(transmitter->puresignal) {
          output_buffer[C2]|=receiver[2]->adc;
        }
#else
        output_buffer[C1]|=receiver[0]->adc;
        output_buffer[C1]|=(receiver[1]->adc<<2);
#endif
        output_buffer[C3]=0x00;
        output_buffer[C3]|=transmitter->attenuation;
        output_buffer[C4]=0x00;
        break;
      case 7:
        output_buffer[C0]=0x1E;
        if(split) {
          mode=vfo[1].mode;
        } else {
          mode=vfo[0].mode;
        }
        output_buffer[C1]=0x00;
        if(mode!=modeCWU && mode!=modeCWL) {
          // output_buffer[C1]|=0x00;
        } else {
          if((tune==1) || (vox==1) || (cw_keyer_internal==0)) {
            output_buffer[C1]|=0x00;
          } else {
            output_buffer[C1]|=0x01;
          }
        }
        output_buffer[C2]=cw_keyer_sidetone_volume;
        output_buffer[C3]=cw_keyer_ptt_delay;
        output_buffer[C4]=0x00;
        break;
      case 8:
        output_buffer[C0]=0x20;
        output_buffer[C1]=(cw_keyer_hang_time>>2) & 0xFF;
        output_buffer[C2]=cw_keyer_hang_time & 0x03;
        output_buffer[C3]=(cw_keyer_sidetone_frequency>>4) & 0xFF;
        output_buffer[C4]=cw_keyer_sidetone_frequency & 0x0F;
        break;
      case 9:
        output_buffer[C0]=0x22;
        output_buffer[C1]=(eer_pwm_min>>2) & 0xFF;
        output_buffer[C2]=eer_pwm_min & 0x03;
        output_buffer[C3]=(eer_pwm_max>>3) & 0xFF;
        output_buffer[C4]=eer_pwm_max & 0x03;
        break;
      case 10:
        output_buffer[C0]=0x24;
        output_buffer[C1]=0x00;
        if(isTransmitting()) {
          output_buffer[C1]|=0x80; // ground RX1 on transmit
        }
        output_buffer[C2]=0x00;
        if(receiver[0]->alex_antenna==5) { // XVTR
          output_buffer[C2]=0x02;
        }
        output_buffer[C3]=0x00;
        output_buffer[C4]=0x00;
        break;
    }

    if(current_rx==0) {
      command++;
      if(command>10) {
        command=1;
      }
    }
  
  }

  // set mox
  if(split) {
    mode=vfo[1].mode;
  } else {
    mode=vfo[0].mode;
  }
  if(mode==modeCWU || mode==modeCWL) {
//
//  VOX is set in CW if we send
//  morse code via CAT. This is
//  supported even if we use the
//  external keyer for our paddles.
//
    if(tune || vox) {
      output_buffer[C0]|=0x01;
    }
  } else {
    if(isTransmitting()) {
      output_buffer[C0]|=0x01;
    }
  }

#ifdef PROTOCOL_DEBUG
//
// DL1YCF debug:
// look for changed parameters and log them
// This is great for debugging protocol problems,
// such as the HAMlab CW error fixed above, so I
// leave it here deactivated
//
  int ind = output_buffer[C0] >> 1;
  if (last_c1[ind] != output_buffer[C1]) {
    fprintf(stderr, "C0=%x Old C1=%x New C1=%x\n", 2*ind,last_c1[ind], output_buffer[C1]);
    last_c1[ind]=output_buffer[C1];
  }
  if (last_c2[ind] != output_buffer[C2]) {
    fprintf(stderr, "C0=%x Old C2=%x New C2=%x\n", 2*ind,last_c2[ind], output_buffer[C2]);
    last_c2[ind]=output_buffer[C2];
  }
  if (last_c3[ind] != output_buffer[C3]) {
    fprintf(stderr, "C0=%x Old C3=%x New C3=%x\n", 2*ind,last_c3[ind], output_buffer[C3]);
    last_c3[ind]=output_buffer[C3];
  }
  if (last_c4[ind] != output_buffer[C4]) {
    fprintf(stderr, "C0=%x Old C4=%x New C4=%x\n", 2*ind,last_c4[ind], output_buffer[C4]);
    last_c4[ind]=output_buffer[C4];
  }
  if ((output_buffer[C0] & 1) != last_mox) {
    fprintf(stderr, "Last Mox=%d New Mox=%d\n", last_mox, output_buffer[C0] & 1);
    last_mox=output_buffer[C0] & 1;
  }
#endif

#ifdef USBOZY
//
// if we have a USB interfaced Ozy device:
//
  if (device == DEVICE_OZY)
        ozyusb_write(output_buffer,OZY_BUFFER_SIZE);
  else
#endif
  metis_write(0x02,output_buffer,OZY_BUFFER_SIZE);

  //fprintf(stderr,"C0=%02X C1=%02X C2=%02X C3=%02X C4=%02X\n",
  //                output_buffer[C0],output_buffer[C1],output_buffer[C2],output_buffer[C3],output_buffer[C4]);
}

#ifdef USBOZY
static int ozyusb_write(char* buffer,int length)
{
  int i;

// batch up 4 USB frames (2048 bytes) then do a USB write
  switch(usb_buffer_block++)
  {
    case 0:
    default:
      memcpy(usb_output_buffer, buffer, length);
      break;

    case 1:
      memcpy(usb_output_buffer + 512, buffer, length);
      break;

    case 2:
      memcpy(usb_output_buffer + 1024, buffer, length);
      break;

    case 3:
      memcpy(usb_output_buffer + 1024 + 512, buffer, length);
      usb_buffer_block = 0;           // reset counter
// and write the 4 usb frames to the usb in one 2k packet
      i = ozy_write(EP2_OUT_ID,usb_output_buffer,EP6_BUFFER_SIZE);
      if(i != EP6_BUFFER_SIZE)
      {
        perror("old_protocol: OzyWrite ozy failed");
      }
      break;
  }
}
#endif

// DL1YCF change buffer to uchar*
static int metis_write(unsigned char ep,unsigned char* buffer,int length) {
  int i;

  // copy the buffer over
  for(i=0;i<512;i++) {
    metis_buffer[i+metis_offset]=buffer[i];
  }

  if(metis_offset==8) {
    metis_offset=520;
  } else {
    send_sequence++;
    metis_buffer[0]=0xEF;
    metis_buffer[1]=0xFE;
    metis_buffer[2]=0x01;
    metis_buffer[3]=ep;
    metis_buffer[4]=(send_sequence>>24)&0xFF;
    metis_buffer[5]=(send_sequence>>16)&0xFF;
    metis_buffer[6]=(send_sequence>>8)&0xFF;
    metis_buffer[7]=(send_sequence)&0xFF;


    // send the buffer
    metis_send_buffer(&metis_buffer[0],1032);
    metis_offset=8;

  }

  return length;
}

static void metis_restart() {
  // reset metis frame
  // DL1YCF change == to = in the next line
  metis_offset=8;

  // reset current rx
  current_rx=0;

#ifdef STEMLAB_FIX
  // DL1YCF:
  // My RedPitaya HPSDR "clone" won't start up
  // if too many commands are sent here. Note these
  // packets are only there for sync-ing in the clock
  // source etc.
  // Note that always two 512-byte OZY buffers are
  // combined into one METIS packet.
  //
  command=1;   // ship out a "C0=0" and a "set tx" command
  ozy_send_buffer();
  ozy_send_buffer();
  command=2;  // ship out a "C0=0" and a "set rx" command for RX1
  ozy_send_buffer();
  ozy_send_buffer();

  // DL1YCF: reset for the next commands
  current_rx=0;
  command=1;
#else
  // DL1YCF this is the original code, which does not do what it pretends ....
  // send commands twice
  command=1;
  do {
    ozy_send_buffer();
  } while (command!=1);

  do {
    ozy_send_buffer();
  } while (command!=1);
#endif

  sleep(1);

  // start the data flowing
  metis_start_stop(1);
}

static void metis_start_stop(int command) {
  int i;
  unsigned char buffer[64];
    
#ifdef USBOZY
  if(device!=DEVICE_OZY)
  {
#endif

  buffer[0]=0xEF;
  buffer[1]=0xFE;
  buffer[2]=0x04;    // start/stop command
  buffer[3]=command;    // send EP6 and EP4 data (0x00=stop)

  for(i=0;i<60;i++) {
    buffer[i+4]=0x00;
  }

  metis_send_buffer(buffer,sizeof(buffer));
#ifdef USBOZY
  }
#endif
}

// DL1YCF changedbuffer to uchar *
static void metis_send_buffer(unsigned char* buffer,int length) {
  if(sendto(data_socket,buffer,length,0,(struct sockaddr*)&data_addr,data_addr_length)!=length) {
    perror("sendto socket failed for metis_send_data\n");
  }
}


