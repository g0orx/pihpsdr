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

#define OUTPUT_BUFFER_SIZE 1024
#define SPEED_48K                 0x00
#define SPEED_96K                 0x01
#define SPEED_192K                0x02
#define SPEED_384K                0x03

static int display_width;
static int running;
static int sampleSpeed =0;

unsigned char iqdata[6];
unsigned char tx_iqdata[6];

static pthread_t radioberry_thread_id;
static void start_radioberry_thread();
static void *radioberry_thread(void* arg);

static void setSampleSpeed();
static void handleReceiveStream();

struct timeval t0;
struct timeval t1;
struct timeval t10;
struct timeval t11;
struct timeval t20;
struct timeval t21;
float elapsed;

#define RADIOSTATE_RX   0
#define RADIOSTATE_TX   1
static int radiostate = RADIOSTATE_RX;

void spiWriter();
void spiReader();


int prev_drive_level;

static int rxcount =0;
static int txcount =0;

sem_t mutex;

#ifdef PSK
static int psk_samples=0;
static int psk_divisor=6;
#endif

static int h;


float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

void radioberry_protocol_init(int rx,int pixels) {
	int i;

	fprintf(stderr,"radioberry_protocol_init\n");
	sem_init(&mutex, 0, 1);	//mutal exlusion
	display_width=pixels;
	fprintf(stderr,"radioberry_protocol: buffer size: =%d\n", buffer_size);

#ifndef GPIO 
	if (gpioInitialise() < 0) {
		fprintf(stderr,"radioberry_protocol: gpio could not be initialized. \n");
		exit(-1);
	}
#endif  
 
	gpioSetMode(13, PI_INPUT); 
	gpioSetMode(20, PI_INPUT); 
	gpioSetMode(21, PI_OUTPUT); 
   
	h = spiOpen(0, 15625000, 49155); 
	if (h < 0) {
		fprintf(stderr,"radioberry_protocol: spi bus could not be initialized. \n");
		exit(-1);
	}
	
	printf("init done \n");
  
	setSampleSpeed();

	if(transmitter->local_microphone) {
		if(audio_open_input()!=0) {
		  fprintf(stderr,"audio_open_input failed\n");
		  transmitter->local_microphone=0;
		}
	 }

	start_radioberry_thread();
}

static void start_radioberry_thread() {
  int rc;
  fprintf(stderr,"radioberry_protocol starting radioberry thread\n");
  rc=pthread_create(&radioberry_thread_id,NULL,radioberry_thread,NULL);
  if(rc != 0) {
    fprintf(stderr,"radioberry_protocol: pthread_create failed on radioberry_thread: rc=%d\n", rc);
    exit(-1);
  }
}

static void *radioberry_thread(void* arg) {
	fprintf(stderr, "radioberry_protocol: radioberry_thread\n");
 
	running=1;

	gettimeofday(&t20, 0);
	gettimeofday(&t0, 0);
	
	gpioSetMode(13, PI_INPUT); 
	gpioSetMode(20, PI_INPUT); 
	gpioSetMode(21, PI_OUTPUT); 
	
	while(running) {
	
		sem_wait(&mutex); 
		
		if (isTransmitting()) 
			radiostate = RADIOSTATE_TX;
		else
			radiostate = RADIOSTATE_RX;
		
		if(radiostate == RADIOSTATE_TX) {
			gpioWrite(21, 1); 
		}
		else 
		{
			gpioWrite(21, 0);
			spiReader();
			handleReceiveStream();
			sem_post(&mutex);
		}
	}
}

void radioberry_protocol_iq_samples(int isample,int qsample) {

	if(radiostate == RADIOSTATE_TX) {
	
		tx_iqdata[0] = 0;
		tx_iqdata[1] = drive / 6.4;  // convert drive level from 0-255 to 0-39 )
		if (prev_drive_level != drive) {
			printf("drive level %d - corrected drive level %d \n", drive_level, tx_iqdata[1]);
			prev_drive_level = drive; 
		}		
		tx_iqdata[2] = isample>>8; 
		tx_iqdata[3] = isample;
		tx_iqdata[4] = qsample>>8;
		tx_iqdata[5] = qsample;

		spiWriter();
		
		sem_post(&mutex);
	}

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

static void handleReceiveStream() {
	int left_sample;
	int right_sample;
	double left_sample_double;
	double right_sample_double;
	int r;
  
	left_sample   = (int)((signed char) iqdata[0]) << 16;
	left_sample  += (int)((unsigned char)iqdata[1]) << 8;
	left_sample  += (int)((unsigned char)iqdata[2]);
	right_sample  = (int)((signed char) iqdata[3]) << 16;
	right_sample += (int)((unsigned char)iqdata[4]) << 8;
	right_sample += (int)((unsigned char)iqdata[5]);
	
	left_sample_double=(double)left_sample/8388607.0; // 24 bit sample 2^23-1
    right_sample_double=(double)right_sample/8388607.0; // 24 bit sample 2^23-1
	
	for(r=0;r<RECEIVERS;r++) {
		add_iq_samples(receiver[r], left_sample_double,right_sample_double);
	}
}

void setSampleSpeed() {
     switch(active_receiver->sample_rate) {
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

	if (h !=0)
		spiClose(h);
		
#ifndef GPIO 
	gpioTerminate();
#endif
	
}

void spiReader() {
	// wait till rxFIFO buffer is filled with at least one element
	while ( gpioRead(13) == 1) {};
	
	setSampleSpeed();
	
	int v=receiver[0]->id;
	long long rxFrequency=vfo[v].frequency-vfo[v].lo;
	if(vfo[v].rit_enabled) {
	  rxFrequency+=vfo[v].rit;
	}
	if(vfo[active_receiver->id].mode==modeCWU) {
	  rxFrequency-=(long long)cw_keyer_sidetone_frequency;
	} else if(vfo[active_receiver->id].mode==modeCWL) {
	  rxFrequency+=(long long)cw_keyer_sidetone_frequency;
	}
	
	iqdata[0] = (sampleSpeed & 0x03);
	iqdata[1] = (((active_receiver->random << 6) & 0x40) | ((active_receiver->dither <<5) & 0x20) |  (attenuation & 0x1F));
	iqdata[2] = ((rxFrequency >> 24) & 0xFF);
	iqdata[3] = ((rxFrequency >> 16) & 0xFF);
	iqdata[4] = ((rxFrequency >> 8) & 0xFF);
	iqdata[5] = (rxFrequency & 0xFF);
			
	spiXfer(h, iqdata, iqdata, 6);
	
	//firmware: tdata(56'h00010203040506) -> 0-1-2-3-4-5-6 (element 0 contains 0; second element contains 1)
	rxcount ++;
	if (rxcount == 48000) {
		rxcount = 0;
		gettimeofday(&t1, 0);
		elapsed = timedifference_msec(t0, t1);
		printf("Code rx mode spi executed in %f milliseconds.\n", elapsed);
		gettimeofday(&t0, 0);
	}
}

void spiWriter() {
	while ( gpioRead(20) == 1) {};

	spiXfer(h, tx_iqdata, tx_iqdata, 6);
	
	txcount ++;
	if (txcount == 48000) {
		txcount = 0;
		gettimeofday(&t21, 0);
		float elapsd = timedifference_msec(t20, t21);
		printf("Code tx mode spi executed in %f milliseconds.\n", elapsd);
		gettimeofday(&t20, 0);
	}
}

