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

#include "main.h"
#include "new_menu.h"
#include "exit_menu.h"
#include "discovery.h"
#include "radio.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "actions.h"
#ifdef GPIO
#include "gpio.h"
#endif

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

static gboolean discovery_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
#ifdef GPIO
  gpio_close();
#endif
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      break;
    case NEW_PROTOCOL:
      new_protocol_stop();
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      soapy_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  radio_stop();
  gtk_container_remove(GTK_CONTAINER(top_window), fixed);
  gtk_widget_destroy(fixed);
  gtk_container_add(GTK_CONTAINER(top_window), grid);
  discovery();
  return TRUE;
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
g_print("exit_cb\n");
#ifdef GPIO
  gpio_close();
#endif
#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
    switch(protocol) {
      case ORIGINAL_PROTOCOL:
        old_protocol_stop();
        break;
      case NEW_PROTOCOL:
        new_protocol_stop();
        break;
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
        soapy_protocol_stop();
        break;
#endif
    }
#ifdef CLIENT_SERVER
  }
#endif
  radioSaveState();

  _exit(0);
}

static gboolean reboot_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
#ifdef GPIO
  gpio_close();
#endif
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      break;
    case NEW_PROTOCOL:
      new_protocol_stop();
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      soapy_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  int rc=system("reboot");
  _exit(0);
}

static gboolean shutdown_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
#ifdef GPIO
  gpio_close();
#endif
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      break;
    case NEW_PROTOCOL:
      new_protocol_stop();
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      soapy_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  int rc=system("shutdown -h -P now");
  _exit(0);
}


void exit_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Exit");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

#ifdef BKGND
  extern GdkRGBA bkgnd_color;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&bkgnd_color);
#else
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

  int row=0;
  int col=0;

  GtkWidget *close_b=gtk_button_new_with_label("Cancel");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  row++;
  col=0;

/*
  GtkWidget *discovery_b=gtk_button_new_with_label("Discovery");
  g_signal_connect (discovery_b, "pressed", G_CALLBACK(discovery_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),discovery_b,col,row,1,1);

  col++;
*/
  GtkWidget *exit_b=gtk_button_new_with_label("Exit");
  g_signal_connect (exit_b, "pressed", G_CALLBACK(exit_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),exit_b,col,row,1,1);

  col++;

  GtkWidget *reboot_b=gtk_button_new_with_label("Reboot");
  g_signal_connect (reboot_b, "pressed", G_CALLBACK(reboot_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),reboot_b,col,row,1,1);

  col++;

  GtkWidget *shutdown_b=gtk_button_new_with_label("Shutdown");
  g_signal_connect (shutdown_b, "pressed", G_CALLBACK(shutdown_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),shutdown_b,col,row,1,1);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

