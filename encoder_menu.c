/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "new_menu.h"
#include "agc_menu.h"
#include "agc.h"
#include "band.h"
#include "channel.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "button_text.h"
#include "gpio.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *b_af_gain;
static GtkWidget *b_agc_gain;
static GtkWidget *b_attenuation;
static GtkWidget *b_mic_gain;
static GtkWidget *b_drive;
static GtkWidget *b_tune_drive;
static GtkWidget *b_rit;
static GtkWidget *b_cw_speed;
static GtkWidget *b_cw_frequency;
static GtkWidget *b_panadapter_high;
static GtkWidget *b_panadapter_low;

static int encoder;


static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  active_menu=NO_MENU;
  return TRUE;
}

void encoder_select(int pos) {
  int action;
  GtkWidget *button;
  switch(encoder) {
    case 1:
      if(pos>0) {
        e1_encoder_action--;
        if(e1_encoder_action<0) {
          e1_encoder_action=ENCODER_LAST;
        }
      } if(pos<0) {
        e1_encoder_action++;
        if(e1_encoder_action>ENCODER_LAST) {
          e1_encoder_action=0;
        }
      }
      action=e1_encoder_action;
      break;
    case 2:
      if(pos>0) {
        e2_encoder_action--;
        if(e2_encoder_action<0) {
          e2_encoder_action=ENCODER_LAST;
        }
      } if(pos<0) {
        e2_encoder_action++;
        if(e2_encoder_action>ENCODER_LAST) {
          e2_encoder_action=0;
        }
      }
      action=e2_encoder_action;
      break;
    case 3:
      if(pos>0) {
        e3_encoder_action--;
        if(e3_encoder_action<0) {
          e3_encoder_action=ENCODER_LAST;
        }
      } if(pos<0) {
        e3_encoder_action++;
        if(e3_encoder_action>ENCODER_LAST) {
          e3_encoder_action=0;
        }
      }
      action=e3_encoder_action;
      break;
  }

  switch(action) {
    case ENCODER_AF_GAIN:
      button=b_af_gain;
      break;
    case ENCODER_AGC_GAIN:
      button=b_agc_gain;
      break;
    case ENCODER_ATTENUATION:
      button=b_attenuation;
      break;
    case ENCODER_MIC_GAIN:
      button=b_mic_gain;
      break;
    case ENCODER_DRIVE:
      button=b_drive;
      break;
    case ENCODER_TUNE_DRIVE:
      button=b_tune_drive;
      break;
    case ENCODER_RIT:
      button=b_rit;
      break;
    case ENCODER_CW_SPEED:
      button=b_cw_speed;
      break;
    case ENCODER_CW_FREQUENCY:
      button=b_cw_frequency;
      break;
    case ENCODER_PANADAPTER_HIGH:
      button=b_panadapter_high;
      break;
    case ENCODER_PANADAPTER_LOW:
      button=b_panadapter_low;
      break;
  }
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

}

static gboolean action_select_cb (GtkWidget *widget, gpointer data) {
  int action=(int)data;
  switch(encoder) {
    case 1:
      e1_encoder_action=action;
      break;
    case 2:
      e2_encoder_action=action;
      break;
    case 3:
      e3_encoder_action=action;
      break;
  }
}

void encoder_menu(GtkWidget *parent,int e) {
  GtkWidget *b;
  int i;
  BAND *band;

  encoder=e;

  int encoder_action;
  switch(encoder) {
    case 1:
      encoder_action=e1_encoder_action;
      break;
    case 2:
      encoder_action=e2_encoder_action;
      break;
    case 3:
      encoder_action=e3_encoder_action;
      break;
  }
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

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  char label_text[32];
  sprintf(label_text,"Encoder E%d Action:",encoder);
  GtkWidget *label=gtk_label_new(label_text);
  gtk_grid_attach(GTK_GRID(grid),label,0,1,2,1);

  b_af_gain=gtk_radio_button_new_with_label(NULL,"AF Gain");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_af_gain), encoder_action==ENCODER_AF_GAIN);
  gtk_widget_show(b_af_gain);
  gtk_grid_attach(GTK_GRID(grid),b_af_gain,0,2,2,1);
  g_signal_connect(b_af_gain,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_AF_GAIN);

  b_agc_gain=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_af_gain),"AGC Gain");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_agc_gain), encoder_action==ENCODER_AGC_GAIN);
  gtk_widget_show(b_agc_gain);
  gtk_grid_attach(GTK_GRID(grid),b_agc_gain,0,3,2,1);
  g_signal_connect(b_agc_gain,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_AGC_GAIN);

  b_attenuation=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_agc_gain),"Attenuation");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_attenuation), encoder_action==ENCODER_ATTENUATION);
  gtk_widget_show(b_attenuation);
  gtk_grid_attach(GTK_GRID(grid),b_attenuation,0,4,2,1);
  g_signal_connect(b_attenuation,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_ATTENUATION);

  b_mic_gain=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_attenuation),"Mic Gain");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_mic_gain), encoder_action==ENCODER_MIC_GAIN);
  gtk_widget_show(b_mic_gain);
  gtk_grid_attach(GTK_GRID(grid),b_mic_gain,0,5,2,1);
  g_signal_connect(b_mic_gain,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_MIC_GAIN);

  b_drive=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_mic_gain),"Drive");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_drive), encoder_action==ENCODER_DRIVE);
  gtk_widget_show(b_drive);
  gtk_grid_attach(GTK_GRID(grid),b_drive,0,6,2,1);
  g_signal_connect(b_drive,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_DRIVE);

  b_tune_drive=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_drive),"Tune Drive");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_tune_drive), encoder_action==ENCODER_TUNE_DRIVE);
  gtk_widget_show(b_tune_drive);
  gtk_grid_attach(GTK_GRID(grid),b_tune_drive,0,7,2,1);
  g_signal_connect(b_tune_drive,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_TUNE_DRIVE);

  b_rit=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_tune_drive),"RIT");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_rit), encoder_action==ENCODER_RIT);
  gtk_widget_show(b_rit);
  gtk_grid_attach(GTK_GRID(grid),b_rit,2,2,2,1);
  g_signal_connect(b_rit,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_RIT);

  b_cw_speed=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_rit),"CW Speed");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_cw_speed), encoder_action==ENCODER_CW_SPEED);
  gtk_widget_show(b_cw_speed);
  gtk_grid_attach(GTK_GRID(grid),b_cw_speed,2,3,2,1);
  g_signal_connect(b_cw_speed,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_CW_SPEED);

  b_cw_frequency=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_cw_speed),"CW Freq");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_cw_frequency), encoder_action==ENCODER_CW_FREQUENCY);
  gtk_widget_show(b_cw_frequency);
  gtk_grid_attach(GTK_GRID(grid),b_cw_frequency,2,4,2,1);
  g_signal_connect(b_cw_frequency,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_CW_FREQUENCY);

  b_panadapter_high=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_cw_frequency),"Panadapter High");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_panadapter_high), encoder_action==ENCODER_PANADAPTER_HIGH);
  gtk_widget_show(b_panadapter_high);
  gtk_grid_attach(GTK_GRID(grid),b_panadapter_high,2,6,2,1);
  g_signal_connect(b_panadapter_high,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_PANADAPTER_HIGH);

  b_panadapter_low=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_cw_frequency),"Panadapter Low");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_panadapter_low), encoder_action==ENCODER_PANADAPTER_LOW);
  gtk_widget_show(b_panadapter_low);
  gtk_grid_attach(GTK_GRID(grid),b_panadapter_low,2,7,2,1);
  g_signal_connect(b_panadapter_low,"pressed",G_CALLBACK(action_select_cb),(gpointer *)ENCODER_PANADAPTER_LOW);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
