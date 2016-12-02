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
#include "filter_menu.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "mode.h"
#include "radio.h"
#include "vfo.h"
#include "button_text.h"
#include "wdsp_init.h"

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

static gboolean filter_select_cb (GtkWidget *widget, gpointer        data) {
  int f=(int)data;
  BANDSTACK_ENTRY *entry;
  entry=bandstack_entry_get_current();
  entry->filter=f;
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);
  set_button_text_color(last_filter,"black");
  last_filter=widget;
  set_button_text_color(last_filter,"orange");
  vfo_update(NULL);
}

static gboolean deviation_select_cb (GtkWidget *widget, gpointer        data) {
  deviation=(int)data;
  if(deviation==2500) {
    setFilter(-4000,4000);
  } else {
    setFilter(-8000,8000);
  }
  wdsp_set_deviation((double)deviation);
  set_button_text_color(last_filter,"black");
  last_filter=widget;
  set_button_text_color(last_filter,"orange");
  vfo_update(NULL);
}

void filter_menu(GtkWidget *parent) {
  GtkWidget *b;
  int i;
  BAND *band;

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

  GtkWidget *close_b=gtk_button_new_with_label("Close Filter");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  BANDSTACK_ENTRY *entry=bandstack_entry_get_current();
  FILTER* band_filters=filters[entry->mode];

  switch(entry->mode) {
    case modeFMN:
      {
      GtkWidget *l=gtk_label_new("Deviation:");
      gtk_grid_attach(GTK_GRID(grid),l,0,1,1,1);

      GtkWidget *b=gtk_button_new_with_label("2.5K");
      if(deviation==2500) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"black");
      }
      g_signal_connect(b,"pressed",G_CALLBACK(deviation_select_cb),(gpointer *)2500);
      gtk_grid_attach(GTK_GRID(grid),b,1,1,1,1);

      b=gtk_button_new_with_label("5.0K");
      if(deviation==5000) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"black");
      }
      g_signal_connect(b,"pressed",G_CALLBACK(deviation_select_cb),(gpointer *)5000);
      gtk_grid_attach(GTK_GRID(grid),b,2,1,1,1);
      }
      break;

    default:
      for(i=0;i<FILTERS;i++) {
        FILTER* band_filter=&band_filters[i];
        GtkWidget *b=gtk_button_new_with_label(band_filters[i].title);
        if(i==entry->filter) {
          set_button_text_color(b,"orange");
          last_filter=b;
        } else {
          set_button_text_color(b,"black");
        }
        gtk_grid_attach(GTK_GRID(grid),b,i%5,1+(i/5),1,1);
        g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer *)i);
      }
      break;
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
