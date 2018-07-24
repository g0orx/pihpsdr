/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
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

#ifndef _TRANSMITTER_H
#define _TRANSMITTER_H

#include <gtk/gtk.h>

#define AUDIO_BUFFER_SIZE 260

typedef struct _transmitter {
  int id;
  int fps;
  int displaying;
  int mic_sample_rate;
  int mic_dsp_rate;
  int iq_output_rate;
  int buffer_size;
  int fft_size;
  int pixels;
  int samples;
  int output_samples;
  double *mic_input_buffer;
  double *iq_output_buffer;

  float *pixel_samples;
  int display_panadapter;
  int display_waterfall;
  gint update_timer_id;

  int mode;
  int filter_low;
  int filter_high;

/*
  long long frequency;
  long long display_frequency;
  long long dds_frequency;
  long long dds_offset;
*/
  int alex_antenna;

  int width;
  int height;

  GtkWidget *panel;
  GtkWidget *panadapter;

  int panadapter_low;
  int panadapter_high;

  cairo_surface_t *panadapter_surface;

  int local_microphone;
  int input_device;

  int out_of_band;
  gint out_of_band_timer_id;

  int low_latency;

// want to have a two-tone generator in any case
  int twotone;
#ifdef PURESIGNAL
  int puresignal;
  int feedback;
  int auto_on;
  int single_on;
#endif

  double ctcss_frequency;
  int ctcss;

  int deviation;

  double am_carrier_level;

  int attenuation;

  int drive;
  int tune_use_drive;
  int tune_percent;

  int drive_level;
 
#ifdef FREEDV
  char freedv_text_data[64];
  int freedv_text_index;
  int freedv_samples;
#endif

  int compressor;
  double compressor_level;

  double fwd;
  double exciter;
  double rev;
  double alc;

  int x;
  int y;

} TRANSMITTER;

extern TRANSMITTER *create_transmitter(int id, int buffer_size, int fft_size, int fps, int width, int height);

void reconfigure_transmitter(TRANSMITTER *tx,int height);


extern void tx_set_mode(TRANSMITTER* tx,int m);
extern void tx_set_filter(TRANSMITTER *tx,int low,int high);
extern void transmitter_set_deviation(TRANSMITTER *tx);
extern void transmitter_set_am_carrier_level(TRANSMITTER *tx);
extern void tx_set_pre_emphasize(TRANSMITTER *tx,int state);
extern void transmitter_set_ctcss(TRANSMITTER *tx,int run,double frequency);

extern void add_mic_sample(TRANSMITTER *tx,short mic_sample);
extern void add_freedv_mic_sample(TRANSMITTER *tx,short mic_sample);

extern void transmitter_save_state(TRANSMITTER *tx);
extern void transmitter_set_out_of_band(TRANSMITTER *tx);
extern void tx_set_displaying(TRANSMITTER *tx,int state);

extern void tx_set_ps(TRANSMITTER *tx,int state);
extern void tx_set_twotone(TRANSMITTER *tx,int state);

extern void transmitter_set_compressor_level(TRANSMITTER *tx,double level);
extern void transmitter_set_compressor(TRANSMITTER *tx,int state);

extern void tx_set_ps_sample_rate(TRANSMITTER *tx,int rate);
extern void add_ps_iq_samples(TRANSMITTER *tx, double i_sample_0,double q_sample_0, double i_sample_1, double q_sample_1);

extern void cw_hold_key(int state);
#endif


