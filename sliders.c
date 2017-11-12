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
#include <stdlib.h>
#include <math.h>

#include "receiver.h"
#include "sliders.h"
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
#include "transmitter.h"
#include "property.h"
#include "main.h"
#include "ext.h"

static int width;
static int height;

static GtkWidget *sliders;

#define NONE 0
#define AF_GAIN 1
#define MIC_GAIN 2
#define LINEIN_GAIN 3
#define AGC_GAIN 4
#define DRIVE 5
#define ATTENUATION 6
#define SQUELCH 7
#define COMP 8

static gint scale_timer;
static int scale_status=NONE;
static GtkWidget *scale_dialog;
static GtkWidget *af_gain_label;
static GtkWidget *af_gain_scale;
static GtkWidget *agc_gain_label;
static GtkWidget *agc_scale;
static GtkWidget *attenuation_label;
static GtkWidget *attenuation_scale;
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

static GdkRGBA white;
static GdkRGBA gray;

void sliders_update() {
  if(display_sliders) {
    if(mic_linein) {
      gtk_label_set_text(GTK_LABEL(mic_gain_label),"Linein:");
      gtk_range_set_range(GTK_RANGE(mic_gain_scale),0.0,31.0);
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
    } else {
      gtk_label_set_text(GTK_LABEL(mic_gain_label),"Mic (dB):");
      gtk_range_set_range(GTK_RANGE(mic_gain_scale),-10.0,50.0);
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
    }
  }
}

int sliders_active_receiver_changed(void *data) {
  if(display_sliders) {
    gtk_range_set_value(GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
    gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
    gtk_range_set_value (GTK_RANGE(attenuation_scale),active_receiver->attenuation);
    sliders_update();
  }
  return FALSE;
}

int scale_timeout_cb(gpointer data) {
  gtk_widget_destroy(scale_dialog);
  scale_status=NONE;
  return FALSE;
}

static void attenuation_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->attenuation=(int)gtk_range_get_value(GTK_RANGE(attenuation_scale));
  set_attenuation(active_receiver->attenuation);
}

void set_attenuation_value(double value) {
  active_receiver->attenuation=(int)value;
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(attenuation_scale),active_receiver->attenuation);
  } else {
    if(scale_status!=ATTENUATION) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
      scale_status=ATTENUATION;
      scale_dialog=gtk_dialog_new_with_buttons("Attenuation (dB)",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.00);
      gtk_widget_set_size_request (attenuation_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(attenuation_scale),active_receiver->attenuation);
      gtk_widget_show(attenuation_scale);
      gtk_container_add(GTK_CONTAINER(content),attenuation_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(attenuation_scale),active_receiver->attenuation);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
  set_attenuation(active_receiver->attenuation);
}

static void agcgain_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->agc_gain=gtk_range_get_value(GTK_RANGE(agc_scale));
  SetRXAAGCTop(active_receiver->id, active_receiver->agc_gain);
}

void set_agc_gain(double value) {
  active_receiver->agc_gain=value;
  SetRXAAGCTop(active_receiver->id, active_receiver->agc_gain);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
  } else {
    if(scale_status!=AGC_GAIN) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
      scale_status=AGC_GAIN;
      scale_dialog=gtk_dialog_new_with_buttons("AGC Gain",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-20.0, 120.0, 1.00);
      gtk_widget_set_size_request (agc_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
      gtk_widget_show(agc_scale);
      gtk_container_add(GTK_CONTAINER(content),agc_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void update_agc_gain(double gain) {
  set_agc_gain(gain);
}

static void afgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    active_receiver->volume=gtk_range_get_value(GTK_RANGE(af_gain_scale))/100.0;
#ifdef FREEDV
    if(!active_receiver->freedv) {
#endif
      SetRXAPanelGain1 (active_receiver->id, active_receiver->volume);
#ifdef FREEDV
    }
#endif
}

void update_af_gain() {
  set_af_gain(active_receiver->volume);
}

void set_af_gain(double value) {
  active_receiver->volume=value;
  SetRXAPanelGain1 (active_receiver->id, active_receiver->volume);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
  } else {
    if(scale_status!=AF_GAIN) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
      scale_status=AF_GAIN;
      scale_dialog=gtk_dialog_new_with_buttons("AF Gain",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      af_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_set_size_request (af_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
      gtk_widget_show(af_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),af_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

static void micgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    if(mic_linein) {
      linein_gain=(int)gtk_range_get_value(GTK_RANGE(widget));
    } else {
      mic_gain=gtk_range_get_value(GTK_RANGE(widget));
      double gain=pow(10.0, mic_gain / 20.0);
      SetTXAPanelGain1(transmitter->id,gain);
    }
}

void set_mic_gain(double value) {
  mic_gain=value;
  double gain=pow(10.0, mic_gain / 20.0);
  SetTXAPanelGain1(transmitter->id,gain);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
  } else {
    if(scale_status!=MIC_GAIN) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
      scale_status=MIC_GAIN;
      scale_dialog=gtk_dialog_new_with_buttons("Mic Gain (dB)",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-10.0, 50.0, 1.00);
      gtk_widget_set_size_request (mic_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
      gtk_widget_show(mic_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),mic_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }

  }
}

int update_mic_gain(void *data) {
  set_mic_gain(*(double*)data);
  free(data);
  return 0;
}

void set_linein_gain(int value) {
  linein_gain=value;
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
  } else {
    if(scale_status!=LINEIN_GAIN) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
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
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
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
  setDrive(value);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(drive_scale),value);
  } else {
    if(scale_status!=DRIVE) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
      scale_status=DRIVE;
      scale_dialog=gtk_dialog_new_with_buttons("Drive",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_set_size_request (drive_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(drive_scale),value);
      gtk_widget_show(drive_scale);
      gtk_container_add(GTK_CONTAINER(content),drive_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
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
  setSquelch(active_receiver);
}

static void squelch_enable_cb(GtkWidget *widget, gpointer data) {
  active_receiver->squelch_enable=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  setSquelch(active_receiver);
}

static void compressor_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor_level(transmitter,gtk_range_get_value(GTK_RANGE(widget)));
}

static void compressor_enable_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor(transmitter,gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

void set_squelch(RECEIVER* rx) {
  setSquelch(active_receiver);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
  } else {
    if(scale_status!=SQUELCH) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
      scale_status=SQUELCH;
      scale_dialog=gtk_dialog_new_with_buttons("Squelch",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      squelch_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
      gtk_widget_set_size_request (squelch_scale, 400, 30);
      gtk_widget_show(squelch_scale);
      gtk_container_add(GTK_CONTAINER(content),squelch_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void set_compression(TRANSMITTER* tx) {
//  if(display_sliders) {
//    gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
//  } else {
    if(scale_status!=COMP) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
      scale_status=COMP;
      scale_dialog=gtk_dialog_new_with_buttons("COMP",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      comp_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 20.0, 1.00);
      gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
      gtk_widget_set_size_request (comp_scale, 400, 30);
      gtk_widget_show(comp_scale);
      gtk_container_add(GTK_CONTAINER(content),comp_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  //}
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
  gtk_widget_show(af_gain_label);
  gtk_grid_attach(GTK_GRID(sliders),af_gain_label,0,0,1,1);

  af_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
  gtk_range_set_value (GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
  gtk_widget_show(af_gain_scale);
  gtk_grid_attach(GTK_GRID(sliders),af_gain_scale,1,0,2,1);
  g_signal_connect(G_OBJECT(af_gain_scale),"value_changed",G_CALLBACK(afgain_value_changed_cb),NULL);

  agc_gain_label=gtk_label_new("AGC:");
  gtk_widget_show(agc_gain_label);
  gtk_grid_attach(GTK_GRID(sliders),agc_gain_label,3,0,1,1);

  agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-20.0, 120.0, 1.0);
  gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
  gtk_widget_show(agc_scale);
  gtk_grid_attach(GTK_GRID(sliders),agc_scale,4,0,2,1);
  g_signal_connect(G_OBJECT(agc_scale),"value_changed",G_CALLBACK(agcgain_value_changed_cb),NULL);

  attenuation_label=gtk_label_new("ATT (dB):");
  gtk_widget_show(attenuation_label);
  gtk_grid_attach(GTK_GRID(sliders),attenuation_label,6,0,1,1);

  attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.0);
  gtk_range_set_value (GTK_RANGE(attenuation_scale),active_receiver->attenuation);
  gtk_widget_show(attenuation_scale);
  gtk_grid_attach(GTK_GRID(sliders),attenuation_scale,7,0,2,1);
  g_signal_connect(G_OBJECT(attenuation_scale),"value_changed",G_CALLBACK(attenuation_value_changed_cb),NULL);


  mic_gain_label=gtk_label_new(mic_linein?"Linein:":"Mic (dB):");
  gtk_grid_attach(GTK_GRID(sliders),mic_gain_label,0,1,1,1);

  mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,mic_linein?0.0:-10.0,mic_linein?31.0:50.0, 1.0);
  gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_linein?linein_gain:mic_gain);
  gtk_grid_attach(GTK_GRID(sliders),mic_gain_scale,1,1,2,1);
  g_signal_connect(G_OBJECT(mic_gain_scale),"value_changed",G_CALLBACK(micgain_value_changed_cb),NULL);

  drive_label=gtk_label_new("Drive:");
  gtk_grid_attach(GTK_GRID(sliders),drive_label,3,1,1,1);

  drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.0);
  gtk_range_set_value (GTK_RANGE(drive_scale),getDrive());
  gtk_widget_show(drive_scale);
  gtk_grid_attach(GTK_GRID(sliders),drive_scale,4,1,2,1);
  g_signal_connect(G_OBJECT(drive_scale),"value_changed",G_CALLBACK(drive_value_changed_cb),NULL);

  squelch_label=gtk_label_new("Squelch:");
  gtk_widget_show(squelch_label);
  gtk_grid_attach(GTK_GRID(sliders),squelch_label,6,1,1,1);

  squelch_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.0);
  gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
  gtk_widget_show(squelch_scale);
  gtk_grid_attach(GTK_GRID(sliders),squelch_scale,7,1,2,1);
  g_signal_connect(G_OBJECT(squelch_scale),"value_changed",G_CALLBACK(squelch_value_changed_cb),NULL);

  squelch_enable=gtk_check_button_new();
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(squelch_enable),active_receiver->squelch_enable);
  gtk_widget_show(squelch_enable);
  gtk_grid_attach(GTK_GRID(sliders),squelch_enable,9,1,1,1);
  g_signal_connect(squelch_enable,"toggled",G_CALLBACK(squelch_enable_cb),NULL);

  return sliders;
}
