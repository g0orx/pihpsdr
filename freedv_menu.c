/*
* Copyright (C)
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
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <codec2/freedv_api.h>
#include <wdsp.h>

#include "new_menu.h"
#include "freedv_menu.h"
#include "freedv.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "ext.h"

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

static void freedv_text_changed_cb(GtkWidget *widget, gpointer data) {
  strcpy(transmitter->freedv_text_data,gtk_entry_get_text(GTK_ENTRY(widget)));
}

static void enable_cb(GtkWidget *widget, gpointer data) {
  set_freedv(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void sq_enable_cb(GtkWidget *widget, gpointer data) {
  freedv_set_sq_enable(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void sq_spin_cb (GtkWidget *widget, gpointer data) {
  freedv_set_sq_threshold(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
}

static void audio_spin_cb (GtkWidget *widget, gpointer data) {
  freedv_set_audio_gain(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
}

static void mode_cb(GtkWidget *widget, gpointer data) {
  int mode=(uintptr_t)data;
  freedv_set_mode(active_receiver,mode);
}

void freedv_menu(GtkWidget *parent) {
  int i,j;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - FreeDV");
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
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *freedv_text_label=gtk_label_new("Tx Message: ");
  gtk_grid_attach(GTK_GRID(grid),freedv_text_label,0,1,1,1);

  GtkWidget *freedv_text=gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(freedv_text),transmitter->freedv_text_data);
  gtk_grid_attach(GTK_GRID(grid),freedv_text,1,1,3,1);
  g_signal_connect(freedv_text,"changed",G_CALLBACK(freedv_text_changed_cb),NULL);


  GtkWidget *enable=gtk_check_button_new_with_label("Enable FreeDV");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable), active_receiver->freedv);
  gtk_grid_attach(GTK_GRID(grid),enable,0,2,1,1);
  g_signal_connect(enable,"toggled",G_CALLBACK(enable_cb),NULL);

  GtkWidget *sq_enable=gtk_check_button_new_with_label("SNR Squelch Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sq_enable), freedv_sq_enable);
  gtk_grid_attach(GTK_GRID(grid),sq_enable,1,2,1,1);
  g_signal_connect(sq_enable,"toggled",G_CALLBACK(sq_enable_cb),NULL);

  GtkWidget *sq_spin=gtk_spin_button_new_with_range(-200.0,0.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sq_spin),freedv_snr_sq_threshold);
  gtk_grid_attach(GTK_GRID(grid),sq_spin,2,2,1,1);
  g_signal_connect(sq_spin,"value-changed",G_CALLBACK(sq_spin_cb),NULL);

  GtkWidget *mode_1600=gtk_radio_button_new_with_label(NULL,"1600");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_1600), freedv_mode==FREEDV_MODE_1600);
  gtk_widget_show(mode_1600);
  gtk_grid_attach(GTK_GRID(grid),mode_1600,0,4,1,1);
  g_signal_connect(mode_1600,"pressed",G_CALLBACK(mode_cb),(gpointer *)FREEDV_MODE_1600);

  GtkWidget *mode_700=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(mode_1600),"700");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_700), freedv_mode==FREEDV_MODE_700);
  gtk_widget_show(mode_700);
  gtk_grid_attach(GTK_GRID(grid),mode_700,1,4,1,1);
  g_signal_connect(mode_700,"pressed",G_CALLBACK(mode_cb),(gpointer *)FREEDV_MODE_700);

  GtkWidget *mode_700B=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(mode_700),"700B");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_700B), freedv_mode==FREEDV_MODE_700B);
  gtk_widget_show(mode_700B);
  gtk_grid_attach(GTK_GRID(grid),mode_700B,2,4,1,1);
  g_signal_connect(mode_700B,"pressed",G_CALLBACK(mode_cb),(gpointer *)FREEDV_MODE_700B);

  GtkWidget *mode_2400A=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(mode_700B),"2400A");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_2400A), freedv_mode==FREEDV_MODE_2400A);
  gtk_widget_show(mode_2400A);
  gtk_grid_attach(GTK_GRID(grid),mode_2400A,0,5,1,1);
  g_signal_connect(mode_2400A,"pressed",G_CALLBACK(mode_cb),(gpointer *)FREEDV_MODE_2400A);

  GtkWidget *mode_2400B=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(mode_2400A),"2400B");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_2400B), freedv_mode==FREEDV_MODE_2400B);
  gtk_widget_show(mode_2400B);
  gtk_grid_attach(GTK_GRID(grid),mode_2400B,1,5,1,1);
  g_signal_connect(mode_2400B,"pressed",G_CALLBACK(mode_cb),(gpointer *)FREEDV_MODE_2400B);

  GtkWidget *mode_800XA=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(mode_2400B),"800XA");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_800XA), freedv_mode==FREEDV_MODE_800XA);
  gtk_widget_show(mode_800XA);
  gtk_grid_attach(GTK_GRID(grid),mode_800XA,2,5,1,1);
  g_signal_connect(mode_800XA,"pressed",G_CALLBACK(mode_cb),(gpointer *)FREEDV_MODE_800XA);

  GtkWidget *mode_700C=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(mode_800XA),"700C");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_700C), freedv_mode==FREEDV_MODE_700C);
  gtk_widget_show(mode_700C);
  gtk_grid_attach(GTK_GRID(grid),mode_700C,3,5,1,1);
  g_signal_connect(mode_700C,"pressed",G_CALLBACK(mode_cb),(gpointer *)FREEDV_MODE_700C);

  GtkWidget *freedv_audio_label=gtk_label_new("Audio Gain: ");
  gtk_grid_attach(GTK_GRID(grid),freedv_audio_label,0,6,1,1);

  GtkWidget *audio_spin=gtk_spin_button_new_with_range(0.0,1.0,0.1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(audio_spin),freedv_audio_gain);
  gtk_grid_attach(GTK_GRID(grid),audio_spin,1,6,1,1);
  g_signal_connect(audio_spin,"value-changed",G_CALLBACK(audio_spin_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

