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
#include <string.h>
#include <pthread.h>

#include "new_menu.h"
#include "radio.h"
#include "transmitter.h"
#include "vox_menu.h"
#include "vox.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *level;

GThread *level_thread_id;
static int run_level=0;
static double peak=0.0;

static int level_update(void *data) {
//fprintf(stderr,"vox peak=%f\n",peak);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(level),peak);
  return 0;
}

static void *level_thread(void* arg) {
  while(run_level) {
    peak=vox_get_peak();
    g_idle_add(level_update,NULL);
    usleep(100000); // 100ms
  }
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static void start_level_thread() {
  int rc;
  run_level=1;
  level_thread_id = g_thread_new( "VOX level", level_thread, NULL);
  if(!level_thread_id ) {
    fprintf(stderr,"g_thread_new failed on level_thread\n");
  }
  fprintf(stderr, "level_thread: id=%p\n",level_thread_id);
}

static void destroy_cb(GtkWidget *widget, gpointer data) {
  run_level=0;
  vox_setting=0;
}

static void vox_value_changed_cb(GtkWidget *widget, gpointer data) {
  vox_threshold=gtk_range_get_value(GTK_RANGE(widget))/1000.0;
}

static void vox_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  vox_gain=gtk_range_get_value(GTK_RANGE(widget));
}

static void vox_hang_value_changed_cb(GtkWidget *widget, gpointer data) {
  vox_hang=gtk_range_get_value(GTK_RANGE(widget));
}

void vox_menu(GtkWidget *parent) {
  int i;

  parent_window=parent;

  dialog=gtk_dialog_new();
  g_signal_connect (dialog, "destroy", G_CALLBACK(destroy_cb), NULL);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);

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

  GtkWidget *close_b=gtk_button_new_with_label("Close VOX");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *level_label=gtk_label_new("Mic Level:");
  gtk_misc_set_alignment (GTK_MISC(level_label), 0, 0);
  gtk_widget_show(level_label);
  gtk_grid_attach(GTK_GRID(grid),level_label,0,1,1,1);

  level=gtk_progress_bar_new();
  gtk_widget_set_size_request (level, 300, 25);
  gtk_widget_show(level);
  gtk_grid_attach(GTK_GRID(grid),level,1,1,1,1);

  GtkWidget *threshold_label=gtk_label_new("VOX Threshold:");
  gtk_misc_set_alignment (GTK_MISC(threshold_label), 0, 0);
  gtk_widget_show(threshold_label);
  gtk_grid_attach(GTK_GRID(grid),threshold_label,0,2,1,1);

  GtkWidget *vox_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0,1000.0,1.0);
  gtk_widget_set_size_request (vox_scale, 300, 25);
  gtk_range_set_value(GTK_RANGE(vox_scale),vox_threshold*1000.0);
  gtk_widget_show(vox_scale);
  gtk_grid_attach(GTK_GRID(grid),vox_scale,1,2,1,1);
  g_signal_connect(G_OBJECT(vox_scale),"value_changed",G_CALLBACK(vox_value_changed_cb),NULL);
  
  GtkWidget *hang_label=gtk_label_new("VOX Hang (ms):");
  gtk_misc_set_alignment (GTK_MISC(hang_label), 0, 0);
  gtk_widget_show(hang_label);
  gtk_grid_attach(GTK_GRID(grid),hang_label,0,4,1,1);

  GtkWidget *vox_hang_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0,1000.0,1.0);
  gtk_range_set_value(GTK_RANGE(vox_hang_scale),vox_hang);
  gtk_widget_show(vox_hang_scale);
  gtk_grid_attach(GTK_GRID(grid),vox_hang_scale,1,4,1,1);
  g_signal_connect(G_OBJECT(vox_hang_scale),"value_changed",G_CALLBACK(vox_hang_value_changed_cb),NULL);
  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

  if(!vox_enabled) {
    vox_setting=1;
  }
  start_level_thread();

}

