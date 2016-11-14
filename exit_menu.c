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
#include "radio.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
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
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_stop();
      break;
#endif
  }
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
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  system("reboot");
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
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  system("shutdown -h -P now");
  _exit(0);
}


void exit_menu(GtkWidget *parent) {
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
  gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *exit_b=gtk_button_new_with_label("Exit");
  g_signal_connect (exit_b, "pressed", G_CALLBACK(exit_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),exit_b,0,1,1,1);

  GtkWidget *reboot_b=gtk_button_new_with_label("Reboot");
  g_signal_connect (reboot_b, "pressed", G_CALLBACK(reboot_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),reboot_b,1,1,1,1);

  GtkWidget *shutdown_b=gtk_button_new_with_label("Shutdown");
  g_signal_connect (shutdown_b, "pressed", G_CALLBACK(shutdown_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),shutdown_b,2,1,1,1);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

