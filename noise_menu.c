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
#include <string.h>

#include "new_menu.h"
#include "noise_menu.h"
#include "channel.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "mode.h"
#include "radio.h"
#include "vfo.h"
#include "button_text.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *last_filter;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static void update_noise() {
  SetRXAANRRun(active_receiver->id, active_receiver->nr);
  SetRXAEMNRRun(active_receiver->id, active_receiver->nr2);
  SetRXAANFRun(active_receiver->id, active_receiver->anf);
  SetRXASNBARun(active_receiver->id, active_receiver->snb);
  vfo_update(NULL);
}

static void nr_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr=active_receiver->nr==1?0:1;
  update_noise();
}

static void nr2_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr2=active_receiver->nr2==1?0:1;
  update_noise();
}

static void anf_cb(GtkWidget *widget, gpointer data) {
  active_receiver->anf=active_receiver->anf==1?0:1;
  update_noise();
}

static void snb_cb(GtkWidget *widget, gpointer data) {
  active_receiver->snb=active_receiver->snb==1?0:1;
  update_noise();
}

void noise_menu(GtkWidget *parent) {
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

  GtkWidget *close_b=gtk_button_new_with_label("Close Noise");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  char label[32];
  sprintf(label,"RX %d VFO %s",active_receiver->id,active_receiver->id==0?"A":"B");
  GtkWidget *rx_label=gtk_label_new(label);
  gtk_grid_attach(GTK_GRID(grid),rx_label,1,0,1,1);

  GtkWidget *b_nr=gtk_check_button_new_with_label("NR");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr), active_receiver->nr);
  gtk_widget_show(b_nr);
  gtk_grid_attach(GTK_GRID(grid),b_nr,0,2,2,1);
  g_signal_connect(b_nr,"toggled",G_CALLBACK(nr_cb),NULL);

  GtkWidget *b_nr2=gtk_check_button_new_with_label("NR2");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr2), active_receiver->nr2);
  gtk_widget_show(b_nr2);
  gtk_grid_attach(GTK_GRID(grid),b_nr2,0,3,2,1);
  g_signal_connect(b_nr2,"toggled",G_CALLBACK(nr2_cb),NULL);

  GtkWidget *b_anf=gtk_check_button_new_with_label("ANF");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_anf), active_receiver->anf);
  gtk_widget_show(b_anf);
  gtk_grid_attach(GTK_GRID(grid),b_anf,0,4,2,1);
  g_signal_connect(b_anf,"toggled",G_CALLBACK(anf_cb),NULL);
 
  GtkWidget *b_snb=gtk_check_button_new_with_label("SNB");
  //gtk_widget_override_font(b_snb, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_snb), active_receiver->snb);
  gtk_widget_show(b_snb);
  gtk_grid_attach(GTK_GRID(grid),b_snb,0,5,2,1);
  g_signal_connect(b_snb,"toggled",G_CALLBACK(snb_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
