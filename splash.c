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
#include "version.h"

GtkWidget *splash_window;
GtkWidget *status;
static cairo_surface_t *splash_surface = NULL;

/* Close the splash screen */
void splash_close()
{
  gtk_widget_destroy(splash_window);
}

static gboolean splash_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  if (splash_surface)
    cairo_surface_destroy (splash_surface);

  splash_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));

  return TRUE;
}


void splash_show(char* image_name,int width,int height,int full_screen)
{
  GtkWidget  *image;
  splash_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  if(full_screen) {
    gtk_window_fullscreen(GTK_WINDOW(splash_window));
  }
  gtk_widget_set_size_request(splash_window, width, height);
  gtk_window_set_position(GTK_WINDOW(splash_window),GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable(GTK_WINDOW(splash_window), FALSE);


  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);

  image=gtk_image_new_from_file(image_name);
  //gtk_container_add(GTK_CONTAINER(splash_window), image);
  gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 4);
  g_signal_connect (splash_window,"configure-event",
            G_CALLBACK (splash_configure_event_cb), NULL);

  char build[64];
  sprintf(build,"build: %s %s",build_date, build_version);

  GtkWidget *pi_label=gtk_label_new("pihpsdr by John Melton g0orx/n6lyt");
  gtk_label_set_justify(GTK_LABEL(pi_label),GTK_JUSTIFY_LEFT);
  gtk_widget_show(pi_label);
  gtk_grid_attach(GTK_GRID(grid),pi_label,1,0,1,1);
  GtkWidget *build_date_label=gtk_label_new(build);
  gtk_label_set_justify(GTK_LABEL(build_date_label),GTK_JUSTIFY_LEFT);
  gtk_widget_show(build_date_label);
  gtk_grid_attach(GTK_GRID(grid),build_date_label,1,1,1,1);

  status=gtk_label_new("");
  gtk_label_set_justify(GTK_LABEL(status),GTK_JUSTIFY_LEFT);
  gtk_widget_override_font(status, pango_font_description_from_string("FreeMono 18"));
  gtk_widget_show(status);
  //gtk_container_add(GTK_CONTAINER(splash_window), status);
  gtk_grid_attach(GTK_GRID(grid), status, 1, 3, 1, 1);

  gtk_container_add(GTK_CONTAINER(splash_window), grid);
  gtk_widget_show_all (splash_window);
}

void splash_status(char *text) {
  //fprintf(stderr,"splash_status: %s\n",text);
  gtk_label_set_text(GTK_LABEL(status),text);
  usleep(10000);
  while (gtk_events_pending ())
    gtk_main_iteration ();
}
