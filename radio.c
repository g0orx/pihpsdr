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

#include "audio.h"
#include "discovered.h"
//#include "discovery.h"
#include "mode.h"
#include "radio.h"
#include "channel.h"
#include "agc.h"
#include "band.h"
#include "property.h"
#include "new_protocol.h"
#ifdef LIMESDR
#include "lime_protocol.h"
#endif
#include "wdsp.h"
#ifdef FREEDV
#include "freedv.h"
#endif

#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

DISCOVERED *radio;

char property_path[128];
sem_t property_sem;

int atlas_penelope=0;
int atlas_clock_source_10mhz=0;
int atlas_clock_source_128mhz=0;
int atlas_config=0;
int atlas_mic_source=0;

int classE=0;

int tx_out_of_band=0;

int tx_cfir=0;
int tx_alc=1;
int tx_leveler=0;

double tone_level=0.0;

int sample_rate=48000;
int filter_board=ALEX;
//int pa=PA_ENABLED;
//int apollo_tuner=0;

int updates_per_second=10;

int display_panadapter=1;
int panadapter_high=-40;
int panadapter_low=-140;

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
int toolbar_dialog_buttons=1;

double volume=0.2;
double mic_gain=0.0;
int binaural=0;

int rx_dither=0;
int rx_random=0;
int rx_preamp=0;

int mic_linein=0;
int mic_boost=0;
int mic_bias_enabled=0;
int mic_ptt_enabled=0;
int mic_ptt_tip_bias_ring=0;

int agc=AGC_MEDIUM;
double agc_gain=80.0;
double agc_slope=35.0;
double agc_hang_threshold=0.0;

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

double tune_drive=10;
double drive=50;

int drive_level=0;
int tune_drive_level=0;

int receivers=RECEIVERS;
int active_receiver=0;

int adc[2]={0,1};

int locked=0;

int step=100;

int rit=0;
int rit_increment=10;

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

int vfo_encoder_divisor=15;

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

long long displayFrequency=14250000;
long long ddsFrequency=14250000;
long long ddsOffset=0;

unsigned char OCtune=0;
int OCfull_tune_time=2800; // ms
int OCmemory_tune_time=550; // ms
long long tune_timeout;

int smeter=RXA_S_AV;
int alc=TXA_ALC_PK;

int local_audio=0;
int local_microphone=0;

int eer_pwm_min=100;
int eer_pwm_max=800;

int tx_filter_low=200;
int tx_filter_high=3100;

#ifdef FREEDV
char freedv_tx_text_data[64];
#endif

static int pre_tune_mode;

int ctun=0;

int enable_tx_equalizer=0;
int tx_equalizer[4]={0,0,0,0};

int enable_rx_equalizer=0;
int rx_equalizer[4]={0,0,0,0};

int deviation=2500;
int pre_emphasize=0;

int vox_enabled=0;
double vox_threshold=0.001;
double vox_gain=10.0;
double vox_hang=250.0;
int vox=0;

int diversity_enabled=0;
double i_rotate[2]={1.0,1.0};
double q_rotate[2]={0.0,0.0};

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

static void rxtx(int state) {
  if(state) {
    // switch to tx
    SetChannelState(CHANNEL_RX0,0,1);
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority(3);
    }
    SetChannelState(CHANNEL_TX,1,0);
#ifdef FREEDV
    if(mode==modeFREEDV) {
      freedv_reset_tx_text_index();
    }
#endif
  } else {
    SetChannelState(CHANNEL_TX,0,1);
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority(3);
    }
    SetChannelState(CHANNEL_RX0,1,0);
  }
}

void setMox(int state) {
  if(mox!=state) {
    mox=state;
    if(vox_enabled && vox) {
      vox_cancel();
    } else {
      rxtx(state);
    }
  }
}

int getMox() {
    return mox;
}

void setVox(int state) {
  if(vox!=state && !tune) {
    vox=state;
    rxtx(state);
  }
}

void setTune(int state) {
  if(tune!=state) {
    tune=state;
    if(vox_enabled && vox) {
      vox_cancel();
    }
    if(tune) {
      if(OCmemory_tune_time!=0) {
        struct timeval te;
        gettimeofday(&te,NULL);
        tune_timeout=(te.tv_sec*1000LL+te.tv_usec/1000)+(long long)OCmemory_tune_time;
      }
    }
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority(4);
      //schedule_general();
    }
    if(tune) {
      SetChannelState(CHANNEL_RX0,0,1);
      pre_tune_mode = mode;
      if(mode==modeCWL) {
        setMode(modeLSB);
      } else if(mode==modeCWU) {
        setMode(modeUSB);
      }
      SetTXAPostGenMode(CHANNEL_TX,0);
      if(mode==modeLSB || mode==modeCWL || mode==modeDIGL) {
        SetTXAPostGenToneFreq(CHANNEL_TX,-(double)cw_keyer_sidetone_frequency);
      } else {
        SetTXAPostGenToneFreq(CHANNEL_TX,(double)cw_keyer_sidetone_frequency);
      }
      SetTXAPostGenToneMag(CHANNEL_TX,0.99999);
      SetTXAPostGenRun(CHANNEL_TX,1);
      SetChannelState(CHANNEL_TX,1,0);
    } else {
      SetChannelState(CHANNEL_TX,0,1);
      SetTXAPostGenRun(CHANNEL_TX,0);
      if(pre_tune_mode==modeCWL || pre_tune_mode==modeCWU) {
        setMode(pre_tune_mode);
      }
      SetChannelState(CHANNEL_RX0,1,0);
    }
  }
}

int getTune() {
  return tune;
}

int isTransmitting() {
  return ptt || mox || vox || tune;
}

void setFrequency(long long f) {
  BAND *band=band_get_current_band();
  BANDSTACK_ENTRY* entry=bandstack_entry_get_current();

  switch(protocol) {
    case NEW_PROTOCOL:
    case ORIGINAL_PROTOCOL:
      if(ctun) {
        long long minf=entry->frequencyA-(long long)(sample_rate/2);
        long long maxf=entry->frequencyA+(long long)(sample_rate/2);
        if(f<minf) f=minf;
        if(f>maxf) f=maxf;
        ddsOffset=f-entry->frequencyA;
        wdsp_set_offset(ddsOffset);
        return;
      } else {
        entry->frequencyA=f;
      }
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      {
      long long minf=entry->frequencyA-(long long)(sample_rate/2);
      long long maxf=entry->frequencyA+(long long)(sample_rate/2);
      if(f<minf) f=minf;
      if(f>maxf) f=maxf;
      ddsOffset=f-entry->frequencyA;
      wdsp_set_offset(ddsOffset);
      return;
      }
      break;
#endif
  }

  displayFrequency=f;
  ddsFrequency=f;
  if(band->frequencyLO!=0LL) {
    ddsFrequency=f-band->frequencyLO;
  }
  switch(protocol) {
    case NEW_PROTOCOL:
      schedule_high_priority(5);
      break;
    case ORIGINAL_PROTOCOL:
      schedule_frequency_changed();
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_set_frequency(f);
      ddsOffset=0;
      wdsp_set_offset(ddsOffset);
      break;
#endif
  }
}

long long getFrequency() {
    return ddsFrequency;
}

double getDrive() {
    return drive;
}

static int calcLevel(double d) {
  int level=0;
  BAND *band=band_get_current_band();
  double target_dbm = 10.0 * log10(d * 1000.0);
  double gbb=band->pa_calibration;
  target_dbm-=gbb;
  double target_volts = sqrt(pow(10, target_dbm * 0.1) * 0.05);
  double volts=min((target_volts / 0.8), 1.0);
  double v=volts*(1.0/0.98);

  if(v<0.0) {
    v=0.0;
  } else if(v>1.0) {
    v=1.0;
  }

  level=(int)(v*255.0);
  return level;
}

void calcDriveLevel() {
    drive_level=calcLevel(drive);
    if(mox && protocol==NEW_PROTOCOL) {
      schedule_high_priority(6);
    }
}

void setDrive(double value) {
    drive=value;
    calcDriveLevel();
}

double getTuneDrive() {
    return tune_drive;
}

void calcTuneDriveLevel() {
    tune_drive_level=calcLevel(tune_drive);
    if(tune  && protocol==NEW_PROTOCOL) {
      schedule_high_priority(7);
    }
}

void setTuneDrive(double value) {
    tune_drive=value;
    calcTuneDriveLevel();
}

void set_attenuation(int value) {
    //attenuation=value;
    switch(protocol) {
      case NEW_PROTOCOL:
        schedule_high_priority(8);
        break;
#ifdef LIMESDR
      case LIMESDR_PROTOCOL:
        lime_protocol_set_attenuation(value);
        break;
#endif
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
#ifdef LIMESDR
    if(protocol==LIMESDR_PROTOCOL) {
        lime_protocol_set_antenna(v);;
    }
#endif
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

fprintf(stderr,"radioRestoreState: %s\n",property_path);
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
/*
    value=getProperty("apollo_tuner");
    if(value) apollo_tuner=atoi(value);
    value=getProperty("pa");
    if(value) pa=atoi(value);
*/
    value=getProperty("updates_per_second");
    if(value) updates_per_second=atoi(value);
    value=getProperty("display_panadapter");
    if(value) display_panadapter=atoi(value);
    value=getProperty("display_filled");
    if(value) display_filled=atoi(value);
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
    value=getProperty("toolbar_dialog_buttons");
    if(value) toolbar_dialog_buttons=atoi(value);
    value=getProperty("waterfall_high");
    if(value) waterfall_high=atoi(value);
    value=getProperty("waterfall_low");
    if(value) waterfall_low=atoi(value);
    value=getProperty("waterfall_automatic");
    if(value) waterfall_automatic=atoi(value);
    value=getProperty("volume");
    if(value) volume=atof(value);
    value=getProperty("drive");
    if(value) drive=atof(value);
    value=getProperty("tune_drive");
    if(value) tune_drive=atof(value);
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
    value=getProperty("agc_slope");
    if(value) agc_slope=atof(value);
    value=getProperty("agc_hang_threshold");
    if(value) agc_hang_threshold=atof(value);
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
    value=getProperty("attenuation");
    if(value) attenuation=atoi(value);
    value=getProperty("rx_dither");
    if(value) rx_dither=atoi(value);
    value=getProperty("rx_random");
    if(value) rx_random=atoi(value);
    value=getProperty("rx_preamp");
    if(value) rx_preamp=atoi(value);
#ifdef FREEDV
    strcpy(freedv_tx_text_data,"NO TEXT DATA");
    value=getProperty("freedv_tx_text_data");
    if(value) strcpy(freedv_tx_text_data,value);
#endif
    value=getProperty("smeter");
    if(value) smeter=atoi(value);
    value=getProperty("alc");
    if(value) alc=atoi(value);
    value=getProperty("local_audio");
    if(value) local_audio=atoi(value);
    value=getProperty("n_selected_output_device");
    if(value) n_selected_output_device=atoi(value);
    value=getProperty("local_microphone");
    if(value) local_microphone=atoi(value);
    value=getProperty("n_selected_input_device");
    if(value) n_selected_input_device=atoi(value);
    value=getProperty("enable_tx_equalizer");
    if(value) enable_tx_equalizer=atoi(value);
    value=getProperty("tx_equalizer.0");
    if(value) tx_equalizer[0]=atoi(value);
    value=getProperty("tx_equalizer.1");
    if(value) tx_equalizer[1]=atoi(value);
    value=getProperty("tx_equalizer.2");
    if(value) tx_equalizer[2]=atoi(value);
    value=getProperty("tx_equalizer.3");
    if(value) tx_equalizer[3]=atoi(value);
    value=getProperty("enable_rx_equalizer");
    if(value) enable_rx_equalizer=atoi(value);
    value=getProperty("rx_equalizer.0");
    if(value) rx_equalizer[0]=atoi(value);
    value=getProperty("rx_equalizer.1");
    if(value) rx_equalizer[1]=atoi(value);
    value=getProperty("rx_equalizer.2");
    if(value) rx_equalizer[2]=atoi(value);
    value=getProperty("rx_equalizer.3");
    if(value) rx_equalizer[3]=atoi(value);
    value=getProperty("rit_increment");
    if(value) rit_increment=atoi(value);
    value=getProperty("deviation");
    if(value) deviation=atoi(value);
    value=getProperty("pre_emphasize");
    if(value) pre_emphasize=atoi(value);

    value=getProperty("vox_enabled");
    if(value) vox_enabled=atoi(value);
    value=getProperty("vox_threshold");
    if(value) vox_threshold=atof(value);
/*
    value=getProperty("vox_gain");
    if(value) vox_gain=atof(value);
*/
    value=getProperty("vox_hang");
    if(value) vox_hang=atof(value);

    value=getProperty("binaural");
    if(value) binaural=atoi(value);

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
/*
    sprintf(value,"%d",apollo_tuner);
    setProperty("apollo_tuner",value);
    sprintf(value,"%d",pa);
    setProperty("pa",value);
*/
    sprintf(value,"%d",updates_per_second);
    setProperty("updates_per_second",value);
    sprintf(value,"%d",display_panadapter);
    setProperty("display_panadapter",value);
    sprintf(value,"%d",display_filled);
    setProperty("display_filled",value);
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
    sprintf(value,"%d",toolbar_dialog_buttons);
    setProperty("toolbar_dialog_buttons",value);
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
    sprintf(value,"%f",drive);
    setProperty("drive",value);
    sprintf(value,"%f",tune_drive);
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
    sprintf(value,"%f",agc_slope);
    setProperty("agc_slope",value);
    sprintf(value,"%f",agc_hang_threshold);
    setProperty("agc_hang_threshold",value);
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
    sprintf(value,"%d",attenuation);
    setProperty("attenuation",value);
    sprintf(value,"%d",rx_dither);
    setProperty("rx_dither",value);
    sprintf(value,"%d",rx_random);
    setProperty("rx_random",value);
    sprintf(value,"%d",rx_preamp);
    setProperty("rx_preamp",value);
#ifdef FREEDV
    if(strlen(freedv_tx_text_data)>0) {
      setProperty("freedv_tx_text_data",freedv_tx_text_data);
    }
#endif
    sprintf(value,"%d",smeter);
    setProperty("smeter",value);
    sprintf(value,"%d",alc);
    setProperty("alc",value);
    sprintf(value,"%d",local_audio);
    setProperty("local_audio",value);
    sprintf(value,"%d",n_selected_output_device);
    setProperty("n_selected_output_device",value);
    sprintf(value,"%d",local_microphone);
    setProperty("local_microphone",value);
    sprintf(value,"%d",n_selected_input_device);
    setProperty("n_selected_input_device",value);

    sprintf(value,"%d",enable_tx_equalizer);
    setProperty("enable_tx_equalizer",value);
    sprintf(value,"%d",tx_equalizer[0]);
    setProperty("tx_equalizer.0",value);
    sprintf(value,"%d",tx_equalizer[1]);
    setProperty("tx_equalizer.1",value);
    sprintf(value,"%d",tx_equalizer[2]);
    setProperty("tx_equalizer.2",value);
    sprintf(value,"%d",tx_equalizer[3]);
    setProperty("tx_equalizer.3",value);
    sprintf(value,"%d",enable_rx_equalizer);
    setProperty("enable_rx_equalizer",value);
    sprintf(value,"%d",rx_equalizer[0]);
    setProperty("rx_equalizer.0",value);
    sprintf(value,"%d",rx_equalizer[1]);
    setProperty("rx_equalizer.1",value);
    sprintf(value,"%d",rx_equalizer[2]);
    setProperty("rx_equalizer.2",value);
    sprintf(value,"%d",rx_equalizer[3]);
    setProperty("rx_equalizer.3",value);
    sprintf(value,"%d",rit_increment);
    setProperty("rit_increment",value);
    sprintf(value,"%d",deviation);
    setProperty("deviation",value);
    sprintf(value,"%d",pre_emphasize);
    setProperty("pre_emphasize",value);

    sprintf(value,"%d",vox_enabled);
    setProperty("vox_enabled",value);
    sprintf(value,"%f",vox_threshold);
    setProperty("vox_threshold",value);
/*
    sprintf(value,"%f",vox_gain);
    setProperty("vox_gain",value);
*/
    sprintf(value,"%f",vox_hang);
    setProperty("vox_hang",value);

    sprintf(value,"%d",binaural);
    setProperty("binaural",value);

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
