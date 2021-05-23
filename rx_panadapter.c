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
#include <arpa/inet.h>

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
#ifdef GPIO
#include "gpio.h"
#endif
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif

#define LINE_THIN  0.5
#define LINE_THICK 1.0

//static float panadapter_max=-60.0;
//static float panadapter_min=-160.0;

static gfloat filter_left;
static gfloat filter_right;
static gfloat cw_frequency;

static gint sequence_error_count=0;
static gint fexchange_error_count=0;

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
  float *samples;
  char text[64];
  cairo_text_extents_t extents;
  long long f;
  long long divisor=20000;
  double x=0.0;

  gboolean active=active_receiver==rx;

  int display_width=gtk_widget_get_allocated_width (rx->panadapter);
  int display_height=gtk_widget_get_allocated_height (rx->panadapter);

  samples=rx->pixel_samples;

  //clear_panadater_surface();
  cairo_t *cr;
  cr = cairo_create (rx->panadapter_surface);
  cairo_set_line_width(cr, LINE_THIN);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_rectangle(cr,0,0,display_width,display_height);
  cairo_fill(cr);
  //cairo_paint (cr);

  double HzPerPixel = rx->hz_per_pixel;  // need this many times

  int mode=vfo[rx->id].mode;
  long long frequency=vfo[rx->id].frequency;
  int vfoband=vfo[rx->id].band;
  long long offset = vfo[rx->id].ctun ? vfo[rx->id].offset : 0;

  // In diversity mode, the RX1 frequency tracks the RX0 frequency
  if (diversity_enabled && rx->id == 1) {
    frequency=vfo[0].frequency;
    vfoband=vfo[0].band;
    mode=vfo[0].mode;
  }

  BAND *band=band_get_band(vfoband);
  long long half=(long long)rx->sample_rate/2LL;
  double vfofreq=((double) rx->pixels * 0.5)-(double)rx->pan;

  //
  // There are two options here in CW mode, depending on cw_is_on_vfo_freq.
  //
  // If true, the CW frequency is the VFO frequency and the center of the spectrum
  // then is at the VFO frequency plus or minus the sidetone frequency. However we
  // will keep the center of the PANADAPTER at the VFO frequency and shift the
  // pixels of the spectrum.
  //
  // If false, the center of the spectrum is at the VFO frequency and the TX
  // frequency is VFO freq +/- sidetone frequency. In this case we mark this
  // freq by a yellow line.
  //

  if (cw_is_on_vfo_freq) {
    if (mode == modeCWU) {
      frequency -= cw_keyer_sidetone_frequency;
      vfofreq += (double) cw_keyer_sidetone_frequency / HzPerPixel;
    } else if (mode == modeCWL) {
      frequency += cw_keyer_sidetone_frequency;
      vfofreq -= (double) cw_keyer_sidetone_frequency / HzPerPixel;
    }
  }
  long long min_display=frequency-half+(long long)((double)rx->pan*HzPerPixel);
  long long max_display=min_display+(long long)((double)rx->width*HzPerPixel);

  if(vfoband==band60) {
    for(i=0;i<channel_entries;i++) {
      long long low_freq=band_channels_60m[i].frequency-(band_channels_60m[i].width/(long long)2);
      long long hi_freq=band_channels_60m[i].frequency+(band_channels_60m[i].width/(long long)2);
      x1=(low_freq-min_display)/(long long)HzPerPixel;
      x2=(hi_freq-min_display)/(long long)HzPerPixel;
      cairo_set_source_rgb (cr, 0.6, 0.3, 0.3);
      cairo_rectangle(cr, x1, 0.0, x2-x1, (double)display_height);
      cairo_fill(cr);
    }
  }

  // filter
  cairo_set_source_rgba (cr, 0.25, 0.25, 0.25, 0.75);
  //filter_left =(double)display_width*0.5 +(((double)rx->filter_low+offset)/HzPerPixel);
  //filter_right=(double)display_width*0.5 +(((double)rx->filter_high+offset)/HzPerPixel);
  filter_left =((double)rx->pixels*0.5)-(double)rx->pan +(((double)rx->filter_low+offset)/HzPerPixel);
  filter_right=((double)rx->pixels*0.5)-(double)rx->pan +(((double)rx->filter_high+offset)/HzPerPixel);
  cairo_rectangle(cr, filter_left, 0.0, filter_right-filter_left, (double)display_height);
  cairo_fill(cr);

  //
  // Draw the "yellow line" indicating the CW frequency when
  // it is not the VFO freq
  //
  if (!cw_is_on_vfo_freq && (mode==modeCWU || mode==modeCWL)) {
    if(active) {
      cairo_set_source_rgb (cr, 1.0, 1.0, 0.0);
    } else {
      cairo_set_source_rgb (cr, 0.25, 0.25, 0.0);
    }
    cw_frequency=filter_left+((filter_right-filter_left)/2.0);
    cairo_move_to(cr,cw_frequency,10.0);
    cairo_line_to(cr,cw_frequency,(double)display_height);
    cairo_set_line_width(cr, LINE_THICK);
    cairo_stroke(cr);
  }

  // plot the levels
  if(active) {
    cairo_set_source_rgb (cr, 0.0, 1.0, 1.0);
  } else {
    cairo_set_source_rgb (cr, 0.0, 0.5, 0.5);
  }

  double dbm_per_line=(double)display_height/((double)rx->panadapter_high-(double)rx->panadapter_low);
  cairo_set_line_width(cr, LINE_THIN);
  cairo_select_font_face(cr, DISPLAY_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, DISPLAY_FONT_SIZE2);
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
  cairo_set_line_width(cr, LINE_THIN);
  cairo_stroke(cr);

  // plot frequency markers
  switch(rx->sample_rate) {
    case 48000:
      divisor=5000LL;
      switch(rx->zoom) {
        case 2:
        case 3:
        case 4:
          divisor=2000LL;
          break;
        case 5:
        case 6:
        case 7:
        case 8:
          divisor=1000LL;
          break;
      }
      break;
    case 96000:
    case 100000:
      divisor=10000LL;
      switch(rx->zoom) {
        case 2:
        case 3:
        case 4:
          divisor=5000LL;
          break;
        case 5:
        case 6:
          divisor=2000LL;
          break;
        case 7:
        case 8:
          divisor=1000LL;
          break;
      }
      break;
    case 192000:
      divisor=20000LL;
      switch(rx->zoom) {
        case 2:
        case 3:
          divisor=10000LL;
          break;
        case 4:
        case 5:
        case 6:
          divisor=5000LL;
          break;
        case 7:
        case 8:
          divisor=2000LL;
          break;
      }
      break;
    case 384000:
      divisor=50000LL;
      switch(rx->zoom) {
        case 2:
        case 3:
          divisor=25000LL;
          break;
        case 4:
        case 5:
        case 6:
          divisor=10000LL;
          break;
        case 7:
        case 8:
          divisor=5000LL;
          break;
      }
      break;
    case 768000:
      divisor=100000LL;
      switch(rx->zoom) {
        case 2:
        case 3:
          divisor=50000LL;
          break;
        case 4:
        case 5:
        case 6:
          divisor=25000LL;
          break;
        case 7:
        case 8:
          divisor=20000LL;
          break;
      }
      break;
    case 1024000:
    case 1536000:
    case 2097152:
      divisor=200000LL;
      switch(rx->zoom) {
        case 2:
        case 3:
          divisor=100000LL;
          break;
        case 4:
        case 5:
        case 6:
          divisor=50000LL;
          break;
        case 7:
        case 8:
          divisor=20000LL;
          break;
      }
      break;
  }

  f = ((min_display/divisor)*divisor)+divisor;
  cairo_select_font_face(cr, DISPLAY_FONT,
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, DISPLAY_FONT_SIZE2);
  while(f<max_display) {
    x=(double)(f-min_display)/HzPerPixel;
    cairo_move_to(cr,(double)x,0.0);
    cairo_line_to(cr,(double)x,(double)display_height);

    sprintf(v,"%0lld.%03lld",f/1000000,(f%1000000)/1000);
    cairo_text_extents(cr, v, &extents);
    cairo_move_to(cr, x-(extents.width/2.0), 10.0);  
    cairo_show_text(cr, v);
    f+=divisor;
  }
  cairo_set_line_width(cr, LINE_THIN);
  cairo_stroke(cr);

  if(vfoband!=band60) {
    // band edges
    if(band->frequencyMin!=0LL) {
      cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
      cairo_set_line_width(cr, LINE_THICK);
      if((min_display<band->frequencyMin)&&(max_display>band->frequencyMin)) {
        i=(band->frequencyMin-min_display)/(long long)HzPerPixel;
        cairo_move_to(cr,(double)i,0.0);
        cairo_line_to(cr,(double)i,(double)display_height);
        cairo_set_line_width(cr, LINE_THICK);
        cairo_stroke(cr);
      }
      if((min_display<band->frequencyMax)&&(max_display>band->frequencyMax)) {
        i=(band->frequencyMax-min_display)/(long long)HzPerPixel;
        cairo_move_to(cr,(double)i,0.0);
        cairo_line_to(cr,(double)i,(double)display_height);
        cairo_set_line_width(cr, LINE_THICK);
        cairo_stroke(cr);
      }
    }
  }
            
#ifdef CLIENT_SERVER
  if(clients!=NULL) {
    cairo_select_font_face(cr, DISPLAY_FONT,
              CAIRO_FONT_SLANT_NORMAL,
              CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
    cairo_set_font_size(cr, DISPLAY_FONT_SIZE4);
    inet_ntop(AF_INET, &(((struct sockaddr_in *)&clients->address)->sin_addr),text,64);
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, ((double)display_width/2.0)-(extents.width/2.0), (double)display_height/2.0);
    cairo_show_text(cr, text);
  }
#endif


  // agc
  if(rx->agc!=AGC_OFF) {
    cairo_set_line_width(cr, LINE_THICK);
    double knee_y=rx->agc_thresh+(double)adc[rx->adc].attenuation;
    if (filter_board == ALEX && rx->adc == 0) knee_y += (double)(10*rx->alex_attenuation);
    knee_y = floor((rx->panadapter_high - knee_y)
                        * (double) display_height
                        / (rx->panadapter_high - rx->panadapter_low));

    double hang_y=rx->agc_hang+(double)adc[rx->adc].attenuation;
    if (filter_board == ALEX && rx->adc == 0) hang_y += (double)(10*rx->alex_attenuation);
    hang_y = floor((rx->panadapter_high - hang_y)
                        * (double) display_height
                        / (rx->panadapter_high - rx->panadapter_low));

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
      cairo_set_line_width(cr, LINE_THICK);
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
    cairo_set_line_width(cr, LINE_THICK);
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
  cairo_move_to(cr,vfofreq+(offset/HzPerPixel),0.0);
  cairo_line_to(cr,vfofreq+(offset/HzPerPixel),(double)display_height);
  cairo_set_line_width(cr, LINE_THIN);
  cairo_stroke(cr);

  // signal
  double s1,s2;

  int pan=rx->pan;
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    pan=0;
  }
#endif

  samples[pan]=-200.0;
  samples[display_width-1+pan]=-200.0;
  if(have_rx_gain) {
    s1=(double)samples[pan]+rx_gain_calibration-adc[rx->adc].attenuation;
  } else {
    s1=(double)samples[pan]+(double)adc[rx->adc].attenuation;
  }
  cairo_move_to(cr, 0.0, s1);
  if (filter_board == ALEX && rx->adc == 0) s1 += (double)(10*rx->alex_attenuation);
  if (filter_board == CHARLY25) {
    if (rx->preamp) s1 -= 18.0;
    if (rx->dither) s1 -= 18.0;
  }
#ifdef SOAPYSDR
  if(protocol==SOAPYSDR_PROTOCOL) {
    s1-=adc[rx->adc].gain;
  }
#endif

  s1 = floor((rx->panadapter_high - s1)
                        * (double) display_height
                        / (rx->panadapter_high - rx->panadapter_low));
  cairo_move_to(cr, 0.0, s1);
  for(i=1;i<display_width;i++) {
    if(have_rx_gain) {
      s2=(double)samples[i+pan]+rx_gain_calibration-adc[rx->adc].attenuation;
    } else {
      s2=(double)samples[i+pan]+(double)adc[rx->adc].attenuation;
    }
    if (filter_board == ALEX && rx->adc == 0) s2 += (double)(10*rx->alex_attenuation);
    if (filter_board == CHARLY25) {
      if (rx->preamp) s2 -= 18.0;
      if (rx->dither) s2 -= 18.0;
    }
#ifdef SOAPYSDR
    if(protocol==SOAPYSDR_PROTOCOL) {
      s2-=adc[rx->adc].gain;
    }
#endif
    s2 = floor((rx->panadapter_high - s2)
                            * (double) display_height
                            / (rx->panadapter_high - rx->panadapter_low));
    cairo_line_to(cr, (double)i, s2);
  }

  cairo_pattern_t *gradient;
  gradient=NULL;
  if(display_gradient) {
    gradient = cairo_pattern_create_linear(0.0, display_height, 0.0, 0.0);
    // calculate where S9 is
    double S9=-73;
    if(vfo[rx->id].frequency>30000000LL) {
      S9=-93;
    }
    S9 = floor((rx->panadapter_high - S9)
                            * (double) display_height
                            / (rx->panadapter_high - rx->panadapter_low));
    S9 = 1.0-(S9/(double)display_height);
  
    if(active) {
      cairo_pattern_add_color_stop_rgb (gradient,0.0,0.0,1.0,0.0); // Green
      cairo_pattern_add_color_stop_rgb (gradient,S9/3.0,1.0,0.65,0.0); // Orange
      cairo_pattern_add_color_stop_rgb (gradient,(S9/3.0)*2.0,1.0,1.0,0.0); // Yellow
      cairo_pattern_add_color_stop_rgb (gradient,S9,1.0,0.0,0.0); // Red
    } else {
      cairo_pattern_add_color_stop_rgb (gradient,0.0,0.0,0.5,0.0); // Green
      cairo_pattern_add_color_stop_rgb (gradient,S9/3.0,0.5,0.325,0.0); // Orange
      cairo_pattern_add_color_stop_rgb (gradient,(S9/3.0)*2.0,0.5,0.5,0.0); // Yellow
      cairo_pattern_add_color_stop_rgb (gradient,S9,0.5,0.0,0.0); // Red
    }
    cairo_set_source(cr, gradient);
  } else {
    //
    // if filled, use 0.5 (active) and 0.25 (inactive)
    // if only drawing the line, use 0.75 for active RX
    //
    double brightness=0.25;
    if (active) brightness = 0.50;
    if (!display_filled && active) brightness =0.75;
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, brightness);
  }

  if(display_filled) {
    cairo_close_path (cr);
    cairo_fill_preserve (cr);
    cairo_set_line_width(cr, LINE_THIN);
  } else {
    //
    // if not filling, use thicker line
    //
    cairo_set_line_width(cr, LINE_THICK);
  }
  cairo_stroke(cr);

  if(gradient) {
    cairo_pattern_destroy(gradient);
  }

#ifdef GPIO
  if(rx->id==0 && controller==CONTROLLER1) {

    cairo_set_source_rgb(cr,1.0,1.0,0.0);
    cairo_set_font_size(cr,DISPLAY_FONT_SIZE3);
    if(ENABLE_E2_ENCODER) {
      cairo_move_to(cr, display_width-200,70);
      sprintf(text,"%s (%s)",encoder_string[e2_encoder_action],sw_string[e2_sw_action]);
      cairo_show_text(cr, text);
    }

    if(ENABLE_E3_ENCODER) {
      cairo_move_to(cr, display_width-200,90);
      sprintf(text,"%s (%s)",encoder_string[e3_encoder_action],sw_string[e3_sw_action]);
      cairo_show_text(cr, text);
    }

    if(ENABLE_E4_ENCODER) {
      cairo_move_to(cr, display_width-200,110);
      sprintf(text,"%s (%s)",encoder_string[e4_encoder_action],sw_string[e4_sw_action]);
      cairo_show_text(cr, text);
    }
  }
#endif

  if(display_sequence_errors) {
    if(sequence_errors!=0) {
      cairo_move_to(cr,100.0,50.0);
      cairo_set_source_rgb(cr,1.0,0.0,0.0);
      cairo_set_font_size(cr,DISPLAY_FONT_SIZE2);
      cairo_show_text(cr, "Sequence Error");
      sequence_error_count++;
      // show for 2 second
      if(sequence_error_count==2*rx->fps) {
        sequence_errors=0;
        sequence_error_count=0;
      }
    }
  }

  if(rx->id==0 && protocol==ORIGINAL_PROTOCOL && device==DEVICE_HERMES_LITE2) {
    cairo_set_source_rgb(cr,1.0,1.0,0.0);
    cairo_set_font_size(cr,DISPLAY_FONT_SIZE3);

    double t = (3.26 * ((double)average_temperature / 4096.0) - 0.5) / 0.01;
    sprintf(text,"%0.1fC",t);
    cairo_move_to(cr, 100.0, 30.0);  
    cairo_show_text(cr, text);

    double c = (((3.26 * ((double)average_current / 4096.0)) / 50.0) / 0.04 * 1000 * 1270 / 1000);
    sprintf(text,"%0.0fmA",c);
    cairo_move_to(cr, 160.0, 30.0);  
    cairo_show_text(cr, text);
  }

  cairo_destroy (cr);
  gtk_widget_queue_draw (rx->panadapter);

}

void rx_panadapter_init(RECEIVER *rx, int width,int height) {

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
