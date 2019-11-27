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
#include <stdint.h>
#include <string.h>

#include "audio.h"
#include "new_menu.h"
#include "rx_menu.h"
#include "band.h"
#include "discovered.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "sliders.h"
#include "new_protocol.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *local_audio_b=NULL;
static GtkWidget *output=NULL;

static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

static void dither_cb(GtkWidget *widget, gpointer data) {
  active_receiver->dither=active_receiver->dither==1?0:1;
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

static void random_cb(GtkWidget *widget, gpointer data) {
  active_receiver->random=active_receiver->random==1?0:1;
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

static void preamp_cb(GtkWidget *widget, gpointer data) {
  active_receiver->preamp=active_receiver->preamp==1?0:1;
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

static void alex_att_cb(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    set_alex_attenuation((intptr_t) data);
    update_att_preamp();
  }
}

static void sample_rate_cb(GtkWidget *widget, gpointer data) {
  receiver_change_sample_rate(active_receiver,GPOINTER_TO_INT(data));
}

static void adc_cb(GtkWidget *widget, gpointer data) {
  receiver_change_adc(active_receiver,GPOINTER_TO_INT(data));
}

static void local_audio_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"local_audio_cb: rx=%d\n",active_receiver->id);

  if(active_receiver->audio_name!=NULL) {
    g_free(active_receiver->audio_name);
    active_receiver->audio_name=NULL;
  }

  int i=gtk_combo_box_get_active(GTK_COMBO_BOX(output));
  active_receiver->audio_name=g_new(gchar,strlen(output_devices[i].name)+1);
  strcpy(active_receiver->audio_name,output_devices[i].name);

  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if(audio_open_output(active_receiver)==0) {
      active_receiver->local_audio=1;
    } else {
fprintf(stderr,"local_audio_cb: audio_open_output failed\n");
      active_receiver->local_audio=0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
    }
  } else {
    if(active_receiver->local_audio) {
      active_receiver->local_audio=0;
      audio_close_output(active_receiver);
    }
  }
fprintf(stderr,"local_audio_cb: local_audio=%d\n",active_receiver->local_audio);
}

static void mute_audio_cb(GtkWidget *widget, gpointer data) {
  active_receiver->mute_when_not_active=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void mute_radio_cb(GtkWidget *widget, gpointer data) {
  active_receiver->mute_radio=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

//
// possible the device has been changed:
// call audo_close_output with old device, audio_open_output with new one
//
static void local_output_changed_cb(GtkWidget *widget, gpointer data) {
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  fprintf(stderr,"local_output_changed rx=%d %s\n",active_receiver->id,output_devices[i].name);
  if(active_receiver->local_audio) {
    audio_close_output(active_receiver);                     // audio_close with OLD device
  }
    
  if(active_receiver->audio_name!=NULL) {
    g_free(active_receiver->audio_name);
    active_receiver->audio_name=NULL;
  }
  
  if(i>=0) {
    active_receiver->audio_name=g_new(gchar,strlen(output_devices[i].name)+1);
    strcpy(active_receiver->audio_name,output_devices[i].name);
    //active_receiver->audio_device=output_devices[i].index;  // update rx to NEW device
  }

  if(active_receiver->local_audio) {
    if(audio_open_output(active_receiver)<0) {              // audio_open with NEW device
      active_receiver->local_audio=0;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (local_audio_b),FALSE);
    }
  }
  fprintf(stderr,"local_output_changed rx=%d local_audio=%d\n",active_receiver->id,active_receiver->local_audio);
}

static void audio_channel_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    active_receiver->audio_channel=GPOINTER_TO_INT(data);
  }
}

void rx_menu(GtkWidget *parent) {
  char label[32];
  GtkWidget *adc_b;
  int i;
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - Receive (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int x=0;

  switch(protocol) {
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

#ifndef raspberrypi
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

#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      {
      int row=1;
      GtkWidget *sample_rate_label=gtk_label_new("Sample Rate");
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,x,row,1,1);
      row++;
      

      char rate_string[16];
      sprintf(rate_string,"%d",radio->info.soapy.sample_rate);
      GtkWidget *sample_rate=gtk_radio_button_new_with_label(NULL,rate_string);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sample_rate), radio->info.soapy.sample_rate);
      gtk_grid_attach(GTK_GRID(grid),sample_rate,x,row,1,1);
      g_signal_connect(sample_rate,"pressed",G_CALLBACK(sample_rate_cb),GINT_TO_POINTER(radio->info.soapy.sample_rate));
      row++;

      int rate=radio->info.soapy.sample_rate/2;
      while(rate>=48000) {
          sprintf(rate_string,"%d",rate);
          GtkWidget *next_sample_rate=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sample_rate),rate_string);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (next_sample_rate), active_receiver->sample_rate==rate);
          gtk_grid_attach(GTK_GRID(grid),next_sample_rate,x,row,1,1);
          g_signal_connect(next_sample_rate,"pressed",G_CALLBACK(sample_rate_cb),GINT_TO_POINTER(rate));
          rate=rate/2;
          row++;
      }
      }
      x++;
      break;
#endif

  }
 
  //
  // The CHARLY25 board (with RedPitaya) has no support for dither or random,
  // so those are left out. For Charly25, PreAmps and Alex Attenuator are controlled via
  // the sliders menu.
  //
  // Preamps are not switchable on all SDR hardware I know of, so this is commented out
  //
  // We assume Alex attenuators are present if we have an ALEX board and no Orion2
  // (ANAN-7000/8000 do not have these), and if the RX is fed by the first ADC.
  //
  if (filter_board != CHARLY25) {
      GtkWidget *dither_b=gtk_check_button_new_with_label("Dither");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dither_b), active_receiver->dither);
      gtk_grid_attach(GTK_GRID(grid),dither_b,x,2,1,1);
      g_signal_connect(dither_b,"toggled",G_CALLBACK(dither_cb),NULL);

      GtkWidget *random_b=gtk_check_button_new_with_label("Random");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (random_b), active_receiver->random);
      gtk_grid_attach(GTK_GRID(grid),random_b,x,3,1,1);
      g_signal_connect(random_b,"toggled",G_CALLBACK(random_cb),NULL);

      //GtkWidget *preamp_b=gtk_check_button_new_with_label("Preamp");
      //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preamp_b), active_receiver->preamp);
      //gtk_grid_attach(GTK_GRID(grid),preamp_b,x,4,1,1);
      //g_signal_connect(preamp_b,"toggled",G_CALLBACK(preamp_cb),NULL);

      if (filter_board == ALEX && active_receiver->adc == 0
          && ((protocol==ORIGINAL_PROTOCOL && device != DEVICE_ORION2) || (protocol==NEW_PROTOCOL && device != NEW_DEVICE_ORION2))) {
  
        GtkWidget *alex_att_label=gtk_label_new("Alex Attenuator");
        gtk_grid_attach(GTK_GRID(grid), alex_att_label, x, 5, 1, 1);
        GtkWidget *last_alex_att_b = NULL;
        for (i = 0; i <= 3; i++) {
          gchar button_text[] = "xx dB";
          sprintf(button_text, "%d dB", i*10);
          GtkWidget *alex_att_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(last_alex_att_b), button_text);
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alex_att_b), active_receiver->alex_attenuation == i);
          gtk_grid_attach(GTK_GRID(grid), alex_att_b, x, 6 + i, 1, 1);
          g_signal_connect(alex_att_b, "toggled", G_CALLBACK(alex_att_cb), GINT_TO_POINTER(i));
          last_alex_att_b = alex_att_b;
        }
    }
    x++;
  }

  // If there is more than one ADC, let the user associate an ADC
  // with the current receiver.
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
      g_signal_connect(adc_b,"pressed",G_CALLBACK(adc_cb),GINT_TO_POINTER(i));
    }
    x++;
  }


  int row=0;
  if(n_output_devices>0) {
    local_audio_b=gtk_check_button_new_with_label("Local Audio Output");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_audio_b), active_receiver->local_audio);
    gtk_widget_show(local_audio_b);
    gtk_grid_attach(GTK_GRID(grid),local_audio_b,x,++row,1,1);
    g_signal_connect(local_audio_b,"toggled",G_CALLBACK(local_audio_cb),NULL);

    if(active_receiver->audio_device==-1) active_receiver->audio_device=0;

    output=gtk_combo_box_text_new();
    for(i=0;i<n_output_devices;i++) {
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(output),NULL,output_devices[i].name);
      if(active_receiver->audio_name!=NULL) {
        if(strcmp(active_receiver->audio_name,output_devices[i].name)==0) {
          gtk_combo_box_set_active(GTK_COMBO_BOX(output),i);
        }
      }
    }
    gtk_grid_attach(GTK_GRID(grid),output,x,++row,1,1);
    g_signal_connect(output,"changed",G_CALLBACK(local_output_changed_cb),NULL);

    row=0;
    x++;

    GtkWidget *stereo_b=gtk_radio_button_new_with_label(NULL,"Stereo");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (stereo_b), active_receiver->audio_channel==STEREO);
    gtk_widget_show(stereo_b);
    gtk_grid_attach(GTK_GRID(grid),stereo_b,x,++row,1,1);
    g_signal_connect(stereo_b,"toggled",G_CALLBACK(audio_channel_cb),GINT_TO_POINTER(STEREO));

    GtkWidget *left_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(stereo_b),"Left");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (left_b), active_receiver->audio_channel==LEFT);
    gtk_widget_show(left_b);
    gtk_grid_attach(GTK_GRID(grid),left_b,x,++row,1,1);
    g_signal_connect(left_b,"toggled",G_CALLBACK(audio_channel_cb),GINT_TO_POINTER(LEFT));

    GtkWidget *right_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(left_b),"Right");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (right_b), active_receiver->audio_channel==RIGHT);
    gtk_widget_show(right_b);
    gtk_grid_attach(GTK_GRID(grid),right_b,x,++row,1,1);
    g_signal_connect(right_b,"toggled",G_CALLBACK(audio_channel_cb),GINT_TO_POINTER(RIGHT));
  }

  GtkWidget *mute_audio_b=gtk_check_button_new_with_label("Mute when not active");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_audio_b), active_receiver->mute_when_not_active);
  gtk_widget_show(mute_audio_b);
  gtk_grid_attach(GTK_GRID(grid),mute_audio_b,x,++row,1,1);
  g_signal_connect(mute_audio_b,"toggled",G_CALLBACK(mute_audio_cb),NULL);
  
  row++;

  GtkWidget *mute_radio_b=gtk_check_button_new_with_label("Mute audio to radio");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_radio_b), active_receiver->mute_radio);
  gtk_widget_show(mute_radio_b);
  gtk_grid_attach(GTK_GRID(grid),mute_radio_b,x,++row,1,1);
  g_signal_connect(mute_radio_b,"toggled",G_CALLBACK(mute_radio_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

