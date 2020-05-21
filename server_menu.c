
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
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "new_menu.h"
#include "server_menu.h"
#include "radio.h"
#include "client_server.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

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

static void server_enable_cb(GtkWidget *widget, gpointer data) {
  hpsdr_server=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if(hpsdr_server) {
     create_hpsdr_server();
  } else {
     destroy_hpsdr_server();
  }
}

static void port_value_changed_cb(GtkWidget *widget, gpointer data) {
   listen_port = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
}

void server_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Server");
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

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);


  GtkWidget *server_enable_b=gtk_check_button_new_with_label("Server Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (server_enable_b), hpsdr_server);
  gtk_widget_show(server_enable_b);
  gtk_grid_attach(GTK_GRID(grid),server_enable_b,0,1,1,1);
  g_signal_connect(server_enable_b,"toggled",G_CALLBACK(server_enable_cb),NULL);
 
  GtkWidget *server_port_label =gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(server_port_label), "<b>Server Port</b>");
  gtk_widget_show(server_port_label);
  gtk_grid_attach(GTK_GRID(grid),server_port_label,0,2,1,1);

  GtkWidget *server_port_spinner =gtk_spin_button_new_with_range(45000,55000,1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(server_port_spinner),(double)listen_port);
  gtk_widget_show(server_port_spinner);
  gtk_grid_attach(GTK_GRID(grid),server_port_spinner,1,2,1,1);
  g_signal_connect(server_port_spinner,"value_changed",G_CALLBACK(port_value_changed_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

