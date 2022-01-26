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

//
// DL1YCF:
// uncomment the #define line following, then you will get
// a "TX compression" slider with an enabling checkbox
// in the bottom right of the sliders area, instead of the
// sequelch slider and checkbox.
// This option can also be passed to the compiler with "-D"
// and thus be activated through the Makefile.
//
//#define COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH 1
//

#include <gtk/gtk.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "receiver.h"
#include "sliders.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "discovered.h"
#include "new_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "vfo.h"
#include "alex.h"
#include "agc.h"
#include "channel.h"
#include "wdsp.h"
#include "radio.h"
#include "transmitter.h"
#include "property.h"
#include "main.h"
#include "ext.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "actions.h"

static int width;
static int height;

static GtkWidget *sliders;

gint scale_timer;
int scale_status=NO_ACTION;
int scale_rx=0;
GtkWidget *scale_dialog;

static GtkWidget *af_gain_label;
static GtkWidget *af_gain_scale;
static GtkWidget *rf_gain_label;
static GtkWidget *rf_gain_scale;
static GtkWidget *agc_gain_label;
static GtkWidget *agc_scale;
static GtkWidget *attenuation_label;
static GtkWidget *attenuation_scale;
static GtkWidget *c25_att_preamp_label;
static GtkWidget *c25_att_combobox;
static GtkWidget *c25_preamp_combobox;
static GtkWidget *mic_gain_label;
static GtkWidget *mic_gain_scale;
static GtkWidget *drive_label;
static GtkWidget *drive_scale;
static GtkWidget *squelch_label;
static GtkWidget *squelch_scale;
static GtkWidget *squelch_enable;
static GtkWidget *comp_label;
static GtkWidget *comp_scale;
static GtkWidget *comp_enable;
static GtkWidget *dummy_label;
static GtkWidget *filter_width_scale;
static GtkWidget *filter_shift_scale;
static GtkWidget *diversity_gain_scale;
static GtkWidget *diversity_phase_scale;

static GdkRGBA white;
static GdkRGBA gray;

void sliders_update() {
  if(display_sliders) {
    if(can_transmit) {
      if(mic_linein) {
        gtk_label_set_text(GTK_LABEL(mic_gain_label),"Linein:");
        gtk_range_set_range(GTK_RANGE(mic_gain_scale),0.0,31.0);
        gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
      } else {
        gtk_label_set_text(GTK_LABEL(mic_gain_label),"Mic:");
        gtk_range_set_range(GTK_RANGE(mic_gain_scale),-12.0,50.0);
        gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
      }
    }
  }
}

int sliders_active_receiver_changed(void *data) {
  if(display_sliders) {
    gtk_range_set_value(GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
    gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
    if (filter_board == CHARLY25) {
      update_att_preamp();
    } else {
      if(attenuation_scale!=NULL) gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
    }
    sliders_update();
  }
  return FALSE;
}

int scale_timeout_cb(gpointer data) {
  gtk_widget_destroy(scale_dialog);
  scale_status=NO_ACTION;
  return FALSE;
}

static void attenuation_value_changed_cb(GtkWidget *widget, gpointer data) {
  adc[active_receiver->adc].gain=gtk_range_get_value(GTK_RANGE(attenuation_scale));
  adc[active_receiver->adc].attenuation=(int)adc[active_receiver->adc].gain;
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_attenuation(client_socket,active_receiver->id,(int)adc[active_receiver->adc].attenuation);
  } else {
#endif
    set_attenuation(adc[active_receiver->adc].attenuation);
#ifdef CLIENT_SERVER
  }
#endif
}

void set_attenuation_value(double value) {
  g_print("%s\n",__FUNCTION__);
  adc[active_receiver->adc].attenuation=(int)value;
  set_attenuation(adc[active_receiver->adc].attenuation);
  if(display_sliders) {
    if (have_rx_gain) {
	gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
    } else {
        gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
    }
  } else {
    if(scale_status!=ATTENUATION) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      char title[64];
      if (have_rx_gain) {
	  sprintf(title,"RX GAIN - ADC-%d (dB)",active_receiver->adc);
      } else {
          sprintf(title,"Attenuation - ADC-%d (dB)",active_receiver->adc);
      }
      scale_status=ATTENUATION;
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      if (have_rx_gain) {
        attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0, 48.0, 1.00);
      } else {
        attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.00);
      }
      gtk_widget_set_size_request (attenuation_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
      gtk_widget_show(attenuation_scale);
      gtk_container_add(GTK_CONTAINER(content),attenuation_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void update_att_preamp(void) {
  // CHARLY25: update the ATT/Pre buttons to the values of the active RX
  // We should also set the attenuation for use in meter.c
  if (filter_board == CHARLY25) {
    char id[] = "x";
    if (active_receiver->adc != 0) {
      active_receiver->alex_attenuation=0;
      active_receiver->preamp=0;
      active_receiver->dither=0;
      adc[active_receiver->adc].attenuation = 0;
    }
    sprintf(id, "%d", active_receiver->alex_attenuation);
    adc[active_receiver->adc].attenuation = 12*active_receiver->alex_attenuation;
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(c25_att_combobox), id);
    sprintf(id, "%d", active_receiver->preamp + active_receiver->dither);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(c25_preamp_combobox), id);
  }
}

void att_type_changed(void) {
  g_print("%s\n",__FUNCTION__);
  if (filter_board == CHARLY25) {
    if(attenuation_label!=NULL) gtk_widget_hide(attenuation_label);
    if(attenuation_scale!=NULL) gtk_widget_hide(attenuation_scale);
    gtk_widget_show(c25_att_preamp_label);
    gtk_widget_show(c25_att_combobox);
    gtk_widget_show(c25_preamp_combobox);
    update_att_preamp();
  } else {
    gtk_widget_hide(c25_att_preamp_label);
    gtk_widget_hide(c25_att_combobox);
    gtk_widget_hide(c25_preamp_combobox);
    if(attenuation_label!=NULL) gtk_widget_show(attenuation_label);
    if(attenuation_scale!=NULL) gtk_widget_show(attenuation_scale);
  }
}

static gboolean load_att_type_cb(gpointer data) {
  att_type_changed();
  return G_SOURCE_REMOVE;
}

static void c25_att_combobox_changed(GtkWidget *widget, gpointer data) {
  int val = atoi(gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget)));
  if (active_receiver->adc == 0) {
    // this button is only valid for the first receiver
    // store attenuation, such that in meter.c the correct level is displayed
    adc[active_receiver->adc].attenuation = 12*val;
    set_alex_attenuation(val);
  } else {
    // always show "0 dB" on the button if the second RX is active
    if (val != 0) {
      gtk_combo_box_set_active_id(GTK_COMBO_BOX(c25_att_combobox), "0");
    }
  }
}

static void c25_preamp_combobox_changed(GtkWidget *widget, gpointer data) {
  int val = atoi(gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget)));
  if (active_receiver->id == 0) {
    // This button is only valid for the first receiver
    // dither and preamp are "misused" to store the PreAmp value.
    // this has to be exploited in meter.c
    active_receiver->dither = (val >= 2);  // second preamp ON
    active_receiver->preamp = (val >= 1);  // first  preamp ON
  } else{
    // always show "0 dB" on the button if the second RX is active
    if (val != 0) {
      gtk_combo_box_set_active_id(GTK_COMBO_BOX(c25_preamp_combobox), "0");
    }
  }
}

static void agcgain_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->agc_gain=gtk_range_get_value(GTK_RANGE(agc_scale));
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_agc_gain(client_socket,active_receiver->id,(int)active_receiver->agc_gain,(int)active_receiver->agc_hang,(int)active_receiver->agc_thresh);
  } else {
#endif
    SetRXAAGCTop(active_receiver->id, active_receiver->agc_gain);
    GetRXAAGCHangLevel(active_receiver->id, &active_receiver->agc_hang);
    GetRXAAGCThresh(active_receiver->id, &active_receiver->agc_thresh, 4096.0, (double)active_receiver->sample_rate);
#ifdef CLIENT_SERVER
  }
#endif
}

void set_agc_gain(int rx,double value) {
  g_print("%s\n",__FUNCTION__);
  receiver[rx]->agc_gain=value;
  SetRXAAGCTop(receiver[rx]->id, receiver[rx]->agc_gain);
  GetRXAAGCHangLevel(receiver[rx]->id, &receiver[rx]->agc_hang);
  GetRXAAGCThresh(receiver[rx]->id, &receiver[rx]->agc_thresh, 4096.0, (double)receiver[rx]->sample_rate);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(agc_scale),receiver[rx]->agc_gain);
  } else {
    if(scale_status!=AGC_GAIN || scale_rx!=rx) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=AGC_GAIN;
      scale_rx=rx;
      char title[64];
      sprintf(title,"AGC Gain RX %d",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-20.0, 120.0, 1.00);
      gtk_widget_set_size_request (agc_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(agc_scale),receiver[rx]->agc_gain);
      gtk_widget_show(agc_scale);
      gtk_container_add(GTK_CONTAINER(content),agc_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(agc_scale),receiver[rx]->agc_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void update_agc_gain(double gain) {
  set_agc_gain(active_receiver->id,gain);
}

static void afgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    active_receiver->volume=gtk_range_get_value(GTK_RANGE(af_gain_scale))/100.0;

#ifdef CLIENT_SERVER
    if(radio_is_remote) {
      int v=(int)(active_receiver->volume*100.0);
      send_volume(client_socket,active_receiver->id,v);
    } else {
#endif
      SetRXAPanelGain1 (active_receiver->id, active_receiver->volume);
#ifdef CLIENT_SERVER
    }
#endif
}

void update_af_gain() {
  set_af_gain(active_receiver->id,active_receiver->volume);
}

void set_af_gain(int rx,double value) {
  receiver[rx]->volume=value;
  SetRXAPanelGain1 (receiver[rx]->id, receiver[rx]->volume);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(af_gain_scale),receiver[rx]->volume*100.0);
  } else {
    if(scale_status!=AF_GAIN || scale_rx!=rx) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=AF_GAIN;
      scale_rx=rx;
      char title[64];
      sprintf(title,"AF Gain RX %d",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      af_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_set_size_request (af_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(af_gain_scale),receiver[rx]->volume*100.0);
      gtk_widget_show(af_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),af_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(af_gain_scale),receiver[rx]->volume*100.0);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

static void rf_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
    adc[active_receiver->adc].gain=gtk_range_get_value(GTK_RANGE(rf_gain_scale));
#ifdef CLIENT_SERVER
    if (radio_is_remote) {
      send_rfgain(client_socket, active_receiver->id, adc[active_receiver->adc].gain);
    }
    return;
#endif
    switch(protocol) {
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
        soapy_protocol_set_gain(active_receiver);
	break;
#endif
      default:
	break;
    }
}

void update_rf_gain() {
  //set_rf_gain(active_receiver->id,active_receiver->rf_gain);
  set_rf_gain(active_receiver->id,adc[active_receiver->id].gain);
}

void set_rf_gain(int rx,double value) {
  g_print("%s\n",__FUNCTION__);
  adc[receiver[rx]->id].gain=value;
#ifdef SOAPYSDR
  if(protocol==SOAPYSDR_PROTOCOL) {
    soapy_protocol_set_gain(receiver[rx]);
  }
#endif
  if(display_sliders) {
    //gtk_range_set_value (GTK_RANGE(attenuation_scale),receiver[rx]->rf_gain);
    gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[receiver[rx]->id].gain);
  } else {
    if(scale_status!=RF_GAIN || scale_rx!=rx) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=RF_GAIN;
      scale_rx=rx;
      char title[64];
      sprintf(title,"RF Gain RX %d",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      rf_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_set_size_request (rf_gain_scale, 400, 30);
      //gtk_range_set_value (GTK_RANGE(rf_gain_scale),receiver[rx]->rf_gain);
      gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[receiver[rx]->id].gain);
      gtk_widget_show(rf_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),rf_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      //gtk_range_set_value (GTK_RANGE(rf_gain_scale),receiver[rx]->rf_gain);
      gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[receiver[rx]->id].gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void set_filter_width(int rx,int width) {
  g_print("%s\n",__FUNCTION__);
    if(scale_status!=IF_WIDTH || scale_rx!=rx) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=IF_WIDTH;
      scale_rx=rx;
      char title[64];
      sprintf(title,"Filter Width RX %d (Hz)",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      filter_width_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 4000.0, 1.00);
      gtk_widget_set_size_request (filter_width_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(filter_width_scale),(double)width);
      gtk_widget_show(filter_width_scale);
      gtk_container_add(GTK_CONTAINER(content),filter_width_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(filter_width_scale),(double)width);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
}

void set_filter_shift(int rx,int shift) {
  g_print("%s\n",__FUNCTION__);
    if(scale_status!=IF_SHIFT || scale_rx!=rx) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=IF_SHIFT;
      scale_rx=rx;
      char title[64];
      sprintf(title,"Filter SHIFT RX %d (Hz)",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      filter_shift_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-1000.0, 1000.0, 1.00);
      gtk_widget_set_size_request (filter_shift_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(filter_shift_scale),(double)shift);
      gtk_widget_show(filter_shift_scale);
      gtk_container_add(GTK_CONTAINER(content),filter_shift_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(filter_shift_scale),(double)shift);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
}

static void micgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    if(mic_linein) {
      linein_gain=(int)gtk_range_get_value(GTK_RANGE(widget));
    } else {
      mic_gain=gtk_range_get_value(GTK_RANGE(widget));
      SetTXAPanelGain1(transmitter->id,pow(10.0, mic_gain/20.0));
    }
}

void set_mic_gain(double value) {
  g_print("%s\n",__FUNCTION__);
  if(can_transmit) {
    mic_gain=value;
    SetTXAPanelGain1(transmitter->id,pow(10.0, mic_gain/20.0));
    if(display_sliders) {
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
    } else {
      if(scale_status!=MIC_GAIN) {
        if(scale_status!=NO_ACTION) {
          g_source_remove(scale_timer);
          gtk_widget_destroy(scale_dialog);
          scale_status=NO_ACTION;
        }
      }
      if(scale_status==NO_ACTION) {
        scale_status=MIC_GAIN;
        scale_dialog=gtk_dialog_new_with_buttons("Mic Gain",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
        GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
        mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0, 50.0, 1.00);
        gtk_widget_set_size_request (mic_gain_scale, 400, 30);
        gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
        gtk_widget_show(mic_gain_scale);
        gtk_container_add(GTK_CONTAINER(content),mic_gain_scale);
        scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
        //gtk_widget_show_all(scale_dialog);
        gtk_dialog_run(GTK_DIALOG(scale_dialog));
      } else {
        g_source_remove(scale_timer);
        gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
        scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      }
    }
  }
}

void set_linein_gain(int value) {
  g_print("%s\n",__FUNCTION__);
  linein_gain=value;
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
  } else {
    if(scale_status!=LINEIN_GAIN) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=LINEIN_GAIN;
      scale_dialog=gtk_dialog_new_with_buttons("Linein Gain",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.00);
      gtk_widget_set_size_request (mic_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
      gtk_widget_show(mic_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),mic_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

int update_linein_gain(void *data) {
  set_linein_gain(*(int*)data);
  free(data);
  return 0;
}

void set_drive(double value) {
  g_print("%s\n",__FUNCTION__);
  setDrive(value);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(drive_scale),value);
  } else {
    if(scale_status!=DRIVE) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=DRIVE;
      scale_dialog=gtk_dialog_new_with_buttons("Drive",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, drive_max, 1.00);
      gtk_widget_override_font(drive_scale, pango_font_description_from_string(SLIDERS_FONT));
      gtk_widget_set_size_request (drive_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(drive_scale),value);
      gtk_widget_show(drive_scale);
      gtk_container_add(GTK_CONTAINER(content),drive_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(drive_scale),value);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

static void drive_value_changed_cb(GtkWidget *widget, gpointer data) {
  setDrive(gtk_range_get_value(GTK_RANGE(drive_scale)));
}

int update_drive(void *data) {
  set_drive(*(double *)data);
  free(data);
  return 0;
}

static void squelch_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->squelch=gtk_range_get_value(GTK_RANGE(widget));
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_squelch(client_socket,active_receiver->id,active_receiver->squelch_enable,active_receiver->squelch);
  } else {
#endif
    setSquelch(active_receiver);
#ifdef CLIENT_SERVER
  }
#endif
}

static void squelch_enable_cb(GtkWidget *widget, gpointer data) {
  active_receiver->squelch_enable=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_squelch(client_socket,active_receiver->id,active_receiver->squelch_enable,active_receiver->squelch);
  } else {
#endif
    setSquelch(active_receiver);
#ifdef CLIENT_SERVER
  }
#endif
}

static void compressor_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor_level(transmitter,gtk_range_get_value(GTK_RANGE(widget)));
  // This value is now also reflected in the VFO panel
  g_idle_add(ext_vfo_update, NULL);

}

static void compressor_enable_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor(transmitter,gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
  // This value is now also reflected in the VFO panel
  g_idle_add(ext_vfo_update, NULL);
}

void set_squelch(RECEIVER *rx) {
  g_print("%s\n",__FUNCTION__);
  //
  // automatically enable/disable squelch if squelch value changed
  // you can still enable/disable squelch via the check-box, but
  // as soon the slider is moved squelch is enabled/disabled
  // depending on the "new" squelch value
  //
  rx->squelch_enable = (rx->squelch > 0.5);
  setSquelch(rx);
#ifndef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(squelch_scale),rx->squelch);
  } else {
#endif
    if(scale_status!=SQUELCH) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=SQUELCH;
      char title[64];
      sprintf(title,"Squelch RX %d (Hz)",rx->id);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      squelch_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_override_font(squelch_scale, pango_font_description_from_string(SLIDERS_FONT));
      gtk_range_set_value (GTK_RANGE(squelch_scale),rx->squelch);
      gtk_widget_set_size_request (squelch_scale, 400, 30);
      gtk_widget_show(squelch_scale);
      gtk_container_add(GTK_CONTAINER(content),squelch_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(squelch_scale),rx->squelch);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
#ifndef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  }
#endif
}

void set_compression(TRANSMITTER* tx) {
  g_print("%s\n",__FUNCTION__);
  // Update VFO panel to reflect changed value
  g_idle_add(ext_vfo_update, NULL);
#ifdef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
  } else {
#endif
    if(scale_status!=COMPRESSION) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=COMPRESSION;
      scale_dialog=gtk_dialog_new_with_buttons("COMP",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      comp_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 20.0, 1.00);
      gtk_widget_override_font(comp_scale, pango_font_description_from_string(SLIDERS_FONT));
      gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
      gtk_widget_set_size_request (comp_scale, 400, 30);
      gtk_widget_show(comp_scale);
      gtk_container_add(GTK_CONTAINER(content),comp_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
#ifdef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  }
#endif
}

void show_diversity_gain() {
  g_print("%s\n",__FUNCTION__);
    if(scale_status!=DIV_GAIN) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=DIV_GAIN;
      scale_dialog=gtk_dialog_new_with_buttons("Diversity Gain",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      diversity_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-27.0, 27.0, 0.01);
      gtk_widget_set_size_request (diversity_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(diversity_gain_scale),div_gain);
      gtk_widget_show(diversity_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),diversity_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(diversity_gain_scale),div_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
}

void show_diversity_phase() {
  g_print("%s\n",__FUNCTION__);
    if(scale_status!=DIV_PHASE) {
      if(scale_status!=NO_ACTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_ACTION;
      }
    }
    if(scale_status==NO_ACTION) {
      scale_status=DIV_PHASE;
      scale_dialog=gtk_dialog_new_with_buttons("Diversity Phase",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      diversity_phase_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -180.0, 180.0, 0.1);
      gtk_widget_set_size_request (diversity_phase_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(diversity_phase_scale),div_phase);
      gtk_widget_show(diversity_phase_scale);
      gtk_container_add(GTK_CONTAINER(content),diversity_phase_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(diversity_phase_scale),div_phase);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
}


GtkWidget *sliders_init(int my_width, int my_height) {
  width=my_width;
  height=my_height;

fprintf(stderr,"sliders_init: width=%d height=%d\n", width,height);

  sliders=gtk_grid_new();
  gtk_widget_set_size_request (sliders, width, height);
  gtk_grid_set_row_homogeneous(GTK_GRID(sliders), FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(sliders),TRUE);

  af_gain_label=gtk_label_new("AF:");
  gtk_widget_override_font(af_gain_label, pango_font_description_from_string(SLIDERS_FONT));
  gtk_widget_show(af_gain_label);
  gtk_grid_attach(GTK_GRID(sliders),af_gain_label,0,0,1,1);

  af_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
  gtk_widget_override_font(af_gain_scale, pango_font_description_from_string(SLIDERS_FONT));
  gtk_range_set_increments (GTK_RANGE(af_gain_scale),1.0,1.0);
  gtk_range_set_value (GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
  gtk_widget_show(af_gain_scale);
  gtk_grid_attach(GTK_GRID(sliders),af_gain_scale,1,0,2,1);
  g_signal_connect(G_OBJECT(af_gain_scale),"value_changed",G_CALLBACK(afgain_value_changed_cb),NULL);

  agc_gain_label=gtk_label_new("AGC:");
  gtk_widget_override_font(agc_gain_label, pango_font_description_from_string(SLIDERS_FONT));
  gtk_widget_show(agc_gain_label);
  gtk_grid_attach(GTK_GRID(sliders),agc_gain_label,3,0,1,1);

  agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-20.0, 120.0, 1.0);
  gtk_widget_override_font(agc_scale, pango_font_description_from_string(SLIDERS_FONT));
  gtk_range_set_increments (GTK_RANGE(agc_scale),1.0,1.0);
  gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
  gtk_widget_show(agc_scale);
  gtk_grid_attach(GTK_GRID(sliders),agc_scale,4,0,2,1);
  g_signal_connect(G_OBJECT(agc_scale),"value_changed",G_CALLBACK(agcgain_value_changed_cb),NULL);


  if(have_rx_gain) {
    rf_gain_label=gtk_label_new("RX-GAIN:");
    gtk_widget_override_font(rf_gain_label, pango_font_description_from_string(SLIDERS_FONT));
    gtk_widget_show(rf_gain_label);
    gtk_grid_attach(GTK_GRID(sliders),rf_gain_label,6,0,1,1);
#ifdef SOAPYSDR
    if(protocol==SOAPYSDR_PROTOCOL) {
      rf_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,adc[0].min_gain, adc[0].max_gain, 1.0);
      gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[0].gain);
    } else {
#endif
      rf_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0, 48.0, 1.0);
      gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[active_receiver->adc].attenuation);
#ifdef SOAPYSDR
    }
#endif
    gtk_widget_override_font(rf_gain_scale, pango_font_description_from_string(SLIDERS_FONT));
    gtk_range_set_increments (GTK_RANGE(rf_gain_scale),1.0,1.0);
    gtk_widget_show(rf_gain_scale);
    gtk_grid_attach(GTK_GRID(sliders),rf_gain_scale,7,0,2,1);
    g_signal_connect(G_OBJECT(rf_gain_scale),"value_changed",G_CALLBACK(rf_gain_value_changed_cb),NULL);
  } else {
    attenuation_label=gtk_label_new("ATT (dB):");
    gtk_widget_override_font(attenuation_label, pango_font_description_from_string(SLIDERS_FONT));
    gtk_widget_show(attenuation_label);
    gtk_grid_attach(GTK_GRID(sliders),attenuation_label,6,0,1,1);
    attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.0);
    gtk_widget_override_font(attenuation_scale, pango_font_description_from_string(SLIDERS_FONT));
    gtk_range_set_value (GTK_RANGE(attenuation_scale),adc[active_receiver->adc].attenuation);
    gtk_range_set_increments (GTK_RANGE(attenuation_scale),1.0,1.0);
    gtk_widget_show(attenuation_scale);
    gtk_grid_attach(GTK_GRID(sliders),attenuation_scale,7,0,2,1);
    g_signal_connect(G_OBJECT(attenuation_scale),"value_changed",G_CALLBACK(attenuation_value_changed_cb),NULL);
  }

  c25_att_preamp_label = gtk_label_new("Att/PreAmp");
  gtk_widget_override_font(c25_att_preamp_label, pango_font_description_from_string(SLIDERS_FONT));
  gtk_grid_attach(GTK_GRID(sliders), c25_att_preamp_label, 6, 0, 1, 1);

  c25_att_combobox = gtk_combo_box_text_new();
  gtk_widget_override_font(c25_att_combobox, pango_font_description_from_string(SLIDERS_FONT));
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_att_combobox), "0", "0 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_att_combobox), "1", "-12 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_att_combobox), "2", "-24 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_att_combobox), "3", "-36 dB");
  gtk_grid_attach(GTK_GRID(sliders), c25_att_combobox, 7, 0, 1, 1);
  g_signal_connect(G_OBJECT(c25_att_combobox), "changed", G_CALLBACK(c25_att_combobox_changed), NULL);

  c25_preamp_combobox = gtk_combo_box_text_new();
  gtk_widget_override_font(c25_preamp_combobox, pango_font_description_from_string(SLIDERS_FONT));
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_preamp_combobox), "0", "0 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_preamp_combobox), "1", "18 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_preamp_combobox), "2", "36 dB");
  gtk_grid_attach(GTK_GRID(sliders), c25_preamp_combobox, 8, 0, 1, 1);
  g_signal_connect(G_OBJECT(c25_preamp_combobox), "changed", G_CALLBACK(c25_preamp_combobox_changed), NULL);
  g_idle_add(load_att_type_cb, NULL);


  if(can_transmit) {

    mic_gain_label=gtk_label_new(mic_linein?"Linein:":"Mic:");
    gtk_widget_override_font(mic_gain_label, pango_font_description_from_string(SLIDERS_FONT));
    gtk_grid_attach(GTK_GRID(sliders),mic_gain_label,0,1,1,1);

    mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,mic_linein?0.0:-12.0,mic_linein?31.0:50.0, 1.0);
    gtk_range_set_increments (GTK_RANGE(mic_gain_scale),1.0,1.0);
    gtk_widget_override_font(mic_gain_scale, pango_font_description_from_string(SLIDERS_FONT));
    gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_linein?linein_gain:mic_gain);
    gtk_grid_attach(GTK_GRID(sliders),mic_gain_scale,1,1,2,1);
    g_signal_connect(G_OBJECT(mic_gain_scale),"value_changed",G_CALLBACK(micgain_value_changed_cb),NULL);

    drive_label=gtk_label_new("Drive:");
    gtk_widget_override_font(drive_label, pango_font_description_from_string(SLIDERS_FONT));
    gtk_grid_attach(GTK_GRID(sliders),drive_label,3,1,1,1);
    drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, drive_max, 1.00);
    gtk_widget_override_font(drive_scale, pango_font_description_from_string(SLIDERS_FONT));
    gtk_range_set_increments (GTK_RANGE(drive_scale),1.0,1.0);
    gtk_range_set_value (GTK_RANGE(drive_scale),getDrive());
    gtk_widget_show(drive_scale);
    gtk_grid_attach(GTK_GRID(sliders),drive_scale,4,1,2,1);
    g_signal_connect(G_OBJECT(drive_scale),"value_changed",G_CALLBACK(drive_value_changed_cb),NULL);
  }

#ifndef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  squelch_label=gtk_label_new("Squelch:");
  gtk_widget_override_font(squelch_label, pango_font_description_from_string(SLIDERS_FONT));
  gtk_widget_show(squelch_label);
  gtk_grid_attach(GTK_GRID(sliders),squelch_label,6,1,1,1);

  squelch_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.0);
  gtk_widget_override_font(squelch_scale, pango_font_description_from_string(SLIDERS_FONT));
  gtk_range_set_increments (GTK_RANGE(squelch_scale),1.0,1.0);
  gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
  gtk_widget_show(squelch_scale);
  gtk_grid_attach(GTK_GRID(sliders),squelch_scale,7,1,2,1);
  g_signal_connect(G_OBJECT(squelch_scale),"value_changed",G_CALLBACK(squelch_value_changed_cb),NULL);

  squelch_enable=gtk_check_button_new();
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(squelch_enable),active_receiver->squelch_enable);
  gtk_widget_show(squelch_enable);
  gtk_grid_attach(GTK_GRID(sliders),squelch_enable,9,1,1,1);
  g_signal_connect(squelch_enable,"toggled",G_CALLBACK(squelch_enable_cb),NULL);
#else
  if(can_transmit) {
    comp_label=gtk_label_new("COMP:");
    gtk_widget_override_font(comp_label, pango_font_description_from_string(SLIDERS_FONT));
    gtk_widget_show(comp_label);
    gtk_grid_attach(GTK_GRID(sliders),comp_label,6,1,1,1);

    comp_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 20.0, 1.0);
    gtk_widget_override_font(comp_scale, pango_font_description_from_string(SLIDERS_FONT));
    gtk_range_set_increments (GTK_RANGE(comp_scale),1.0,1.0);
    gtk_range_set_value (GTK_RANGE(comp_scale),transmitter->compressor_level);
    gtk_widget_show(comp_scale);
    gtk_grid_attach(GTK_GRID(sliders),comp_scale,7,1,2,1);
    g_signal_connect(G_OBJECT(comp_scale),"value_changed",G_CALLBACK(compressor_value_changed_cb),NULL);
  
    comp_enable=gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(comp_enable),transmitter->compressor);
    gtk_widget_show(comp_enable);
    gtk_grid_attach(GTK_GRID(sliders),comp_enable,9,1,1,1);
    g_signal_connect(comp_enable,"toggled",G_CALLBACK(compressor_enable_cb),NULL);
  }
#endif

  return sliders;
}
