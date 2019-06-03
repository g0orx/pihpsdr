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
#include <stdlib.h>
#include <string.h>

#include <wdsp.h>

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
#include "ext.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *last_filter;

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

void update_noise() {
  SetEXTANBRun(active_receiver->id, active_receiver->nb);
  SetEXTNOBRun(active_receiver->id, active_receiver->nb2);
  SetRXAANRRun(active_receiver->id, active_receiver->nr);
  SetRXAEMNRRun(active_receiver->id, active_receiver->nr2);
  SetRXAANFRun(active_receiver->id, active_receiver->anf);
  SetRXASNBARun(active_receiver->id, active_receiver->snb);
  g_idle_add(ext_vfo_update,NULL);
}

static void nb_none_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nb=0;
  active_receiver->nb2=0;
  mode_settings[vfo[active_receiver->id].mode].nb=0;
  mode_settings[vfo[active_receiver->id].mode].nb2=0;
  update_noise();
}

static void nb_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nb=1;
  active_receiver->nb2=0;
  mode_settings[vfo[active_receiver->id].mode].nb=1;
  mode_settings[vfo[active_receiver->id].mode].nb2=0;
  update_noise();
}

static void nr_none_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr=0;
  active_receiver->nr2=0;
  mode_settings[vfo[active_receiver->id].mode].nr=0;
  mode_settings[vfo[active_receiver->id].mode].nr2=0;
  update_noise();
}

static void nr_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr=1;
  active_receiver->nr2=0;
  mode_settings[vfo[active_receiver->id].mode].nr=1;
  mode_settings[vfo[active_receiver->id].mode].nr2=0;
  update_noise();
}

static void nb2_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nb=0;
  active_receiver->nb2=1;
  mode_settings[vfo[active_receiver->id].mode].nb=0;
  mode_settings[vfo[active_receiver->id].mode].nb2=1;
  update_noise();
}

static void nr2_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr=0;
  active_receiver->nr2=1;
  mode_settings[vfo[active_receiver->id].mode].nr=0;
  mode_settings[vfo[active_receiver->id].mode].nr2=1;
  update_noise();
}

static void anf_cb(GtkWidget *widget, gpointer data) {
  active_receiver->anf=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  mode_settings[vfo[active_receiver->id].mode].anf=active_receiver->anf;
  update_noise();
}

static void snb_cb(GtkWidget *widget, gpointer data) {
  active_receiver->snb=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  mode_settings[vfo[active_receiver->id].mode].snb=active_receiver->snb;
  update_noise();
}

void noise_menu(GtkWidget *parent) {
  GtkWidget *b;
  int i;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - Noise (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
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

  GtkWidget *nb_title=gtk_label_new("Noise Blanker");
  gtk_widget_show(nb_title);
  gtk_grid_attach(GTK_GRID(grid),nb_title,col,row,1,1);

  col++;

  GtkWidget *nr_title=gtk_label_new("Noise Reduction");
  gtk_widget_show(nr_title);
  gtk_grid_attach(GTK_GRID(grid),nr_title,col,row,1,1);

  row++;
  col=0;

  GtkWidget *b_nb_none=gtk_radio_button_new_with_label(NULL, "None");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nb_none), active_receiver->nb==0 && active_receiver->nb2==0);
  gtk_widget_show(b_nb_none);
  gtk_grid_attach(GTK_GRID(grid),b_nb_none,col,row,1,1);
  g_signal_connect(b_nb_none,"pressed",G_CALLBACK(nb_none_cb),NULL);

  col++;

  GtkWidget *b_nr_none=gtk_radio_button_new_with_label(NULL, "None");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr_none), active_receiver->nr==0 && active_receiver->nr2==0);
  gtk_widget_show(b_nr_none);
  gtk_grid_attach(GTK_GRID(grid),b_nr_none,col,row,1,1);
  g_signal_connect(b_nr_none,"pressed",G_CALLBACK(nr_none_cb),NULL);

  col++;

  GtkWidget *b_snb=gtk_check_button_new_with_label("SNB");
  //gtk_widget_override_font(b_snb, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_snb), active_receiver->snb);
  gtk_widget_show(b_snb);
  gtk_grid_attach(GTK_GRID(grid),b_snb,col,row,1,1);
  g_signal_connect(b_snb,"toggled",G_CALLBACK(snb_cb),NULL);

  row++;
  col=0;

  GtkWidget *b_nb=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nb_none),"NB");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nb), active_receiver->nb);
  gtk_widget_show(b_nb);
  gtk_grid_attach(GTK_GRID(grid),b_nb,col,row,1,1);
  g_signal_connect(b_nb,"pressed",G_CALLBACK(nb_cb),NULL);

  col++;

  GtkWidget *b_nr=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr_none),"NR");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr), active_receiver->nr);
  gtk_widget_show(b_nr);
  gtk_grid_attach(GTK_GRID(grid),b_nr,col,row,1,1);
  g_signal_connect(b_nr,"pressed",G_CALLBACK(nr_cb),NULL);

  col++;

  GtkWidget *b_anf=gtk_check_button_new_with_label("ANF");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_anf), active_receiver->anf);
  gtk_widget_show(b_anf);
  gtk_grid_attach(GTK_GRID(grid),b_anf,col,row,1,1);
  g_signal_connect(b_anf,"toggled",G_CALLBACK(anf_cb),NULL);

  row++;
  col=0;

  GtkWidget *b_nb2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nb),"NB2");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nb2), active_receiver->nb2);
  gtk_widget_show(b_nb2);
  gtk_grid_attach(GTK_GRID(grid),b_nb2,col,row,1,1);
  g_signal_connect(b_nb2,"pressed",G_CALLBACK(nb2_cb),NULL);

  col++;

  GtkWidget *b_nr2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr),"NR2");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr2), active_receiver->nr2);
  gtk_widget_show(b_nr2);
  gtk_grid_attach(GTK_GRID(grid),b_nr2,col,row,1,1);
  g_signal_connect(b_nr2,"pressed",G_CALLBACK(nr2_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
