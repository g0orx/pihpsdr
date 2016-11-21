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
#include "radio.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

GtkWidget *sub_menu=NULL;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
  }
  return TRUE;
}

static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  exit_menu(parent_window);
  return TRUE;
}

static gboolean general_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  general_menu(parent_window);
  return TRUE;
}

static gboolean audio_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  audio_menu(parent_window);
  return TRUE;
}

static gboolean ant_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  ant_menu(parent_window);
  return TRUE;
}

static gboolean display_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  display_menu(parent_window);
  return TRUE;
}

static gboolean dsp_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  dsp_menu(parent_window);
  return TRUE;
}

static gboolean pa_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  pa_menu(parent_window);
  return TRUE;
}

static gboolean cw_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  cw_menu(parent_window);
  return TRUE;
}

static gboolean oc_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  oc_menu(parent_window);
  return TRUE;
}

#ifdef FREEDV
static gboolean freedv_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  freedv_menu(parent_window);
  return TRUE;
}
#endif

static gboolean xvtr_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(dialog);
  dialog=NULL;
  xvtr_menu(parent_window);
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
    gtk_grid_attach(GTK_GRID(grid),exit_b,3,0,2,1);

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
    gtk_grid_attach(GTK_GRID(grid),pa_b,0,2,1,1);

    GtkWidget *cw_b=gtk_button_new_with_label("CW");
    g_signal_connect (cw_b, "button-press-event", G_CALLBACK(cw_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),cw_b,1,2,1,1);

    GtkWidget *oc_b=gtk_button_new_with_label("OC");
    g_signal_connect (oc_b, "button-press-event", G_CALLBACK(oc_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),oc_b,2,2,1,1);

#ifdef FREEDV
    GtkWidget *freedv_b=gtk_button_new_with_label("FreeDV");
    g_signal_connect (freedv_b, "button-press-event", G_CALLBACK(freedv_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),freedv_b,3,2,1,1);
#endif
/*
    GtkWidget *xvtr_b=gtk_button_new_with_label("XVTR");
    g_signal_connect (xvtr_b, "button-press-event", G_CALLBACK(xvtr_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),xvtr_b,4,2,1,1);
*/

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
