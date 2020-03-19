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
#ifdef GPIO
#include "gpio.h"
#endif
#include "ext.h"
#include "new_menu.h"

static gint last_x;
static gboolean has_moved=FALSE;
static gboolean pressed=FALSE;

static gdouble hz_per_pixel;
static gdouble filter_left=0.0;
static gdouble filter_right=0.0;


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
  if (event->button == 1) {
    last_x=(int)event->x;
    has_moved=FALSE;
    pressed=TRUE;
  } else {
    g_idle_add(ext_start_tx,NULL);
  }
  return TRUE;
}

static gboolean
tx_panadapter_button_release_event_cb (GtkWidget      *widget,
               GdkEventButton *event,
               gpointer        data)
{
  TRANSMITTER *tx=(TRANSMITTER *)data;
  int display_width=gtk_widget_get_allocated_width (tx->panadapter);
  //int display_height=gtk_widget_get_allocated_height (tx->panadapter);

  if(pressed) {
    int x=(int)event->x;
    if (event->button == 1) {
      if(has_moved) {
        // drag
        vfo_move((long long)((float)(x-last_x)*hz_per_pixel),TRUE);
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
  //
  // DL1YCF: if !pressed, we may come from the destruction
  //         of a menu, and should not move the VFO
  //
  if (pressed) {
    gdk_window_get_device_position (event->window,
                                    event->device,
                                    &x,
                                    &y,
                                    &state);
    if(state & GDK_BUTTON1_MASK) {
      int moved=last_x-x;
      vfo_move((long long)((float)moved*hz_per_pixel),FALSE);
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
    vfo_move(step,TRUE);
  } else {
    vfo_move(-step,TRUE);
  }
  return FALSE;
}

void tx_panadapter_update(TRANSMITTER *tx) {
  int i;
  float *samples;

  if(tx->panadapter_surface) {

  int display_width=gtk_widget_get_allocated_width (tx->panadapter);
  int display_height=gtk_widget_get_allocated_height (tx->panadapter);


  int txvfo = get_tx_vfo();
  int txmode = get_tx_mode();

  samples=tx->pixel_samples;

  hz_per_pixel=(double)tx->iq_output_rate/(double)tx->pixels;

  //clear_panadater_surface();
  cairo_t *cr;
  cr = cairo_create (tx->panadapter_surface);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_paint (cr);

  
  // filter
  if (txmode != modeCWU && txmode != modeCWL) {
    cairo_set_source_rgb (cr, 0.25, 0.25, 0.25);
    filter_left=(double)display_width/2.0+((double)tx->filter_low/hz_per_pixel);
    filter_right=(double)display_width/2.0+((double)tx->filter_high/hz_per_pixel);
    cairo_rectangle(cr, filter_left, 0.0, filter_right-filter_left, (double)display_height);
    cairo_fill(cr);

  }

  // plot the levels   0, -20,  40, ... dBm (bright turquoise line with label)
  // additionally, plot the levels in steps of the chosen panadapter step size
  // (dark turquoise line without label)

  double dbm_per_line=(double)display_height/((double)tx->panadapter_high-(double)tx->panadapter_low);
  cairo_set_source_rgb (cr, 0.00, 1.00, 1.00);
  cairo_set_line_width(cr, 1.0);
  cairo_select_font_face(cr, "FreeMono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

  for(i=tx->panadapter_high;i>=tx->panadapter_low;i--) {
    char v[32];
    if((abs(i)%tx->panadapter_step) ==0) {
      double y = (double)(tx->panadapter_high-i)*dbm_per_line;
      if ((abs(i) % 20) == 0) {
        cairo_set_source_rgb (cr, 0.00, 1.00, 1.00);
        cairo_move_to(cr,0.0,y);
        cairo_line_to(cr,(double)display_width,y);

        sprintf(v,"%d dBm",i);
        cairo_move_to(cr, 1, y);
        cairo_show_text(cr, v);
        cairo_stroke(cr);
      } else {
        cairo_set_source_rgb (cr, 0.00, 0.50, 0.50);
        cairo_move_to(cr,0.0,y);
        cairo_line_to(cr,(double)display_width,y);
        cairo_stroke(cr);
      }
    }
  }

  // plot frequency markers
  long long half= duplex ? 3000LL : 12000LL; //(long long)(tx->output_rate/2);
  long long frequency;
  if(vfo[txvfo].ctun) {
    frequency=vfo[txvfo].ctun_frequency;
  } else {
    frequency=vfo[txvfo].frequency;
  }
  double vfofreq=(double)display_width * 0.5;
  if (!cw_is_on_vfo_freq) {
    if(txmode==modeCWU) {
      frequency+=(long long)cw_keyer_sidetone_frequency;
      vfofreq -=  (double) cw_keyer_sidetone_frequency/hz_per_pixel;
    } else if(txmode==modeCWL) {
      frequency-=(long long)cw_keyer_sidetone_frequency;
      vfofreq +=  (double) cw_keyer_sidetone_frequency/hz_per_pixel;
    }
  }

#ifdef TX_FREQ_MARKERS
  cairo_text_extents_t extents;
  long long f;
  long long divisor=2000;
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
#endif

  // band edges
  long long min_display=frequency-half;
  long long max_display=frequency+half;
  int b=vfo[txvfo].band;
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
  //cairo_move_to(cr,vfofreq+(vfo[id].offset/hz_per_pixel),0.0);
  //cairo_line_to(cr,vfofreq+(vfo[id].offset/hz_per_pixel),(double)display_height);
  cairo_move_to(cr,vfofreq,0.0);
  cairo_line_to(cr,vfofreq,(double)display_height);
  cairo_stroke(cr);

  // signal
  double s1,s2;

  int offset=(tx->pixels/2)-(display_width/2);
  samples[offset]=-200.0;
  samples[offset+display_width-1]=-200.0;

  s1=(double)samples[offset];
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

#ifdef GPIO
  if(controller==CONTROLLER1 && !duplex) {
    char text[64];

    cairo_set_source_rgb(cr,1.0,1.0,0.0);
    cairo_set_font_size(cr,16);
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

  if(duplex) {
    char text[64];
    cairo_set_source_rgb(cr,1.0,0.0,0.0);
    cairo_set_font_size(cr, 16);

    if(transmitter->fwd<0.0001) {
      sprintf(text,"FWD: %0.3f",transmitter->exciter);
    } else {
      sprintf(text,"FWD: %0.3f",transmitter->fwd);
    }
    cairo_move_to(cr,10,15);
    cairo_show_text(cr, text);

    sprintf(text,"REV: %0.3f",transmitter->rev);
    cairo_move_to(cr,10,30);
    cairo_show_text(cr, text);

    sprintf(text,"ALC: %0.3f",transmitter->alc);
    cairo_move_to(cr,10,45);
    cairo_show_text(cr, text);

/*
    sprintf(text,"FWD: %0.8f",transmitter->fwd);
    cairo_move_to(cr,10,60);
    cairo_show_text(cr, text);

    sprintf(text,"EXC: %0.3f",transmitter->exciter);
    cairo_move_to(cr,10,75);
    cairo_show_text(cr, text);
*/
  }

  if(tx->dialog==NULL && protocol==ORIGINAL_PROTOCOL && device==DEVICE_HERMES_LITE2) {
    char text[64];
    cairo_set_source_rgb(cr,1.0,1.0,0.0);
    cairo_set_font_size(cr,16);

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
  gtk_widget_queue_draw (tx->panadapter);

  }
}

void tx_panadapter_init(TRANSMITTER *tx, int width,int height) {

fprintf(stderr,"tx_panadapter_init: %d x %d\n",width,height);

  tx->panadapter_surface=NULL;
  tx->panadapter=gtk_drawing_area_new ();
  gtk_widget_set_size_request (tx->panadapter, width, height);

  /* Signals used to handle the backing surface */
  g_signal_connect (tx->panadapter, "draw",
            G_CALLBACK (tx_panadapter_draw_cb), tx);
  g_signal_connect (tx->panadapter,"configure-event",
            G_CALLBACK (tx_panadapter_configure_event_cb), tx);

  /* Event signals */
/*
  g_signal_connect (tx->panadapter, "motion-notify-event",
            G_CALLBACK (tx_panadapter_motion_notify_event_cb), tx);
*/
  g_signal_connect (tx->panadapter, "button-press-event",
            G_CALLBACK (tx_panadapter_button_press_event_cb), tx);
/*
  g_signal_connect (tx->panadapter, "button-release-event",
            G_CALLBACK (tx_panadapter_button_release_event_cb), tx);
  g_signal_connect(tx->panadapter,"scroll_event",
            G_CALLBACK(tx_panadapter_scroll_event_cb),tx);
*/

  /* Ask to receive events the drawing area doesn't normally
   * subscribe to. In particular, we need to ask for the
   * button press and motion notify events that want to handle.
   */
  gtk_widget_set_events (tx->panadapter, gtk_widget_get_events (tx->panadapter)
                     | GDK_BUTTON_PRESS_MASK);
/*
                     | GDK_BUTTON_RELEASE_MASK
                     | GDK_BUTTON1_MOTION_MASK
                     | GDK_SCROLL_MASK
                     | GDK_POINTER_MOTION_MASK
                     | GDK_POINTER_MOTION_HINT_MASK);
*/
}
