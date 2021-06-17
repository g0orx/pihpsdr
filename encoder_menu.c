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
#include "actions.h"
#include "gpio.h"
#include "i2c.h"

typedef struct _choice {
  int id;
  int action;
  GtkWidget *button;
} CHOICE;

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

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

static void encoder_bottom_select_cb(GtkWidget *widget,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  encoders[choice->id].bottom_encoder_function=choice->action;
  gtk_button_set_label(GTK_BUTTON(choice->button),ActionTable[choice->action].button_str);
}

static void encoder_top_select_cb(GtkWidget *widget,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  encoders[choice->id].top_encoder_function=choice->action;
  gtk_button_set_label(GTK_BUTTON(choice->button),ActionTable[choice->action].button_str);
}

static void encoder_switch_select_cb(GtkWidget *widget,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  encoders[choice->id].switch_function=choice->action;
  gtk_button_set_label(GTK_BUTTON(choice->button),ActionTable[choice->action].button_str);
}

static gboolean encoder_bottom_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int encoder=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<ACTIONS;i++) {
    if(ActionTable[i].type|CONTROLLER_ENCODER) {
      GtkWidget *menu_item=gtk_menu_item_new_with_label(ActionTable[i].str);
      CHOICE *choice=g_new0(CHOICE,1);
      choice->id=encoder;
      choice->action=i;
      choice->button=widget;
      g_signal_connect(menu_item,"activate",G_CALLBACK(encoder_bottom_select_cb),choice);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
    }
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

static gboolean encoder_top_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int encoder=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<ACTIONS;i++) {
    if(ActionTable[i].type|CONTROLLER_ENCODER) {
      GtkWidget *menu_item=gtk_menu_item_new_with_label(ActionTable[i].str);
      CHOICE *choice=g_new0(CHOICE,1);
      choice->id=encoder;
      choice->action=i;
      choice->button=widget;
      g_signal_connect(menu_item,"activate",G_CALLBACK(encoder_top_select_cb),choice);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
    }
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

static gboolean encoder_switch_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int encoder=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<ACTIONS;i++) {
    if(ActionTable[i].type|CONTROLLER_SWITCH) {
      GtkWidget *menu_item=gtk_menu_item_new_with_label(ActionTable[i].str);
      CHOICE *choice=g_new0(CHOICE,1);
      choice->id=encoder;
      choice->action=i;
      choice->button=widget;
      g_signal_connect(menu_item,"activate",G_CALLBACK(encoder_switch_select_cb),choice);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
    }
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


void encoder_menu(GtkWidget *parent) {
  gint row=0;
  gint col=0;
  char label[32];

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[32];
  sprintf(title,"piHPSDR - Encoder Actions");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  row++;
  col=0;

  gint max_encoders=MAX_ENCODERS;
  switch(controller) {
    case NO_CONTROLLER:
      max_encoders=0;
      break;
    case CONTROLLER1:
      max_encoders=4;
      break;
    case CONTROLLER2_V1:
      max_encoders=5;
      break;
    case CONTROLLER2_V2:
      max_encoders=5;
      break;
  }

  GtkWidget *widget=gtk_label_new("");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
  col++;

  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), controller==CONTROLLER2_V2?"<b>Bottom Encoder</b>":"<b>Encoder</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
  col++;

  if(controller==CONTROLLER2_V2) {
    widget=gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL(widget), "<b>Top Encoder</b>");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;
  }

  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), "<b>Switch</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);

  row++;
  col=0;

  for(int i=0;i<max_encoders;i++) {
    widget=gtk_label_new(NULL);
    g_sprintf(label,"<b>%d</b>",i);
    gtk_label_set_markup (GTK_LABEL(widget), label);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    if(i==(max_encoders-1)) {
      widget=gtk_label_new(NULL);
      g_sprintf(label,"<b>%s</b>",ActionTable[encoders[i].bottom_encoder_function].str);
      gtk_label_set_markup (GTK_LABEL(widget), label);
    } else {
      widget=gtk_button_new_with_label(ActionTable[encoders[i].bottom_encoder_function].str);
      g_signal_connect(widget,"button_press_event",G_CALLBACK(encoder_bottom_cb),GINT_TO_POINTER(i));
    }
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    if(controller==CONTROLLER2_V2) {
      widget=gtk_button_new_with_label(ActionTable[encoders[i].top_encoder_function].str);
      g_signal_connect(widget,"button_press_event",G_CALLBACK(encoder_top_cb),GINT_TO_POINTER(i));
      gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
      col++;
    }

    if(i!=(max_encoders-1)) {
      widget=gtk_button_new_with_label(ActionTable[encoders[i].switch_function].str);
      g_signal_connect(widget,"button_press_event",G_CALLBACK(encoder_switch_cb),GINT_TO_POINTER(i));
      gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
      col++;
    }
    
    row++;
    col=0;
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);
}
