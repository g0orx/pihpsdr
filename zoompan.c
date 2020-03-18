/* Copyright (C)
* 2020 - John Melton, G0ORX/N6LYT
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
#include <stdlib.h>
#include <math.h>

#include "main.h"
#include "receiver.h"
#include "radio.h"
#include "vfo.h"
#include "sliders.h"
#include "zoompan.h"

static int width;
static int height;

static GtkWidget *zoompan;
static GtkWidget *zoom_label;
static GtkWidget *zoom_scale;
static GtkWidget *pan_label;
static GtkWidget *pan_scale;

static GdkRGBA white;
static GdkRGBA gray;

int zoompan_active_receiver_changed(void *data) {
  if(display_zoompan) {
    gtk_range_set_value(GTK_RANGE(zoom_scale),active_receiver->zoom);
    gtk_range_set_range(GTK_RANGE(pan_scale),0.0,(double)(active_receiver->zoom==1?active_receiver->pixels:active_receiver->pixels-active_receiver->width));
    gtk_range_set_value (GTK_RANGE(pan_scale),active_receiver->pan);
    if(active_receiver->zoom == 1) {
      gtk_widget_set_sensitive(pan_scale, FALSE);
    }
  }
  return FALSE;
}

static void zoom_value_changed_cb(GtkWidget *widget, gpointer data) {
  g_mutex_lock(&active_receiver->display_mutex);
  receiver_change_zoom(active_receiver,gtk_range_get_value(GTK_RANGE(zoom_scale)));
  gtk_range_set_range(GTK_RANGE(pan_scale),0.0,(double)(active_receiver->zoom==1?active_receiver->pixels:active_receiver->pixels-active_receiver->width));
  gtk_range_set_value (GTK_RANGE(pan_scale),active_receiver->pan);
  if(active_receiver->zoom==1) {
    gtk_widget_set_sensitive(pan_scale, FALSE);
  } else {
    gtk_widget_set_sensitive(pan_scale, TRUE);
  }
  g_mutex_unlock(&active_receiver->display_mutex);
  vfo_update();
}

void set_zoom(int rx,double value) {
  receiver[rx]->zoom=value;
  receiver_change_zoom(active_receiver,value);
  if(display_zoompan) {
    gtk_range_set_value (GTK_RANGE(zoom_scale),receiver[rx]->zoom);
  } else {
    if(scale_status!=ZOOM || scale_rx!=rx) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=ZOOM;
      scale_rx=rx;
      char title[64];
      sprintf(title,"Zoom RX %d",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      zoom_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,1.0, MAX_ZOOM, 1.00);
      gtk_widget_set_size_request (zoom_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(zoom_scale),receiver[rx]->zoom);
      gtk_widget_show(zoom_scale);
      gtk_container_add(GTK_CONTAINER(content),zoom_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(zoom_scale),receiver[rx]->zoom);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
  vfo_update();
}

void update_zoom(double zoom) {
  int z=active_receiver->zoom+(int)zoom;
  if(z>MAX_ZOOM) z=MAX_ZOOM;
  if(z<1) z=1;
  set_zoom(active_receiver->id,z);
}

static void pan_value_changed_cb(GtkWidget *widget, gpointer data) {
  receiver_change_pan(active_receiver,gtk_range_get_value(GTK_RANGE(pan_scale)));
}

void set_pan(int rx,double value) {
  receiver[rx]->pan=(int)value;
  if(display_zoompan) {
    gtk_range_set_value (GTK_RANGE(pan_scale),receiver[rx]->pan);
  } else {
    if(scale_status!=PAN || scale_rx!=rx) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=PAN;
      scale_rx=rx;
      char title[64];
      sprintf(title,"Pan RX %d",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      pan_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, receiver[rx]->zoom==1?receiver[rx]->pixels:receiver[rx]->pixels-receiver[rx]->width, 1.00);
      gtk_widget_set_size_request (pan_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(pan_scale),receiver[rx]->pan);
      gtk_widget_show(pan_scale);
      gtk_container_add(GTK_CONTAINER(content),pan_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(pan_scale),receiver[rx]->pan);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void update_pan(double pan) {
  if(active_receiver->zoom>1) {
    int p=active_receiver->pan+(int)pan;
    if(p<0) p=0;
    if(p>(active_receiver->pixels-active_receiver->width)) p=active_receiver->pixels-active_receiver->width;
    set_pan(active_receiver->id,(double)p);
  }
}

GtkWidget *zoompan_init(int my_width, int my_height) {
  width=my_width;
  height=my_height;

fprintf(stderr,"zoompan_init: width=%d height=%d\n", width,height);

  zoompan=gtk_grid_new();
  gtk_widget_set_size_request (zoompan, width, height);
  gtk_grid_set_row_homogeneous(GTK_GRID(zoompan), FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(zoompan),TRUE);

  zoom_label=gtk_label_new("Zoom");
  gtk_widget_override_font(zoom_label, pango_font_description_from_string("Sans 10"));
  gtk_widget_show(zoom_label);
  gtk_grid_attach(GTK_GRID(zoompan),zoom_label,0,0,1,1);

  zoom_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,1.0,MAX_ZOOM,1.00);
  gtk_widget_override_font(zoom_scale, pango_font_description_from_string("Sans 10"));
  gtk_range_set_increments (GTK_RANGE(zoom_scale),1.0,1.0);
  gtk_range_set_value (GTK_RANGE(zoom_scale),active_receiver->zoom);
  gtk_widget_show(zoom_scale);
  gtk_grid_attach(GTK_GRID(zoompan),zoom_scale,1,0,2,1);
  g_signal_connect(G_OBJECT(zoom_scale),"value_changed",G_CALLBACK(zoom_value_changed_cb),NULL);

  pan_label=gtk_label_new("Pan:");
  gtk_widget_override_font(pan_label, pango_font_description_from_string("Sans 10"));
  gtk_widget_show(pan_label);
  gtk_grid_attach(GTK_GRID(zoompan),pan_label,3,0,1,1);

  pan_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0,active_receiver->zoom==1?active_receiver->pixels:active_receiver->pixels-active_receiver->width,1.0);
  gtk_widget_override_font(pan_scale, pango_font_description_from_string("Sans 10"));
  gtk_scale_set_draw_value (GTK_SCALE(pan_scale), FALSE);
  gtk_range_set_increments (GTK_RANGE(pan_scale),10.0,10.0);
  gtk_range_set_value (GTK_RANGE(pan_scale),active_receiver->pan);
  gtk_widget_show(pan_scale);
  gtk_grid_attach(GTK_GRID(zoompan),pan_scale,4,0,5,1);
  g_signal_connect(G_OBJECT(pan_scale),"value_changed",G_CALLBACK(pan_value_changed_cb),NULL);

  if(active_receiver->zoom == 1) {
    gtk_widget_set_sensitive(pan_scale, FALSE);
  }

  return zoompan;
}
