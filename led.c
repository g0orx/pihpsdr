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

#include <gtk/gtk.h>

static GtkWidget *led;

static double red=0.0;
static double green=1.0;
static double blue=0.0;


static gboolean draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data) {
  cairo_set_source_rgb(cr, red, green, blue);
  cairo_paint(cr);      
  return FALSE;
}

void led_set_color(double r,double g,double b) {
  red=r;
  green=g;
  blue=b;
  gtk_widget_queue_draw (led);
}

GtkWidget *create_led(int width,int height) {
  led=gtk_drawing_area_new();
  gtk_widget_set_size_request(led,width,height);

  g_signal_connect (led, "draw", G_CALLBACK (draw_cb), NULL);

  return led;
}
