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
#include "gpio.h"
#if defined (CONTROLLER2_V2) || defined (CONTROLLER_V1)
#include "i2c.h"
#endif

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *b_af_gain_rx1;
static GtkWidget *b_af_gain_rx2;
static GtkWidget *b_agc_gain_rx1;
static GtkWidget *b_agc_gain_rx2;
static GtkWidget *b_attenuation;
static GtkWidget *b_mic_gain;
static GtkWidget *b_drive;
static GtkWidget *b_tune_drive;
static GtkWidget *b_rit_rx1;
static GtkWidget *b_rit_rx2;
static GtkWidget *b_xit;
static GtkWidget *b_cw_speed;
static GtkWidget *b_cw_frequency;
static GtkWidget *b_panadapter_high;
static GtkWidget *b_panadapter_low;
static GtkWidget *b_squelch;
static GtkWidget *b_compression;

#ifdef CONTROLLER2_V2
static GtkWidget *b_top_af_gain_rx1;
static GtkWidget *b_top_af_gain_rx2;
static GtkWidget *b_top_agc_gain_rx1;
static GtkWidget *b_top_agc_gain_rx2;
static GtkWidget *b_top_attenuation;
static GtkWidget *b_top_mic_gain;
static GtkWidget *b_top_drive;
static GtkWidget *b_top_tune_drive;
static GtkWidget *b_top_rit_rx1;
static GtkWidget *b_top_rit_rx2;
static GtkWidget *b_top_xit;
static GtkWidget *b_top_cw_speed;
static GtkWidget *b_top_cw_frequency;
static GtkWidget *b_top_panadapter_high;
static GtkWidget *b_top_panadapter_low;
static GtkWidget *b_top_squelch;
static GtkWidget *b_top_compression;
#endif

enum {
  ENC2,
#ifdef CONTROLLER2_V2
  ENC2_TOP,
#endif
  ENC2_SW,
  ENC3,
#ifdef CONTROLLER2_V2
  ENC3_TOP,
#endif
  ENC3_SW,
  ENC4,
#ifdef CONTROLLER2_V2
  ENC4_TOP,
#endif
  ENC4_SW,
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  ENC5,
#if defined (CONTROLLER2_V2)
  ENC5_TOP,
#endif
  ENC5_SW,
#endif
};

typedef struct _choice {
  int id;
  int action;
  GtkWidget *button;
} CHOICE;

static int encoder;

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

static void enc_select_cb(GtkWidget *widget,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  switch(choice->id) {
    case ENC2:
      e2_encoder_action=choice->action;
      break;
#ifdef CONTROLLER2_V2
    case ENC2_TOP:
      e2_top_encoder_action=choice->action;
      break;
#endif
    case ENC3:
      e3_encoder_action=choice->action;
      break;
#ifdef CONTROLLER2_V2
    case ENC3_TOP:
      e3_top_encoder_action=choice->action;
      break;
#endif
    case ENC4:
      e4_encoder_action=choice->action;
      break;
#ifdef CONTROLLER2_V2
    case ENC4_TOP:
      e4_top_encoder_action=choice->action;
      break;
#endif
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
    case ENC5:
      e5_encoder_action=choice->action;
      break;
#endif
#if defined (CONTROLLER2_V2)
    case ENC5_TOP:
      e5_top_encoder_action=choice->action;
      break;
#endif
  }
  gtk_button_set_label(GTK_BUTTON(choice->button),encoder_string[choice->action]);
}

static gboolean enc_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int enc=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<ENCODER_ACTIONS;i++) {
    GtkWidget *menu_item=gtk_menu_item_new_with_label(encoder_string[i]);
    CHOICE *choice=g_new0(CHOICE,1);
    choice->id=enc;
    choice->action=i;
    choice->button=widget;
    g_signal_connect(menu_item,"activate",G_CALLBACK(enc_select_cb),choice);
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

static void sw_select_cb(GtkWidget *widget,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  switch(choice->id) {
    case ENC2_SW:
      e2_sw_action=choice->action;
      break;
    case ENC3_SW:
      e3_sw_action=choice->action;
      break;
    case ENC4_SW:
      e4_sw_action=choice->action;
      break;
#ifdef CONTROLLER2_V2
    case ENC5_SW:
      e5_sw_action=choice->action;
      break;
#endif
  }
  gtk_button_set_label(GTK_BUTTON(choice->button),sw_string[choice->action]);
}

static gboolean sw_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int sw=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<SWITCH_ACTIONS;i++) {
    GtkWidget *menu_item=gtk_menu_item_new_with_label(sw_string[i]);
    CHOICE *choice=g_new0(CHOICE,1);
    choice->id=sw;
    choice->action=i;
    choice->button=widget;
    g_signal_connect(menu_item,"activate",G_CALLBACK(sw_select_cb),choice);
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

GtkWidget* getRadioButton(int action) {
  GtkWidget* button;
  switch(action) {
    case ENCODER_AF_GAIN_RX1:
      button=b_af_gain_rx1;
      break;
    case ENCODER_AF_GAIN_RX2:
      button=b_af_gain_rx2;
      break;
    case ENCODER_AGC_GAIN_RX1:
      button=b_agc_gain_rx1;
      break;
    case ENCODER_AGC_GAIN_RX2:
      button=b_agc_gain_rx2;
      break;
    case ENCODER_ATTENUATION:
      button=b_attenuation;
      break;
    case ENCODER_MIC_GAIN:
      button=b_mic_gain;
      break;
    case ENCODER_DRIVE:
      button=b_drive;
      break;
/*
    case ENCODER_TUNE_DRIVE:
      button=b_tune_drive;
      break;
*/
    case ENCODER_RIT_RX1:
      button=b_rit_rx1;
      break;
    case ENCODER_RIT_RX2:
      button=b_rit_rx2;
      break;
    case ENCODER_XIT:
      button=b_xit;
      break;
    case ENCODER_CW_SPEED:
      button=b_cw_speed;
      break;
    case ENCODER_CW_FREQUENCY:
      button=b_cw_frequency;
      break;
    case ENCODER_PANADAPTER_HIGH:
      button=b_panadapter_high;
      break;
    case ENCODER_PANADAPTER_LOW:
      button=b_panadapter_low;
      break;
    case ENCODER_SQUELCH:
      button=b_squelch;
      break;
    case ENCODER_COMP:
      button=b_compression;
      break;
  }
  return button;
}

#ifdef CONTROLLER2_V2
GtkWidget* getTopRadioButton(int action) {
  GtkWidget* button;
  switch(action) {
    case ENCODER_AF_GAIN_RX1:
      button=b_top_af_gain_rx1;
      break;
    case ENCODER_AF_GAIN_RX2:
      button=b_top_af_gain_rx2;
      break;
    case ENCODER_AGC_GAIN_RX1:
      button=b_top_agc_gain_rx1;
      break;
    case ENCODER_AGC_GAIN_RX2:
      button=b_top_agc_gain_rx2;
      break;
    case ENCODER_ATTENUATION:
      button=b_top_attenuation;
      break;
    case ENCODER_MIC_GAIN:
      button=b_top_mic_gain;
      break;
    case ENCODER_DRIVE:
      button=b_top_drive;
      break;
/*
    case ENCODER_TUNE_DRIVE:
      button=b_top_tune_drive;
      break;
*/
    case ENCODER_RIT_RX1:
      button=b_top_rit_rx1;
      break;
    case ENCODER_RIT_RX2:
      button=b_top_rit_rx2;
      break;
    case ENCODER_XIT:
      button=b_top_xit;
      break;
    case ENCODER_CW_SPEED:
      button=b_top_cw_speed;
      break;
    case ENCODER_CW_FREQUENCY:
      button=b_top_cw_frequency;
      break;
    case ENCODER_PANADAPTER_HIGH:
      button=b_top_panadapter_high;
      break;
    case ENCODER_PANADAPTER_LOW:
      button=b_top_panadapter_low;
      break;
    case ENCODER_SQUELCH:
      button=b_top_squelch;
      break;
    case ENCODER_COMP:
      button=b_top_compression;
      break;
  }
  return button;
}
#endif


static gboolean select_cb (GtkWidget *widget, gpointer data) {
  GtkWidget *button;
  int action;

  switch(encoder) {
    case 2:
      action=e2_encoder_action;
      break;
    case 3:
      action=e3_encoder_action;
      break;
    case 4:
      action=e4_encoder_action;
      break;
    case 5:
      action=e5_encoder_action;
      break;
  }
  button=getRadioButton(action);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
}

#ifdef CONTROLLER2_V2
static gboolean top_select_cb (GtkWidget *widget, gpointer data) {
  GtkWidget *button;
  int action;

  switch(encoder) {
    case 2:
      action=e2_top_encoder_action;
      break;
    case 3:
      action=e3_top_encoder_action;
      break;
    case 4:
      action=e4_top_encoder_action;
      break;
    case 5:
      action=e5_top_encoder_action;
      break;
  }
  button=getTopRadioButton(action);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
}
#endif

void encoder_select(int pos) {
  int action;
  GtkWidget *button;
  switch(encoder) {
    case 2:
        if(pos>0) {
          e2_encoder_action--;
          if(e2_encoder_action<0) {
            e2_encoder_action=ENCODER_ACTIONS-1;
          }
        } if(pos<0) {
          e2_encoder_action++;
          if(e2_encoder_action>=ENCODER_ACTIONS) {
            e2_encoder_action=0;
          }
        }
        action=e2_encoder_action;
      break;
    case 3:
        if(pos>0) {
          e3_encoder_action--;
          if(e3_encoder_action<0) {
            e3_encoder_action=ENCODER_ACTIONS-1;
          }
        } if(pos<0) {
          e3_encoder_action++;
          if(e3_encoder_action>=ENCODER_ACTIONS) {
            e3_encoder_action=0;
          }
        }
        action=e3_encoder_action;
      break;
    case 4:
        if(pos>0) {
          e4_encoder_action--;
          if(e4_encoder_action<0) {
            e4_encoder_action=ENCODER_ACTIONS-1;
          }
        } if(pos<0) {
          e4_encoder_action++;
          if(e4_encoder_action>=ENCODER_ACTIONS) {
            e4_encoder_action=0;
          }
        }
        action=e4_encoder_action;
      break;
  }

  button=getRadioButton(action);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

}

#ifdef CONTROLLER2_V2
void top_encoder_select(int pos) {
  int action;
  GtkWidget *button;
  switch(encoder) {
    case 2:
        if(pos>0) {
          e2_top_encoder_action--;
          if(e2_top_encoder_action<0) {
            e2_top_encoder_action=ENCODER_ACTIONS-1;
          }
        } if(pos<0) {
          e2_top_encoder_action++;
          if(e2_top_encoder_action>=ENCODER_ACTIONS) {
            e2_top_encoder_action=0;
          }
        }
        action=e2_top_encoder_action;
      break;
    case 3:
        if(pos>0) {
          e3_top_encoder_action--;
          if(e3_top_encoder_action<0) {
            e3_top_encoder_action=ENCODER_ACTIONS-1;
          }
        } if(pos<0) {
          e3_top_encoder_action++;
          if(e3_top_encoder_action>=ENCODER_ACTIONS) {
            e3_top_encoder_action=0;
          }
        }
        action=e3_top_encoder_action;
      break;
    case 4:
        if(pos>0) {
          e4_top_encoder_action--;
          if(e4_top_encoder_action<0) {
            e4_top_encoder_action=ENCODER_ACTIONS-1;
          }
        } if(pos<0) {
          e4_top_encoder_action++;
          if(e4_top_encoder_action>=ENCODER_ACTIONS) {
            e4_top_encoder_action=0;
          }
        }
        action=e4_top_encoder_action;
      break;
    case 5:
        if(pos>0) {
          e5_top_encoder_action--;
          if(e5_top_encoder_action<0) {
            e5_top_encoder_action=ENCODER_ACTIONS-1;
          }
        } if(pos<0) {
          e5_top_encoder_action++;
          if(e5_top_encoder_action>=ENCODER_ACTIONS) {
            e5_top_encoder_action=0;
          }
        }
        action=e5_top_encoder_action;
      break;
  }

  button=getTopRadioButton(action);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

}
#endif

  static gboolean action_select_cb (GtkWidget *widget, gpointer data) {
    int action=(int)data;
    switch(encoder) {
      case 2:
        e2_encoder_action=action;
        break;
      case 3:
        e3_encoder_action=action;
        break;
      case 4:
        e4_encoder_action=action;
        break;
      case 5:
        e5_encoder_action=action;
        break;
    }
  }

#ifdef CONTROLLER2_V2
  static gboolean top_action_select_cb (GtkWidget *widget, gpointer data) {
    int action=(int)data;
    switch(encoder) {
      case 2:
        e2_top_encoder_action=action;
        break;
      case 3:
        e3_top_encoder_action=action;
        break;
      case 4:
        e4_top_encoder_action=action;
        break;
      case 5:
        e5_top_encoder_action=action;
        break;
    }
  }
#endif

  static gboolean enc2_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e2_encoder_action=i;
    return TRUE;
  }

  static gboolean enc2_sw_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e2_sw_action=i;
    return TRUE;
  }

  static gboolean enc3_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e3_encoder_action=i;
    return TRUE;
  }

  static gboolean enc3_sw_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e3_sw_action=i;
    return TRUE;
  }

  static gboolean enc4_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e4_encoder_action=i;
    return TRUE;
  }

  static gboolean enc4_sw_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e4_sw_action=i;
    return TRUE;
  }

#ifdef CONTROLLER2_V2
  static gboolean enc2_top_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e2_top_encoder_action=i;
    return TRUE;
  }

  static gboolean enc3_top_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e3_top_encoder_action=i;
    return TRUE;
  }

  static gboolean enc4_top_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e4_top_encoder_action=i;
    return TRUE;
  }

  static gboolean enc5_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e5_encoder_action=i;
    return TRUE;
  }
  static gboolean enc5_sw_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e5_sw_action=i;
    return TRUE;
  }


  static gboolean enc5_top_cb(GtkWidget *widget, gpointer data) {
    int i=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
    e5_top_encoder_action=i;
    return TRUE;
  }
#endif

void encoder_menu(GtkWidget *parent) {
  int row=0;
  int col=0;
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

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  row++;
  col=1;
#ifdef CONTROLLER2_V2
#ifdef CONTROLLER2_V1
  GtkWidget *label_encoder=gtk_label_new("Encoder");
  gtk_grid_attach(GTK_GRID(grid),label_encoder,col,row,1,1);
  col++;
#else
  GtkWidget *label_bottom=gtk_label_new("Bottom");
  gtk_grid_attach(GTK_GRID(grid),label_bottom,col,row,1,1);
  col++;
  GtkWidget *label_top=gtk_label_new("Top");
  gtk_grid_attach(GTK_GRID(grid),label_top,col,row,1,1);
  col++;
#endif
#else
  GtkWidget *label_encoder=gtk_label_new("Encoder");
  gtk_grid_attach(GTK_GRID(grid),label_encoder,col,row,1,1);
  col++;
#endif
  GtkWidget *label_switch=gtk_label_new("Switch");
  gtk_grid_attach(GTK_GRID(grid),label_switch,col,row,1,1);
  col++;

  row++;
  col=0;

  GtkWidget *enc2_title=gtk_label_new("ENC2: ");
  gtk_grid_attach(GTK_GRID(grid),enc2_title,col,row,1,1);
  col++;

  GtkWidget *enc2=gtk_button_new_with_label(encoder_string[e2_encoder_action]);
  gtk_grid_attach(GTK_GRID(grid),enc2,col,row,1,1);
  g_signal_connect(enc2,"button_press_event",G_CALLBACK(enc_cb),GINT_TO_POINTER(ENC2));
  col++;
  
#ifdef CONTROLLER2_V2
  GtkWidget *enc2_top=gtk_button_new_with_label(encoder_string[e2_top_encoder_action]);
  gtk_grid_attach(GTK_GRID(grid),enc2_top,col,row,1,1);
  g_signal_connect(enc2_top,"button_press_event",G_CALLBACK(enc_cb),GINT_TO_POINTER(ENC2_TOP));
  col++;
#endif

  GtkWidget *enc2_sw=gtk_button_new_with_label(sw_string[e2_sw_action]);
  gtk_grid_attach(GTK_GRID(grid),enc2_sw,col,row,1,1);
  g_signal_connect(enc2_sw,"button_press_event",G_CALLBACK(sw_cb),GINT_TO_POINTER(ENC2_SW));

  row++;
  col=0;

  GtkWidget *enc3_title=gtk_label_new("ENC3: ");
  gtk_grid_attach(GTK_GRID(grid),enc3_title,col,row,1,1);
  col++;

  GtkWidget *enc3=gtk_button_new_with_label(encoder_string[e3_encoder_action]);
  gtk_grid_attach(GTK_GRID(grid),enc3,col,row,1,1);
  g_signal_connect(enc3,"button_press_event",G_CALLBACK(enc_cb),GINT_TO_POINTER(ENC3));
  col++;
  
#ifdef CONTROLLER2_V2
  GtkWidget *enc3_top=gtk_button_new_with_label(encoder_string[e3_top_encoder_action]);
  gtk_grid_attach(GTK_GRID(grid),enc3_top,col,row,1,1);
  g_signal_connect(enc3_top,"button_press_event",G_CALLBACK(enc_cb),GINT_TO_POINTER(ENC3_TOP));
  col++;
#endif

  GtkWidget *enc3_sw=gtk_button_new_with_label(sw_string[e3_sw_action]);
  gtk_grid_attach(GTK_GRID(grid),enc3_sw,col,row,1,1);
  g_signal_connect(enc3_sw,"button_press_event",G_CALLBACK(sw_cb),GINT_TO_POINTER(ENC3_SW));

  row++;
  col=0;

  GtkWidget *enc4_title=gtk_label_new("ENC4: ");
  gtk_grid_attach(GTK_GRID(grid),enc4_title,col,row,1,1);
  col++;

  GtkWidget *enc4=gtk_button_new_with_label(encoder_string[e4_encoder_action]);
  gtk_grid_attach(GTK_GRID(grid),enc4,col,row,1,1);
  g_signal_connect(enc4,"button_press_event",G_CALLBACK(enc_cb),GINT_TO_POINTER(ENC4));
  col++;
  
#ifdef CONTROLLER2_V2
  GtkWidget *enc4_top=gtk_button_new_with_label(encoder_string[e4_top_encoder_action]);
  gtk_grid_attach(GTK_GRID(grid),enc4_top,col,row,1,1);
  g_signal_connect(enc4_top,"button_press_event",G_CALLBACK(enc_cb),GINT_TO_POINTER(ENC4_TOP));
  col++;
#endif

  GtkWidget *enc4_sw=gtk_button_new_with_label(sw_string[e4_sw_action]);
  gtk_grid_attach(GTK_GRID(grid),enc4_sw,col,row,1,1);
  g_signal_connect(enc4_sw,"button_press_event",G_CALLBACK(sw_cb),GINT_TO_POINTER(ENC4_SW));

  row++;
  col=0;

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
  GtkWidget *enc5_title=gtk_label_new("ENC5: ");
  gtk_grid_attach(GTK_GRID(grid),enc5_title,col,row,1,1);
  col++;

  GtkWidget *enc5=gtk_button_new_with_label(encoder_string[e5_encoder_action]);
  gtk_grid_attach(GTK_GRID(grid),enc5,col,row,1,1);
  g_signal_connect(enc5,"button_press_event",G_CALLBACK(enc_cb),GINT_TO_POINTER(ENC5));
  col++;
  
#if defined (CONTROLLER2_V2)
  GtkWidget *enc5_top=gtk_button_new_with_label(encoder_string[e5_top_encoder_action]);
  gtk_grid_attach(GTK_GRID(grid),enc5_top,col,row,1,1);
  g_signal_connect(enc5_top,"button_press_event",G_CALLBACK(enc_cb),GINT_TO_POINTER(ENC5_TOP));
  col++;
#endif

  GtkWidget *enc5_sw=gtk_button_new_with_label(sw_string[e5_sw_action]);
  gtk_grid_attach(GTK_GRID(grid),enc5_sw,col,row,1,1);
  g_signal_connect(enc5_sw,"button_press_event",G_CALLBACK(sw_cb),GINT_TO_POINTER(ENC5_SW));
#endif

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);
}
