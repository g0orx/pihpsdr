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
#include "gpio.h"

//static float panadapter_max=-60.0;
//static float panadapter_min=-160.0;

static gfloat filter_left;
static gfloat filter_right;

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
  cairo_set_source_rgb(cr, 0, 0, 0);
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
    cairo_set_source_surface (cr, rx->panadapter_surface, 0, 0);
    cairo_paint (cr);
  }

  return TRUE;
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
  int result;
  float *samples;
  float saved_max;
  float saved_min;
  cairo_text_extents_t extents;

  gboolean active=active_receiver==rx;

  int display_width=gtk_widget_get_allocated_width (rx->panadapter);
  int display_height=gtk_widget_get_allocated_height (rx->panadapter);

  samples=rx->pixel_samples;

  //clear_panadater_surface();
  cairo_t *cr;
  cr = cairo_create (rx->panadapter_surface);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_rectangle(cr,0,0,display_width,display_height);
  cairo_fill(cr);
  //cairo_paint (cr);

  // filter
  cairo_set_source_rgb (cr, 0.25, 0.25, 0.25);
  filter_left=(double)display_width/2.0+(((double)rx->filter_low+vfo[rx->id].offset)/rx->hz_per_pixel);
  filter_right=(double)display_width/2.0+(((double)rx->filter_high+vfo[rx->id].offset)/rx->hz_per_pixel);
  cairo_rectangle(cr, filter_left, 0.0, filter_right-filter_left, (double)display_height);
  cairo_fill(cr);

  // plot the levels
  if(active) {
    cairo_set_source_rgb (cr, 0, 1, 1);
  } else {
    cairo_set_source_rgb (cr, 0, 0.5, 0.5);
  }

  double dbm_per_line=(double)display_height/((double)rx->panadapter_high-(double)rx->panadapter_low);
  cairo_set_line_width(cr, 1.0);
  cairo_select_font_face(cr, "FreeMono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char v[32];

  for(i=rx->panadapter_high;i>=rx->panadapter_low;i--) {
    int mod=abs(i)%20;
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
  long half=(long)rx->sample_rate/2L;
  long long frequency=vfo[rx->id].frequency;
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
      divisor=25000L;
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

  // band edges
  long long min_display=frequency-half;
  long long max_display=frequency+half;
  BAND *band=band_get_band(vfo[rx->id].band);
  if(band->frequencyMin!=0LL) {
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_set_line_width(cr, 2.0);
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
            
  // agc
  if(rx->agc!=AGC_OFF) {
    double hang=0.0;
    double thresh=0;

    GetRXAAGCHangLevel(rx->id, &hang);
    GetRXAAGCThresh(rx->id, &thresh, 4096.0, (double)rx->sample_rate);

    double knee_y=thresh+(double)get_attenuation();
    knee_y = floor((rx->panadapter_high - knee_y)
                        * (double) display_height
                        / (rx->panadapter_high - rx->panadapter_low));

    double hang_y=hang+(double)get_attenuation();
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
    cairo_set_source_rgb (cr, 1, 0, 0);
  } else {
    cairo_set_source_rgb (cr, 0.5, 0, 0);
  }
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr,(double)(display_width/2.0)+(vfo[rx->id].offset/rx->hz_per_pixel),0.0);
  cairo_line_to(cr,(double)(display_width/2.0)+(vfo[rx->id].offset/rx->hz_per_pixel),(double)display_height);
  cairo_stroke(cr);

  // signal
  double s1,s2;
  samples[0]=-200.0;
  samples[display_width-1]=-200.0;

  s1=(double)samples[0]+(double)get_attenuation();
  s1 = floor((rx->panadapter_high - s1)
                        * (double) display_height
                        / (rx->panadapter_high - rx->panadapter_low));
  cairo_move_to(cr, 0.0, s1);
  for(i=1;i<display_width;i++) {
    s2=(double)samples[i]+(double)get_attenuation();
    s2 = floor((rx->panadapter_high - s2)
                            * (double) display_height
                            / (rx->panadapter_high - rx->panadapter_low));
    cairo_line_to(cr, (double)i, s2);
  }

  if(display_filled) {
    cairo_close_path (cr);
    if(active) {
      cairo_set_source_rgba(cr, 1, 1, 1,0.5);
    } else {
      cairo_set_source_rgba(cr, 0.5, 0.5, 0.5,0.5);
    }
    cairo_fill_preserve (cr);
  }
  if(active) {
    cairo_set_source_rgb(cr, 1, 1, 1);
  } else {
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
  }
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);

#ifdef FREEDV
  int mode=rx->mode;
  if(mode==modeFREEDV) {
    cairo_set_source_rgb(cr, 0, 1, 0);
    cairo_set_font_size(cr, 16);
    cairo_text_extents(cr, freedv_text_data, &extents);
    cairo_move_to(cr, (double)display_width/2.0-(extents.width/2.0),(double)display_height-2.0);
    cairo_show_text(cr, freedv_text_data);
  }
#endif

#ifdef GPIO
  if(active) {
    cairo_set_source_rgb(cr,1,1,0);
    cairo_set_font_size(cr,12);
    if(ENABLE_E1_ENCODER) {
      cairo_move_to(cr, display_width-100,20);
      cairo_show_text(cr, encoder_string[e1_encoder_action]);
    }

    if(ENABLE_E2_ENCODER) {
      cairo_move_to(cr, display_width-100,40);
      cairo_show_text(cr, encoder_string[e2_encoder_action]);
    }

    if(ENABLE_E3_ENCODER) {
      cairo_move_to(cr, display_width-100,60);
      cairo_show_text(cr, encoder_string[e3_encoder_action]);
    }
  }
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
