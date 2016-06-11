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
#ifdef INCLUDE_GPIO
#include "gpio.h"
#endif
#include "toolbar.h"
#include "mode.h"
#include "filter.h"
#include "frequency.h"
#include "bandstack.h"
#include "band.h"
#include "discovered.h"
#include "new_protocol.h"
#include "vfo.h"
#include "alex.h"
#include "agc.h"
#include "channel.h"
#include "wdsp.h"
#include "radio.h"
#include "property.h"

int function=0;

static int width;
static int height;

static GtkWidget *parent_window;
static GtkWidget *toolbar;

static GtkWidget *sim_band;
static GtkWidget *sim_bandstack;
static GtkWidget *sim_mode;
static GtkWidget *sim_filter;
static GtkWidget *sim_agc;
static GtkWidget *sim_noise;
static GtkWidget *sim_function;
static GtkWidget *sim_mox;


static GtkWidget *last_band;
static GtkWidget *last_mode;
static GtkWidget *last_filter;

static GdkRGBA white;
static GdkRGBA gray;

static void band_select_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *label;
  int b=(int)data;
  BANDSTACK_ENTRY *entry;
  if(b==band_get_current()) {
    entry=bandstack_entry_next();
  } else {
    BAND* band=band_set_current(b);
    entry=bandstack_entry_get_current();
    gtk_widget_override_background_color(last_band, GTK_STATE_NORMAL, &white);
    last_band=widget;
    gtk_widget_override_background_color(last_band, GTK_STATE_NORMAL, &gray);
  }
  setMode(entry->mode);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);
  setFrequency(entry->frequencyA);

  BAND *band=band_get_current_band();
  set_alex_rx_antenna(band->alexRxAntenna);
  set_alex_tx_antenna(band->alexTxAntenna);
  set_alex_attenuation(band->alexAttenuation);

  vfo_update(NULL);

  setFrequency(entry->frequencyA);
}

static void band_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Band",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  GtkWidget *b;
  int i;
  for(i=0;i<BANDS;i++) {
#ifdef LIMESDR
    if(protocol!=LIMESDR_PROTOCOL) {
      if(i>=band70 && i<=band3400) {
        continue;
      }
    }
#endif
    BAND* band=band_get_band(i);
    GtkWidget *b=gtk_button_new_with_label(band->title);
    gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &white);
    //gtk_widget_override_font(b, pango_font_description_from_string("Arial 20"));
    if(i==band_get_current()) {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &gray);
      last_band=b;
    }
    gtk_widget_show(b);
    gtk_grid_attach(GTK_GRID(grid),b,i%5,i/5,1,1);
    g_signal_connect(b,"clicked",G_CALLBACK(band_select_cb),(gpointer *)i);
  }
  
  gtk_container_add(GTK_CONTAINER(content),grid);

  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));
  
}

static void mode_select_cb(GtkWidget *widget, gpointer data) {
  int m=(int)data;
  BANDSTACK_ENTRY *entry;
  entry=bandstack_entry_get_current();
  entry->mode=m;
  setMode(entry->mode);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);
  gtk_widget_override_background_color(last_mode, GTK_STATE_NORMAL, &white);
  last_mode=widget;
  gtk_widget_override_background_color(last_mode, GTK_STATE_NORMAL, &gray);
  vfo_update(NULL);
}

static void mode_cb(GtkWidget *widget, gpointer data) {
  BANDSTACK_ENTRY *entry=bandstack_entry_get_current();
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Mode",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *b;
  int i;
  for(i=0;i<MODES;i++) {
    GtkWidget *b=gtk_button_new_with_label(mode_string[i]);
    if(i==entry->mode) {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &gray);
      last_mode=b;
    } else {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &white);
    }
    //gtk_widget_override_font(b, pango_font_description_from_string("Arial 20"));
    gtk_widget_show(b);
    gtk_grid_attach(GTK_GRID(grid),b,i%5,i/5,1,1);
    g_signal_connect(b,"pressed",G_CALLBACK(mode_select_cb),(gpointer *)i);
  }
  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}

static void filter_select_cb(GtkWidget *widget, gpointer data) {
  int f=(int)data;
  BANDSTACK_ENTRY *entry;
  entry=bandstack_entry_get_current();
  entry->filter=f;
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);
  gtk_widget_override_background_color(last_filter, GTK_STATE_NORMAL, &white);
  last_filter=widget;
  gtk_widget_override_background_color(last_filter, GTK_STATE_NORMAL, &gray);
  vfo_update(NULL);
}

static void filter_cb(GtkWidget *widget, gpointer data) {
  BANDSTACK_ENTRY *entry=bandstack_entry_get_current();
  FILTER* band_filters=filters[entry->mode];
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Mode",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *b;
  int i;
  for(i=0;i<FILTERS;i++) {
    FILTER* band_filter=&band_filters[i];
    GtkWidget *b=gtk_button_new_with_label(band_filters[i].title);
    //gtk_widget_override_font(b, pango_font_description_from_string("Arial 20"));
    if(i==entry->filter) {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &gray);
      last_filter=b;
    } else {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &white);
    }
    gtk_widget_show(b);
    gtk_grid_attach(GTK_GRID(grid),b,i%5,i/5,1,1);
    g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer *)i);
  }
  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}

static void agc_select_cb(GtkWidget *widget, gpointer data) {
  agc=(int)data;
  SetRXAAGCMode(CHANNEL_RX0, agc);
}

static void update_noise() {
  SetRXAANRRun(CHANNEL_RX0, nr);
  SetRXAEMNRRun(CHANNEL_RX0, nr2);
  SetRXAANFRun(CHANNEL_RX0, anf);
  SetRXASNBARun(CHANNEL_RX0, snb);
  vfo_update(NULL);
}

static void nr_none_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=0;
  nb2=0;
  anf=0;
  snb=0;
  update_noise();
}

static void nr_cb(GtkWidget *widget, gpointer data) {
  nr=1;
  nr2=0;
  nb=0;
  nb2=0;
  anf=0;
  snb=0;
  update_noise();
}

static void nr2_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=1;
  nb=0;
  nb2=0;
  anf=0;
  snb=0;
  update_noise();
}

static void nb_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=1;
  nb2=0;
  anf=0;
  snb=0;
  update_noise();
}

static void nb2_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=0;
  nb2=1;
  anf=0;
  snb=0;
  update_noise();
}

static void anf_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=0;
  nb2=0;
  anf=1;
  snb=0;
  update_noise();
}

static void snb_cb(GtkWidget *widget, gpointer data) {
  nr=0;
  nr2=0;
  nb=0;
  nb2=0;
  anf=0;
  snb=1;
  update_noise();
}

static void audio_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Audio",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *b_off=gtk_radio_button_new_with_label(NULL,"Off"); 
  //gtk_widget_override_font(b_off, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_off), agc==AGC_OFF);
  gtk_widget_show(b_off);
  gtk_grid_attach(GTK_GRID(grid),b_off,0,0,2,1);
  g_signal_connect(b_off,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_OFF);

  GtkWidget *b_long=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_off),"Long"); 
  //gtk_widget_override_font(b_long, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_long), agc==AGC_LONG);
  gtk_widget_show(b_long);
  gtk_grid_attach(GTK_GRID(grid),b_long,0,1,2,1);
  g_signal_connect(b_long,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_LONG);

  GtkWidget *b_slow=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_long),"Slow"); 
  //gtk_widget_override_font(b_slow, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_slow), agc==AGC_SLOW);
  gtk_widget_show(b_slow);
  gtk_grid_attach(GTK_GRID(grid),b_slow,0,2,2,1);
  g_signal_connect(b_slow,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_SLOW);

  GtkWidget *b_medium=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_slow),"Medium"); 
  //gtk_widget_override_font(b_medium, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_medium), agc==AGC_MEDIUM);
  gtk_widget_show(b_medium);
  gtk_grid_attach(GTK_GRID(grid),b_medium,0,3,2,1);
  g_signal_connect(b_medium,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_MEDIUM);

  GtkWidget *b_fast=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_medium),"Fast"); 
  //gtk_widget_override_font(b_fast, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_fast), agc==AGC_FAST);
  gtk_widget_show(b_fast);
  gtk_grid_attach(GTK_GRID(grid),b_fast,0,4,2,1);
  g_signal_connect(b_fast,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_FAST);

  GtkWidget *b_nr_none=gtk_radio_button_new_with_label(NULL,"None");
  //gtk_widget_override_font(b_none, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr_none), nr_none==1);
  gtk_widget_show(b_nr_none);
  gtk_grid_attach(GTK_GRID(grid),b_nr_none,2,0,2,1);
  g_signal_connect(b_nr_none,"pressed",G_CALLBACK(nr_none_cb),NULL);

  GtkWidget *b_nr=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr_none),"NR");
  //gtk_widget_override_font(b_nr, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr), nr==1);
  gtk_widget_show(b_nr);
  gtk_grid_attach(GTK_GRID(grid),b_nr,2,1,2,1);
  g_signal_connect(b_nr,"pressed",G_CALLBACK(nr_cb),NULL);

  GtkWidget *b_nr2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr),"NR2");
  //gtk_widget_override_font(b_nr2, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr2), nr2==1);
  gtk_widget_show(b_nr2);
  gtk_grid_attach(GTK_GRID(grid),b_nr2,2,2,2,1);
  g_signal_connect(b_nr2,"pressed",G_CALLBACK(nr2_cb),NULL);

/*
  GtkWidget *b_nb=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr2),"NB");
  //gtk_widget_override_font(b_nb, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nb), nb==1);
  gtk_widget_show(b_nb);
  gtk_grid_attach(GTK_GRID(grid),b_nb,2,3,2,1);
  g_signal_connect(b_nb,"pressed",G_CALLBACK(nb_cb),NULL);

  GtkWidget *b_nb2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nb),"NB2");
  //gtk_widget_override_font(b_nb2, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nb2), nb2==1);
  gtk_widget_show(b_nb2);
  gtk_grid_attach(GTK_GRID(grid),b_nb2,2,4,2,1);
  g_signal_connect(b_nb2,"pressed",G_CALLBACK(nb2_cb),NULL);
*/

  GtkWidget *b_anf=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_nr2),"ANF");
  //gtk_widget_override_font(b_anf, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_anf), anf==1);
  gtk_widget_show(b_anf);
  gtk_grid_attach(GTK_GRID(grid),b_anf,2,3,2,1);
  g_signal_connect(b_anf,"pressed",G_CALLBACK(anf_cb),NULL);

  GtkWidget *b_snb=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_anf),"SNB");
  //gtk_widget_override_font(b_snb, pango_font_description_from_string("Arial 16"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_snb), snb==1);
  gtk_widget_show(b_snb);
  gtk_grid_attach(GTK_GRID(grid),b_snb,2,4,2,1);
  g_signal_connect(b_snb,"pressed",G_CALLBACK(snb_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 16"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}

static void stop() {
  if(protocol==ORIGINAL_PROTOCOL) {
    old_protocol_stop();
  } else {
    new_protocol_stop();
  }
#ifdef INCLUDE_GPIO
  gpio_close();
#endif
}

static void yes_cb(GtkWidget *widget, gpointer data) {
  stop();
  _exit(0);
}

static void halt_cb(GtkWidget *widget, gpointer data) {
  stop();
  system("shutdown -h -P now");
  _exit(0);
}

static void exit_cb(GtkWidget *widget, gpointer data) {

  radioSaveState();

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Exit",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *label=gtk_label_new("Exit?");
  //gtk_widget_override_font(label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid),label,1,0,1,1);

  GtkWidget *b_yes=gtk_button_new_with_label("Yes");
  //gtk_widget_override_font(b_yes, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(b_yes);
  gtk_grid_attach(GTK_GRID(grid),b_yes,0,1,1,1);
  g_signal_connect(b_yes,"pressed",G_CALLBACK(yes_cb),NULL);

  GtkWidget *b_halt=gtk_button_new_with_label("Halt System");
  //gtk_widget_override_font(b_halt, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(b_halt);
  gtk_grid_attach(GTK_GRID(grid),b_halt,2,1,1,1);
  g_signal_connect(b_halt,"pressed",G_CALLBACK(halt_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Cancel",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}

static void cw_keyer_internal_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_internal=cw_keyer_internal==1?0:1;
  cw_changed();
}

static void cw_keyer_speed_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_speed=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_breakin_cb(GtkWidget *widget, gpointer data) {
  cw_breakin=cw_breakin==1?0:1;
  cw_changed();
}

static void cw_keyer_hang_time_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_hang_time=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_keys_reversed_cb(GtkWidget *widget, gpointer data) {
  cw_keys_reversed=cw_keys_reversed==1?0:1;
  cw_changed();
}

static void cw_keyer_mode_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_mode=(int)data;
  cw_changed();
}

static void vfo_divisor_value_changed_cb(GtkWidget *widget, gpointer data) {
  vfo_encoder_divisor=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_high_value_changed_cb(GtkWidget *widget, gpointer data) {
  panadapter_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_low_value_changed_cb(GtkWidget *widget, gpointer data) {
  panadapter_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_high_value_changed_cb(GtkWidget *widget, gpointer data) {
  waterfall_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_low_value_changed_cb(GtkWidget *widget, gpointer data) {
  waterfall_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_automatic_cb(GtkWidget *widget, gpointer data) {
  waterfall_automatic=waterfall_automatic==1?0:1;
}

static void config_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Audio",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *display_label=gtk_label_new("Display: ");
  //gtk_widget_override_font(display_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(display_label);
  gtk_grid_attach(GTK_GRID(grid),display_label,0,0,1,1);

  GtkWidget *panadapter_high_label=gtk_label_new("Panadapter High: ");
  //gtk_widget_override_font(panadapter_high_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_high_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_high_label,0,1,1,1);

  GtkWidget *panadapter_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(panadapter_high_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_high_r),(double)panadapter_high);
  gtk_widget_show(panadapter_high_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_high_r,1,1,1,1);
  g_signal_connect(panadapter_high_r,"value_changed",G_CALLBACK(panadapter_high_value_changed_cb),NULL);

  GtkWidget *panadapter_low_label=gtk_label_new("Panadapter Low: ");
  //gtk_widget_override_font(panadapter_low_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_low_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_low_label,0,2,1,1);

  GtkWidget *panadapter_low_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(panadapter_low_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_low_r),(double)panadapter_low);
  gtk_widget_show(panadapter_low_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_low_r,1,2,1,1);
  g_signal_connect(panadapter_low_r,"value_changed",G_CALLBACK(panadapter_low_value_changed_cb),NULL);

  GtkWidget *waterfall_automatic_label=gtk_label_new("Waterfall Automatic: ");
  //gtk_widget_override_font(waterfall_automatic_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_automatic_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_automatic_label,0,3,1,1);

  GtkWidget *waterfall_automatic_b=gtk_check_button_new();
  ////gtk_widget_override_font(waterfall_automatic_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (waterfall_automatic_b), waterfall_automatic);
  gtk_widget_show(waterfall_automatic_b);
  gtk_grid_attach(GTK_GRID(grid),waterfall_automatic_b,1,3,1,1);
  g_signal_connect(waterfall_automatic_b,"toggled",G_CALLBACK(waterfall_automatic_cb),NULL);

  GtkWidget *waterfall_high_label=gtk_label_new("Waterfall High: ");
  //gtk_widget_override_font(waterfall_high_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_high_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_high_label,0,4,1,1);

  GtkWidget *waterfall_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(waterfall_high_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(waterfall_high_r),(double)waterfall_high);
  gtk_widget_show(waterfall_high_r);
  gtk_grid_attach(GTK_GRID(grid),waterfall_high_r,1,4,1,1);
  g_signal_connect(waterfall_high_r,"value_changed",G_CALLBACK(waterfall_high_value_changed_cb),NULL);

  GtkWidget *waterfall_low_label=gtk_label_new("Waterfall Low: ");
  //gtk_widget_override_font(waterfall_low_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_low_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_low_label,0,5,1,1);

  GtkWidget *waterfall_low_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(waterfall_low_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(waterfall_low_r),(double)waterfall_low);
  gtk_widget_show(waterfall_low_r);
  gtk_grid_attach(GTK_GRID(grid),waterfall_low_r,1,5,1,1);
  g_signal_connect(waterfall_low_r,"value_changed",G_CALLBACK(waterfall_low_value_changed_cb),NULL);


  GtkWidget *vfo_encoder_label=gtk_label_new("VFO Encoder: ");
  //gtk_widget_override_font(vfo_encoder_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(vfo_encoder_label);
  gtk_grid_attach(GTK_GRID(grid),vfo_encoder_label,0,6,1,1);

  GtkWidget *vfo_divisor_label=gtk_label_new("Divisor: ");
  //gtk_widget_override_font(vfo_divisor_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(vfo_divisor_label);
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor_label,0,7,1,1);

  GtkWidget *vfo_divisor=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  //gtk_widget_override_font(vfo_divisor, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(vfo_divisor),(double)vfo_encoder_divisor);
  gtk_widget_show(vfo_divisor);
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor,1,7,1,1);
  g_signal_connect(vfo_divisor,"value_changed",G_CALLBACK(vfo_divisor_value_changed_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));
}

static void cw_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog=gtk_dialog_new_with_buttons("CW",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);


  GtkWidget *cw_keyer_internal_b=gtk_check_button_new_with_label("CW Internal - Speed (WPM)");
  //gtk_widget_override_font(cw_keyer_internal_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_internal_b), cw_keyer_internal);
  gtk_widget_show(cw_keyer_internal_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_internal_b,0,0,1,1);
  g_signal_connect(cw_keyer_internal_b,"toggled",G_CALLBACK(cw_keyer_internal_cb),NULL);

  GtkWidget *cw_keyer_speed_b=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  //gtk_widget_override_font(cw_keyer_speed_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_speed_b),(double)cw_keyer_speed);
  gtk_widget_show(cw_keyer_speed_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_speed_b,1,0,1,1);
  g_signal_connect(cw_keyer_speed_b,"value_changed",G_CALLBACK(cw_keyer_speed_value_changed_cb),NULL);

  GtkWidget *cw_breakin_b=gtk_check_button_new_with_label("CW Break In - Delay (ms)");
  //gtk_widget_override_font(cw_breakin_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_breakin_b), cw_breakin);
  gtk_widget_show(cw_breakin_b);
  gtk_grid_attach(GTK_GRID(grid),cw_breakin_b,0,1,1,1);
  g_signal_connect(cw_breakin_b,"toggled",G_CALLBACK(cw_breakin_cb),NULL);

  GtkWidget *cw_keyer_hang_time_b=gtk_spin_button_new_with_range(0.0,1000.0,1.0);
  //gtk_widget_override_font(cw_keyer_hang_time_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_hang_time_b),(double)cw_keyer_hang_time);
  gtk_widget_show(cw_keyer_hang_time_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_hang_time_b,1,1,1,1);
  g_signal_connect(cw_keyer_hang_time_b,"value_changed",G_CALLBACK(cw_keyer_hang_time_value_changed_cb),NULL);
  
  GtkWidget *cw_keyer_straight=gtk_radio_button_new_with_label(NULL,"CW KEYER STRAIGHT");
  //gtk_widget_override_font(cw_keyer_straight, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_straight), cw_keyer_mode==KEYER_STRAIGHT);
  gtk_widget_show(cw_keyer_straight);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_straight,0,2,1,1);
  g_signal_connect(cw_keyer_straight,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_STRAIGHT);

  GtkWidget *cw_keyer_mode_a=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(cw_keyer_straight),"CW KEYER MODE A");
  //gtk_widget_override_font(cw_keyer_mode_a, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_mode_a), cw_keyer_mode==KEYER_MODE_A);
  gtk_widget_show(cw_keyer_mode_a);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_mode_a,0,3,1,1);
  g_signal_connect(cw_keyer_mode_a,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_MODE_A);

  GtkWidget *cw_keyer_mode_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(cw_keyer_mode_a),"CW KEYER MODE B");
  //gtk_widget_override_font(cw_keyer_mode_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_mode_b), cw_keyer_mode==KEYER_MODE_B);
  gtk_widget_show(cw_keyer_mode_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_mode_b,0,4,1,1);
  g_signal_connect(cw_keyer_mode_b,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_MODE_B);

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));
}

/*
static void adc_cb(GtkWidget *widget, gpointer data) {
  int adc0=adc[0];
  adc[0]=adc[1];
  adc[1]=adc0;

  char label[16];
  gtk_grid_remove_row(GTK_GRID(toolbar_top_1),0);
  
  sprintf(label,"RX0=%d",adc[0]);
  GtkWidget *rx0=gtk_label_new(label);
  //gtk_widget_override_font(rx0, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(rx0);
  gtk_grid_attach(GTK_GRID(toolbar_top_1),rx0,0,0,1,1);
  
  sprintf(label,"RX1=%d",adc[1]);
  GtkWidget *rx1=gtk_label_new(label);
  //gtk_widget_override_font(rx1, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(rx1);
  gtk_grid_attach(GTK_GRID(toolbar_top_1),rx1,1,0,1,1);
}
*/

void lock_cb(GtkWidget *widget, gpointer data) {
  locked=locked==1?0:1;
  vfo_update(NULL);
}

void mox_cb(GtkWidget *widget, gpointer data) {
  if(getTune()==1) {
    setTune(0);
  }
  if(canTransmit() || tx_out_of_band) {
    setMox(getMox()==0?1:0);
    vfo_update(NULL);
  }
}

int ptt_update(void *data) {
  BANDSTACK_ENTRY *entry;
  entry=bandstack_entry_get_current();
  int ptt=(int)data;
  if((entry->mode==modeCWL || entry->mode==modeCWU) && cw_keyer_internal==1) {
    //if(ptt!=getMox()) {
    //  mox_cb(NULL,NULL);
    //}
  } else {
    mox_cb(NULL,NULL);
  }
  return 0;
}

void tune_cb(GtkWidget *widget, gpointer data) {
  if(getMox()==1) {
    setMox(0);
  }
  if(canTransmit() || tx_out_of_band) {
    setTune(getTune()==0?1:0);
    vfo_update(NULL);
  }
}

void sim_band_cb(GtkWidget *widget, gpointer data) {
  BAND* band;
  BANDSTACK_ENTRY *entry;
fprintf(stderr,"sim_band_cb\n");
  int b=band_get_current();
  if(function) {
    b--;
    if(b<0) {
      b=BANDS-1;
    }
#ifdef LIMESDR
    if(protocol!=LIMESDR_PROTOCOL) {
      if(b==band3400) {
        b=band6;
      }
    }
#endif
  } else {
    b++;
    if(b>=BANDS) {
      b=0;
    }
#ifdef LIMESDR
    if(protocol!=LIMESDR_PROTOCOL) {
      if(b==band70) { 
        b=bandGen;
      }
    }
#endif
  }
  band=band_set_current(b);
  entry=bandstack_entry_get_current();

  setFrequency(entry->frequencyA);
  setMode(entry->mode);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);

  band=band_get_current_band();
  set_alex_rx_antenna(band->alexRxAntenna);
  set_alex_tx_antenna(band->alexTxAntenna);
  set_alex_attenuation(band->alexAttenuation);
  vfo_update(NULL);
}

void sim_bandstack_cb(GtkWidget *widget, gpointer data) {
  BANDSTACK_ENTRY *entry;
  fprintf(stderr,"sim_bandstack_cb\n");
  if(function) {
    entry=bandstack_entry_previous();
  } else {
    entry=bandstack_entry_next();
  }
  setFrequency(entry->frequencyA);
  setMode(entry->mode);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);
  vfo_update(NULL);
}

void sim_mode_cb(GtkWidget *widget, gpointer data) {
  BAND* band;
  BANDSTACK_ENTRY *entry;

fprintf(stderr,"sim_mode_cb\n");
  band=band_get_current_band();
  entry=bandstack_entry_get_current();
  if(function) {
    entry->mode--;
    if(entry->mode<0) {
      entry->mode=MODES-1;
    }
  } else {
    entry->mode++;
    if(entry->mode>=MODES) {
      entry->mode=0;
    }
  }
  setMode(entry->mode);

fprintf(stderr,"sim_mode_cb: entry->mode=%d entry->filter=%d\n",entry->mode,entry->filter);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);

  vfo_update(NULL);
}

void sim_filter_cb(GtkWidget *widget, gpointer data) {
  BAND* band;
  BANDSTACK_ENTRY *entry;

fprintf(stderr,"sim_filter_cb\n");
  band=band_get_current_band();
  entry=bandstack_entry_get_current();
  // note order of filter reversed (largest first)
  if(function) {
    entry->filter++;
    if(entry->filter>=FILTERS) {
      entry->filter=0;
    }
  } else {
    entry->filter--;
    if(entry->filter<0) {
      entry->filter=FILTERS-1;
    }
  }

fprintf(stderr,"sim_filter_cb: entry->mode=%d entry->filter=%d\n",entry->mode,entry->filter);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);

  vfo_update(NULL);

}

void sim_agc_cb(GtkWidget *widget, gpointer data) {
  fprintf(stderr,"sim_agc_cb\n");
  if(function) {
    agc--;
    if(agc<0) {
      agc=3;
    }
  } else {
    agc++;
    if(agc>=4) {
      agc=0;
    }
  }
  SetRXAAGCMode(CHANNEL_RX0, agc);
  vfo_update(NULL);
}

void sim_noise_cb(GtkWidget *widget, gpointer data) {
  fprintf(stderr,"sim_noise_cb\n");
  if(function) {
    if(nr) {
      nr=0;
    } else if(nr2) {
      nr2=0;
      nr=1;
    } else if(anf) {
      anf=0;
      nr2=1;
    } else if(snb) {
      snb=0;
      anf=1;
    } else {
      snb=1;
    }
  } else {
    if(nr) {
      nr=0;
      nr2=1;
    } else if(nr2) {
      nr2=0;
      anf=1;
    } else if(anf) {
      anf=0;
      snb=1;
    } else if(snb) {
      snb=0;
    } else {
      nr=1;
    }
  }
  SetRXAANRRun(CHANNEL_RX0, nr);
  SetRXAEMNRRun(CHANNEL_RX0, nr2);
  SetRXAANFRun(CHANNEL_RX0, anf);
  SetRXASNBARun(CHANNEL_RX0, snb);
  vfo_update(NULL);

}

void update_toolbar() {
  if(function) {
    gtk_button_set_label(GTK_BUTTON(sim_band),"Band v");
    gtk_button_set_label(GTK_BUTTON(sim_bandstack),"BStack v");
    gtk_button_set_label(GTK_BUTTON(sim_mode),"Mode v");
    gtk_button_set_label(GTK_BUTTON(sim_filter),"Filter v");
    gtk_button_set_label(GTK_BUTTON(sim_agc),"AGC v");
    gtk_button_set_label(GTK_BUTTON(sim_noise),"Noise v");
    gtk_button_set_label(GTK_BUTTON(sim_mox),"Tune");
  } else {
    gtk_button_set_label(GTK_BUTTON(sim_band),"Band ^");
    gtk_button_set_label(GTK_BUTTON(sim_bandstack),"BStack ^");
    gtk_button_set_label(GTK_BUTTON(sim_mode),"Mode ^");
    gtk_button_set_label(GTK_BUTTON(sim_filter),"Filter ^");
    gtk_button_set_label(GTK_BUTTON(sim_agc),"AGC ^");
    gtk_button_set_label(GTK_BUTTON(sim_noise),"Noise ^");
    gtk_button_set_label(GTK_BUTTON(sim_mox),"Mox");
  }
}

void sim_function_cb(GtkWidget *widget, gpointer data) {
  fprintf(stderr,"sim_function_cb\n");
  function=function==1?0:1;
  update_toolbar();
  vfo_update(NULL);
}

void sim_mox_cb(GtkWidget *widget, gpointer data) {
  fprintf(stderr,"sim_pressed\n");
  if(function) {
    tune_cb((GtkWidget *)NULL, (gpointer)NULL);
  } else {
    mox_cb((GtkWidget *)NULL, (gpointer)NULL);
  }
}

GtkWidget *toolbar_init(int my_width, int my_height, GtkWidget* parent) {
    width=my_width;
    height=my_height;
    parent_window=parent;

    int button_width=width/8;

    fprintf(stderr,"toolbar_init: width=%d height=%d button_width=%d\n", width,height,button_width);

    white.red=1.0;
    white.green=1.0;
    white.blue=1.0;
    white.alpha=0.0;

    gray.red=0.25;
    gray.green=0.25;
    gray.blue=0.25;
    gray.alpha=0.0;

    toolbar=gtk_grid_new();
    gtk_widget_set_size_request (toolbar, width, height);
    gtk_grid_set_column_homogeneous(GTK_GRID(toolbar),TRUE);

    if(toolbar_simulate_buttons) {
      sim_band=gtk_button_new_with_label("Band ^");
      gtk_widget_set_size_request (sim_band, button_width, 0);
      //gtk_widget_override_font(sim_band, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(sim_band),"clicked",G_CALLBACK(sim_band_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),sim_band,0,0,4,1);

      sim_bandstack=gtk_button_new_with_label("BStack ^");
      gtk_widget_set_size_request (sim_bandstack, button_width, 0);
      //gtk_widget_override_font(sim_bandstack, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(sim_bandstack),"clicked",G_CALLBACK(sim_bandstack_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),sim_bandstack,4,0,4,1);

      sim_mode=gtk_button_new_with_label("Mode ^");
      //gtk_widget_override_font(sim_mode, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(sim_mode),"clicked",G_CALLBACK(sim_mode_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),sim_mode,8,0,4,1);

      sim_filter=gtk_button_new_with_label("Filter ^");
      //gtk_widget_override_font(sim_filter, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(sim_filter),"clicked",G_CALLBACK(sim_filter_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),sim_filter,12,0,4,1);

      sim_agc=gtk_button_new_with_label("AGC ^");
      //gtk_widget_override_font(sim_agc, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(sim_agc),"clicked",G_CALLBACK(sim_agc_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),sim_agc,16,0,4,1);

      sim_noise=gtk_button_new_with_label("Noise ^");
      //gtk_widget_override_font(sim_noise, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(sim_noise),"clicked",G_CALLBACK(sim_noise_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),sim_noise,20,0,4,1);

      sim_function=gtk_button_new_with_label("Function");
      //gtk_widget_override_font(sim_function, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(sim_function),"clicked",G_CALLBACK(sim_function_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),sim_function,24,0,4,1);

      sim_mox=gtk_button_new_with_label("Mox");
      //gtk_widget_override_font(sim_mox, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(sim_mox),"clicked",G_CALLBACK(sim_mox_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),sim_mox,28,0,4,1);

    } else {
      GtkWidget *band=gtk_button_new_with_label("Band");
      gtk_widget_set_size_request (band, button_width, 0);
      //gtk_widget_override_font(band, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(band),"clicked",G_CALLBACK(band_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),band,0,0,4,1);

      GtkWidget *mode=gtk_button_new_with_label("Mode");
      //gtk_widget_override_font(mode, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(mode),"clicked",G_CALLBACK(mode_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),mode,4,0,4,1);

      GtkWidget *filter=gtk_button_new_with_label("Filter");
      //gtk_widget_override_font(filter, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(filter),"clicked",G_CALLBACK(filter_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),filter,8,0,4,1);

      GtkWidget *audio=gtk_button_new_with_label("Audio");
      //gtk_widget_override_font(audio, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(audio),"clicked",G_CALLBACK(audio_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),audio,12,0,4,1);

      GtkWidget *lock=gtk_button_new_with_label("Lock");
      //gtk_widget_override_font(lock, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(lock),"clicked",G_CALLBACK(lock_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),lock,16,0,4,1);

      GtkWidget *tune=gtk_button_new_with_label("Tune");
      //gtk_widget_override_font(tune, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(tune),"clicked",G_CALLBACK(tune_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),tune,24,0,4,1);

      GtkWidget *tx=gtk_button_new_with_label("Mox");
      //gtk_widget_override_font(tx, pango_font_description_from_string("Arial 16"));
      g_signal_connect(G_OBJECT(tx),"clicked",G_CALLBACK(mox_cb),NULL);
      gtk_grid_attach(GTK_GRID(toolbar),tx,28,0,4,1);
    }

    gtk_widget_show_all(toolbar);

  return toolbar;
}
