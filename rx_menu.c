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

#include "audio.h"
#include "new_menu.h"
#include "rx_menu.h"
#include "band.h"
#include "discovered.h"
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

static void dither_cb(GtkWidget *widget, gpointer data) {
  active_receiver->dither=active_receiver->dither==1?0:1;
}

static void random_cb(GtkWidget *widget, gpointer data) {
  active_receiver->random=active_receiver->random==1?0:1;
}

static void preamp_cb(GtkWidget *widget, gpointer data) {
  active_receiver->preamp=active_receiver->preamp==1?0:1;
}

static void sample_rate_cb(GtkWidget *widget, gpointer data) {
  receiver_change_sample_rate(active_receiver,(int)data);
}

static void adc_cb(GtkWidget *widget, gpointer data) {
  receiver_change_adc(active_receiver,(int)data);
}

static void local_audio_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if(audio_open_output(active_receiver)==0) {
      active_receiver->local_audio=1;
    } else {
      active_receiver->local_audio=0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
    }
  } else {
    if(active_receiver->local_audio) {
      active_receiver->local_audio=0;
      audio_close_output(active_receiver);
    }
  }
}

static void mute_audio_cb(GtkWidget *widget, gpointer data) {
  active_receiver->mute_when_not_active=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void local_output_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->audio_device=(int)(long)data;
  if(active_receiver->local_audio) {
    audio_close_output(active_receiver);
    if(audio_open_output(active_receiver)==0) {
      active_receiver->local_audio=1;
    }
  }
}

static void audio_channel_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    active_receiver->audio_channel=(int)data;
  }
}

void rx_menu(GtkWidget *parent) {
  char label[32];
  GtkWidget *adc_b;
  int i;
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

  GtkWidget *close_b=gtk_button_new_with_label("Close RX");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  sprintf(label,"RX %d VFO %s",active_receiver->id,active_receiver->id==0?"A":"B");
  GtkWidget *rx_label=gtk_label_new(label);
  gtk_grid_attach(GTK_GRID(grid),rx_label,1,0,1,1);

  int x=0;

  switch(protocol) {
#ifdef RADIOBERRY
	case RADIOBERRY_PROTOCOL:
#endif
    case NEW_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new("Sample Rate");
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

#ifndef GPIO
      GtkWidget *sample_rate_768=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_384),"768000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_768), active_receiver->sample_rate==768000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_768,x,6,1,1);
      g_signal_connect(sample_rate_768,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)768000);

      GtkWidget *sample_rate_1536=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_768),"1536000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1536), active_receiver->sample_rate==1536000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_1536,x,7,1,1);
      g_signal_connect(sample_rate_1536,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1536000);
#endif
      }
      x++;
      break;

#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new("Sample Rate");
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,x,1,1,1);

      GtkWidget *sample_rate_1M=gtk_radio_button_new_with_label(NULL,"1000000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_1M), active_receiver->sample_rate==1000000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_1M,x,2,1,1);
      g_signal_connect(sample_rate_1M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)1000000);

      GtkWidget *sample_rate_2M=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate_1M),"2000000");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate_2M), active_receiver->sample_rate==2000000);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_2M,x,3,1,1);
      g_signal_connect(sample_rate_2M,"pressed",G_CALLBACK(sample_rate_cb),(gpointer *)2000000);
      }
      x++;
      break;
#endif
  }

  switch(protocol) {
#ifdef RADIOBERRY
	case RADIOBERRY_PROTOCOL:
#endif
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL: 
      {
      GtkWidget *dither_b=gtk_check_button_new_with_label("Dither");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dither_b), active_receiver->dither);
      gtk_grid_attach(GTK_GRID(grid),dither_b,x,2,1,1);
      g_signal_connect(dither_b,"toggled",G_CALLBACK(dither_cb),NULL);

      GtkWidget *random_b=gtk_check_button_new_with_label("Random");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (random_b), active_receiver->random);
      gtk_grid_attach(GTK_GRID(grid),random_b,x,3,1,1);
      g_signal_connect(random_b,"toggled",G_CALLBACK(random_cb),NULL);

      if((protocol==ORIGINAL_PROTOCOL && device==DEVICE_METIS) ||
#ifdef USBOZY
         (protocol==ORIGINAL_PROTOCOL && device==DEVICE_OZY) ||
#endif
         (protocol==NEW_PROTOCOL && device==NEW_DEVICE_ATLAS)) {

        GtkWidget *preamp_b=gtk_check_button_new_with_label("Preamp");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preamp_b), active_receiver->preamp);
        gtk_grid_attach(GTK_GRID(grid),preamp_b,x,4,1,1);
        g_signal_connect(preamp_b,"toggled",G_CALLBACK(preamp_cb),NULL);
      }
      }
      x++;
      break;
    default:
      break;
  }

  int n_adc=1;
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      switch(device) {
        case DEVICE_METIS:
          n_adc=1;  // FIX for multiple Mercury cards
          break;
        case DEVICE_HERMES:
        case DEVICE_HERMES_LITE:
          n_adc=1;
          break;
        default: 
          n_adc=2;
          break;
      }
      break;
    case NEW_PROTOCOL:
      switch(device) {
        case NEW_DEVICE_ATLAS:
          n_adc=1; // FIX for multiple Mercury cards
          break;
        case NEW_DEVICE_HERMES:
        case NEW_DEVICE_HERMES2:
        case NEW_DEVICE_HERMES_LITE:
          n_adc=1;
          break;
        default:
          n_adc=2;
          break;
      }
      break;
    default:
      break;
  }

  if(n_adc>1) {
    for(i=0;i<n_adc;i++) {
      sprintf(label,"ADC-%d",i);
      if(i==0) {
        adc_b=gtk_radio_button_new_with_label(NULL,label);
      } else {
        adc_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(adc_b),label);
      }
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (adc_b), active_receiver->adc==i);
      gtk_grid_attach(GTK_GRID(grid),adc_b,x,2+i,1,1);
      g_signal_connect(adc_b,"pressed",G_CALLBACK(adc_cb),(gpointer *)i);
    }
    x++;
  }


  int row=0;
  if(n_output_devices>0) {
    GtkWidget *local_audio_b=gtk_check_button_new_with_label("Local Audio Output");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_audio_b), active_receiver->local_audio);
    gtk_widget_show(local_audio_b);
    gtk_grid_attach(GTK_GRID(grid),local_audio_b,x,++row,1,1);
    g_signal_connect(local_audio_b,"toggled",G_CALLBACK(local_audio_cb),NULL);

    if(active_receiver->audio_device==-1) active_receiver->audio_device=0;

    for(i=0;i<n_output_devices;i++) {
      GtkWidget *output;
      if(i==0) {
        output=gtk_radio_button_new_with_label(NULL,output_devices[i]);
      } else {
        output=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(output),output_devices[i]);
      }
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (output), active_receiver->audio_device==i);
      gtk_widget_show(output);
      gtk_grid_attach(GTK_GRID(grid),output,x,++row,1,1);
      g_signal_connect(output,"pressed",G_CALLBACK(local_output_changed_cb),(gpointer *)i);
    }

    row=0;
    x++;

    GtkWidget *stereo_b=gtk_radio_button_new_with_label(NULL,"Stereo");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (stereo_b), active_receiver->audio_channel==STEREO);
    gtk_widget_show(stereo_b);
    gtk_grid_attach(GTK_GRID(grid),stereo_b,x,++row,1,1);
    g_signal_connect(stereo_b,"toggled",G_CALLBACK(audio_channel_cb),(gpointer)STEREO);

    GtkWidget *left_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(stereo_b),"Left");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (left_b), active_receiver->audio_channel==LEFT);
    gtk_widget_show(left_b);
    gtk_grid_attach(GTK_GRID(grid),left_b,x,++row,1,1);
    g_signal_connect(left_b,"toggled",G_CALLBACK(audio_channel_cb),(gpointer)LEFT);

    GtkWidget *right_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(left_b),"Right");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (right_b), active_receiver->audio_channel==RIGHT);
    gtk_widget_show(right_b);
    gtk_grid_attach(GTK_GRID(grid),right_b,x,++row,1,1);
    g_signal_connect(right_b,"toggled",G_CALLBACK(audio_channel_cb),(gpointer)RIGHT);
  }

  GtkWidget *mute_audio_b=gtk_check_button_new_with_label("Mute when not active");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_audio_b), active_receiver->mute_when_not_active);
  gtk_widget_show(mute_audio_b);
  gtk_grid_attach(GTK_GRID(grid),mute_audio_b,x,++row,1,1);
  g_signal_connect(mute_audio_b,"toggled",G_CALLBACK(mute_audio_cb),NULL);
  
  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

