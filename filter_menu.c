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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "new_menu.h"
#include "filter_menu.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "mode.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "button_text.h"

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

int filter_select(void *data) {
  int f=(uintptr_t)data;
  vfo_filter_changed(f);
  return 0;
}

static gboolean filter_select_cb (GtkWidget *widget, gpointer        data) {
  filter_select(data);
  set_button_text_color(last_filter,"black");
  last_filter=widget;
  set_button_text_color(last_filter,"orange");
  // DL1YCF added return statement to make the compiler happy.
  // however I am unsure about the correct return value.
  // I would have coded this as a void function.
  return FALSE;
}

static gboolean deviation_select_cb (GtkWidget *widget, gpointer data) {
  active_receiver->deviation=(uintptr_t)data;
  transmitter->deviation=(uintptr_t)data;
  if(active_receiver->deviation==2500) {
    //setFilter(-4000,4000);
    set_filter(active_receiver,-4000,4000);
    tx_set_filter(transmitter,-4000,4000);
  } else {
    //setFilter(-8000,8000);
    set_filter(active_receiver,-8000,8000);
    tx_set_filter(transmitter,-8000,8000);
  }
  set_deviation(active_receiver);
  transmitter_set_deviation(transmitter);
  set_button_text_color(last_filter,"black");
  last_filter=widget;
  set_button_text_color(last_filter,"orange");
  vfo_update();
  // DL1YCF added return statement to make the compiler happy.
  // however I am unsure about the correct return value.
  // I would have coded this as a void function.
  return FALSE;
}

static void var_spin_low_cb (GtkWidget *widget, gpointer data) {
  int f=(uintptr_t)data;
  int id=active_receiver->id;

  FILTER *mode_filters=filters[vfo[id].mode];
  FILTER *filter=&mode_filters[f];

  filter->low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  if(vfo[id].mode==modeCWL || vfo[id].mode==modeCWU) {
    filter->high=filter->low;
  }
  if(f==vfo[id].filter) {
    vfo_filter_changed(f);
    //receiver_filter_changed(receiver[id]);
  }
}

static void var_spin_high_cb (GtkWidget *widget, gpointer data) {
  int f=(uintptr_t)data;
  int id=active_receiver->id;

  FILTER *mode_filters=filters[vfo[id].mode];
  FILTER *filter=&mode_filters[f];

  filter->high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  if(f==vfo[id].filter) {
    vfo_filter_changed(f);
    //receiver_filter_changed(receiver[id]);
  }
}

void filter_menu(GtkWidget *parent) {
  GtkWidget *b;
  int i;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - Filter (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
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

  BAND *band=band_get_band(vfo[active_receiver->id].band);
  BANDSTACK *bandstack=band->bandstack;
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[active_receiver->id].bandstack];
  FILTER* band_filters=filters[vfo[active_receiver->id].mode];
  FILTER* band_filter=&band_filters[vfo[active_receiver->id].filter];

  switch(vfo[active_receiver->id].mode) {
    case modeFMN:
      {
      GtkWidget *l=gtk_label_new("Deviation:");
      gtk_grid_attach(GTK_GRID(grid),l,0,1,1,1);

      GtkWidget *b=gtk_button_new_with_label("2.5K");
      if(active_receiver->deviation==2500) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"black");
      }
      g_signal_connect(b,"pressed",G_CALLBACK(deviation_select_cb),(gpointer)(long)2500);
      gtk_grid_attach(GTK_GRID(grid),b,1,1,1,1);

      b=gtk_button_new_with_label("5.0K");
      if(active_receiver->deviation==5000) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"black");
      }
      g_signal_connect(b,"pressed",G_CALLBACK(deviation_select_cb),(gpointer)(long)5000);
      gtk_grid_attach(GTK_GRID(grid),b,2,1,1,1);
      }
      break;

    default:
      for(i=0;i<FILTERS-2;i++) {
        FILTER* band_filter=&band_filters[i];
        GtkWidget *b=gtk_button_new_with_label(band_filters[i].title);
        if(i==vfo[active_receiver->id].filter) {
          set_button_text_color(b,"orange");
          last_filter=b;
        } else {
          set_button_text_color(b,"black");
        }
        gtk_grid_attach(GTK_GRID(grid),b,i%5,1+(i/5),1,1);
        g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer)(long)i);
      }

      // last 2 are var1 and var2
      int row=1+((i+4)/5);
      FILTER* band_filter=&band_filters[i];
      GtkWidget *b=gtk_button_new_with_label(band_filters[i].title);
      if(i==vfo[active_receiver->id].filter) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"black");
      }
      gtk_grid_attach(GTK_GRID(grid),b,0,row,1,1);
      g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer)(long)i);

      GtkWidget *var1_spin_low=gtk_spin_button_new_with_range(-8000.0,+8000.0,1.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_low),(double)band_filter->low);
      gtk_grid_attach(GTK_GRID(grid),var1_spin_low,1,row,1,1);
      g_signal_connect(var1_spin_low,"value-changed",G_CALLBACK(var_spin_low_cb),(gpointer)(long)i);

      if(vfo[active_receiver->id].mode!=modeCWL && vfo[active_receiver->id].mode!=modeCWU) {
        GtkWidget *var1_spin_high=gtk_spin_button_new_with_range(-8000.0,+8000.0,1.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_high),(double)band_filter->high);
        gtk_grid_attach(GTK_GRID(grid),var1_spin_high,2,row,1,1);
        g_signal_connect(var1_spin_high,"value-changed",G_CALLBACK(var_spin_high_cb),(gpointer)(long)i);
      }

      row++;
      i++;
      band_filter=&band_filters[i];
      b=gtk_button_new_with_label(band_filters[i].title);
      if(i==vfo[active_receiver->id].filter) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"black");
      }
      gtk_grid_attach(GTK_GRID(grid),b,0,row,1,1);
      g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer)(long)i);
      
      GtkWidget *var2_spin_low=gtk_spin_button_new_with_range(-8000.0,+8000.0,1.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_low),(double)band_filter->low);
      gtk_grid_attach(GTK_GRID(grid),var2_spin_low,1,row,1,1);
      g_signal_connect(var2_spin_low,"value-changed",G_CALLBACK(var_spin_low_cb),(gpointer)(long)i);

      if(vfo[active_receiver->id].mode!=modeCWL && vfo[active_receiver->id].mode!=modeCWU) {
        GtkWidget *var2_spin_high=gtk_spin_button_new_with_range(-8000.0,+8000.0,1.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_high),(double)band_filter->high);
        gtk_grid_attach(GTK_GRID(grid),var2_spin_high,2,row,1,1);
        g_signal_connect(var2_spin_high,"value-changed",G_CALLBACK(var_spin_high_cb),(gpointer)(long)i);
      }

      break;
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
