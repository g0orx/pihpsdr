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
#include "radio_menu.h"
#include "band.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"

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

static void vfo_divisor_value_changed_cb(GtkWidget *widget, gpointer data) {
  vfo_encoder_divisor=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

/*
static void toolbar_dialog_buttons_cb(GtkWidget *widget, gpointer data) {
  toolbar_dialog_buttons=toolbar_dialog_buttons==1?0:1;
  update_toolbar_labels();
}
*/

static void ptt_cb(GtkWidget *widget, gpointer data) {
  mic_ptt_enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void ptt_ring_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    mic_ptt_tip_bias_ring=0;
  }
}

static void ptt_tip_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    mic_ptt_tip_bias_ring=1;
  }
}

static void bias_cb(GtkWidget *widget, gpointer data) {
  mic_bias_enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void apollo_cb(GtkWidget *widget, gpointer data);

static void alex_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    if(filter_board==ALEX) {
      filter_board=NONE;
    } else if(filter_board==NONE) {
      filter_board=ALEX;
    } else if(filter_board==APOLLO) {
      GtkWidget *w=(GtkWidget *)data;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
      filter_board=ALEX;
    }

    if(protocol==NEW_PROTOCOL) {
      filter_board_changed();
    }

    if(filter_board==ALEX) {
      BAND *band=band_get_current_band();
      BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
      setFrequency(entry->frequency);
      //setMode(entry->mode);
      set_mode(active_receiver,entry->mode);
      FILTER* band_filters=filters[entry->mode];
      FILTER* band_filter=&band_filters[entry->filter];
      //setFilter(band_filter->low,band_filter->high);
      set_filter(active_receiver,band_filter->low,band_filter->high);
      if(active_receiver->id==0) {
        set_alex_rx_antenna(band->alexRxAntenna);
        set_alex_tx_antenna(band->alexTxAntenna);
        set_alex_attenuation(band->alexAttenuation);
      }
    }
  }
}

static void apollo_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    if(filter_board==APOLLO) {
      filter_board=NONE;
    } else if(filter_board==NONE) {
      filter_board=APOLLO;
    } else if(filter_board==ALEX) {
      GtkWidget *w=(GtkWidget *)data;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
      filter_board=APOLLO;
    }
    if(protocol==NEW_PROTOCOL) {
      filter_board_changed();
    }

    if(filter_board==APOLLO) {
      BAND *band=band_get_current_band();
      BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
      setFrequency(entry->frequency);
      //setMode(entry->mode);
      set_mode(active_receiver,entry->mode);
      FILTER* band_filters=filters[entry->mode];
      FILTER* band_filter=&band_filters[entry->filter];
      //setFilter(band_filter->low,band_filter->high);
      set_filter(active_receiver,band_filter->low,band_filter->high);
    }
  }
}

static void sample_rate_cb(GtkWidget *widget, gpointer data) {
  radio_change_sample_rate((int)data);
}

static void receivers_cb(GtkWidget *widget, gpointer data) {
  radio_change_receivers((int)data);
}

static void rit_cb(GtkWidget *widget,gpointer data) {
  rit_increment=(int)data;
}

static void ck10mhz_cb(GtkWidget *widget, gpointer data) {
  atlas_clock_source_10mhz = (int)data;
}

static void ck128mhz_cb(GtkWidget *widget, gpointer data) {
  atlas_clock_source_128mhz=atlas_clock_source_128mhz==1?0:1;
}

static void micsource_cb(GtkWidget *widget, gpointer data) {
  atlas_mic_source=atlas_mic_source==1?0:1;
}

static void penelopetx_cb(GtkWidget *widget, gpointer data) {
  atlas_penelope=atlas_penelope==1?0:1;
}

void radio_menu(GtkWidget *parent) {
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

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int x=0;

  GtkWidget *receivers_label=gtk_label_new("Receivers: ");
  gtk_grid_attach(GTK_GRID(grid),receivers_label,x,1,1,1);
  
  GtkWidget *receivers_1=gtk_radio_button_new_with_label(NULL,"1");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (receivers_1), receivers==1);
  gtk_grid_attach(GTK_GRID(grid),receivers_1,x,2,1,1);
  g_signal_connect(receivers_1,"pressed",G_CALLBACK(receivers_cb),(gpointer *)1);

  GtkWidget *receivers_2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(receivers_1),"2");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (receivers_2), receivers==2);
  gtk_grid_attach(GTK_GRID(grid),receivers_2,x,3,1,1);
  g_signal_connect(receivers_2,"pressed",G_CALLBACK(receivers_cb),(gpointer *)2);

  x++;

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new("Sample Rate:");
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,x,1,1,1);

      GtkWidget *sample_rate_48=gtk_radio_button_new_with_label(NULL,"48000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_48), active_receiver->sample_rate==48000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_48,x,2,1,1);
      g_signal_connect(sample_rate_48,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)48000);

      GtkWidget *sample_rate_96=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_48),"96000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_96), active_receiver->sample_rate==96000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_96,x,3,1,1);
      g_signal_connect(sample_rate_96,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)96000);

      GtkWidget *sample_rate_192=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_96),"192000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_192), active_receiver->sample_rate==192000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_192,x,4,1,1);
      g_signal_connect(sample_rate_192,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)192000);

      GtkWidget *sample_rate_384=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_192),"384000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_384), active_receiver->sample_rate==384000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_384,x,5,1,1);
      g_signal_connect(sample_rate_384,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)384000);

      if(protocol==NEW_PROTOCOL) {
        GtkWidget *sample_rate_768=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_384),"768000");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_768), active_receiver->sample_rate==768000);
        gtk_grid_attach(GTK_GRID(grid),sample_rate_768,x,6,1,1);
        g_signal_connect(sample_rate_768,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)768000);
  
        GtkWidget *sample_rate_1536=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_768),"1536000");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1536), active_receiver->sample_rate==1536000);
          gtk_grid_attach(GTK_GRID(grid),sample_rate_1536,x,7,1,1);
        g_signal_connect(sample_rate_1536,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1536000);
  
  #ifdef GPIO
        gtk_widget_set_sensitive(sample_rate_768,FALSE);
        gtk_widget_set_sensitive(sample_rate_1536,FALSE);
  #endif
      }
      x++;
      }
      break;
  
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new("Sample Rate:");
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,x,1,1,1);

      GtkWidget *sample_rate_1M=gtk_radio_button_new_with_label(NULL,"1000000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1M), active_receiver->sample_rate==1000000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_1M,x,2,1,1);
      g_signal_connect(sample_rate_1M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1000000);
  
      GtkWidget *sample_rate_2M=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_1M),"2000000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_2M), active_receiver->sample_rate==2000000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_2M,x,3,1,1);
      g_signal_connect(sample_rate_2M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)2000000);
      x++;
      }
      break;
#endif
     

  }


  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {

    if((protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION) ||
       (protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION2) ||
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_ORION) ||
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_ORION2)) {

      GtkWidget *ptt_ring_b=gtk_radio_button_new_with_label(NULL,"PTT On Ring, Mic and Bias on Tip");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_ring_b), mic_ptt_tip_bias_ring==0);
      gtk_grid_attach(GTK_GRID(grid),ptt_ring_b,x,1,1,1);
      g_signal_connect(ptt_ring_b,"toggled",G_CALLBACK(ptt_ring_cb),NULL);

      GtkWidget *ptt_tip_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ptt_ring_b),"PTT On Tip, Mic and Bias on Ring");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_tip_b), mic_ptt_tip_bias_ring==1);
      gtk_grid_attach(GTK_GRID(grid),ptt_tip_b,x,2,1,1);
      g_signal_connect(ptt_tip_b,"toggled",G_CALLBACK(ptt_tip_cb),NULL);

      GtkWidget *ptt_b=gtk_check_button_new_with_label("PTT Enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_b), mic_ptt_enabled);
      gtk_grid_attach(GTK_GRID(grid),ptt_b,x,3,1,1);
      g_signal_connect(ptt_b,"toggled",G_CALLBACK(ptt_cb),NULL);

      GtkWidget *bias_b=gtk_check_button_new_with_label("BIAS Enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bias_b), mic_bias_enabled);
      gtk_grid_attach(GTK_GRID(grid),bias_b,x,4,1,1);
      g_signal_connect(bias_b,"toggled",G_CALLBACK(bias_cb),NULL);

      x++;
    }

    GtkWidget *alex_b=gtk_check_button_new_with_label("ALEX");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alex_b), filter_board==ALEX);
    gtk_grid_attach(GTK_GRID(grid),alex_b,x,1,1,1);

    GtkWidget *apollo_b=gtk_check_button_new_with_label("APOLLO");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apollo_b), filter_board==APOLLO);
    gtk_grid_attach(GTK_GRID(grid),apollo_b,x,2,1,1);

    g_signal_connect(alex_b,"toggled",G_CALLBACK(alex_cb),apollo_b);
    g_signal_connect(apollo_b,"toggled",G_CALLBACK(apollo_cb),alex_b);

    x++;
  }


  GtkWidget *rit_label=gtk_label_new("RIT step: ");
  gtk_grid_attach(GTK_GRID(grid),rit_label,x,1,1,1);

  GtkWidget *rit_1=gtk_radio_button_new_with_label(NULL,"1 Hz");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rit_1), rit_increment==1);
  gtk_grid_attach(GTK_GRID(grid),rit_1,x,2,1,1);
  g_signal_connect(rit_1,"pressed",G_CALLBACK(rit_cb),(gpointer *)1);

  GtkWidget *rit_10=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rit_1),"10");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rit_10), rit_increment==10);
  gtk_grid_attach(GTK_GRID(grid),rit_10,x,3,1,1);
  g_signal_connect(rit_10,"pressed",G_CALLBACK(rit_cb),(gpointer *)10);

  GtkWidget *rit_100=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rit_10),"100");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rit_100), rit_increment==100);
  gtk_grid_attach(GTK_GRID(grid),rit_100,x,4,1,1);
  g_signal_connect(rit_100,"pressed",G_CALLBACK(rit_cb),(gpointer *)100);

  x++;

  GtkWidget *vfo_divisor_label=gtk_label_new("VFO Encoder Divisor: ");
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor_label,x,1,1,1);

  GtkWidget *vfo_divisor=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(vfo_divisor),(double)vfo_encoder_divisor);
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor,x,2,1,1);
  g_signal_connect(vfo_divisor,"value_changed",G_CALLBACK(vfo_divisor_value_changed_cb),NULL);

  x++;

#ifdef USBOZY
  if (protocol==ORIGINAL_PROTOCOL && (device == DEVICE_OZY) || (device == DEVICE_METIS))
#else
  if (protocol==ORIGINAL_PROTOCOL && radio->device == DEVICE_METIS)
#endif
  {
    GtkWidget *atlas_label=gtk_label_new("Atlas bus: ");
    gtk_grid_attach(GTK_GRID(grid),atlas_label,x,1,1,1);

    GtkWidget *ck10mhz_1=gtk_radio_button_new_with_label(NULL,"10MHz clock=Atlas");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ck10mhz_1), atlas_clock_source_10mhz==0);
    gtk_grid_attach(GTK_GRID(grid),ck10mhz_1,x,2,1,1);
    g_signal_connect(ck10mhz_1,"toggled",G_CALLBACK(ck10mhz_cb),(gpointer *)0);

    GtkWidget *ck10mhz_2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ck10mhz_1),"10MHz clock=Penelope");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ck10mhz_2), atlas_clock_source_10mhz==1);
    gtk_grid_attach(GTK_GRID(grid),ck10mhz_2,x,3,1,1);
    g_signal_connect(ck10mhz_2,"toggled",G_CALLBACK(ck10mhz_cb),(gpointer *)1);

    GtkWidget *ck10mhz_3=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ck10mhz_2),"10MHz clock=Mercury");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ck10mhz_3), atlas_clock_source_10mhz==2);
    gtk_grid_attach(GTK_GRID(grid),ck10mhz_3,x,4,1,1);
    g_signal_connect(ck10mhz_3,"toggled",G_CALLBACK(ck10mhz_cb),(gpointer *)2);

    GtkWidget *ck128_b=gtk_check_button_new_with_label("122.88MHz ck=Mercury");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ck128_b), atlas_clock_source_128mhz);
    gtk_grid_attach(GTK_GRID(grid),ck128_b,x,5,1,1);
    g_signal_connect(ck128_b,"toggled",G_CALLBACK(ck128mhz_cb),NULL);

    GtkWidget *mic_src_b=gtk_check_button_new_with_label("Mic src=Penelope");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mic_src_b), atlas_mic_source);
    gtk_grid_attach(GTK_GRID(grid),mic_src_b,x,6,1,1);
    g_signal_connect(mic_src_b,"toggled",G_CALLBACK(micsource_cb),NULL);

    GtkWidget *pene_tx_b=gtk_check_button_new_with_label("Penelope TX");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pene_tx_b), atlas_penelope);
    gtk_grid_attach(GTK_GRID(grid),pene_tx_b,x,7,1,1);
    g_signal_connect(pene_tx_b,"toggled",G_CALLBACK(penelopetx_cb),NULL);
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

