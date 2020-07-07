/* Copyright (C)
* 2020 - John Melton, G0ORX/N6LYT
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
#include "protocols.h"
#include "property.h"


static GtkWidget *dialog;

gboolean enable_protocol_1;
gboolean enable_protocol_2;
#ifdef SOAPYSDR
gboolean enable_soapy_protocol;
#endif
#ifdef STEMLAB_DISCOVERY
gboolean enable_stemlab;
#endif
gboolean autostart;

void protocols_save_state() {
  char value[80];

  clearProperties();
  sprintf(value,"%d",enable_protocol_1);
  setProperty("enable_protocol_1",value);
  sprintf(value,"%d",enable_protocol_2);
  setProperty("enable_protocol_2",value);
#ifdef SOAPYSDR
  sprintf(value,"%d",enable_soapy_protocol);
  setProperty("enable_soapy_protocol",value);
#endif
#ifdef STEMLAB_DISCOVERY
  sprintf(value,"%d",enable_stemlab);
  setProperty("enable_stemlab",value);
#endif

  sprintf(value,"%d",autostart);
  setProperty("autostart",value);
  saveProperties("protocols.props");
  
}

void protocols_restore_state() {
  char *value;

  loadProperties("protocols.props");
  enable_protocol_1=TRUE;
  value=getProperty("enable_protocol_1");
  if(value) enable_protocol_1=atoi(value);
  enable_protocol_2=TRUE;
  value=getProperty("enable_protocol_2");
  if(value) enable_protocol_2=atoi(value);
#ifdef SOAPYSDR
  enable_soapy_protocol=TRUE;
  value=getProperty("enable_soapy_protocol");
  if(value) enable_soapy_protocol=atoi(value);
#endif
#ifdef STEMLAB_DISCOVERY
  enable_stemlab=TRUE;
  value=getProperty("enable_stemlab");
  if(value) enable_stemlab=atoi(value);
#endif
  autostart=FALSE;
  value=getProperty("autostart");
  if(value) autostart=atoi(value);

  clearProperties();
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if(dialog!=NULL) {
    protocols_save_state();
    gtk_widget_destroy(dialog);
  }
  return TRUE;
}

static void protocol_1_cb(GtkToggleButton *widget, gpointer data) {
  enable_protocol_1=gtk_toggle_button_get_active(widget);
}

static void protocol_2_cb(GtkToggleButton *widget, gpointer data) {
  enable_protocol_2=gtk_toggle_button_get_active(widget);
}

#ifdef SOAPYSDR
static void soapy_protocol_cb(GtkToggleButton *widget, gpointer data) {
  enable_soapy_protocol=gtk_toggle_button_get_active(widget);
}
#endif

#ifdef STEMLAB_DISCOVERY
static void stemlab_cb(GtkToggleButton *widget, gpointer data) {
  enable_stemlab=gtk_toggle_button_get_active(widget);
}
#endif

static void autostart_cb(GtkToggleButton *widget, gpointer data) {
  autostart=gtk_toggle_button_get_active(widget);
}

void configure_protocols(GtkWidget *parent) {
  int row;

  dialog=gtk_dialog_new_with_buttons("Protocols",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  row=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,row,1,1);
  row++;

  GtkWidget *b_enable_protocol_1=gtk_check_button_new_with_label("Enable Protocol 1");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_protocol_1), enable_protocol_1);
  gtk_widget_show(b_enable_protocol_1);
  g_signal_connect(b_enable_protocol_1,"toggled",G_CALLBACK(protocol_1_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),b_enable_protocol_1,0,row,1,1);
  row++;

  GtkWidget *b_enable_protocol_2=gtk_check_button_new_with_label("Enable Protocol 2");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_protocol_2), enable_protocol_2);
  gtk_widget_show(b_enable_protocol_2);
  g_signal_connect(b_enable_protocol_2,"toggled",G_CALLBACK(protocol_2_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),b_enable_protocol_2,0,row,1,1);
  row++;

#ifdef SOAPYSDR
  GtkWidget *b_enable_soapy_protocol=gtk_check_button_new_with_label("Enable SoapySDR Protocol");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_soapy_protocol), enable_soapy_protocol);
  gtk_widget_show(b_enable_soapy_protocol);
  g_signal_connect(b_enable_soapy_protocol,"toggled",G_CALLBACK(soapy_protocol_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),b_enable_soapy_protocol,0,row,1,1);
  row++;
#endif

#ifdef STEMLAB_DISCOVERY
  GtkWidget *b_enable_stemlab=gtk_check_button_new_with_label("Enable Stemlab");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_stemlab), enable_stemlab);
  gtk_widget_show(b_enable_stemlab);
  g_signal_connect(b_enable_stemlab,"toggled",G_CALLBACK(stemlab_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),b_enable_stemlab,0,row,1,1);
  row++;
#endif

  GtkWidget *b_autostart=gtk_check_button_new_with_label("Auto start if only one device");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_autostart), autostart);
  gtk_widget_show(b_autostart);
  g_signal_connect(b_autostart,"toggled",G_CALLBACK(autostart_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),b_autostart,0,row,1,1);
  row++;

  gtk_container_add(GTK_CONTAINER(content),grid);

  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));

}

