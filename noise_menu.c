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
  SetRXAANRRun(CHANNEL_RX0, nr);
  SetRXAEMNRRun(CHANNEL_RX0, nr2);
  SetRXAANFRun(CHANNEL_RX0, anf);
  SetRXASNBARun(CHANNEL_RX0, snb);
  vfo_update(NULL);
}

static void nr_none_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=0;
  nb2=0;
  anf=0;
  snb=0;
  update_noise();
}

static void nr_cb(GtkWidget *widget, gpointer data) {
  nr=1;
  nr2=0;
  nb=0;
  nb2=0;
  anf=0;
  snb=0;
  update_noise();
}

static void nr2_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=1;
  nb=0;
  nb2=0;
  anf=0;
  snb=0;
  update_noise();
}

static void nb_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=1;
  nb2=0;
  anf=0;
  snb=0;
  update_noise();
}

static void nb2_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=0;
  nb2=1;
  anf=0;
  snb=0;
  update_noise();
}

static void anf_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=0;
  nb2=0;
  anf=1;
  snb=0;
  update_noise();
}

static void snb_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=0;
  nb2=0;
  anf=0;
  snb=1;
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

  GtkWidget *b_nr_none=gtk_radio_button_new_with_label(NULL,"None");
  //gtk_widget_override_font(b_none, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr_none), nr_none==1);
  gtk_widget_show(b_nr_none);
  gtk_grid_attach(GTK_GRID(grid),b_nr_none,0,1,2,1);
  g_signal_connect(b_nr_none,"pressed",G_CALLBACK(nr_none_cb),NULL);

  GtkWidget *b_nr=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr_none),"NR");
  //gtk_widget_override_font(b_nr, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr), nr==1);
  gtk_widget_show(b_nr);
  gtk_grid_attach(GTK_GRID(grid),b_nr,0,2,2,1);
  g_signal_connect(b_nr,"pressed",G_CALLBACK(nr_cb),NULL);

  GtkWidget *b_nr2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr),"NR2");
  //gtk_widget_override_font(b_nr2, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr2), nr2==1);
  gtk_widget_show(b_nr2);
  gtk_grid_attach(GTK_GRID(grid),b_nr2,0,3,2,1);
  g_signal_connect(b_nr2,"pressed",G_CALLBACK(nr2_cb),NULL);

/*
  GtkWidget *b_nb=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr2),"NB");
  //gtk_widget_override_font(b_nb, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nb), nb==1);
  gtk_widget_show(b_nb);
  gtk_grid_attach(GTK_GRID(grid),b_nb,0,4,2,1);
  g_signal_connect(b_nb,"pressed",G_CALLBACK(nb_cb),NULL);

  GtkWidget *b_nb2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nb),"NB2");
  //gtk_widget_override_font(b_nb2, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nb2), nb2==1);
  gtk_widget_show(b_nb2);
  gtk_grid_attach(GTK_GRID(grid),b_nb2,0,5,2,1);
  g_signal_connect(b_nb2,"pressed",G_CALLBACK(nb2_cb),NULL);
*/

  GtkWidget *b_anf=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr2),"ANF");
  //gtk_widget_override_font(b_anf, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_anf), anf==1);
  gtk_widget_show(b_anf);
  gtk_grid_attach(GTK_GRID(grid),b_anf,0,4,2,1);
  g_signal_connect(b_anf,"pressed",G_CALLBACK(anf_cb),NULL);
 
  GtkWidget *b_snb=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_anf),"SNB");
  //gtk_widget_override_font(b_snb, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_snb), snb==1);
  gtk_widget_show(b_snb);
  gtk_grid_attach(GTK_GRID(grid),b_snb,0,5,2,1);
  g_signal_connect(b_snb,"pressed",G_CALLBACK(snb_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
