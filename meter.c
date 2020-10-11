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
#include <math.h>

#include "receiver.h"
#include "meter.h"
#include "radio.h"
#include "wdsp.h"
#include "radio.h"
#include "version.h"
#include "mode.h"
#include "vox.h"
#include "new_menu.h"
#include "vfo.h"

static GtkWidget *parent_window;

static GtkWidget *meter;
static cairo_surface_t *meter_surface = NULL;

static int meter_width;
static int meter_height;
static int last_meter_type=SMETER;
static double max_level=-200.0;
static int max_count=0;
static int max_reverse=0;

static void
meter_clear_surface (void)
{
  cairo_t *cr;
  cr = cairo_create (meter_surface);

  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_fill (cr);

  cairo_destroy (cr);
}

static gboolean
meter_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  if (meter_surface)
    cairo_surface_destroy (meter_surface);

  meter_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));

  /* Initialize the surface to black */
  cairo_t *cr;
  cr = cairo_create (meter_surface);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_paint (cr);
  cairo_destroy (cr);

  return TRUE;
}

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean
meter_draw_cb (GtkWidget *widget, cairo_t   *cr, gpointer   data) {
  cairo_set_source_surface (cr, meter_surface, 0.0, 0.0);
  cairo_paint (cr);
  return FALSE;
}

/*
static void smeter_select_cb (GtkWidget *widget, gpointer        data)
{
  smeter=(int)data;
}

static void alc_meter_select_cb (GtkWidget *widget, gpointer        data)
{
  alc=(int)data;
}
*/

static gboolean
meter_press_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
  start_meter();
  return TRUE;
}


GtkWidget* meter_init(int width,int height,GtkWidget *parent) {

fprintf(stderr,"meter_init: width=%d height=%d\n",width,height);
  meter_width=width;
  meter_height=height;
  parent_window=parent;

  meter = gtk_drawing_area_new ();
  gtk_widget_set_size_request (meter, width, height);
 
  /* Signals used to handle the backing surface */
  g_signal_connect (meter, "draw",
            G_CALLBACK (meter_draw_cb), NULL);
  g_signal_connect (meter,"configure-event",
            G_CALLBACK (meter_configure_event_cb), NULL);

  /* Event signals */
  g_signal_connect (meter, "button-press-event",
            G_CALLBACK (meter_press_event_cb), NULL);
  gtk_widget_set_events (meter, gtk_widget_get_events (meter)
                     | GDK_BUTTON_PRESS_MASK);

  return meter;
}


void meter_update(RECEIVER *rx,int meter_type,double value,double reverse,double exciter,double alc) {
  
  double level;
  char sf[32];
  int text_location;
  double offset;
  char *units="W";
  double interval=10.0;
  cairo_t *cr = cairo_create (meter_surface);

  if(meter_type==POWER) {
    level=value;
    if(level==0.0) {
      level=exciter;
    }
    switch(pa_power) {
      case PA_1W:
        units="mW";
        interval=100.0;
        level=level*1000.0;
        break;
      case PA_10W:
        interval=1.0;
        break;
      case PA_30W:
        interval=3.0;
        break;
      case PA_50W:
        interval=5.0;
        break;
      case PA_100W:
        interval=10.0;
        break;
      case PA_200W:
        interval=20.0;
        break;
      case PA_500W:
        interval=50.0;
        break;
    }
  }

if(analog_meter) {
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_paint (cr);

  cairo_set_font_size(cr, 12);

  switch(meter_type) {
    case SMETER:
      {
      if(have_rx_gain) {
        level=value+rx_gain_calibration-adc_attenuation[rx->adc];
      } else {
        level=value+(double)adc_attenuation[rx->adc];
      }
#ifdef SOAPYSDR
      if(protocol==SOAPYSDR_PROTOCOL) {
       level-=rx->rf_gain;
      }
#endif
      if (filter_board == CHARLY25) {
	// preamp/dither encodes the preamp level
        if (rx->preamp) level -= 18.0;
        if (rx->dither) level -= 18.0;
      }
      //
      // Assume that alex_attenuation is set correctly if we have an ALEX board
      //
      if (filter_board == ALEX && rx->adc == 0) {
	level += 10*rx->alex_attenuation;
      }
	    
      offset=210.0;

      int i;
      double x;
      double y;
      double angle;
      double radians;
      double cx=(double)meter_width/2.0;
      double cy=(double)meter_width/2.0;
      double radius=cy-20.0;

      cairo_set_line_width(cr, 1.0);
      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
      cairo_arc(cr, cx, cy, radius, 216.0*M_PI/180.0, 324.0*M_PI/180.0);
      cairo_stroke(cr);

      cairo_set_line_width(cr, 2.0);
      cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
      cairo_arc(cr, cx, cy, radius+2, 264.0*M_PI/180.0, 324.0*M_PI/180.0);
      cairo_stroke(cr);

      cairo_set_line_width(cr, 1.0);
      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

      for(i=1;i<10;i++) {
        angle=((double)i*6.0)+offset;
        radians=angle*M_PI/180.0;

        if((i%2)==1) {
          cairo_arc(cr, cx, cy, radius+4, radians, radians);
          cairo_get_current_point(cr, &x, &y);
          cairo_arc(cr, cx, cy, radius, radians, radians);
          cairo_line_to(cr, x, y);
          cairo_stroke(cr);

          sprintf(sf,"%d",i);
          cairo_arc(cr, cx, cy, radius+5, radians, radians);
          cairo_get_current_point(cr, &x, &y);
          cairo_new_path(cr);
          x-=4.0;
          cairo_move_to(cr, x, y);
          cairo_show_text(cr, sf);
        } else {
          cairo_arc(cr, cx, cy, radius+2, radians, radians);
          cairo_get_current_point(cr, &x, &y);
          cairo_arc(cr, cx, cy, radius, radians, radians);
          cairo_line_to(cr, x, y);
          cairo_stroke(cr);
        }
        cairo_new_path(cr);
      }

      for(i=20;i<=60;i+=20) {
        angle=((double)i+54.0)+offset;
        radians=angle*M_PI/180.0;
        cairo_arc(cr, cx, cy, radius+4, radians, radians);
        cairo_get_current_point(cr, &x, &y);
        cairo_arc(cr, cx, cy, radius, radians, radians);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);

        sprintf(sf,"+%d",i);
        cairo_arc(cr, cx, cy, radius+5, radians, radians);
        cairo_get_current_point(cr, &x, &y);
        cairo_new_path(cr);
        x-=4.0;
        cairo_move_to(cr, x, y);
        cairo_show_text(cr, sf);
        cairo_new_path(cr);
      }



      cairo_set_line_width(cr, 1.0);
      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

      angle=fmax(-127.0,level)+127.0+offset;

      // if frequency > 30MHz then -93 is S9
      if(vfo[active_receiver->id].frequency>30000000LL) {
        angle=angle+20;
      }
     
      
      radians=angle*M_PI/180.0;
      cairo_arc(cr, cx, cy, radius+8, radians, radians);
      cairo_line_to(cr, cx, cy);
      cairo_stroke(cr);

      cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
      sprintf(sf,"%d dBm",(int)level);
      cairo_move_to(cr, 80, meter_height-2);
      cairo_show_text(cr, sf);

      }
      break;
    case POWER:
      {
      offset=220.0;

      int i;
      double x;
      double y;
      double angle;
      double radians;
      double cx=(double)meter_width/2.0;
      double cy=(double)meter_width/2.0;
      double radius=cy-20.0;

      cairo_set_line_width(cr, 1.0);
      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
      cairo_arc(cr, cx, cy, radius, 220.0*M_PI/180.0, 320.0*M_PI/180.0);
      cairo_stroke(cr);

      cairo_set_line_width(cr, 1.0);
      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

      char *units="W";
      double interval=10.0;
      switch(pa_power) {
        case PA_1W:
          units="mW";
          interval=100.0;
          break;
        case PA_10W:
          interval=1.0;
          break;
        case PA_30W:
          interval=3.0;
          break;
        case PA_50W:
          interval=5.0;
          break;
        case PA_100W:
          interval=10.0;
          break;
        case PA_200W:
          interval=20.0;
          break;
        case PA_500W:
          interval=50.0;
          break;
      }

      for(i=0;i<=100;i++) {
        angle=(double)i+offset;
        radians=angle*M_PI/180.0;
        if((i%10)==0) {
          cairo_arc(cr, cx, cy, radius+4, radians, radians);
          cairo_get_current_point(cr, &x, &y);
          cairo_arc(cr, cx, cy, radius, radians, radians);
          cairo_line_to(cr, x, y);
          cairo_stroke(cr);

          if((i%20)==0) {
            sprintf(sf,"%d",(i/10)*(int)interval);
            cairo_arc(cr, cx, cy, radius+5, radians, radians);
            cairo_get_current_point(cr, &x, &y);
            cairo_new_path(cr);
            x-=6.0;
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, sf);
          }
        }
        cairo_new_path(cr);
      }

/*
      for(i=0;i<=100;i++) {
        angle=(double)i+offset;
        radians=angle*M_PI/180.0;

        switch(i) {
          //case 5:
          case 0:
          case 25:
          case 50:
          case 75:
          case 100:
            cairo_arc(cr, cx, cy, radius+4, radians, radians);
            cairo_get_current_point(cr, &x, &y);
            cairo_arc(cr, cx, cy, radius, radians, radians);
            cairo_line_to(cr, x, y);
            cairo_stroke(cr);

            sprintf(sf,"%d",i*2);
            cairo_arc(cr, cx, cy, radius+5, radians, radians);
            cairo_get_current_point(cr, &x, &y);
            cairo_new_path(cr);
            x-=6.0;
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, sf);
            break;
          default:
            if((i%5)==0) {
              cairo_arc(cr, cx, cy, radius+2, radians, radians);
              cairo_get_current_point(cr, &x, &y);
              cairo_arc(cr, cx, cy, radius, radians, radians);
              cairo_line_to(cr, x, y);
              cairo_stroke(cr);
            }
            break;
        }
        cairo_new_path(cr);
      }
*/

      cairo_set_line_width(cr, 1.0);
      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

      if(level>max_level || max_count==10) {
          max_level=level;
          max_count=0;
      }
      max_count++;

      //angle=(max_level/2.0)+offset;
      angle=(max_level*10.0/(double)interval)+offset;
      radians=angle*M_PI/180.0;
      cairo_arc(cr, cx, cy, radius+8, radians, radians);
      cairo_line_to(cr, cx, cy);
      cairo_stroke(cr);


      cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
      sprintf(sf,"%0.1f %s",max_level,units);
      cairo_move_to(cr, 80, meter_height-22);
      cairo_show_text(cr, sf);

      double swr;
      if (max_level > reverse) {
        swr=(max_level+reverse)/(max_level-reverse);
      } else {
        swr=999.9;
      }
      sprintf(sf,"SWR: %1.1f:1",swr);
      cairo_move_to(cr, 60, meter_height-12);
      cairo_show_text(cr, sf);

      sprintf(sf,"ALC: %2.1f dB",alc);
      cairo_move_to(cr, 60, meter_height-2);
      cairo_show_text(cr, sf);


      }
      break;
  }

  if((meter_type==POWER) || (vox_enabled)) {
    offset=((double)meter_width-100.0)/2.0;
    double peak=vox_get_peak();
    peak=peak*100.0;
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    cairo_rectangle(cr, offset, 0.0, peak, 5.0);
    cairo_fill(cr);

    cairo_select_font_face(cr, "FreeMono",
                CAIRO_FONT_SLANT_NORMAL,
                CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, 0.0, 8.0);
    cairo_show_text(cr, "Mic Lvl");
    

    cairo_move_to(cr, offset, 0.0);
    cairo_line_to(cr, offset, 5.0);
    cairo_stroke(cr);
    cairo_move_to(cr, offset+50.0, 0.0);
    cairo_line_to(cr, offset+50.0, 5.0);
    cairo_stroke(cr);
    cairo_move_to(cr, offset+100.0, 0.0);
    cairo_line_to(cr, offset+100.0, 5.0);
    cairo_stroke(cr);
    cairo_move_to(cr, offset, 5.0);
    cairo_line_to(cr, offset+100.0, 5.0);
    cairo_stroke(cr);

    if(vox_enabled) {
      cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
      cairo_move_to(cr,offset+(vox_threshold*100.0),0.0);
      cairo_line_to(cr,offset+(vox_threshold*100.0),5.0);
      cairo_stroke(cr);
    }
  }

} else {
  // clear the meter
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_paint (cr);

  cairo_select_font_face(cr, "FreeMono",
                CAIRO_FONT_SLANT_NORMAL,
                CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

  cairo_set_line_width(cr, 1.0);

  if(can_transmit) {
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, 5.0, 15.0);
    cairo_line_to(cr, 5.0, 5.0);
    cairo_move_to(cr, 5.0+25.0, 15.0);
    cairo_line_to(cr, 5.0+25.0, 10.0);
    cairo_move_to(cr, 5.0+50.0, 15.0);
    cairo_line_to(cr, 5.0+50.0, 5.0);
    cairo_move_to(cr, 5.0+75.0, 15.0);
    cairo_line_to(cr, 5.0+75.0, 10.0);
    cairo_move_to(cr, 5.0+100.0, 15.0);
    cairo_line_to(cr, 5.0+100.0, 5.0);
    cairo_stroke(cr);

    double peak=vox_get_peak();
    peak=peak*100.0;
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    cairo_rectangle(cr, 5.0, 5.0, peak, 5.0);
    cairo_fill(cr);

    if(vox_enabled) {
      cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
      cairo_move_to(cr,5.0+(vox_threshold*100.0),5.0);
      cairo_line_to(cr,5.0+(vox_threshold*100.0),15.0);
      cairo_stroke(cr);
    }

    cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
    cairo_move_to(cr, 115.0, 15.0);
    cairo_show_text(cr, "Mic Level");
  }

  if(last_meter_type!=meter_type) {
    last_meter_type=meter_type;
    max_count=0;
    if(meter_type==SMETER) {
      max_level=-200;
    } else {
      max_level=0;
      max_reverse=0;
    }
  }

  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  switch(meter_type) {
    case SMETER:
      // value is dBm
      text_location=10;
      offset=5.0;
      if(have_rx_gain) {
        level=value+rx_gain_calibration-adc_attenuation[rx->adc];
      } else {
        level=value+(double)adc_attenuation[rx->adc];
      }
#ifdef SOAPYSDR
      if(protocol==SOAPYSDR_PROTOCOL) {
       level-=rx->rf_gain;
      }
#endif
      if (filter_board == CHARLY25) {
	// preamp/dither encodes the preamp level
        if (rx->preamp) level -= 18.0;
        if (rx->dither) level -= 18.0;
      }

      //
      // Assume that alex_attenuation is set correctly if we have an ALEX board
      //
      if (filter_board == ALEX && rx->adc == 0) {
	level += 10*rx->alex_attenuation;
      }

      if(meter_width>=114) {
        int db=1;
        int i;
        cairo_set_line_width(cr, 1.0);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        for(i=0;i<54;i++) {
          cairo_move_to(cr,offset+(double)(i*db),(double)meter_height-10);
          if(i%18==0) {
            cairo_line_to(cr,offset+(double)(i*db),(double)(meter_height-20));
          } else if(i%6==0) {
            cairo_line_to(cr,offset+(double)(i*db),(double)(meter_height-15));
          }
        }
        cairo_stroke(cr);

        cairo_set_font_size(cr, 10);
        cairo_move_to(cr, offset+(double)(18*db)-3.0, (double)meter_height-1);
        cairo_show_text(cr, "3");
        cairo_move_to(cr, offset+(double)(36*db)-3.0, (double)meter_height-1);
        cairo_show_text(cr, "6");

        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_move_to(cr,offset+(double)(54*db),(double)meter_height-10);
        cairo_line_to(cr,offset+(double)(54*db),(double)(meter_height-20));
        cairo_move_to(cr,offset+(double)(74*db),(double)meter_height-10);
        cairo_line_to(cr,offset+(double)(74*db),(double)(meter_height-20));
        cairo_move_to(cr,offset+(double)(94*db),(double)meter_height-10);
        cairo_line_to(cr,offset+(double)(94*db),(double)(meter_height-20));
        cairo_move_to(cr,offset+(double)(114*db),(double)meter_height-10);
        cairo_line_to(cr,offset+(double)(114*db),(double)(meter_height-20));
        cairo_stroke(cr);

        cairo_move_to(cr, offset+(double)(54*db)-3.0, (double)meter_height-1);
        cairo_show_text(cr, "9");
        cairo_move_to(cr, offset+(double)(74*db)-12.0, (double)meter_height-1);
        cairo_show_text(cr, "+20");
        cairo_move_to(cr, offset+(double)(94*db)-9.0, (double)meter_height-1);
        cairo_show_text(cr, "+40");
        cairo_move_to(cr, offset+(double)(114*db)-6.0, (double)meter_height-1);
        cairo_show_text(cr, "+60");

        // if frequency > 30MHz then -93 is S9
        double l=fmax(-127.0,level);
        if(vfo[active_receiver->id].frequency>30000000LL) {
          l=level+20.0;
        }
        
        //cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
	// use gradient to draw green below S9 else red
	cairo_pattern_t *pat=cairo_pattern_create_linear(0.0,0.0,114.0,0.0);
	cairo_pattern_add_color_stop_rgb(pat,0.0,0.0,1.0,0.0); // Green
	cairo_pattern_add_color_stop_rgb(pat,0.5,0.0,1.0,0.0); // Green
	cairo_pattern_add_color_stop_rgb(pat,0.5,1.0,0.0,0.0); // Red
	cairo_pattern_add_color_stop_rgb(pat,1.0,1.0,0.0,0.0); // Red
        cairo_set_source(cr, pat);

        cairo_rectangle(cr, offset+0.0, (double)(meter_height-40), (double)((l+127.0)*db), 20.0);
        cairo_fill(cr);
	cairo_pattern_destroy(pat);

        if(l>max_level || max_count==10) {
          max_level=l;
          max_count=0;
        }

        if(l!=0) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
          cairo_move_to(cr,offset+(double)((max_level+127.0)*db),(double)meter_height-20);
          cairo_line_to(cr,offset+(double)((max_level+127.0)*db),(double)(meter_height-40));
        }


        cairo_stroke(cr);

        max_count++;


        text_location=offset+(db*114)+5;
      }

      cairo_set_font_size(cr, 12);
      sprintf(sf,"%d dBm",(int)level);
      cairo_move_to(cr, text_location, meter_height-12);
      cairo_show_text(cr, sf);
      break;
    case POWER:
      cairo_select_font_face(cr, "FreeMono",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 12);

      if(level>max_level || max_count==10) {
          max_level=level;
          max_count=0;
      }
      max_count++;

      sprintf(sf,"FWD: %0.1f %s",max_level,units);
      cairo_move_to(cr, 10, 35);
      cairo_show_text(cr, sf);

      double swr;
      if (max_level > reverse) {
        swr=(max_level+reverse)/(max_level-reverse);
      } else {
        swr=999.9;
      }
      cairo_select_font_face(cr, "FreeMono",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 12);
      sprintf(sf,"SWR: %1.1f:1",swr);
      cairo_move_to(cr, 10, 55);
      cairo_show_text(cr, sf);

      sprintf(sf,"ALC: %2.1f dB",alc);
      cairo_move_to(cr, meter_width/2, 35);
      cairo_show_text(cr, sf);
      break;
  }

}
  cairo_destroy(cr);
  gtk_widget_queue_draw (meter);
}
