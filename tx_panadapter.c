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
#include "tx_panadapter.h"
#include "vfo.h"
#include "mode.h"
#ifdef FREEDV
#include "freedv.h"
#endif
#include "gpio.h"


static gint last_x;
static gboolean has_moved=FALSE;
static gboolean pressed=FALSE;

static gfloat hz_per_pixel;
static gfloat filter_left;
static gfloat filter_right;

#include "new_menu.h"
#include "ext.h"
#include "sliders.h"
static gboolean making_active = FALSE;

/* Create a new surface of the appropriate size to store our scribbles */
static gboolean
tx_panadapter_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  TRANSMITTER *tx=(TRANSMITTER *)data;
  int display_width=gtk_widget_get_allocated_width (tx->panadapter);
  int display_height=gtk_widget_get_allocated_height (tx->panadapter);

  if (tx->panadapter_surface)
    cairo_surface_destroy (tx->panadapter_surface);

  tx->panadapter_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       display_width,
                                       display_height);

  cairo_t *cr=cairo_create(tx->panadapter_surface);
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
tx_panadapter_draw_cb (GtkWidget *widget,
 cairo_t   *cr,
 gpointer   data)
{
  TRANSMITTER *tx=(TRANSMITTER *)data;
  if(tx->panadapter_surface) {
    cairo_set_source_surface (cr, tx->panadapter_surface, 0.0, 0.0);
    cairo_paint (cr);
  }
  return FALSE;
}

static gboolean
tx_panadapter_button_press_event_cb (GtkWidget      *widget,
               GdkEventButton *event,
               gpointer        data)
{
  int x=(int)event->x;
  if (event->button == 1) {
    last_x=(int)event->x;
    has_moved=FALSE;
    pressed=TRUE;
  }
  making_active=(active_receiver != receiver[0]);
  return TRUE;
}

static gboolean
tx_panadapter_button_release_event_cb (GtkWidget      *widget,
               GdkEventButton *event,
               gpointer        data)
{
  TRANSMITTER *tx=(TRANSMITTER *)data;
  int display_width=gtk_widget_get_allocated_width (tx->panadapter);
  int display_height=gtk_widget_get_allocated_height (tx->panadapter);

#ifdef SPLIT_RXTX
  // when clicking into the TX window, the first receiver is
  // selected since the TX window only hides the display of
  // the first RX
  if (making_active) {
    active_receiver=receiver[0];
    making_active=FALSE;
    g_idle_add(menu_active_receiver_changed,NULL);
    g_idle_add(ext_vfo_update,NULL);
    g_idle_add(sliders_active_receiver_changed,NULL);
  }
#endif
  if(pressed) {
    int x=(int)event->x;
    if (event->button == 1) {
      if(has_moved) {
        // drag
        vfo_move((long long)((float)(x-last_x)*hz_per_pixel));
      } else {
        // move to this frequency
        vfo_move_to((long long)((float)(x-(display_width/2))*hz_per_pixel));
      }
      last_x=x;
      pressed=FALSE;
    }
  }
  return TRUE;
}

static gboolean
tx_panadapter_motion_notify_event_cb (GtkWidget      *widget,
                GdkEventMotion *event,
                gpointer        data)
{
  int x, y;
  GdkModifierType state;
  // do not do this upon the first click. Instead, click first, drag later
  if (!making_active) {
    gdk_window_get_device_position (event->window,
                                    event->device,
                                    &x,
                                    &y,
                                    &state);
    if(((state & GDK_BUTTON1_MASK) == GDK_BUTTON1_MASK) || pressed) {
      int moved=last_x-x;
      vfo_move((long long)((float)moved*hz_per_pixel));
      last_x=x;
      has_moved=TRUE;
    }
  }

  return TRUE;
}

static gboolean
tx_panadapter_scroll_event_cb (GtkWidget      *widget,
               GdkEventScroll *event,
               gpointer        data)
{
  if(event->direction==GDK_SCROLL_UP) {
    vfo_move(step);
  } else {
    vfo_move(-step);
  }
  // DL1YCF added return statement to make the compiler happy.
  // however I am unsure about the correct return value.
  // I would have coded this as a void function.
  return FALSE;
}

void tx_panadapter_update(TRANSMITTER *tx) {
  int i;
  int result;
  float *samples;
  float saved_max;
  float saved_min;
  gfloat saved_hz_per_pixel;
  cairo_text_extents_t extents;

  if(tx->panadapter_surface) {

  int display_width=gtk_widget_get_allocated_width (tx->panadapter);
  int display_height=gtk_widget_get_allocated_height (tx->panadapter);

  // id = VFO which contains the TX frequency
  int id = active_receiver->id;
  if (split) {
    id = 1-id;
  }
  samples=tx->pixel_samples;

  //hz_per_pixel=(double)tx->output_rate/(double)display_width;
  //hz_per_pixel=24000.0/(double)display_width;
  hz_per_pixel=48000.0/(double)display_width;

  //clear_panadater_surface();
  cairo_t *cr;
  cr = cairo_create (tx->panadapter_surface);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_paint (cr);

  // filter
  cairo_set_source_rgb (cr, 0.25, 0.25, 0.25);
  filter_left=(double)display_width/2.0+((double)tx->filter_low/hz_per_pixel);
  filter_right=(double)display_width/2.0+((double)tx->filter_high/hz_per_pixel);
  cairo_rectangle(cr, filter_left, 0.0, filter_right-filter_left, (double)display_height);
  cairo_fill(cr);

  // plot the levels
  int V = (int)(tx->panadapter_high - tx->panadapter_low);
  int numSteps = V / 20;
  for (i = 1; i < numSteps; i++) {
    int num = tx->panadapter_high - i * 20;
    int y = (int)floor((tx->panadapter_high - num) * display_height / V);
    cairo_set_source_rgb (cr, 0.0, 1.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr,0.0,(double)y);
    cairo_line_to(cr,(double)display_width,(double)y);

    cairo_set_source_rgb (cr, 0.0, 1.0, 1.0);
    cairo_select_font_face(cr, "FreeMono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    char v[32];
    sprintf(v,"%d dBm",num);
    cairo_move_to(cr, 1, (double)y);  
    cairo_show_text(cr, v);
    cairo_stroke(cr);
  }

  // plot frequency markers
  long long f;
  long long divisor=20000;
  //long long half=12000LL; //(long long)(tx->output_rate/2);
  long long half=24000LL; //(long long)(tx->output_rate/2);
  long long frequency;
  frequency=vfo[id].frequency+vfo[id].offset;
  divisor=5000LL;
  for(i=0;i<display_width;i++) {
    f = frequency - half + (long) (hz_per_pixel * i);
    if (f > 0) {
      if ((f % divisor) < (long) hz_per_pixel) {
        cairo_set_source_rgb (cr, 0.0, 1.0, 1.0);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr,(double)i,10.0);
        cairo_line_to(cr,(double)i,(double)display_height);

        cairo_set_source_rgb (cr, 0.0, 1.0, 1.0);
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
  int b=vfo[id].band;
  BAND *band=band_get_band(b);
  if(band->frequencyMin!=0LL) {
    cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
    if((min_display<band->frequencyMin)&&(max_display>band->frequencyMin)) {
      i=(band->frequencyMin-min_display)/(long long)hz_per_pixel;
      cairo_move_to(cr,(double)i,0.0);
      cairo_line_to(cr,(double)i,(double)display_height);
      cairo_stroke(cr);
    }
    if((min_display<band->frequencyMax)&&(max_display>band->frequencyMax)) {
      i=(band->frequencyMax-min_display)/(long long)hz_per_pixel;
      cairo_move_to(cr,(double)i,0.0);
      cairo_line_to(cr,(double)i,(double)display_height);
      cairo_stroke(cr);
    }
  }
            
  // cursor
  cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
  cairo_set_line_width(cr, 1.0);
//fprintf(stderr,"cursor: x=%f\n",(double)(display_width/2.0)+(vfo[tx->id].offset/hz_per_pixel));
  cairo_move_to(cr,(double)(display_width/2.0)+(vfo[id].offset/hz_per_pixel),0.0);
  cairo_line_to(cr,(double)(display_width/2.0)+(vfo[id].offset/hz_per_pixel),(double)display_height);
  cairo_stroke(cr);

  // signal
  double s1,s2;
  samples[0]=-200.0;
  samples[display_width-1]=-200.0;

  int offset=0;

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      //offset=(tx->pixels/4)*1;
      offset=0;
      break;
    case NEW_PROTOCOL:
      //offset=(tx->pixels/16)*7;
      offset=(tx->pixels/8)*3;
      break;
  }

  s1=(double)samples[0+offset];
  s1 = floor((tx->panadapter_high - s1)
                        * (double) display_height
                        / (tx->panadapter_high - tx->panadapter_low));
  cairo_move_to(cr, 0.0, s1);
  for(i=1;i<display_width;i++) {
    s2=(double)samples[i+offset];
    s2 = floor((tx->panadapter_high - s2)
                            * (double) display_height
                            / (tx->panadapter_high - tx->panadapter_low));
    cairo_line_to(cr, (double)i, s2);
  }

  if(display_filled) {
    cairo_close_path (cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,0.5);
    cairo_fill_preserve (cr);
  }
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);

#ifdef FREEDV
  if(active_receiver->freedv) {
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_set_font_size(cr, 16);
    cairo_text_extents(cr, transmitter->freedv_text_data, &extents);
    cairo_move_to(cr, (double)display_width/2.0-(extents.width/2.0),(double)display_height-2.0);
    cairo_show_text(cr, transmitter->freedv_text_data);
  }
#endif

#ifdef GPIO
  cairo_set_source_rgb(cr,1.0,1.0,0.0);
  cairo_set_font_size(cr,16);
  if(ENABLE_E1_ENCODER) {
    cairo_move_to(cr, display_width-150,30);
    cairo_show_text(cr, encoder_string[e1_encoder_action]);
  }

  if(ENABLE_E2_ENCODER) {
    cairo_move_to(cr, display_width-150,50);
    cairo_show_text(cr, encoder_string[e2_encoder_action]);
  }

  if(ENABLE_E3_ENCODER) {
    cairo_move_to(cr, display_width-150,70);
    cairo_show_text(cr, encoder_string[e3_encoder_action]);
  }
#endif

#ifdef PURESIGNAL
  if(tx->puresignal) {
    cairo_set_source_rgb(cr,0.0,1.0,0.0);
    cairo_move_to(cr,display_width/2,display_height-10);
    cairo_show_text(cr, "PureSignal");

    int info[16];
    GetPSInfo(tx->id,&info[0]);
    if(info[14]==0) {
      cairo_set_source_rgb(cr,1.0,0.0,0.0);
    } else {
      cairo_set_source_rgb(cr,0.0,1.0,0.0);
    }
    cairo_move_to(cr,(display_width/2)+100,display_height-10);
    cairo_show_text(cr, "Correcting");
  }
#endif

  cairo_destroy (cr);
  gtk_widget_queue_draw (tx->panadapter);

  }
}

void tx_panadapter_init(TRANSMITTER *tx, int width,int height) {

  int display_width=width;
  int display_height=height;

fprintf(stderr,"tx_panadapter_init: %d x %d\n",display_width,display_height);

  tx->panadapter_surface=NULL;
  tx->panadapter=gtk_drawing_area_new ();
  gtk_widget_set_size_request (tx->panadapter, width, height);

  /* Signals used to handle the backing surface */
  g_signal_connect (tx->panadapter, "draw",
            G_CALLBACK (tx_panadapter_draw_cb), tx);
  g_signal_connect (tx->panadapter,"configure-event",
            G_CALLBACK (tx_panadapter_configure_event_cb), tx);

  /* Event signals */
  g_signal_connect (tx->panadapter, "motion-notify-event",
            G_CALLBACK (tx_panadapter_motion_notify_event_cb), tx);
  g_signal_connect (tx->panadapter, "button-press-event",
            G_CALLBACK (tx_panadapter_button_press_event_cb), tx);
  g_signal_connect (tx->panadapter, "button-release-event",
            G_CALLBACK (tx_panadapter_button_release_event_cb), tx);
  g_signal_connect(tx->panadapter,"scroll_event",
            G_CALLBACK(tx_panadapter_scroll_event_cb),tx);

  /* Ask to receive events the drawing area doesn't normally
   * subscribe to. In particular, we need to ask for the
   * button press and motion notify events that want to handle.
   */
  gtk_widget_set_events (tx->panadapter, gtk_widget_get_events (tx->panadapter)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_BUTTON_RELEASE_MASK
                     | GDK_BUTTON1_MOTION_MASK
                     | GDK_SCROLL_MASK
                     | GDK_POINTER_MOTION_MASK
                     | GDK_POINTER_MOTION_HINT_MASK);

}
