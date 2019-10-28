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
//#include "radio.h"
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
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
static GtkWidget *b_enable_E5_encoder;
static   GtkWidget *E5_a_label;
static   GtkWidget *E5_a;
static   GtkWidget *E5_b_label;
static   GtkWidget *E5_b;
static   GtkWidget *b_enable_E5_pullup;
#endif
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
  if(dialog!=NULL) {
    ENABLE_VFO_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_vfo_encoder))?1:0;
    VFO_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vfo_a));
    VFO_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vfo_b));
    ENABLE_VFO_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_vfo_pullup))?1:0;

    ENABLE_E2_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E2_encoder))?1:0;
    E2_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E2_a));
    E2_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E2_b));
    ENABLE_E2_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E2_pullup))?1:0;

    ENABLE_E3_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E3_encoder))?1:0;
    E3_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E3_a));
    E3_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E3_b));
    ENABLE_E3_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E3_pullup))?1:0;

    ENABLE_E4_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E4_encoder))?1:0;
    E4_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E4_a));
    E4_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(E4_b));
    ENABLE_E4_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_E4_pullup))?1:0;


/*
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
*/
#ifdef LOCALCW
    ENABLE_CW_BUTTONS=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_cwlr))?1:0;
    CW_ACTIVE_LOW=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_cw_active_low))?1:0;
    CWL_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cwl));
    CWR_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cwr));
    ENABLE_GPIO_SIDETONE=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_cws))?1:0;
    SIDETONE_GPIO=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cws));
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
  gpio_restore_state();

  dialog=gtk_dialog_new_with_buttons("Configure GPIO",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  int y=0;

  b_enable_vfo_encoder=gtk_check_button_new_with_label("Enable VFO");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_vfo_encoder), ENABLE_VFO_ENCODER);
  gtk_widget_show(b_enable_vfo_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_vfo_encoder,0,y,1,1);

  vfo_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(vfo_a_label);
  gtk_grid_attach(GTK_GRID(grid),vfo_a_label,1,y,1,1);

  vfo_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(vfo_a),VFO_ENCODER_A);
  gtk_widget_show(vfo_a);
  gtk_grid_attach(GTK_GRID(grid),vfo_a,2,y,1,1);

  vfo_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(vfo_b_label);
  gtk_grid_attach(GTK_GRID(grid),vfo_b_label,3,y,1,1);

  vfo_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(vfo_b),VFO_ENCODER_B);
  gtk_widget_show(vfo_b);
  gtk_grid_attach(GTK_GRID(grid),vfo_b,4,y,1,1);

  b_enable_vfo_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_vfo_pullup), ENABLE_VFO_PULLUP);
  gtk_widget_show(b_enable_vfo_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_vfo_pullup,5,y,1,1);


  y++;

  b_enable_E2_encoder=gtk_check_button_new_with_label("Enable E2");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E2_encoder), ENABLE_E2_ENCODER);
  gtk_widget_show(b_enable_E2_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_E2_encoder,0,y,1,1);

  E2_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(E2_a_label);
  gtk_grid_attach(GTK_GRID(grid),E2_a_label,1,y,1,1);

  E2_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(E2_a),E2_ENCODER_A);
  gtk_widget_show(E2_a);
  gtk_grid_attach(GTK_GRID(grid),E2_a,2,y,1,1);

  E2_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(E2_b_label);
  gtk_grid_attach(GTK_GRID(grid),E2_b_label,3,y,1,1);

  E2_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(E2_b),E2_ENCODER_B);
  gtk_widget_show(E2_b);
  gtk_grid_attach(GTK_GRID(grid),E2_b,4,y,1,1);

  b_enable_E2_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E2_pullup), ENABLE_E2_PULLUP);
  gtk_widget_show(b_enable_E2_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_E2_pullup,5,y,1,1);


  y++;

  b_enable_E3_encoder=gtk_check_button_new_with_label("Enable E3");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E3_encoder), ENABLE_E3_ENCODER);
  gtk_widget_show(b_enable_E3_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_E3_encoder,0,y,1,1);

  E3_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(E3_a_label);
  gtk_grid_attach(GTK_GRID(grid),E3_a_label,1,y,1,1);

  E3_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(E3_a),E3_ENCODER_A);
  gtk_widget_show(E3_a);
  gtk_grid_attach(GTK_GRID(grid),E3_a,2,y,1,1);

  E3_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(E3_b_label);
  gtk_grid_attach(GTK_GRID(grid),E3_b_label,3,y,1,1);

  E3_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(E3_b),E3_ENCODER_B);
  gtk_widget_show(E3_b);
  gtk_grid_attach(GTK_GRID(grid),E3_b,4,y,1,1);

  b_enable_E3_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E3_pullup), ENABLE_E3_PULLUP);
  gtk_widget_show(b_enable_E3_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_E3_pullup,5,y,1,1);



  y++;
  b_enable_E4_encoder=gtk_check_button_new_with_label("Enable E4");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E4_encoder), ENABLE_E4_ENCODER);
  gtk_widget_show(b_enable_E4_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_E4_encoder,0,y,1,1);

  E4_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(E4_a_label);
  gtk_grid_attach(GTK_GRID(grid),E4_a_label,1,y,1,1);

  E4_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(E4_a),E4_ENCODER_A);
  gtk_widget_show(E4_a);
  gtk_grid_attach(GTK_GRID(grid),E4_a,2,y,1,1);

  E4_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(E4_b_label);
  gtk_grid_attach(GTK_GRID(grid),E4_b_label,3,y,1,1);

  E4_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(E4_b),E4_ENCODER_B);
  gtk_widget_show(E4_b);
  gtk_grid_attach(GTK_GRID(grid),E4_b,4,y,1,1);

  b_enable_E4_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E4_pullup), ENABLE_E4_PULLUP);
  gtk_widget_show(b_enable_E4_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_E4_pullup,5,y,1,1);

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  y++;
  b_enable_E5_encoder=gtk_check_button_new_with_label("Enable E5");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E5_encoder), ENABLE_E5_ENCODER);
  gtk_widget_show(b_enable_E5_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_E5_encoder,0,y,1,1);

  E5_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(E5_a_label);
  gtk_grid_attach(GTK_GRID(grid),E5_a_label,1,y,1,1);

  E5_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(E5_a),E5_ENCODER_A);
  gtk_widget_show(E5_a);
  gtk_grid_attach(GTK_GRID(grid),E5_a,2,y,1,1);

  E5_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(E5_b_label);
  gtk_grid_attach(GTK_GRID(grid),E5_b_label,3,y,1,1);

  E5_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(E5_b),E5_ENCODER_B);
  gtk_widget_show(E5_b);
  gtk_grid_attach(GTK_GRID(grid),E5_b,4,y,1,1);

  b_enable_E5_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_E5_pullup), ENABLE_E5_PULLUP);
  gtk_widget_show(b_enable_E5_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_E5_pullup,5,y,1,1);

#endif

#if !defined (CONTROLLER2_V2) && !defined(CONTROLLER2_V1)
  y++;
  b_enable_mox=gtk_check_button_new_with_label("Enable MOX/TUN");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_mox), ENABLE_MOX_BUTTON);
  gtk_widget_show(b_enable_mox);
  gtk_grid_attach(GTK_GRID(grid),b_enable_mox,0,y,1,1);

  mox_label=gtk_label_new("GPIO:");
  gtk_widget_show(mox_label);
  gtk_grid_attach(GTK_GRID(grid),mox_label,1,y,1,1);

  mox=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(mox),MOX_BUTTON);
  gtk_widget_show(mox);
  gtk_grid_attach(GTK_GRID(grid),mox,2,y,1,1);

  y++;

  b_enable_S1=gtk_check_button_new_with_label("Enable S1");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S1), ENABLE_S1_BUTTON);
  gtk_widget_show(b_enable_S1);
  gtk_grid_attach(GTK_GRID(grid),b_enable_S1,0,y,1,1);

  S1_label=gtk_label_new("GPIO:");
  gtk_widget_show(S1_label);
  gtk_grid_attach(GTK_GRID(grid),S1_label,1,y,1,1);

  S1=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(S1),S1_BUTTON);
  gtk_widget_show(S1);
  gtk_grid_attach(GTK_GRID(grid),S1,2,y,1,1);
#endif

#ifdef LOCALCW
  // With LOCALCW, the menu got too long (does not fit on the screen)
  // Therefore it has been moved to the right of S1/S2/S3
  // The GPIO side tone is also configured here. Note that
  // these setting are only active when doing local CW
  cwl_label=gtk_label_new("CWL GPIO:");
  gtk_widget_show(cwl_label);
  gtk_grid_attach(GTK_GRID(grid),cwl_label,3,y,1,1);

  cwl=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(cwl),CWL_BUTTON);
  gtk_widget_show(cwl);
  gtk_grid_attach(GTK_GRID(grid),cwl,4,y,1,1);

  b_enable_cwlr=gtk_check_button_new_with_label("CWLR Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_cwlr), ENABLE_CW_BUTTONS);
  gtk_widget_show(b_enable_cwlr);
  gtk_grid_attach(GTK_GRID(grid),b_enable_cwlr,5,y,1,1);
#endif

  y++;

#if !defined (CONTROLLER2_V2) && !defined (CONTROLLER2_V1)
  b_enable_S2=gtk_check_button_new_with_label("Enable S2");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S2), ENABLE_S2_BUTTON);
  gtk_widget_show(b_enable_S2);
  gtk_grid_attach(GTK_GRID(grid),b_enable_S2,0,y,1,1);

  S2_label=gtk_label_new("GPIO:");
  gtk_widget_show(S2_label);
  gtk_grid_attach(GTK_GRID(grid),S2_label,1,y,1,1);

  S2=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(S2),S2_BUTTON);
  gtk_widget_show(S2);
  gtk_grid_attach(GTK_GRID(grid),S2,2,y,1,1);
#endif

#ifdef LOCALCW
  cwr_label=gtk_label_new("CWR GPIO:");
  gtk_widget_show(cwr_label);
  gtk_grid_attach(GTK_GRID(grid),cwr_label,3,y,1,1);

  cwr=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(cwr),CWR_BUTTON);
  gtk_widget_show(cwr);
  gtk_grid_attach(GTK_GRID(grid),cwr,4,y,1,1);

  b_cw_active_low=gtk_check_button_new_with_label("CWLR active-low");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_cw_active_low), CW_ACTIVE_LOW);
  gtk_widget_show(b_cw_active_low);
  gtk_grid_attach(GTK_GRID(grid),b_cw_active_low,5,y,1,1);
#endif

  y++;

#if !defined (CONTROLLER2_V2) && !defined (CONTROLLER2_V1)
  b_enable_S3=gtk_check_button_new_with_label("Enable S3");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S3), ENABLE_S3_BUTTON);
  gtk_widget_show(b_enable_S3);
  gtk_grid_attach(GTK_GRID(grid),b_enable_S3,0,y,1,1);

  S3_label=gtk_label_new("GPIO:");
  gtk_widget_show(S3_label);
  gtk_grid_attach(GTK_GRID(grid),S3_label,1,y,1,1);

  S3=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(S3),S3_BUTTON);
  gtk_widget_show(S3);
  gtk_grid_attach(GTK_GRID(grid),S3,2,y,1,1);
#endif

#ifdef LOCALCW
  cws_label=gtk_label_new("  SideTone GPIO:");
  gtk_widget_show(cws_label);
  gtk_grid_attach(GTK_GRID(grid),cws_label,3,y,1,1);

  cws=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(cws),SIDETONE_GPIO);
  gtk_widget_show(cws);
  gtk_grid_attach(GTK_GRID(grid),cws,4,y,1,1);

  b_enable_cws=gtk_check_button_new_with_label("Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_cws), ENABLE_GPIO_SIDETONE);
  gtk_widget_show(b_enable_cws);
  gtk_grid_attach(GTK_GRID(grid),b_enable_cws,5,y,1,1);
#endif
  y++;

#if !defined (CONTROLLER2_V2) && !defined (CONTROLLER2_V1)
  b_enable_S4=gtk_check_button_new_with_label("Enable S4");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S4), ENABLE_S4_BUTTON);
  gtk_widget_show(b_enable_S4);
  gtk_grid_attach(GTK_GRID(grid),b_enable_S4,0,y,1,1);

  S4_label=gtk_label_new("GPIO:");
  gtk_widget_show(S4_label);
  gtk_grid_attach(GTK_GRID(grid),S4_label,1,y,1,1);

  S4=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(S4),S4_BUTTON);
  gtk_widget_show(S4);
  gtk_grid_attach(GTK_GRID(grid),S4,2,y,1,1);

  y++;

  b_enable_S5=gtk_check_button_new_with_label("Enable S5");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S5), ENABLE_S5_BUTTON);
  gtk_widget_show(b_enable_S5);
  gtk_grid_attach(GTK_GRID(grid),b_enable_S5,0,y,1,1);

  S5_label=gtk_label_new("GPIO:");
  gtk_widget_show(S5_label);
  gtk_grid_attach(GTK_GRID(grid),S5_label,1,y,1,1);

  S5=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(S5),S5_BUTTON);
  gtk_widget_show(S5);
  gtk_grid_attach(GTK_GRID(grid),S5,2,y,1,1);

  y++;

  b_enable_S6=gtk_check_button_new_with_label("Enable S6");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_S6), ENABLE_S6_BUTTON);
  gtk_widget_show(b_enable_S6);
  gtk_grid_attach(GTK_GRID(grid),b_enable_S6,0,y,1,1);

  S6_label=gtk_label_new("GPIO:");
  gtk_widget_show(S6_label);
  gtk_grid_attach(GTK_GRID(grid),S6_label,1,y,1,1);

  S6=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(S6),S6_BUTTON);
  gtk_widget_show(S6);
  gtk_grid_attach(GTK_GRID(grid),S6,2,y,1,1);

  y++;

  b_enable_function=gtk_check_button_new_with_label("Enable Function");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_function), ENABLE_FUNCTION_BUTTON);
  gtk_widget_show(b_enable_function);
  gtk_grid_attach(GTK_GRID(grid),b_enable_function,0,y,1,1);

  function_label=gtk_label_new("GPIO:");
  gtk_widget_show(function_label);
  gtk_grid_attach(GTK_GRID(grid),function_label,1,y,1,1);

  function=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(function),FUNCTION_BUTTON);
  gtk_widget_show(function);
  gtk_grid_attach(GTK_GRID(grid),function,2,y,1,1);
#endif

  y++;

  GtkWidget *save_b=gtk_button_new_with_label("Save");
  g_signal_connect (save_b, "button_press_event", G_CALLBACK(save_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),save_b,4,y-1,1,1);

  GtkWidget *cancel_b=gtk_button_new_with_label("Cancel");
  g_signal_connect (cancel_b, "button_press_event", G_CALLBACK(cancel_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),cancel_b,5,y-1,1,1);

  gtk_container_add(GTK_CONTAINER(content),grid);

  gtk_widget_show_all(dialog);
  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}
#endif

