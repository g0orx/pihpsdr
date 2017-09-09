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

#include "radio.h"
#include "rit.h"
#include "vfo.h"

static GtkWidget *parent_window;

static GtkWidget *h_box;
static GtkWidget *v_box_1;
static GtkWidget *v_box_2;
static GtkWidget *ctun_b;
static GtkWidget *rit_b;
static GtkWidget *rit_plus_b;
static GtkWidget *rit_minus_b;
static gint rit_timer;

static gboolean rit_enabled=FALSE;

static void set_button_text_color(GtkWidget *widget,char *color) {
  GtkStyleContext *style_context;
  GtkCssProvider *provider = gtk_css_provider_new ();
  gchar tmp[64];
  style_context = gtk_widget_get_style_context(widget);
  gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_snprintf(tmp, sizeof tmp, "GtkButton, GtkLabel { color: %s; }", color);
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider), tmp, -1, NULL);
  g_object_unref (provider);
}

static gboolean ctun_pressed_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer        data) {
  ctun=ctun==1?0:1;
  
  if(ctun) {
    set_button_text_color(ctun_b,"red");
  } else {
    set_button_text_color(ctun_b,"black");
    vfo[active_receiver->id].offset=0;
    set_offset(active_receiver,0);
    vfo_update();
  }
  return TRUE;
}

static gboolean rit_timer_cb(gpointer data) {
  int v=active_receiver->id;
  if((GtkWidget*)data==rit_plus_b) {
    vfo[v].rit+=rit_increment;
  } else {
    vfo[v].rit-=rit_increment;
  }
  if(vfo[v].rit>1000) vfo[v].rit=1000;
  if(vfo[v].rit<-1000) vfo[v].rit=-1000;
  vfo_update();
  return TRUE;
}

static gboolean rit_pressed_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer        data) {
  if(rit_enabled) {
    rit_enabled=FALSE;
    set_button_text_color(rit_b,"black");
    vfo[active_receiver->id].offset=0;
    gtk_widget_set_sensitive(rit_plus_b,FALSE);
    gtk_widget_set_sensitive(rit_minus_b,FALSE);
  } else {
    rit_enabled=TRUE;
    set_button_text_color(rit_b,"red");
    gtk_widget_set_sensitive(rit_plus_b,TRUE);
    gtk_widget_set_sensitive(rit_minus_b,TRUE);
  }
  vfo_update();
}

static gboolean rit_step_pressed_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer        data) {
  int v=active_receiver->id;
  if(widget==rit_plus_b) {
    vfo[v].rit+=rit_increment;
  } else {
    vfo[v].rit-=rit_increment;
  }
  if(vfo[v].rit>1000) vfo[v].rit=1000;
  if(vfo[v].rit<-1000) vfo[v].rit=-1000;
  vfo_update();
  rit_timer=g_timeout_add(200,rit_timer_cb,widget);
  return TRUE;
}


static gboolean rit_step_released_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer        data) {
  g_source_remove(rit_timer);
  return TRUE;
}


GtkWidget* rit_init(int width,int height,GtkWidget *parent) {

  GdkRGBA black;
  black.red=0.0;
  black.green=0.0;
  black.blue=0.0;
  black.alpha=0.0;

  fprintf(stderr,"rit_init: width=%d height=%d\n",width,height);

  parent_window=parent;

  h_box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  gtk_widget_set_size_request (h_box, width, height);
  gtk_widget_override_background_color(h_box, GTK_STATE_NORMAL, &black);

  v_box_2=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
  gtk_widget_override_background_color(v_box_2, GTK_STATE_NORMAL, &black);

  rit_plus_b=gtk_button_new_with_label("RIT+");
  gtk_widget_override_font(rit_plus_b, pango_font_description_from_string("FreeMono Bold 10"));
  g_signal_connect (rit_plus_b, "pressed", G_CALLBACK(rit_step_pressed_event_cb), NULL);
  g_signal_connect (rit_plus_b, "released", G_CALLBACK(rit_step_released_event_cb), NULL);
  gtk_box_pack_start (GTK_BOX(v_box_2),rit_plus_b,TRUE,TRUE,0);

  rit_minus_b=gtk_button_new_with_label("RIT-");
  gtk_widget_override_font(rit_minus_b, pango_font_description_from_string("FreeMono Bold 10"));
  g_signal_connect (rit_minus_b, "pressed", G_CALLBACK(rit_step_pressed_event_cb), NULL);
  g_signal_connect (rit_minus_b, "released", G_CALLBACK(rit_step_released_event_cb), NULL);
  gtk_box_pack_start (GTK_BOX(v_box_2),rit_minus_b,TRUE,TRUE,0);
  
  gtk_widget_set_sensitive(rit_plus_b,FALSE);
  gtk_widget_set_sensitive(rit_minus_b,FALSE);

  gtk_box_pack_start (GTK_BOX(h_box),v_box_2,TRUE,TRUE,0);
  
  v_box_1=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
  gtk_widget_override_background_color(v_box_1, GTK_STATE_NORMAL, &black);

  ctun_b=gtk_button_new_with_label("CTUN");
  gtk_widget_override_font(ctun_b, pango_font_description_from_string("FreeMono Bold 10"));
  g_signal_connect (ctun_b, "pressed", G_CALLBACK(ctun_pressed_event_cb), NULL);
  gtk_box_pack_start (GTK_BOX(v_box_1),ctun_b,TRUE,TRUE,0);
  
  rit_b=gtk_button_new_with_label("RIT");
  gtk_widget_override_font(rit_b, pango_font_description_from_string("FreeMono Bold 10"));
  g_signal_connect (rit_b, "pressed", G_CALLBACK(rit_pressed_event_cb), NULL);
  gtk_box_pack_start (GTK_BOX(v_box_1),rit_b,TRUE,TRUE,0);
  
  gtk_box_pack_start (GTK_BOX(h_box),v_box_1,TRUE,TRUE,0);

  gtk_widget_show_all(h_box);

  return h_box;
}
