/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "new_menu.h"
#include "agc_menu.h"
#include "agc.h"
#include "band.h"
#include "channel.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "button_text.h"
#include "toolbar.h"
#include "actions.h"
#include "gpio.h"
#include "i2c.h"

typedef struct _choice {
  int sw;
  int action;
  GtkWidget *button;
} CHOICE;

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static SWITCH *temp_switches;


static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    active_menu=NO_MENU;
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

void switch_page_cb(GtkNotebook *notebook,GtkWidget *page,guint page_num,gpointer user_data) {
  g_print("%s: page %d\n",__FUNCTION__,page_num);
  temp_switches=switches_controller1[page_num];
}

static void switch_select_cb(GtkWidget *widget, gpointer data) {
  char text[128];
  CHOICE *choice=(CHOICE *)data;
  temp_switches[choice->sw].switch_function=choice->action;
  GtkWidget *label=gtk_bin_get_child(GTK_BIN(choice->button));
  sprintf(text,"<span size=\"smaller\">%s</span>",sw_string[choice->action]);
  gtk_label_set_markup (GTK_LABEL(label), text);
  update_toolbar_labels();
}

static gboolean switch_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int sw=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<SWITCH_ACTIONS;i++) {
    GtkWidget *menu_item=gtk_menu_item_new_with_label(sw_string[i]);
    CHOICE *choice=g_new0(CHOICE,1);
    choice->sw=sw;
    choice->action=i;
    choice->button=widget;
    g_signal_connect(menu_item,"activate",G_CALLBACK(switch_select_cb),choice);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
  }
  gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
  gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
// the following line of code is to work around the problem of the popup menu not having scroll bars.
  gtk_menu_reposition(GTK_MENU(menu));
#else
  gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
#endif
  return TRUE;
}

static void response_event(GtkWidget *dialog,gint id,gpointer user_data) {
  g_print("%s: id=%d\n",__FUNCTION__,id);
  if(id==GTK_RESPONSE_ACCEPT) {
    g_print("%s: ACCEPT\n",__FUNCTION__);
  }
  gtk_widget_destroy(dialog);
  dialog=NULL;
  active_menu=NO_MENU;
  sub_menu=NULL;
}

void switch_menu(GtkWidget *parent) {
  gint row;
  gint col;
  gchar label[64];
  GtkWidget *notebook;
  GtkWidget *grid;
  gint function=0;


  dialog=gtk_dialog_new_with_buttons("piHPSDR - Switch Actions",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,("OK"),GTK_RESPONSE_ACCEPT,NULL);
  g_signal_connect (dialog, "response", G_CALLBACK (response_event), NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  function=0;

  if(controller==NO_CONTROLLER || controller==CONTROLLER1) {
    notebook=gtk_notebook_new();
  }
 
next_function_set:

  grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),2);
  gtk_grid_set_row_spacing (GTK_GRID(grid),2);


  row=0;
  col=0;

  gint max_switches=MAX_SWITCHES;
  switch(controller) {
    case NO_CONTROLLER:
      max_switches=8;
      temp_switches=switches_controller1[function];
      break;
    case CONTROLLER1:
      max_switches=8;
      temp_switches=switches_controller1[function];
      break;
    case CONTROLLER2_V1:
      max_switches=16;
      temp_switches=switches_controller2_v1;
      break;
    case CONTROLLER2_V2:
      max_switches=16;
      temp_switches=switches_controller2_v2;
      break;
  }

  GtkWidget *widget=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(widget),"<b>Switch</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
  col++;

  widget=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(widget),"<b>Action</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);

  row++;
  col=0;

  for(int i=0;i<max_switches;i++) {
    widget=gtk_label_new(NULL);
    g_sprintf(label,"<b>%d</b>",i);
    gtk_label_set_markup (GTK_LABEL(widget), label);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    if((controller==NO_CONTROLLER || controller==CONTROLLER1) && (temp_switches[i].switch_function==FUNCTION)) {
      widget=gtk_label_new(NULL);
      g_sprintf(label,"<b>%s</b>",sw_string[temp_switches[i].switch_function]);
      gtk_label_set_markup (GTK_LABEL(widget), label);
    } else {
      widget=gtk_button_new_with_label(sw_string[temp_switches[i].switch_function]);
      g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(i));
    }
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);

    row++;
    col=0;
  }

  if(controller==NO_CONTROLLER || controller==CONTROLLER1) {
    g_sprintf(label,"Function %d",function);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid,gtk_label_new(label));
    function++;
    if(function<MAX_FUNCTIONS) {
      goto next_function_set;
    }
    gtk_container_add(GTK_CONTAINER(content),notebook);
    g_signal_connect (notebook, "switch-page",G_CALLBACK(switch_page_cb),NULL);
  } else {
    gtk_container_add(GTK_CONTAINER(content),grid);
  }

  sub_menu=dialog;

  gtk_widget_show_all(dialog);
}
