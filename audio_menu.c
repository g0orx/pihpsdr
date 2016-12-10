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

#include "new_menu.h"
#include "audio_menu.h"
#include "audio.h"
#include "radio.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

GtkWidget *linein_b;
GtkWidget *micboost_b;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static void micboost_cb(GtkWidget *widget, gpointer data) {
  mic_boost=mic_boost==1?0:1;
}

static void linein_cb(GtkWidget *widget, gpointer data) {
  mic_linein=mic_linein==1?0:1;
}

static void local_audio_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if(audio_open_output()==0) {
      local_audio=1;
    } else {
      local_audio=0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
    }
  } else {
    if(local_audio) {
      local_audio=0;
      audio_close_output();
    }
  }
}

static void local_output_changed_cb(GtkWidget *widget, gpointer data) {
  n_selected_output_device=(int)(long)data;

  if(local_audio) {
    audio_close_output();
    if(audio_open_output()==0) {
      local_audio=1;
    }
  }
}

static void local_microphone_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"local_microphone_cb: %d\n",local_microphone);
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if(audio_open_input()==0) {
      local_microphone=1;
      gtk_widget_hide(linein_b);
      gtk_widget_hide(micboost_b);
    } else {
      local_microphone=0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
      gtk_widget_show(linein_b);
      gtk_widget_show(micboost_b);
    }
  } else {
    if(local_microphone) {
      local_microphone=0;
      audio_close_input();
      gtk_widget_show(linein_b);
      gtk_widget_show(micboost_b);
    }
  }
}

static void local_input_changed_cb(GtkWidget *widget, gpointer data) {
  n_selected_input_device=(int)(long)data;
fprintf(stderr,"local_input_changed_cb: %d selected=%d\n",local_microphone,n_selected_input_device);
  if(local_microphone) {
    audio_close_input();
    if(audio_open_input()==0) {
      local_microphone=1;
    } else {
      local_microphone=0;
    }
  }
}

void audio_menu(GtkWidget *parent) {
  int i;

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close Audio");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int row=0;

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    linein_b=gtk_check_button_new_with_label("Mic Line In (ACC connector)");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (linein_b), mic_linein);
    gtk_grid_attach(GTK_GRID(grid),linein_b,0,++row,1,1);
    g_signal_connect(linein_b,"toggled",G_CALLBACK(linein_cb),NULL);

    micboost_b=gtk_check_button_new_with_label("Mic Boost (radio only)");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (micboost_b), mic_boost);
    gtk_grid_attach(GTK_GRID(grid),micboost_b,0,++row,1,1);
    g_signal_connect(micboost_b,"toggled",G_CALLBACK(micboost_cb),NULL);

  }



  if(n_input_devices>0) {
    GtkWidget *local_microphone_b=gtk_check_button_new_with_label("Local Microphone Input");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_microphone_b), local_microphone);
    gtk_widget_show(local_microphone_b);
    gtk_grid_attach(GTK_GRID(grid),local_microphone_b,0,++row,1,1);
    g_signal_connect(local_microphone_b,"toggled",G_CALLBACK(local_microphone_cb),NULL);

    if(n_selected_input_device==-1) n_selected_input_device=0;

    for(i=0;i<n_input_devices;i++) {
      GtkWidget *input;
      if(i==0) {
        input=gtk_radio_button_new_with_label(NULL,input_devices[i]);
      } else {
        input=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(input),input_devices[i]);
      }
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (input), n_selected_input_device==i);
      gtk_widget_show(input);
      gtk_grid_attach(GTK_GRID(grid),input,0,++row,1,1);
      g_signal_connect(input,"pressed",G_CALLBACK(local_input_changed_cb),(gpointer *)i);
    }
  }

  row=0;

  if(n_output_devices>0) {
    GtkWidget *local_audio_b=gtk_check_button_new_with_label("Local Audio Output");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_audio_b), local_audio);
    gtk_widget_show(local_audio_b);
    gtk_grid_attach(GTK_GRID(grid),local_audio_b,1,++row,1,1);
    g_signal_connect(local_audio_b,"toggled",G_CALLBACK(local_audio_cb),NULL);

    if(n_selected_output_device==-1) n_selected_output_device=0;

    for(i=0;i<n_output_devices;i++) {
      GtkWidget *output;
      if(i==0) {
        output=gtk_radio_button_new_with_label(NULL,output_devices[i]);
      } else {
        output=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(output),output_devices[i]);
      }
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (output), n_selected_output_device==i);
      gtk_widget_show(output);
      gtk_grid_attach(GTK_GRID(grid),output,1,++row,1,1);
      g_signal_connect(output,"pressed",G_CALLBACK(local_output_changed_cb),(gpointer *)i);
    }
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

  if(local_microphone && (protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL)) {
    gtk_widget_hide(linein_b);
    gtk_widget_hide(micboost_b);
  }
}

