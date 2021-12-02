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
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "new_menu.h"
#include "rigctl_menu.h"
#include "rigctl.h"
#include "band.h"
#include "radio.h"
#include "vfo.h"

int  serial_enable;
char ser_port[64]="/dev/ttyACM0";
int serial_baud_rate = B9600;
int serial_parity = 0; // 0=none, 1=even, 2=odd
gboolean rigctl_debug=FALSE;

static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *serial_port_entry;

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

static void rigctl_value_changed_cb(GtkWidget *widget, gpointer data) {
   rigctl_port_base = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)); 
}

static void serial_value_changed_cb(GtkWidget *widget, gpointer data) {
     sprintf(ser_port,"/dev/ttyACM%0d",(int) gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget))); 
     fprintf(stderr,"RIGCTL_MENU: New Serial port=%s\n",ser_port);
}

static void rigctl_debug_cb(GtkWidget *widget, gpointer data) {
  rigctl_debug=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  g_print("---------- RIGCTL DEBUG %s ----------\n",rigctl_debug?"ON":"OFF");
}

static void rigctl_enable_cb(GtkWidget *widget, gpointer data) {
  rigctl_enable=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if(rigctl_enable) {
     launch_rigctl();
  } else {
     disable_rigctl();
  }
}

static void serial_enable_cb(GtkWidget *widget, gpointer data) {
  strcpy(ser_port,gtk_entry_get_text(GTK_ENTRY(serial_port_entry)));
  serial_enable=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if(serial_enable) {
     if(launch_serial() == 0) {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(widget),FALSE)	     ;
     }	      
  } else {
     disable_serial();
  }
}

// Set Baud Rate
static void baud_rate_cb(GtkWidget *widget, gpointer data) {
   serial_baud_rate = GPOINTER_TO_INT(data);
   fprintf(stderr,"RIGCTL_MENU: Baud rate changed: %d\n",serial_baud_rate);
}

// Set Parity 0=None, 1=Even, 2=0dd
static void parity_cb(GtkWidget *widget, gpointer data) {
   serial_parity = GPOINTER_TO_INT(data);
   fprintf(stderr,"RITCTL_MENU: Serial Parity changed=%d\n", serial_parity);
}

void rigctl_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - RIGCTL");
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
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *rigctl_debug_b=gtk_check_button_new_with_label("Debug");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rigctl_debug_b), rigctl_debug);
  gtk_widget_show(rigctl_debug_b);
  gtk_grid_attach(GTK_GRID(grid),rigctl_debug_b,3,0,1,1);
  g_signal_connect(rigctl_debug_b,"toggled",G_CALLBACK(rigctl_debug_cb),NULL);
 
  GtkWidget *rigctl_enable_b=gtk_check_button_new_with_label("Rigctl Enable");
  //gtk_widget_override_font(tx_out_of_band_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rigctl_enable_b), rigctl_enable);
  gtk_widget_show(rigctl_enable_b);
  gtk_grid_attach(GTK_GRID(grid),rigctl_enable_b,0,1,1,1);
  g_signal_connect(rigctl_enable_b,"toggled",G_CALLBACK(rigctl_enable_cb),NULL);
 
  GtkWidget *rigctl_port_label =gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rigctl_port_label), "<b>RigCtl Port Number</b>");
  //gtk_widget_override_font(band_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(rigctl_port_label);
  gtk_grid_attach(GTK_GRID(grid),rigctl_port_label,0,2,1,1);

  GtkWidget *rigctl_port_spinner =gtk_spin_button_new_with_range(18000,21000,1);
  //gtk_widget_override_font(rigctl_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rigctl_port_spinner),(double)19090);
  gtk_widget_show(rigctl_port_spinner);
  gtk_grid_attach(GTK_GRID(grid),rigctl_port_spinner,1,2,2,1);
  g_signal_connect(rigctl_port_spinner,"value_changed",G_CALLBACK(rigctl_value_changed_cb),NULL);

  /* Put the Serial Port stuff here */
  GtkWidget *serial_enable_b=gtk_check_button_new_with_label("Serial Port Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_enable_b), serial_enable);
  gtk_widget_show(serial_enable_b);
  gtk_grid_attach(GTK_GRID(grid),serial_enable_b,0,3,1,1);
  g_signal_connect(serial_enable_b,"toggled",G_CALLBACK(serial_enable_cb),NULL);

  GtkWidget *serial_text_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(serial_text_label), "<b>Serial Port: </b>");
  gtk_grid_attach(GTK_GRID(grid),serial_text_label,0,4,1,1);

  serial_port_entry=gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(serial_port_entry),ser_port);
  gtk_widget_show(serial_port_entry);
  gtk_grid_attach(GTK_GRID(grid),serial_port_entry,1,4,2,1);

/*
  GtkWidget *serial_port_spinner =gtk_spin_button_new_with_range(0,7,1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(serial_port_spinner),(double)0);
  gtk_widget_show(serial_port_spinner);
  gtk_grid_attach(GTK_GRID(grid),serial_port_spinner,1,4,1,1);
  g_signal_connect(serial_port_spinner,"value_changed",G_CALLBACK(serial_value_changed_cb),NULL);
*/

  // Serial baud rate here
  GtkWidget *baud_rate_label =gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(baud_rate_label), "<b>Baud Rate:</b>");
  gtk_widget_show(baud_rate_label);
  gtk_grid_attach(GTK_GRID(grid),baud_rate_label,0,5,1,1);
  
  GtkWidget *baud_rate_b4800=gtk_radio_button_new_with_label(NULL,"4800");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (baud_rate_b4800), serial_baud_rate == B4800);
  gtk_widget_show(baud_rate_b4800);
  gtk_grid_attach(GTK_GRID(grid),baud_rate_b4800,1,5,1,1);
  g_signal_connect(baud_rate_b4800,"toggled",G_CALLBACK(baud_rate_cb),(gpointer *) B4800);

  GtkWidget *baud_rate_b9600=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(baud_rate_b4800),"9600");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (baud_rate_b9600), serial_baud_rate == B9600);
  gtk_widget_show(baud_rate_b9600);
  gtk_grid_attach(GTK_GRID(grid),baud_rate_b9600,2,5,1,1);
  g_signal_connect(baud_rate_b9600,"toggled",G_CALLBACK(baud_rate_cb),(gpointer *) B9600);

  GtkWidget *baud_rate_b19200=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(baud_rate_b9600),"19200");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (baud_rate_b19200),serial_baud_rate == B19200);
  gtk_widget_show(baud_rate_b19200);
  gtk_grid_attach(GTK_GRID(grid),baud_rate_b19200,3,5,1,1);
  g_signal_connect(baud_rate_b19200,"toggled",G_CALLBACK(baud_rate_cb),(gpointer *) B19200);

  GtkWidget *baud_rate_b38400=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(baud_rate_b19200),"38400");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (baud_rate_b38400),serial_baud_rate == B38400);
  gtk_widget_show(baud_rate_b38400);
  gtk_grid_attach(GTK_GRID(grid),baud_rate_b38400,4,5,1,1);
  g_signal_connect(baud_rate_b38400,"toggled",G_CALLBACK(baud_rate_cb),(gpointer *) B38400);

/*
  // Serial parity
  GtkWidget *parity_label =gtk_label_new("Parity:");
  gtk_widget_show(parity_label);
  gtk_grid_attach(GTK_GRID(grid),parity_label,0,6,1,1);
  
  GtkWidget *parity_none_b=gtk_radio_button_new_with_label(NULL,"None");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (parity_none_b), serial_parity == 0);
  gtk_widget_show(parity_none_b);
  gtk_grid_attach(GTK_GRID(grid),parity_none_b,1,6,1,1);
  g_signal_connect(parity_none_b,"toggled",G_CALLBACK(parity_cb),(gpointer *) 0);

  GtkWidget *parity_even_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(parity_none_b),"Even");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (parity_even_b), serial_parity == 1);
  gtk_widget_show(parity_even_b);
  gtk_grid_attach(GTK_GRID(grid),parity_even_b,2,6,1,1);
  g_signal_connect(parity_even_b,"toggled",G_CALLBACK(parity_cb),(gpointer *) 1);

  GtkWidget *parity_odd_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(parity_even_b),"Odd");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (parity_odd_b), serial_parity == 2);
  gtk_widget_show(parity_odd_b);
  gtk_grid_attach(GTK_GRID(grid),parity_odd_b,3,6,1,1);
  g_signal_connect(parity_odd_b,"toggled",G_CALLBACK(parity_cb),(gpointer *) 1);
*/


  // Below stays put
  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

