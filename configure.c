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
#include <gdk/gdk.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "main.h"
#include "channel.h"
#include "discovered.h"
#include "radio.h"
#include "gpio.h"

#ifdef raspberrypi
#define INCLUDE_GPIO
#endif
#ifdef odroid
#define INCLUDE_GPIO
#endif
#ifdef up
#define INCLUDE_GPIO
#endif

static void sample_rate_cb(GtkWidget *widget, gpointer data) {
  sample_rate=(int)data;
}

static void display_panadapter_cb(GtkWidget *widget, gpointer data) {
  display_panadapter=display_panadapter==1?0:1;
}

static void display_waterfall_cb(GtkWidget *widget, gpointer data) {
  display_waterfall=display_waterfall==1?0:1;
}

static void display_sliders_cb(GtkWidget *widget, gpointer data) {
  display_sliders=display_sliders==1?0:1;
}

static void display_toolbar_cb(GtkWidget *widget, gpointer data) {
  display_toolbar=display_toolbar==1?0:1;
}

/*
static void toolbar_dialog_buttons_cb(GtkWidget *widget, gpointer data) {
  toolbar_dialog_buttons=toolbar_dialog_buttons==1?0:1;
}
*/

#ifdef INCLUDE_GPIO
void configure_gpio(GtkWidget *parent) {
  gpio_restore_state();

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Configure GPIO",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);


  GtkWidget *b_enable_vfo_encoder=gtk_check_button_new_with_label("Enable VFO");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_vfo_encoder), ENABLE_VFO_ENCODER);
  gtk_widget_show(b_enable_vfo_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_vfo_encoder,0,0,1,1);

  GtkWidget *vfo_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(vfo_a_label);
  gtk_grid_attach(GTK_GRID(grid),vfo_a_label,1,0,1,1);

  GtkWidget *vfo_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(vfo_a),VFO_ENCODER_A);
  gtk_widget_show(vfo_a);
  gtk_grid_attach(GTK_GRID(grid),vfo_a,2,0,1,1);

  GtkWidget *vfo_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(vfo_b_label);
  gtk_grid_attach(GTK_GRID(grid),vfo_b_label,3,0,1,1);

  GtkWidget *vfo_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(vfo_b),VFO_ENCODER_B);
  gtk_widget_show(vfo_b);
  gtk_grid_attach(GTK_GRID(grid),vfo_b,4,0,1,1);

  GtkWidget *b_enable_vfo_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_vfo_pullup), ENABLE_VFO_PULLUP);
  gtk_widget_show(b_enable_vfo_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_vfo_pullup,5,0,1,1);



  GtkWidget *b_enable_af_encoder=gtk_check_button_new_with_label("Enable AF");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_af_encoder), ENABLE_AF_ENCODER);
  gtk_widget_show(b_enable_af_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_af_encoder,0,1,1,1);

  GtkWidget *af_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(af_a_label);
  gtk_grid_attach(GTK_GRID(grid),af_a_label,1,1,1,1);

  GtkWidget *af_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(af_a),AF_ENCODER_A);
  gtk_widget_show(af_a);
  gtk_grid_attach(GTK_GRID(grid),af_a,2,1,1,1);

  GtkWidget *af_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(af_b_label);
  gtk_grid_attach(GTK_GRID(grid),af_b_label,3,1,1,1);

  GtkWidget *af_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(af_b),AF_ENCODER_B);
  gtk_widget_show(af_b);
  gtk_grid_attach(GTK_GRID(grid),af_b,4,1,1,1);

  GtkWidget *b_enable_af_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_af_pullup), ENABLE_AF_PULLUP);
  gtk_widget_show(b_enable_af_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_af_pullup,5,1,1,1);



  GtkWidget *b_enable_rf_encoder=gtk_check_button_new_with_label("Enable RF");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_rf_encoder), ENABLE_RF_ENCODER);
  gtk_widget_show(b_enable_rf_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_rf_encoder,0,2,1,1);

  GtkWidget *rf_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(rf_a_label);
  gtk_grid_attach(GTK_GRID(grid),rf_a_label,1,2,1,1);

  GtkWidget *rf_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(rf_a),RF_ENCODER_A);
  gtk_widget_show(rf_a);
  gtk_grid_attach(GTK_GRID(grid),rf_a,2,2,1,1);

  GtkWidget *rf_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(rf_b_label);
  gtk_grid_attach(GTK_GRID(grid),rf_b_label,3,2,1,1);

  GtkWidget *rf_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(rf_b),RF_ENCODER_B);
  gtk_widget_show(rf_b);
  gtk_grid_attach(GTK_GRID(grid),rf_b,4,2,1,1);

  GtkWidget *b_enable_rf_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_rf_pullup), ENABLE_RF_PULLUP);
  gtk_widget_show(b_enable_rf_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_rf_pullup,5,2,1,1);



  GtkWidget *b_enable_agc_encoder=gtk_check_button_new_with_label("Enable AGC");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_agc_encoder), ENABLE_AGC_ENCODER);
  gtk_widget_show(b_enable_agc_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_agc_encoder,0,3,1,1);

  GtkWidget *agc_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(agc_a_label);
  gtk_grid_attach(GTK_GRID(grid),agc_a_label,1,3,1,1);

  GtkWidget *agc_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(agc_a),AGC_ENCODER_A);
  gtk_widget_show(agc_a);
  gtk_grid_attach(GTK_GRID(grid),agc_a,2,3,1,1);

  GtkWidget *agc_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(agc_b_label);
  gtk_grid_attach(GTK_GRID(grid),agc_b_label,3,3,1,1);

  GtkWidget *agc_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(agc_b),AGC_ENCODER_B);
  gtk_widget_show(agc_b);
  gtk_grid_attach(GTK_GRID(grid),agc_b,4,3,1,1);

  GtkWidget *b_enable_agc_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_agc_pullup), ENABLE_AGC_PULLUP);
  gtk_widget_show(b_enable_agc_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_agc_pullup,5,3,1,1);



  GtkWidget *b_enable_band=gtk_check_button_new_with_label("Enable Band");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_band), ENABLE_BAND_BUTTON);
  gtk_widget_show(b_enable_band);
  gtk_grid_attach(GTK_GRID(grid),b_enable_band,0,4,1,1);

  GtkWidget *band_label=gtk_label_new("GPIO:");
  gtk_widget_show(band_label);
  gtk_grid_attach(GTK_GRID(grid),band_label,1,4,1,1);

  GtkWidget *band=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(band),BAND_BUTTON);
  gtk_widget_show(band);
  gtk_grid_attach(GTK_GRID(grid),band,2,4,1,1);


  GtkWidget *b_enable_mode=gtk_check_button_new_with_label("Enable Mode");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_mode), ENABLE_MODE_BUTTON);
  gtk_widget_show(b_enable_mode);
  gtk_grid_attach(GTK_GRID(grid),b_enable_mode,0,5,1,1);

  GtkWidget *mode_label=gtk_label_new("GPIO:");
  gtk_widget_show(mode_label);
  gtk_grid_attach(GTK_GRID(grid),mode_label,1,5,1,1);

  GtkWidget *mode=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(mode),MODE_BUTTON);
  gtk_widget_show(mode);
  gtk_grid_attach(GTK_GRID(grid),mode,2,5,1,1);


  GtkWidget *b_enable_filter=gtk_check_button_new_with_label("Enable Filter");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_filter), ENABLE_FILTER_BUTTON);
  gtk_widget_show(b_enable_filter);
  gtk_grid_attach(GTK_GRID(grid),b_enable_filter,0,6,1,1);

  GtkWidget *filter_label=gtk_label_new("GPIO:");
  gtk_widget_show(filter_label);
  gtk_grid_attach(GTK_GRID(grid),filter_label,1,6,1,1);

  GtkWidget *filter=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(filter),FILTER_BUTTON);
  gtk_widget_show(filter);
  gtk_grid_attach(GTK_GRID(grid),filter,2,6,1,1);


  GtkWidget *b_enable_noise=gtk_check_button_new_with_label("Enable Noise");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_noise), ENABLE_NOISE_BUTTON);
  gtk_widget_show(b_enable_noise);
  gtk_grid_attach(GTK_GRID(grid),b_enable_noise,0,7,1,1);

  GtkWidget *noise_label=gtk_label_new("GPIO:");
  gtk_widget_show(noise_label);
  gtk_grid_attach(GTK_GRID(grid),noise_label,1,7,1,1);

  GtkWidget *noise=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(noise),NOISE_BUTTON);
  gtk_widget_show(noise);
  gtk_grid_attach(GTK_GRID(grid),noise,2,7,1,1);


  GtkWidget *b_enable_agc=gtk_check_button_new_with_label("Enable AGC");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_agc), ENABLE_AGC_BUTTON);
  gtk_widget_show(b_enable_agc);
  gtk_grid_attach(GTK_GRID(grid),b_enable_agc,0,8,1,1);

  GtkWidget *agc_label=gtk_label_new("GPIO:");
  gtk_widget_show(agc_label);
  gtk_grid_attach(GTK_GRID(grid),agc_label,1,8,1,1);

  GtkWidget *agc=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(agc),AGC_BUTTON);
  gtk_widget_show(agc);
  gtk_grid_attach(GTK_GRID(grid),agc,2,8,1,1);


  GtkWidget *b_enable_mox=gtk_check_button_new_with_label("Enable MOX");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_mox), ENABLE_MOX_BUTTON);
  gtk_widget_show(b_enable_mox);
  gtk_grid_attach(GTK_GRID(grid),b_enable_mox,0,9,1,1);

  GtkWidget *mox_label=gtk_label_new("GPIO:");
  gtk_widget_show(mox_label);
  gtk_grid_attach(GTK_GRID(grid),mox_label,1,9,1,1);

  GtkWidget *mox=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(mox),MOX_BUTTON);
  gtk_widget_show(mox);
  gtk_grid_attach(GTK_GRID(grid),mox,2,9,1,1);


  GtkWidget *b_enable_function=gtk_check_button_new_with_label("Enable Function");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_function), ENABLE_FUNCTION_BUTTON);
  gtk_widget_show(b_enable_function);
  gtk_grid_attach(GTK_GRID(grid),b_enable_function,0,10,1,1);

  GtkWidget *function_label=gtk_label_new("GPIO:");
  gtk_widget_show(function_label);
  gtk_grid_attach(GTK_GRID(grid),function_label,1,10,1,1);

  GtkWidget *function=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(function),FUNCTION_BUTTON);
  gtk_widget_show(function);
  gtk_grid_attach(GTK_GRID(grid),function,2,10,1,1);



  gtk_container_add(GTK_CONTAINER(content),grid);

  gtk_container_add(GTK_CONTAINER(content),grid);

  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

  ENABLE_VFO_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_vfo_encoder))?1:0;
  VFO_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vfo_a));
  VFO_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vfo_b));
  ENABLE_VFO_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_vfo_pullup))?1:0;
  ENABLE_AF_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_af_encoder))?1:0;
  AF_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(af_a));
  AF_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(af_b));
  ENABLE_AF_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_af_pullup))?1:0;
  ENABLE_RF_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_rf_encoder))?1:0;
  RF_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rf_a));
  RF_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rf_b));
  ENABLE_RF_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_rf_pullup))?1:0;
  ENABLE_AGC_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_agc_encoder))?1:0;
  AGC_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(agc_a));
  AGC_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(agc_b));
  ENABLE_AGC_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_agc_pullup))?1:0;
  ENABLE_BAND_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_band))?1:0;
  BAND_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(band));
  ENABLE_MODE_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_mode))?1:0;
  MODE_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(mode));
  ENABLE_FILTER_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_filter))?1:0;
  FILTER_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(filter));
  ENABLE_NOISE_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_noise))?1:0;
  NOISE_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(noise));
  ENABLE_AGC_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_agc))?1:0;
  AGC_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(agc));
  ENABLE_MOX_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_mox))?1:0;
  MOX_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(mox));
  ENABLE_FUNCTION_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_function))?1:0;
  FUNCTION_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(function));

  gtk_widget_destroy(dialog);

  gpio_save_state();
}
#endif

void configure(DISCOVERED* d,GtkWidget *parent) {

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Configure",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);

  GtkWidget *sample_rate_label=gtk_label_new("Sample Rate:");
  //gtk_widget_override_font(sample_rate_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(sample_rate_label);
  gtk_grid_attach(GTK_GRID(grid),sample_rate_label,0,0,1,1);

  GtkWidget *sample_rate_48=gtk_radio_button_new_with_label(NULL,"48000");
  //gtk_widget_override_font(sample_rate_48, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_48), sample_rate==48000);
  gtk_widget_show(sample_rate_48);
  gtk_grid_attach(GTK_GRID(grid),sample_rate_48,0,1,1,1);
  g_signal_connect(sample_rate_48,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)48000);

  GtkWidget *sample_rate_96=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_48),"96000");
  //gtk_widget_override_font(sample_rate_96, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_96), sample_rate==96000);
  gtk_widget_show(sample_rate_96);
  gtk_grid_attach(GTK_GRID(grid),sample_rate_96,0,2,1,1);
  g_signal_connect(sample_rate_96,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)96000);

  GtkWidget *sample_rate_192=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_96),"192000");
  //gtk_widget_override_font(sample_rate_192, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_192), sample_rate==192000);
  gtk_widget_show(sample_rate_192);
  gtk_grid_attach(GTK_GRID(grid),sample_rate_192,0,3,1,1);
  g_signal_connect(sample_rate_192,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)192000);

  GtkWidget *sample_rate_384=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_192),"384000");
  //gtk_widget_override_font(sample_rate_384, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_384), sample_rate==384000);
  gtk_widget_show(sample_rate_384);
  gtk_grid_attach(GTK_GRID(grid),sample_rate_384,0,4,1,1);
  g_signal_connect(sample_rate_384,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)384000);

  if(d->protocol==NEW_PROTOCOL
#ifdef LIMESDR
    || d->protocol==LIMESDR_PROTOCOL
#endif
    ) {
    GtkWidget *sample_rate_768=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_384),"768000");
    //gtk_widget_override_font(sample_rate_768, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_768), sample_rate==768000);
    gtk_widget_show(sample_rate_768);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_768,0,5,1,1);
    g_signal_connect(sample_rate_768,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)768000);

    GtkWidget *sample_rate_921=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_768),"921180");
    //gtk_widget_override_font(sample_rate_921, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_921), sample_rate==921180);
    gtk_widget_show(sample_rate_921);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_921,0,6,1,1);
    g_signal_connect(sample_rate_921,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)921180);

    GtkWidget *sample_rate_1536=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_921),"1536000");
    //gtk_widget_override_font(sample_rate_1536, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1536), sample_rate==1536000);
    gtk_widget_show(sample_rate_1536);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_1536,0,7,1,1);
    g_signal_connect(sample_rate_1536,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1536000);

#ifdef LIMESDR
    if(d->protocol==LIMESDR_PROTOCOL) {
      GtkWidget *sample_rate_1M=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_1536),"1048576");
      //gtk_widget_override_font(sample_rate_1M, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1M), sample_rate==1048576);
      gtk_widget_show(sample_rate_1M);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_1M,0,8,1,1);
      g_signal_connect(sample_rate_1M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1048576);
  
      GtkWidget *sample_rate_2M=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_1M),"2097152");
      //gtk_widget_override_font(sample_rate_2M, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_2M), sample_rate==2097152);
      gtk_widget_show(sample_rate_2M);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_2M,0,9,1,1);
      g_signal_connect(sample_rate_2M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)2097152);
    }
#endif
  }


  GtkWidget *display_label=gtk_label_new("Display:");
  //gtk_widget_override_font(display_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(display_label);
  gtk_grid_attach(GTK_GRID(grid),display_label,0,9,1,1);

  GtkWidget *b_display_panadapter=gtk_check_button_new_with_label("Display Panadapter");
  //gtk_widget_override_font(b_display_panadapter, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_panadapter), display_panadapter);
  gtk_widget_show(b_display_panadapter);
  gtk_grid_attach(GTK_GRID(grid),b_display_panadapter,0,10,1,1);
  g_signal_connect(b_display_panadapter,"toggled",G_CALLBACK(display_panadapter_cb),(gpointer *)NULL);

  GtkWidget *b_display_waterfall=gtk_check_button_new_with_label("Display Waterfall");
  //gtk_widget_override_font(b_display_waterfall, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_waterfall), display_waterfall);
  gtk_widget_show(b_display_waterfall);
  gtk_grid_attach(GTK_GRID(grid),b_display_waterfall,0,11,1,1);
  g_signal_connect(b_display_waterfall,"toggled",G_CALLBACK(display_waterfall_cb),(gpointer *)NULL);

  GtkWidget *b_display_sliders=gtk_check_button_new_with_label("Display Sliders");
  //gtk_widget_override_font(b_display_sliders, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_sliders), display_sliders);
  gtk_widget_show(b_display_sliders);
  gtk_grid_attach(GTK_GRID(grid),b_display_sliders,0,12,1,1);
  g_signal_connect(b_display_sliders,"toggled",G_CALLBACK(display_sliders_cb),(gpointer *)NULL);

  GtkWidget *b_display_toolbar=gtk_check_button_new_with_label("Display Toolbar");
  //gtk_widget_override_font(b_display_toolbar, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_toolbar), display_toolbar);
  gtk_widget_show(b_display_toolbar);
  gtk_grid_attach(GTK_GRID(grid),b_display_toolbar,0,13,1,1);
  g_signal_connect(b_display_toolbar,"toggled",G_CALLBACK(display_toolbar_cb),(gpointer *)NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

  radioSaveState();

}
