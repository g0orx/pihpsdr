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

#include "new_menu.h"
#include "equalizer_menu.h"
#include "radio.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static gboolean enable_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  enable_rx_equalizer=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  // update WDSP
}

static gboolean rx_value_changed_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  int i=(int)data;
  rx_equalizer[i]=gtk_range_get_value(GTK_RANGE(widget));
  // update WDSP
}

void equalizer_menu(GtkWidget *parent) {
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
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);

  GtkWidget *close_b=gtk_button_new_with_label("Close TX Equalizer");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);


  GtkWidget *enable_b=gtk_check_button_new_with_label("Enable TX Equalizer");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_b), enable_rx_equalizer);
  g_signal_connect(enable_b,"toggled-changed",G_CALLBACK(enable_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),enable_b,0,1,1,1);


  GtkWidget *label=gtk_label_new("Preamp");
  gtk_grid_attach(GTK_GRID(grid),label,0,2,1,1);

  label=gtk_label_new("Low");
  gtk_grid_attach(GTK_GRID(grid),label,1,2,1,1);

  label=gtk_label_new("Mid");
  gtk_grid_attach(GTK_GRID(grid),label,2,2,1,1);

  label=gtk_label_new("High");
  gtk_grid_attach(GTK_GRID(grid),label,3,2,1,1);

  GtkWidget *preamp_scale=gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL,-12.0,15.0,1.0);
  gtk_range_set_value(GTK_RANGE(preamp_scale),rx_equalizer[0]);
  g_signal_connect(preamp_scale,"value-changed",G_CALLBACK(rx_value_changed_cb),(gpointer)0);
  gtk_grid_attach(GTK_GRID(grid),preamp_scale,0,3,1,10);
  gtk_widget_set_size_request(preamp_scale,10,270);
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

  GtkWidget *low_scale=gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL,-12.0,15.0,1.0);
  gtk_range_set_value(GTK_RANGE(low_scale),rx_equalizer[1]);
  g_signal_connect(low_scale,"value-changed",G_CALLBACK(rx_value_changed_cb),(gpointer)1);
  gtk_grid_attach(GTK_GRID(grid),low_scale,1,3,1,10);
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

  GtkWidget *mid_scale=gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL,-12.0,15.0,1.0);
  gtk_range_set_value(GTK_RANGE(mid_scale),rx_equalizer[2]);
  g_signal_connect(mid_scale,"value-changed",G_CALLBACK(rx_value_changed_cb),(gpointer)2);
  gtk_grid_attach(GTK_GRID(grid),mid_scale,2,3,1,10);
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

  GtkWidget *high_scale=gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL,-12.0,15.0,1.0);
  gtk_range_set_value(GTK_RANGE(high_scale),rx_equalizer[3]);
  g_signal_connect(high_scale,"value-changed",G_CALLBACK(rx_value_changed_cb),(gpointer)3);
  gtk_grid_attach(GTK_GRID(grid),high_scale,3,3,1,10);
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

