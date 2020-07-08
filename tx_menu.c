/*
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

#include "audio.h"
#include "new_menu.h"
#include "radio.h"
#include "sliders.h"
#include "transmitter.h"
#include "ext.h"
#include "filter.h"
#include "mode.h"
#include "vfo.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *last_filter;
static GtkWidget *input;
static GtkWidget *micin_b=NULL;
static GtkWidget *linein_b=NULL;
static GtkWidget *micboost_b=NULL;
static GtkWidget *tx_spin_low;
static GtkWidget *tx_spin_high;

static GtkWidget *tune_label;
static GtkWidget *tune_scale;

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

static void comp_enable_cb(GtkWidget *widget, gpointer data) {
  transmitter->compressor=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  g_idle_add(ext_vfo_update, NULL);
}

static void comp_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor_level(transmitter,gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)));
  g_idle_add(ext_vfo_update, NULL);
}

static void tx_spin_low_cb (GtkWidget *widget, gpointer data) {
  tx_filter_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  tx_set_filter(transmitter,tx_filter_low,tx_filter_high);
}

static void tx_spin_high_cb (GtkWidget *widget, gpointer data) {
  tx_filter_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  tx_set_filter(transmitter,tx_filter_low,tx_filter_high);
}

static void micboost_cb(GtkWidget *widget, gpointer data) {
  mic_boost=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void panadapter_high_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->panadapter_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_low_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->panadapter_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_step_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->panadapter_step=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void am_carrier_level_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->am_carrier_level=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  transmitter_set_am_carrier_level(transmitter);
}

static void ctcss_cb (GtkWidget *widget, gpointer data) {
  int state=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  transmitter_set_ctcss(transmitter,state,transmitter->ctcss);
}

static void ctcss_frequency_cb(GtkWidget *widget, gpointer data) {
  int i=gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  transmitter_set_ctcss(transmitter,transmitter->ctcss_enabled,i);
}

/*
static void ctcss_spin_cb (GtkWidget *widget, gpointer data) {
  double frequency=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  transmitter_set_ctcss(transmitter,transmitter->ctcss,frequency);
}
*/

static void tune_use_drive_cb (GtkWidget *widget, gpointer data) {
  transmitter->tune_use_drive=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void tune_percent_cb (GtkWidget *widget, gpointer data) {
  transmitter->tune_percent=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void use_rx_filter_cb(GtkWidget *widget, gpointer data) {
  transmitter->use_rx_filter=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  int filter_low,filter_high;

  if(transmitter->use_rx_filter) {
    int m=vfo[active_receiver->id].mode;
    if(m==modeFMN) {
      if(active_receiver->deviation==2500) {
        filter_low=-4000;
        filter_high=4000;
      } else {
        filter_low=-8000;
        filter_high=8000;
      }
    } else {
      FILTER *mode_filters=filters[m];
      FILTER *filter=&mode_filters[vfo[active_receiver->id].filter];
      filter_low=filter->low;
      filter_high=filter->high;
    }
  } else {
    filter_low=tx_filter_low;
    filter_high=tx_filter_high;
  }

  tx_set_filter(transmitter,filter_low,filter_high);

  if(transmitter->use_rx_filter) {
    gtk_widget_set_sensitive (tx_spin_low, FALSE);
    gtk_widget_set_sensitive (tx_spin_high, FALSE);
  } else {
    gtk_widget_set_sensitive (tx_spin_low, TRUE);
    gtk_widget_set_sensitive (tx_spin_high, TRUE);
  }
}

static void local_microphone_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if(transmitter->microphone_name==NULL) {
      int i=gtk_combo_box_get_active(GTK_COMBO_BOX(input));
      transmitter->microphone_name=g_new(gchar,strlen(input_devices[i].name)+1);
      strcpy(transmitter->microphone_name,input_devices[i].name);
    }
    if(audio_open_input()==0) {
      transmitter->local_microphone=1;
      if(micin_b!=NULL) gtk_widget_hide(micin_b);
      if(linein_b!=NULL) gtk_widget_hide(linein_b);
      if(micboost_b!=NULL) gtk_widget_hide(micboost_b);
    } else {
      transmitter->local_microphone=0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
      if(micin_b!=NULL) gtk_widget_show(micin_b);
      if(linein_b!=NULL) gtk_widget_show(linein_b);
      if(micboost_b!=NULL) gtk_widget_show(micboost_b);
    }
  } else {
    if(transmitter->local_microphone) {
      transmitter->local_microphone=0;
      audio_close_input();
      if(micin_b!=NULL) gtk_widget_show(micin_b);
      if(linein_b!=NULL) gtk_widget_show(linein_b);
      if(micboost_b!=NULL) gtk_widget_show(micboost_b);
    }
  }
}

static void micin_changed(GtkWidget *widget, gpointer data) {
  mic_linein=0;
  g_idle_add(ext_sliders_update,NULL);
}

static void linein_changed(GtkWidget *widget, gpointer data) {
  mic_linein=1;
  g_idle_add(ext_sliders_update,NULL);
}

static void local_input_changed_cb(GtkWidget *widget, gpointer data) {
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
g_print("local_input_changed_cp: %d %s\n",i,input_devices[i].name);
  if(transmitter->local_microphone) {
    audio_close_input();
  }

  if(transmitter->microphone_name!=NULL) {
    g_free(transmitter->microphone_name);
  }

  transmitter->microphone_name=g_new(gchar,strlen(input_devices[i].name)+1);
  strcpy(transmitter->microphone_name,input_devices[i].name);

  if(transmitter->local_microphone) {
    if(audio_open_input()<0) {
      transmitter->local_microphone=0;
    }
  }
}

static gboolean emp_cb (GtkWidget *widget, gpointer data) {
  pre_emphasize=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  tx_set_pre_emphasize(transmitter,pre_emphasize);
  return FALSE;
}

static void tune_value_changed_cb(GtkWidget *widget, gpointer data) {
  setTuneDrive(gtk_range_get_value(GTK_RANGE(tune_scale)));
}

void tx_menu(GtkWidget *parent) {
  int i;
  char temp[32];

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Transmit");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();

  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  int row=0;
  int col=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  row++;
  col=0;

  if(n_input_devices>0) {
    GtkWidget *local_microphone_b=gtk_check_button_new_with_label("Local Microphone");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_microphone_b), transmitter->local_microphone);
    gtk_widget_show(local_microphone_b);
    gtk_grid_attach(GTK_GRID(grid),local_microphone_b,col,row,1,1);
    g_signal_connect(local_microphone_b,"toggled",G_CALLBACK(local_microphone_cb),NULL);
    col++;

    input=gtk_combo_box_text_new();
    for(i=0;i<n_input_devices;i++) {
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(input),NULL,input_devices[i].name);
      if(transmitter->microphone_name!=NULL) {
        if(strcmp(transmitter->microphone_name,input_devices[i].name)==0) {
          gtk_combo_box_set_active(GTK_COMBO_BOX(input),i);
        }
      }
    }

    // If the combo box shows no device, take the first one
    i=gtk_combo_box_get_active(GTK_COMBO_BOX(input));
    if (i < 0) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(input),0);
    }

    gtk_grid_attach(GTK_GRID(grid),input,col,row,3,1);
    g_signal_connect(input,"changed",G_CALLBACK(local_input_changed_cb),NULL);
  }

  row++;
  col=0;

  gboolean device_has_microphone_input=FALSE;
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      switch(device) {
        case DEVICE_METIS:
        case DEVICE_HERMES:
        case DEVICE_GRIFFIN:
        case DEVICE_ANGELIA:
        case DEVICE_ORION:
        case DEVICE_ORION2:
          device_has_microphone_input=TRUE;
          break;
      }
      break;
    case NEW_PROTOCOL:
      switch(device) {
        case NEW_DEVICE_ATLAS:
        case NEW_DEVICE_HERMES:
        case NEW_DEVICE_HERMES2:
        case NEW_DEVICE_ANGELIA:
        case NEW_DEVICE_ORION:
        case NEW_DEVICE_ORION2:
          device_has_microphone_input=TRUE;
          break;
      }
      break;
  }

  if(device_has_microphone_input) {
    micboost_b=gtk_check_button_new_with_label("Radio Mic Boost");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (micboost_b), mic_boost);
    gtk_grid_attach(GTK_GRID(grid),micboost_b,col,row,1,1);
    g_signal_connect(micboost_b,"toggled",G_CALLBACK(micboost_cb),NULL);

    col++;

    micin_b=gtk_radio_button_new_with_label_from_widget(NULL,"Radio Mic In");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (micin_b), mic_linein==0);
    gtk_widget_show(micin_b);
    gtk_grid_attach(GTK_GRID(grid),micin_b,col,row,1,1);
    g_signal_connect(micin_b,"toggled",G_CALLBACK(micin_changed),NULL);

    col++;

    linein_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(micin_b),"Radio Line In");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (linein_b), mic_linein==1);
    gtk_widget_show(linein_b);
    gtk_grid_attach(GTK_GRID(grid),linein_b,col,row,1,1);
    g_signal_connect(linein_b,"toggled",G_CALLBACK(linein_changed),NULL);

    row++;
    col=0;

  }


  GtkWidget *comp_enable=gtk_check_button_new_with_label("Compression (dB):");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (comp_enable), transmitter->compressor);
  gtk_grid_attach(GTK_GRID(grid),comp_enable,col,row,1,1);
  g_signal_connect(comp_enable,"toggled",G_CALLBACK(comp_enable_cb),NULL);

  col++;

  GtkWidget *comp=gtk_spin_button_new_with_range(0.0,20.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(comp),(double)transmitter->compressor_level);
  gtk_grid_attach(GTK_GRID(grid),comp,col,row,1,1);
  g_signal_connect(comp,"value-changed",G_CALLBACK(comp_cb),NULL);


  row++;
  col=0;


  GtkWidget *use_rx_filter_b=gtk_check_button_new_with_label("Use RX Filter");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_rx_filter_b), transmitter->use_rx_filter);
  gtk_widget_show(use_rx_filter_b);
  gtk_grid_attach(GTK_GRID(grid),use_rx_filter_b,col,row,1,1);
  g_signal_connect(use_rx_filter_b,"toggled",G_CALLBACK(use_rx_filter_cb),NULL);

  col++;

  GtkWidget *label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>TX Filter: </b>");
#ifdef GTK316
  gtk_label_set_xalign(GTK_LABEL(label),0);
#endif
  gtk_grid_attach(GTK_GRID(grid),label,col,row,1,1);

  col++;
  tx_spin_low=gtk_spin_button_new_with_range(0.0,8000.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_spin_low),(double)tx_filter_low);
  gtk_grid_attach(GTK_GRID(grid),tx_spin_low,col,row,1,1);
  g_signal_connect(tx_spin_low,"value-changed",G_CALLBACK(tx_spin_low_cb),NULL);
  if(transmitter->use_rx_filter) {
    gtk_widget_set_sensitive (tx_spin_low, FALSE);
  }

  col++;

  tx_spin_high=gtk_spin_button_new_with_range(0.0,8000.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_spin_high),(double)tx_filter_high);
  gtk_grid_attach(GTK_GRID(grid),tx_spin_high,col,row,1,1);
  g_signal_connect(tx_spin_high,"value-changed",G_CALLBACK(tx_spin_high_cb),NULL);

  if(transmitter->use_rx_filter) {
    gtk_widget_set_sensitive (tx_spin_high, FALSE);
  }

  row++;
  col=0;

  GtkWidget *panadapter_high_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_high_label), "<b>Panadapter High: </b>");
#ifdef GTK316
  gtk_label_set_xalign(GTK_LABEL(panadapter_high_label),0);
#endif
  gtk_widget_show(panadapter_high_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_high_label,col,row,1,1);

  col++;

  GtkWidget *panadapter_low_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_low_label), "<b>Panadapter Low: </b>");
#ifdef GTK316
  gtk_label_set_xalign(GTK_LABEL(panadapter_low_label),0);
#endif
  gtk_widget_show(panadapter_low_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_low_label,col,row,1,1);

  col++;

  GtkWidget *panadapter_step_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_step_label), "<b>Panadapter Step: </b>");
#ifdef GTK316
  gtk_label_set_xalign(GTK_LABEL(panadapter_step_label),0);
#endif
  gtk_widget_show(panadapter_step_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_step_label,col,row,1,1);

  row++;
  col=0;

  GtkWidget *panadapter_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_high_r),(double)transmitter->panadapter_high);
  gtk_widget_show(panadapter_high_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_high_r,col,row,1,1);
  g_signal_connect(panadapter_high_r,"value_changed",G_CALLBACK(panadapter_high_value_changed_cb),NULL);

  col++;

  GtkWidget *panadapter_low_r=gtk_spin_button_new_with_range(-400.0,100.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_low_r),(double)transmitter->panadapter_low);
  gtk_widget_show(panadapter_low_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_low_r,col,row,1,1);
  g_signal_connect(panadapter_low_r,"value_changed",G_CALLBACK(panadapter_low_value_changed_cb),NULL);

  col++;

  GtkWidget *panadapter_step_r=gtk_spin_button_new_with_range(-400.0,100.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_step_r),(double)transmitter->panadapter_step);
  gtk_widget_show(panadapter_step_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_step_r,col,row,1,1);
  g_signal_connect(panadapter_step_r,"value_changed",G_CALLBACK(panadapter_step_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *am_carrier_level_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(am_carrier_level_label), "<b>AM Carrier Level:</b>");
#ifdef GTK316
  gtk_label_set_xalign(GTK_LABEL(am_carrier_level_label),0);
#endif
  gtk_widget_show(am_carrier_level_label);
  gtk_grid_attach(GTK_GRID(grid),am_carrier_level_label,col,row,1,1);

  col++;

  GtkWidget *am_carrier_level=gtk_spin_button_new_with_range(0.0,1.0,0.1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(am_carrier_level),(double)transmitter->am_carrier_level);
  gtk_widget_show(am_carrier_level);
  gtk_grid_attach(GTK_GRID(grid),am_carrier_level,col,row,1,1);
  g_signal_connect(am_carrier_level,"value_changed",G_CALLBACK(am_carrier_level_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *emp_b=gtk_check_button_new_with_label("FM TX Pre-emphasize before limiting");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (emp_b), pre_emphasize);
  gtk_widget_show(emp_b);
  gtk_grid_attach(GTK_GRID(grid),emp_b,col,row,2,1);
  g_signal_connect(emp_b,"toggled",G_CALLBACK(emp_cb),NULL);

  row++;
  col=0;

  GtkWidget *ctcss_b=gtk_check_button_new_with_label("CTCSS Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctcss_b), transmitter->ctcss_enabled);
  gtk_widget_show(ctcss_b);
  gtk_grid_attach(GTK_GRID(grid),ctcss_b,col,row,1,1);
  g_signal_connect(ctcss_b,"toggled",G_CALLBACK(ctcss_cb),NULL);


  col++;
  GtkWidget *ctcss_frequency_b=gtk_combo_box_text_new();
  for(i=0;i<CTCSS_FREQUENCIES;i++) {
    sprintf(temp,"%0.1f",ctcss_frequencies[i]);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ctcss_frequency_b),NULL,temp);
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(ctcss_frequency_b),transmitter->ctcss);
  gtk_grid_attach(GTK_GRID(grid),ctcss_frequency_b,col,row,1,1);
  g_signal_connect(ctcss_frequency_b,"changed",G_CALLBACK(ctcss_frequency_cb),NULL);

/*
  col++;
  GtkWidget *ctcss_spin=gtk_spin_button_new_with_range(67.0,250.3,0.1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctcss_spin),(double)transmitter->ctcss_frequency);
  gtk_grid_attach(GTK_GRID(grid),ctcss_spin,col,row,1,1);
  g_signal_connect(ctcss_spin,"value-changed",G_CALLBACK(ctcss_spin_cb),NULL);
*/  
  row++;
  col=0;

  GtkWidget *tune_use_drive_b=gtk_check_button_new_with_label("Tune use drive");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tune_use_drive_b), transmitter->tune_use_drive);
  gtk_widget_show(tune_use_drive_b);
  gtk_grid_attach(GTK_GRID(grid),tune_use_drive_b,col,row,1,1);
  g_signal_connect(tune_use_drive_b,"toggled",G_CALLBACK(tune_use_drive_cb),NULL);

  row++;
  col=0;
  
  GtkWidget *tune_percent_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(tune_percent_label), "<b>Tune Percent:</b>");
#ifdef GTK316
  gtk_label_set_xalign(GTK_LABEL(tune_percent_label),0);
#endif
  gtk_widget_show(tune_percent_label);
  gtk_grid_attach(GTK_GRID(grid),tune_percent_label,col,row,1,1);

  col++;
  GtkWidget *tune_percent=gtk_spin_button_new_with_range(1.0,100.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tune_percent),(double)transmitter->tune_percent);
  gtk_grid_attach(GTK_GRID(grid),tune_percent,col,row,1,1);
  g_signal_connect(tune_percent,"value-changed",G_CALLBACK(tune_percent_cb),NULL);


  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);


  if(transmitter->local_microphone && (protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL)) {
    gtk_widget_hide(linein_b);
    gtk_widget_hide(micboost_b);
  }

}
