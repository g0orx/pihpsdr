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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "new_menu.h"
#include "agc_menu.h"
#include "agc.h"
#include "band.h"
#include "channel.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "button_text.h"

static GtkWidget *parent_window=NULL;

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

static gboolean agc_select_cb (GtkWidget *widget, gpointer        data) {
  active_receiver->agc=(uintptr_t)data;
  //wdsp_set_agc(CHANNEL_RX0, agc);
  set_agc(active_receiver, active_receiver->agc);
  vfo_update();
  return FALSE;
}

void agc_menu(GtkWidget *parent) {
  GtkWidget *b;
  int i;
  BAND *band;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - AGC (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

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
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int row=1;
  int col=0;

  GtkWidget *b_off=gtk_radio_button_new_with_label(NULL,"Off");
  //gtk_widget_override_font(b_off, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_off), active_receiver->agc==AGC_OFF);
  gtk_widget_show(b_off);
  gtk_grid_attach(GTK_GRID(grid),b_off,col,row,1,1);
  g_signal_connect(b_off,"pressed",G_CALLBACK(agc_select_cb),(gpointer)(long)AGC_OFF);

  col++;

  GtkWidget *b_long=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_off),"Long");
  //gtk_widget_override_font(b_long, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_long), active_receiver->agc==AGC_LONG);
  gtk_widget_show(b_long);
  gtk_grid_attach(GTK_GRID(grid),b_long,col,row,1,1);
  g_signal_connect(b_long,"pressed",G_CALLBACK(agc_select_cb),(gpointer)(long)AGC_LONG);

  col++;

  GtkWidget *b_slow=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_long),"Slow");
  //gtk_widget_override_font(b_slow, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_slow), active_receiver->agc==AGC_SLOW);
  gtk_widget_show(b_slow);
  gtk_grid_attach(GTK_GRID(grid),b_slow,col,row,1,1);
  g_signal_connect(b_slow,"pressed",G_CALLBACK(agc_select_cb),(gpointer)(long)AGC_SLOW);

  col++;

  GtkWidget *b_medium=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_slow),"Medium");
  //gtk_widget_override_font(b_medium, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_medium), active_receiver->agc==AGC_MEDIUM);
  gtk_widget_show(b_medium);
  gtk_grid_attach(GTK_GRID(grid),b_medium,col,row,1,1);
  g_signal_connect(b_medium,"pressed",G_CALLBACK(agc_select_cb),(gpointer)(long)AGC_MEDIUM);

  col++;

  GtkWidget *b_fast=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_medium),"Fast");
  //gtk_widget_override_font(b_fast, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_fast), active_receiver->agc==AGC_FAST);
  gtk_widget_show(b_fast);
  gtk_grid_attach(GTK_GRID(grid),b_fast,col,row,1,1);
  g_signal_connect(b_fast,"pressed",G_CALLBACK(agc_select_cb),(gpointer)(long)AGC_FAST);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
