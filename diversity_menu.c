/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "new_menu.h"
#include "diversity_menu.h"
#include "radio.h"
#include <wdsp.h>

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *level;

static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

static void diversity_cb(GtkWidget *widget, gpointer data) {
  diversity_enabled=diversity_enabled==1?0:1;
  SetEXTDIVRun(0,diversity_enabled);
}

static void i_rotate_value_changed_cb(GtkWidget *widget, gpointer data) {
  i_rotate[1]=gtk_range_get_value(GTK_RANGE(widget));
  SetEXTDIVRotate (0, 2, &i_rotate[0], &q_rotate[0]);
}

static void q_rotate_value_changed_cb(GtkWidget *widget, gpointer data) {
  q_rotate[1]=gtk_range_get_value(GTK_RANGE(widget));
  SetEXTDIVRotate (0, 2, &i_rotate[0], &q_rotate[0]);
}

void diversity_menu(GtkWidget *parent) {
  int i;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Diversity");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *diversity_b=gtk_check_button_new_with_label("Diversity Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (diversity_b), diversity_enabled);
  gtk_widget_show(diversity_b);
  gtk_grid_attach(GTK_GRID(grid),diversity_b,0,1,1,1);
  g_signal_connect(diversity_b,"toggled",G_CALLBACK(diversity_cb),NULL);


  GtkWidget *i_rotate_label=gtk_label_new("I Rotate:");
  gtk_misc_set_alignment (GTK_MISC(i_rotate_label), 0, 0);
  gtk_widget_show(i_rotate_label);
  gtk_grid_attach(GTK_GRID(grid),i_rotate_label,0,2,1,1);

  GtkWidget *i_rotate_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0,1.0,0.01);
  gtk_widget_set_size_request (i_rotate_scale, 300, 25);
  gtk_range_set_value(GTK_RANGE(i_rotate_scale),i_rotate[1]);
  gtk_widget_show(i_rotate_scale);
  gtk_grid_attach(GTK_GRID(grid),i_rotate_scale,1,2,1,1);
  g_signal_connect(G_OBJECT(i_rotate_scale),"value_changed",G_CALLBACK(i_rotate_value_changed_cb),NULL);

  GtkWidget *q_rotate_label=gtk_label_new("Q Rotate:");
  gtk_misc_set_alignment (GTK_MISC(q_rotate_label), 0, 0);
  gtk_widget_show(q_rotate_label);
  gtk_grid_attach(GTK_GRID(grid),q_rotate_label,0,3,1,1);

  GtkWidget *q_rotate_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0,1.0,0.01);
  gtk_widget_set_size_request (q_rotate_scale, 300, 25);
  gtk_range_set_value(GTK_RANGE(q_rotate_scale),q_rotate[1]);
  gtk_widget_show(q_rotate_scale);
  gtk_grid_attach(GTK_GRID(grid),q_rotate_scale,1,3,1,1);
  g_signal_connect(G_OBJECT(q_rotate_scale),"value_changed",G_CALLBACK(q_rotate_value_changed_cb),NULL);

  
  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

