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
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

#include "new_menu.h"
#include "fft_menu.h"
#include "radio.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static void filter_type_cb(GtkWidget *widget, gpointer data) {
  set_filter_type((int)data);
}

#ifdef SET_FILTER_SIZE
static void filter_size_cb(GtkWidget *widget, gpointer data) {
  set_filter_size((int)data);
}
#endif

void fft_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
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
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int x=0;

  GtkWidget *filter_type_label=gtk_label_new("Filter Type: ");
  gtk_grid_attach(GTK_GRID(grid),filter_type_label,x,1,1,1);
  
  GtkWidget *linear_phase=gtk_radio_button_new_with_label(NULL,"Linear Phase");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (linear_phase), receiver[0]->low_latency==0);
  gtk_grid_attach(GTK_GRID(grid),linear_phase,x,2,1,1);
  g_signal_connect(linear_phase,"pressed",G_CALLBACK(filter_type_cb),(gpointer *)0);

  GtkWidget *low_latency=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(linear_phase),"Low Latency");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (low_latency), receiver[0]->low_latency==1);
  gtk_grid_attach(GTK_GRID(grid),low_latency,x,3,1,1);
  g_signal_connect(low_latency,"pressed",G_CALLBACK(filter_type_cb),(gpointer *)1);

  x++;

#ifdef SET_FILTER_SIZE
  GtkWidget *filter_size_label=gtk_label_new("Filter Size: ");
  gtk_grid_attach(GTK_GRID(grid),filter_size_label,x,1,1,1);
  
  GtkWidget *filter_1024=gtk_radio_button_new_with_label(NULL,"1024");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_1024), receiver[0]->fft_size==1024);
  gtk_grid_attach(GTK_GRID(grid),filter_1024,x,2,1,1);
  g_signal_connect(filter_1024,"pressed",G_CALLBACK(filter_size_cb),(gpointer *)1024);

  GtkWidget *filter_2048=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filter_1024),"2048");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_2048), receiver[0]->fft_size==2048);
  gtk_grid_attach(GTK_GRID(grid),filter_2048,x,3,1,1);
  g_signal_connect(filter_2048,"pressed",G_CALLBACK(filter_size_cb),(gpointer *)2048);

  GtkWidget *filter_4096=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filter_2048),"4096");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_4096), receiver[0]->fft_size==4096);
  gtk_grid_attach(GTK_GRID(grid),filter_4096,x,4,1,1);
  g_signal_connect(filter_4096,"pressed",G_CALLBACK(filter_size_cb),(gpointer *)4096);

  GtkWidget *filter_8192=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filter_4096),"8192");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_8192), receiver[0]->fft_size==8192);
  gtk_grid_attach(GTK_GRID(grid),filter_8192,x,5,1,1);
  g_signal_connect(filter_8192,"pressed",G_CALLBACK(filter_size_cb),(gpointer *)8192);

  GtkWidget *filter_16384=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filter_8192),"16384");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_16384), receiver[0]->fft_size==16384);
  gtk_grid_attach(GTK_GRID(grid),filter_16384,x,6,1,1);
  g_signal_connect(filter_16384,"pressed",G_CALLBACK(filter_size_cb),(gpointer *)16394);

  x++;
#endif
  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

