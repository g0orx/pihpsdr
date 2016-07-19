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

#ifdef raspberrypi
#define INCLUDE_GPIO
#endif
#ifdef odroid
#define INCLUDE_GPIO
#endif

#include <gtk/gtk.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "new_protocol.h"
#include "radio.h"
#include "toolbar.h"
#include "version.h"
#include "wdsp.h"
#ifdef FREEDV
#include "freedv.h"
#endif

static GtkWidget *parent_window;

static GtkWidget *box;
static GtkWidget *menu;

static GtkWidget *ant_grid;
static gint ant_id=-1;

static void sample_rate_cb(GtkWidget *widget, gpointer data) {
  if(protocol==ORIGINAL_PROTOCOL) {
    old_protocol_new_sample_rate((int)data);
  } else {
    sample_rate=(int)data;
  }
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

static void cw_keyer_weight_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_weight=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
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

static void cw_keyer_sidetone_level_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_sidetone_volume=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_keyer_sidetone_frequency_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_sidetone_frequency=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
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

static void linein_cb(GtkWidget *widget, gpointer data) {
  mic_linein=mic_linein==1?0:1;
}

static void micboost_cb(GtkWidget *widget, gpointer data) {
  mic_boost=mic_boost==1?0:1;
}

static void local_audio_cb(GtkWidget *widget, gpointer data) {
  if(local_audio) {
    local_audio=0;
    audio_close();
  } else {
    if(audio_init()==0) {
      local_audio=1;
    }
  }
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
}

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


static void tx_out_of_band_cb(GtkWidget *widget, gpointer data) {
  tx_out_of_band=tx_out_of_band==1?0:1;
}

static void pa_value_changed_cb(GtkWidget *widget, gpointer data) {
  BAND *band=(BAND *)data;
  band->pa_calibration=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static gboolean exit_pressed_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
#ifdef INCLUDE_GPIO
  gpio_close();
#endif
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      break;
    case NEW_PROTOCOL:
      new_protocol_stop();
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  _exit(0);
}

static gboolean reboot_pressed_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
#ifdef INCLUDE_GPIO
  gpio_close();
#endif
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      break;
    case NEW_PROTOCOL:
      new_protocol_stop();
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  system("reboot");
  _exit(0);
}

static gboolean shutdown_pressed_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
#ifdef INCLUDE_GPIO
  gpio_close();
#endif
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      break;
    case NEW_PROTOCOL:
      new_protocol_stop();
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  system("shutdown -h -P now");
  _exit(0);
}

static void oc_rx_cb(GtkWidget *widget, gpointer data) {
  int b=((int)data)>>4;
  int oc=((int)data)&0xF;
  BAND *band=band_get_band(b);
  int mask=0x01<<oc;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    band->OCrx|=mask;
  } else {
    band->OCrx&=~mask;
  }
}

static void oc_tx_cb(GtkWidget *widget, gpointer data) {
  int b=((int)data)>>4;
  int oc=((int)data)&0xF;
  BAND *band=band_get_band(b);
  int mask=0x01<<oc;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    band->OCtx|=mask;
  } else {
    band->OCtx&=~mask;
  }
}

static void oc_tune_cb(GtkWidget *widget, gpointer data) {
  int oc=((int)data)&0xF;
  int mask=0x01<<oc;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    OCtune|=mask;
  } else {
    OCtune&=~mask;
  }
}

static void detector_mode_cb(GtkWidget *widget, gpointer data) {
  display_detector_mode=(int)data;
  SetDisplayDetectorMode(CHANNEL_RX0, 0, display_detector_mode);
}

static void average_mode_cb(GtkWidget *widget, gpointer data) {
  display_average_mode=(int)data;
  SetDisplayAverageMode(CHANNEL_RX0, 0, display_average_mode);
}

static void time_value_changed_cb(GtkWidget *widget, gpointer data) {
  display_average_time=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  calculate_display_average();
  //SetDisplayAvBackmult(CHANNEL_RX0, 0, display_avb);
  //SetDisplayNumAverage(CHANNEL_RX0, 0, display_average);
}

static void filled_cb(GtkWidget *widget, gpointer data) {
  display_filled=display_filled==1?0:1;
}

static void frames_per_second_value_changed_cb(GtkWidget *widget, gpointer data) {
  updates_per_second=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  calculate_display_average();
}

static void oc_full_tune_time_cb(GtkWidget *widget, gpointer data) {
  OCfull_tune_time=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void oc_memory_tune_time_cb(GtkWidget *widget, gpointer data) {
  OCmemory_tune_time=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void pre_post_agc_cb(GtkWidget *widget, gpointer data) {
  nr_agc=(int)data;
  SetRXAEMNRPosition(CHANNEL_RX0, nr_agc);

}

static void nr2_gain_cb(GtkWidget *widget, gpointer data) {
  nr2_gain_method==(int)data;
  SetRXAEMNRgainMethod(CHANNEL_RX0, nr2_gain_method);
}

static void nr2_npe_method_cb(GtkWidget *widget, gpointer data) {
  nr2_npe_method=(int)data;
  SetRXAEMNRnpeMethod(CHANNEL_RX0, nr2_npe_method);
}

static void ae_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    nr2_ae=1;
  } else {
    nr2_ae=0;
  }
  SetRXAEMNRaeRun(CHANNEL_RX0, nr2_ae);
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

#ifdef FREEDV
static void freedv_text_changed_cb(GtkWidget *widget, gpointer data) {
  strcpy(freedv_tx_text_data,gtk_entry_get_text(GTK_ENTRY(widget)));
}
#endif

static void switch_page_cb(GtkNotebook *notebook,
               GtkWidget   *page,
               guint        page_num,
               gpointer     user_data)
{
  int i, j;
  GtkWidget *child;
  if(ant_id!=-1) {
    if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
      if(page_num==ant_id) {
        if(filter_board==ALEX) {
          for(i=0;i<HAM_BANDS;i++) {
            for(j=0;j<11;j++) {
              child=gtk_grid_get_child_at(GTK_GRID(ant_grid),j,i+1);
              gtk_widget_set_sensitive(child,TRUE);
            }
          }
        } else {
          for(i=0;i<HAM_BANDS;i++) {
            for(j=0;j<11;j++) {
              child=gtk_grid_get_child_at(GTK_GRID(ant_grid),j,i+1);
              gtk_widget_set_sensitive(child,FALSE);
            }
          }
        }
      }
    }
  }
}

static gboolean menu_pressed_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
  int i, j, id;

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Menu",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));



  GtkWidget *notebook=gtk_notebook_new();
  g_signal_connect(notebook,"switch_page",G_CALLBACK(switch_page_cb),NULL);

  GtkWidget *general_label=gtk_label_new("General");
  GtkWidget *general_grid=gtk_grid_new();
  //gtk_grid_set_row_homogeneous(GTK_GRID(general_grid),TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(general_grid),TRUE);


  GtkWidget *vfo_divisor_label=gtk_label_new("VFO Encoder Divisor: ");
  //gtk_widget_override_font(vfo_divisor_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(vfo_divisor_label);
  gtk_grid_attach(GTK_GRID(general_grid),vfo_divisor_label,0,0,1,1);

  GtkWidget *vfo_divisor=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  //gtk_widget_override_font(vfo_divisor, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(vfo_divisor),(double)vfo_encoder_divisor);
  gtk_widget_show(vfo_divisor);
  gtk_grid_attach(GTK_GRID(general_grid),vfo_divisor,1,0,1,1);
  g_signal_connect(vfo_divisor,"value_changed",G_CALLBACK(vfo_divisor_value_changed_cb),NULL);

#ifdef FREEDV
  GtkWidget *freedv_text_label=gtk_label_new("FreeDV Text Message: ");
  gtk_grid_attach(GTK_GRID(general_grid),freedv_text_label,0,1,1,1);

  GtkWidget *freedv_text=gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(freedv_text),freedv_tx_text_data);
  gtk_grid_attach(GTK_GRID(general_grid),freedv_text,1,1,1,1);
  g_signal_connect(freedv_text,"changed",G_CALLBACK(freedv_text_changed_cb),NULL);
#endif

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *rx_dither_b=gtk_check_button_new_with_label("Dither");
    //gtk_widget_override_font(rx_dither_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx_dither_b), rx_dither);
    gtk_widget_show(rx_dither_b);
    gtk_grid_attach(GTK_GRID(general_grid),rx_dither_b,0,2,1,1);
    g_signal_connect(rx_dither_b,"toggled",G_CALLBACK(rx_dither_cb),NULL);

    GtkWidget *rx_random_b=gtk_check_button_new_with_label("Random");
    //gtk_widget_override_font(rx_random_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx_random_b), rx_random);
    gtk_widget_show(rx_random_b);
    gtk_grid_attach(GTK_GRID(general_grid),rx_random_b,0,3,1,1);
    g_signal_connect(rx_random_b,"toggled",G_CALLBACK(rx_random_cb),NULL);


/*
    GtkWidget *rx_preamp_b=gtk_check_button_new_with_label("Preamp");
    //gtk_widget_override_font(rx_preamp_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx_preamp_b), rx_preamp);
    gtk_widget_show(rx_preamp_b);
    gtk_grid_attach(GTK_GRID(general_grid),rx_preamp_b,0,4,1,1);
    g_signal_connect(rx_preamp_b,"toggled",G_CALLBACK(rx_preamp_cb),NULL);
*/

    GtkWidget *linein_b=gtk_check_button_new_with_label("Mic Line In");
    //gtk_widget_override_font(linein_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (linein_b), mic_linein);
    gtk_widget_show(linein_b);
    gtk_grid_attach(GTK_GRID(general_grid),linein_b,1,2,1,1);
    g_signal_connect(linein_b,"toggled",G_CALLBACK(linein_cb),NULL);

    GtkWidget *micboost_b=gtk_check_button_new_with_label("Mic Boost");
    //gtk_widget_override_font(micboost_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (micboost_b), mic_boost);
    gtk_widget_show(micboost_b);
    gtk_grid_attach(GTK_GRID(general_grid),micboost_b,1,3,1,1);
    g_signal_connect(micboost_b,"toggled",G_CALLBACK(micboost_cb),NULL);

    GtkWidget *local_audio_b=gtk_check_button_new_with_label("Local Audio");
    //gtk_widget_override_font(local_audio_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_audio_b), local_audio);
    gtk_widget_show(local_audio_b);
    gtk_grid_attach(GTK_GRID(general_grid),local_audio_b,1,4,1,1);
    g_signal_connect(local_audio_b,"toggled",G_CALLBACK(local_audio_cb),NULL);

    GtkWidget *b_toolbar_dialog_buttons=gtk_check_button_new_with_label("Buttons Display Dialog");
    //gtk_widget_override_font(b_toolbar_dialog_buttons, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_toolbar_dialog_buttons), toolbar_dialog_buttons);
    gtk_widget_show(b_toolbar_dialog_buttons);
    gtk_grid_attach(GTK_GRID(general_grid),b_toolbar_dialog_buttons,1,5,1,1);
    g_signal_connect(b_toolbar_dialog_buttons,"toggled",G_CALLBACK(toolbar_dialog_buttons_cb),(gpointer *)NULL);

    if((protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION) ||
       (protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION2) ||
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_ORION)) {

      GtkWidget *ptt_ring_b=gtk_radio_button_new_with_label(NULL,"PTT On Ring, Mic and Bias on Tip");
      //gtk_widget_override_font(ptt_ring_b, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_ring_b), mic_ptt_tip_bias_ring==0);
      gtk_widget_show(ptt_ring_b);
      gtk_grid_attach(GTK_GRID(general_grid),ptt_ring_b,1,6,1,1);
      g_signal_connect(ptt_ring_b,"pressed",G_CALLBACK(ptt_ring_cb),NULL);

      GtkWidget *ptt_tip_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ptt_ring_b),"PTT On Tip, Mic and Bias on Ring");
      //gtk_widget_override_font(ptt_tip_b, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_tip_b), mic_ptt_tip_bias_ring==1);
      gtk_widget_show(ptt_tip_b);
      gtk_grid_attach(GTK_GRID(general_grid),ptt_tip_b,1,7,1,1);
      g_signal_connect(ptt_tip_b,"pressed",G_CALLBACK(ptt_tip_cb),NULL);

      GtkWidget *ptt_b=gtk_check_button_new_with_label("PTT Enabled");
      //gtk_widget_override_font(ptt_b, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_b), mic_ptt_enabled);
      gtk_widget_show(ptt_b);
      gtk_grid_attach(GTK_GRID(general_grid),ptt_b,1,8,1,1);
      g_signal_connect(ptt_b,"toggled",G_CALLBACK(ptt_cb),NULL);

      GtkWidget *bias_b=gtk_check_button_new_with_label("BIAS Enabled");
      //gtk_widget_override_font(bias_b, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bias_b), mic_bias_enabled);
      gtk_widget_show(bias_b);
      gtk_grid_attach(GTK_GRID(general_grid),bias_b,1,9,1,1);
      g_signal_connect(bias_b,"toggled",G_CALLBACK(bias_cb),NULL);
    }


    GtkWidget *alex_b=gtk_check_button_new_with_label("ALEX");
    //gtk_widget_override_font(alex_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alex_b), filter_board==ALEX);
    gtk_widget_show(alex_b);
    gtk_grid_attach(GTK_GRID(general_grid),alex_b,2,2,1,1);

    GtkWidget *apollo_b=gtk_check_button_new_with_label("APOLLO");
    //gtk_widget_override_font(apollo_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apollo_b), filter_board==APOLLO);
    gtk_widget_show(apollo_b);
    gtk_grid_attach(GTK_GRID(general_grid),apollo_b,2,3,1,1);
  
    g_signal_connect(alex_b,"toggled",G_CALLBACK(alex_cb),apollo_b);
    g_signal_connect(apollo_b,"toggled",G_CALLBACK(apollo_cb),alex_b);
  
    GtkWidget *apollo_tuner_b=gtk_check_button_new_with_label("Auto Tuner");
    //gtk_widget_override_font(apollo_tuner_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apollo_tuner_b), apollo_tuner);
    gtk_widget_show(apollo_tuner_b);
    gtk_grid_attach(GTK_GRID(general_grid),apollo_tuner_b,2,4,1,1);
    g_signal_connect(apollo_tuner_b,"toggled",G_CALLBACK(apollo_tuner_cb),NULL);
  
    GtkWidget *pa_b=gtk_check_button_new_with_label("PA");
    //gtk_widget_override_font(pa_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pa_b), pa);
    gtk_widget_show(pa_b);
    gtk_grid_attach(GTK_GRID(general_grid),pa_b,2,5,1,1);
    g_signal_connect(pa_b,"toggled",G_CALLBACK(pa_cb),NULL);
  
  }

  GtkWidget *sample_rate_label=gtk_label_new("Sample Rate:");
  //gtk_widget_override_font(sample_rate_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(sample_rate_label);
  gtk_grid_attach(GTK_GRID(general_grid),sample_rate_label,0,4,1,1);

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *sample_rate_48=gtk_radio_button_new_with_label(NULL,"48000");
    //gtk_widget_override_font(sample_rate_48, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_48), sample_rate==48000);
    gtk_widget_show(sample_rate_48);
    gtk_grid_attach(GTK_GRID(general_grid),sample_rate_48,0,5,1,1);
    g_signal_connect(sample_rate_48,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)48000);
  
    GtkWidget *sample_rate_96=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_48),"96000");
    //gtk_widget_override_font(sample_rate_96, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_96), sample_rate==96000);
    gtk_widget_show(sample_rate_96);
    gtk_grid_attach(GTK_GRID(general_grid),sample_rate_96,0,6,1,1);
    g_signal_connect(sample_rate_96,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)96000);
  
    GtkWidget *sample_rate_192=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_96),"192000");
    //gtk_widget_override_font(sample_rate_192, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_192), sample_rate==192000);
    gtk_widget_show(sample_rate_192);
    gtk_grid_attach(GTK_GRID(general_grid),sample_rate_192,0,7,1,1);
    g_signal_connect(sample_rate_192,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)192000);
  
    GtkWidget *sample_rate_384=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_192),"384000");
    //gtk_widget_override_font(sample_rate_384, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_384), sample_rate==384000);
    gtk_widget_show(sample_rate_384);
    gtk_grid_attach(GTK_GRID(general_grid),sample_rate_384,0,8,1,1);
    g_signal_connect(sample_rate_384,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)384000);
  
    if(protocol==NEW_PROTOCOL) {
      GtkWidget *sample_rate_768=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_384),"768000");
      //gtk_widget_override_font(sample_rate_768, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_768), sample_rate==768000);
      gtk_widget_show(sample_rate_768);
      gtk_grid_attach(GTK_GRID(general_grid),sample_rate_768,0,9,1,1);
      g_signal_connect(sample_rate_768,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)768000);
  
      GtkWidget *sample_rate_1536=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_768),"1536000");
      //gtk_widget_override_font(sample_rate_1536, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1536), sample_rate==1536000);
      gtk_widget_show(sample_rate_1536);
        gtk_grid_attach(GTK_GRID(general_grid),sample_rate_1536,0,10,1,1);
      g_signal_connect(sample_rate_1536,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1536000);
    }
  }
  
#ifdef LIMESDR
  if(protocol==LIMESDR_PROTOCOL) {
    GtkWidget *sample_rate_1M=gtk_radio_button_new_with_label(NULL,"1000000");
    //gtk_widget_override_font(sample_rate_1M, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1M), sample_rate==1000000);
    gtk_widget_show(sample_rate_1M);
    gtk_grid_attach(GTK_GRID(general_grid),sample_rate_1M,0,5,1,1);
    g_signal_connect(sample_rate_1M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1000000);

    GtkWidget *sample_rate_2M=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_1M),"2000000");
    //gtk_widget_override_font(sample_rate_2M, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_2M), sample_rate==2000000);
    gtk_widget_show(sample_rate_2M);
    gtk_grid_attach(GTK_GRID(general_grid),sample_rate_2M,0,6,1,1);
    g_signal_connect(sample_rate_2M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)2000000);

  }
#endif

  id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),general_grid,general_label);

  GtkWidget *ant_label=gtk_label_new("Ant");
  ant_grid=gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(ant_grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(ant_grid),10);

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *rx_ant_label=gtk_label_new("Receive");
    //gtk_widget_override_font(rx_ant_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx_ant_label);
    gtk_grid_attach(GTK_GRID(ant_grid),rx_ant_label,0,0,1,1);

    GtkWidget *rx1_label=gtk_label_new("1");
    //gtk_widget_override_font(rx1_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx1_label);
    gtk_grid_attach(GTK_GRID(ant_grid),rx1_label,1,0,1,1);

    GtkWidget *rx2_label=gtk_label_new("2");
    //gtk_widget_override_font(rx2_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx2_label);
    gtk_grid_attach(GTK_GRID(ant_grid),rx2_label,2,0,1,1);

    GtkWidget *rx3_label=gtk_label_new("3");
    //gtk_widget_override_font(rx3_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx3_label);
    gtk_grid_attach(GTK_GRID(ant_grid),rx3_label,3,0,1,1);

    GtkWidget *ext1_label=gtk_label_new("EXT1");
    //gtk_widget_override_font(ext1_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(ext1_label);
    gtk_grid_attach(GTK_GRID(ant_grid),ext1_label,4,0,1,1);

    GtkWidget *ext2_label=gtk_label_new("EXT2");
    //gtk_widget_override_font(ext2_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(ext2_label);
    gtk_grid_attach(GTK_GRID(ant_grid),ext2_label,5,0,1,1);

    GtkWidget *xvtr_label=gtk_label_new("XVTR");
    //gtk_widget_override_font(xvtr_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(xvtr_label);
    gtk_grid_attach(GTK_GRID(ant_grid),xvtr_label,6,0,1,1);

    GtkWidget *tx_ant_label=gtk_label_new("Transmit");
    //gtk_widget_override_font(tx_ant_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx_ant_label);
    gtk_grid_attach(GTK_GRID(ant_grid),tx_ant_label,7,0,1,1);

    GtkWidget *tx1_label=gtk_label_new("1");
    //gtk_widget_override_font(tx1_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx1_label);
    gtk_grid_attach(GTK_GRID(ant_grid),tx1_label,8,0,1,1);

    GtkWidget *tx2_label=gtk_label_new("2");
    //gtk_widget_override_font(tx2_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx2_label);
    gtk_grid_attach(GTK_GRID(ant_grid),tx2_label,9,0,1,1);

    GtkWidget *tx3_label=gtk_label_new("3");
    //gtk_widget_override_font(tx3_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx3_label);
    gtk_grid_attach(GTK_GRID(ant_grid),tx3_label,10,0,1,1);


  
    for(i=0;i<HAM_BANDS;i++) {
      BAND *band=band_get_band(i);
  
      GtkWidget *band_label=gtk_label_new(band->title);
      //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(band_label);
      gtk_grid_attach(GTK_GRID(ant_grid),band_label,0,i+1,1,1);
  
      GtkWidget *rx1_b=gtk_radio_button_new(NULL);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_b), band->alexRxAntenna==0);
      gtk_widget_show(rx1_b);
      gtk_grid_attach(GTK_GRID(ant_grid),rx1_b,1,i+1,1,1);
      g_signal_connect(rx1_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+0));
  
      GtkWidget *rx2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx1_b));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx2_b), band->alexRxAntenna==1);
      gtk_widget_show(rx2_b);
      gtk_grid_attach(GTK_GRID(ant_grid),rx2_b,2,i+1,1,1);
      g_signal_connect(rx2_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+1));
  
      GtkWidget *rx3_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx2_b));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx3_b), band->alexRxAntenna==2);
      gtk_widget_show(rx3_b);
      gtk_grid_attach(GTK_GRID(ant_grid),rx3_b,3,i+1,1,1);
      g_signal_connect(rx3_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+2));
  
      GtkWidget *ext1_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(rx3_b));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ext1_b), band->alexRxAntenna==3);
      gtk_widget_show(ext1_b);
      gtk_grid_attach(GTK_GRID(ant_grid),ext1_b,4,i+1,1,1);
      g_signal_connect(ext1_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+3));
  
      GtkWidget *ext2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(ext1_b));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ext2_b), band->alexRxAntenna==4);
      gtk_widget_show(ext2_b);
      gtk_grid_attach(GTK_GRID(ant_grid),ext2_b,5,i+1,1,1);
      g_signal_connect(ext2_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+4));
  
      GtkWidget *xvtr_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(ext2_b));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (xvtr_b), band->alexRxAntenna==5);
      gtk_widget_show(xvtr_b);
      gtk_grid_attach(GTK_GRID(ant_grid),xvtr_b,6,i+1,1,1);
      g_signal_connect(xvtr_b,"pressed",G_CALLBACK(rx_ant_cb),(gpointer)((i<<4)+5));
  
      GtkWidget *ant_band_label=gtk_label_new(band->title);
      //gtk_widget_override_font(ant_band_label, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(ant_band_label);
      gtk_grid_attach(GTK_GRID(ant_grid),ant_band_label,7,i+1,1,1);
  
      GtkWidget *tx1_b=gtk_radio_button_new(NULL);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx1_b), band->alexTxAntenna==0);
      gtk_widget_show(tx1_b);
      gtk_grid_attach(GTK_GRID(ant_grid),tx1_b,8,i+1,1,1);
      g_signal_connect(tx1_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)((i<<4)+0));
  
      GtkWidget *tx2_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(tx1_b));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx2_b), band->alexTxAntenna==1);
      gtk_widget_show(tx2_b);
      gtk_grid_attach(GTK_GRID(ant_grid),tx2_b,9,i+1,1,1);
      g_signal_connect(tx2_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)((i<<4)+1));
  
      GtkWidget *tx3_b=gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(tx2_b));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx3_b), band->alexTxAntenna==2);
      gtk_widget_show(tx3_b);
      gtk_grid_attach(GTK_GRID(ant_grid),tx3_b,10,i+1,1,1);
      g_signal_connect(tx3_b,"pressed",G_CALLBACK(tx_ant_cb),(gpointer)((i<<4)+2));
  
    }
  }

#ifdef LIMESDR
  if(protocol==LIMESDR_PROTOCOL) {
    BAND *band=band_get_current_band();

    GtkWidget *rx1_none=gtk_radio_button_new_with_label(NULL,"RX 1: NONE");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_none), band->alexRxAntenna==0);
    gtk_widget_show(rx1_none);
    gtk_grid_attach(GTK_GRID(ant_grid),rx1_none,0,0,1,1);
    g_signal_connect(rx1_none,"pressed",G_CALLBACK(rx_lime_ant_cb),(gpointer)0);

    GtkWidget *rx1_lnah=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rx1_none),"RX1: LNAH");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_lnah), band->alexRxAntenna==1);
    gtk_widget_show(rx1_lnah);
    gtk_grid_attach(GTK_GRID(ant_grid),rx1_lnah,0,1,1,1);
    g_signal_connect(rx1_lnah,"pressed",G_CALLBACK(rx_lime_ant_cb),(gpointer)+1);

    GtkWidget *rx1_lnal=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rx1_lnah),"RX1: LNAL");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_lnal), band->alexRxAntenna==2);
    gtk_widget_show(rx1_lnal);
    gtk_grid_attach(GTK_GRID(ant_grid),rx1_lnal,0,2,1,1);
    g_signal_connect(rx1_lnal,"pressed",G_CALLBACK(rx_lime_ant_cb),(gpointer)2);

    GtkWidget *rx1_lnaw=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rx1_lnal),"RX1: LNAW");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rx1_lnaw), band->alexRxAntenna==3);
    gtk_widget_show(rx1_lnaw);
    gtk_grid_attach(GTK_GRID(ant_grid),rx1_lnaw,0,3,1,1);
    g_signal_connect(rx1_lnaw,"pressed",G_CALLBACK(rx_lime_ant_cb),(gpointer)3);
  }
#endif

  ant_id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),ant_grid,ant_label);


  GtkWidget *display_label=gtk_label_new("Display");
  GtkWidget *display_grid=gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(display_grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(display_grid),10);

  GtkWidget *filled_b=gtk_check_button_new_with_label("Fill Panadapter");
  //gtk_widget_override_font(filled_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filled_b), display_filled);
  gtk_widget_show(filled_b);
  gtk_grid_attach(GTK_GRID(display_grid),filled_b,0,0,1,1);
  g_signal_connect(filled_b,"toggled",G_CALLBACK(filled_cb),NULL);

  GtkWidget *frames_per_second_label=gtk_label_new("Frames Per Second: ");
  //gtk_widget_override_font(frames_per_second_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(frames_per_second_label);
  gtk_grid_attach(GTK_GRID(display_grid),frames_per_second_label,0,1,1,1);

  GtkWidget *frames_per_second_r=gtk_spin_button_new_with_range(1.0,100.0,1.0);
  //gtk_widget_override_font(frames_per_second_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(frames_per_second_r),(double)updates_per_second);
  gtk_widget_show(frames_per_second_r);
  gtk_grid_attach(GTK_GRID(display_grid),frames_per_second_r,1,1,1,1);
  g_signal_connect(frames_per_second_r,"value_changed",G_CALLBACK(frames_per_second_value_changed_cb),NULL);


  GtkWidget *panadapter_high_label=gtk_label_new("Panadapter High: ");
  //gtk_widget_override_font(panadapter_high_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_high_label);
  gtk_grid_attach(GTK_GRID(display_grid),panadapter_high_label,0,2,1,1);

  GtkWidget *panadapter_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(panadapter_high_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_high_r),(double)panadapter_high);
  gtk_widget_show(panadapter_high_r);
  gtk_grid_attach(GTK_GRID(display_grid),panadapter_high_r,1,2,1,1);
  g_signal_connect(panadapter_high_r,"value_changed",G_CALLBACK(panadapter_high_value_changed_cb),NULL);

  GtkWidget *panadapter_low_label=gtk_label_new("Panadapter Low: ");
  //gtk_widget_override_font(panadapter_low_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_low_label);
  gtk_grid_attach(GTK_GRID(display_grid),panadapter_low_label,0,3,1,1);

  GtkWidget *panadapter_low_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(panadapter_low_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_low_r),(double)panadapter_low);
  gtk_widget_show(panadapter_low_r);
  gtk_grid_attach(GTK_GRID(display_grid),panadapter_low_r,1,3,1,1);
  g_signal_connect(panadapter_low_r,"value_changed",G_CALLBACK(panadapter_low_value_changed_cb),NULL);

  GtkWidget *waterfall_automatic_label=gtk_label_new("Waterfall Automatic: ");
  //gtk_widget_override_font(waterfall_automatic_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_automatic_label);
  gtk_grid_attach(GTK_GRID(display_grid),waterfall_automatic_label,0,4,1,1);

  GtkWidget *waterfall_automatic_b=gtk_check_button_new();
  //gtk_widget_override_font(waterfall_automatic_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (waterfall_automatic_b), waterfall_automatic);
  gtk_widget_show(waterfall_automatic_b);
  gtk_grid_attach(GTK_GRID(display_grid),waterfall_automatic_b,1,4,1,1);
  g_signal_connect(waterfall_automatic_b,"toggled",G_CALLBACK(waterfall_automatic_cb),NULL);

  GtkWidget *waterfall_high_label=gtk_label_new("Waterfall High: ");
  //gtk_widget_override_font(waterfall_high_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_high_label);
  gtk_grid_attach(GTK_GRID(display_grid),waterfall_high_label,0,5,1,1);

  GtkWidget *waterfall_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(waterfall_high_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(waterfall_high_r),(double)waterfall_high);
  gtk_widget_show(waterfall_high_r);
  gtk_grid_attach(GTK_GRID(display_grid),waterfall_high_r,1,5,1,1);
  g_signal_connect(waterfall_high_r,"value_changed",G_CALLBACK(waterfall_high_value_changed_cb),NULL);

  GtkWidget *waterfall_low_label=gtk_label_new("Waterfall Low: ");
  //gtk_widget_override_font(waterfall_low_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_low_label);
  gtk_grid_attach(GTK_GRID(display_grid),waterfall_low_label,0,6,1,1);

  GtkWidget *waterfall_low_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(waterfall_low_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(waterfall_low_r),(double)waterfall_low);
  gtk_widget_show(waterfall_low_r);
  gtk_grid_attach(GTK_GRID(display_grid),waterfall_low_r,1,6,1,1);
  g_signal_connect(waterfall_low_r,"value_changed",G_CALLBACK(waterfall_low_value_changed_cb),NULL);

  GtkWidget *detector_mode_label=gtk_label_new("Detector: ");
  //gtk_widget_override_font(detector_mode_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(detector_mode_label);
  gtk_grid_attach(GTK_GRID(display_grid),detector_mode_label,3,0,1,1);

  GtkWidget *detector_mode_peak=gtk_radio_button_new_with_label(NULL,"Peak");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (detector_mode_peak), display_detector_mode==DETECTOR_MODE_PEAK);
  gtk_widget_show(detector_mode_peak);
  gtk_grid_attach(GTK_GRID(display_grid),detector_mode_peak,3,1,1,1);
  g_signal_connect(detector_mode_peak,"pressed",G_CALLBACK(detector_mode_cb),(gpointer *)DETECTOR_MODE_PEAK);

  GtkWidget *detector_mode_rosenfell=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(detector_mode_peak),"Rosenfell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (detector_mode_rosenfell), display_detector_mode==DETECTOR_MODE_ROSENFELL);
  gtk_widget_show(detector_mode_rosenfell);
  gtk_grid_attach(GTK_GRID(display_grid),detector_mode_rosenfell,3,2,1,1);
  g_signal_connect(detector_mode_rosenfell,"pressed",G_CALLBACK(detector_mode_cb),(gpointer *)DETECTOR_MODE_ROSENFELL);

  GtkWidget *detector_mode_average=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(detector_mode_rosenfell),"Average");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (detector_mode_average), display_detector_mode==DETECTOR_MODE_AVERAGE);
  gtk_widget_show(detector_mode_average);
  gtk_grid_attach(GTK_GRID(display_grid),detector_mode_average,3,3,1,1);
  g_signal_connect(detector_mode_average,"pressed",G_CALLBACK(detector_mode_cb),(gpointer *)DETECTOR_MODE_AVERAGE);

  GtkWidget *detector_mode_sample=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(detector_mode_average),"Sample");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (detector_mode_sample), display_detector_mode==DETECTOR_MODE_SAMPLE);
  gtk_widget_show(detector_mode_sample);
  gtk_grid_attach(GTK_GRID(display_grid),detector_mode_sample,3,4,1,1);
  g_signal_connect(detector_mode_sample,"pressed",G_CALLBACK(detector_mode_cb),(gpointer *)DETECTOR_MODE_SAMPLE);


  GtkWidget *average_mode_label=gtk_label_new("Averaging: ");
  //gtk_widget_override_font(average_mode_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(average_mode_label);
  gtk_grid_attach(GTK_GRID(display_grid),average_mode_label,4,0,1,1);

  GtkWidget *average_mode_none=gtk_radio_button_new_with_label(NULL,"None");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average_mode_none), display_detector_mode==AVERAGE_MODE_NONE);
  gtk_widget_show(average_mode_none);
  gtk_grid_attach(GTK_GRID(display_grid),average_mode_none,4,1,1,1);
  g_signal_connect(average_mode_none,"pressed",G_CALLBACK(average_mode_cb),(gpointer *)AVERAGE_MODE_NONE);

  GtkWidget *average_mode_recursive=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(average_mode_none),"Recursive");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average_mode_recursive), display_average_mode==AVERAGE_MODE_RECURSIVE);
  gtk_widget_show(average_mode_recursive);
  gtk_grid_attach(GTK_GRID(display_grid),average_mode_recursive,4,2,1,1);
  g_signal_connect(average_mode_recursive,"pressed",G_CALLBACK(average_mode_cb),(gpointer *)AVERAGE_MODE_RECURSIVE);

  GtkWidget *average_mode_time_window=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(average_mode_recursive),"Time Window");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average_mode_time_window), display_average_mode==AVERAGE_MODE_TIME_WINDOW);
  gtk_widget_show(average_mode_time_window);
  gtk_grid_attach(GTK_GRID(display_grid),average_mode_time_window,4,3,1,1);
  g_signal_connect(average_mode_time_window,"pressed",G_CALLBACK(average_mode_cb),(gpointer *)AVERAGE_MODE_TIME_WINDOW);

  GtkWidget *average_mode_log_recursive=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(average_mode_time_window),"Log Recursive");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average_mode_log_recursive), display_average_mode==AVERAGE_MODE_LOG_RECURSIVE);
  gtk_widget_show(average_mode_log_recursive);
  gtk_grid_attach(GTK_GRID(display_grid),average_mode_log_recursive,4,4,1,1);
  g_signal_connect(average_mode_log_recursive,"pressed",G_CALLBACK(average_mode_cb),(gpointer *)AVERAGE_MODE_LOG_RECURSIVE);


  GtkWidget *time_label=gtk_label_new("Time (ms): ");
  //gtk_widget_override_font(average_mode_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(time_label);
  gtk_grid_attach(GTK_GRID(display_grid),time_label,4,5,1,1);

  GtkWidget *time_r=gtk_spin_button_new_with_range(1.0,9999.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(time_r),(double)display_average_time);
  gtk_widget_show(time_r);
  gtk_grid_attach(GTK_GRID(display_grid),time_r,5,5,1,1);
  g_signal_connect(time_r,"value_changed",G_CALLBACK(time_value_changed_cb),NULL);

  id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),display_grid,display_label);




  GtkWidget *dsp_label=gtk_label_new("DSP");
  GtkWidget *dsp_grid=gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(dsp_grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(dsp_grid),10);

  GtkWidget *pre_post_agc_label=gtk_label_new("NR/NR2/ANF");
  //gtk_widget_override_font(pre_post_agc_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(pre_post_agc_label);
  gtk_grid_attach(GTK_GRID(dsp_grid),pre_post_agc_label,0,0,1,1);

  GtkWidget *pre_agc_b=gtk_radio_button_new_with_label(NULL,"Pre AGC");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pre_agc_b),nr_agc==0);
  gtk_widget_show(pre_agc_b);
  gtk_grid_attach(GTK_GRID(dsp_grid),pre_agc_b,1,0,1,1);
  g_signal_connect(pre_agc_b,"pressed",G_CALLBACK(pre_post_agc_cb),(gpointer *)0);

  GtkWidget *post_agc_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pre_agc_b),"Post AGC");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (post_agc_b), nr_agc==1);
  gtk_widget_show(post_agc_b);
  gtk_grid_attach(GTK_GRID(dsp_grid),post_agc_b,2,0,1,1);
  g_signal_connect(post_agc_b,"pressed",G_CALLBACK(pre_post_agc_cb),(gpointer *)1);

  GtkWidget *nr2_gain_label=gtk_label_new("NR2 Gain Method");
  //gtk_widget_override_font(nr2_gain_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(nr2_gain_label);
  gtk_grid_attach(GTK_GRID(dsp_grid),nr2_gain_label,0,1,1,1);

  GtkWidget *linear_b=gtk_radio_button_new_with_label(NULL,"Linear");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (linear_b),nr2_gain_method==0);
  gtk_widget_show(linear_b);
  gtk_grid_attach(GTK_GRID(dsp_grid),linear_b,1,1,1,1);
  g_signal_connect(linear_b,"pressed",G_CALLBACK(nr2_gain_cb),(gpointer *)0);

  GtkWidget *log_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(linear_b),"Log");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (log_b), nr2_gain_method==1);
  gtk_widget_show(log_b);
  gtk_grid_attach(GTK_GRID(dsp_grid),log_b,2,1,1,1);
  g_signal_connect(log_b,"pressed",G_CALLBACK(nr2_gain_cb),(gpointer *)1);

  GtkWidget *gamma_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(log_b),"Gamma");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gamma_b), nr2_gain_method==2);
  gtk_widget_show(gamma_b);
  gtk_grid_attach(GTK_GRID(dsp_grid),gamma_b,3,1,1,1);
  g_signal_connect(gamma_b,"pressed",G_CALLBACK(nr2_gain_cb),(gpointer *)2);

  GtkWidget *nr2_npe_method_label=gtk_label_new("NR2 NPE Method");
  //gtk_widget_override_font(nr2_npe_method_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(nr2_npe_method_label);
  gtk_grid_attach(GTK_GRID(dsp_grid),nr2_npe_method_label,0,2,1,1);

  GtkWidget *osms_b=gtk_radio_button_new_with_label(NULL,"OSMS");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (osms_b),nr2_npe_method==0);
  gtk_widget_show(osms_b);
  gtk_grid_attach(GTK_GRID(dsp_grid),osms_b,1,2,1,1);
  g_signal_connect(osms_b,"pressed",G_CALLBACK(nr2_npe_method_cb),(gpointer *)0);

  GtkWidget *mmse_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(osms_b),"MMSE");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mmse_b), nr2_npe_method==1);
  gtk_widget_show(mmse_b);
  gtk_grid_attach(GTK_GRID(dsp_grid),mmse_b,2,2,1,1);
  g_signal_connect(mmse_b,"pressed",G_CALLBACK(nr2_npe_method_cb),(gpointer *)1);

  GtkWidget *ae_b=gtk_check_button_new_with_label("NR2 AE Filter");
  //gtk_widget_override_font(ae_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ae_b), nr2_ae);
  gtk_widget_show(ae_b);
  gtk_grid_attach(GTK_GRID(dsp_grid),ae_b,0,3,1,1);
  g_signal_connect(ae_b,"toggled",G_CALLBACK(ae_cb),NULL);

  id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),dsp_grid,dsp_label);

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *tx_label=gtk_label_new("PA Gain");
    GtkWidget *tx_grid=gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(tx_grid),TRUE);
    gtk_grid_set_column_spacing (GTK_GRID(tx_grid),10);

    for(i=0;i<HAM_BANDS;i++) {
      BAND *band=band_get_band(i);

      GtkWidget *band_label=gtk_label_new(band->title);
      //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(band_label);
      gtk_grid_attach(GTK_GRID(tx_grid),band_label,(i/6)*2,i%6,1,1);

      GtkWidget *pa_r=gtk_spin_button_new_with_range(0.0,100.0,1.0);
      //gtk_widget_override_font(pa_r, pango_font_description_from_string("Arial 18"));
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(pa_r),(double)band->pa_calibration);
      gtk_widget_show(pa_r);
      gtk_grid_attach(GTK_GRID(tx_grid),pa_r,((i/6)*2)+1,i%6,1,1);
      g_signal_connect(pa_r,"value_changed",G_CALLBACK(pa_value_changed_cb),band);
    }

    GtkWidget *tx_out_of_band_b=gtk_check_button_new_with_label("Transmit out of band");
    //gtk_widget_override_font(tx_out_of_band_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tx_out_of_band_b), tx_out_of_band);
    gtk_widget_show(tx_out_of_band_b);
    gtk_grid_attach(GTK_GRID(tx_grid),tx_out_of_band_b,0,7,4,1);
    g_signal_connect(tx_out_of_band_b,"toggled",G_CALLBACK(tx_out_of_band_cb),NULL);

    id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),tx_grid,tx_label);
  }

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *cw_label=gtk_label_new("CW");
    GtkWidget *cw_grid=gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(cw_grid),TRUE);

/*
    GtkWidget *cw_keyer_internal_b=gtk_check_button_new_with_label("CW Internal - Speed (WPM)");
    //gtk_widget_override_font(cw_keyer_internal_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_internal_b), cw_keyer_internal);
    gtk_widget_show(cw_keyer_internal_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_internal_b,0,0,1,1);
    g_signal_connect(cw_keyer_internal_b,"toggled",G_CALLBACK(cw_keyer_internal_cb),NULL);
*/
    GtkWidget *cw_speed_label=gtk_label_new("CW Speed (WPM)");
    //gtk_widget_override_font(cw_speed_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(cw_speed_label);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_speed_label,0,0,1,1);

    GtkWidget *cw_keyer_speed_b=gtk_spin_button_new_with_range(1.0,60.0,1.0);
    //gtk_widget_override_font(cw_keyer_speed_b, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_speed_b),(double)cw_keyer_speed);
    gtk_widget_show(cw_keyer_speed_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_speed_b,1,0,1,1);
    g_signal_connect(cw_keyer_speed_b,"value_changed",G_CALLBACK(cw_keyer_speed_value_changed_cb),NULL);

    GtkWidget *cw_breakin_b=gtk_check_button_new_with_label("CW Break In - Delay (ms)");
    //gtk_widget_override_font(cw_breakin_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_breakin_b), cw_breakin);
    gtk_widget_show(cw_breakin_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_breakin_b,0,1,1,1);
    g_signal_connect(cw_breakin_b,"toggled",G_CALLBACK(cw_breakin_cb),NULL);

    GtkWidget *cw_keyer_hang_time_b=gtk_spin_button_new_with_range(0.0,1000.0,1.0);
    //gtk_widget_override_font(cw_keyer_hang_time_b, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_hang_time_b),(double)cw_keyer_hang_time);
    gtk_widget_show(cw_keyer_hang_time_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_hang_time_b,1,1,1,1);
    g_signal_connect(cw_keyer_hang_time_b,"value_changed",G_CALLBACK(cw_keyer_hang_time_value_changed_cb),NULL);
 
    GtkWidget *cw_keyer_straight=gtk_radio_button_new_with_label(NULL,"CW KEYER STRAIGHT");
    //gtk_widget_override_font(cw_keyer_straight, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_straight), cw_keyer_mode==KEYER_STRAIGHT);
    gtk_widget_show(cw_keyer_straight);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_straight,0,2,1,1);
    g_signal_connect(cw_keyer_straight,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_STRAIGHT);

    GtkWidget *cw_keyer_mode_a=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(cw_keyer_straight),"CW KEYER MODE A");
    //gtk_widget_override_font(cw_keyer_mode_a, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_mode_a), cw_keyer_mode==KEYER_MODE_A);
    gtk_widget_show(cw_keyer_mode_a);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_mode_a,0,3,1,1);
    g_signal_connect(cw_keyer_mode_a,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_MODE_A);

    GtkWidget *cw_keyer_mode_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(cw_keyer_mode_a),"CW KEYER MODE B");
    //gtk_widget_override_font(cw_keyer_mode_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_mode_b), cw_keyer_mode==KEYER_MODE_B);
    gtk_widget_show(cw_keyer_mode_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_mode_b,0,4,1,1);
    g_signal_connect(cw_keyer_mode_b,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_MODE_B);

    GtkWidget *cw_keys_reversed_b=gtk_check_button_new_with_label("Keys reversed");
    //gtk_widget_override_font(cw_keys_reversed_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keys_reversed_b), cw_keys_reversed);
    gtk_widget_show(cw_keys_reversed_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keys_reversed_b,0,5,1,1);
    g_signal_connect(cw_keys_reversed_b,"toggled",G_CALLBACK(cw_keys_reversed_cb),NULL);
  
    GtkWidget *cw_keyer_sidetone_level_label=gtk_label_new("Sidetone Level:");
    //gtk_widget_override_font(cw_keyer_sidetone_level_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(cw_keyer_sidetone_level_label);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_sidetone_level_label,0,6,1,1);
  
    GtkWidget *cw_keyer_sidetone_level_b=gtk_spin_button_new_with_range(1.0,protocol==NEW_PROTOCOL?255.0:127.0,1.0);
    //gtk_widget_override_font(cw_keyer_sidetone_level_b, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_sidetone_level_b),(double)cw_keyer_sidetone_volume);
    gtk_widget_show(cw_keyer_sidetone_level_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_sidetone_level_b,1,6,1,1);
    g_signal_connect(cw_keyer_sidetone_level_b,"value_changed",G_CALLBACK(cw_keyer_sidetone_level_value_changed_cb),NULL);

    GtkWidget *cw_keyer_sidetone_frequency_label=gtk_label_new("Sidetone Freq:");
    //gtk_widget_override_font(cw_keyer_sidetone_frequency_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(cw_keyer_sidetone_frequency_label);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_sidetone_frequency_label,0,7,1,1);

    GtkWidget *cw_keyer_sidetone_frequency_b=gtk_spin_button_new_with_range(100.0,1000.0,1.0);
    //gtk_widget_override_font(cw_keyer_sidetone_frequency_b, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_sidetone_frequency_b),(double)cw_keyer_sidetone_frequency);
    gtk_widget_show(cw_keyer_sidetone_frequency_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_sidetone_frequency_b,1,7,1,1);
    g_signal_connect(cw_keyer_sidetone_frequency_b,"value_changed",G_CALLBACK(cw_keyer_sidetone_frequency_value_changed_cb),NULL);
  
    GtkWidget *cw_keyer_weight_label=gtk_label_new("Weight:");
    //gtk_widget_override_font(cw_keyer_weight_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(cw_keyer_weight_label);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_weight_label,0,8,1,1);

    GtkWidget *cw_keyer_weight_b=gtk_spin_button_new_with_range(0.0,100.0,1.0);
    //gtk_widget_override_font(cw_keyer_weight_b, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_weight_b),(double)cw_keyer_weight);
    gtk_widget_show(cw_keyer_weight_b);
    gtk_grid_attach(GTK_GRID(cw_grid),cw_keyer_weight_b,1,8,1,1);
    g_signal_connect(cw_keyer_weight_b,"value_changed",G_CALLBACK(cw_keyer_weight_value_changed_cb),NULL);
 
    id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),cw_grid,cw_label);
  }

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *oc_label=gtk_label_new("OC");
    GtkWidget *oc_grid=gtk_grid_new();
    //gtk_grid_set_row_homogeneous(GTK_GRID(oc_grid),TRUE);
    gtk_grid_set_column_spacing (GTK_GRID(oc_grid),10);

    GtkWidget *band_title=gtk_label_new("Band");
    //gtk_widget_override_font(band_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(band_title);
    gtk_grid_attach(GTK_GRID(oc_grid),band_title,0,0,1,1);

    GtkWidget *rx_title=gtk_label_new("Rx");
    //gtk_widget_override_font(rx_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(rx_title);
    gtk_grid_attach(GTK_GRID(oc_grid),rx_title,4,0,1,1);

    GtkWidget *tx_title=gtk_label_new("Tx");
    //gtk_widget_override_font(tx_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tx_title);
    gtk_grid_attach(GTK_GRID(oc_grid),tx_title,11,0,1,1);
  
    GtkWidget *tune_title=gtk_label_new("Tune (ORed with TX)");
    //gtk_widget_override_font(tune_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tune_title);
    gtk_grid_attach(GTK_GRID(oc_grid),tune_title,18,0,2,1);
  
    for(i=1;i<8;i++) {
      char oc_id[8];
      sprintf(oc_id,"%d",i);
      GtkWidget *oc_rx_title=gtk_label_new(oc_id);
      //gtk_widget_override_font(oc_rx_title, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(oc_rx_title);
      gtk_grid_attach(GTK_GRID(oc_grid),oc_rx_title,i,1,1,1);
      GtkWidget *oc_tx_title=gtk_label_new(oc_id);
      //gtk_widget_override_font(oc_tx_title, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(oc_tx_title);
      gtk_grid_attach(GTK_GRID(oc_grid),oc_tx_title,i+7,1,1,1);
/*
      GtkWidget *oc_tune_title=gtk_label_new(oc_id);
      //gtk_widget_override_font(oc_tune_title, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(oc_tune_title);
      gtk_grid_attach(GTK_GRID(oc_grid),oc_tune_title,i+14,1,1,1);
*/
  }

    for(i=0;i<HAM_BANDS;i++) {
      BAND *band=band_get_band(i);
  
      GtkWidget *band_label=gtk_label_new(band->title);
      //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(band_label);
      gtk_grid_attach(GTK_GRID(oc_grid),band_label,0,i+2,1,1);
  
      int mask;
      for(j=1;j<8;j++) {
        mask=0x01<<j;
        GtkWidget *oc_rx_b=gtk_check_button_new();
        //gtk_widget_override_font(oc_rx_b, pango_font_description_from_string("Arial 18"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_rx_b), (band->OCrx&mask)==mask);
        gtk_widget_show(oc_rx_b);
        gtk_grid_attach(GTK_GRID(oc_grid),oc_rx_b,j,i+2,1,1);
        g_signal_connect(oc_rx_b,"toggled",G_CALLBACK(oc_rx_cb),(gpointer)(j+(i<<4)));
  
        GtkWidget *oc_tx_b=gtk_check_button_new();
        //gtk_widget_override_font(oc_tx_b, pango_font_description_from_string("Arial 18"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_tx_b), (band->OCtx&mask)==mask);
        gtk_widget_show(oc_tx_b);
        gtk_grid_attach(GTK_GRID(oc_grid),oc_tx_b,j+7,i+2,1,1);
        g_signal_connect(oc_tx_b,"toggled",G_CALLBACK(oc_tx_cb),(gpointer)(j+(i<<4)));
  
/*
        GtkWidget *oc_tune_b=gtk_check_button_new();
        //gtk_widget_override_font(oc_tune_b, pango_font_description_from_string("Arial 18"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_tune_b), (band->OCtune&mask)==mask);
        gtk_widget_show(oc_tune_b);
        gtk_grid_attach(GTK_GRID(oc_grid),oc_tune_b,j+14,i+2,1,1);
        g_signal_connect(oc_tune_b,"toggled",G_CALLBACK(oc_tune_cb),(gpointer)(j+(i<<4)));
*/
      }
    }


    int mask;
    for(j=1;j<8;j++) {
      char oc_id[8];
      sprintf(oc_id,"%d",j);
      GtkWidget *oc_tune_title=gtk_label_new(oc_id);
      //gtk_widget_override_font(oc_tune_title, pango_font_description_from_string("Arial 18"));
      gtk_widget_show(oc_tune_title);
      gtk_grid_attach(GTK_GRID(oc_grid),oc_tune_title,18,j,1,1);
  
      mask=0x01<<j;
      GtkWidget *oc_tune_b=gtk_check_button_new();
      //gtk_widget_override_font(oc_tune_b, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oc_tune_b), (OCtune&mask)==mask);
      gtk_widget_show(oc_tune_b);
      gtk_grid_attach(GTK_GRID(oc_grid),oc_tune_b,19,j,1,1);
      g_signal_connect(oc_tune_b,"toggled",G_CALLBACK(oc_tune_cb),(gpointer)j);
    }

    GtkWidget *oc_full_tune_time_title=gtk_label_new("Full Tune(ms):");
    //gtk_widget_override_font(oc_full_tune_time_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(oc_full_tune_time_title);
    gtk_grid_attach(GTK_GRID(oc_grid),oc_full_tune_time_title,18,j,2,1);
    j++;
  
    GtkWidget *oc_full_tune_time_b=gtk_spin_button_new_with_range(0.0,9999.0,1.0);
    //gtk_widget_override_font(oc_full_tune_time_b, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(oc_full_tune_time_b),(double)OCfull_tune_time);
    gtk_widget_show(oc_full_tune_time_b);
    gtk_grid_attach(GTK_GRID(oc_grid),oc_full_tune_time_b,18,j,2,1);
    g_signal_connect(oc_full_tune_time_b,"value_changed",G_CALLBACK(oc_full_tune_time_cb),NULL);
    j++;
  
    GtkWidget *oc_memory_tune_time_title=gtk_label_new("Memory Tune(ms):");
    //gtk_widget_override_font(oc_memory_tune_time_title, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(oc_memory_tune_time_title);
    gtk_grid_attach(GTK_GRID(oc_grid),oc_memory_tune_time_title,18,j,2,1);
    j++;

    GtkWidget *oc_memory_tune_time_b=gtk_spin_button_new_with_range(0.0,9999.0,1.0);
    //gtk_widget_override_font(oc_memory_tune_time_b, pango_font_description_from_string("Arial 18"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(oc_memory_tune_time_b),(double)OCmemory_tune_time);
    gtk_widget_show(oc_memory_tune_time_b);
    gtk_grid_attach(GTK_GRID(oc_grid),oc_memory_tune_time_b,18,j,2,1);
    g_signal_connect(oc_memory_tune_time_b,"value_changed",G_CALLBACK(oc_memory_tune_time_cb),NULL);
    j++;
  
    id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),oc_grid,oc_label);
  }





  GtkWidget *exit_label=gtk_label_new("Exit");
  GtkWidget *exit_grid=gtk_grid_new();
  //gtk_grid_set_row_homogeneous(GTK_GRID(exit_grid),TRUE);
  gtk_grid_set_row_spacing (GTK_GRID(exit_grid),20);

  GtkWidget *exit_button=gtk_button_new_with_label("Exit PiHPSDR");
  g_signal_connect (exit_button, "pressed", G_CALLBACK(exit_pressed_event_cb), NULL);
  gtk_grid_attach(GTK_GRID(exit_grid),exit_button,0,0,1,1);

  GtkWidget *reboot_button=gtk_button_new_with_label("Reboot");
  g_signal_connect (reboot_button, "pressed", G_CALLBACK(reboot_pressed_event_cb), NULL);
  gtk_grid_attach(GTK_GRID(exit_grid),reboot_button,0,1,1,1);

  GtkWidget *shutdown_button=gtk_button_new_with_label("Shutdown");
  g_signal_connect (shutdown_button, "pressed", G_CALLBACK(shutdown_pressed_event_cb), NULL);
  gtk_grid_attach(GTK_GRID(exit_grid),shutdown_button,0,2,1,1);

  id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),exit_grid,exit_label);



/*
  GtkWidget *about_label=gtk_label_new("About");
  GtkWidget *about_grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(about_grid),TRUE);

  char build[64];

  sprintf(build,"build: %s %s",build_date, build_time);
  GtkWidget *pi_label=gtk_label_new("pihpsdr by John Melton g0orx/n6lyt");
  gtk_widget_show(pi_label);
  gtk_grid_attach(GTK_GRID(about_grid),pi_label,0,0,1,1);
  GtkWidget *filler_label=gtk_label_new("");
  gtk_widget_show(filler_label);
  gtk_grid_attach(GTK_GRID(about_grid),filler_label,0,1,1,1);
  GtkWidget *build_date_label=gtk_label_new(build);
  gtk_widget_show(build_date_label);
  gtk_grid_attach(GTK_GRID(about_grid),build_date_label,0,2,1,1);
   
  id=gtk_notebook_append_page(GTK_NOTEBOOK(notebook),about_grid,about_label);
*/
  gtk_container_add(GTK_CONTAINER(content),notebook);

  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close Dialog",GTK_RESPONSE_OK);
 // gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));

  gtk_widget_show_all(dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

  radioSaveState();

  gtk_widget_destroy (dialog);

  return TRUE;

}

GtkWidget* menu_init(int width,int height,GtkWidget *parent) {

  GdkRGBA black;
  black.red=0.0;
  black.green=0.0;
  black.blue=0.0;
  black.alpha=0.0;

  fprintf(stderr,"menu_init: width=%d height=%d\n",width,height);

  parent_window=parent;

  box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  gtk_widget_set_size_request (box, width, height);
  gtk_widget_override_background_color(box, GTK_STATE_NORMAL, &black);

  menu=gtk_button_new_with_label("Menu");
 // gtk_widget_set_size_request (menu, width, height);
  g_signal_connect (menu, "pressed", G_CALLBACK(menu_pressed_event_cb), NULL);
  gtk_box_pack_start (GTK_BOX(box),menu,TRUE,TRUE,0);

  gtk_widget_show_all(box);

  return box;
}
