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

static GtkWidget *grid2;

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

static void pa_value_changed_cb(GtkWidget *widget, gpointer data) {
  BAND *band=(BAND *)data;
  band->pa_calibration=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  int txvfo=get_tx_vfo();
  int b=vfo[txvfo].band;
  BAND *current=band_get_band(b);
  if(band==current) {
    calcDriveLevel();
  }
}

static void tx_out_of_band_cb(GtkWidget *widget, gpointer data) {
  tx_out_of_band=tx_out_of_band==1?0:1;
}

static void trim_changed_cb(GtkWidget *widget, gpointer data) {
  int i=GPOINTER_TO_INT(data);
  pa_trim[i]=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
}

static void show_1W(gboolean reset) {
  int i;
  char text[16];

  if(reset) {
    for(i=0;i<11;i++) {
      pa_trim[i]=i*100;
    }
  }
  for(i=1;i<11;i++) {
    sprintf(text,"<b>%dmW</b>",pa_trim[i]);
    GtkWidget *label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_grid_attach(GTK_GRID(grid2),label,0,i,1,1);

    GtkWidget *spin=gtk_spin_button_new_with_range(0.0,(double)((i+2)*100),1.0);
    gtk_grid_attach(GTK_GRID(grid2),spin,1,i,1,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin),(double)pa_trim[i]);
    g_signal_connect(spin,"value_changed",G_CALLBACK(trim_changed_cb),GINT_TO_POINTER(i));
  }
}

static void show_W(int watts,gboolean reset) {
  int i;
  char text[16];
  int increment=watts/10;
  double limit;

  if(reset) {
    for(i=0;i<11;i++) {
      pa_trim[i]=i*increment;
    }
  }

  for(i=1;i<11;i++) {
    sprintf(text,"<b>%dW</b>",i*increment);
    GtkWidget *label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_grid_attach(GTK_GRID(grid2),label,0,i,1,1);
    if(watts>=30 && watts <=100) {
      limit=(double)((i*increment)+100);
    } else {
      limit=(double)((i+2)*increment);
    }

    GtkWidget *spin=gtk_spin_button_new_with_range(0.0,limit,1.0);
    gtk_grid_attach(GTK_GRID(grid2),spin,1,i,1,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin),(double)pa_trim[i]);
    g_signal_connect(spin,"value_changed",G_CALLBACK(trim_changed_cb),GINT_TO_POINTER(i));
  }
}

static void clear_W() {
  int i;
  for(i=0;i<10;i++) {
    gtk_grid_remove_row(GTK_GRID(grid2),1);
  }
}

static void max_power_changed_cb(GtkWidget *widget, gpointer data) {
  pa_power = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
g_print("max_power_changed_cb: %d\n",pa_power);
  clear_W();
  switch(pa_power) {
    case PA_1W:
      show_1W(TRUE);
      break;
    case PA_10W:
      show_W(10,TRUE);
      break;
    case PA_30W:
      show_W(30,TRUE);
      break;
    case PA_50W:
      show_W(50,TRUE);
      break;
    case PA_100W:
      show_W(100,TRUE);
      break;
    case PA_200W:
      show_W(200,TRUE);
      break;
    case PA_500W:
      show_W(500,TRUE);
      break;
  }
  gtk_widget_show_all(grid2);
}

void pa_menu(GtkWidget *parent) {
  int i;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - PA Calibration");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *notebook=gtk_notebook_new();

  GtkWidget *grid0=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid0),10);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid0),close_b,0,0,1,1);

  GtkWidget *max_power_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(max_power_label), "<b>MAX Power</b>");
  gtk_grid_attach(GTK_GRID(grid0),max_power_label,1,0,1,1);

  GtkWidget *max_power_b=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(max_power_b),NULL,"1W");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(max_power_b),NULL,"10W");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(max_power_b),NULL,"30W");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(max_power_b),NULL,"50W");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(max_power_b),NULL,"100W");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(max_power_b),NULL,"200W");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(max_power_b),NULL,"500W");
  gtk_combo_box_set_active(GTK_COMBO_BOX(max_power_b),pa_power);
  gtk_grid_attach(GTK_GRID(grid0),max_power_b,2,0,1,1);
  g_signal_connect(max_power_b,"changed",G_CALLBACK(max_power_changed_cb),NULL);

  GtkWidget *tx_out_of_band_b=gtk_check_button_new_with_label("Transmit out of band");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx_out_of_band_b), tx_out_of_band);
  gtk_widget_show(tx_out_of_band_b);
  gtk_grid_attach(GTK_GRID(grid0),tx_out_of_band_b,3,0,1,1);
  g_signal_connect(tx_out_of_band_b,"toggled",G_CALLBACK(tx_out_of_band_cb),NULL);
 
  GtkWidget *grid1=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid1),10);

  int bands=BANDS;
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      switch(device) {
        case DEVICE_HERMES_LITE:
        case DEVICE_HERMES_LITE2:
          bands=band10+1;
          break;
        default:
          bands=band6+1;
          break;
      }
      break;
    case NEW_PROTOCOL:
      switch(device) {
        case NEW_DEVICE_HERMES_LITE:
        case NEW_DEVICE_HERMES_LITE2:
          bands=band10+1;
          break;
        default:
          bands=band6+1;
          break;
      }
      break;
  }


  for(i=0;i<bands;i++) {
    BAND *band=band_get_band(i);

    GtkWidget *band_label=gtk_label_new(NULL);
    char band_text[32];
    sprintf(band_text,"<b>%s</b>",band->title);
    gtk_label_set_markup(GTK_LABEL(band_label), band_text);
    //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(band_label);
    gtk_grid_attach(GTK_GRID(grid1),band_label,(i/6)*2,(i%6)+1,1,1);

    GtkWidget *pa_r=gtk_spin_button_new_with_range(38.8,100.0,0.1);
    //gtk_widget_override_font(pa_r, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(pa_r),(double)band->pa_calibration);
    gtk_widget_show(pa_r);
    gtk_grid_attach(GTK_GRID(grid1),pa_r,((i/6)*2)+1,(i%6)+1,1,1);
    g_signal_connect(pa_r,"value_changed",G_CALLBACK(pa_value_changed_cb),band);
  }


  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid1,gtk_label_new("Calibrate"));

  grid2=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid2),10);
  
  switch(pa_power) {
    case PA_1W:
      show_1W(FALSE);
      break;
    case PA_10W:
      show_W(10,FALSE);
      break;
    case PA_30W:
      show_W(30,FALSE);
      break;
    case PA_50W:
      show_W(50,FALSE);
      break;
    case PA_100W:
      show_W(100,FALSE);
      break;
    case PA_200W:
      show_W(200,FALSE);
      break;
    case PA_500W:
      show_W(500,FALSE);
      break;
  }

  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid2,gtk_label_new("Watt Meter Calibrate"));

  gtk_grid_attach(GTK_GRID(grid0),notebook,0,1,6,1);
  gtk_container_add(GTK_CONTAINER(content),grid0);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

