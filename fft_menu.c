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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "new_menu.h"
#include "fft_menu.h"
#include "radio.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

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

static void filter_type_cb(GtkToggleButton *widget, gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    set_filter_type(GPOINTER_TO_UINT(data));
  }
}

#ifdef SET_FILTER_SIZE
static void filter_size_cb(GtkToggleButton *widget, gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    set_filter_size((int)data);
  }
}
#endif

void fft_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - FFT");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

#ifdef BKGND
  extern GdkRGBA bkgnd_color;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&bkgnd_color);
#else
  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);
#endif

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int row=1;
  int col=0;

  GtkWidget *filter_type_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(filter_type_label), "<b>Filter Type</b>");
  gtk_grid_attach(GTK_GRID(grid),filter_type_label,col,row,1,1);
  
  row++;
  col=0;

  GtkWidget *linear_phase=gtk_radio_button_new_with_label(NULL,"Linear Phase");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (linear_phase), receiver[0]->low_latency==0);
  gtk_grid_attach(GTK_GRID(grid),linear_phase,col,row,1,1);
  g_signal_connect(linear_phase,"toggled",G_CALLBACK(filter_type_cb),(gpointer *)0);

  col++;

  GtkWidget *low_latency=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(linear_phase),"Low Latency");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (low_latency), receiver[0]->low_latency==1);
  gtk_grid_attach(GTK_GRID(grid),low_latency,col,row,1,1);
  g_signal_connect(low_latency,"toggled",G_CALLBACK(filter_type_cb),(gpointer *)1);

#ifdef SET_FILTER_SIZE

  row++;
  col=0;

  GtkWidget *filter_size_label=gtk_label_new("Filter Size: ");
  gtk_grid_attach(GTK_GRID(grid),filter_size_label,col,row,1,1);
  
  row++;
  col=0;

  GtkWidget *filter_1024=gtk_radio_button_new_with_label(NULL,"1024");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_1024), receiver[0]->fft_size==1024);
  gtk_grid_attach(GTK_GRID(grid),filter_1024,col,row,1,1);
  g_signal_connect(filter_1024,"toggled",G_CALLBACK(filter_size_cb),(gpointer *)1024);

  col++;

  GtkWidget *filter_2048=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filter_1024),"2048");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_2048), receiver[0]->fft_size==2048);
  gtk_grid_attach(GTK_GRID(grid),filter_2048,col,row,1,1);
  g_signal_connect(filter_2048,"toggled",G_CALLBACK(filter_size_cb),(gpointer *)2048);

  col++;

  GtkWidget *filter_4096=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filter_1024),"4096");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_4096), receiver[0]->fft_size==4096);
  gtk_grid_attach(GTK_GRID(grid),filter_4096,col,row,1,1);
  g_signal_connect(filter_4096,"toggled",G_CALLBACK(filter_size_cb),(gpointer *)4096);

  col++;

  GtkWidget *filter_8192=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filter_1024),"8192");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_8192), receiver[0]->fft_size==8192);
  gtk_grid_attach(GTK_GRID(grid),filter_8192,col,row,1,1);
  g_signal_connect(filter_8192,"toggled",G_CALLBACK(filter_size_cb),(gpointer *)8192);

  col++;

  GtkWidget *filter_16384=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filter_1024),"16384");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filter_16384), receiver[0]->fft_size==16384);
  gtk_grid_attach(GTK_GRID(grid),filter_16384,col,row,1,1);
  g_signal_connect(filter_16384,"toggled",G_CALLBACK(filter_size_cb),(gpointer *)16394);

#endif
  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

