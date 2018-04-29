/* Copyright (C)
* 2017 - Johan Maas, PA3GSB
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
#include <semaphore.h>
#include <pthread.h>

#include <string.h>
#include <errno.h>
#include <math.h>

#include "audio.h"

#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "old_protocol.h"
#include "radio.h"
#include "toolbar.h"
#include "vox.h"
#include <semaphore.h>
#ifdef PSK
#include "psk.h"
#endif
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include <pigpio.h>
#include "ext.h"

#define SPEED_48K                 0x00
#define SPEED_96K                 0x01
#define SPEED_192K                0x02
#define SPEED_384K                0x03

static int display_width;
static int running;
static int sampleSpeed =0;

unsigned char tx_iqdata[6];

static GThread *radioberry_thread_id;
static gpointer radioberry_thread(gpointer arg);

static GThread *rx1_stream_thread_id;
static gpointer rx1_stream_thread(gpointer arg);
static GThread *rx2_stream_thread_id;
static gpointer rx2_stream_thread(gpointer arg);

static void setSampleSpeed(int r);
static void handleReceiveStream(int r);

struct timeval rx1_t0;
struct timeval rx1_t1;
struct timeval rx2_t0;
struct timeval rx2_t1;
struct timeval t10;
struct timeval t11;
struct timeval t20;
struct timeval t21;
float elapsed;

void spiWriter();
void rx1_spiReader();
void rx2_spiReader();

int prev_drive_level;

static int rx1_count =0;
static int rx2_count =0;
static int txcount =0;

GMutex buf_rx1_mutex;
GMutex buf_rx2_mutex;
GMutex mutex_rxtx;
GMutex mutex_rxstream;

#define MAX_RX_BUFFER (48000)


unsigned char buffer_rx1[MAX_RX_BUFFER];
int fill_rx1 = 0; 
int use_rx1  = 0;
sem_t empty;	
sem_t full;

void put_rx1(unsigned char[]);
void get_rx1(unsigned char[]);

unsigned char buffer_rx2[MAX_RX_BUFFER];
int fill_rx2 = 0; 
int use_rx2  = 0;
sem_t empty_rx2;	
sem_t full_rx2;

void put_rx2(unsigned char[]);
void get_rx2(unsigned char[]);
 
#ifdef PSK
static int psk_samples=0;
static int psk_divisor=6;
#endif

static int rx1_spi_handler;
static int rx2_spi_handler;


#define HANDLER_STEADY_TIME_US 5000
void setup_handler(int pin, gpioAlertFunc_t pAlert) {
  gpioSetMode(pin, PI_INPUT);
  gpioSetPullUpDown(pin,PI_PUD_UP);
  // give time to settle to avoid false triggers
  usleep(10000);
  gpioSetAlertFunc(pin, pAlert);
  gpioGlitchFilter(pin, HANDLER_STEADY_TIME_US);
}

void cwPTT_Alert(int gpio, int level, uint32_t tick) {
	//fprintf(stderr,"radioberry ptt swith %d  0=ptt off and 1=ptt on\n", level);
	//fprintf(stderr,"%d - %d -%d - %d\n", running, cw_breakin, transmitter->mode, level);
    if (running && cw_breakin && (transmitter->mode==modeCWU || transmitter->mode==modeCWL)){
		g_idle_add(ext_mox_update,(gpointer)level);
	}
}

float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

void radioberry_protocol_init(int rx,int pixels) {
	int i;

	sem_init(&empty, 0, MAX_RX_BUFFER); 
    sem_init(&full, 0, 0); 
	sem_init(&empty_rx2, 0, MAX_RX_BUFFER); 
    sem_init(&full_rx2, 0, 0); 

	fprintf(stderr,"radioberry_protocol_init\n");
	display_width=pixels;
	fprintf(stderr,"radioberry_protocol: buffer size: =%d\n", buffer_size);

#ifndef GPIO
	if (gpioInitialise() < 0) {
		fprintf(stderr,"radioberry_protocol: gpio could not be initialized. \n");
		exit(-1);
	}
#endif
  
 	gpioSetMode(13, PI_INPUT); 	//rx1_FIFOEmpty
	gpioSetMode(16, PI_INPUT);	//rx2_FIFOEmpty
	gpioSetMode(20, PI_INPUT); 
	gpioSetMode(21, PI_OUTPUT); 
	
	setup_handler(17, cwPTT_Alert); 
	
	rx1_spi_handler = spiOpen(0, 15625000, 49155);  //channel 0
	if (rx1_spi_handler < 0) {
		fprintf(stderr,"radioberry_protocol: spi bus rx1 could not be initialized. \n");
		exit(-1);
	}
	
	rx2_spi_handler = spiOpen(1, 15625000, 49155); 	//channel 1
	if (rx2_spi_handler < 0) {
		fprintf(stderr,"radioberry_protocol: spi bus rx2 could not be initialized. \n");
		exit(-1);
	}

	running=1;
	printf("init done \n");
 
	if(transmitter->local_microphone) {
		if(audio_open_input()!=0) {
		  fprintf(stderr,"audio_open_input failed\n");
		  transmitter->local_microphone=0;
		}
	 }

	radioberry_thread_id = g_thread_new( "radioberry", radioberry_thread, NULL);
	if( ! radioberry_thread_id )
	{
		fprintf(stderr,"g_thread_new failed on radioberry_thread\n");
		exit( -1 );
	}
	fprintf(stderr, "radioberry_thread: id=%p\n",radioberry_thread_id);
	 
	rx1_stream_thread_id = g_thread_new( "rx1-streaming", rx1_stream_thread, NULL);
	if( ! rx1_stream_thread_id )
	{
		fprintf(stderr,"g_thread_new failed on rx_stream_thread\n");
		exit( -1 );
	}
	fprintf(stderr, "rx1-streaming: id=%p\n",rx1_stream_thread_id);
	
	
	rx2_stream_thread_id = g_thread_new( "rx2-streaming", rx2_stream_thread, NULL);
	if( ! rx2_stream_thread_id )
	{
		fprintf(stderr,"g_thread_new failed on rx2_stream_thread\n");
		exit( -1 );
	}
	fprintf(stderr, "rx2-streaming: id=%p\n",rx2_stream_thread_id);
	
}

static gpointer rx1_stream_thread(gpointer arg) {
	
	int left_sample;
	int right_sample;
	double left_sample_double;
	double right_sample_double;
	
	unsigned char _iqdata[6];
	
	fprintf(stderr, "radioberry_protocol: rx_stream_thread\n");
	
	while(running) {
		
		get_rx1(_iqdata); 
	
		left_sample   = (int)((signed char) _iqdata[0]) << 16;
		left_sample  |= (int)((((unsigned char)_iqdata[1]) << 8)&0xFF00);
		left_sample  |= (int)((unsigned char)_iqdata[2]&0xFF);
		right_sample  = (int)((signed char) _iqdata[3]) << 16;
		right_sample |= (int)((((unsigned char)_iqdata[4]) << 8)&0xFF00);
		right_sample |= (int)((unsigned char)_iqdata[5]&0xFF);
		
		left_sample_double=(double)left_sample/8388607.0; // 24 bit sample 2^23-1
		right_sample_double=(double)right_sample/8388607.0; // 24 bit sample 2^23-1

		g_mutex_lock(&mutex_rxstream);
			// add the samples to the receiver..
			add_iq_samples(receiver[0], left_sample_double,right_sample_double);
		g_mutex_unlock(&mutex_rxstream);
	}
	
	fprintf(stderr, "radioberry_protocol stop: rx_stream_thread\n");
}

static gpointer rx2_stream_thread(gpointer arg) {
	
	int left_sample;
	int right_sample;
	double left_sample_double;
	double right_sample_double;
	
	unsigned char _iqdata[6];
	
	fprintf(stderr, "radioberry_protocol: rx2_stream_thread\n");
	
	while(running) {
		
		get_rx2(_iqdata); 
		
		left_sample   = (int)((signed char) _iqdata[0]) << 16;
		left_sample  |= (int)((((unsigned char)_iqdata[1]) << 8)&0xFF00);
		left_sample  |= (int)((unsigned char)_iqdata[2]&0xFF);
		right_sample  = (int)((signed char) _iqdata[3]) << 16;
		right_sample |= (int)((((unsigned char)_iqdata[4]) << 8)&0xFF00);
		right_sample |= (int)((unsigned char)_iqdata[5]&0xFF);
	
		left_sample_double=(double)left_sample/8388607.0; // 24 bit sample 2^23-1
		right_sample_double=(double)right_sample/8388607.0; // 24 bit sample 2^23-1
	
		g_mutex_lock(&mutex_rxstream);
			// add the samples to the receiver..
			add_iq_samples(receiver[1], left_sample_double,right_sample_double);
		g_mutex_unlock(&mutex_rxstream);
		
	}
	
	fprintf(stderr, "radioberry_protocol stop: rx2_stream_thread\n");
}

static gpointer radioberry_thread(gpointer arg) {
	fprintf(stderr, "radioberry_protocol: radioberry_thread\n");

	gettimeofday(&t20, 0);
	gettimeofday(&rx1_t0, 0);
	gettimeofday(&rx2_t0, 0);
	
	gpioSetMode(13, PI_INPUT); 
	gpioSetMode(16, PI_INPUT); 
	gpioSetMode(20, PI_INPUT); 
	gpioSetMode(21, PI_OUTPUT); 
	
	while(running) {
	
		if (!isTransmitting()) 
		{
			g_mutex_lock(&mutex_rxtx);
			
			gpioWrite(21, 0);
			
			rx1_spiReader();
			rx2_spiReader();
			
			g_mutex_unlock(&mutex_rxtx);
		}
	}
	
	fprintf(stderr, "radioberry_protocol stop: radioberry_thread\n");
}

void radioberry_protocol_iq_samples(int isample,int qsample) {
	
		g_mutex_lock(&mutex_rxtx);
		
		int power=0;
		 if(tune && !transmitter->tune_use_drive) {
            power=(int)((double)transmitter->drive_level/100.0*(double)transmitter->tune_percent);
          } else {
            power=transmitter->drive_level;
          }
		
		tx_iqdata[0] = 0;
		tx_iqdata[1] = power / 6.4;  // convert drive level from 0-255 to 0-39 steps of 0.5dB to control AD9866)
		if (prev_drive_level != transmitter->drive_level) {
			printf("drive level %d - corrected drive level %d \n", transmitter->drive_level, tx_iqdata[1]);
			prev_drive_level = transmitter->drive_level; 
		}		
		tx_iqdata[2] = isample>>8; 
		tx_iqdata[3] = isample;
		tx_iqdata[4] = qsample>>8;
		tx_iqdata[5] = qsample;

		spiWriter();

		g_mutex_unlock(&mutex_rxtx);
}

void *radioberry_protocol_process_local_mic(unsigned char *buffer,int le) {
	int b;
	short mic_sample;
	// always 48000 samples per second
 
    b=0;
    int i;
    for(i=0;i<1024;i++) {
		if(le) {
			mic_sample = (short)((buffer[b++]&0xFF) | (buffer[b++]<<8));
		} else {
			mic_sample = (short)((buffer[b++]<<8) | (buffer[b++]&0xFF));
		}
		add_mic_sample(transmitter,mic_sample);
	}
}

void setSampleSpeed(int r) {
	
     switch(receiver[r]->sample_rate) {
        case 48000:
          sampleSpeed=SPEED_48K;
          break;
        case 96000:
          sampleSpeed=SPEED_96K;
          break;
        case 192000:
          sampleSpeed=SPEED_192K;
          break;
        case 384000:
          sampleSpeed=SPEED_384K;
          break;
      }
}

void radioberry_protocol_stop() {
  
	running=FALSE;

	if (rx1_spi_handler !=0)
		spiClose(rx1_spi_handler);
	if (rx2_spi_handler !=0)
		spiClose(rx2_spi_handler);

	gpioTerminate();
	
}

void rx1_spiReader() {
	unsigned char iqdata[6];
	
	// return till rxFIFO buffer is filled with at least one element
	while ( gpioRead(13) == 1) {return;};
	
	setSampleSpeed(0);

	long long rxFrequency=vfo[0].frequency-vfo[0].lo;
	if(vfo[0].rit_enabled) {
	  rxFrequency+=vfo[0].rit;
	}
	if (active_receiver->id==0) {
		if(vfo[0].mode==modeCWU) {
			rxFrequency-=(long long)cw_keyer_sidetone_frequency;
		} else if(vfo[0].mode==modeCWL) {
			rxFrequency+=(long long)cw_keyer_sidetone_frequency;
		}
	}
	
	iqdata[0] = (sampleSpeed & 0x03);
	iqdata[1] = (((active_receiver->random << 6) & 0x40) | ((active_receiver->dither <<5) & 0x20) |  (attenuation & 0x1F));
	iqdata[2] = ((rxFrequency >> 24) & 0xFF);
	iqdata[3] = ((rxFrequency >> 16) & 0xFF);
	iqdata[4] = ((rxFrequency >> 8) & 0xFF);
	iqdata[5] = (rxFrequency & 0xFF);
			
	spiXfer(rx1_spi_handler, iqdata, iqdata, 6);
	
	//48'h0102030405060708
	//if (iqdata[0] != 1 || iqdata[1] != 2 || iqdata[3] != 3 || iqdata[4] != 4) {
	//	printf("error");
	//}
	//if (iqdata[5] != 0x05 | iqdata[6] != 0x06) {
	//	printf("error %x %x", iqdata[5], iqdata[6]);
	//}
	//printf("waarde %x %x %x %x %x %x \n", iqdata[0], iqdata[1], iqdata[2], iqdata[3], iqdata[4], iqdata[5]);
	//printf("xxxxxxxxxxxx");
			
	put_rx1(iqdata);
	
	rx1_count ++;
	if (rx1_count == 48000) {
		rx1_count = 0;
		gettimeofday(&rx1_t1, 0);
		elapsed = timedifference_msec(rx1_t0, rx1_t1);
		printf("Code rx1 mode spi executed in %f milliseconds.\n", elapsed);
		gettimeofday(&rx1_t0, 0);
	}

	
}

void rx2_spiReader() {
	
	unsigned char iqdata[6];
	
	if (receivers==1) return;

	// return till rx2FIFO buffer is filled with at least one element
	while ( gpioRead(16) == 1) {return;};
	
	setSampleSpeed(1);
	
	long long rxFrequency=vfo[1].frequency-vfo[1].lo;
	if(vfo[1].rit_enabled) {
	  rxFrequency+=vfo[1].rit;
	}
	if (active_receiver->id==1) {
		if(vfo[1].mode==modeCWU) {
			rxFrequency-=(long long)cw_keyer_sidetone_frequency;
		} else if(vfo[1].mode==modeCWL) {
			rxFrequency+=(long long)cw_keyer_sidetone_frequency;
		}
	}
	
	iqdata[0] = (sampleSpeed & 0x03);
	iqdata[1] = 0x00;	//not used by firmware; the active receiver settings are set via rx1
	iqdata[2] = ((rxFrequency >> 24) & 0xFF);
	iqdata[3] = ((rxFrequency >> 16) & 0xFF);
	iqdata[4] = ((rxFrequency >> 8) & 0xFF);
	iqdata[5] = (rxFrequency & 0xFF);
			
	spiXfer(rx2_spi_handler, iqdata, iqdata, 6);
			
	put_rx2(iqdata);
	
	rx2_count ++;
	if (rx2_count == 48000) {
		rx2_count = 0;
		gettimeofday(&rx2_t1, 0);
		elapsed = timedifference_msec(rx2_t0, rx2_t1);
		printf("Code rx2 mode spi executed in %f milliseconds.\n", elapsed);
		gettimeofday(&rx2_t0, 0);
	}
	
}

void spiWriter() {

	gpioWrite(21, 1); 

	while ( gpioRead(20) == 1) {};

	spiXfer(rx1_spi_handler, tx_iqdata, tx_iqdata, 6);
	
	long long txFrequency;
	if(split) {
		txFrequency=vfo[VFO_B].frequency-vfo[VFO_A].lo+vfo[VFO_B].offset;
	} else {
		txFrequency=vfo[VFO_A].frequency-vfo[VFO_B].lo+vfo[VFO_A].offset;
	}
	tx_iqdata[0] = cw_keyer_speed | (cw_keyer_mode<<6);
	tx_iqdata[1] = cw_keyer_weight | (cw_keyer_spacing<<7);
	tx_iqdata[2] = ((txFrequency >> 24) & 0xFF);
	tx_iqdata[3] = ((txFrequency >> 16) & 0xFF);
	tx_iqdata[4] = ((txFrequency >> 8) & 0xFF);
	tx_iqdata[5] = (txFrequency & 0xFF);
				
	spiXfer(rx2_spi_handler, tx_iqdata, tx_iqdata, 6);
	
	txcount ++;
	if (txcount == 48000) {
		txcount = 0;
		gettimeofday(&t21, 0);
		float elapsd = timedifference_msec(t20, t21);
		printf("Code tx mode spi executed in %f milliseconds.\n", elapsd);
		gettimeofday(&t20, 0);
	}
}

void put_rx1(unsigned char* value) {
	sem_wait(&empty);
	
	g_mutex_lock(&buf_rx1_mutex);
	int i =0;
	for (i; i< 6; i++){
		buffer_rx1[fill_rx1] = value[i];    
		fill_rx1 = (fill_rx1 + 1) % MAX_RX_BUFFER; 
	}
	g_mutex_unlock(&buf_rx1_mutex);
	
	sem_post(&full);
}

void get_rx1(unsigned char buffer[]) {
	sem_wait(&full);
	
	g_mutex_lock(&buf_rx1_mutex);
	int i =0;
	for (i; i< 6; i++){
		buffer[i] = buffer_rx1[use_rx1];   
		use_rx1 = (use_rx1 + 1) % MAX_RX_BUFFER;   
	}
	g_mutex_unlock(&buf_rx1_mutex);
	
	sem_post(&empty);
}

void put_rx2(unsigned char* value) {
	sem_wait(&empty_rx2);
	
	g_mutex_lock(&buf_rx2_mutex);
	int i =0;
	for (i; i< 6; i++){
		buffer_rx2[fill_rx2] = value[i];    
		fill_rx2 = (fill_rx2 + 1) % MAX_RX_BUFFER; 
	}
	g_mutex_unlock(&buf_rx2_mutex);
	
	sem_post(&full_rx2);
}

void get_rx2(unsigned char buffer[]) {
	sem_wait(&full_rx2); 
	
	g_mutex_lock(&buf_rx2_mutex);
	int i =0;
	for (i; i< 6; i++){
		buffer[i] = buffer_rx2[use_rx2];   
		use_rx2 = (use_rx2 + 1) % MAX_RX_BUFFER;   
	}
	g_mutex_unlock(&buf_rx2_mutex);
	
	sem_post(&empty_rx2); 
}

// end of source
