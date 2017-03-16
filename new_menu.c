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
#include "exit_menu.h"
#include "radio_menu.h"
#include "rx_menu.h"
#include "ant_menu.h"
#include "display_menu.h"
#include "dsp_menu.h"
#include "pa_menu.h"
#include "oc_menu.h"
#ifdef FREEDV
#include "freedv_menu.h"
#endif
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
#include "freqent_menu.h"
#include "tx_menu.h"
#ifdef GPIO
#include "encoder_menu.h"
#endif
#include "vfo_menu.h"
#include "fft_menu.h"
#include "main.h"


static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

GtkWidget *sub_menu=NULL;

int active_menu=NO_MENU;

static cleanup() {
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

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
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

#ifdef FREEDV
static gboolean freedv_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  freedv_menu(top_window);
  return TRUE;
}
#endif

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

void start_freqent() {
  cleanup();
  freqent_menu(top_window);
}

static gboolean freqent_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_freqent();
  return TRUE;
}

void start_vfo() {
  cleanup();
  vfo_menu(top_window);
}

static gboolean vfo_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_vfo();
  return TRUE;
}

void start_store() {
  cleanup();
  store_menu(top_window);
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

#ifdef GPIO
void start_encoder(int encoder) {
  int old_menu=active_menu;
fprintf(stderr,"start_encoder: encoder=%d active_menu=%d\n",encoder,active_menu);
  cleanup();
  switch(encoder) {
    case 1:
      if(old_menu!=E1_MENU) {
        encoder_menu(top_window,encoder);
        active_menu=E1_MENU;
      }
      break;
    case 2:
      if(old_menu!=E2_MENU) {
        encoder_menu(top_window,encoder);
        active_menu=E2_MENU;
      }
      break;
    case 3:
      if(old_menu!=E3_MENU) {
        encoder_menu(top_window,encoder);
        active_menu=E3_MENU;
      }
      break;
  }
}

static gboolean encoder_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  int encoder=(int)data;
  start_encoder(encoder);
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

void new_menu()
{
  int i, j, id;

  if(dialog==NULL) {

    if(sub_menu!=NULL) {
      gtk_widget_destroy(sub_menu);
      sub_menu=NULL;
    }

    dialog=gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(top_window));
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
    gtk_grid_set_row_spacing (GTK_GRID(grid),10);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  
    GtkWidget *close_b=gtk_button_new_with_label("Close Menu");
    g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),close_b,0,0,2,1);

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

    GtkWidget *tx_b=gtk_button_new_with_label("TX");
    g_signal_connect (tx_b, "button-press-event", G_CALLBACK(tx_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),tx_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *pa_b=gtk_button_new_with_label("PA");
    g_signal_connect (pa_b, "button-press-event", G_CALLBACK(pa_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),pa_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *cw_b=gtk_button_new_with_label("CW");
    g_signal_connect (cw_b, "button-press-event", G_CALLBACK(cw_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),cw_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *ant_b=gtk_button_new_with_label("Ant");
    g_signal_connect (ant_b, "button-press-event", G_CALLBACK(ant_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),ant_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *dsp_b=gtk_button_new_with_label("DSP");
    g_signal_connect (dsp_b, "button-press-event", G_CALLBACK(dsp_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),dsp_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *oc_b=gtk_button_new_with_label("OC");
    g_signal_connect (oc_b, "button-press-event", G_CALLBACK(oc_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),oc_b,(i%5),i/5,1,1);
    i++;

#ifdef FREEDV
    GtkWidget *freedv_b=gtk_button_new_with_label("FreeDV");
    g_signal_connect (freedv_b, "button-press-event", G_CALLBACK(freedv_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),freedv_b,(i%5),i/5,1,1);
    i++;
#endif

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

    GtkWidget *vox_b=gtk_button_new_with_label("VOX");
    g_signal_connect (vox_b, "button-press-event", G_CALLBACK(vox_b_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),vox_b,(i%5),i/5,1,1);
    i++;

    GtkWidget *fft_b=gtk_button_new_with_label("FFT");
    g_signal_connect (fft_b, "button-press-event", G_CALLBACK(fft_b_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),fft_b,(i%5),i/5,1,1);
    i++;

#ifdef DIVERSITY
    if(RECEIVERS==2) {
      GtkWidget *diversity_b=gtk_button_new_with_label("Diversity");
      g_signal_connect (diversity_b, "button-press-event", G_CALLBACK(diversity_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),diversity_b,(i%5),i/5,1,1);
      i++;
    }
#endif

    gtk_container_add(GTK_CONTAINER(content),grid);

    gtk_widget_show_all(dialog);

  } else {
    gtk_widget_destroy(dialog);
    dialog=NULL;
  }

}
