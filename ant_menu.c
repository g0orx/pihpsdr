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
#include "ant_menu.h"
#include "band.h"
#include "radio.h"
#include "new_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif


static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *grid=NULL;
static GtkWidget *adc0_antenna_combo_box;
static GtkWidget *dac0_antenna_combo_box;

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

static void rx_ant_cb(GtkWidget *widget, gpointer data) {
  int b=((uintptr_t)data)>>4;
  int ant=((uintptr_t)data)&0xF;
  BAND *band=band_get_band(b);
  band->alexRxAntenna=ant;
  if(active_receiver->id==0) {
    set_alex_rx_antenna(ant);
  }
}

static void adc0_antenna_cb(GtkComboBox *widget,gpointer data) {
  ADC *adc=(ADC *)data;
  adc->antenna=gtk_combo_box_get_active(widget);
  if(radio->protocol==NEW_PROTOCOL) {
    schedule_high_priority();
#ifdef SOAPYSDR
  } else if(radio->device==SOAPYSDR_USB_DEVICE) {
    soapy_protocol_set_rx_antenna(receiver[0],adc[0].antenna);
#endif
  }
}

static void dac0_antenna_cb(GtkComboBox *widget,gpointer data) {
  DAC *dac=(DAC *)data;
  dac->antenna=gtk_combo_box_get_active(widget);
  if(radio->protocol==NEW_PROTOCOL) {
    schedule_high_priority();
#ifdef SOAPYSDR
  } else if(radio->device==SOAPYSDR_USB_DEVICE) {
    soapy_protocol_set_tx_antenna(transmitter,dac[0].antenna);
#endif
  }
}

static void rx_lime_ant_cb(GtkWidget *widget, gpointer data) {
  int ant=((uintptr_t)data)&0xF;
  BAND *band=band_get_current_band();
  band->alexRxAntenna=ant;
  if(active_receiver->id==0) {
    set_alex_rx_antenna(ant);
  }
}

static void tx_ant_cb(GtkWidget *widget, gpointer data) {
  int b=((uintptr_t)data)>>4;
  int ant=((uintptr_t)data)&0xF;
  BAND *band=band_get_band(b);
  band->alexTxAntenna=ant;
  if(active_receiver->id==0) {
    set_alex_tx_antenna(ant);
  }
}

static void show_hf() {
  int i;
    for(i=0;i<BANDS;i++) {
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
        g_signal_connect(rx1_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)((i<<4)+0));

        GtkWidget *rx2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx2_b), band->alexRxAntenna==1);
        gtk_widget_show(rx2_b);
        gtk_grid_attach(GTK_GRID(grid),rx2_b,2,i+2,1,1);
        g_signal_connect(rx2_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)((i<<4)+1));

        GtkWidget *rx3_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx3_b), band->alexRxAntenna==2);
        gtk_widget_show(rx3_b);
        gtk_grid_attach(GTK_GRID(grid),rx3_b,3,i+2,1,1);
        g_signal_connect(rx3_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)((i<<4)+2));

        GtkWidget *ext1_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx3_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ext1_b), band->alexRxAntenna==3);
        gtk_widget_show(ext1_b);
        gtk_grid_attach(GTK_GRID(grid),ext1_b,4,i+2,1,1);
        g_signal_connect(ext1_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)((i<<4)+3));

        GtkWidget *ext2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(ext1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ext2_b), band->alexRxAntenna==4);
        gtk_widget_show(ext2_b);
        gtk_grid_attach(GTK_GRID(grid),ext2_b,5,i+2,1,1);
        g_signal_connect(ext2_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)((i<<4)+4));

        GtkWidget *xvtr_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(ext2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (xvtr_b), band->alexRxAntenna==5);
        gtk_widget_show(xvtr_b);
        gtk_grid_attach(GTK_GRID(grid),xvtr_b,6,i+2,1,1);
        g_signal_connect(xvtr_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)((i<<4)+5));

        GtkWidget *ant_band_label=gtk_label_new(band->title);
        //gtk_widget_override_font(ant_band_label, pango_font_description_from_string("Arial 18"));
        gtk_widget_show(ant_band_label);
        gtk_grid_attach(GTK_GRID(grid),ant_band_label,7,i+2,1,1);
  
        GtkWidget *tx1_b=gtk_radio_button_new(NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx1_b), band->alexTxAntenna==0);
        gtk_widget_show(tx1_b);
        gtk_grid_attach(GTK_GRID(grid),tx1_b,8,i+2,1,1);
        g_signal_connect(tx1_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)(long)((i<<4)+0));
  
        GtkWidget *tx2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(tx1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx2_b), band->alexTxAntenna==1);
        gtk_widget_show(tx2_b);
        gtk_grid_attach(GTK_GRID(grid),tx2_b,9,i+2,1,1);
        g_signal_connect(tx2_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)(long)((i<<4)+1));
  
        GtkWidget *tx3_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(tx2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx3_b), band->alexTxAntenna==2);
        gtk_widget_show(tx3_b);
        gtk_grid_attach(GTK_GRID(grid),tx3_b,10,i+2,1,1);
        g_signal_connect(tx3_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)(long)((i<<4)+2));
      }
    }
}

static void show_xvtr() {
  int i;
    for(i=0;i<XVTRS;i++) {
      BAND *band=band_get_band(BANDS+i);
      if(strlen(band->title)>0) {
        GtkWidget *band_label=gtk_label_new(band->title);
        //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
        gtk_widget_show(band_label);
        gtk_grid_attach(GTK_GRID(grid),band_label,0,i+2,1,1);

        GtkWidget *rx1_b=gtk_radio_button_new(NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_b), band->alexRxAntenna==0);
        gtk_widget_show(rx1_b);
        gtk_grid_attach(GTK_GRID(grid),rx1_b,1,i+2,1,1);
        g_signal_connect(rx1_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+0));

        GtkWidget *rx2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx2_b), band->alexRxAntenna==1);
        gtk_widget_show(rx2_b);
        gtk_grid_attach(GTK_GRID(grid),rx2_b,2,i+2,1,1);
        g_signal_connect(rx2_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+1));

        GtkWidget *rx3_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx3_b), band->alexRxAntenna==2);
        gtk_widget_show(rx3_b);
        gtk_grid_attach(GTK_GRID(grid),rx3_b,3,i+2,1,1);
        g_signal_connect(rx3_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+2));

        GtkWidget *ext1_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx3_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ext1_b), band->alexRxAntenna==3);
        gtk_widget_show(ext1_b);
        gtk_grid_attach(GTK_GRID(grid),ext1_b,4,i+2,1,1);
        g_signal_connect(ext1_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+3));

        GtkWidget *ext2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(ext1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ext2_b), band->alexRxAntenna==4);
        gtk_widget_show(ext2_b);
        gtk_grid_attach(GTK_GRID(grid),ext2_b,5,i+2,1,1);
        g_signal_connect(ext2_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+4));

        GtkWidget *xvtr_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(ext2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (xvtr_b), band->alexRxAntenna==5);
        gtk_widget_show(xvtr_b);
        gtk_grid_attach(GTK_GRID(grid),xvtr_b,6,i+2,1,1);
        g_signal_connect(xvtr_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+5));

        GtkWidget *ant_band_label=gtk_label_new(band->title);
        //gtk_widget_override_font(ant_band_label, pango_font_description_from_string("Arial 18"));
        gtk_widget_show(ant_band_label);
        gtk_grid_attach(GTK_GRID(grid),ant_band_label,7,i+2,1,1);
  
        GtkWidget *tx1_b=gtk_radio_button_new(NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx1_b), band->alexTxAntenna==0);
        gtk_widget_show(tx1_b);
        gtk_grid_attach(GTK_GRID(grid),tx1_b,8,i+2,1,1);
        g_signal_connect(tx1_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+0));
  
        GtkWidget *tx2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(tx1_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx2_b), band->alexTxAntenna==1);
        gtk_widget_show(tx2_b);
        gtk_grid_attach(GTK_GRID(grid),tx2_b,9,i+2,1,1);
        g_signal_connect(tx2_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+1));
  
        GtkWidget *tx3_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(tx2_b));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx3_b), band->alexTxAntenna==2);
        gtk_widget_show(tx3_b);
        gtk_grid_attach(GTK_GRID(grid),tx3_b,10,i+2,1,1);
        g_signal_connect(tx3_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)(long)(((i+BANDS)<<4)+2));
      }
    }
}

static void hf_rb_cb(GtkWidget *widget,GdkEventButton *event, gpointer data) {
  int i;
  for(i=XVTRS-1;i>=0;i--) {
    gtk_grid_remove_row (GTK_GRID(grid),i+2);
  }
  show_hf();
}

static void xvtr_rb_cb(GtkWidget *widget,GdkEventButton *event, gpointer data) {
  int i;
  for(i=BANDS-1;i>=0;i--) {
    gtk_grid_remove_row (GTK_GRID(grid),i+2);
  }
  show_xvtr();
}

static void newpa_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    new_pa_board=1;
  } else {
    new_pa_board=0;
  }
}

void ant_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - ANT");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

#ifdef SOAPYSDR
  if(radio->device!=SOAPYSDR_USB_DEVICE) {
#endif
    GtkWidget *hf_rb=gtk_radio_button_new_with_label(NULL,"HF");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hf_rb),TRUE);
    g_signal_connect(hf_rb,"toggled",G_CALLBACK(hf_rb_cb),NULL);
    gtk_grid_attach(GTK_GRID(grid),hf_rb,1,0,1,1);

    GtkWidget *xvtr_rb=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(hf_rb),"XVTR");
    g_signal_connect(xvtr_rb,"toggled",G_CALLBACK(xvtr_rb_cb),NULL);
    gtk_grid_attach(GTK_GRID(grid),xvtr_rb,2,0,1,1);

#ifdef SOAPYSDR
  }
#endif

  if ((protocol == NEW_PROTOCOL      && (device == NEW_DEVICE_HERMES || device == NEW_DEVICE_ANGELIA || device == NEW_DEVICE_ORION)) ||
      (protocol == ORIGINAL_PROTOCOL && (device == DEVICE_HERMES     || device == DEVICE_ANGELIA     || device == DEVICE_ORION))) {

      //
      // ANAN-100/200: There is an "old" (Rev. 15/16) and "new" (Rev. 24) PA board
      //               around which differs in relay settings for using EXT1,2 and
      //               differs in how to do PS feedback.
      //
      GtkWidget *new_pa_b = gtk_check_button_new_with_label("ANAN 100/200 new PA board");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(new_pa_b), new_pa_board);
      gtk_grid_attach(GTK_GRID(grid), new_pa_b, 3, 0, 1, 1);
      g_signal_connect(new_pa_b, "toggled", G_CALLBACK(newpa_cb), NULL);
  }


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

    show_hf();
  }

#ifdef SOAPYSDR
  if(radio->device==SOAPYSDR_USB_DEVICE) {
    int i;

g_print("rx_antennas=%d\n",radio->info.soapy.rx_antennas);
    if(radio->info.soapy.rx_antennas>0) {
      GtkWidget *antenna_label=gtk_label_new("RX Antenna:");
      gtk_grid_attach(GTK_GRID(grid),antenna_label,0,1,1,1);
      adc0_antenna_combo_box=gtk_combo_box_text_new();

      for(i=0;i<radio->info.soapy.rx_antennas;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(adc0_antenna_combo_box),NULL,radio->info.soapy.rx_antenna[i]);
      }

      gtk_combo_box_set_active(GTK_COMBO_BOX(adc0_antenna_combo_box),adc[0].antenna);
      g_signal_connect(adc0_antenna_combo_box,"changed",G_CALLBACK(adc0_antenna_cb),&adc[0]);
      gtk_grid_attach(GTK_GRID(grid),adc0_antenna_combo_box,1,1,1,1);
    }

    g_print("tx_antennas=%d\n",radio->info.soapy.tx_antennas);
    if(radio->info.soapy.tx_antennas>0) {
      GtkWidget *antenna_label=gtk_label_new("TX Antenna:");
      gtk_grid_attach(GTK_GRID(grid),antenna_label,0,2,1,1);
      dac0_antenna_combo_box=gtk_combo_box_text_new();

      for(i=0;i<radio->info.soapy.tx_antennas;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dac0_antenna_combo_box),NULL,radio->info.soapy.tx_antenna[i]);
      }

      gtk_combo_box_set_active(GTK_COMBO_BOX(dac0_antenna_combo_box),dac[0].antenna);
      g_signal_connect(dac0_antenna_combo_box,"changed",G_CALLBACK(dac0_antenna_cb),&dac[0]);
      gtk_grid_attach(GTK_GRID(grid),dac0_antenna_combo_box,1,2,1,1);
    }

  }
#endif

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

