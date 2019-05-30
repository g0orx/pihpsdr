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

static int data_socket=-1;
static int tcp_socket=-1;
static struct sockaddr_in data_addr;

static int output_buffer_size;

static unsigned char control_in[5]={0x00,0x00,0x00,0x00,0x00};

static double tuning_phase;
static double phase=0.0;

static int running;
static long ep4_sequence;

static uint32_t last_seq_num=-0xffffffff;
static int suppress_ozy_packet = 0;

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
static gpointer receive_thread(gpointer arg);
static void process_ozy_input_buffer(unsigned char  *buffer);
static void process_bandscope_buffer(char  *buffer);
void ozy_send_buffer();

static unsigned char metis_buffer[1032];
static uint32_t send_sequence=0;
static int metis_offset=8;

static int metis_write(unsigned char ep,unsigned char* buffer,int length);
static void metis_start_stop(int command);
static void metis_send_buffer(unsigned char* buffer,int length);
static void metis_restart();

static void open_tcp_socket(void);
static void open_udp_socket(void);

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
  {
    fprintf(stderr,"old_protocol starting receive thread: buffer_size=%d output_buffer_size=%d\n",buffer_size,output_buffer_size);
    if (radio->use_tcp) {
      open_tcp_socket();
    } else  {
      open_udp_socket();
    }
    receive_thread_id = g_thread_new( "old protocol", receive_thread, NULL);
    if( ! receive_thread_id )
    {
      fprintf(stderr,"g_thread_new failed on receive_thread\n");
      exit( -1 );
    }
    fprintf(stderr, "receive_thread: id=%p\n",receive_thread_id);
  }


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

static void open_udp_socket() {
    int tmp;

    if (data_socket >= 0) {
      tmp=data_socket;
      data_socket=-1;
      usleep(100000);
      close(tmp);
    }
    tmp=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(tmp<0) {
      perror("old_protocol: create socket failed for data_socket\n");
      exit(-1);
    }

    int optval = 1;
    if(setsockopt(tmp, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))<0) {
      perror("data_socket: SO_REUSEADDR");
    }
    if(setsockopt(tmp, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))<0) {
      perror("data_socket: SO_REUSEPORT");
    }
    optval=0xffff;
    if (setsockopt(tmp, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval))<0) {
      perror("data_socket: SO_SNDBUF");
    }
    if (setsockopt(tmp, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval))<0) {
      perror("data_socket: SO_RCVBUF");
    }
    //
    // set a timeout for receive
    // This is necessary because we might already "sit" in an UDP recvfrom() call while
    // instructing the radio to switch to TCP. Then this call has to finish eventually
    // and the next recvfrom() then uses the TCP socket.
    //
    struct timeval tv;
    tv.tv_sec=0;
    tv.tv_usec=100000;
    if(setsockopt(tmp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0) {
      perror("data_socket: SO_RCVTIMEO");
    }

    // bind to the interface
    if(bind(tmp,(struct sockaddr*)&radio->info.network.interface_address,radio->info.network.interface_length)<0) {
      perror("old_protocol: bind socket failed for data_socket\n");
      exit(-1);
    }

    memcpy(&data_addr,&radio->info.network.address,radio->info.network.address_length);
    data_addr.sin_port=htons(DATA_PORT);
    data_socket=tmp;
    fprintf(stderr,"UDP socket established: %d\n", data_socket);
}

static void open_tcp_socket() {
    int tmp;

    if (tcp_socket >= 0) {
      tmp=tcp_socket;
      tcp_socket=-1;
      usleep(100000);
      close(tmp);
    }
    memcpy(&data_addr,&radio->info.network.address,radio->info.network.address_length);
    data_addr.sin_port=htons(DATA_PORT);
    data_addr.sin_family = AF_INET;
    fprintf(stderr,"Trying to open TCP connection to %s\n", inet_ntoa(radio->info.network.address.sin_addr));

    tmp=socket(AF_INET, SOCK_STREAM, 0);
    if (tmp < 0) {
      perror("tcp_socket: create socket failed for TCP socket");
      exit(-1);
    }
    int optval = 1;
    if(setsockopt(tmp, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))<0) {
      perror("tcp_socket: SO_REUSEADDR");
    }
    if(setsockopt(tmp, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))<0) {
      perror("tcp_socket: SO_REUSEPORT");
    }
    if (connect(tmp,(const struct sockaddr *)&data_addr,sizeof(data_addr)) < 0) {
      perror("tcp_socket: connect");
    }
    optval=0xffff;
    if (setsockopt(tmp, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval))<0) {
      perror("tcp_socket: SO_SNDBUF");
    }
    if (setsockopt(tmp, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval))<0) {
      perror("tcp_socket: SO_RCVBUF");
    }
    tcp_socket=tmp;
    fprintf(stderr,"TCP socket established: %d\n", tcp_socket);
}

static gpointer receive_thread(gpointer arg) {
  struct sockaddr_in addr;
  socklen_t length;
  unsigned char buffer[2048];
  int bytes_read;
  int ret,left;
  int ep;
  uint32_t sequence;

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
	for (;;) {
          if (tcp_socket >= 0) {
	    // TCP messages may be split, so collect exactly 1032 bytes.
	    // Remember, this is a STREAMING protocol.
            bytes_read=0;
            left=1032;
            while (left > 0) {
              ret=recvfrom(tcp_socket,buffer+bytes_read,(size_t)(left),0,NULL,0);
              if (ret < 0 && errno == EAGAIN) continue; // time-out
              if (ret < 0) break;                       // error
              bytes_read += ret;
              left -= ret;
            }
	    if (ret < 0) {
	      bytes_read=ret;                          // error case: discard whole packet
              //perror("old_protocol recvfrom TCP:");
	    }
	  } else if (data_socket >= 0) {
            bytes_read=recvfrom(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&addr,&length);
            if(bytes_read < 0 && errno != EAGAIN) perror("old_protocol recvfrom UDP:");
          } else {
	    // This could happen in METIS start/stop sequences
	    usleep(100000);
	    continue;
	  }
          if(bytes_read >= 0 || errno != EAGAIN) break;
	}
        if(bytes_read <= 0) {
          continue;
        }

        if(buffer[0]==0xEF && buffer[1]==0xFE) {
          switch(buffer[2]) {
            case 1:
              // get the end point
              ep=buffer[3]&0xFF;

              // get the sequence number
              sequence=((buffer[4]&0xFF)<<24)+((buffer[5]&0xFF)<<16)+((buffer[6]&0xFF)<<8)+(buffer[7]&0xFF);

	      // A sequence error with a seqnum of zero usually indicates a METIS restart
	      // and is no error condition
              if (sequence != 0 && sequence != last_seq_num+1) {
		fprintf(stderr,"SEQ ERROR: last %ld, recvd %ld\n", (long) last_seq_num, (long) sequence);
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
  return NULL;
}

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

  int id=active_receiver->id;

  int tx_vfo=split?VFO_B:VFO_A;

  int nreceivers;

#ifdef PURESIGNAL
    // for PureSignal, the number of receivers needed is hard-coded below.
    // we need at least 3 (for RX), and up to 5 for Orion2 boards, since
    // the TX DAC channel is hard-wired to RX5.
    nreceivers=3;
    if (device == DEVICE_HERMES) nreceivers=4;
    if (device == DEVICE_ANGELIA || device == DEVICE_ORION || device == DEVICE_ORION2) nreceivers=5;
#else
#if defined(RADIOBERRY) || defined(PI_SDR)
	nreceivers = receivers;
#else
	nreceivers=RECEIVERS;
#endif
#endif

  if(buffer[b++]==SYNC && buffer[b++]==SYNC && buffer[b++]==SYNC) {
    // extract control bytes
    control_in[0]=buffer[b++];
    control_in[1]=buffer[b++];
    control_in[2]=buffer[b++];
    control_in[3]=buffer[b++];
    control_in[4]=buffer[b++];

    // do not set ptt. In PURESIGNAL, this would stop the
    // receiver sending samples to WDSP abruptly.
    // Do the RX-TX change only via ext_mox_update.
    previous_ptt=local_ptt;
    previous_dot=dot;
    previous_dash=dash;
    local_ptt=(control_in[0]&0x01)==0x01;
    dash=(control_in[0]&0x02)==0x02;
    dot=(control_in[0]&0x04)==0x04;

    if (dot || dash) cw_key_hit=1;
    if(vfo[tx_vfo].mode==modeCWL || vfo[tx_vfo].mode==modeCWU) {
      local_ptt=local_ptt|dot|dash;
    }
    if(previous_ptt!=local_ptt) {
      g_idle_add(ext_mox_update,(gpointer)(long)(local_ptt));
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
                add_ps_iq_samples(transmitter, left_sample_double_tx,right_sample_double_tx,left_sample_double_rx,right_sample_double_rx);
              }
              break;
            case 2:
              if(device==DEVICE_HERMES)  {
                left_sample_double_rx=left_sample_double;
                right_sample_double_rx=right_sample_double;
              }
              break;
            case 3:
              if(device==DEVICE_HERMES)  {
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
      sample = (short) (buffer[b++]&0xFF);
      sample |= (short) (buffer[b++]<<8);
    } else {
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

void ozy_send_buffer() {

  int mode;
  int i;
  BAND *band;
  int nreceivers;

#ifdef PURESIGNAL
    // for PureSignal, the number of receivers needed is hard-coded below.
    // we need at least 3 (for RX), and up to 5 for Orion2 boards, since
    // the TX DAC channel is hard-wired to RX5.
    nreceivers=3;
    if (device == DEVICE_HERMES) nreceivers=4;
    if (device == DEVICE_ANGELIA || device == DEVICE_ORION || device == DEVICE_ORION2) nreceivers=5;
#else
#if defined(RADIOBERRY) || defined(PI_SDR)
	nreceivers = receivers;
#else
	nreceivers=RECEIVERS;
#endif
#endif

  output_buffer[SYNC0]=SYNC;
  output_buffer[SYNC1]=SYNC;
  output_buffer[SYNC2]=SYNC;

  if(metis_offset==8) {
    //
    // Every second packet is a "C0=0" packet
    //
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

    output_buffer[C3] = (receiver[0]->alex_attenuation) & 0x03;  // do not set higher bits
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

    i=receiver[0]->alex_antenna;
#ifdef PURESIGNAL
    //
    // Upon TX, we might have to activate a different RX path for the
    // attenuated feedback signal. Use feedback_antenna == 0, if
    // the feedback signal is routed automatically/internally
    // If feedback is to the second ADC, leave RX1 ANT settings untouched
    //
    if (isTransmitting() && transmitter->puresignal && receiver[PS_RX_FEEDBACK]->adc == 0) i=receiver[PS_RX_FEEDBACK]->feedback_antenna;
#endif
    switch(i) {
      case 3:  // Alex: RX2 IN, ANAN: EXT1, ANAN7000: still uses internal feedback 
        output_buffer[C3]|=0x80;
        break;
      case 4:  // Alex: RX1 IN, ANAN: EXT2, ANAN7000: RX BYPASS
        output_buffer[C3]|=0xA0;
        break;
      default:
	// RX1_OUT and RX1_ANT bits remain zero
        break;
    }


    output_buffer[C4]=0x04;  // duplex

    // 0 ... 7 maps on 1 ... 8 receivers
    output_buffer[C4]|=(nreceivers-1)<<3;
    
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
    //
    // metis_offset !=8: send the other C&C packets in round-robin
    // RX frequency commands are repeated for each RX
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
        if(current_rx<nreceivers) {
          output_buffer[C0]=0x04+(current_rx*2);
#ifdef PURESIGNAL
          int v=receiver[current_rx/2]->id;
	  // for the "last" receiver, v is out of range. In this case,
	  // use TX frequency also while receiving
          if((isTransmitting() && transmitter->puresignal) || (v >= MAX_VFOS)) {
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
	// Some HPSDR apps for the RedPitaya generate CW inside the FPGA, but while
	// doing this, DriveLevel changes are processed by the server, but do not become effective.
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
        output_buffer[C4]=0x00;
  
        if(radio->device==DEVICE_HERMES || radio->device==DEVICE_ANGELIA || radio->device==DEVICE_ORION || radio->device==DEVICE_ORION2) {
	  // if attenuation is zero, then disable attenuator
	  i = adc_attenuation[receiver[0]->adc] & 0x1F;
          if (i >0) output_buffer[C4]=0x20| i;
        } else {
#ifdef RADIOBERRY
	  int att = 63 - rx_gain_slider[active_receiver->adc];
          output_buffer[C4]=0x20|att;
#endif
        }
        break;
      case 5:
        output_buffer[C0]=0x16;
        output_buffer[C1]=0x00;
        if(receivers==2) {
          if(radio->device==DEVICE_HERMES || radio->device==DEVICE_ANGELIA || radio->device==DEVICE_ORION || radio->device==DEVICE_ORION2) {
	    // if attenuation is zero, then disable attenuator
	    i = adc_attenuation[receiver[1]->adc] & 0x1F;
            if (i > 0) output_buffer[C1]=0x20|i;
          }
        }
        output_buffer[C2]=0x00; // ADC3 attenuator disabled.
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
        output_buffer[C2]=0x00;
#ifdef PURESIGNAL
        // if n_adc == 1, there is only a single ADC, so we can leave everything
        // set to zero
	if (n_adc  > 1) {
	    // Angelia, Orion, Orion2 have two ADCs, so we use the ADC settings from the menu
            output_buffer[C1]|=receiver[0]->adc;			// RX1 bound to ADC of first receiver
            output_buffer[C1]|=(receiver[1]->adc<<2);			// RX2 actually unsused with PURESIGNAL
            output_buffer[C1]|=receiver[1]->adc<<4;			// RX3 bound to ADC of second receiver
            output_buffer[C1]|=(receiver[PS_RX_FEEDBACK]->adc<<6);	// RX4 is PS_RX_Feedbacka
									// Usually ADC0, but if feedback is to
									// RX2 input it must be ADC1 (see ps_menu.c)
	    // RX5 is hard-wired to the TX DAC and needs no ADC setting.
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
          if((tune==1) || (vox==1) || (cw_keyer_internal==0) || (transmitter->twotone==1)) {
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
#ifdef PURESIGNAL
	  // If we are using the RX2 jack for PURESIGNAL RX feedback, then we MUST NOT ground
	  // the ADC2 input upon TX.
	  if (transmitter->puresignal && receiver[PS_RX_FEEDBACK]->adc == 1) {
	    // Note that this statement seems to have no effect since
	    // one cannot goto to "manual filter setting" for the ALEX2 board individually
            output_buffer[C1]|=0x40; // Set ADC2 filter board to "ByPass"
	  } else
#endif

            output_buffer[C1]|=0x80; // ground RX2 on transmit, bit0-6 are Alex2 filters
        }
        output_buffer[C2]=0x00;
        if(receiver[0]->alex_antenna==5) { // XVTR
          output_buffer[C2]=0x02;          // Alex2 XVTR enable
        }
#ifdef PURESIGNAL
        if(transmitter->puresignal) {
          output_buffer[C2]|=0x40;	   // Synchronize RX5 andh TX frequency on transmit
        }
#endif
        output_buffer[C3]=0x00;            // Alex2 filters
        output_buffer[C4]=0x00;            // Alex2 filters
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
  if (isTransmitting()) {
    if(mode==modeCWU || mode==modeCWL) {
//
//    If CW is done on the HPSDR board, we should not set
//    the MOX bit, everything is done in the FPGA.
//
//    However, if we are doing CAT CW, local CW or tuning/TwoTone,
//    we must put the SDR into TX mode *here*.
//
      if(tune || CAT_cw_is_active || !cw_keyer_internal || transmitter->twotone) {
        output_buffer[C0]|=0x01;
      }
    } else {
      // not doing CW? always set MOX if transmitting
      output_buffer[C0]|=0x01;
    }
  }

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

static int metis_write(unsigned char ep,unsigned char* buffer,int length) {
  int i;

  // copy the buffer over
  for(i=0;i<512;i++) {
    metis_buffer[i+metis_offset]=buffer[i];
  }

  if(metis_offset==8) {
    metis_offset=520;
  } else {
    metis_buffer[0]=0xEF;
    metis_buffer[1]=0xFE;
    metis_buffer[2]=0x01;
    metis_buffer[3]=ep;
    metis_buffer[4]=(send_sequence>>24)&0xFF;
    metis_buffer[5]=(send_sequence>>16)&0xFF;
    metis_buffer[6]=(send_sequence>>8)&0xFF;
    metis_buffer[7]=(send_sequence)&0xFF;

    //
    // When using UDP, the buffer will ALWAYS be sent. However, when using TCP,
    // we must be able to suppress sending buffers HERE asynchronously
    // when we want to sent a METIS start or stop packet. This is so because TCP
    // is a byte stream, and data from two sources might end up interleaved
    // In order not to confuse the SDR, we increase the sequence number only
    // for packets actually sent.
    //
    if (!suppress_ozy_packet) {
      // send the buffer
      send_sequence++;
      metis_send_buffer(&metis_buffer[0],1032);
    }
    metis_offset=8;

  }

  return length;
}

static void metis_restart() {
  //
  // In TCP-ONLY mode, we possibly need to re-connect
  // since if we come from a METIS-stop, the server
  // has closed the socket. Note that the UDP socket, once
  // opened is never closed.
  //
  if (radio->use_tcp && tcp_socket < 1) open_tcp_socket();

  // reset metis frame
  metis_offset=8;

  // reset current rx
  current_rx=0;

#ifdef STEMLAB_FIX
  // 
  // Some (older) HPSDR apps on the RedPitaya have very small
  // buffers that over-run if too much data is sent
  // to the RedPitaya *before* sending a METIS start packet.
  // Therefore we send only four OZY buffers here.
  //
  command=1;   // ship out a "C0=0" and a "set tx" command
  ozy_send_buffer();
  ozy_send_buffer();
  command=2;  // ship out a "C0=0" and a "set rx" command for RX1
  ozy_send_buffer();
  ozy_send_buffer();

  current_rx=0;
  command=1;
#else
  // DL1YCF: this is the original code, which does not do what it pretends ....
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
  int tmp;
  unsigned char buffer[1032];
    
#ifdef USBOZY
  if(device!=DEVICE_OZY)
  {
#endif

  buffer[0]=0xEF;
  buffer[1]=0xFE;
  buffer[2]=0x04;	// start/stop command
  buffer[3]=command;	// send EP6 and EP4 data (0x00=stop)

  if (tcp_socket < 0) {
    // use UDP  -- send a short packet
    for(i=4;i<64;i++) {
      buffer[i]=0x00;
    }
    metis_send_buffer(buffer,64);
  } else {
    // use TCP -- send a long packet
    //
    // Stop the sending of TX/audio packets (1032-byte-length) and wait a while
    // Then, send the start/stop buffer with a length of 1032
    //
    suppress_ozy_packet=1;
    usleep(100000);
    for(i=4;i<1032;i++) {
      buffer[i]=0x00;
    }
    metis_send_buffer(buffer,1032);
    //
    // Wait a while before resuming sending TX/audio packets.
    // This prevents mangling of data from TX/audio and Start/Stop packets.
    //
    usleep(100000);
    suppress_ozy_packet=0;
  }
  if (command == 0 && tcp_socket >= 0) {
    // We just have sent a METIS stop in TCP
    // Radio will close the TCP connection, therefore we do this as well
    tmp=tcp_socket;
    tcp_socket=-1;
    usleep(100000);  // give some time to swallow incoming TCP packets
    close(tmp);
    fprintf(stderr,"TCP socket closed\n");
  }

#ifdef USBOZY
  }
#endif
}

static void metis_send_buffer(unsigned char* buffer,int length) {
  //
  // Send using either the UDP or TCP socket. Do not use TCP for
  // packets that are not 1032 bytes long
  //

  if (tcp_socket >= 0) {
    if (length != 1032) {
       fprintf(stderr,"PROGRAMMING ERROR: TCP LENGTH != 1032\n");
       exit(-1);
    }
    if(sendto(tcp_socket,buffer,length,0,NULL, 0) != length) {
      perror("sendto socket failed for TCP metis_send_data\n");
    }
  } else if (data_socket >= 0) {
    if(sendto(data_socket,buffer,length,0,(struct sockaddr*)&data_addr,sizeof(data_addr))!=length) {
      perror("sendto socket failed for UDP metis_send_data\n");
    }
  } else {
    // This should not happen
    fprintf(stderr,"METIS send: neither UDP nor TCP socket available!\n");
    exit(-1);
  }
}


