/*
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
#include <string.h>

#include "audio.h"
#include "new_menu.h"
#include "radio.h"
#include "sliders.h"
#include "transmitter.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *last_filter;

static GtkWidget *linein_b;
static GtkWidget *micboost_b;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static void tx_spin_low_cb (GtkWidget *widget, gpointer data) {
  tx_filter_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  tx_set_filter(transmitter,tx_filter_low,tx_filter_high);
}

static void tx_spin_high_cb (GtkWidget *widget, gpointer data) {
  tx_filter_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  tx_set_filter(transmitter,tx_filter_low,tx_filter_high);
}

static void micboost_cb(GtkWidget *widget, gpointer data) {
  mic_boost=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void linein_cb(GtkWidget *widget, gpointer data) {
  mic_linein=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  g_idle_add(linein_changed,NULL);
}

static void local_microphone_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if(audio_open_input()==0) {
      transmitter->local_microphone=1;
      gtk_widget_hide(linein_b);
      gtk_widget_hide(micboost_b);
    } else {
      transmitter->local_microphone=0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
      gtk_widget_show(linein_b);
      gtk_widget_show(micboost_b);
    }
  } else {
    if(transmitter->local_microphone) {
      transmitter->local_microphone=0;
      audio_close_input();
      gtk_widget_show(linein_b);
      gtk_widget_show(micboost_b);
    }
  }
}

static void local_input_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->input_device=(int)(long)data;
  if(transmitter->local_microphone) {
    audio_close_input();
    if(audio_open_input()==0) {
      transmitter->local_microphone=1;
    } else {
      transmitter->local_microphone=0;
    }
  }
}

static gboolean emp_cb (GtkWidget *widget, gpointer data) {
  pre_emphasize=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  tx_set_pre_emphasize(transmitter,pre_emphasize);
}

void tx_menu(GtkWidget *parent) {
  GtkWidget *b;
  int i;

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

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  int row=0;
  int col=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  col++;

  GtkWidget *label=gtk_label_new("Transmit");
  gtk_grid_attach(GTK_GRID(grid),label,col,row,1,1);

  row++;
  col=0;


  label=gtk_label_new("Filter: ");
  gtk_grid_attach(GTK_GRID(grid),label,col,row,1,1);

  col++;

  GtkWidget *tx_spin_low=gtk_spin_button_new_with_range(0.0,8000.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_spin_low),(double)tx_filter_low);
  gtk_grid_attach(GTK_GRID(grid),tx_spin_low,col,row,1,1);
  g_signal_connect(tx_spin_low,"value-changed",G_CALLBACK(tx_spin_low_cb),NULL);

  col++;

  GtkWidget *tx_spin_high=gtk_spin_button_new_with_range(0.0,8000.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_spin_high),(double)tx_filter_high);
  gtk_grid_attach(GTK_GRID(grid),tx_spin_high,col,row,1,1);
  g_signal_connect(tx_spin_high,"value-changed",G_CALLBACK(tx_spin_high_cb),NULL);

  row++;
  col=0;

#ifdef RADIOBERRY
	if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL || protocol==RADIOBERRY_PROTOCOL) {
#else
	if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
#endif
    linein_b=gtk_check_button_new_with_label("Mic Line In (ACC connector)");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (linein_b), mic_linein);
    gtk_grid_attach(GTK_GRID(grid),linein_b,col,++row,3,1);
    g_signal_connect(linein_b,"toggled",G_CALLBACK(linein_cb),NULL);

    micboost_b=gtk_check_button_new_with_label("Mic Boost (radio only)");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (micboost_b), mic_boost);
    gtk_grid_attach(GTK_GRID(grid),micboost_b,col,++row,3,1);
    g_signal_connect(micboost_b,"toggled",G_CALLBACK(micboost_cb),NULL);

  }

  if(n_input_devices>0) {
    GtkWidget *local_microphone_b=gtk_check_button_new_with_label("Local Microphone Input");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_microphone_b), transmitter->local_microphone);
    gtk_widget_show(local_microphone_b);
    gtk_grid_attach(GTK_GRID(grid),local_microphone_b,col,++row,3,1);
    g_signal_connect(local_microphone_b,"toggled",G_CALLBACK(local_microphone_cb),NULL);

    if(transmitter->input_device==-1) transmitter->input_device=0;

    for(i=0;i<n_input_devices;i++) {
      GtkWidget *input;
      if(i==0) {
        input=gtk_radio_button_new_with_label(NULL,input_devices[i]);
      } else {
        input=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(input),input_devices[i]);
      }
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (input), transmitter->input_device==i);
      gtk_widget_show(input);
      gtk_grid_attach(GTK_GRID(grid),input,col,++row,3,1);
      g_signal_connect(input,"pressed",G_CALLBACK(local_input_changed_cb),(gpointer *)i);
    }
  }

  row++;
  col=0;

  GtkWidget *emp_b=gtk_check_button_new_with_label("FM TX Pre-emphasize before limiting");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (emp_b), pre_emphasize);
  gtk_widget_show(emp_b);
  gtk_grid_attach(GTK_GRID(grid),emp_b,col,row,3,1);
  g_signal_connect(emp_b,"toggled",G_CALLBACK(emp_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

#ifdef RADIOBERRY
	if(transmitter->local_microphone && (protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL || protocol==RADIOBERRY_PROTOCOL)) {
#else
	if(transmitter->local_microphone && (protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL)) {
#endif
    gtk_widget_hide(linein_b);
    gtk_widget_hide(micboost_b);
  }

}
