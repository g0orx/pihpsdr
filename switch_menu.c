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
  int sw;
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


static void sw_select_cb(GtkWidget *widget, gpointer data) {
  char text[128];
  CHOICE *choice=(CHOICE *)data;
  sw_action[choice->sw]=choice->action;
  GtkWidget *label=gtk_bin_get_child(GTK_BIN(choice->button));
  sprintf(text,"<span size=\"smaller\">%s</span>",sw_string[choice->action]);
  gtk_label_set_markup (GTK_LABEL(label), text);
}

static gboolean sw_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int sw=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<SWITCH_ACTIONS;i++) {
    GtkWidget *menu_item=gtk_menu_item_new_with_label(sw_string[i]);
    CHOICE *choice=g_new0(CHOICE,1);
    choice->sw=sw;
    choice->action=i;
    choice->button=widget;
    g_signal_connect(menu_item,"activate",G_CALLBACK(sw_select_cb),choice);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
fprintf(stderr,"%d=%s\n",i,sw_string[i]);
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

void switch_menu(GtkWidget *parent) {
  int row=0;
  int col=0;
  char label[64];
  int i;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[32];
  sprintf(title,"piHPSDR - Encoder Actions:");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),2);
  gtk_grid_set_row_spacing (GTK_GRID(grid),2);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  GtkWidget *close_label=gtk_bin_get_child(GTK_BIN(close_b));
  sprintf(label,"<span size=\"smaller\">Close</span>");
  gtk_label_set_markup (GTK_LABEL(close_label), label);
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  row++;
  col=0;

  switch(controller) {
    case NO_CONTROLLER:
      // Nothing to do in this pihpsdr version
      {
      GtkWidget *text_w=gtk_label_new("Nothing to do here with the  'No Controller' option!     ");
      gtk_grid_attach(GTK_GRID(grid),text_w,1,1,3,1);
      col++;
      }
      break;
    case CONTROLLER1:
      {
      GtkWidget *sw7_title=gtk_label_new("SW7: ");
      gtk_grid_attach(GTK_GRID(grid),sw7_title,col,row,1,1);
      col++;

      GtkWidget *sw7_combo_box=gtk_combo_box_text_new();
      for(i=0;i<SWITCH_ACTIONS;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sw7_combo_box),NULL,sw_string[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sw7_combo_box),sw_action[6]);
      g_signal_connect(sw7_combo_box,"changed",G_CALLBACK(sw_cb),GINT_TO_POINTER(CONTROLLER1_SW7));
      gtk_grid_attach(GTK_GRID(grid),sw7_combo_box,col,row,1,1);
      col++;

      GtkWidget *sw1_title=gtk_label_new("SW1: ");
      gtk_grid_attach(GTK_GRID(grid),sw1_title,col,row,1,1);
      col++;

      GtkWidget *sw1_combo_box=gtk_combo_box_text_new();
      for(i=0;i<SWITCH_ACTIONS;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sw1_combo_box),NULL,sw_string[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sw1_combo_box),sw_action[0]);
      g_signal_connect(sw1_combo_box,"changed",G_CALLBACK(sw_cb),GINT_TO_POINTER(CONTROLLER1_SW1));
      gtk_grid_attach(GTK_GRID(grid),sw1_combo_box,col,row,1,1);
      row++;
      col=0;

      GtkWidget *sw2_title=gtk_label_new("SW2: ");
      gtk_grid_attach(GTK_GRID(grid),sw2_title,col,row,1,1);
      col++;

      GtkWidget *sw2_combo_box=gtk_combo_box_text_new();
      for(i=0;i<SWITCH_ACTIONS;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sw2_combo_box),NULL,sw_string[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sw2_combo_box),sw_action[1]);
      g_signal_connect(sw2_combo_box,"changed",G_CALLBACK(sw_cb),GINT_TO_POINTER(CONTROLLER1_SW2));
      gtk_grid_attach(GTK_GRID(grid),sw2_combo_box,col,row,1,1);
      col++;

      GtkWidget *sw3_title=gtk_label_new("SW3: ");
      gtk_grid_attach(GTK_GRID(grid),sw3_title,col,row,1,1);
      col++;

      GtkWidget *sw3_combo_box=gtk_combo_box_text_new();
      for(i=0;i<SWITCH_ACTIONS;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sw3_combo_box),NULL,sw_string[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sw3_combo_box),sw_action[2]);
      g_signal_connect(sw3_combo_box,"changed",G_CALLBACK(sw_cb),GINT_TO_POINTER(CONTROLLER1_SW3));
      gtk_grid_attach(GTK_GRID(grid),sw3_combo_box,col,row,1,1);
      row++;
      col=0;

      GtkWidget *sw4_title=gtk_label_new("SW4: ");
      gtk_grid_attach(GTK_GRID(grid),sw4_title,col,row,1,1);
      col++;

      GtkWidget *sw4_combo_box=gtk_combo_box_text_new();
      for(i=0;i<SWITCH_ACTIONS;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sw4_combo_box),NULL,sw_string[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sw4_combo_box),sw_action[3]);
      g_signal_connect(sw4_combo_box,"changed",G_CALLBACK(sw_cb),GINT_TO_POINTER(CONTROLLER1_SW4));
      gtk_grid_attach(GTK_GRID(grid),sw4_combo_box,col,row,1,1);
      col++;

      GtkWidget *sw5_title=gtk_label_new("SW5: ");
      gtk_grid_attach(GTK_GRID(grid),sw5_title,col,row,1,1);
      col++;

      GtkWidget *sw5_combo_box=gtk_combo_box_text_new();
      for(i=0;i<SWITCH_ACTIONS;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sw5_combo_box),NULL,sw_string[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sw5_combo_box),sw_action[4]);
      g_signal_connect(sw5_combo_box,"changed",G_CALLBACK(sw_cb),GINT_TO_POINTER(CONTROLLER1_SW5));
      gtk_grid_attach(GTK_GRID(grid),sw5_combo_box,col,row,1,1);
      row++;
      col=0;

      GtkWidget *sw6_title=gtk_label_new("SW6: ");
      gtk_grid_attach(GTK_GRID(grid),sw6_title,col,row,1,1);
      col++;

      GtkWidget *sw6_combo_box=gtk_combo_box_text_new();
      for(i=0;i<SWITCH_ACTIONS;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sw6_combo_box),NULL,sw_string[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sw6_combo_box),sw_action[5]);
      g_signal_connect(sw6_combo_box,"changed",G_CALLBACK(sw_cb),GINT_TO_POINTER(CONTROLLER1_SW6));
      gtk_grid_attach(GTK_GRID(grid),sw6_combo_box,col,row,1,1);
      col++;

      GtkWidget *sw8_title=gtk_label_new("SW8: ");
      gtk_grid_attach(GTK_GRID(grid),sw8_title,col,row,1,1);
      col++;

      GtkWidget *sw8_combo_box=gtk_combo_box_text_new();
      for(i=0;i<SWITCH_ACTIONS;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sw8_combo_box),NULL,sw_string[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sw8_combo_box),sw_action[7]);
      g_signal_connect(sw8_combo_box,"changed",G_CALLBACK(sw_cb),GINT_TO_POINTER(CONTROLLER1_SW8));
      gtk_grid_attach(GTK_GRID(grid),sw8_combo_box,col,row,1,1);
      col++;
      }
      break;

    case CONTROLLER2_V1:
    case CONTROLLER2_V2:
      {
      col=8;

      GtkWidget *sw13=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW13]]);
      GtkWidget *sw13_label=gtk_bin_get_child(GTK_BIN(sw13));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW13]]);
      gtk_label_set_markup (GTK_LABEL(sw13_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw13,col,row,1,1);
      g_signal_connect (sw13, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW13));
      row++;
      col=7;

      GtkWidget *sw12=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW12]]);
      GtkWidget *sw12_label=gtk_bin_get_child(GTK_BIN(sw12));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW12]]);
      gtk_label_set_markup (GTK_LABEL(sw12_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw12,col,row,1,1);
      g_signal_connect (sw12, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW12));
      col++;

      GtkWidget *sw11=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW11]]);
      GtkWidget *sw11_label=gtk_bin_get_child(GTK_BIN(sw11));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW11]]);
      gtk_label_set_markup (GTK_LABEL(sw11_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw11,col,row,1,1);
      g_signal_connect (sw11, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW11));
      row++;
      col=7;

      GtkWidget *sw10=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW10]]);
      GtkWidget *sw10_label=gtk_bin_get_child(GTK_BIN(sw10));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW10]]);
      gtk_label_set_markup (GTK_LABEL(sw10_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw10,col,row,1,1);
      g_signal_connect (sw10, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW10));
      col++;

      GtkWidget *sw9=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW9]]);
      GtkWidget *sw9_label=gtk_bin_get_child(GTK_BIN(sw9));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW9]]);
      gtk_label_set_markup (GTK_LABEL(sw9_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw9,col,row,1,1);
      g_signal_connect (sw9, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW9));
      row++;
      col=7;

      GtkWidget *sw7=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW7]]);
      GtkWidget *sw7_label=gtk_bin_get_child(GTK_BIN(sw7));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW7]]);
      gtk_label_set_markup (GTK_LABEL(sw7_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw7,col,row,1,1);
      g_signal_connect (sw7, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW7));
      col++;

      GtkWidget *sw8=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW8]]);
      GtkWidget *sw8_label=gtk_bin_get_child(GTK_BIN(sw8));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW8]]);
      gtk_label_set_markup (GTK_LABEL(sw8_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw8,col,row,1,1);
      g_signal_connect (sw8, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW8));
      row++;
      col=7;

      GtkWidget *sw16=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW16]]);
      GtkWidget *sw16_label=gtk_bin_get_child(GTK_BIN(sw16));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW16]]);
      gtk_label_set_markup (GTK_LABEL(sw16_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw16,col,row,1,1);
      g_signal_connect (sw16, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW16));
      col++;

      GtkWidget *sw17=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW17]]);
      GtkWidget *sw17_label=gtk_bin_get_child(GTK_BIN(sw17));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW17]]);
      gtk_label_set_markup (GTK_LABEL(sw17_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw17,col,row,1,1);
      g_signal_connect (sw17, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW17));
      row++;
      col=0;

      GtkWidget *sw2=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW2]]);
      GtkWidget *sw2_label=gtk_bin_get_child(GTK_BIN(sw2));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW2]]);
      gtk_label_set_markup (GTK_LABEL(sw2_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw2,col,row,1,1);
      g_signal_connect (sw2, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW2));
      col++;
  
      GtkWidget *sw3=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW3]]);
      GtkWidget *sw3_label=gtk_bin_get_child(GTK_BIN(sw3));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW3]]);
      gtk_label_set_markup (GTK_LABEL(sw3_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw3,col,row,1,1);
      g_signal_connect (sw3, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW3));
      col++;
  
      GtkWidget *sw4=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW4]]);
      GtkWidget *sw4_label=gtk_bin_get_child(GTK_BIN(sw4));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW4]]);
      gtk_label_set_markup (GTK_LABEL(sw4_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw4,col,row,1,1);
      g_signal_connect (sw4, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW4));
      col++;
  
      GtkWidget *sw5=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW5]]);
      GtkWidget *sw5_label=gtk_bin_get_child(GTK_BIN(sw5));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW5]]);
      gtk_label_set_markup (GTK_LABEL(sw5_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw5,col,row,1,1);
      g_signal_connect (sw5, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW5));
      col++;
  
      GtkWidget *sw6=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW6]]);
      GtkWidget *sw6_label=gtk_bin_get_child(GTK_BIN(sw6));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW6]]);
      gtk_label_set_markup (GTK_LABEL(sw6_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw6,col,row,1,1);
      g_signal_connect (sw6, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW6));
      col++;
  
      GtkWidget *sw14=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW14]]);
      GtkWidget *sw14_label=gtk_bin_get_child(GTK_BIN(sw14));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW14]]);
      gtk_label_set_markup (GTK_LABEL(sw14_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw14,col,row,1,1);
      g_signal_connect (sw14, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW14));
      col++;
  
      GtkWidget *sw15=gtk_button_new_with_label(sw_string[sw_action[CONTROLLER2_SW15]]);
      GtkWidget *sw15_label=gtk_bin_get_child(GTK_BIN(sw15));
      sprintf(label,"<span size=\"smaller\">%s</span>",sw_string[sw_action[CONTROLLER2_SW15]]);
      gtk_label_set_markup (GTK_LABEL(sw15_label), label);
      gtk_grid_attach(GTK_GRID(grid),sw15,col,row,1,1);
      g_signal_connect (sw15, "button_press_event", G_CALLBACK(sw_cb), GINT_TO_POINTER(CONTROLLER2_SW15));
      col++;
      }
      break;
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);
}
