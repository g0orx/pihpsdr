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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <wdsp.h>

#include "new_menu.h"
#include "equalizer_menu.h"
#include "radio.h"
#include "channel.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;
static GtkWidget *enable_b;
static GtkWidget *preamp_scale;
static GtkWidget *low_scale;
static GtkWidget *mid_scale;
static GtkWidget *high_scale;

static gboolean tx_menu=TRUE;

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

static gboolean tx_rb_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    tx_menu=TRUE;
    gtk_button_set_label(GTK_BUTTON(enable_b),"Enable TX Equalizer");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_b), enable_tx_equalizer);
    gtk_range_set_value(GTK_RANGE(preamp_scale),(double)tx_equalizer[0]);
    gtk_range_set_value(GTK_RANGE(low_scale),(double)tx_equalizer[1]);
    gtk_range_set_value(GTK_RANGE(mid_scale),(double)tx_equalizer[2]);
    gtk_range_set_value(GTK_RANGE(high_scale),(double)tx_equalizer[3]);
  }
  return FALSE;
}

static gboolean rx_rb_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    tx_menu=FALSE;
    gtk_button_set_label(GTK_BUTTON(enable_b),"Enable RX Equalizer");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_b), enable_rx_equalizer);
    gtk_range_set_value(GTK_RANGE(preamp_scale),(double)rx_equalizer[0]);
    gtk_range_set_value(GTK_RANGE(low_scale),(double)rx_equalizer[1]);
    gtk_range_set_value(GTK_RANGE(mid_scale),(double)rx_equalizer[2]);
    gtk_range_set_value(GTK_RANGE(high_scale),(double)rx_equalizer[3]);
  }
  return FALSE;
}

static gboolean enable_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(tx_menu) {
    enable_tx_equalizer=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    SetTXAEQRun(transmitter->id, enable_tx_equalizer);
  } else {
    enable_rx_equalizer=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    SetRXAEQRun(active_receiver->id, enable_rx_equalizer);
  }
  return FALSE;
}

static void value_changed_cb (GtkWidget *widget, gpointer data) {
  int i=GPOINTER_TO_UINT(data);
  if(tx_menu) {
    tx_equalizer[i]=(int)gtk_range_get_value(GTK_RANGE(widget));
    SetTXAGrphEQ(transmitter->id, tx_equalizer);
  } else {
    rx_equalizer[i]=(int)gtk_range_get_value(GTK_RANGE(widget));
    SetRXAGrphEQ(active_receiver->id, rx_equalizer);
  }
}

void equalizer_menu(GtkWidget *parent) {
  int i;

  if(can_transmit) {
    tx_menu=TRUE;
  } else {
    tx_menu=FALSE;
  }
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Equalizer");
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
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  if(can_transmit) {
    GtkWidget *tx_rb=gtk_radio_button_new_with_label(NULL,"TX Equalizer");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tx_rb),TRUE);
    g_signal_connect(tx_rb,"toggled",G_CALLBACK(tx_rb_cb),NULL);
    gtk_grid_attach(GTK_GRID(grid),tx_rb,1,0,1,1);

    GtkWidget *rx_rb=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(tx_rb),"RX Equalizer");
    g_signal_connect(rx_rb,"toggled",G_CALLBACK(rx_rb_cb),NULL);
    gtk_grid_attach(GTK_GRID(grid),rx_rb,2,0,1,1);
  } else {
    GtkWidget *rx_label=gtk_label_new("RX Equalizer");
    gtk_grid_attach(GTK_GRID(grid),rx_label,1,0,1,1);
  }


  if(can_transmit) {
    enable_b=gtk_check_button_new_with_label("Enable TX Equalizer");
  } else {
    enable_b=gtk_check_button_new_with_label("Enable RX Equalizer");
  }
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_b), enable_tx_equalizer);
  g_signal_connect(enable_b,"toggled",G_CALLBACK(enable_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),enable_b,0,1,1,1);


  GtkWidget *label=gtk_label_new("Preamp");
  gtk_grid_attach(GTK_GRID(grid),label,0,2,1,2);

  label=gtk_label_new("Low");
  gtk_grid_attach(GTK_GRID(grid),label,0,4,1,2);

  label=gtk_label_new("Mid");
  gtk_grid_attach(GTK_GRID(grid),label,0,6,1,2);

  label=gtk_label_new("High");
  gtk_grid_attach(GTK_GRID(grid),label,0,8,1,2);

  preamp_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0,15.0,1.0);
  if(can_transmit) {
    gtk_range_set_value(GTK_RANGE(preamp_scale),(double)tx_equalizer[0]);
  } else {
    gtk_range_set_value(GTK_RANGE(preamp_scale),(double)rx_equalizer[0]);
  }
  g_signal_connect(preamp_scale,"value-changed",G_CALLBACK(value_changed_cb),(gpointer)0);
  gtk_grid_attach(GTK_GRID(grid),preamp_scale,1,2,4,2);
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),-12.0,GTK_POS_LEFT,"-12dB");
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),-9.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),-6.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),-3.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),0.0,GTK_POS_LEFT,"0dB");
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),3.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),6.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),9.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),12.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(preamp_scale),15.0,GTK_POS_LEFT,"15dB");

  low_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0,15.0,1.0);
  if(can_transmit) {
    gtk_range_set_value(GTK_RANGE(low_scale),(double)tx_equalizer[1]);
  } else {
    gtk_range_set_value(GTK_RANGE(low_scale),(double)rx_equalizer[1]);
  }
  g_signal_connect(low_scale,"value-changed",G_CALLBACK(value_changed_cb),(gpointer)1);
  gtk_grid_attach(GTK_GRID(grid),low_scale,1,4,4,2);
  gtk_scale_add_mark(GTK_SCALE(low_scale),-12.0,GTK_POS_LEFT,"-12dB");
  gtk_scale_add_mark(GTK_SCALE(low_scale),-9.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(low_scale),-6.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(low_scale),-3.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(low_scale),0.0,GTK_POS_LEFT,"0dB");
  gtk_scale_add_mark(GTK_SCALE(low_scale),3.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(low_scale),6.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(low_scale),9.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(low_scale),12.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(low_scale),15.0,GTK_POS_LEFT,"15dB");

  mid_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0,15.0,1.0);
  if(can_transmit) {
    gtk_range_set_value(GTK_RANGE(mid_scale),(double)tx_equalizer[2]);
  } else {
    gtk_range_set_value(GTK_RANGE(mid_scale),(double)rx_equalizer[2]);
  }
  g_signal_connect(mid_scale,"value-changed",G_CALLBACK(value_changed_cb),(gpointer)2);
  gtk_grid_attach(GTK_GRID(grid),mid_scale,1,6,4,2);
  gtk_scale_add_mark(GTK_SCALE(mid_scale),-12.0,GTK_POS_LEFT,"-12dB");
  gtk_scale_add_mark(GTK_SCALE(mid_scale),-9.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(mid_scale),-6.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(mid_scale),-3.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(mid_scale),0.0,GTK_POS_LEFT,"0dB");
  gtk_scale_add_mark(GTK_SCALE(mid_scale),3.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(mid_scale),6.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(mid_scale),9.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(mid_scale),12.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(mid_scale),15.0,GTK_POS_LEFT,"15dB");

  high_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0,15.0,1.0);
  if(can_transmit) {
    gtk_range_set_value(GTK_RANGE(high_scale),(double)tx_equalizer[3]);
  } else {
    gtk_range_set_value(GTK_RANGE(high_scale),(double)rx_equalizer[3]);
  }
  g_signal_connect(high_scale,"value-changed",G_CALLBACK(value_changed_cb),(gpointer)3);
  gtk_grid_attach(GTK_GRID(grid),high_scale,1,8,4,2);
  gtk_scale_add_mark(GTK_SCALE(high_scale),-12.0,GTK_POS_LEFT,"-12dB");
  gtk_scale_add_mark(GTK_SCALE(high_scale),-9.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(high_scale),-6.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(high_scale),-3.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(high_scale),0.0,GTK_POS_LEFT,"0dB");
  gtk_scale_add_mark(GTK_SCALE(high_scale),3.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(high_scale),6.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(high_scale),9.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(high_scale),12.0,GTK_POS_LEFT,NULL);
  gtk_scale_add_mark(GTK_SCALE(high_scale),15.0,GTK_POS_LEFT,"15dB");

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);


}

