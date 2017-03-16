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
#include "bandstack_menu.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "button_text.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *last_bandstack;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static gboolean bandstack_select_cb (GtkWidget *widget, gpointer        data) {
  int b=(int)data;
  set_button_text_color(last_bandstack,"black");
  last_bandstack=widget;
  set_button_text_color(last_bandstack,"orange");
  vfo_bandstack_changed(b);
}

void bandstack_menu(GtkWidget *parent) {
  GtkWidget *b;
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

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  GtkWidget *close_b=gtk_button_new_with_label("Close Band Stack");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  char label[32];
  sprintf(label,"RX %d VFO %s",active_receiver->id,active_receiver->id==0?"A":"B");
  GtkWidget *rx_label=gtk_label_new(label);
  gtk_grid_attach(GTK_GRID(grid),rx_label,1,0,1,1);

  BAND *band=band_get_band(vfo[active_receiver->id].band);
  BANDSTACK *bandstack=band->bandstack;

  for(i=0;i<bandstack->entries;i++) {
    BANDSTACK_ENTRY *entry=&bandstack->entry[i];
    sprintf(label,"%lld %s",entry->frequency,mode_string[entry->mode]);
    GtkWidget *b=gtk_button_new_with_label(label);
    set_button_text_color(b,"black");
    //gtk_widget_override_font(b, pango_font_description_from_string("Arial 20"));
    if(i==vfo[active_receiver->id].bandstack) {
      set_button_text_color(b,"orange");
      last_bandstack=b;
    }
    gtk_widget_show(b);
    gtk_grid_attach(GTK_GRID(grid),b,i/5,1+(i%5),1,1);
    g_signal_connect(b,"clicked",G_CALLBACK(bandstack_select_cb),(gpointer *)i);
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
