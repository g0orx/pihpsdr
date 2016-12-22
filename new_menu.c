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
#include "general_menu.h"
#include "audio_menu.h"
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
#include "fm_menu.h"
#include "test_menu.h"
#include "vox_menu.h"
#include "diversity_menu.h"
#include "freqent_menu.h"


static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

GtkWidget *sub_menu=NULL;

static cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
  }
  if(sub_menu!=NULL) {
    gtk_widget_destroy(sub_menu);
    sub_menu=NULL;
  }
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}


static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  exit_menu(parent_window);
  return TRUE;
}

static gboolean general_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  general_menu(parent_window);
  return TRUE;
}

static gboolean audio_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  audio_menu(parent_window);
  return TRUE;
}

static gboolean ant_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  ant_menu(parent_window);
  return TRUE;
}

static gboolean display_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  display_menu(parent_window);
  return TRUE;
}

static gboolean dsp_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  dsp_menu(parent_window);
  return TRUE;
}

static gboolean pa_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  pa_menu(parent_window);
  return TRUE;
}

static gboolean cw_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  cw_menu(parent_window);
  return TRUE;
}

static gboolean oc_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  oc_menu(parent_window);
  return TRUE;
}

#ifdef FREEDV
static gboolean freedv_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  freedv_menu(parent_window);
  return TRUE;
}
#endif

static gboolean xvtr_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  xvtr_menu(parent_window);
  return TRUE;
}

static gboolean equalizer_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  equalizer_menu(parent_window);
  return TRUE;
}

void start_step() {
  cleanup();
  step_menu(parent_window);
}

static gboolean step_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_step();
  return TRUE;
}

void start_meter() {
  cleanup();
  meter_menu(parent_window);
}

static gboolean meter_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_meter();
  return TRUE;
}

void start_band() {
  cleanup();
  band_menu(parent_window);
}

static gboolean band_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_band();
  return TRUE;
}

void start_bandstack() {
  cleanup();
  bandstack_menu(parent_window);
}

static gboolean bandstack_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_bandstack();
  return TRUE;
}

void start_mode() {
  cleanup();
  mode_menu(parent_window);
}

static gboolean mode_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_mode();
  return TRUE;
}

void start_filter() {
  cleanup();
  filter_menu(parent_window);
}

static gboolean filter_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_filter();
  return TRUE;
}

void start_noise() {
  cleanup();
  noise_menu(parent_window);
}

static gboolean noise_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_noise();
  return TRUE;
}

void start_agc() {
  cleanup();
  agc_menu(parent_window);
}

static gboolean agc_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_agc();
  return TRUE;
}

void start_fm() {
  cleanup();
  fm_menu(parent_window);
}

static gboolean fm_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_fm();
  return TRUE;
}

void start_vox() {
  cleanup();
  vox_menu(parent_window);
}

static gboolean vox_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_vox();
  return TRUE;
}

void start_diversity() {
  cleanup();
  diversity_menu(parent_window);
}

static gboolean diversity_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_diversity();
  return TRUE;
}

void start_freqent() {
  cleanup();
  freqent_menu(parent_window);
}

static gboolean freqent_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_freqent();
  return TRUE;
}

void start_test() {
  cleanup();
  test_menu(parent_window);
}

static gboolean test_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  start_test();
  return TRUE;
}

static gboolean new_menu_pressed_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
  int i, j, id;


  if(dialog==NULL) {

    if(sub_menu!=NULL) {
      gtk_widget_destroy(sub_menu);
      sub_menu=NULL;
    }

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
    gtk_grid_set_row_spacing (GTK_GRID(grid),10);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  
    GtkWidget *close_b=gtk_button_new_with_label("Close Menu");
    g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),close_b,0,0,2,1);

    GtkWidget *exit_b=gtk_button_new_with_label("Exit piHPSDR");
    g_signal_connect (exit_b, "button-press-event", G_CALLBACK(exit_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),exit_b,4,0,2,1);

    GtkWidget *general_b=gtk_button_new_with_label("General");
    g_signal_connect (general_b, "button-press-event", G_CALLBACK(general_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),general_b,0,1,1,1);

    GtkWidget *audio_b=gtk_button_new_with_label("Audio");
    g_signal_connect (audio_b, "button-press-event", G_CALLBACK(audio_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),audio_b,1,1,1,1);

    GtkWidget *ant_b=gtk_button_new_with_label("Ant");
    g_signal_connect (ant_b, "button-press-event", G_CALLBACK(ant_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),ant_b,2,1,1,1);

    GtkWidget *display_b=gtk_button_new_with_label("Display");
    g_signal_connect (display_b, "button-press-event", G_CALLBACK(display_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),display_b,3,1,1,1);

    GtkWidget *dsp_b=gtk_button_new_with_label("DSP");
    g_signal_connect (dsp_b, "button-press-event", G_CALLBACK(dsp_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),dsp_b,4,1,1,1);

    GtkWidget *pa_b=gtk_button_new_with_label("PA");
    g_signal_connect (pa_b, "button-press-event", G_CALLBACK(pa_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),pa_b,5,1,1,1);

    GtkWidget *cw_b=gtk_button_new_with_label("CW");
    g_signal_connect (cw_b, "button-press-event", G_CALLBACK(cw_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),cw_b,0,2,1,1);

    GtkWidget *oc_b=gtk_button_new_with_label("OC");
    g_signal_connect (oc_b, "button-press-event", G_CALLBACK(oc_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),oc_b,1,2,1,1);

#ifdef FREEDV
    GtkWidget *freedv_b=gtk_button_new_with_label("FreeDV");
    g_signal_connect (freedv_b, "button-press-event", G_CALLBACK(freedv_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),freedv_b,2,2,1,1);
#endif

    GtkWidget *xvtr_b=gtk_button_new_with_label("XVTR");
    g_signal_connect (xvtr_b, "button-press-event", G_CALLBACK(xvtr_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),xvtr_b,3,2,1,1);

    GtkWidget *equalizer_b=gtk_button_new_with_label("Equalizer");
    g_signal_connect (equalizer_b, "button-press-event", G_CALLBACK(equalizer_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),equalizer_b,4,2,1,1);

    GtkWidget *fm_b=gtk_button_new_with_label("FM");
    g_signal_connect (fm_b, "button-press-event", G_CALLBACK(fm_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),fm_b,5,2,1,1);

    GtkWidget *step_b=gtk_button_new_with_label("Step");
    g_signal_connect (step_b, "button-press-event", G_CALLBACK(step_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),step_b,0,3,1,1);

    GtkWidget *meter_b=gtk_button_new_with_label("Meter");
    g_signal_connect (meter_b, "button-press-event", G_CALLBACK(meter_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),meter_b,1,3,1,1);

    GtkWidget *vox_b=gtk_button_new_with_label("VOX");
    g_signal_connect (vox_b, "button-press-event", G_CALLBACK(vox_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),vox_b,2,3,1,1);

    GtkWidget *frequency_b=gtk_button_new_with_label("Frequency");
    g_signal_connect (frequency_b, "button-press-event", G_CALLBACK(freqent_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),frequency_b,3,3,1,1);

    if(RECEIVERS==2) {
      GtkWidget *diversity_b=gtk_button_new_with_label("Diversity");
      g_signal_connect (diversity_b, "button-press-event", G_CALLBACK(diversity_cb), NULL);
      gtk_grid_attach(GTK_GRID(grid),diversity_b,4,3,1,1);
    }

    GtkWidget *band_b=gtk_button_new_with_label("Band");
    g_signal_connect (band_b, "button-press-event", G_CALLBACK(band_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),band_b,0,4,1,1);

    GtkWidget *bandstack_b=gtk_button_new_with_label("Band Stack");
    g_signal_connect (bandstack_b, "button-press-event", G_CALLBACK(bandstack_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),bandstack_b,1,4,1,1);

    GtkWidget *mode_b=gtk_button_new_with_label("Mode");
    g_signal_connect (mode_b, "button-press-event", G_CALLBACK(mode_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),mode_b,2,4,1,1);

    GtkWidget *filter_b=gtk_button_new_with_label("Filter");
    g_signal_connect (filter_b, "button-press-event", G_CALLBACK(filter_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),filter_b,3,4,1,1);

    GtkWidget *noise_b=gtk_button_new_with_label("Noise");
    g_signal_connect (noise_b, "button-press-event", G_CALLBACK(noise_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),noise_b,4,4,1,1);

    GtkWidget *agc_b=gtk_button_new_with_label("AGC");
    g_signal_connect (agc_b, "button-press-event", G_CALLBACK(agc_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),agc_b,5,4,1,1);

    GtkWidget *test_b=gtk_button_new_with_label("Test");
    g_signal_connect (test_b, "button-press-event", G_CALLBACK(test_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),test_b,0,5,1,1);

    gtk_container_add(GTK_CONTAINER(content),grid);

    gtk_widget_show_all(dialog);

  } else {
    gtk_widget_destroy(dialog);
    dialog=NULL;
  }

  return TRUE;
}

GtkWidget* new_menu_init(int width,int height,GtkWidget *parent) {

  parent_window=parent;

  menu_b=gtk_button_new_with_label("Menu");
  gtk_widget_override_font(menu_b, pango_font_description_from_string("FreeMono Bold 10"));
  gtk_widget_set_size_request (menu_b, width, height);
  g_signal_connect (menu_b, "button-press-event", G_CALLBACK(new_menu_pressed_event_cb), NULL);
  gtk_widget_show(menu_b);

  return menu_b;
}
