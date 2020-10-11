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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "new_menu.h"
#include "oc_menu.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "radio.h"
#include "new_protocol.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

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

static void oc_rx_cb(GtkWidget *widget, gpointer data) {
  int b=(GPOINTER_TO_UINT(data))>>4;
  int oc=(GPOINTER_TO_UINT(data))&0xF;
  BAND *band=band_get_band(b);
  int mask=0x01<<(oc-1);
fprintf(stderr,"oc_rx_cb: band=%d oc=%d mask=%d\n",b,oc,mask);
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    band->OCrx|=mask;
  } else {
    band->OCrx&=~mask;
  }

  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

static void oc_tx_cb(GtkWidget *widget, gpointer data) {
  int b=(GPOINTER_TO_UINT(data))>>4;
  int oc=(GPOINTER_TO_UINT(data))&0xF;
  BAND *band=band_get_band(b);
  int mask=0x01<<(oc-1);

fprintf(stderr,"oc_tx_cb: band=%d oc=%d mask=%d\n",b,oc,mask);

  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    band->OCtx|=mask;
  } else {
    band->OCtx&=~mask;
  }
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

static void oc_tune_cb(GtkWidget *widget, gpointer data) {
  int oc=(GPOINTER_TO_UINT(data))&0xF;
  int mask=0x01<<(oc-1);
fprintf(stderr,"oc_tune_cb: oc=%d mask=%d\n",oc,mask);
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

g_print("oc_menu: parent=%p\n",parent);
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Open Collector Output");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *sw=gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
  gtk_widget_set_size_request(sw, 600, 400);
  GtkWidget *viewport=gtk_viewport_new(NULL,NULL);

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *band_title=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(band_title), "<b>Band</b>");
  //gtk_widget_override_font(band_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(band_title);
  gtk_grid_attach(GTK_GRID(grid),band_title,0,1,1,1);

  GtkWidget *rx_title=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rx_title), "<b>Rx</b>");
  //gtk_widget_override_font(rx_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(rx_title);
  gtk_grid_attach(GTK_GRID(grid),rx_title,4,1,1,1);

  GtkWidget *tx_title=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(tx_title), "<b>Tx</b>");
  //gtk_widget_override_font(tx_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(tx_title);
  gtk_grid_attach(GTK_GRID(grid),tx_title,11,1,1,1);

  GtkWidget *tune_title=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(tune_title), "<b>Tune (ORed with TX)</b>");
  //gtk_widget_override_font(tune_title, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(tune_title);
  gtk_grid_attach(GTK_GRID(grid),tune_title,18,1,2,1);

  for(i=1;i<8;i++) {
    char oc_id[16];
    sprintf(oc_id,"<b>%d</b>",i);
    GtkWidget *oc_rx_title=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(oc_rx_title), oc_id);
    //gtk_widget_override_font(oc_rx_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(oc_rx_title);
    gtk_grid_attach(GTK_GRID(grid),oc_rx_title,i,2,1,1);
    GtkWidget *oc_tx_title=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(oc_tx_title), oc_id);
    //gtk_widget_override_font(oc_tx_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(oc_tx_title);
    gtk_grid_attach(GTK_GRID(grid),oc_tx_title,i+7,2,1,1);
  }

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

  int row=3;

  //
  // fused loop. i runs over the following values:
  // bandGen, 0 ... bands-1, BANDS ... BANDS+XVTRS-1
  // XVTR bands not-yet-assigned have an empty title
  // and are filtered out
  //
  i=bandGen;
  for(;;) {
    BAND *band=band_get_band(i);
    if(strlen(band->title)>0) {
      GtkWidget *band_label=gtk_label_new(NULL);
      char band_text[16];
      sprintf(band_text,"<b>%s</b>",band->title);
      gtk_label_set_markup(GTK_LABEL(band_label), band_text);
      //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(band_label);
      gtk_grid_attach(GTK_GRID(grid),band_label,0,row,1,1);

      int mask;
      for(j=1;j<8;j++) {
        mask=0x01<<(j-1);
        GtkWidget *oc_rx_b=gtk_check_button_new();
        //gtk_widget_override_font(oc_rx_b, pango_font_description_from_string("Arial 18"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_rx_b), (band->OCrx&mask)==mask);
        gtk_widget_show(oc_rx_b);
        gtk_grid_attach(GTK_GRID(grid),oc_rx_b,j,row,1,1);
        g_signal_connect(oc_rx_b,"toggled",G_CALLBACK(oc_rx_cb),(gpointer)(long)(j+(i<<4)));
  
        GtkWidget *oc_tx_b=gtk_check_button_new();
        //gtk_widget_override_font(oc_tx_b, pango_font_description_from_string("Arial 18"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_tx_b), (band->OCtx&mask)==mask);
        gtk_widget_show(oc_tx_b);
        gtk_grid_attach(GTK_GRID(grid),oc_tx_b,j+7,row,1,1);
        g_signal_connect(oc_tx_b,"toggled",G_CALLBACK(oc_tx_cb),(gpointer)(long)(j+(i<<4)));

      }
      row++;
    }
    // update "loop index"
    if (i == bandGen) {
      i=0;
    } else if (i == bands-1) {
      i=BANDS;
    } else {
      i++;
    }
    if (i >= BANDS+XVTRS) break;
  }

  int mask;
  for(j=1;j<8;j++) {
    char oc_id[8];
    sprintf(oc_id,"%d",j);
    GtkWidget *oc_tune_title=gtk_label_new(oc_id);
    //gtk_widget_override_font(oc_tune_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(oc_tune_title);
    gtk_grid_attach(GTK_GRID(grid),oc_tune_title,18,j+1,1,1);

    mask=0x01<<(j-1);
    GtkWidget *oc_tune_b=gtk_check_button_new();
    //gtk_widget_override_font(oc_tune_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_tune_b), (OCtune&mask)==mask);
    gtk_widget_show(oc_tune_b);
    gtk_grid_attach(GTK_GRID(grid),oc_tune_b,19,j+1,1,1);
    g_signal_connect(oc_tune_b,"toggled",G_CALLBACK(oc_tune_cb),(gpointer)(long)j);
  }

  GtkWidget *oc_full_tune_time_title=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(oc_full_tune_time_title), "Full Tune(ms):");
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

  GtkWidget *oc_memory_tune_time_title=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(oc_memory_tune_time_title), "Memory Tune(ms):");
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

  gtk_container_add(GTK_CONTAINER(viewport),grid);
  gtk_container_add(GTK_CONTAINER(sw),viewport);
  gtk_container_add(GTK_CONTAINER(content),sw);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

