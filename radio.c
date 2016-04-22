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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <math.h>

#include "radio.h"
#include "channel.h"
#include "agc.h"
#include "band.h"
#include "discovered.h"
#include "property.h"
#include "new_protocol.h"
#include "wdsp.h"

#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

char property_path[128];
sem_t property_sem;


int atlas_penelope=0;
int atlas_clock_source_10mhz=0;
int atlas_clock_source_128mhz=0;
int atlas_config=0;
int atlas_mic_source=0;

int classE=0;

int tx_out_of_band=0;

int sample_rate=48000;
int filter_board=ALEX;
int pa=PA_ENABLED;
int apollo_tuner=0;

int updates_per_second=10;

int display_panadapter=1;
int panadapter_high=-60;
int panadapter_low=-160;

int display_filled=1;
int display_detector_mode=DETECTOR_MODE_AVERAGE;
int display_average_mode=AVERAGE_MODE_LOG_RECURSIVE;
double display_average_time=120.0;


int display_waterfall=1;
int waterfall_high=-100;
int waterfall_low=-150;
int waterfall_automatic=1;

int display_sliders=1;
int display_toolbar=1;
int toolbar_simulate_buttons=1;

double volume=0.2;
double mic_gain=1.5;

int rx_dither=0;
int rx_random=0;
int rx_preamp=0;

int mic_linein=0;
int mic_boost=0;
int mic_bias_enabled=0;
int mic_ptt_enabled=0;
int mic_ptt_tip_bias_ring=0;

int agc=AGC_MEDIUM;
double agc_gain=60.0;

int nr_none=1;
int nr=0;
int nr2=0;
int nb=0;
int nb2=0;
int anf=0;
int snb=0;

int nr_agc=0; // 0=pre AGC 1=post AGC
int nr2_gain_method=2; // 0=Linear 1=Log 2=gamma
int nr2_npe_method=0; // 0=OSMS 1=MMSE
int nr2_ae=1; // 0=disable 1=enable

int cwPitch=600;

int tune_drive=6;
int drive=60;

int receivers=2;
int adc[2]={0,1};

int locked=0;

int step=100;

int lt2208Dither = 0;
int lt2208Random = 0;
int attenuation = 0; // 0dB
//unsigned long alex_rx_antenna=0;
//unsigned long alex_tx_antenna=0;
//unsigned long alex_attenuation=0;

int cw_keys_reversed=0; // 0=disabled 1=enabled
int cw_keyer_speed=12; // 1-60 WPM
int cw_keyer_mode=KEYER_STRAIGHT;
int cw_keyer_weight=30; // 0-100
int cw_keyer_spacing=0; // 0=on 1=off
int cw_keyer_internal=1; // 0=external 1=internal
int cw_keyer_sidetone_volume=127; // 0-127
int cw_keyer_ptt_delay=20; // 0-255ms
int cw_keyer_hang_time=300; // ms
int cw_keyer_sidetone_frequency=400; // Hz
int cw_breakin=1; // 0=disabled 1=enabled

int vfo_encoder_divisor=25;

int protocol;
int device;
int ozy_software_version;
int mercury_software_version;
int penelope_software_version;
int ptt;
int dot;
int dash;
int adc_overload;
int pll_locked;
unsigned int exciter_power;
unsigned int alex_forward_power;
unsigned int alex_reverse_power;
unsigned int AIN3;
unsigned int AIN4;
unsigned int AIN6;
unsigned int IO1;
unsigned int IO2;
unsigned int IO3;
int supply_volts;
int mox;
int tune;

long long ddsFrequency=14250000;

unsigned char OCtune=0;
int OCfull_tune_time=2800; // ms
int OCmemory_tune_time=550; // ms
long long tune_timeout;

void init_radio() {
  int rc;
  rc=sem_init(&property_sem, 0, 0);
  if(rc!=0) {
    fprintf(stderr,"init_radio: sem_init failed for property_sem: %d\n", rc);
    exit(-1);
  }
  sem_post(&property_sem);
}

void setSampleRate(int rate) {
    sample_rate=rate;
}

int getSampleRate() {
    return sample_rate;
}

void setMox(int state) {
fprintf(stderr,"setMox: protocol=%d\n", protocol);
  if(mox!=state) {
    mox=state;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority(3);
    }
  }
}

int getMox() {
    return mox;
}

void setTune(int state) {
fprintf(stderr,"setTune: protocol=%d\n", protocol);
  if(tune!=state) {
    tune=state;
    if(tune) {
      if(OCmemory_tune_time!=0) {
        struct timeval te;
        gettimeofday(&te,NULL);
        tune_timeout=(te.tv_sec*1000LL+te.tv_usec/1000)+(long long)OCmemory_tune_time;
      }
    }
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority(4);
      schedule_general();
    }
  }
}

int getTune() {
  return tune;
}

int isTransmitting() {
  return ptt!=0 || mox!=0 || tune!=0;
}

void setFrequency(long long f) {
//fprintf(stderr,"setFrequency: protocol=%d f=%lld\n", protocol, f);
  ddsFrequency=f;
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority(5);
  } else {
    schedule_frequency_changed();
  }
}

long long getFrequency() {
    return ddsFrequency;
}

double getDrive() {
    return (double)drive/255.0;
}

void setDrive(double value) {
//fprintf(stderr,"setDrive: protocol=%d\n", protocol);
    drive=(int)(value*255.0);
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority(6);
    }
}

double getTuneDrive() {
    return (double)tune_drive/255.0;
}

void setTuneDrive(double value) {
fprintf(stderr,"setTuneDrive: protocol=%d\n", protocol);
    tune_drive=(int)(value*255.0);
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority(7);
    }
}

void set_attenuation(int value) {
    //attenuation=value;
    if(protocol==NEW_PROTOCOL) {
        schedule_high_priority(8);
    }
}

int get_attenuation() {
    return attenuation;
}

void set_alex_rx_antenna(int v) {
    //alex_rx_antenna=v;
    if(protocol==NEW_PROTOCOL) {
        schedule_high_priority(1);
    }
}

void set_alex_tx_antenna(int v) {
    //alex_tx_antenna=v;
    if(protocol==NEW_PROTOCOL) {
        schedule_high_priority(2);
    }
}

void set_alex_attenuation(int v) {
    //alex_attenuation=v;
    if(protocol==NEW_PROTOCOL) {
        schedule_high_priority(0);
    }
}

void radioRestoreState() {
    char *value;

    sem_wait(&property_sem);
    loadProperties(property_path);

    value=getProperty("atlas_penelope");
    if(value) atlas_penelope=atoi(value);
    value=getProperty("tx_out_of_band");
    if(value) tx_out_of_band=atoi(value);
    value=getProperty("sample_rate");
    if(value) sample_rate=atoi(value);
    value=getProperty("filter_board");
    if(value) filter_board=atoi(value);
    value=getProperty("apollo_tuner");
    if(value) apollo_tuner=atoi(value);
    value=getProperty("pa");
    if(value) pa=atoi(value);
    value=getProperty("updates_per_second");
    if(value) updates_per_second=atoi(value);
    value=getProperty("display_panadapter");
    if(value) display_panadapter=atoi(value);
    value=getProperty("display_detector_mode");
    if(value) display_detector_mode=atoi(value);
    value=getProperty("display_average_mode");
    if(value) display_average_mode=atoi(value);
    value=getProperty("display_average_time");
    if(value) display_average_time=atof(value);
    value=getProperty("panadapter_high");
    if(value) panadapter_high=atoi(value);
    value=getProperty("panadapter_low");
    if(value) panadapter_low=atoi(value);
    value=getProperty("display_waterfall");
    if(value) display_waterfall=atoi(value);
    value=getProperty("display_sliders");
    if(value) display_sliders=atoi(value);
    value=getProperty("display_toolbar");
    if(value) display_toolbar=atoi(value);
    value=getProperty("toolbar_simulate_buttons");
    if(value) toolbar_simulate_buttons=atoi(value);
    value=getProperty("waterfall_high");
    if(value) waterfall_high=atoi(value);
    value=getProperty("waterfall_low");
    if(value) waterfall_low=atoi(value);
    value=getProperty("waterfall_automatic");
    if(value) waterfall_automatic=atoi(value);
    value=getProperty("volume");
    if(value) volume=atof(value);
    value=getProperty("drive");
    if(value) drive=atoi(value);
    value=getProperty("tune_drive");
    if(value) tune_drive=atoi(value);
    value=getProperty("mic_gain");
    if(value) mic_gain=atof(value);
    value=getProperty("mic_boost");
    if(value) mic_boost=atof(value);
    value=getProperty("mic_linein");
    if(value) mic_linein=atof(value);
    value=getProperty("mic_ptt_enabled");
    if(value) mic_ptt_enabled=atof(value);
    value=getProperty("mic_bias_enabled");
    if(value) mic_bias_enabled=atof(value);
    value=getProperty("mic_ptt_tip_bias_ring");
    if(value) mic_ptt_tip_bias_ring=atof(value);
    value=getProperty("nr_none");
    if(value) nr_none=atoi(value);
    value=getProperty("nr");
    if(value) nr=atoi(value);
    value=getProperty("nr2");
    if(value) nr2=atoi(value);
    value=getProperty("nb");
    if(value) nb=atoi(value);
    value=getProperty("nb2");
    if(value) nb2=atoi(value);
    value=getProperty("anf");
    if(value) anf=atoi(value);
    value=getProperty("snb");
    if(value) snb=atoi(value);
    value=getProperty("nr_agc");
    if(value) nr_agc=atoi(value);
    value=getProperty("nr2_gain_method");
    if(value) nr2_gain_method=atoi(value);
    value=getProperty("nr2_npe_method");
    if(value) nr2_npe_method=atoi(value);
    value=getProperty("nr2_ae");
    if(value) nr2_ae=atoi(value);
    value=getProperty("agc");
    if(value) agc=atoi(value);
    value=getProperty("agc_gain");
    if(value) agc_gain=atof(value);
    value=getProperty("step");
    if(value) step=atoi(value);
    value=getProperty("cw_keys_reversed");
    if(value) cw_keys_reversed=atoi(value);
    value=getProperty("cw_keyer_speed");
    if(value) cw_keyer_speed=atoi(value);
    value=getProperty("cw_keyer_mode");
    if(value) cw_keyer_mode=atoi(value);
    value=getProperty("cw_keyer_weight");
    if(value) cw_keyer_weight=atoi(value);
    value=getProperty("cw_keyer_spacing");
    if(value) cw_keyer_spacing=atoi(value);
    value=getProperty("cw_keyer_internal");
    if(value) cw_keyer_internal=atoi(value);
    value=getProperty("cw_keyer_sidetone_volume");
    if(value) cw_keyer_sidetone_volume=atoi(value);
    value=getProperty("cw_keyer_ptt_delay");
    if(value) cw_keyer_ptt_delay=atoi(value);
    value=getProperty("cw_keyer_hang_time");
    if(value) cw_keyer_hang_time=atoi(value);
    value=getProperty("cw_keyer_sidetone_frequency");
    if(value) cw_keyer_sidetone_frequency=atoi(value);
    value=getProperty("cw_breakin");
    if(value) cw_breakin=atoi(value);
    value=getProperty("vfo_encoder_divisor");
    if(value) vfo_encoder_divisor=atoi(value);
    value=getProperty("OCtune");
    if(value) OCtune=atoi(value);
    value=getProperty("OCfull_tune_time");
    if(value) OCfull_tune_time=atoi(value);
    value=getProperty("OCmemory_tune_time");
    if(value) OCmemory_tune_time=atoi(value);

    bandRestoreState();
    sem_post(&property_sem);
}

void radioSaveState() {
    char value[80];

    sem_wait(&property_sem);
    sprintf(value,"%d",atlas_penelope);
    setProperty("atlas_penelope",value);
    sprintf(value,"%d",sample_rate);
    setProperty("sample_rate",value);
    sprintf(value,"%d",filter_board);
    setProperty("filter_board",value);
    sprintf(value,"%d",apollo_tuner);
    setProperty("apollo_tuner",value);
    sprintf(value,"%d",pa);
    setProperty("pa",value);
    sprintf(value,"%d",updates_per_second);
    setProperty("updates_per_second",value);
    sprintf(value,"%d",display_panadapter);
    setProperty("display_panadapter",value);
    sprintf(value,"%d",display_detector_mode);
    setProperty("display_detector_mode",value);
    sprintf(value,"%d",display_average_mode);
    setProperty("display_average_mode",value);
    sprintf(value,"%f",display_average_time);
    setProperty("display_average_time",value);
    sprintf(value,"%d",panadapter_high);
    setProperty("panadapter_high",value);
    sprintf(value,"%d",panadapter_low);
    setProperty("panadapter_low",value);
    sprintf(value,"%d",display_waterfall);
    setProperty("display_waterfall",value);
    sprintf(value,"%d",display_sliders);
    setProperty("display_sliders",value);
    sprintf(value,"%d",display_toolbar);
    setProperty("display_toolbar",value);
    sprintf(value,"%d",toolbar_simulate_buttons);
    setProperty("toolbar_simulate_buttons",value);
    sprintf(value,"%d",waterfall_high);
    setProperty("waterfall_high",value);
    sprintf(value,"%d",waterfall_low);
    setProperty("waterfall_low",value);
    sprintf(value,"%d",waterfall_automatic);
    setProperty("waterfall_automatic",value);
    sprintf(value,"%f",volume);
    setProperty("volume",value);
    sprintf(value,"%f",mic_gain);
    setProperty("mic_gain",value);
    sprintf(value,"%d",drive);
    setProperty("drive",value);
    sprintf(value,"%d",tune_drive);
    setProperty("tune_drive",value);
    sprintf(value,"%d",mic_boost);
    setProperty("mic_boost",value);
    sprintf(value,"%d",mic_linein);
    setProperty("mic_linein",value);
    sprintf(value,"%d",mic_ptt_enabled);
    setProperty("mic_ptt_enabled",value);
    sprintf(value,"%d",mic_bias_enabled);
    setProperty("mic_bias_enabled",value);
    sprintf(value,"%d",mic_ptt_tip_bias_ring);
    setProperty("mic_ptt_tip_bias_ring",value);
    sprintf(value,"%d",nr_none);
    setProperty("nr_none",value);
    sprintf(value,"%d",nr);
    setProperty("nr",value);
    sprintf(value,"%d",nr2);
    setProperty("nr2",value);
    sprintf(value,"%d",nb);
    setProperty("nb",value);
    sprintf(value,"%d",nb2);
    setProperty("nb2",value);
    sprintf(value,"%d",anf);
    setProperty("anf",value);
    sprintf(value,"%d",snb);
    setProperty("snb",value);
    sprintf(value,"%d",nr_agc);
    setProperty("nr_agc",value);
    sprintf(value,"%d",nr2_gain_method);
    setProperty("nr2_gain_method",value);
    sprintf(value,"%d",nr2_npe_method);
    setProperty("nr2_npe_method",value);
    sprintf(value,"%d",nr2_ae);
    setProperty("nr2_ae",value);
    sprintf(value,"%d",agc);
    setProperty("agc",value);
    sprintf(value,"%f",agc_gain);
    setProperty("agc_gain",value);
    sprintf(value,"%d",step);
    setProperty("step",value);
    sprintf(value,"%d",cw_keys_reversed);
    setProperty("cw_keys_reversed",value);
    sprintf(value,"%d",cw_keyer_speed);
    setProperty("cw_keyer_speed",value);
    sprintf(value,"%d",cw_keyer_mode);
    setProperty("cw_keyer_mode",value);
    sprintf(value,"%d",cw_keyer_weight);
    setProperty("cw_keyer_weight",value);
    sprintf(value,"%d",cw_keyer_spacing);
    setProperty("cw_keyer_spacing",value);
    sprintf(value,"%d",cw_keyer_internal);
    setProperty("cw_keyer_internal",value);
    sprintf(value,"%d",cw_keyer_sidetone_volume);
    setProperty("cw_keyer_sidetone_volume",value);
    sprintf(value,"%d",cw_keyer_ptt_delay);
    setProperty("cw_keyer_ptt_delay",value);
    sprintf(value,"%d",cw_keyer_hang_time);
    setProperty("cw_keyer_hang_time",value);
    sprintf(value,"%d",cw_keyer_sidetone_frequency);
    setProperty("cw_keyer_sidetone_frequency",value);
    sprintf(value,"%d",cw_breakin);
    setProperty("cw_breakin",value);
    sprintf(value,"%d",vfo_encoder_divisor);
    setProperty("vfo_encoder_divisor",value);
    sprintf(value,"%d",OCtune);
    setProperty("OCtune",value);
    sprintf(value,"%d",OCfull_tune_time);
    setProperty("OCfull_tune_time",value);
    sprintf(value,"%d",OCmemory_tune_time);
    setProperty("OCmemory_tune_time",value);

    bandSaveState();

    saveProperties(property_path);
    sem_post(&property_sem);
}

void calculate_display_average() {
  double display_avb;
  int display_average;

  double t=0.001*display_average_time;
  display_avb = exp(-1.0 / ((double)updates_per_second * t));
  display_average = max(2, (int)min(60, (double)updates_per_second * t));
  SetDisplayAvBackmult(CHANNEL_RX0, 0, display_avb);
  SetDisplayNumAverage(CHANNEL_RX0, 0, display_average);
}
