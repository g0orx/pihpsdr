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
#include <gdk/gdk.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include <wdsp.h>

#include "agc.h"
#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "rx_panadapter.h"
#include "vfo.h"
#include "mode.h"
#ifdef FREEDV
#include "freedv.h"
#endif
#ifdef GPIO
#include "gpio.h"
#endif

//static float panadapter_max=-60.0;
//static float panadapter_min=-160.0;

static gfloat filter_left;
static gfloat filter_right;
static gfloat cw_frequency;

/* Create a new surface of the appropriate size to store our scribbles */
static gboolean
panadapter_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{

  RECEIVER *rx=(RECEIVER *)data;

  int display_width=gtk_widget_get_allocated_width (widget);
  int display_height=gtk_widget_get_allocated_height (widget);

  if (rx->panadapter_surface)
    cairo_surface_destroy (rx->panadapter_surface);

  rx->panadapter_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       display_width,
                                       display_height);

  cairo_t *cr=cairo_create(rx->panadapter_surface);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_paint(cr);
  cairo_destroy(cr);
  return TRUE;
}

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean
panadapter_draw_cb (GtkWidget *widget,
 cairo_t   *cr,
 gpointer   data)
{
  RECEIVER *rx=(RECEIVER *)data;
  if(rx->panadapter_surface) {
    cairo_set_source_surface (cr, rx->panadapter_surface, 0.0, 0.0);
    cairo_paint (cr);
  }

  return FALSE;
}

static gboolean panadapter_button_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  return receiver_button_press_event(widget,event,data);
}

static gboolean panadapter_button_release_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  return receiver_button_release_event(widget,event,data);
}

static gboolean panadapter_motion_notify_event_cb(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
  return receiver_motion_notify_event(widget,event,data);
}

static gboolean panadapter_scroll_event_cb(GtkWidget *widget, GdkEventScroll *event, gpointer data) {
  return receiver_scroll_event(widget,event,data);
}

void rx_panadapter_update(RECEIVER *rx) {
  int i;
  int x1,x2;
  int result;
  float *samples;
  float saved_max;
  float saved_min;
  cairo_text_extents_t extents;

  gboolean active=active_receiver==rx;

  int display_width=gtk_widget_get_allocated_width (rx->panadapter);
  int display_height=gtk_widget_get_allocated_height (rx->panadapter);

#ifdef FREEDV
  if(rx->freedv) {
    display_height=display_height-20;
  }
#endif
  samples=rx->pixel_samples;

  //clear_panadater_surface();
  cairo_t *cr;
  cr = cairo_create (rx->panadapter_surface);
  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_rectangle(cr,0,0,display_width,display_height);
  cairo_fill(cr);
  //cairo_paint (cr);

  long long frequency=vfo[rx->id].frequency;
  long half=(long)rx->sample_rate/2L;
  long long min_display=frequency-half;
  long long max_display=frequency+half;
  BAND *band=band_get_band(vfo[rx->id].band);

  if(vfo[rx->id].band==band60) {
    for(i=0;i<channel_entries;i++) {
      long long low_freq=band_channels_60m[i].frequency-(band_channels_60m[i].width/(long long)2);
      long long hi_freq=band_channels_60m[i].frequency+(band_channels_60m[i].width/(long long)2);
      x1=(low_freq-min_display)/(long long)rx->hz_per_pixel;
      x2=(hi_freq-min_display)/(long long)rx->hz_per_pixel;
      cairo_set_source_rgb (cr, 0.6, 0.3, 0.3);
      cairo_rectangle(cr, x1, 0.0, x2-x1, (double)display_height);
      cairo_fill(cr);
/*
      cairo_set_source_rgba (cr, 0.5, 1.0, 0.0, 1.0);
      cairo_move_to(cr,(double)x1,0.0);
      cairo_line_to(cr,(double)x1,(double)display_height);
      cairo_stroke(cr);
      cairo_move_to(cr,(double)x2,0.0);
      cairo_line_to(cr,(double)x2,(double)display_height);
      cairo_stroke(cr);
*/
    }
  }

  double cwshift;
  int begin,end,icwshift;

  //
  // In this program, when using CW, we will always transmit on exactly
  // the frequency which is the "VFO display" frequency, not 800 Hz above
  // or below. Therfore, the RX center frequency sent to the SDR is shifted
  // by the sidetone frequency.
  // Previous versions showed a "shifted" RX spectrum where the CW signal that
  // was received exactly on the VFO frequency was shifted to the right (CWU)
  // or to the left (CWL), the exact position shown by a yellow line.
  // 
  // Alternatively, one could arrange things such that the RX spectrum is
  // shown correctly and that in CWU you "hear" a CW signal at 7000,0 kHz
  // when the VFO display frequency is 7000.0 kHz. Then, the TX signal must
  // have an offset, that is, you send a CW signal at 7000.8 kHz when the
  // VFO display shows 7000.0 kHz, and the yellow line is at 7000.8 kHz.
  //
  // Instead of drawing a "yellow line" to show where the received CW
  // signal is, we shift the displayed spectrum by the side tone such
  // that the received signals occur at the nominal frequencies
  // That is, in CW we TX at the VFO display frequency which is marked by
  // a red line, and we also receive CW signals from exactly that position in
  // the RX spectrum. Furthermore, when switching between CWU and CWL, the
  // displayed RX spectrum remains unchanged except for the part at the right
  // or left margin of the display.
  //
  // For me (DL1YCF) this seems more logical.
  //
  switch (vfo[rx->id].mode) {
    case modeCWU:
        // spectrum must be shifted to the left
        cwshift=(double) cw_keyer_sidetone_frequency / rx->hz_per_pixel;
        icwshift=(int) cwshift;
        begin=0;
        end=display_width-icwshift;
        break;
    case modeCWL:
        // spectrum must shifted to the right
        cwshift=-(double) cw_keyer_sidetone_frequency / rx->hz_per_pixel;
        icwshift=(int) cwshift;
        begin=-cwshift;
        end=display_width;
        break;
    default:
        cwshift=0.0;
        icwshift=0;
        begin=0;
        end=display_width;
        break;
  }


  // filter
  cairo_set_source_rgba (cr, 0.50, 0.50, 0.50, 0.75);
  filter_left =-cwshift+(double)display_width/2.0+(((double)rx->filter_low+vfo[rx->id].offset)/rx->hz_per_pixel);
  filter_right=-cwshift+(double)display_width/2.0+(((double)rx->filter_high+vfo[rx->id].offset)/rx->hz_per_pixel);
  cairo_rectangle(cr, filter_left, 0.0, filter_right-filter_left, (double)display_height);
  cairo_fill(cr);

/*
  do not draw the "yellow line". Instead, shift the spectrum
  such that the rx frequency offset is compensated

  if(vfo[rx->id].mode==modeCWU || vfo[rx->id].mode==modeCWL) {
    if(active) {
      cairo_set_source_rgb (cr, 1.0, 1.0, 0.0);
    } else {
      cairo_set_source_rgb (cr, 0.25, 0.25, 0.0);
    }
    cw_frequency=filter_left+((filter_right-filter_left)/2.0);
    cairo_move_to(cr,cw_frequency,10.0);
    cairo_line_to(cr,cw_frequency,(double)display_height);
    cairo_stroke(cr);
  }
*/

  // plot the levels
  if(active) {
    cairo_set_source_rgb (cr, 0.0, 1.0, 1.0);
  } else {
    cairo_set_source_rgb (cr, 0.0, 0.5, 0.5);
  }

  double dbm_per_line=(double)display_height/((double)rx->panadapter_high-(double)rx->panadapter_low);
  cairo_set_line_width(cr, 1.0);
  cairo_select_font_face(cr, "FreeMono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char v[32];

  for(i=rx->panadapter_high;i>=rx->panadapter_low;i--) {
    int mod=abs(i)%rx->panadapter_step;
    if(mod==0) {
      double y = (double)(rx->panadapter_high-i)*dbm_per_line;
      cairo_move_to(cr,0.0,y);
      cairo_line_to(cr,(double)display_width,y);

      sprintf(v,"%d dBm",i);
      cairo_move_to(cr, 1, y);  
      cairo_show_text(cr, v);
    }
  }
  cairo_stroke(cr);

  // plot frequency markers
  long long f;
  long divisor=20000;
  switch(rx->sample_rate) {
    case 48000:
      divisor=5000L;
      break;
    case 96000:
    case 100000:
      divisor=10000L;
      break;
    case 192000:
      divisor=20000L;
      break;
    case 384000:
      divisor=50000L;
      break;
    case 768000:
      divisor=50000L;
      break;
    case 1048576:
    case 1536000:
    case 2097152:
      divisor=100000L;
      break;
  }
  for(i=0;i<display_width;i++) {
    f = frequency - half + (long) (rx->hz_per_pixel * i);
    if (f > 0) {
      if ((f % divisor) < (long) rx->hz_per_pixel) {
        cairo_set_line_width(cr, 1.0);
        //cairo_move_to(cr,(double)i,0.0);
        cairo_move_to(cr,(double)i,10.0);
        cairo_line_to(cr,(double)i,(double)display_height);

        cairo_select_font_face(cr, "FreeMono",
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        char v[32];
        sprintf(v,"%0lld.%03lld",f/1000000,(f%1000000)/1000);
        //cairo_move_to(cr, (double)i, (double)(display_height-10));  
        cairo_text_extents(cr, v, &extents);
        cairo_move_to(cr, (double)i-(extents.width/2.0), 10.0);  
        cairo_show_text(cr, v);
      }
    }
  }
  cairo_stroke(cr);

  if(vfo[rx->id].band!=band60) {
    // band edges
    if(band->frequencyMin!=0LL) {
      cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
      cairo_set_line_width(cr, 1.0);
      if((min_display<band->frequencyMin)&&(max_display>band->frequencyMin)) {
        i=(band->frequencyMin-min_display)/(long long)rx->hz_per_pixel;
        cairo_move_to(cr,(double)i,0.0);
        cairo_line_to(cr,(double)i,(double)display_height);
        cairo_stroke(cr);
      }
      if((min_display<band->frequencyMax)&&(max_display>band->frequencyMax)) {
        i=(band->frequencyMax-min_display)/(long long)rx->hz_per_pixel;
        cairo_move_to(cr,(double)i,0.0);
        cairo_line_to(cr,(double)i,(double)display_height);
        cairo_stroke(cr);
      }
    }
  }
            
  // agc
  if(rx->agc!=AGC_OFF) {
    double hang=0.0;
    double thresh=0;

    GetRXAAGCHangLevel(rx->id, &hang);
    GetRXAAGCThresh(rx->id, &thresh, 4096.0, (double)rx->sample_rate);

    double knee_y=thresh+(double)adc_attenuation[rx->adc];
    if (filter_board == ALEX && rx->adc == 0) knee_y += (double)(10*rx->alex_attenuation);
    knee_y = floor((rx->panadapter_high - knee_y)
                        * (double) display_height
                        / (rx->panadapter_high - rx->panadapter_low));

    double hang_y=hang+(double)adc_attenuation[rx->adc];
    if (filter_board == ALEX && rx->adc == 0) hang_y += (double)(10*rx->alex_attenuation);
    hang_y = floor((rx->panadapter_high - hang_y)
                        * (double) display_height
                        / (rx->panadapter_high - rx->panadapter_low));

//fprintf(stderr,"hang=%f thresh=%f hang_y=%f knee_y=%f\n",rx1_hang,rx1_thresh,hang_y,knee_y);
    if(rx->agc!=AGC_MEDIUM && rx->agc!=AGC_FAST) {
      if(active) {
        cairo_set_source_rgb (cr, 1.0, 1.0, 0.0);
      } else {
        cairo_set_source_rgb (cr, 0.5, 0.5, 0.0);
      }
      cairo_move_to(cr,40.0,hang_y-8.0);
      cairo_rectangle(cr, 40, hang_y-8.0,8.0,8.0);
      cairo_fill(cr);
      cairo_move_to(cr,40.0,hang_y);
      cairo_line_to(cr,(double)display_width-40.0,hang_y);
      cairo_stroke(cr);
      cairo_move_to(cr,48.0,hang_y);
      cairo_show_text(cr, "-H");
    }

    if(active) {
      cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
    } else {
      cairo_set_source_rgb (cr, 0.0, 0.5, 0.0);
    }
    cairo_move_to(cr,40.0,knee_y-8.0);
    cairo_rectangle(cr, 40, knee_y-8.0,8.0,8.0);
    cairo_fill(cr);
    cairo_move_to(cr,40.0,knee_y);
    cairo_line_to(cr,(double)display_width-40.0,knee_y);
    cairo_stroke(cr);
    cairo_move_to(cr,48.0,knee_y);
    cairo_show_text(cr, "-G");
  }


  // cursor
  if(active) {
    cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
  } else {
    cairo_set_source_rgb (cr, 0.5, 0.0, 0.0);
  }
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr,(double)(display_width/2.0)+(vfo[rx->id].offset/rx->hz_per_pixel),0.0);
  cairo_line_to(cr,(double)(display_width/2.0)+(vfo[rx->id].offset/rx->hz_per_pixel),(double)display_height);
  cairo_stroke(cr);

  // signal
  double s1,s2;

  samples[begin+icwshift]=-200.0;
  samples[end-1+icwshift]=-200.0;
  s1=(double)samples[begin+icwshift]+(double)adc_attenuation[rx->adc];
  if (filter_board == ALEX && rx->adc == 0) s1 += (double)(10*rx->alex_attenuation);

  s1 = floor((rx->panadapter_high - s1)
                        * (double) display_height
                        / (rx->panadapter_high - rx->panadapter_low));
  cairo_move_to(cr, (double) begin, s1);
  for(i=begin+1;i<end;i++) {
    s2=(double)samples[i+icwshift]+(double)adc_attenuation[rx->adc];
    if (filter_board == ALEX && rx->adc == 0) s2 += (double)(10*rx->alex_attenuation);
    s2 = floor((rx->panadapter_high - s2)
                            * (double) display_height
                            / (rx->panadapter_high - rx->panadapter_low));
    cairo_line_to(cr, (double)i, s2);
  }

  if(display_filled) {
    cairo_close_path (cr);
    if(active) {
      cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,0.5);
    } else {
      cairo_set_source_rgba(cr, 0.5, 0.5, 0.5,0.5);
    }
    cairo_fill_preserve (cr);
  }
  if(active) {
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  } else {
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
  }
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);

#ifdef FREEDV
  if(rx->freedv) {
    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    cairo_rectangle(cr,0,display_height,display_width,display_height+20);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    cairo_set_font_size(cr, 18);
    cairo_move_to(cr, 0.0, (double)display_height+20.0-2.0);
    cairo_show_text(cr, rx->freedv_text_data);
  }
#endif

#ifdef GPIO 
#ifndef CONTROLLER2 
  if(active) {
    cairo_set_source_rgb(cr,1.0,1.0,0.0);
    cairo_set_font_size(cr,16);
    if(ENABLE_E2_ENCODER) {
      cairo_move_to(cr, display_width-150,30);
      cairo_show_text(cr, encoder_string[e2_encoder_action]);
    }

    if(ENABLE_E3_ENCODER) {
      cairo_move_to(cr, display_width-150,50);
      cairo_show_text(cr, encoder_string[e3_encoder_action]);
    }

    if(ENABLE_E4_ENCODER) {
      cairo_move_to(cr, display_width-150,70);
      cairo_show_text(cr, encoder_string[e4_encoder_action]);
    }
  }
#endif
#endif


  cairo_destroy (cr);
  gtk_widget_queue_draw (rx->panadapter);

}

void rx_panadapter_init(RECEIVER *rx, int width,int height) {

  int display_width=width;
  int display_height=height;

  rx->panadapter_surface=NULL;
  rx->panadapter = gtk_drawing_area_new ();
  gtk_widget_set_size_request (rx->panadapter, width, height);

  /* Signals used to handle the backing surface */
  g_signal_connect (rx->panadapter, "draw",
            G_CALLBACK (panadapter_draw_cb), rx);
  g_signal_connect (rx->panadapter,"configure-event",
            G_CALLBACK (panadapter_configure_event_cb), rx);

  /* Event signals */
  g_signal_connect (rx->panadapter, "motion-notify-event",
            G_CALLBACK (panadapter_motion_notify_event_cb), rx);
  g_signal_connect (rx->panadapter, "button-press-event",
            G_CALLBACK (panadapter_button_press_event_cb), rx);
  g_signal_connect (rx->panadapter, "button-release-event",
            G_CALLBACK (panadapter_button_release_event_cb), rx);
  g_signal_connect(rx->panadapter,"scroll_event",
            G_CALLBACK(panadapter_scroll_event_cb),rx);

  /* Ask to receive events the drawing area doesn't normally
   * subscribe to. In particular, we need to ask for the
   * button press and motion notify events that want to handle.
   */
  gtk_widget_set_events (rx->panadapter, gtk_widget_get_events (rx->panadapter)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_BUTTON_RELEASE_MASK
                     | GDK_BUTTON1_MOTION_MASK
                     | GDK_SCROLL_MASK
                     | GDK_POINTER_MOTION_MASK
                     | GDK_POINTER_MOTION_HINT_MASK);

}
