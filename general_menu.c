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
#include "general_menu.h"
#include "band.h"
#include "filter.h"
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
}

static void vfo_divisor_value_changed_cb(GtkWidget *widget, gpointer data) {
  vfo_encoder_divisor=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void toolbar_dialog_buttons_cb(GtkWidget *widget, gpointer data) {
  toolbar_dialog_buttons=toolbar_dialog_buttons==1?0:1;
  update_toolbar_labels();
}

static void ptt_cb(GtkWidget *widget, gpointer data) {
  mic_ptt_enabled=mic_ptt_enabled==1?0:1;
}

static void ptt_ring_cb(GtkWidget *widget, gpointer data) {
  mic_ptt_tip_bias_ring=0;
}

static void ptt_tip_cb(GtkWidget *widget, gpointer data) {
  mic_ptt_tip_bias_ring=1;
}

static void bias_cb(GtkWidget *widget, gpointer data) {
  mic_bias_enabled=mic_bias_enabled==1?0:1;
}

static void apollo_cb(GtkWidget *widget, gpointer data);

static void alex_cb(GtkWidget *widget, gpointer data) {
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
    setFrequency(entry->frequencyA);
    setMode(entry->mode);
    FILTER* band_filters=filters[entry->mode];
    FILTER* band_filter=&band_filters[entry->filter];
    setFilter(band_filter->low,band_filter->high);
    set_alex_rx_antenna(band->alexRxAntenna);
    set_alex_tx_antenna(band->alexTxAntenna);
    set_alex_attenuation(band->alexAttenuation);
  }
}

static void apollo_cb(GtkWidget *widget, gpointer data) {
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
    setFrequency(entry->frequencyA);
    setMode(entry->mode);
    FILTER* band_filters=filters[entry->mode];
    FILTER* band_filter=&band_filters[entry->filter];
    setFilter(band_filter->low,band_filter->high);
  }
}
/*
static void apollo_tuner_cb(GtkWidget *widget, gpointer data) {
  apollo_tuner=apollo_tuner==1?0:1;
  if(protocol==NEW_PROTOCOL) {
    tuner_changed();
  }
}

static void pa_cb(GtkWidget *widget, gpointer data) {
  pa=pa==1?0:1;
  if(protocol==NEW_PROTOCOL) {
    pa_changed();
  }
}
*/

static void rx_dither_cb(GtkWidget *widget, gpointer data) {
  rx_dither=rx_dither==1?0:1;
  if(protocol==NEW_PROTOCOL) {
  }
}

static void rx_random_cb(GtkWidget *widget, gpointer data) {
  rx_random=rx_random==1?0:1;
  if(protocol==NEW_PROTOCOL) {
  }
}

static void rx_preamp_cb(GtkWidget *widget, gpointer data) {
  rx_preamp=rx_preamp==1?0:1;
  if(protocol==NEW_PROTOCOL) {
  }
}

static void sample_rate_cb(GtkWidget *widget, gpointer data) {
  if(protocol==ORIGINAL_PROTOCOL) {
    old_protocol_new_sample_rate((int)data);
  } else {
    new_protocol_new_sample_rate((int)data);
  }
}

void general_menu(GtkWidget *parent) {
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
  //gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close General");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *vfo_divisor_label=gtk_label_new("VFO Encoder Divisor: ");
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor_label,4,1,1,1);

  GtkWidget *vfo_divisor=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(vfo_divisor),(double)vfo_encoder_divisor);
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor,4,2,1,1);
  g_signal_connect(vfo_divisor,"value_changed",G_CALLBACK(vfo_divisor_value_changed_cb),NULL);

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *rx_dither_b=gtk_check_button_new_with_label("Dither");
    //gtk_widget_override_font(rx_dither_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx_dither_b), rx_dither);
    gtk_grid_attach(GTK_GRID(grid),rx_dither_b,1,4,1,1);
    g_signal_connect(rx_dither_b,"toggled",G_CALLBACK(rx_dither_cb),NULL);

    GtkWidget *rx_random_b=gtk_check_button_new_with_label("Random");
    //gtk_widget_override_font(rx_random_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx_random_b), rx_random);
    gtk_grid_attach(GTK_GRID(grid),rx_random_b,1,5,1,1);
    g_signal_connect(rx_random_b,"toggled",G_CALLBACK(rx_random_cb),NULL);

    if((protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION) ||
       (protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION2) ||
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_ORION)) {

      GtkWidget *ptt_ring_b=gtk_radio_button_new_with_label(NULL,"PTT On Ring, Mic and Bias on Tip");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_ring_b), mic_ptt_tip_bias_ring==0);
      gtk_grid_attach(GTK_GRID(grid),ptt_ring_b,3,1,1,1);
      g_signal_connect(ptt_ring_b,"pressed",G_CALLBACK(ptt_ring_cb),NULL);

      GtkWidget *ptt_tip_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ptt_ring_b),"PTT On Tip, Mic and Bias on Ring");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_tip_b), mic_ptt_tip_bias_ring==1);
      gtk_grid_attach(GTK_GRID(grid),ptt_tip_b,3,2,1,1);
      g_signal_connect(ptt_tip_b,"pressed",G_CALLBACK(ptt_tip_cb),NULL);

      GtkWidget *ptt_b=gtk_check_button_new_with_label("PTT Enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_b), mic_ptt_enabled);
      gtk_grid_attach(GTK_GRID(grid),ptt_b,3,3,1,1);
      g_signal_connect(ptt_b,"toggled",G_CALLBACK(ptt_cb),NULL);

      GtkWidget *bias_b=gtk_check_button_new_with_label("BIAS Enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bias_b), mic_bias_enabled);
      gtk_grid_attach(GTK_GRID(grid),bias_b,3,4,1,1);
      g_signal_connect(bias_b,"toggled",G_CALLBACK(bias_cb),NULL);
    }

    GtkWidget *alex_b=gtk_check_button_new_with_label("ALEX");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alex_b), filter_board==ALEX);
    gtk_grid_attach(GTK_GRID(grid),alex_b,1,1,1,1);

    GtkWidget *apollo_b=gtk_check_button_new_with_label("APOLLO");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apollo_b), filter_board==APOLLO);
    gtk_grid_attach(GTK_GRID(grid),apollo_b,1,2,1,1);

    g_signal_connect(alex_b,"toggled",G_CALLBACK(alex_cb),apollo_b);
    g_signal_connect(apollo_b,"toggled",G_CALLBACK(apollo_cb),alex_b);

  }

  GtkWidget *sample_rate_label=gtk_label_new("Sample Rate:");
  gtk_grid_attach(GTK_GRID(grid),sample_rate_label,0,1,1,1);

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *sample_rate_48=gtk_radio_button_new_with_label(NULL,"48000");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_48), sample_rate==48000);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_48,0,2,1,1);
    g_signal_connect(sample_rate_48,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)48000);

    GtkWidget *sample_rate_96=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_48),"96000");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_96), sample_rate==96000);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_96,0,3,1,1);
    g_signal_connect(sample_rate_96,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)96000);

    GtkWidget *sample_rate_192=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_96),"192000");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_192), sample_rate==192000);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_192,0,4,1,1);
    g_signal_connect(sample_rate_192,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)192000);

    GtkWidget *sample_rate_384=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_192),"384000");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_384), sample_rate==384000);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_384,0,5,1,1);
    g_signal_connect(sample_rate_384,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)384000);

    if(protocol==NEW_PROTOCOL) {
      GtkWidget *sample_rate_768=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_384),"768000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_768), sample_rate==768000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_768,0,6,1,1);
      g_signal_connect(sample_rate_768,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)768000);

      GtkWidget *sample_rate_1536=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_768),"1536000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1536), sample_rate==1536000);
        gtk_grid_attach(GTK_GRID(grid),sample_rate_1536,0,7,1,1);
      g_signal_connect(sample_rate_1536,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1536000);

#ifdef raspberrypi
      gtk_widget_set_sensitive(sample_rate_768,FALSE);
      gtk_widget_set_sensitive(sample_rate_1536,FALSE);
#endif
    }

  }

#ifdef LIMESDR
  if(protocol==LIMESDR_PROTOCOL) {
    GtkWidget *sample_rate_1M=gtk_radio_button_new_with_label(NULL,"1000000");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1M), sample_rate==1000000);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_1M,0,2,1,1);
    g_signal_connect(sample_rate_1M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1000000);

    GtkWidget *sample_rate_2M=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_1M),"2000000");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_2M), sample_rate==2000000);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_2M,0,3,1,1);
    g_signal_connect(sample_rate_2M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)2000000);

  }
#endif


  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

