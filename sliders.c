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
#include <math.h>

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
#include "property.h"

static int width;
static int height;

static GtkWidget *parent_window;
static GtkWidget *sliders;

#define NONE 0
#define AF_GAIN 1
#define MIC_GAIN 2
#define AGC_GAIN 3
#define DRIVE 4
#define TUNE_DRIVE 5
#define ATTENUATION 6

//#define MIC_GAIN_FUDGE 25.0

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
static GtkWidget *tune_label;
static GtkWidget *tune_scale;
static GtkWidget *dummy_label;

static GdkRGBA white;
static GdkRGBA gray;

int scale_timeout_cb(gpointer data) {
  gtk_widget_destroy(scale_dialog);
  scale_status=NONE;
  return FALSE;
}

static void attenuation_value_changed_cb(GtkWidget *widget, gpointer data) {
  attenuation=(int)gtk_range_get_value(GTK_RANGE(attenuation_scale));
  set_attenuation(attenuation);
}

void set_attenuation_value(double value) {
  attenuation=(int)value;
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(attenuation_scale),attenuation);
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
      scale_dialog=gtk_dialog_new_with_buttons("Attenuation (dB)",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.00);
      gtk_widget_set_size_request (attenuation_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(attenuation_scale),attenuation);
      gtk_widget_show(attenuation_scale);
      gtk_container_add(GTK_CONTAINER(content),attenuation_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(attenuation_scale),attenuation);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
  set_attenuation(attenuation);
}

static void agcgain_value_changed_cb(GtkWidget *widget, gpointer data) {
  agc_gain=gtk_range_get_value(GTK_RANGE(agc_scale));
  SetRXAAGCTop(CHANNEL_RX0, agc_gain);
}

void set_agc_gain(double value) {
  agc_gain=value;
  SetRXAAGCTop(CHANNEL_RX0, agc_gain);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(agc_scale),agc_gain);
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
      scale_dialog=gtk_dialog_new_with_buttons("AGC Gain",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-20.0, 120.0, 1.00);
      gtk_widget_set_size_request (agc_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(agc_scale),agc_gain);
      gtk_widget_show(agc_scale);
      gtk_container_add(GTK_CONTAINER(content),agc_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(agc_scale),agc_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

static void afgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    volume=gtk_range_get_value(GTK_RANGE(af_gain_scale))/100.0;
    SetRXAPanelGain1 (CHANNEL_RX0, volume);
}

int update_af_gain(void *data) {
  set_af_gain(volume);
  return 0;
}

void set_af_gain(double value) {
  volume=value;
  SetRXAPanelGain1 (CHANNEL_RX0, volume);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(af_gain_scale),volume*100.0);
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
      scale_dialog=gtk_dialog_new_with_buttons("AF Gain",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      af_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_set_size_request (af_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(af_gain_scale),volume*100.0);
      gtk_widget_show(af_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),af_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(af_gain_scale),volume*100.0);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

static void micgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    mic_gain=gtk_range_get_value(GTK_RANGE(widget));
    double gain=pow(10.0, mic_gain / 20.0);
    if(protocol==NEW_PROTOCOL) {
      SetTXAPanelGain1(CHANNEL_TX,gain);
    }
}

void set_mic_gain(double value) {
  mic_gain=value;
  double gain=pow(10.0, mic_gain / 20.0);
  if(protocol==NEW_PROTOCOL) {
    SetTXAPanelGain1(CHANNEL_TX,gain);
  }
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
      scale_dialog=gtk_dialog_new_with_buttons(mic_linein?"Linein Gain (dB)":"Mic Gain (dB)",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
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
      scale_dialog=gtk_dialog_new_with_buttons("Drive",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
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

void set_tune(double value) {
  setTuneDrive(value);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(tune_scale),value);
  } else {
    if(scale_status!=TUNE_DRIVE) {
      if(scale_status!=NONE) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NONE;
      }
    }
    if(scale_status==NONE) {
      scale_status=TUNE_DRIVE;
      scale_dialog=gtk_dialog_new_with_buttons("Tune Drive",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      tune_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_set_size_request (tune_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(tune_scale),value);
      gtk_widget_show(tune_scale);
      gtk_container_add(GTK_CONTAINER(content),tune_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      int result=gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(tune_scale),value);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

static void tune_value_changed_cb(GtkWidget *widget, gpointer data) {
  setTuneDrive(gtk_range_get_value(GTK_RANGE(tune_scale)));
}

GtkWidget *sliders_init(int my_width, int my_height, GtkWidget* parent) {
    width=my_width;
    height=my_height;
    parent_window=parent;

    fprintf(stderr,"sliders_init: width=%d height=%d\n", width,height);

    sliders=gtk_grid_new();
    gtk_widget_set_size_request (sliders, width, height);
    gtk_grid_set_row_homogeneous(GTK_GRID(sliders), FALSE);
    gtk_grid_set_column_homogeneous(GTK_GRID(sliders),TRUE);

  af_gain_label=gtk_label_new("AF:");
  //gtk_widget_override_font(af_gain_label, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(af_gain_label);
  gtk_grid_attach(GTK_GRID(sliders),af_gain_label,0,0,1,1);

  af_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
  gtk_range_set_value (GTK_RANGE(af_gain_scale),volume*100.0);
  gtk_widget_show(af_gain_scale);
  gtk_grid_attach(GTK_GRID(sliders),af_gain_scale,1,0,2,1);
  g_signal_connect(G_OBJECT(af_gain_scale),"value_changed",G_CALLBACK(afgain_value_changed_cb),NULL);

  agc_gain_label=gtk_label_new("AGC:");
  //gtk_widget_override_font(agc_gain_label, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(agc_gain_label);
  gtk_grid_attach(GTK_GRID(sliders),agc_gain_label,3,0,1,1);

  agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-20.0, 120.0, 1.0);
  gtk_range_set_value (GTK_RANGE(agc_scale),agc_gain);
  gtk_widget_show(agc_scale);
  gtk_grid_attach(GTK_GRID(sliders),agc_scale,4,0,2,1);
  g_signal_connect(G_OBJECT(agc_scale),"value_changed",G_CALLBACK(agcgain_value_changed_cb),NULL);

  attenuation_label=gtk_label_new("ATT (dB):");
  //gtk_widget_override_font(attenuation_label, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(attenuation_label);
  gtk_grid_attach(GTK_GRID(sliders),attenuation_label,6,0,1,1);

  attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.0);
  gtk_range_set_value (GTK_RANGE(attenuation_scale),attenuation);
  gtk_widget_show(attenuation_scale);
  gtk_grid_attach(GTK_GRID(sliders),attenuation_scale,7,0,2,1);
  g_signal_connect(G_OBJECT(attenuation_scale),"value_changed",G_CALLBACK(attenuation_value_changed_cb),NULL);



  mic_gain_label=gtk_label_new(mic_linein?"Linein (dB):":"Mic (dB):");
  //gtk_widget_override_font(mic_gain_label, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(mic_gain_label);
  gtk_grid_attach(GTK_GRID(sliders),mic_gain_label,0,1,1,1);

  mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-10.0, 50.0, 1.0);
  gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
  gtk_widget_show(mic_gain_scale);
  gtk_grid_attach(GTK_GRID(sliders),mic_gain_scale,1,1,2,1);
  g_signal_connect(G_OBJECT(mic_gain_scale),"value_changed",G_CALLBACK(micgain_value_changed_cb),NULL);

  drive_label=gtk_label_new("Drive:");
  //gtk_widget_override_font(drive_label, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(drive_label);
  gtk_grid_attach(GTK_GRID(sliders),drive_label,3,1,1,1);

  drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.0);
  gtk_range_set_value (GTK_RANGE(drive_scale),getDrive());
  gtk_widget_show(drive_scale);
  gtk_grid_attach(GTK_GRID(sliders),drive_scale,4,1,2,1);
  g_signal_connect(G_OBJECT(drive_scale),"value_changed",G_CALLBACK(drive_value_changed_cb),NULL);

  tune_label=gtk_label_new("Tune:");
  //gtk_widget_override_font(tune_label, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(tune_label);
  gtk_grid_attach(GTK_GRID(sliders),tune_label,6,1,1,1);

  tune_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.0);
  gtk_range_set_value (GTK_RANGE(tune_scale),getTuneDrive());
  gtk_widget_show(tune_scale);
  gtk_grid_attach(GTK_GRID(sliders),tune_scale,7,1,2,1);
  g_signal_connect(G_OBJECT(tune_scale),"value_changed",G_CALLBACK(tune_value_changed_cb),NULL);

  dummy_label=gtk_label_new(" ");
  //gtk_widget_override_font(dummy_label, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(dummy_label);
  gtk_grid_attach(GTK_GRID(sliders),dummy_label,9,1,1,1);

  return sliders;
}
