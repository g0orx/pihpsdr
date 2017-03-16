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
#include "pa_menu.h"
#include "band.h"
#include "radio.h"
#include "vfo.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static void pa_value_changed_cb(GtkWidget *widget, gpointer data) {
  BAND *band=(BAND *)data;
  band->pa_calibration=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  int v=VFO_A;
  if(split) v=VFO_B;
  int b=vfo[v].band;
  BAND *current=band_get_band(b);
  if(band==current) {
    calcDriveLevel();
    calcTuneDriveLevel();
  }
}

static void tx_out_of_band_cb(GtkWidget *widget, gpointer data) {
  tx_out_of_band=tx_out_of_band==1?0:1;
}

void pa_menu(GtkWidget *parent) {
  int i;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);

#ifdef FORCE_WHITE_MENU
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

  GtkWidget *close_b=gtk_button_new_with_label("Close PA Gain(dB)");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  for(i=0;i<BANDS;i++) {
    BAND *band=band_get_band(i);

    GtkWidget *band_label=gtk_label_new(band->title);
    //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(band_label);
    gtk_grid_attach(GTK_GRID(grid),band_label,(i/6)*2,(i%6)+1,1,1);

    GtkWidget *pa_r=gtk_spin_button_new_with_range(38.8,100.0,0.1);
    //gtk_widget_override_font(pa_r, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(pa_r),(double)band->pa_calibration);
    gtk_widget_show(pa_r);
    gtk_grid_attach(GTK_GRID(grid),pa_r,((i/6)*2)+1,(i%6)+1,1,1);
    g_signal_connect(pa_r,"value_changed",G_CALLBACK(pa_value_changed_cb),band);
  }

  GtkWidget *tx_out_of_band_b=gtk_check_button_new_with_label("Transmit out of band");
  //gtk_widget_override_font(tx_out_of_band_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx_out_of_band_b), tx_out_of_band);
  gtk_widget_show(tx_out_of_band_b);
  gtk_grid_attach(GTK_GRID(grid),tx_out_of_band_b,0,7,4,1);
  g_signal_connect(tx_out_of_band_b,"toggled",G_CALLBACK(tx_out_of_band_cb),NULL);
 

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

