#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "meter.h"

static GtkWidget *meter;
static cairo_surface_t *meter_surface = NULL;

static int meter_width;
static int meter_height;

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


void meter_update(int meter_type,double value,double reverse) {
  
  char sf[32];
  int text_location;
  cairo_t *cr;
  cr = cairo_create (meter_surface);

  // clear the meter
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  cairo_set_source_rgb(cr, 1, 1, 1);
  switch(meter_type) {
    case SMETER:
      // value is dBm
      text_location=10;
      cairo_select_font_face(cr, "Arial",
                    CAIRO_FONT_SLANT_NORMAL,
                    CAIRO_FONT_WEIGHT_BOLD);
      double level=value+(double)get_attenuation();
      if(meter_width>=114) {
        int db=meter_width/114; // S9+60 (9*6)+60
        int i;
        cairo_set_line_width(cr, 1.0);
        cairo_set_source_rgb(cr, 1, 1, 1);
        for(i=0;i<54;i++) {
          cairo_move_to(cr,(double)(i*db),(double)meter_height-20);
          if(i%18==0) {
            cairo_line_to(cr,(double)(i*db),(double)(meter_height-30));
          } else if(i%6==0) {
            cairo_line_to(cr,(double)(i*db),(double)(meter_height-25));
          }
        }
        cairo_stroke(cr);

        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, (double)(18*db)-3.0, (double)meter_height);
        cairo_show_text(cr, "3");
        cairo_move_to(cr, (double)(36*db)-3.0, (double)meter_height);
        cairo_show_text(cr, "6");

        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_move_to(cr,(double)(54*db),(double)meter_height-20);
        cairo_line_to(cr,(double)(54*db),(double)(meter_height-30));
        cairo_move_to(cr,(double)(74*db),(double)meter_height-20);
        cairo_line_to(cr,(double)(74*db),(double)(meter_height-30));
        cairo_move_to(cr,(double)(94*db),(double)meter_height-20);
        cairo_line_to(cr,(double)(94*db),(double)(meter_height-30));
        cairo_move_to(cr,(double)(114*db),(double)meter_height-20);
        cairo_line_to(cr,(double)(114*db),(double)(meter_height-30));
        cairo_stroke(cr);

        cairo_move_to(cr, (double)(54*db)-3.0, (double)meter_height);
        cairo_show_text(cr, "9");
        cairo_move_to(cr, (double)(74*db)-12.0, (double)meter_height);
        cairo_show_text(cr, "+20");
        cairo_move_to(cr, (double)(94*db)-9.0, (double)meter_height);
        cairo_show_text(cr, "+40");
        cairo_move_to(cr, (double)(114*db)-6.0, (double)meter_height);
        cairo_show_text(cr, "+60");

        cairo_set_source_rgb(cr, 0, 1, 0);
        cairo_move_to(cr,(double)((level+127.0)*db),(double)meter_height-30);
        cairo_line_to(cr,(double)((level+127.0)*db),(double)(meter_height-50));
        cairo_stroke(cr);

        text_location=(db*114)+5;
      }

      cairo_set_font_size(cr, 16);
      sprintf(sf,"%d dBm",(int)level);
      cairo_move_to(cr, text_location, 45);
      cairo_show_text(cr, sf);
      break;
    case POWER:
      // value is Watts
      cairo_select_font_face(cr, "Arial",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 18);

      sprintf(sf,"FWD: %3.2f W",value);
      cairo_move_to(cr, 10, 25);
      cairo_show_text(cr, sf);

      // value is Watts
      double swr=(value+reverse)/(value-reverse);
      cairo_select_font_face(cr, "Arial",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 18);

      sprintf(sf,"SWR: %1.2f:1",swr);
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
