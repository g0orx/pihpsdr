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
#include "wdsp.h"
#include "radio.h"
#include "version.h"
#include "mode.h"
#ifdef FREEDV
#include "freedv.h"
#endif
#ifdef PSK
#include "psk.h"
#endif
#include "new_menu.h"

static GtkWidget *parent_window;

static GtkWidget *meter;
static cairo_surface_t *meter_surface = NULL;

static int meter_width;
static int meter_height;
static int last_meter_type=SMETER;
static int max_level=-200;
static int max_count=0;
static int max_reverse=0;

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

/*
static void
smeter_select_cb (GtkWidget *widget,
               gpointer        data)
{
  smeter=(int)data;
}

static void
alc_meter_select_cb (GtkWidget *widget,
               gpointer        data)
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
/*
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Meter",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

   
  GtkWidget *smeter_peak=gtk_radio_button_new_with_label(NULL,"S Meter Peak");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (smeter_peak), smeter==RXA_S_PK);
  gtk_widget_show(smeter_peak);
  gtk_grid_attach(GTK_GRID(grid),smeter_peak,0,1,1,1);
  g_signal_connect(smeter_peak,"pressed",G_CALLBACK(smeter_select_cb),(gpointer *)RXA_S_PK);

  GtkWidget *smeter_average=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(smeter_peak),"S Meter Average");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (smeter_average), smeter==RXA_S_AV);
  gtk_widget_show(smeter_average);
  gtk_grid_attach(GTK_GRID(grid),smeter_average,0,2,1,1);
  g_signal_connect(smeter_average,"pressed",G_CALLBACK(smeter_select_cb),(gpointer *)RXA_S_AV);

  GtkWidget *alc_peak=gtk_radio_button_new_with_label(NULL,"ALC Peak");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alc_peak), alc==TXA_ALC_PK);
  gtk_widget_show(alc_peak);
  gtk_grid_attach(GTK_GRID(grid),alc_peak,1,1,1,1);
  g_signal_connect(alc_peak,"pressed",G_CALLBACK(alc_meter_select_cb),(gpointer *)TXA_ALC_PK);

  GtkWidget *alc_average=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(alc_peak),"ALC Average");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alc_average), alc==TXA_ALC_AV);
  gtk_widget_show(alc_average);
  gtk_grid_attach(GTK_GRID(grid),alc_average,1,2,1,1);
  g_signal_connect(alc_average,"pressed",G_CALLBACK(alc_meter_select_cb),(gpointer *)TXA_ALC_AV);

  GtkWidget *alc_gain=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(alc_average),"ALC Gain");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alc_gain), alc==TXA_ALC_GAIN);
  gtk_widget_show(alc_gain);
  gtk_grid_attach(GTK_GRID(grid),alc_gain,1,3,1,1);
  g_signal_connect(alc_gain,"pressed",G_CALLBACK(alc_meter_select_cb),(gpointer *)TXA_ALC_GAIN);


  gtk_container_add(GTK_CONTAINER(content),grid);

  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  gtk_widget_override_font(close_button, pango_font_description_from_string("FreeMono 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));
*/
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


void meter_update(int meter_type,double value,double reverse,double exciter,double alc) {
  
  char text[128];
  char sf[32];
  int text_location;
  double offset;
  cairo_t *cr;
  cr = cairo_create (meter_surface);

  // clear the meter
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  //sprintf(text,"Version: %s %s", build_date, version);
  sprintf(text,"Version: %s", version);
  cairo_select_font_face(cr, "FreeMono",
                CAIRO_FONT_SLANT_NORMAL,
                CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
  cairo_set_font_size(cr, 10);
  cairo_move_to(cr, 5, 15);
  cairo_show_text(cr, text);

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

  cairo_set_source_rgb(cr, 1, 1, 1);
  switch(meter_type) {
    case SMETER:
      // value is dBm
      text_location=10;
      offset=5.0;
      double level=value+(double)get_attenuation();
      if(meter_width>=114) {
        //int db=meter_width/114; // S9+60 (9*6)+60
        //if(db>2) db=2;
        int db=1;
        int i;
        cairo_set_line_width(cr, 1.0);
        cairo_set_source_rgb(cr, 1, 1, 1);
        for(i=0;i<54;i++) {
          cairo_move_to(cr,offset+(double)(i*db),(double)meter_height-10);
          if(i%18==0) {
            cairo_line_to(cr,offset+(double)(i*db),(double)(meter_height-20));
          } else if(i%6==0) {
            cairo_line_to(cr,offset+(double)(i*db),(double)(meter_height-15));
          }
        }
        cairo_stroke(cr);

        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, offset+(double)(18*db)-3.0, (double)meter_height);
        cairo_show_text(cr, "3");
        cairo_move_to(cr, offset+(double)(36*db)-3.0, (double)meter_height);
        cairo_show_text(cr, "6");

        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_move_to(cr,offset+(double)(54*db),(double)meter_height-10);
        cairo_line_to(cr,offset+(double)(54*db),(double)(meter_height-20));
        cairo_move_to(cr,offset+(double)(74*db),(double)meter_height-10);
        cairo_line_to(cr,offset+(double)(74*db),(double)(meter_height-20));
        cairo_move_to(cr,offset+(double)(94*db),(double)meter_height-10);
        cairo_line_to(cr,offset+(double)(94*db),(double)(meter_height-20));
        cairo_move_to(cr,offset+(double)(114*db),(double)meter_height-10);
        cairo_line_to(cr,offset+(double)(114*db),(double)(meter_height-20));
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
        cairo_rectangle(cr, offset+0.0, (double)(meter_height-40), (double)((level+127.0)*db), 20.0);
        cairo_fill(cr);

        if(level>max_level || max_count==10) {
          max_level=level;
          max_count=0;
        }

        if(max_level!=0) {
          cairo_set_source_rgb(cr, 1, 1, 0);
          cairo_move_to(cr,offset+(double)((max_level+127.0)*db),(double)meter_height-20);
          cairo_line_to(cr,offset+(double)((max_level+127.0)*db),(double)(meter_height-40));
        }


        cairo_stroke(cr);

        max_count++;


        text_location=offset+(db*114)+5;
      }

      cairo_set_font_size(cr, 12);
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
        cairo_set_font_size(cr, 12);
        sprintf(sf,"SNR: %3.2f",freedv_snr);
        cairo_move_to(cr, text_location, 30);
        cairo_show_text(cr, sf);
      }
#endif
      break;
#ifdef PSK
    case PSKMETER:
      {
      int i;
      offset=5.0;
      cairo_set_line_width(cr, 1.0);
      cairo_set_source_rgb(cr, 1, 1, 1);
      for(i=0;i<11;i++) {
        cairo_move_to(cr,offset+(double)(i*20),(double)meter_height-10);
        if((i%2)==0) {
          cairo_line_to(cr,offset+(double)(i*20),(double)(meter_height-20));
          cairo_move_to(cr,offset+(double)(i*20),(double)meter_height);
          sprintf(sf,"%d",i*10);
          cairo_show_text(cr, sf);
        } else {
          cairo_line_to(cr,offset+(double)(i*20),(double)(meter_height-15));
        }
      }
      cairo_stroke(cr);

      cairo_set_source_rgb(cr, 0, 1, 0);
      cairo_rectangle(cr, offset+0.0, (double)(meter_height-40), value*2, 20.0);
      cairo_fill(cr);

      cairo_set_source_rgb(cr, 0, 1, 0);
      cairo_set_font_size(cr, 12);
      sprintf(sf,"Level: %d",(int)value);
      cairo_move_to(cr, 210, 45);
      cairo_show_text(cr, sf);
      }
      break;
#endif
    case POWER:
      // value is Watts
      cairo_select_font_face(cr, "FreeMono",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 12);

      if((int)value>max_level || max_count==10) {
          max_level=(int)value;
          max_count=0;
      }
      max_count++;

      sprintf(sf,"FWD: %d W",(int)max_level);
      cairo_move_to(cr, 10, 35);
      cairo_show_text(cr, sf);

      double swr=(max_level+reverse)/(max_level-reverse);
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
