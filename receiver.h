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
#ifndef _RECEIVER_H
#define _RECEIVER_H

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#define AUDIO_BUFFER_SIZE 260

enum _audio_t {
    STEREO=0,
    LEFT,
    RIGHT
};

typedef enum _audio_t audio_t;

typedef struct _receiver {
  int id;

  int ddc;
  int adc;

  double volume;
  int agc;
  double agc_gain;
  double agc_slope;
  double agc_hang_threshold;
  int fps;
  int displaying;
  audio_t audio_channel;
  int sample_rate;
  int buffer_size;
  int fft_size;
  int pixels;
  int samples;
  int output_samples;
  long iq_sequence;
  double *iq_input_buffer;
  double *audio_output_buffer;
  unsigned char *audio_buffer;
  int audio_index;
  long audio_sequence;
  float *pixel_samples;
  int display_panadapter;
  int display_waterfall;
  gint update_timer_id;

  double hz_per_pixel;

  int dither;
  int random;
  int preamp;

  int nb;
  int nb2;
  int nr;
  int nr2;
  int anf;
  int snb;

  int nr_agc;
  int nr2_gain_method;
  int nr2_npe_method;
  int nr2_ae;


  //int attenuation;
  int alex_antenna;
  int alex_attenuation;

  int filter_low;
  int filter_high;

  int width;
  int height;

  GtkWidget *panel;
  GtkWidget *panadapter;
  GtkWidget *waterfall;

  int panadapter_low;
  int panadapter_high;

  int waterfall_low;
  int waterfall_high;
  int waterfall_automatic;
  cairo_surface_t *panadapter_surface;
  GdkPixbuf *pixbuf;
  int local_audio;
  int mute_when_not_active;
  int audio_device;
  snd_pcm_t *playback_handle;
  int playback_offset;
  unsigned char *playback_buffer;
  int low_latency;

  int squelch_enable;
  double squelch;

  int deviation;

  long long waterfall_frequency;
  int waterfall_sample_rate;

  int mute_radio;

#ifdef FREEDV
  GMutex freedv_mutex;
  int freedv;
  int freedv_samples;
  char freedv_text_data[64];
  int freedv_text_index;
#endif

  int x;
  int y;
} RECEIVER;

extern RECEIVER *create_pure_signal_receiver(int id, int buffer_size,int sample_rate,int pixels);
extern RECEIVER *create_receiver(int id, int buffer_size, int fft_size, int pixels, int fps, int width, int height);
extern void receiver_change_sample_rate(RECEIVER *rx,int sample_rate);
extern void receiver_change_adc(RECEIVER *rx,int adc);
extern void receiver_frequency_changed(RECEIVER *rx);
extern void receiver_mode_changed(RECEIVER *rx);
extern void receiver_filter_changed(RECEIVER *rx);
extern void receiver_vfo_changed(RECEIVER *rx);

extern void set_mode(RECEIVER* rx,int m);
extern void set_filter(RECEIVER *rx,int low,int high);
extern void set_agc(RECEIVER *rx, int agc);
extern void set_offset(RECEIVER *rx, long long offset);
extern void set_deviation(RECEIVER *rx);

extern void add_iq_samples(RECEIVER *rx, double i_sample,double q_sample);

extern void reconfigure_receiver(RECEIVER *rx,int height);

extern void receiver_save_state(RECEIVER *rx);

extern gboolean receiver_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data);
extern gboolean receiver_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data);
extern gboolean receiver_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data);
extern gboolean receiver_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer data);

extern void set_displaying(RECEIVER *rx,int state);

#endif
