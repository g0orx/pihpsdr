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
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

#include "new_menu.h"
#include "oc_menu.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
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
}

static void oc_rx_cb(GtkWidget *widget, gpointer data) {
  int b=((int)data)>>4;
  int oc=((int)data)&0xF;
  BAND *band=band_get_band(b);
  int mask=0x01<<oc;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    band->OCrx|=mask;
  } else {
    band->OCrx&=~mask;
  }
}

static void oc_tx_cb(GtkWidget *widget, gpointer data) {
  int b=((int)data)>>4;
  int oc=((int)data)&0xF;
  BAND *band=band_get_band(b);
  int mask=0x01<<oc;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    band->OCtx|=mask;
  } else {
    band->OCtx&=~mask;
  }
}

static void oc_tune_cb(GtkWidget *widget, gpointer data) {
  int oc=((int)data)&0xF;
  int mask=0x01<<oc;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    OCtune|=mask;
  } else {
    OCtune&=~mask;
  }
}

static void oc_full_tune_time_cb(GtkWidget *widget, gpointer data) {
  OCfull_tune_time=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void oc_memory_tune_time_cb(GtkWidget *widget, gpointer data) {
  OCmemory_tune_time=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

void oc_menu(GtkWidget *parent) {
  int i,j;

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

  GtkWidget *close_b=gtk_button_new_with_label("Close OC");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *band_title=gtk_label_new("Band");
  //gtk_widget_override_font(band_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(band_title);
  gtk_grid_attach(GTK_GRID(grid),band_title,0,1,1,1);

  GtkWidget *rx_title=gtk_label_new("Rx");
  //gtk_widget_override_font(rx_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(rx_title);
  gtk_grid_attach(GTK_GRID(grid),rx_title,4,1,1,1);

  GtkWidget *tx_title=gtk_label_new("Tx");
  //gtk_widget_override_font(tx_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(tx_title);
  gtk_grid_attach(GTK_GRID(grid),tx_title,11,1,1,1);

  GtkWidget *tune_title=gtk_label_new("Tune (ORed with TX)");
  //gtk_widget_override_font(tune_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(tune_title);
  gtk_grid_attach(GTK_GRID(grid),tune_title,18,1,2,1);

  for(i=1;i<8;i++) {
    char oc_id[8];
    sprintf(oc_id,"%d",i);
    GtkWidget *oc_rx_title=gtk_label_new(oc_id);
    //gtk_widget_override_font(oc_rx_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(oc_rx_title);
    gtk_grid_attach(GTK_GRID(grid),oc_rx_title,i,2,1,1);
    GtkWidget *oc_tx_title=gtk_label_new(oc_id);
    //gtk_widget_override_font(oc_tx_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(oc_tx_title);
    gtk_grid_attach(GTK_GRID(grid),oc_tx_title,i+7,2,1,1);
  }

  for(i=0;i<HAM_BANDS;i++) {
    BAND *band=band_get_band(i);

    GtkWidget *band_label=gtk_label_new(band->title);
    //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(band_label);
    gtk_grid_attach(GTK_GRID(grid),band_label,0,i+3,1,1);

    int mask;
    for(j=1;j<8;j++) {
      mask=0x01<<j;
      GtkWidget *oc_rx_b=gtk_check_button_new();
      //gtk_widget_override_font(oc_rx_b, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_rx_b), (band->OCrx&mask)==mask);
      gtk_widget_show(oc_rx_b);
      gtk_grid_attach(GTK_GRID(grid),oc_rx_b,j,i+3,1,1);
      g_signal_connect(oc_rx_b,"toggled",G_CALLBACK(oc_rx_cb),(gpointer)(j+(i<<4)));

      GtkWidget *oc_tx_b=gtk_check_button_new();
      //gtk_widget_override_font(oc_tx_b, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_tx_b), (band->OCtx&mask)==mask);
      gtk_widget_show(oc_tx_b);
      gtk_grid_attach(GTK_GRID(grid),oc_tx_b,j+7,i+3,1,1);
      g_signal_connect(oc_tx_b,"toggled",G_CALLBACK(oc_tx_cb),(gpointer)(j+(i<<4)));

    }
  }

  int mask;
  for(j=1;j<8;j++) {
    char oc_id[8];
    sprintf(oc_id,"%d",j);
    GtkWidget *oc_tune_title=gtk_label_new(oc_id);
    //gtk_widget_override_font(oc_tune_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(oc_tune_title);
    gtk_grid_attach(GTK_GRID(grid),oc_tune_title,18,j+1,1,1);

    mask=0x01<<j;
    GtkWidget *oc_tune_b=gtk_check_button_new();
    //gtk_widget_override_font(oc_tune_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_tune_b), (OCtune&mask)==mask);
    gtk_widget_show(oc_tune_b);
    gtk_grid_attach(GTK_GRID(grid),oc_tune_b,19,j+1,1,1);
    g_signal_connect(oc_tune_b,"toggled",G_CALLBACK(oc_tune_cb),(gpointer)j);
  }

  GtkWidget *oc_full_tune_time_title=gtk_label_new("Full Tune(ms):");
  //gtk_widget_override_font(oc_full_tune_time_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(oc_full_tune_time_title);
  gtk_grid_attach(GTK_GRID(grid),oc_full_tune_time_title,18,j+1,2,1);
  j++;

  GtkWidget *oc_full_tune_time_b=gtk_spin_button_new_with_range(0.0,9999.0,1.0);
  //gtk_widget_override_font(oc_full_tune_time_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(oc_full_tune_time_b),(double)OCfull_tune_time);
  gtk_widget_show(oc_full_tune_time_b);
  gtk_grid_attach(GTK_GRID(grid),oc_full_tune_time_b,18,j+1,2,1);
  g_signal_connect(oc_full_tune_time_b,"value_changed",G_CALLBACK(oc_full_tune_time_cb),NULL);
  j++;

  GtkWidget *oc_memory_tune_time_title=gtk_label_new("Memory Tune(ms):");
  //gtk_widget_override_font(oc_memory_tune_time_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(oc_memory_tune_time_title);
  gtk_grid_attach(GTK_GRID(grid),oc_memory_tune_time_title,18,j+1,2,1);
  j++;

  GtkWidget *oc_memory_tune_time_b=gtk_spin_button_new_with_range(0.0,9999.0,1.0);
  //gtk_widget_override_font(oc_memory_tune_time_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(oc_memory_tune_time_b),(double)OCmemory_tune_time);
  gtk_widget_show(oc_memory_tune_time_b);
  gtk_grid_attach(GTK_GRID(grid),oc_memory_tune_time_b,18,j+1,2,1);
  g_signal_connect(oc_memory_tune_time_b,"value_changed",G_CALLBACK(oc_memory_tune_time_cb),NULL);
  j++;

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

