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

GtkWidget *grid;
GtkWidget *status;
static cairo_surface_t *splash_surface = NULL;


/* Close the splash screen */
void splash_close()
{
  gtk_widget_destroy(grid);
}

GtkWidget *splash_create(char* image_name,int width,int height)
{
  GtkWidget  *image;

  grid = gtk_grid_new();
  gtk_widget_set_size_request(grid, width, height);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);

  image=gtk_image_new_from_file(image_name);
  gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 4);

  char build[64];
  sprintf(build,"build: %s %s",build_date, version);

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
  gtk_grid_attach(GTK_GRID(grid), status, 1, 3, 1, 1);

  return grid;
}

void splash_status(char *text) {
  //fprintf(stderr,"splash_status: %s\n",text);
  gtk_label_set_text(GTK_LABEL(status),text);
  usleep(10000);
  while (gtk_events_pending ())
    gtk_main_iteration ();
}
