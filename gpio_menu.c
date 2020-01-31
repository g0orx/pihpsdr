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
#include <gdk/gdk.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "main.h"
#include "channel.h"
#include "discovered.h"
#include "gpio_menu.h"
#include "gpio.h"


#ifdef GPIO

static GtkWidget *dialog;

static GtkWidget *b_enable_vfo_encoder;
static   GtkWidget *vfo_a_label;
static   GtkWidget *vfo_a;
static   GtkWidget *vfo_b_label;
static   GtkWidget *vfo_b;
static   GtkWidget *b_enable_vfo_pullup;
static GtkWidget *b_enable_E2_encoder;
static   GtkWidget *E2_a_label;
static   GtkWidget *E2_a;
static   GtkWidget *E2_b_label;
static   GtkWidget *E2_b;
static   GtkWidget *b_enable_E2_pullup;
static GtkWidget *b_enable_E3_encoder;
static   GtkWidget *E3_a_label;
static   GtkWidget *E3_a;
static   GtkWidget *E3_b_label;
static   GtkWidget *E3_b;
static   GtkWidget *b_enable_E3_pullup;
static GtkWidget *b_enable_E4_encoder;
static   GtkWidget *E4_a_label;
static   GtkWidget *E4_a;
static   GtkWidget *E4_b_label;
static   GtkWidget *E4_b;
static   GtkWidget *b_enable_E4_pullup;
static GtkWidget *b_enable_E5_encoder;
static   GtkWidget *E5_a_label;
static   GtkWidget *E5_a;
static   GtkWidget *E5_b_label;
static   GtkWidget *E5_b;
static   GtkWidget *b_enable_E5_pullup;
static GtkWidget *b_enable_mox;
static   GtkWidget *mox_label;
static   GtkWidget *mox;

static GtkWidget *b_enable_S1;
static   GtkWidget *S1_label;
static   GtkWidget *S1;

static GtkWidget *b_enable_S2;
static   GtkWidget *S2_label;
static   GtkWidget *S2;

static GtkWidget *b_enable_S3;
static   GtkWidget *S3_label;
static   GtkWidget *S3;

static GtkWidget *b_enable_S4;
static   GtkWidget *S4_label;
static   GtkWidget *S4;

static GtkWidget *b_enable_S5;
static   GtkWidget *S5_label;
static   GtkWidget *S5;

static GtkWidget *b_enable_S6;
static   GtkWidget *S6_label;
static   GtkWidget *S6;

static GtkWidget *b_enable_function;
static   GtkWidget *function_label;
static   GtkWidget *function;

#ifdef LOCALCW
static GtkWidget *cwl_label;
static GtkWidget *cwl;
static GtkWidget *cwr_label;
static GtkWidget *cwr;
static GtkWidget *cws_label;
static GtkWidget *cws;
static GtkWidget *b_enable_cws;
static GtkWidget *b_enable_cwlr;
static GtkWidget *b_cw_active_low;
#endif

static gboolean save_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gpio_save_state();
  gtk_widget_destroy(dialog);
}

static gboolean cancel_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
  }
  return TRUE;
}

static void controller_select_cb(GtkToggleButton *widget,gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    controller=GPOINTER_TO_INT(data);
    gpio_set_defaults(controller);
  }
}

/*
void configure_gpio(GtkWidget *parent) {

  gpio_restore_state();

  dialog=gtk_dialog_new_with_buttons("Configure GPIO",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  int row=0;
  int col=0;

  GtkWidget *b_no_controller=gtk_radio_button_new_with_label(NULL,"No Controller");
  gtk_grid_attach(GTK_GRID(grid),b_no_controller,col,row,1,1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_no_controller), controller==NO_CONTROLLER);
  g_signal_connect(b_no_controller,"toggled",G_CALLBACK(controller_select_cb),GINT_TO_POINTER(NO_CONTROLLER));
  row++;
  col=0;

  GtkWidget *b_controller1=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_no_controller),"Controller 1");
  gtk_grid_attach(GTK_GRID(grid),b_controller1,col,row,1,1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_controller1), controller==CONTROLLER1);
  g_signal_connect(b_controller1,"toggled",G_CALLBACK(controller_select_cb),GINT_TO_POINTER(CONTROLLER1));
  row++;
  col=0;

  GtkWidget *b_controller2v1=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_no_controller),"Controller 2 (Single Encoders)");
  gtk_grid_attach(GTK_GRID(grid),b_controller2v1,col,row,1,1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_controller2v1), controller==CONTROLLER2_V1);
  g_signal_connect(b_controller2v1,"toggled",G_CALLBACK(controller_select_cb),GINT_TO_POINTER(CONTROLLER2_V1));
  
  row++;
  col=0;

  GtkWidget *b_controller2v2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_no_controller),"Controller 2 (Dual Encoders)");
  gtk_grid_attach(GTK_GRID(grid),b_controller2v2,col,row,1,1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_controller2v2), controller==CONTROLLER2_V2);
  g_signal_connect(b_controller2v2,"toggled",G_CALLBACK(controller_select_cb),GINT_TO_POINTER(CONTROLLER2_V2));
  row++;
  col=0;

  GtkWidget *b_custom_controller=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_no_controller),"Custom Controller");
  gtk_grid_attach(GTK_GRID(grid),b_custom_controller,col,row,1,1);
  g_signal_connect(b_custom_controller,"toggled",G_CALLBACK(controller_select_cb),GINT_TO_POINTER(CUSTOM_CONTROLLER));
  row++;
  col=0;

  GtkWidget *save_b=gtk_button_new_with_label("Save");
  g_signal_connect (save_b, "button_press_event", G_CALLBACK(save_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),save_b,col,row,1,1);
  col++;

  GtkWidget *cancel_b=gtk_button_new_with_label("Cancel");
  g_signal_connect (cancel_b, "button_press_event", G_CALLBACK(cancel_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),cancel_b,col,row,1,1);

  gtk_container_add(GTK_CONTAINER(content),grid);

  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));

}
*/
#endif

