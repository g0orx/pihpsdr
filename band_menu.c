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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "new_menu.h"
#include "band_menu.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "button_text.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *last_band;

static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
    active_menu=NO_MENU;
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

gboolean band_select_cb (GtkWidget *widget, gpointer        data) {
  int b=GPOINTER_TO_UINT(data);
  set_button_text_color(last_band,"black");
  last_band=widget;
  //fprintf(stderr,"%s: %d\n",__FUNCTION__,b);
  set_button_text_color(last_band,"orange");
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_band(client_socket,active_receiver->id,b);
  } else {
#endif
    vfo_band_changed(active_receiver->id,b);
#ifdef CLIENT_SERVER
  }
#endif
  return FALSE;
}

void band_menu(GtkWidget *parent) {
  int i,j;
  BAND *band;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - Band (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

#ifdef BKGND
  extern GdkRGBA bkgnd_color;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&bkgnd_color);
#else
  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);
#endif

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  long long frequency_min=radio->frequency_min;
  long long frequency_max=radio->frequency_max;

  //g_print("band_menu: min=%lld max=%lld\n",frequency_min,frequency_max);
  j=0;
  for(i=0;i<BANDS+XVTRS;i++) {
    band=(BAND*)band_get_band(i);
    if(strlen(band->title)>0) {
      if(i<BANDS) {
        if(!(band->frequencyMin==0.0 && band->frequencyMax==0.0)) {
          if(band->frequencyMin<frequency_min || band->frequencyMax>frequency_max) {
            continue;
          }
        }
      }
      GtkWidget *b=gtk_button_new_with_label(band->title);
      set_button_text_color(b,"black");
      if(i==vfo[active_receiver->id].band) {
        set_button_text_color(b,"orange");
        last_band=b;
      }
      gtk_widget_show(b);
      gtk_grid_attach(GTK_GRID(grid),b,j%5,1+(j/5),1,1);
      g_signal_connect(b,"clicked",G_CALLBACK(band_select_cb),(gpointer)(long)i);
      j++;
    }
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
