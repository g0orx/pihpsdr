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
#include "about_menu.h"
#include "exit_menu.h"
#include "radio_menu.h"
#include "rx_menu.h"
#include "ant_menu.h"
#include "display_menu.h"
#include "dsp_menu.h"
#include "pa_menu.h"
#include "rigctl_menu.h"
#include "oc_menu.h"
#include "cw_menu.h"
#include "store_menu.h"
#include "xvtr_menu.h"
#include "equalizer_menu.h"
#include "radio.h"
#include "step_menu.h"
#include "meter_menu.h"
#include "band_menu.h"
#include "bandstack_menu.h"
#include "mode_menu.h"
#include "filter_menu.h"
#include "noise_menu.h"
#include "agc_menu.h"
#include "test_menu.h"
#include "vox_menu.h"
#include "diversity_menu.h"
#include "tx_menu.h"
#include "ps_menu.h"
#include "encoder_menu.h"
#include "switch_menu.h"
#include "toolbar_menu.h"
#include "vfo_menu.h"
#include "fft_menu.h"
#include "main.h"
#include "actions.h"
#include "gpio.h"
#include "old_protocol.h"
#include "new_protocol.h"
#ifdef CLIENT_SERVER
#include "server_menu.h"
#endif
#ifdef MIDI
#include "midi_menu.h"
#endif


static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

GtkWidget *sub_menu=NULL;

int active_menu=NO_MENU;

int menu_active_receiver_changed(void *data) {
  if(sub_menu!=NULL) {
    gtk_widget_destroy(sub_menu);
    sub_menu=NULL;
  }
  return FALSE;
}

static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
  }
  if(sub_menu!=NULL) {
    gtk_widget_destroy(sub_menu);
    sub_menu=NULL;
  }
  active_menu=NO_MENU;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

//
// The "Restart" button restarts the protocol
// This may help to recover from certain error conditions
// Hitting this button automatically closes the menu window via cleanup()
//
static gboolean restart_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      usleep(200000);
      old_protocol_run();
      break;
    case NEW_PROTOCOL:
      new_protocol_restart();
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      // dunno how to do this for soapy
      break;
#endif
  }
  return TRUE;
}

static gboolean about_b_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  about_menu(top_window);
  return TRUE;
}

static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  exit_menu(top_window);
  return TRUE;
}

static gboolean radio_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  radio_menu(top_window);
  return TRUE;
}

static gboolean rx_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  rx_menu(top_window);
  return TRUE;
}

static gboolean ant_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  ant_menu(top_window);
  return TRUE;
}

static gboolean display_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  display_menu(top_window);
  return TRUE;
}

static gboolean dsp_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  dsp_menu(top_window);
  return TRUE;
}

static gboolean pa_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  pa_menu(top_window);
  return TRUE;
}

static gboolean rigctl_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  rigctl_menu(top_window);
  return TRUE;
}

#ifdef GPIO
static gboolean encoder_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  encoder_menu(top_window);
  return TRUE;
}

static gboolean switch_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  switch_menu(top_window);
  return TRUE;
}
#endif

static gboolean toolbar_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  toolbar_menu(top_window);
  return TRUE;
}

static gboolean cw_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  cw_menu(top_window);
  return TRUE;
}

static gboolean oc_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  oc_menu(top_window);
  return TRUE;
}

static gboolean xvtr_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  xvtr_menu(top_window);
  return TRUE;
}

static gboolean equalizer_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  equalizer_menu(top_window);
  return TRUE;
}

void start_rx() {
  cleanup();
  rx_menu(top_window);
}

void start_step() {
  cleanup();
  step_menu(top_window);
}

static gboolean step_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_step();
  return TRUE;
}

void start_meter() {
  cleanup();
  meter_menu(top_window);
}

static gboolean meter_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_meter();
  return TRUE;
}

void start_band() {
  int old_menu=active_menu;
  cleanup();
  if(old_menu!=BAND_MENU) {
    band_menu(top_window);
    active_menu=BAND_MENU;
  }
}

static gboolean band_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_band();
  return TRUE;
}

void start_bandstack() {
  int old_menu=active_menu;
  cleanup();
  if(old_menu!=BANDSTACK_MENU) {
    bandstack_menu(top_window);
    active_menu=BANDSTACK_MENU;
  }
}

static gboolean bandstack_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_bandstack();
  return TRUE;
}

void start_mode() {
  int old_menu=active_menu;
  cleanup();
  if(old_menu!=MODE_MENU) {
    mode_menu(top_window);
    active_menu=MODE_MENU;
  }
}

static gboolean mode_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_mode();
  return TRUE;
}

void start_filter() {
  int old_menu=active_menu;
  cleanup();
  if(old_menu!=FILTER_MENU) {
    filter_menu(top_window);
    active_menu=FILTER_MENU;
  }
}

static gboolean filter_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_filter();
  return TRUE;
}

void start_noise() {
  int old_menu=active_menu;
  cleanup();
  if(old_menu!=NOISE_MENU) {
    noise_menu(top_window);
    active_menu=NOISE_MENU;
  }
}

static gboolean noise_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_noise();
  return TRUE;
}

void start_agc() {
  int old_menu=active_menu;
  cleanup();
  if(old_menu!=AGC_MENU) {
    agc_menu(top_window);
    active_menu=AGC_MENU;
  }
}

static gboolean agc_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_agc();
  return TRUE;
}

void start_vox() {
  cleanup();
  vox_menu(top_window);
}

static gboolean vox_b_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_vox();
  return TRUE;
}

void start_fft() {
  cleanup();
  fft_menu(top_window);
}

static gboolean fft_b_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_fft();
  return TRUE;
}

void start_diversity() {
  cleanup();
  diversity_menu(top_window);
}

static gboolean diversity_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_diversity();
  return TRUE;
}

void start_vfo(int vfo) {
  int old_menu=active_menu;
  cleanup();
  if(old_menu!=VFO_MENU) {
    vfo_menu(top_window,vfo);
    active_menu=VFO_MENU;
  }
}

static gboolean vfo_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_vfo(active_receiver->id);
  return TRUE;
}

void start_store() {
  int old_menu=active_menu;
  cleanup();
  if (old_menu != STORE_MENU) {
    store_menu(top_window);
    active_menu=STORE_MENU;
  }
}

static gboolean store_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_store();
  return TRUE;
}

void start_tx() {
  cleanup();
  tx_menu(top_window);
}

static gboolean tx_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_tx();
  return TRUE;
}

#ifdef PURESIGNAL
void start_ps() {
  cleanup();
  ps_menu(top_window);
}

static gboolean ps_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_ps();
  return TRUE;
}
#endif

void start_test() {
  cleanup();
  test_menu(top_window);
}

static gboolean test_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_test();
  return TRUE;
}

#ifdef CLIENT_SERVER
void start_server() {
  cleanup();
  server_menu(top_window);
}

static gboolean server_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_server();
  return TRUE;
}
#endif

#ifdef MIDI
void start_midi() {
  cleanup();
  midi_menu(top_window);
}

static gboolean midi_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_midi();
  return TRUE;
}
#endif

void new_menu()
{
  int i;

  if(dialog==NULL) {

    if(sub_menu!=NULL) {
      gtk_widget_destroy(sub_menu);
      sub_menu=NULL;
    }

    dialog=gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(top_window));
    //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
    gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Menu");
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
    gtk_grid_set_row_spacing (GTK_GRID(grid),10);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  
    GtkWidget *close_b=gtk_button_new_with_label("Close");
    g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),close_b,0,0,2,1);

    //
    // The "Restart" restarts the protocol
    // This may help to recover from certain error conditions
    // At the moment I do not know how to do this for SOAPY
    //
    if (protocol != SOAPYSDR_PROTOCOL) {
      GtkWidget *restart_b=gtk_button_new_with_label("Restart");
      g_signal_connect (restart_b, "button-press-event", G_CALLBACK(restart_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),restart_b,2,0,2,1);
    }

    GtkWidget *exit_b=gtk_button_new_with_label("Exit piHPSDR");
    g_signal_connect (exit_b, "button-press-event", G_CALLBACK(exit_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),exit_b,4,0,2,1);

    i=5;

    GtkWidget *radio_b=gtk_button_new_with_label("Radio");
    g_signal_connect (radio_b, "button-press-event", G_CALLBACK(radio_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),radio_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *rx_b=gtk_button_new_with_label("RX");
    g_signal_connect (rx_b, "button-press-event", G_CALLBACK(rx_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),rx_b,(i%5),i/5,1,1);
    i++;

    if(can_transmit) {
      GtkWidget *tx_b=gtk_button_new_with_label("TX");
      g_signal_connect (tx_b, "button-press-event", G_CALLBACK(tx_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),tx_b,(i%5),i/5,1,1);
      i++;

#ifdef PURESIGNAL
      if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
        GtkWidget *ps_b=gtk_button_new_with_label("PS");
        g_signal_connect (ps_b, "button-press-event", G_CALLBACK(ps_cb), NULL);
        gtk_grid_attach(GTK_GRID(grid),ps_b,(i%5),i/5,1,1);
        i++;
      }
#endif

      GtkWidget *pa_b=gtk_button_new_with_label("PA");
      g_signal_connect (pa_b, "button-press-event", G_CALLBACK(pa_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),pa_b,(i%5),i/5,1,1);
      i++;

      GtkWidget *cw_b=gtk_button_new_with_label("CW");
      g_signal_connect (cw_b, "button-press-event", G_CALLBACK(cw_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),cw_b,(i%5),i/5,1,1);
      i++;
    }

    GtkWidget *ant_b=gtk_button_new_with_label("Ant");
    g_signal_connect (ant_b, "button-press-event", G_CALLBACK(ant_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),ant_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *dsp_b=gtk_button_new_with_label("DSP");
    g_signal_connect (dsp_b, "button-press-event", G_CALLBACK(dsp_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),dsp_b,(i%5),i/5,1,1);
    i++;

    if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
      GtkWidget *oc_b=gtk_button_new_with_label("OC");
      g_signal_connect (oc_b, "button-press-event", G_CALLBACK(oc_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),oc_b,(i%5),i/5,1,1);
      i++;
    }

    GtkWidget *display_b=gtk_button_new_with_label("Display");
    g_signal_connect (display_b, "button-press-event", G_CALLBACK(display_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),display_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *xvtr_b=gtk_button_new_with_label("XVTR");
    g_signal_connect (xvtr_b, "button-press-event", G_CALLBACK(xvtr_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),xvtr_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *equalizer_b=gtk_button_new_with_label("Equalizer");
    g_signal_connect (equalizer_b, "button-press-event", G_CALLBACK(equalizer_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),equalizer_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *step_b=gtk_button_new_with_label("Step");
    g_signal_connect (step_b, "button-press-event", G_CALLBACK(step_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),step_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *meter_b=gtk_button_new_with_label("Meter");
    g_signal_connect (meter_b, "button-press-event", G_CALLBACK(meter_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),meter_b,(i%5),i/5,1,1);
    i++;

    if(can_transmit) {
      GtkWidget *vox_b=gtk_button_new_with_label("VOX");
      g_signal_connect (vox_b, "button-press-event", G_CALLBACK(vox_b_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),vox_b,(i%5),i/5,1,1);
      i++;
    }

    GtkWidget *fft_b=gtk_button_new_with_label("FFT");
    g_signal_connect (fft_b, "button-press-event", G_CALLBACK(fft_b_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),fft_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *rigctl_b=gtk_button_new_with_label("RIGCTL");
    g_signal_connect (rigctl_b, "button-press-event", G_CALLBACK(rigctl_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),rigctl_b,(i%5),i/5,1,1);
    i++;

    switch(controller) {
      case NO_CONTROLLER:
        {
        GtkWidget *toolbar_b=gtk_button_new_with_label("Toolbar");
        g_signal_connect (toolbar_b, "button-press-event", G_CALLBACK(toolbar_cb), NULL);
        gtk_grid_attach(GTK_GRID(grid),toolbar_b,(i%5),i/5,1,1);
        i++;
        }
        break;
      case CONTROLLER1:
#ifdef GPIO
        {
        GtkWidget *encoders_b=gtk_button_new_with_label("Encoders");
        g_signal_connect (encoders_b, "button-press-event", G_CALLBACK(encoder_cb), NULL);
        gtk_grid_attach(GTK_GRID(grid),encoders_b,(i%5),i/5,1,1);
        i++;

        GtkWidget *switches_b=gtk_button_new_with_label("Switches");
        g_signal_connect (switches_b, "button-press-event", G_CALLBACK(switch_cb), NULL);
        gtk_grid_attach(GTK_GRID(grid),switches_b,(i%5),i/5,1,1);
        i++;
        }
#endif
        break;
      case CONTROLLER2_V1:
      case CONTROLLER2_V2:
        {
#ifdef GPIO
        GtkWidget *encoders_b=gtk_button_new_with_label("Encoders");
        g_signal_connect (encoders_b, "button-press-event", G_CALLBACK(encoder_cb), NULL);
        gtk_grid_attach(GTK_GRID(grid),encoders_b,(i%5),i/5,1,1);
        i++;

        GtkWidget *switches_b=gtk_button_new_with_label("Switches");
        g_signal_connect (switches_b, "button-press-event", G_CALLBACK(switch_cb), NULL);
        gtk_grid_attach(GTK_GRID(grid),switches_b,(i%5),i/5,1,1);
        i++;
#endif
        GtkWidget *toolbar_b=gtk_button_new_with_label("Toolbar");
        g_signal_connect (toolbar_b, "button-press-event", G_CALLBACK(toolbar_cb), NULL);
        gtk_grid_attach(GTK_GRID(grid),toolbar_b,(i%5),i/5,1,1);
        i++;
        }
        break;
    }

#ifdef MIDI
    GtkWidget *midi_b=gtk_button_new_with_label("MIDI");
    g_signal_connect (midi_b, "button-press-event", G_CALLBACK(midi_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),midi_b,(i%5),i/5,1,1);
    i++;
#endif

//
//  We need at least two receivers and two ADCs to do DIVERSITY
//
    if(RECEIVERS==2 && n_adc > 1) {
      GtkWidget *diversity_b=gtk_button_new_with_label("Diversity");
      g_signal_connect (diversity_b, "button-press-event", G_CALLBACK(diversity_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),diversity_b,(i%5),i/5,1,1);
      i++;
    }

#ifdef CLIENT_SERVER
    GtkWidget *server_b=gtk_button_new_with_label("Server");
    g_signal_connect (server_b, "button-press-event", G_CALLBACK(server_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),server_b,(i%5),i/5,1,1);
    i++;
#endif

    GtkWidget *about_b=gtk_button_new_with_label("About");
    g_signal_connect (about_b, "button-press-event", G_CALLBACK(about_b_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),about_b,(i%5),i/5,1,1);
    i++;

    gtk_container_add(GTK_CONTAINER(content),grid);

    gtk_widget_show_all(dialog);

  } else {
    gtk_widget_destroy(dialog);
    dialog=NULL;
  }

}
