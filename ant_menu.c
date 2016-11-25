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
#include "ant_menu.h"
#include "band.h"
#include "radio.h"

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

static void rx_ant_cb(GtkWidget *widget, gpointer data) {
  int b=((int)data)>>4;
  int ant=((int)data)&0xF;
  BAND *band=band_get_band(b);
  band->alexRxAntenna=ant;
  set_alex_rx_antenna(ant);
}

static void rx_lime_ant_cb(GtkWidget *widget, gpointer data) {
  int ant=((int)data)&0xF;
  BAND *band=band_get_current_band();
  band->alexRxAntenna=ant;
  set_alex_rx_antenna(ant);
}

static void tx_ant_cb(GtkWidget *widget, gpointer data) {
  int b=((int)data)>>4;
  int ant=((int)data)&0xF;
  BAND *band=band_get_band(b);
  band->alexTxAntenna=ant;
  set_alex_tx_antenna(ant);
}

void ant_menu(GtkWidget *parent) {
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
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close ANT");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *rx_ant_label=gtk_label_new("Receive");
    //gtk_widget_override_font(rx_ant_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx_ant_label);
    gtk_grid_attach(GTK_GRID(grid),rx_ant_label,0,1,1,1);

    GtkWidget *rx1_label=gtk_label_new("1");
    //gtk_widget_override_font(rx1_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx1_label);
    gtk_grid_attach(GTK_GRID(grid),rx1_label,1,1,1,1);

    GtkWidget *rx2_label=gtk_label_new("2");
    //gtk_widget_override_font(rx2_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx2_label);
    gtk_grid_attach(GTK_GRID(grid),rx2_label,2,1,1,1);

    GtkWidget *rx3_label=gtk_label_new("3");
    //gtk_widget_override_font(rx3_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx3_label);
    gtk_grid_attach(GTK_GRID(grid),rx3_label,3,1,1,1);

    GtkWidget *ext1_label=gtk_label_new("EXT1");
    //gtk_widget_override_font(ext1_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(ext1_label);
    gtk_grid_attach(GTK_GRID(grid),ext1_label,4,1,1,1);

    GtkWidget *ext2_label=gtk_label_new("EXT2");
    //gtk_widget_override_font(ext2_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(ext2_label);
    gtk_grid_attach(GTK_GRID(grid),ext2_label,5,1,1,1);

    GtkWidget *xvtr_label=gtk_label_new("XVTR");
    //gtk_widget_override_font(xvtr_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(xvtr_label);
    gtk_grid_attach(GTK_GRID(grid),xvtr_label,6,1,1,1);

    GtkWidget *tx_ant_label=gtk_label_new("Transmit");
    //gtk_widget_override_font(tx_ant_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx_ant_label);
    gtk_grid_attach(GTK_GRID(grid),tx_ant_label,7,1,1,1);

    GtkWidget *tx1_label=gtk_label_new("1");
    //gtk_widget_override_font(tx1_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx1_label);
    gtk_grid_attach(GTK_GRID(grid),tx1_label,8,1,1,1);

    GtkWidget *tx2_label=gtk_label_new("2");
    //gtk_widget_override_font(tx2_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx2_label);
    gtk_grid_attach(GTK_GRID(grid),tx2_label,9,1,1,1);

    GtkWidget *tx3_label=gtk_label_new("3");
    //gtk_widget_override_font(tx3_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx3_label);
    gtk_grid_attach(GTK_GRID(grid),tx3_label,10,1,1,1);

    for(i=0;i<BANDS+XVTRS;i++) {
      BAND *band=band_get_band(i);
      if(strlen(band->title)>0) {

        GtkWidget *band_label=gtk_label_new(band->title);
        //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
        gtk_widget_show(band_label);
        gtk_grid_attach(GTK_GRID(grid),band_label,0,i+2,1,1);

        GtkWidget *rx1_b=gtk_radio_button_new(NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_b), band->alexRxAntenna==0);
        gtk_widget_show(rx1_b);
        gtk_grid_attach(GTK_GRID(grid),rx1_b,1,i+2,1,1);
        g_signal_connect(rx1_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+0));

        GtkWidget *rx2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx2_b), band->alexRxAntenna==1);
        gtk_widget_show(rx2_b);
        gtk_grid_attach(GTK_GRID(grid),rx2_b,2,i+2,1,1);
        g_signal_connect(rx2_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+1));

        GtkWidget *rx3_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx3_b), band->alexRxAntenna==2);
        gtk_widget_show(rx3_b);
        gtk_grid_attach(GTK_GRID(grid),rx3_b,3,i+2,1,1);
        g_signal_connect(rx3_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+2));

        GtkWidget *ext1_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx3_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ext1_b), band->alexRxAntenna==3);
        gtk_widget_show(ext1_b);
        gtk_grid_attach(GTK_GRID(grid),ext1_b,4,i+2,1,1);
        g_signal_connect(ext1_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+3));

        GtkWidget *ext2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(ext1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ext2_b), band->alexRxAntenna==4);
        gtk_widget_show(ext2_b);
        gtk_grid_attach(GTK_GRID(grid),ext2_b,5,i+2,1,1);
        g_signal_connect(ext2_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+4));

        GtkWidget *xvtr_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(ext2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (xvtr_b), band->alexRxAntenna==5);
        gtk_widget_show(xvtr_b);
        gtk_grid_attach(GTK_GRID(grid),xvtr_b,6,i+2,1,1);
        g_signal_connect(xvtr_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+5));

        GtkWidget *ant_band_label=gtk_label_new(band->title);
        //gtk_widget_override_font(ant_band_label, pango_font_description_from_string("Arial 18"));
        gtk_widget_show(ant_band_label);
        gtk_grid_attach(GTK_GRID(grid),ant_band_label,7,i+2,1,1);
  
        GtkWidget *tx1_b=gtk_radio_button_new(NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx1_b), band->alexTxAntenna==0);
        gtk_widget_show(tx1_b);
        gtk_grid_attach(GTK_GRID(grid),tx1_b,8,i+2,1,1);
  
        GtkWidget *tx2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(tx1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx2_b), band->alexTxAntenna==1);
        gtk_widget_show(tx2_b);
        gtk_grid_attach(GTK_GRID(grid),tx2_b,9,i+2,1,1);
        g_signal_connect(tx2_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)((i<<4)+1));
  
        GtkWidget *tx3_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(tx2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx3_b), band->alexTxAntenna==2);
        gtk_widget_show(tx3_b);
        gtk_grid_attach(GTK_GRID(grid),tx3_b,10,i+2,1,1);
        g_signal_connect(tx3_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)((i<<4)+2));
      }
    }
  }

#ifdef LIMESDR
  if(protocol==LIMESDR_PROTOCOL) {
    BAND *band=band_get_current_band();

    GtkWidget *rx1_none=gtk_radio_button_new_with_label(NULL,"RX 1: NONE");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_none), band->alexRxAntenna==0);
    gtk_widget_show(rx1_none);
    gtk_grid_attach(GTK_GRID(grid),rx1_none,0,1,1,1);
    g_signal_connect(rx1_none,"pressed",G_CALLBACK(rx_lime_ant_cb),(gpointer)0);

    GtkWidget *rx1_lnah=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rx1_none),"RX1: LNAH");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_lnah), band->alexRxAntenna==1);
    gtk_widget_show(rx1_lnah);
    gtk_grid_attach(GTK_GRID(grid),rx1_lnah,0,2,1,1);
    g_signal_connect(rx1_lnah,"pressed",G_CALLBACK(rx_lime_ant_cb),(gpointer)+1);

    GtkWidget *rx1_lnal=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rx1_lnah),"RX1: LNAL");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_lnal), band->alexRxAntenna==2);
    gtk_widget_show(rx1_lnal);
    gtk_grid_attach(GTK_GRID(grid),rx1_lnal,0,3,1,1);
    g_signal_connect(rx1_lnal,"pressed",G_CALLBACK(rx_lime_ant_cb),(gpointer)2);

    GtkWidget *rx1_lnaw=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rx1_lnal),"RX1: LNAW");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_lnaw), band->alexRxAntenna==3);
    gtk_widget_show(rx1_lnaw);
    gtk_grid_attach(GTK_GRID(grid),rx1_lnaw,0,4,1,1);
    g_signal_connect(rx1_lnaw,"pressed",G_CALLBACK(rx_lime_ant_cb),(gpointer)3);
  }
#endif


  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

