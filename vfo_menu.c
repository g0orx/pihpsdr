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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <wdsp.h>

#include "new_menu.h"
#include "band.h"
#include "filter.h"
#include "mode.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include "button_text.h"
#include "ext.h"

static GtkWidget *parent_window=NULL;
static gint v;
static GtkWidget *dialog=NULL;
static GtkWidget *label;

#define BUF_SIZE 88

static char *btn_labels[] = {"1","2","3",
               "4","5","6",
               "7","8","9",
               ".","0","BS",
               "Hz","kHz","MHz"
               ,"CL"
              };

static GtkWidget *btn[16];

static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
    active_menu=NO_MENU;
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

static void squelch_enable_cb(GtkWidget *widget, gpointer data) {
  active_receiver->squelch_enable=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  setSquelch(active_receiver);
}

static gboolean freqent_select_cb (GtkWidget *widget, gpointer data) {
  char *str = (char *) data;
  const char *labelText;
  char output[BUF_SIZE+12], buffer[BUF_SIZE];
  int  len;
  double  mult;
  long long f;
  static int set = 0;

  // Instead of messing with LOCALE settings,
  // we print a "0.0" and look what the decimal
  // point is. What comes out of sprintf should be
  // OK for atof.
  char decimalpoint;
  mult=0.0;
  sprintf(output,"%1.1f",mult);
  decimalpoint=output[1];


  if (set) {
    set = 0;
    strcpy (buffer, "0");
    sprintf(output, "<big>%s</big>", buffer);
    gtk_label_set_markup (GTK_LABEL (label), output);
    len = 1;
  } else {
    labelText = gtk_label_get_text (GTK_LABEL (label));
    strcpy (buffer, labelText);
    len = strlen (buffer);
  }

  if (isdigit (str[0]) || str[0] == '.') {

    // substitute decimal point by the LOCALE character for the decimal point
    // otherwise atof() does not understand it.
    if (str[0] == '.') {
      buffer[len] = (gchar) decimalpoint;
    } else {
      buffer[len] = (gchar) str[0];
    }
    buffer[len+1] = (gchar) 0;

    len = (buffer[0] == '0') ? 1 : 0;

    sprintf(output, "<big>%s</big>", buffer+len);
    gtk_label_set_markup (GTK_LABEL (label), output);
  } else {

    if (strcmp (str, "BS") == 0) {
      /* --- Remove the last character on it. --- */
      if (len > 0) buffer[len-1] = (gchar) 0;

      /* --- Remove digit from field. --- */
      sprintf(output, "<big>%s</big>", buffer);
      gtk_label_set_markup (GTK_LABEL (label), output);

    /* --- clear? --- */
    mult=0.0;
    } else if (strcmp (str, "CL") == 0) {
      strcpy (buffer, "0");
      sprintf(output, "<big>%s</big>", buffer);
      gtk_label_set_markup (GTK_LABEL (label), output);
    } else if(strcmp(str,"Hz")==0) {
      mult = 10.0;
    } else if(strcmp(str,"kHz")==0) {
      mult = 10000.0;
    } else if(strcmp(str,"MHz")==0) {
      mult = 10000000.0;
    }
    if(mult!=0.0) {
      f = ((long long)(atof(buffer)*mult)+5)/10;
      sprintf(output, "<big>%lld</big>", f);
      gtk_label_set_markup (GTK_LABEL (label), output);
#ifdef CLIENT_SERVER
      if(radio_is_remote) {
        send_vfo_frequency(client_socket,active_receiver->id,f);
      } else {
#endif
        //This is inside a callback so we do not need g_idle_add
        //fp=g_new(SET_FREQUENCY,1);
        //fp->vfo=v;
        //fp->frequency = f;
        //g_idle_add(ext_set_frequency, fp);
        set_frequency(v, f);
#ifdef CLIENT_SERVER
      }
#endif
      set = 1;
    }
  }
  g_idle_add(ext_vfo_update,NULL);
  return FALSE;
}

static void rit_cb(GtkComboBox *widget,gpointer data) {
  switch(gtk_combo_box_get_active(widget)) {
    case 0:
      rit_increment=1;
      break;
    case 1:
      rit_increment=10;
      break;
    case 2:
      rit_increment=100;
      break;
  }
  g_idle_add(ext_vfo_update,NULL);
}

static void vfo_cb(GtkComboBox *widget,gpointer data) {
  int i=gtk_combo_box_get_active(widget);
  vfo_set_step_from_index(i);
  g_idle_add(ext_vfo_update,NULL);
}

#ifdef PURESIGNAL
static void enable_ps_cb(GtkWidget *widget, gpointer data) {
  tx_set_ps(transmitter,gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}
#endif

static void set_btn_state() {
  int i;

  for(i=0;i<16;i++) {
    gtk_widget_set_sensitive(btn[i], locked==0);
  }
}

static void lock_cb(GtkWidget *widget, gpointer data) {
  locked=locked==1?0:1;
  set_btn_state();
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_menu(GtkWidget *parent,int vfo) {
  int i;

  parent_window=parent;
  v=vfo;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - VFO %s",vfo==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),4);
  gtk_grid_set_row_spacing (GTK_GRID(grid),4);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,2,1);

  GtkWidget *lock_b=gtk_check_button_new_with_label("Lock VFO");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (lock_b), locked);
  gtk_grid_attach(GTK_GRID(grid),lock_b,2,0,2,1);
  g_signal_connect(lock_b,"toggled",G_CALLBACK(lock_cb),NULL);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<big>0</big>");
  gtk_misc_set_alignment (GTK_MISC (label), 1, .5);
  gtk_grid_attach(GTK_GRID(grid),label,0,1,3,1);

  for (i=0; i<16; i++) {
    btn[i]=gtk_button_new_with_label(btn_labels[i]);
    set_button_text_color(btn[i],"black");
    gtk_widget_show(btn[i]);
    gtk_grid_attach(GTK_GRID(grid),btn[i],i%3,2+(i/3),1,1);
    g_signal_connect(btn[i],"pressed",G_CALLBACK(freqent_select_cb),(gpointer *)btn_labels[i]);
  }
  set_btn_state();

  GtkWidget *rit_label=gtk_label_new("RIT step: ");
  gtk_grid_attach(GTK_GRID(grid),rit_label,3,2,1,1);

  GtkWidget *rit_b=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_b),NULL,"1 Hz");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_b),NULL,"10 Hz");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_b),NULL,"100 Hz");
  switch(rit_increment) {
    case 1:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_b), 0);
      break;
    case 10:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_b), 1);
      break;
    case 100:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_b), 2);
      break;
  }
  g_signal_connect(rit_b,"changed",G_CALLBACK(rit_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),rit_b,4,2,1,1);

  GtkWidget *vfo_label=gtk_label_new("VFO step: ");
  gtk_grid_attach(GTK_GRID(grid),vfo_label,3,3,1,1);

  GtkWidget *vfo_b=gtk_combo_box_text_new();
  for (i=0; i<STEPS; i++) {
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(vfo_b),NULL,step_labels[i]);
    if(steps[i]==step) {
      gtk_combo_box_set_active (GTK_COMBO_BOX(vfo_b), i);
    }
  }
  g_signal_connect(vfo_b,"changed",G_CALLBACK(vfo_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),vfo_b,4,3,1,1);


  GtkWidget *enable_squelch=gtk_check_button_new_with_label("Enable Squelch");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_squelch), active_receiver->squelch_enable);
  gtk_grid_attach(GTK_GRID(grid),enable_squelch,3,5,1,1);
  g_signal_connect(enable_squelch,"toggled",G_CALLBACK(squelch_enable_cb),NULL);

#ifdef PURESIGNAL
  if(can_transmit && (protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL)) {
    GtkWidget *enable_ps=gtk_check_button_new_with_label("Enable Pure Signal");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_ps), transmitter->puresignal);
    gtk_grid_attach(GTK_GRID(grid),enable_ps,3,7,1,1);
    g_signal_connect(enable_ps,"toggled",G_CALLBACK(enable_ps_cb),NULL);
  }
#endif

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
