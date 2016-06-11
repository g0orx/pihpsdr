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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "meter.h"
#ifdef FREEDV
#include "radio.h"
#include "mode.h"
#include "freedv.h"
#endif

static GtkWidget *meter;
static cairo_surface_t *meter_surface = NULL;

static int meter_width;
static int meter_height;
static int last_meter_type=SMETER;
static int max_level=-200;
static int max_count=0;

static void
meter_clear_surface (void)
{
  cairo_t *cr;
  cr = cairo_create (meter_surface);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
}

meter_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
fprintf(stderr,"meter_configure_event_cb: width=%d height=%d\n",
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));
  if (meter_surface)
    cairo_surface_destroy (meter_surface);

  meter_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));

  /* Initialize the surface to black */
  meter_clear_surface ();

  return TRUE;
}

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean
meter_draw_cb (GtkWidget *widget, cairo_t   *cr, gpointer   data) {
  cairo_set_source_surface (cr, meter_surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
}

GtkWidget* meter_init(int width,int height) {

fprintf(stderr,"meter_init: width=%d height=%d\n",width,height);
  meter_width=width;
  meter_height=height;

  meter = gtk_drawing_area_new ();
  gtk_widget_set_size_request (meter, width, height);
 
  /* Signals used to handle the backing surface */
  g_signal_connect (meter, "draw",
            G_CALLBACK (meter_draw_cb), NULL);
  g_signal_connect (meter,"configure-event",
            G_CALLBACK (meter_configure_event_cb), NULL);


  return meter;
}


void meter_update(int meter_type,double value,double reverse,double exciter) {
  
  char sf[32];
  int text_location;
  double offset;
  cairo_t *cr;
  cr = cairo_create (meter_surface);

  // clear the meter
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  if(last_meter_type!=meter_type) {
    last_meter_type=meter_type;
    max_count=0;
    if(meter_type==SMETER) {
      max_level=-200;
    } else {
      max_level=0;
    }
  }

  cairo_set_source_rgb(cr, 1, 1, 1);
  switch(meter_type) {
    case SMETER:
      // value is dBm
      text_location=10;
      offset=5.0;
      cairo_select_font_face(cr, "Arial",
                    CAIRO_FONT_SLANT_NORMAL,
                    CAIRO_FONT_WEIGHT_BOLD);
      double level=value+(double)get_attenuation();
      if(meter_width>=114) {
        int db=meter_width/114; // S9+60 (9*6)+60
        if(db>2) db=2;
        int i;
        cairo_set_line_width(cr, 1.0);
        cairo_set_source_rgb(cr, 1, 1, 1);
        for(i=0;i<54;i++) {
          cairo_move_to(cr,offset+(double)(i*db),(double)meter_height-20);
          if(i%18==0) {
            cairo_line_to(cr,offset+(double)(i*db),(double)(meter_height-30));
          } else if(i%6==0) {
            cairo_line_to(cr,offset+(double)(i*db),(double)(meter_height-25));
          }
        }
        cairo_stroke(cr);

        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, offset+(double)(18*db)-3.0, (double)meter_height);
        cairo_show_text(cr, "3");
        cairo_move_to(cr, offset+(double)(36*db)-3.0, (double)meter_height);
        cairo_show_text(cr, "6");

        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_move_to(cr,offset+(double)(54*db),(double)meter_height-20);
        cairo_line_to(cr,offset+(double)(54*db),(double)(meter_height-30));
        cairo_move_to(cr,offset+(double)(74*db),(double)meter_height-20);
        cairo_line_to(cr,offset+(double)(74*db),(double)(meter_height-30));
        cairo_move_to(cr,offset+(double)(94*db),(double)meter_height-20);
        cairo_line_to(cr,offset+(double)(94*db),(double)(meter_height-30));
        cairo_move_to(cr,offset+(double)(114*db),(double)meter_height-20);
        cairo_line_to(cr,offset+(double)(114*db),(double)(meter_height-30));
        cairo_stroke(cr);

        cairo_move_to(cr, offset+(double)(54*db)-3.0, (double)meter_height);
        cairo_show_text(cr, "9");
        cairo_move_to(cr, offset+(double)(74*db)-12.0, (double)meter_height);
        cairo_show_text(cr, "+20");
        cairo_move_to(cr, offset+(double)(94*db)-9.0, (double)meter_height);
        cairo_show_text(cr, "+40");
        cairo_move_to(cr, offset+(double)(114*db)-6.0, (double)meter_height);
        cairo_show_text(cr, "+60");

        cairo_set_source_rgb(cr, 0, 1, 0);
        cairo_rectangle(cr, offset+0.0, (double)(meter_height-50), (double)((level+127.0)*db), 20.0);
        cairo_fill(cr);

        if(level>max_level || max_count==10) {
          max_level=level;
          max_count=0;
        }

        if(max_level!=0) {
          cairo_set_source_rgb(cr, 1, 1, 0);
          cairo_move_to(cr,offset+(double)((max_level+127.0)*db),(double)meter_height-30);
          cairo_line_to(cr,offset+(double)((max_level+127.0)*db),(double)(meter_height-50));
        }


        cairo_stroke(cr);

        max_count++;


        text_location=offset+(db*114)+5;
      }

      cairo_set_font_size(cr, 16);
      sprintf(sf,"%d dBm",(int)level);
      cairo_move_to(cr, text_location, 45);
      cairo_show_text(cr, sf);

#ifdef FREEDV
      if(mode==modeFREEDV) {
        if(freedv_sync) {
          cairo_set_source_rgb(cr, 0, 1, 0);
        } else {
          cairo_set_source_rgb(cr, 1, 0, 0);
        }
        cairo_set_font_size(cr, 16);
        sprintf(sf,"SNR: %3.2f",freedv_snr);
        cairo_move_to(cr, text_location, 20);
        cairo_show_text(cr, sf);
      }
#endif

      break;
    case POWER:
      // value is Watts
      cairo_select_font_face(cr, "Arial",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 18);

      if((int)value>max_level || max_count==10) {
          max_level=(int)value;
          max_count=0;
      }
      max_count++;

      sprintf(sf,"FWD: %d W",(int)max_level);
      cairo_move_to(cr, 10, 25);
      cairo_show_text(cr, sf);

      double swr=(value+reverse)/(value-reverse);
      cairo_select_font_face(cr, "Arial",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 18);

      sprintf(sf,"SWR: %1.1f:1",swr);
      cairo_move_to(cr, 10, 45);
      cairo_show_text(cr, sf);
      
/*
      sprintf(sf,"REV: %3.2f W",reverse);
      cairo_move_to(cr, 10, 45);
      cairo_show_text(cr, sf);
*/
      break;
  }

  cairo_destroy(cr);
  gtk_widget_queue_draw (meter);
}
