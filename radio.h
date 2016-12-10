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

#ifndef _RADIO_H
#define _RADIO_H

#include "discovered.h"

#define NEW_MIC_IN 0x00
#define NEW_LINE_IN 0x01
#define NEW_MIC_BOOST 0x02
#define NEW_ORION_MIC_PTT_ENABLED 0x00
#define NEW_ORION_MIC_PTT_DISABLED 0x04
#define NEW_ORION_MIC_PTT_RING_BIAS_TIP 0x00
#define NEW_ORION_MIC_PTT_TIP_BIAS_RING 0x08
#define NEW_ORION_MIC_BIAS_DISABLED 0x00
#define NEW_ORION_MIC_BIAS_ENABLED 0x10

#define OLD_MIC_IN 0x00
#define OLD_LINE_IN 0x02
#define OLD_MIC_BOOST 0x01
#define OLD_ORION_MIC_PTT_ENABLED 0x40
#define OLD_ORION_MIC_PTT_DISABLED 0x00
#define OLD_ORION_MIC_PTT_RING_BIAS_TIP 0x00
#define OLD_ORION_MIC_PTT_TIP_BIAS_RING 0x08
#define OLD_ORION_MIC_BIAS_DISABLED 0x00
#define OLD_ORION_MIC_BIAS_ENABLED 0x20

extern DISCOVERED *radio;

extern char property_path[];

#define NONE 0

#define ALEX 1
#define APOLLO 2

// soecify how many receivers (only 1 or 2 for now)
#define RECEIVERS 2

/*
#define PA_DISABLED 0
#define PA_ENABLED 1

#define APOLLO_TUNER 1
*/
#define KEYER_STRAIGHT 0
#define KEYER_MODE_A 1
#define KEYER_MODE_B 2

extern int rx_dither;
extern int rx_random;
extern int rx_preamp;

extern int atlas_penelope;
extern int atlas_clock_source_10mhz;
extern int atlas_clock_source_128mhz;
extern int atlas_config;
extern int atlas_mic_source;

extern int classE;

extern int tx_out_of_band;

extern int tx_cfir;
extern int tx_alc;
extern int tx_leveler;

extern double tone_level;

extern int sample_rate;
extern int filter_board;
extern int pa;
extern int apollo_tuner;

extern int updates_per_second;
extern int display_panadapter;
extern int panadapter_high;
extern int panadapter_low;

extern int display_filled;
extern int display_detector_mode;
extern int display_average_mode;
extern double display_average_time;


extern int display_waterfall;
extern int waterfall_high;
extern int waterfall_low;
extern int waterfall_automatic;

extern int display_sliders;
extern int display_toolbar;
extern int toolbar_dialog_buttons;

extern double volume;
extern double mic_gain;
extern int binaural;
extern int agc;
extern double agc_gain;
extern double agc_slope;
extern double agc_hang_threshold;

extern int nr_none;
extern int nr;
extern int nr2;
extern int nb;
extern int nb2;
extern int anf;
extern int snb;

extern int nr_agc;
extern int nr2_gain_method;
extern int nr2_npe_method;
extern int nr2_ae;

extern int mic_linein;
extern int mic_boost;
extern int mic_bias_enabled;
extern int mic_ptt_enabled;
extern int mic_ptt_tip_bias_ring;

extern double tune_drive;
extern double drive;

extern int tune_drive_level;
extern int drive_level;

int receivers;
int active_receiver;

int adc[2];

int locked;

extern int step;
extern int rit;
extern int rit_increment;

extern int lt2208Dither;
extern int lt2208Random;
extern int attenuation;
extern unsigned long alex_rx_antenna;
extern unsigned long alex_tx_antenna;
extern unsigned long alex_attenuation;

extern int cw_keys_reversed;
extern int cw_keyer_speed;
extern int cw_keyer_mode;
extern int cw_keyer_weight;
extern int cw_keyer_spacing;
extern int cw_keyer_internal;
extern int cw_keyer_sidetone_volume;
extern int cw_keyer_ptt_delay;
extern int cw_keyer_hang_time;
extern int cw_keyer_sidetone_frequency;
extern int cw_breakin;
extern int cw_active_level;

extern int vfo_encoder_divisor;

extern int protocol;
extern int device;
extern int ozy_software_version;
extern int mercury_software_version;
extern int penelope_software_version;
extern int mox;
extern int tune;
extern int ptt;
extern int dot;
extern int dash;
extern int adc_overload;
extern int pll_locked;
extern unsigned int exciter_power;
extern unsigned int alex_forward_power;
extern unsigned int alex_reverse_power;
extern unsigned int IO1;
extern unsigned int IO2;
extern unsigned int IO3;
extern unsigned int AIN3;
extern unsigned int AIN4;
extern unsigned int AIN6;
extern int supply_volts;

extern long long displayFrequency;
extern long long ddsFrequency;
extern long long ddsOffset;

extern unsigned char OCtune;
extern int OCfull_tune_time;
extern int OCmemory_tune_time;
extern long long tune_timeout;

#ifdef FREEDV
extern char freedv_tx_text_data[64];
#endif

extern int smeter;
extern int alc;

extern int local_audio;
extern int local_microphone;

extern int eer_pwm_min;
extern int eer_pwm_max;

extern int tx_filter_low;
extern int tx_filter_high;

extern int ctun;

extern int enable_tx_equalizer;
extern int tx_equalizer[4];

extern int enable_rx_equalizer;
extern int rx_equalizer[4];

extern int deviation;
extern int pre_emphasize;

extern int vox_enabled;
extern double vox_threshold;
extern double vox_gain;
extern double vox_hang;
extern int vox;

extern int diversity_enabled;
extern double i_rotate[2];
extern double q_rotate[2];

extern void init_radio();
extern void setSampleRate(int rate);
extern int getSampleRate();
extern void setMox(int state);
extern int getMox();
extern void setTune(int state);
extern int getTune();
extern double getDrive();
extern void setDrive(double d);
extern void calcDriveLevel();
extern double getTuneDrive();
extern void setTuneDrive(double d);
extern void calcTuneDriveLevel();

extern void set_attenuation(int value);
extern int get_attenuation();
extern void set_alex_rx_antenna(int v);
extern void set_alex_tx_antenna(int v);
extern void set_alex_attenuation(int v);

extern int isTransmitting();

extern void setFrequency(long long f);
extern long long getFrequency();

extern void radioRestoreState();
extern void radioSaveState();

extern void calculate_display_average();

#endif
