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

} TRANSMITTER;

extern TRANSMITTER *create_transmitter(int id, int buffer_size, int fft_size, int fps, int width, int height);

void reconfigure_transmitter(TRANSMITTER *tx,int height);

extern void tx_set_mode(TRANSMITTER* tx,int m);
extern void tx_set_filter(TRANSMITTER *tx,int low,int high);
extern void tx_set_pre_emphasize(TRANSMITTER *tx,int state);

extern void add_mic_sample(TRANSMITTER *tx,short mic_sample);

extern void transmitter_save_state(TRANSMITTER *tx);
#endif
