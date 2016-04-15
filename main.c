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
#include "gpio.h"
#include "old_discovery.h"
#include "new_discovery.h"
#include "new_protocol.h"
#include "wdsp.h"
#include "vfo.h"
#include "menu.h"
#include "meter.h"
#include "panadapter.h"
#include "splash.h"
#include "waterfall.h"
#include "toolbar.h"
#include "radio.h"
#include "wdsp_init.h"
#include "version.h"

#define VFO_HEIGHT ((display_height/32)*4)
#define VFO_WIDTH ((display_width/32)*16)
#define MENU_HEIGHT VFO_HEIGHT
#define MENU_WIDTH ((display_width/32)*3)
#define METER_HEIGHT VFO_HEIGHT
#define METER_WIDTH ((display_width/32)*13)
#define PANADAPTER_HEIGHT ((display_height/32)*8)
#define SLIDERS_HEIGHT ((display_height/32)*6)
#define TOOLBAR_HEIGHT ((display_height/32)*2)
#define WATERFALL_HEIGHT (display_height-(VFO_HEIGHT+PANADAPTER_HEIGHT+SLIDERS_HEIGHT+TOOLBAR_HEIGHT))

struct utsname unameData;

gint display_width;
gint display_height;

static gint update_timer_id;

static gint save_timer_id;

static float *samples;

static int start=0;

static GtkWidget *discovery_dialog;

static sem_t wisdom_sem;

static GdkCursor *cursor_arrow;
static GdkCursor *cursor_watch;

gint update(gpointer data) {
    int result;
    double fwd;
    double rev;
    double exciter;

    GetPixels(isTransmitting()==0?CHANNEL_RX0:CHANNEL_TX,0,samples,&result);
    if(result==1) {
        if(display_panadapter) {
          panadapter_update(samples,isTransmitting());
        }
        if(!isTransmitting()) {
            if(display_waterfall) {
              waterfall_update(samples);
            }
        }
    }

    if(!isTransmitting()) {
        float m=GetRXAMeter(CHANNEL_RX0, 1/*WDSP.S_AV*/);
        meter_update(SMETER,(double)m,0.0,0.0);
    } else {

        DISCOVERED *d=&discovered[selected_device];

        double constant1=3.3;
        double constant2=0.09;

        if(d->protocol==ORIGINAL_PROTOCOL) {
            switch(d->device) {
                case DEVICE_METIS:
                    break;
                case DEVICE_HERMES:
                    //constant2=0.095; HERMES 2
                    break;
                case DEVICE_ANGELIA:
                    break;
                case DEVICE_ORION:
                    constant1=5.0;
                    constant2=0.108;
                    break;
                case DEVICE_HERMES_LITE:
                    break;
            }

            int power=alex_forward_power;
            if(power==0) {
                power=exciter_power;
            }
            double v1;
            v1=((double)power/4095.0)*constant1;
            fwd=(v1*v1)/constant2;

            power=exciter_power;
            v1=((double)power/4095.0)*constant1;
            exciter=(v1*v1)/constant2;

            rev=0.0;
            if(alex_forward_power!=0) {
                power=alex_reverse_power;
                v1=((double)power/4095.0)*constant1;
                rev=(v1*v1)/constant2;
            }
         
        } else {
            switch(d->device) {
                case NEW_DEVICE_ATLAS:
                    constant1=3.3;
                    constant2=0.09;
                    break;
                case NEW_DEVICE_HERMES:
                    constant1=3.3;
                    constant2=0.09;
                    break;
                case NEW_DEVICE_HERMES2:
                    constant1=3.3;
                    constant2=0.095;
                    break;
                case NEW_DEVICE_ANGELIA:
                    constant1=3.3;
                    constant2=0.095;
                    break;
                case NEW_DEVICE_ORION:
                    constant1=5.0;
                    constant2=0.108;
                    break;
                case NEW_DEVICE_ANAN_10E:
                    constant1=3.3;
                    constant2=0.09;
                    break;
                case NEW_DEVICE_HERMES_LITE:
                    constant1=3.3;
                    constant2=0.09;
                    break;
            }
        
            int power=alex_forward_power;
            if(power==0) {
                power=exciter_power;
            }
            double v1;
            v1=((double)power/4095.0)*constant1;
            fwd=(v1*v1)/constant2;

            power=exciter_power;
            v1=((double)power/4095.0)*constant1;
            exciter=(v1*v1)/constant2;

            rev=0.0;
            if(alex_forward_power!=0) {
                power=alex_reverse_power;
                v1=((double)power/4095.0)*constant1;
                rev=(v1*v1)/constant2;
            }
        }

//fprintf(stderr,"drive=%d tune_drive=%d alex_forward_power=%d alex_reverse_power=%d exciter_power=%d fwd=%f rev=%f exciter=%f\n",
//               drive, tune_drive, alex_forward_power, alex_reverse_power, exciter_power, fwd, rev, exciter);
        meter_update(POWER,fwd,rev,exciter);
    }

    return TRUE;
}

static gint save_cb(gpointer data) {
    radioSaveState();
    return TRUE;
}

static void start_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"start_cb\n");
    selected_device=(int)data;
    start=1;
    gtk_widget_destroy(discovery_dialog);
}

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

static void configure_gpio() {
  gpio_restore_state();

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Configure GPIO",GTK_WINDOW(splash_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
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

static void configure_cb(GtkWidget *widget, gpointer data) {
  DISCOVERED* d;
  d=&discovered[(int)data];
  sprintf(property_path,"%02X-%02X-%02X-%02X-%02X-%02X.props",
                        d->mac_address[0],
                        d->mac_address[1],
                        d->mac_address[2],
                        d->mac_address[3],
                        d->mac_address[4],
                        d->mac_address[5]);
  radioRestoreState();
  
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Configure",GTK_WINDOW(splash_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
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


  GtkWidget *display_label=gtk_label_new("Display:");
  //gtk_widget_override_font(display_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(display_label);
  gtk_grid_attach(GTK_GRID(grid),display_label,0,5,1,1);

  GtkWidget *b_display_panadapter=gtk_check_button_new_with_label("Display Panadapter");
  //gtk_widget_override_font(b_display_panadapter, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_panadapter), display_panadapter);
  gtk_widget_show(b_display_panadapter);
  gtk_grid_attach(GTK_GRID(grid),b_display_panadapter,0,6,1,1);
  g_signal_connect(b_display_panadapter,"toggled",G_CALLBACK(display_panadapter_cb),(gpointer *)NULL);

  GtkWidget *b_display_waterfall=gtk_check_button_new_with_label("Display Waterfall");
  //gtk_widget_override_font(b_display_waterfall, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_waterfall), display_waterfall);
  gtk_widget_show(b_display_waterfall);
  gtk_grid_attach(GTK_GRID(grid),b_display_waterfall,0,7,1,1);
  g_signal_connect(b_display_waterfall,"toggled",G_CALLBACK(display_waterfall_cb),(gpointer *)NULL);

  GtkWidget *b_display_sliders=gtk_check_button_new_with_label("Display Sliders");
  //gtk_widget_override_font(b_display_sliders, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_sliders), display_sliders);
  gtk_widget_show(b_display_sliders);
  gtk_grid_attach(GTK_GRID(grid),b_display_sliders,0,8,1,1);
  g_signal_connect(b_display_sliders,"toggled",G_CALLBACK(display_sliders_cb),(gpointer *)NULL);

  GtkWidget *b_display_toolbar=gtk_check_button_new_with_label("Display Toolbar");
  //gtk_widget_override_font(b_display_toolbar, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_toolbar), display_toolbar);
  gtk_widget_show(b_display_toolbar);
  gtk_grid_attach(GTK_GRID(grid),b_display_toolbar,0,9,1,1);
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

static pthread_t wisdom_thread_id;

static void* wisdom_thread(void *arg) {
  splash_status("Creating FFTW Wisdom file ...");
  WDSPwisdom ((char *)arg);
  sem_post(&wisdom_sem);
}

gint init(void* arg) {

  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *fixed;
  gint x;
  gint y;
  GtkWidget *vfo;
  GtkWidget *menu;
  GtkWidget *meter;
  GtkWidget *panadapter;
  GtkWidget *waterfall;
  GtkWidget *sliders;
  GtkWidget *toolbar;

  DISCOVERED* d;

  char *res;
  char wisdom_directory[1024];
  char wisdom_file[1024];

  fprintf(stderr,"init\n");

  cursor_arrow=gdk_cursor_new(GDK_ARROW);
  cursor_watch=gdk_cursor_new(GDK_WATCH);

  GdkWindow *gdk_splash_window = gtk_widget_get_window(splash_window);
  gdk_window_set_cursor(gdk_splash_window,cursor_watch);

  init_radio();

  // check if wisdom file exists
  res=getcwd(wisdom_directory, sizeof(wisdom_directory));
  strcpy(&wisdom_directory[strlen(wisdom_directory)],"/");
  strcpy(wisdom_file,wisdom_directory);
  strcpy(&wisdom_file[strlen(wisdom_file)],"wdspWisdom");
  splash_status("Checking FFTW Wisdom file ...");
  if(access(wisdom_file,F_OK)<0) {
      int rc=sem_init(&wisdom_sem, 0, 0);
      rc=pthread_create(&wisdom_thread_id, NULL, wisdom_thread, (void *)wisdom_directory);
      while(sem_trywait(&wisdom_sem)<0) {
        splash_status(wisdom_get_status());
        while (gtk_events_pending ())
          gtk_main_iteration ();
        usleep(100000); // 100ms
      }
  }

  while(!start) {
      gdk_window_set_cursor(gdk_splash_window,cursor_watch);
      selected_device=0;
      devices=0;
      splash_status("Old Protocol ... Discovering Devices");
      old_discovery();
      splash_status("New Protocol ... Discovering Devices");
      new_discovery();
      splash_status("Discovery");
      if(devices==0) {
          gdk_window_set_cursor(gdk_splash_window,cursor_arrow);
          fprintf(stderr,"No devices found!\n");
          GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
          discovery_dialog = gtk_message_dialog_new (GTK_WINDOW(splash_window),
                                 flags,
                                 GTK_MESSAGE_ERROR,
                                 GTK_BUTTONS_OK_CANCEL,
                                 "No devices found! Retry Discovery?");
          gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("Arial 18"));
          gint result=gtk_dialog_run (GTK_DIALOG (discovery_dialog));
          gtk_widget_destroy(discovery_dialog);
          if(result==GTK_RESPONSE_CANCEL) {
               _exit(0);
          }
      } else {
          fprintf(stderr,"%s: found %d devices.\n", (char *)arg, devices);
          gdk_window_set_cursor(gdk_splash_window,cursor_arrow);
          GtkDialogFlags flags=GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
          discovery_dialog = gtk_dialog_new_with_buttons ("Discovered",
                                      GTK_WINDOW(splash_window),
                                      flags,
                                      "Configure GPIO",
                                      GTK_RESPONSE_YES,
                                      "Discover",
                                      GTK_RESPONSE_REJECT,
                                      "Exit",
                                      GTK_RESPONSE_CLOSE,
                                      NULL);

          gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("Arial 18"));
          GtkWidget *content;

          content=gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));

          GtkWidget *grid=gtk_grid_new();
          gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
          gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

          int i;
          char text[128];
          for(i=0;i<devices;i++) {
              d=&discovered[i];
              sprintf(text,"%s (%s %d.%d) %s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n",
                        d->name,
                        d->protocol==ORIGINAL_PROTOCOL?"old":"new",
                        d->protocol==ORIGINAL_PROTOCOL?d->software_version/10:d->software_version/100,
                        d->protocol==ORIGINAL_PROTOCOL?d->software_version%10:d->software_version%100,
                        inet_ntoa(d->address.sin_addr),
                        d->mac_address[0],
                        d->mac_address[1],
                        d->mac_address[2],
                        d->mac_address[3],
                        d->mac_address[4],
                        d->mac_address[5],
                        d->interface_name);

              GtkWidget *label=gtk_label_new(text);
              gtk_widget_override_font(label, pango_font_description_from_string("Arial 12"));
              gtk_widget_show(label);
              gtk_grid_attach(GTK_GRID(grid),label,0,i,4,1);

              GtkWidget *start_button=gtk_button_new_with_label("Start");
              gtk_widget_override_font(start_button, pango_font_description_from_string("Arial 18"));
              gtk_widget_show(start_button);
              gtk_grid_attach(GTK_GRID(grid),start_button,4,i,1,1);
              g_signal_connect(start_button,"pressed",G_CALLBACK(start_cb),(gpointer *)i);

              GtkWidget *configure_button=gtk_button_new_with_label("Configure");
              gtk_widget_override_font(configure_button, pango_font_description_from_string("Arial 18"));
              gtk_widget_show(configure_button);
              gtk_grid_attach(GTK_GRID(grid),configure_button,5,i,1,1);
              g_signal_connect(configure_button,"pressed",G_CALLBACK(configure_cb),(gpointer *)i);
          }

          gtk_container_add (GTK_CONTAINER (content), grid);
          gtk_widget_show_all(discovery_dialog);
          gint result=gtk_dialog_run(GTK_DIALOG(discovery_dialog));

          if(result==GTK_RESPONSE_CLOSE) {
              _exit(0);
          }
         
          if(result==GTK_RESPONSE_YES) {
              gtk_widget_destroy(discovery_dialog);
              configure_gpio();
          }
      }
  }

  gdk_window_set_cursor(gdk_splash_window,cursor_watch);

  splash_status("Initializing wdsp ...");

  d=&discovered[selected_device];
  protocol=d->protocol;
  device=d->device;

  sprintf(property_path,"%02X-%02X-%02X-%02X-%02X-%02X.props",
                        d->mac_address[0],
                        d->mac_address[1],
                        d->mac_address[2],
                        d->mac_address[3],
                        d->mac_address[4],
                        d->mac_address[5]);

  radioRestoreState();

  samples=malloc(display_width*sizeof(float));

  //splash_status("Initializing wdsp ...");
  wdsp_init(0,display_width,d->protocol);

  if(d->protocol==ORIGINAL_PROTOCOL) {
      splash_status("Initializing old protocol ...");
      old_protocol_init(0,display_width);
  } else {
      splash_status("Initializing new protocol ...");
      new_protocol_init(0,display_width);
  }

  splash_status("Initializing GPIO ...");
  gpio_init();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "pihpsdr");
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);

#ifdef GRID_LAYOUT
  grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_container_add(GTK_CONTAINER(window), grid);
#else
  fixed=gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(window), fixed);
  y=0;
#endif

fprintf(stderr,"vfo_height=%d\n",VFO_HEIGHT);
  vfo = vfo_init(VFO_WIDTH,VFO_HEIGHT,window);
#ifdef GRID_LAYOUT
  gtk_grid_attach(GTK_GRID(grid), vfo, 0, y, 16, 1);
#else
  gtk_fixed_put(GTK_FIXED(fixed),vfo,0,0);
#endif


fprintf(stderr,"menu_height=%d\n",MENU_HEIGHT);
  menu = menu_init(MENU_WIDTH,MENU_HEIGHT,window);
#ifdef GRID_LAYOUT
  gtk_grid_attach(GTK_GRID(grid), menu, 16, 0, 1, 1);
#else
  gtk_fixed_put(GTK_FIXED(fixed),menu,VFO_WIDTH,y);
#endif

fprintf(stderr,"meter_height=%d\n",METER_HEIGHT);
  meter = meter_init(METER_WIDTH,METER_HEIGHT);
#ifdef GRID_LAYOUT
  gtk_grid_attach(GTK_GRID(grid), meter, 17, 0, 15, 1);
#else
  gtk_fixed_put(GTK_FIXED(fixed),meter,VFO_WIDTH+MENU_WIDTH,y);
  y+=VFO_HEIGHT;
#endif

  if(display_panadapter) {
    int height=PANADAPTER_HEIGHT;
    if(!display_waterfall) {
      height+=WATERFALL_HEIGHT;
      if(!display_sliders) {
        height+=SLIDERS_HEIGHT;
      }
      if(!display_toolbar) {
        height+=TOOLBAR_HEIGHT;
      }
    } else {
      if(!display_sliders) {
        height+=SLIDERS_HEIGHT/2;
      }
    }
fprintf(stderr,"panadapter_height=%d\n",height);
    panadapter = panadapter_init(display_width,height);
#ifdef GRID_LAYOUT
    gtk_grid_attach(GTK_GRID(grid), panadapter, 0, 1, 32, 1);
#else
    gtk_fixed_put(GTK_FIXED(fixed),panadapter,0,VFO_HEIGHT);
    y+=height;
#endif
  }

  if(display_waterfall) {
    int height=WATERFALL_HEIGHT;
    if(!display_panadapter) {
      height+=PANADAPTER_HEIGHT;
    }
    if(!display_sliders) {
      if(display_panadapter) {
        height+=SLIDERS_HEIGHT/2;
      } else {
        height+=SLIDERS_HEIGHT;
      }
    }
    if(!display_toolbar) {
      height+=TOOLBAR_HEIGHT;
    }
fprintf(stderr,"waterfall_height=%d\n",height);
    waterfall = waterfall_init(display_width,height);
#ifdef GRID_LAYOUT
    gtk_grid_attach(GTK_GRID(grid), waterfall, 0, 2, 32, 1);
#else
    gtk_fixed_put(GTK_FIXED(fixed),waterfall,0,y);
    y+=height;
#endif
  }

  if(display_sliders) {
fprintf(stderr,"sliders_height=%d\n",SLIDERS_HEIGHT);
    sliders = sliders_init(display_width,SLIDERS_HEIGHT,window);
#ifdef GRID_LAYOUT
    gtk_grid_attach(GTK_GRID(grid), sliders, 0, 3, 32, 1);
#else
    gtk_fixed_put(GTK_FIXED(fixed),sliders,0,y);
    y+=SLIDERS_HEIGHT;
#endif
  }

  if(display_toolbar) {
fprintf(stderr,"toolbar_height=%d\n",TOOLBAR_HEIGHT);
    toolbar = toolbar_init(display_width,TOOLBAR_HEIGHT,window);
#ifdef GRID_LAYOUT
    gtk_grid_attach(GTK_GRID(grid), toolbar, 0, 4, 32, 1);
#else
    gtk_fixed_put(GTK_FIXED(fixed),toolbar,0,y);
    y+=TOOLBAR_HEIGHT;
#endif
  }

  splash_close();


  gtk_widget_show_all (window);

  gtk_window_fullscreen(GTK_WINDOW(window));

  GdkWindow *gdk_window = gtk_widget_get_window(window);
  gdk_window_set_cursor(gdk_window,cursor_arrow);

  update_timer_id=gdk_threads_add_timeout(1000/updates_per_second, update, NULL);

  // save every 30 seconds
  save_timer_id=gdk_threads_add_timeout(30000, save_cb, NULL);

  vfo_update(NULL);

  if(protocol==ORIGINAL_PROTOCOL) {
    setFrequency(getFrequency());
  }

  return 0;
}


int
main (int   argc,
      char *argv[])
{
  gtk_init (&argc, &argv);

  fprintf(stderr,"Build: %s %s\n",build_date,build_time);

  uname(&unameData);
  fprintf(stderr,"sysname: %s\n",unameData.sysname);
  fprintf(stderr,"nodename: %s\n",unameData.nodename);
  fprintf(stderr,"release: %s\n",unameData.release);
  fprintf(stderr,"version: %s\n",unameData.version);
  fprintf(stderr,"machine: %s\n",unameData.machine);

  GdkScreen *screen=gdk_screen_get_default();
  if(screen==NULL) {
    fprintf(stderr,"no default screen!\n");
    _exit(0);
  }


  display_width=gdk_screen_get_width(screen);
  display_height=gdk_screen_get_height(screen);

  fprintf(stderr,"display_width=%d display_height=%d\n", display_width, display_height);

  splash_show("hpsdr.png", 0, display_width, display_height);

  g_idle_add(init,(void *)argv[0]);

  gtk_main();

  return 0;
}
