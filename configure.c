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
#include <glib.h>
#include <glib/gprintf.h>
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
#include "actions.h"
#include "gpio.h"
#include "i2c.h"


#ifdef GPIO

static GtkWidget *dialog;

static GtkWidget *i2c_sw_text[16];

static void response_event(GtkWidget *dialog,gint id,gpointer user_data) {
  g_print("%s: id=%d\n",__FUNCTION__,id);
  if(id==GTK_RESPONSE_ACCEPT) {
    gpio_save_state();
    g_print("%s: ACCEPT\n",__FUNCTION__);
  }
  gtk_widget_destroy(dialog);
  dialog=NULL;
}

void configure_gpio(GtkWidget *parent) {
  gint row=0;
  gint col=0;
  GtkWidget *widget;
  int i;

  gpio_restore_state();

  GtkWidget *dialog=gtk_dialog_new_with_buttons("piHPSDR - GPIO pins (Broadcom Numbers) ",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,("OK"),GTK_RESPONSE_ACCEPT,"Cancel",GTK_RESPONSE_REJECT,NULL);

  g_signal_connect (dialog, "response", G_CALLBACK (response_event), NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *notebook=gtk_notebook_new();

  // Encoders
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

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),2);
  gtk_grid_set_row_spacing (GTK_GRID(grid),2);


/*
  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), "<span foreground=\"#ff0000\"><b>Note: Pin number now use Broadcom GPIO</b></span>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,6,1);

  row++;
  col=0;
*/
  widget=gtk_label_new("");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
  col++;

  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), controller==CONTROLLER2_V2?"<b>Bottom Encoder</b>":"<b>Encoder</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,2,1);
  col+=2;

  if(controller==CONTROLLER2_V2) {
    widget=gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL(widget), "<b>Top Encoder</b>");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,2,1);
    col+=2;
  }

  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), "<b>Switch</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);

  row++;
  col=0;

  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), "<b>ID</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
  col++;

  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), "<b>Gpio A</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
  col++;

  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), "<b>Gpio B</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
  col++;

  if(controller==CONTROLLER2_V2) {
    widget=gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL(widget), "<b>Gpio A</b>");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    widget=gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL(widget), "<b>Gpio B</b>");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;
  }

  widget=gtk_label_new(NULL);
  gtk_label_set_markup (GTK_LABEL(widget), "<b>Gpio</b>");
  gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
  col++;

  row++;
  col=0;

  for(i=0;i<max_encoders;i++) {
    widget=gtk_label_new(NULL);
    gchar id[16];
    g_sprintf(id,"<b>%d</b>",i);
    gtk_label_set_markup (GTK_LABEL(widget), id);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    widget=gtk_spin_button_new_with_range (0.0,28.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(widget),encoders[i].bottom_encoder_address_a);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;
    
    widget=gtk_spin_button_new_with_range (0.0,28.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(widget),encoders[i].bottom_encoder_address_b);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;
    
    if(controller==CONTROLLER2_V2 && i<(max_encoders-1)) {
      widget=gtk_spin_button_new_with_range (0.0,28.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(widget),encoders[i].top_encoder_address_a);
      gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
      col++;
   
      widget=gtk_spin_button_new_with_range (0.0,28.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(widget),encoders[i].top_encoder_address_b);
      gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
      col++;
    }

    if(i<(max_encoders-1)) {
      widget=gtk_spin_button_new_with_range (0.0,28.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(widget),encoders[i].switch_address);
      gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
      col++;
    }

    row++;
    col=0;
  }
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid,gtk_label_new("Encoders"));


  // switches
  if(controller==CONTROLLER1) {
    gint max_switches=MAX_SWITCHES;
    switch(controller) {
      case NO_CONTROLLER:
        max_switches=0;
        break;
      case CONTROLLER1:
        max_switches=8;
        break;
      case CONTROLLER2_V1:
        max_switches=0;
        break;
      case CONTROLLER2_V2:
        max_switches=0;
        break;
    }
  
    grid=gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_spacing (GTK_GRID(grid),2);
    gtk_grid_set_row_spacing (GTK_GRID(grid),2);

    row=0;
    col=0;
  
/*
    widget=gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL(widget), "<span foreground=\"#ff0000\"><b>Note: Pin number now use Broadcom GPIO</b></span>");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,6,1);
  
    row++;
    col=0;
*/
    for(i=0;i<max_switches/8;i++) {
      widget=gtk_label_new(NULL);
      gtk_label_set_markup (GTK_LABEL(widget), "<b>ID</b>");
      gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
      col++;
    
      widget=gtk_label_new(NULL);
      gtk_label_set_markup (GTK_LABEL(widget), "<b>Gpio</b>");
      gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
      col++;
    }
  
    row++;
    col=0;
  
    for(i=0;i<max_switches;i++) {
      widget=gtk_label_new(NULL);
      gchar id[16];
      g_sprintf(id,"<b>%d</b>",i);
      gtk_label_set_markup (GTK_LABEL(widget), id);
      gtk_grid_attach(GTK_GRID(grid),widget,(i/8)*2,(row+(i%8)),1,1);
  
      widget=gtk_spin_button_new_with_range (0.0,28.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(widget),switches[i].switch_address);
      gtk_grid_attach(GTK_GRID(grid),widget,((i/8)*2)+1,(row+(i%8)),1,1);
    }
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid,gtk_label_new("switches"));
  }

  if(controller==CONTROLLER2_V1 || controller==CONTROLLER2_V2) {
    grid=gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_spacing (GTK_GRID(grid),2);
    gtk_grid_set_row_spacing (GTK_GRID(grid),2);

    row=0;
    col=0;
 
    char text[16];
    grid=gtk_grid_new();
    gtk_grid_set_column_spacing (GTK_GRID(grid),10);

    widget=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(widget),"<b>I2C Device</b>");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    widget=gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(widget),i2c_device);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    widget=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(widget),"<b>I2C Address</b>");
    gtk_widget_show(widget);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    widget=gtk_entry_new();
    sprintf(text,"0x%02X",i2c_address_1);
    gtk_entry_set_text(GTK_ENTRY(widget),text);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);

    row++;
    col=0;

    for(int i=0;i<8;i++) {
      widget=gtk_label_new(NULL);
      sprintf(text,"<b>SW_%d</b>",i+2);
      gtk_label_set_markup(GTK_LABEL(widget),text);
      gtk_grid_attach(GTK_GRID(grid),widget,0,row,1,1);

      i2c_sw_text[i]=gtk_entry_new();
      sprintf(text,"0x%04X",i2c_sw[i]);
      gtk_entry_set_text (GTK_ENTRY(i2c_sw_text[i]),text);
      gtk_grid_attach(GTK_GRID(grid),i2c_sw_text[i],1,row,1,1);

      widget=gtk_label_new(NULL);
      sprintf(text,"<b>SW_%d</b>",i+10);
      gtk_label_set_markup(GTK_LABEL(widget),text);
      gtk_grid_attach(GTK_GRID(grid),widget,2,row,1,1);

      i2c_sw_text[i+8]=gtk_entry_new();
      sprintf(text,"0x%04X",i2c_sw[i+8]);
      gtk_entry_set_text (GTK_ENTRY(i2c_sw_text[i+8]),text);
      gtk_grid_attach(GTK_GRID(grid),i2c_sw_text[i+8],3,row,1,1);

      row++;
    }
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid,gtk_label_new("i2c"));
  }

  gtk_container_add(GTK_CONTAINER(content),notebook);

  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
}
#endif

