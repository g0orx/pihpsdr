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
#include "gpio.h"
#include "i2c.h"


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
static GtkWidget *b_enable_E2_top_encoder;
static   GtkWidget *E2_top_a_label;
static   GtkWidget *E2_top_a;
static   GtkWidget *E2_top_b_label;
static   GtkWidget *E2_top_b;
static GtkWidget *E2_sw_label;
static   GtkWidget *E2_sw;
static GtkWidget *b_enable_E3_encoder;
static   GtkWidget *E3_a_label;
static   GtkWidget *E3_a;
static   GtkWidget *E3_b_label;
static   GtkWidget *E3_b;
static   GtkWidget *b_enable_E3_pullup;
static GtkWidget *b_enable_E3_top_encoder;
static   GtkWidget *E3_top_a_label;
static   GtkWidget *E3_top_a;
static   GtkWidget *E3_top_b_label;
static   GtkWidget *E3_top_b;
static GtkWidget *E3_sw_label;
static   GtkWidget *E3_sw;
static GtkWidget *b_enable_E4_encoder;
static   GtkWidget *E4_a_label;
static   GtkWidget *E4_a;
static   GtkWidget *E4_b_label;
static   GtkWidget *E4_b;
static   GtkWidget *b_enable_E4_pullup;
static GtkWidget *b_enable_E4_top_encoder;
static   GtkWidget *E4_top_a_label;
static   GtkWidget *E4_top_a;
static   GtkWidget *E4_top_b_label;
static   GtkWidget *E4_top_b;
static GtkWidget *E4_sw_label;
static   GtkWidget *E4_sw;
static GtkWidget *b_enable_E5_encoder;
static   GtkWidget *E5_a_label;
static   GtkWidget *E5_a;
static   GtkWidget *E5_b_label;
static   GtkWidget *E5_b;
static   GtkWidget *b_enable_E5_pullup;
static GtkWidget *b_enable_E5_top_encoder;
static   GtkWidget *E5_top_a_label;
static   GtkWidget *E5_top_a;
static   GtkWidget *E5_top_b_label;
static   GtkWidget *E5_top_b;
static GtkWidget *E5_sw_label;
static   GtkWidget *E5_sw;

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

static GtkWidget *i2c_device_text;
static GtkWidget *i2c_address;
static GtkWidget *i2c_sw_text[16];


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

#ifdef PTT
static GtkWidget *ptt_label;
static GtkWidget *ptt;
static GtkWidget *b_enable_ptt;
static GtkWidget *b_ptt_active_low;
#endif

static gboolean save_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    ENABLE_VFO_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_vfo_encoder))?1:0;
    VFO_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vfo_a));
    VFO_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vfo_b));
    ENABLE_VFO_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_vfo_pullup))?1:0;

    ENABLE_E2_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E2_encoder))?1:0;
    E2_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E2_a));
    E2_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E2_b));
    ENABLE_E2_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E2_pullup))?1:0;
    E2_FUNCTION=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E2_sw));

    ENABLE_E3_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E3_encoder))?1:0;
    E3_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E3_a));
    E3_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E3_b));
    ENABLE_E3_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E3_pullup))?1:0;
    E3_FUNCTION=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E3_sw));

    ENABLE_E4_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E4_encoder))?1:0;
    E4_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E4_a));
    E4_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E4_b));
    ENABLE_E4_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E4_pullup))?1:0;
    E4_FUNCTION=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E4_sw));

    if(controller==CONTROLLER2_V1 || controller==CONTROLLER2_V2) {
      ENABLE_E5_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E5_encoder))?1:0;
      E5_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E5_a));
      E5_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E5_b));
      ENABLE_E5_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E5_pullup))?1:0;
      E5_FUNCTION=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E5_sw));

      const char *temp=gtk_entry_get_text(GTK_ENTRY(i2c_device_text));
      i2c_device=g_new(char,strlen(temp)+1);
      strcpy(i2c_device,temp);
      i2c_address_1=(unsigned int)strtol(gtk_entry_get_text(GTK_ENTRY(i2c_address)),NULL,16);
      for(int i=0;i<16;i++) {
        i2c_sw[i]=(unsigned int)strtol(gtk_entry_get_text(GTK_ENTRY(i2c_sw_text[i])),NULL,16);
      }
    }

  if(controller==CONTROLLER1) {
    ENABLE_MOX_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_mox))?1:0;
    MOX_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(mox));

    ENABLE_S1_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_S1))?1:0;
    S1_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(S1));

    ENABLE_S2_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_S2))?1:0;
    S2_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(S2));

    ENABLE_S3_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_S3))?1:0;
    S3_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(S3));

    ENABLE_S4_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_S4))?1:0;
    S4_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(S4));

    ENABLE_S5_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_S5))?1:0;
    S5_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(S5));

    ENABLE_S6_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_S6))?1:0;
    S6_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(S6));

    ENABLE_FUNCTION_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_function))?1:0;
    FUNCTION_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(function));
  }

#ifdef LOCALCW
    ENABLE_CW_BUTTONS=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_cwlr))?1:0;
    CW_ACTIVE_LOW=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_cw_active_low))?1:0;
    CWL_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cwl));
    CWR_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cwr));
    ENABLE_GPIO_SIDETONE=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_cws))?1:0;
    SIDETONE_GPIO=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cws));
#endif

#ifdef PTT
    ENABLE_PTT_GPIO=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_ptt))?1:0;
    PTT_ACTIVE_LOW=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_ptt_active_low))?1:0;
    PTT_GPIO=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ptt));
#endif

    gpio_save_state();
    gtk_widget_destroy(dialog);
  }
  return TRUE;
}

static gboolean cancel_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
  }
  return TRUE;
}

void configure_gpio(GtkWidget *parent) {
  int row;

  gpio_restore_state();

  dialog=gtk_dialog_new_with_buttons("Configure GPIO (WiringPi pin numbers)",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *notebook=gtk_notebook_new();

  GtkWidget *grid0=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid0),10);

  GtkWidget *save_b=gtk_button_new_with_label("Save");
  g_signal_connect (save_b, "button_press_event", G_CALLBACK(save_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid0),save_b,0,0,1,1);

  GtkWidget *cancel_b=gtk_button_new_with_label("Cancel");
  g_signal_connect (cancel_b, "button_press_event", G_CALLBACK(cancel_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid0),cancel_b,1,0,1,1);


  if(controller!=NO_CONTROLLER) {

    // Encoders
  
    GtkWidget *grid1=gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(grid1),FALSE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid1),TRUE);
    gtk_grid_set_column_spacing (GTK_GRID(grid1),5);
    row=0;
  
    b_enable_vfo_encoder=gtk_check_button_new_with_label("Enable VFO");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_vfo_encoder), ENABLE_VFO_ENCODER);
    gtk_widget_show(b_enable_vfo_encoder);
    gtk_grid_attach(GTK_GRID(grid1),b_enable_vfo_encoder,0,row,1,1);
  
    vfo_a_label=gtk_label_new("GPIO A:");
    gtk_widget_show(vfo_a_label);
    gtk_grid_attach(GTK_GRID(grid1),vfo_a_label,1,row,1,1);
  
    vfo_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(vfo_a),VFO_ENCODER_A);
    gtk_widget_show(vfo_a);
    gtk_grid_attach(GTK_GRID(grid1),vfo_a,2,row,1,1);
  
    vfo_b_label=gtk_label_new("GPIO B:");
    gtk_widget_show(vfo_b_label);
    gtk_grid_attach(GTK_GRID(grid1),vfo_b_label,3,row,1,1);
  
    vfo_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(vfo_b),VFO_ENCODER_B);
    gtk_widget_show(vfo_b);
    gtk_grid_attach(GTK_GRID(grid1),vfo_b,4,row,1,1);

    b_enable_vfo_pullup=gtk_check_button_new_with_label("Enable Pull-up");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_vfo_pullup), ENABLE_VFO_PULLUP);
    gtk_widget_show(b_enable_vfo_pullup);
    gtk_grid_attach(GTK_GRID(grid1),b_enable_vfo_pullup,7,row,1,1);
  
  
    row++;
  
    b_enable_E2_encoder=gtk_check_button_new_with_label("Enable E2");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E2_encoder), ENABLE_E2_ENCODER);
    gtk_widget_show(b_enable_E2_encoder);
    gtk_grid_attach(GTK_GRID(grid1),b_enable_E2_encoder,0,row,1,1);
  
    E2_a_label=gtk_label_new("GPIO A:");
    gtk_widget_show(E2_a_label);
    gtk_grid_attach(GTK_GRID(grid1),E2_a_label,1,row,1,1);
  
    E2_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E2_a),E2_ENCODER_A);
    gtk_widget_show(E2_a);
    gtk_grid_attach(GTK_GRID(grid1),E2_a,2,row,1,1);
  
    E2_b_label=gtk_label_new("GPIO B:");
    gtk_widget_show(E2_b_label);
    gtk_grid_attach(GTK_GRID(grid1),E2_b_label,3,row,1,1);
  
    E2_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E2_b),E2_ENCODER_B);
    gtk_widget_show(E2_b);
    gtk_grid_attach(GTK_GRID(grid1),E2_b,4,row,1,1);
  
    E2_sw_label=gtk_label_new("SW:");
    gtk_widget_show(E2_sw_label);
    gtk_grid_attach(GTK_GRID(grid1),E2_sw_label,5,row,1,1);
  
    E2_sw=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E2_sw),E2_FUNCTION);
    gtk_widget_show(E2_sw);
    gtk_grid_attach(GTK_GRID(grid1),E2_sw,6,row,1,1);
  
    b_enable_E2_pullup=gtk_check_button_new_with_label("Enable Pull-up");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E2_pullup), ENABLE_E2_PULLUP);
    gtk_widget_show(b_enable_E2_pullup);
    gtk_grid_attach(GTK_GRID(grid1),b_enable_E2_pullup,7,row,1,1);
  
  
    row++;
  
    b_enable_E3_encoder=gtk_check_button_new_with_label("Enable E3");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E3_encoder), ENABLE_E3_ENCODER);
    gtk_widget_show(b_enable_E3_encoder);
    gtk_grid_attach(GTK_GRID(grid1),b_enable_E3_encoder,0,row,1,1);
  
    E3_a_label=gtk_label_new("GPIO A:");
    gtk_widget_show(E3_a_label);
    gtk_grid_attach(GTK_GRID(grid1),E3_a_label,1,row,1,1);
  
    E3_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E3_a),E3_ENCODER_A);
    gtk_widget_show(E3_a);
    gtk_grid_attach(GTK_GRID(grid1),E3_a,2,row,1,1);
  
    E3_b_label=gtk_label_new("GPIO B:");
    gtk_widget_show(E3_b_label);
    gtk_grid_attach(GTK_GRID(grid1),E3_b_label,3,row,1,1);
  
    E3_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E3_b),E3_ENCODER_B);
    gtk_widget_show(E3_b);
    gtk_grid_attach(GTK_GRID(grid1),E3_b,4,row,1,1);
  
    E3_sw_label=gtk_label_new("SW:");
    gtk_widget_show(E3_sw_label);
    gtk_grid_attach(GTK_GRID(grid1),E3_sw_label,5,row,1,1);
  
    E3_sw=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E3_sw),E3_FUNCTION);
    gtk_widget_show(E3_sw);
    gtk_grid_attach(GTK_GRID(grid1),E3_sw,6,row,1,1);
  
    b_enable_E3_pullup=gtk_check_button_new_with_label("Enable Pull-up");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E3_pullup), ENABLE_E3_PULLUP);
    gtk_widget_show(b_enable_E3_pullup);
    gtk_grid_attach(GTK_GRID(grid1),b_enable_E3_pullup,7,row,1,1);
  
    row++;
  
    b_enable_E4_encoder=gtk_check_button_new_with_label("Enable E4");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E4_encoder), ENABLE_E4_ENCODER);
    gtk_widget_show(b_enable_E4_encoder);
    gtk_grid_attach(GTK_GRID(grid1),b_enable_E4_encoder,0,row,1,1);
  
    E4_a_label=gtk_label_new("GPIO A:");
    gtk_widget_show(E4_a_label);
    gtk_grid_attach(GTK_GRID(grid1),E4_a_label,1,row,1,1);
  
    E4_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E4_a),E4_ENCODER_A);
    gtk_widget_show(E4_a);
    gtk_grid_attach(GTK_GRID(grid1),E4_a,2,row,1,1);
  
    E4_b_label=gtk_label_new("GPIO B:");
    gtk_widget_show(E4_b_label);
    gtk_grid_attach(GTK_GRID(grid1),E4_b_label,3,row,1,1);
  
    E4_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E4_b),E4_ENCODER_B);
    gtk_widget_show(E4_b);
    gtk_grid_attach(GTK_GRID(grid1),E4_b,4,row,1,1);
  
    E4_sw_label=gtk_label_new("SW:");
    gtk_widget_show(E4_sw_label);
    gtk_grid_attach(GTK_GRID(grid1),E4_sw_label,5,row,1,1);
  
    E4_sw=gtk_spin_button_new_with_range (0.0,100.0,1.0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(E4_sw),E4_FUNCTION);
    gtk_widget_show(E4_sw);
    gtk_grid_attach(GTK_GRID(grid1),E4_sw,6,row,1,1);
  
    b_enable_E4_pullup=gtk_check_button_new_with_label("Enable Pull-up");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E4_pullup), ENABLE_E4_PULLUP);
    gtk_widget_show(b_enable_E4_pullup);
    gtk_grid_attach(GTK_GRID(grid1),b_enable_E4_pullup,7,row,1,1);
  
    row++;
  
    if(controller==CONTROLLER2_V1 || controller==CONTROLLER2_V2) {
      b_enable_E5_encoder=gtk_check_button_new_with_label("Enable E5");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E5_encoder), ENABLE_E5_ENCODER);
      gtk_widget_show(b_enable_E5_encoder);
      gtk_grid_attach(GTK_GRID(grid1),b_enable_E5_encoder,0,row,1,1);
  
      E5_a_label=gtk_label_new("GPIO A:");
      gtk_widget_show(E5_a_label);
      gtk_grid_attach(GTK_GRID(grid1),E5_a_label,1,row,1,1);
  
      E5_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(E5_a),E5_ENCODER_A);
      gtk_widget_show(E5_a);
      gtk_grid_attach(GTK_GRID(grid1),E5_a,2,row,1,1);
  
      E5_b_label=gtk_label_new("GPIO B:");
      gtk_widget_show(E5_b_label);
      gtk_grid_attach(GTK_GRID(grid1),E5_b_label,3,row,1,1);
  
      E5_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(E5_b),E5_ENCODER_B);
      gtk_widget_show(E5_b);
      gtk_grid_attach(GTK_GRID(grid1),E5_b,4,row,1,1);

      E5_sw_label=gtk_label_new("SW:");
      gtk_widget_show(E5_sw_label);
      gtk_grid_attach(GTK_GRID(grid1),E5_sw_label,5,row,1,1);
    
      E5_sw=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(E5_sw),E5_FUNCTION);
      gtk_widget_show(E5_sw);
      gtk_grid_attach(GTK_GRID(grid1),E5_sw,6,row,1,1);
  
      b_enable_E5_pullup=gtk_check_button_new_with_label("Enable Pull-up");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E5_pullup), ENABLE_E5_PULLUP);
      gtk_widget_show(b_enable_E5_pullup);
      gtk_grid_attach(GTK_GRID(grid1),b_enable_E5_pullup,7,row,1,1);
    }
  
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid1,gtk_label_new("Encoders"));
  
    // Switches
  
    GtkWidget *grid2=gtk_grid_new();
    gtk_grid_set_column_spacing (GTK_GRID(grid2),10);
    row=0;


    if(controller==CONTROLLER1) {
      b_enable_mox=gtk_check_button_new_with_label("Enable MOX/TUN");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_mox), ENABLE_MOX_BUTTON);
      gtk_widget_show(b_enable_mox);
      gtk_grid_attach(GTK_GRID(grid2),b_enable_mox,0,row,1,1);
  
      mox_label=gtk_label_new("GPIO:");
      gtk_widget_show(mox_label);
      gtk_grid_attach(GTK_GRID(grid2),mox_label,1,row,1,1);
  
      mox=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(mox),MOX_BUTTON);
      gtk_widget_show(mox);
      gtk_grid_attach(GTK_GRID(grid2),mox,2,row,1,1);

      row++;
  
      b_enable_S1=gtk_check_button_new_with_label("Enable S1");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S1), ENABLE_S1_BUTTON);
      gtk_widget_show(b_enable_S1);
      gtk_grid_attach(GTK_GRID(grid2),b_enable_S1,0,row,1,1);
  
      S1_label=gtk_label_new("GPIO:");
      gtk_widget_show(S1_label);
      gtk_grid_attach(GTK_GRID(grid2),S1_label,1,row,1,1);
  
      S1=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(S1),S1_BUTTON);
      gtk_widget_show(S1);
      gtk_grid_attach(GTK_GRID(grid2),S1,2,row,1,1);

      row++;

      b_enable_S2=gtk_check_button_new_with_label("Enable S2");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S2), ENABLE_S2_BUTTON);
      gtk_widget_show(b_enable_S2);
      gtk_grid_attach(GTK_GRID(grid),b_enable_S2,0,row,1,1);
    
      S2_label=gtk_label_new("GPIO:");
      gtk_widget_show(S2_label);
      gtk_grid_attach(GTK_GRID(grid),S2_label,1,row,1,1);
  
      S2=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(S2),S2_BUTTON);
      gtk_widget_show(S2);
      gtk_grid_attach(GTK_GRID(grid),S2,2,row,1,1);
  
      row++;
  
      b_enable_S3=gtk_check_button_new_with_label("Enable S3");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S3), ENABLE_S3_BUTTON);
      gtk_widget_show(b_enable_S3);
      gtk_grid_attach(GTK_GRID(grid),b_enable_S3,0,row,1,1);
    
      S3_label=gtk_label_new("GPIO:");
      gtk_widget_show(S3_label);
      gtk_grid_attach(GTK_GRID(grid),S3_label,1,row,1,1);
    
      S3=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(S3),S3_BUTTON);
      gtk_widget_show(S3);
      gtk_grid_attach(GTK_GRID(grid),S3,2,row,1,1);

      row++;

      b_enable_S4=gtk_check_button_new_with_label("Enable S4");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S4), ENABLE_S4_BUTTON);
      gtk_widget_show(b_enable_S4);
      gtk_grid_attach(GTK_GRID(grid2),b_enable_S4,0,row,1,1);
    
      S4_label=gtk_label_new("GPIO:");
      gtk_widget_show(S4_label);
      gtk_grid_attach(GTK_GRID(grid2),S4_label,1,row,1,1);
    
      S4=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(S4),S4_BUTTON);
      gtk_widget_show(S4);
      gtk_grid_attach(GTK_GRID(grid2),S4,2,row,1,1);
    
      row++;
    
      b_enable_S5=gtk_check_button_new_with_label("Enable S5");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S5), ENABLE_S5_BUTTON);
      gtk_widget_show(b_enable_S5);
      gtk_grid_attach(GTK_GRID(grid2),b_enable_S5,0,row,1,1);
  
      S5_label=gtk_label_new("GPIO:");
      gtk_widget_show(S5_label);
      gtk_grid_attach(GTK_GRID(grid2),S5_label,1,row,1,1);
    
      S5=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(S5),S5_BUTTON);
      gtk_widget_show(S5);
      gtk_grid_attach(GTK_GRID(grid2),S5,2,row,1,1);
    
      row++;
    
      b_enable_S6=gtk_check_button_new_with_label("Enable S6");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S6), ENABLE_S6_BUTTON);
      gtk_widget_show(b_enable_S6);
      gtk_grid_attach(GTK_GRID(grid2),b_enable_S6,0,row,1,1);
    
      S6_label=gtk_label_new("GPIO:");
      gtk_widget_show(S6_label);
      gtk_grid_attach(GTK_GRID(grid2),S6_label,1,row,1,1);
    
      S6=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(S6),S6_BUTTON);
      gtk_widget_show(S6);
      gtk_grid_attach(GTK_GRID(grid2),S6,2,row,1,1);
    
      row++;
    
      b_enable_function=gtk_check_button_new_with_label("Enable Function");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_function), ENABLE_FUNCTION_BUTTON);
      gtk_widget_show(b_enable_function);
      gtk_grid_attach(GTK_GRID(grid2),b_enable_function,0,row,1,1);
    
      function_label=gtk_label_new("GPIO:");
      gtk_widget_show(function_label);
      gtk_grid_attach(GTK_GRID(grid2),function_label,1,row,1,1);
    
      function=gtk_spin_button_new_with_range (0.0,100.0,1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON(function),FUNCTION_BUTTON);
      gtk_widget_show(function);
      gtk_grid_attach(GTK_GRID(grid2),function,2,row,1,1);

      gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid2,gtk_label_new("Switches"));
    } else {
      char text[16];
      GtkWidget *grid2=gtk_grid_new();
      gtk_grid_set_column_spacing (GTK_GRID(grid2),10);
      row=0;
  
      GtkWidget *label=gtk_label_new("I2C Device:");
      gtk_widget_show(label);
      gtk_grid_attach(GTK_GRID(grid2),label,0,row,1,1);
  
      i2c_device_text=gtk_entry_new();
      gtk_widget_show(i2c_device_text);
      gtk_entry_set_text (GTK_ENTRY(i2c_device_text),i2c_device);
      gtk_grid_attach(GTK_GRID(grid2),i2c_device_text,1,row,1,1);
     
      label=gtk_label_new("I2C Address:");
      gtk_widget_show(label);
      gtk_grid_attach(GTK_GRID(grid2),label,2,row,1,1);
  
      i2c_address=gtk_entry_new();
      sprintf(text,"0x%02X",i2c_address_1);
      gtk_entry_set_text (GTK_ENTRY(i2c_address),text);
      gtk_widget_show(i2c_address);
      gtk_grid_attach(GTK_GRID(grid2),i2c_address,3,row,1,1);

      row++;

      for(int i=0;i<8;i++) {
        sprintf(text,"SW_%d",i+2);
        label=gtk_label_new(text);
        gtk_widget_show(label);
        gtk_grid_attach(GTK_GRID(grid2),label,0,row,1,1);

        i2c_sw_text[i]=gtk_entry_new();
        sprintf(text,"0x%04X",i2c_sw[i]);
        gtk_entry_set_text (GTK_ENTRY(i2c_sw_text[i]),text);
        gtk_widget_show(i2c_sw_text[i]);
        gtk_grid_attach(GTK_GRID(grid2),i2c_sw_text[i],1,row,1,1);

        sprintf(text,"SW_%d",i+10);
        label=gtk_label_new(text);
        gtk_widget_show(label);
        gtk_grid_attach(GTK_GRID(grid2),label,2,row,1,1);

        i2c_sw_text[i+8]=gtk_entry_new();
        sprintf(text,"0x%04X",i2c_sw[i+8]);
        gtk_entry_set_text (GTK_ENTRY(i2c_sw_text[i+8]),text);
        gtk_widget_show(i2c_sw_text[i+8]);
        gtk_grid_attach(GTK_GRID(grid2),i2c_sw_text[i+8],3,row,1,1);

        row++;

      }

      gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid2,gtk_label_new("I2C"));
    }

  }

#ifdef LOCALCW
  // CW

  GtkWidget *grid3=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid3),10);
  row=0;

  cwl_label=gtk_label_new("CWL GPIO:");
  gtk_widget_show(cwl_label);
  gtk_grid_attach(GTK_GRID(grid3),cwl_label,0,row,1,1);

  cwl=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(cwl),CWL_BUTTON);
  gtk_widget_show(cwl);
  gtk_grid_attach(GTK_GRID(grid3),cwl,1,row,1,1);

  b_enable_cwlr=gtk_check_button_new_with_label("CWLR Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_cwlr), ENABLE_CW_BUTTONS);
  gtk_widget_show(b_enable_cwlr);
  gtk_grid_attach(GTK_GRID(grid3),b_enable_cwlr,2,row,1,1);

  row++;

  cwr_label=gtk_label_new("CWR GPIO:");
  gtk_widget_show(cwr_label);
  gtk_grid_attach(GTK_GRID(grid3),cwr_label,0,row,1,1);

  cwr=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(cwr),CWR_BUTTON);
  gtk_widget_show(cwr);
  gtk_grid_attach(GTK_GRID(grid3),cwr,1,row,1,1);

  b_cw_active_low=gtk_check_button_new_with_label("CWLR active-low");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_cw_active_low), CW_ACTIVE_LOW);
  gtk_widget_show(b_cw_active_low);
  gtk_grid_attach(GTK_GRID(grid3),b_cw_active_low,2,row,1,1);

  row++;

  cws_label=gtk_label_new("  SideTone GPIO:");
  gtk_widget_show(cws_label);
  gtk_grid_attach(GTK_GRID(grid3),cws_label,0,row,1,1);

  cws=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(cws),SIDETONE_GPIO);
  gtk_widget_show(cws);
  gtk_grid_attach(GTK_GRID(grid3),cws,1,row,1,1);

  b_enable_cws=gtk_check_button_new_with_label("Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_cws), ENABLE_GPIO_SIDETONE);
  gtk_widget_show(b_enable_cws);
  gtk_grid_attach(GTK_GRID(grid3),b_enable_cws,2,row,1,1);

  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid3,gtk_label_new("CW"));
#endif

#ifdef PTT
  GtkWidget *grid4=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid4),10);
  row=0;

  b_enable_ptt=gtk_check_button_new_with_label("PTT Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_ptt), ENABLE_PTT_GPIO);
  gtk_widget_show(b_enable_ptt);
  gtk_grid_attach(GTK_GRID(grid4),b_enable_ptt,0,row,1,1);

  row++;

  ptt_label=gtk_label_new("PTT GPIO:");
  gtk_widget_show(ptt_label);
  gtk_grid_attach(GTK_GRID(grid4),ptt_label,0,row,1,1);

  ptt=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(ptt),CWR_BUTTON);
  gtk_widget_show(ptt);
  gtk_grid_attach(GTK_GRID(grid4),ptt,1,row,1,1);

  row++;

  b_ptt_active_low=gtk_check_button_new_with_label("PTT active-low");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_ptt_active_low), PTT_ACTIVE_LOW);
  gtk_widget_show(b_ptt_active_low);
  gtk_grid_attach(GTK_GRID(grid4),b_ptt_active_low,0,row,1,1);

  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid4,gtk_label_new("PTT"));
#endif

  gtk_grid_attach(GTK_GRID(grid0),notebook,0,1,6,1);
  gtk_container_add(GTK_CONTAINER(content),grid0);

  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));

}
#endif

