/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wdsp.h>

#include "new_menu.h"
#include "about_menu.h"
#include "discovered.h"
#include "radio.h"
#include "version.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *label;

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

void about_menu(GtkWidget *parent) {
  char text[2048];
  char line[128];
  char addr[64];
  char interface_addr[64];

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - About");
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

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),4);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),4);

  int row=0;

  
  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,row,1,1);
  row++;

  sprintf(text,"piHPSDR by John Melton G0ORX/N6LYT");
  strcat(text,"\n\nWith help from:");
  strcat(text,"\n    Steve Wilson, KA6S: RIGCTL (CAT over TCP)");
  strcat(text,"\n    Laurence Barker, G8NJJ: USB OZY Support");
  strcat(text,"\n    Johan Maas, PA3GSB: RadioBerry support");
  strcat(text,"\n    Ken Hopper, N9VV: Testing and Documentation");
  strcat(text,"\n    Christoph van Wüllen, DL1YCF: CW, Pure Signal, Diversity, MIDI ");

  sprintf(line,"\n\nBuild date: %s", build_date);
  strcat(text,line);

  sprintf(line,"\nBuild version: %s", build_version);
  strcat(text,line);

  sprintf(line,"\nWDSP version: %d.%02d", GetWDSPVersion()/100, GetWDSPVersion()%100);
  strcat(text,line);

  sprintf(line,"\n\nDevice: %s Protocol %s v%d.%d",radio->name,radio->protocol==ORIGINAL_PROTOCOL?"1":"2",radio->software_version/10,radio->software_version%10);
  strcat(text,line);

  switch(radio->protocol) {
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL:
#ifdef USBOZY
      if(radio->device==DEVICE_OZY) {
        sprintf(line,"\nDevice OZY: USB /dev/ozy Protocol %s v%d.%d",radio->protocol==ORIGINAL_PROTOCOL?"1":"2",radio->software_version/10,radio->software_version%10);
        strcat(text,line);
      } else {
#endif
        
        strcpy(addr,inet_ntoa(radio->info.network.address.sin_addr));
        strcpy(interface_addr,inet_ntoa(radio->info.network.interface_address.sin_addr));
        sprintf(line,"\nDevice Mac Address: %02X:%02X:%02X:%02X:%02X:%02X",
                radio->info.network.mac_address[0],
                radio->info.network.mac_address[1],
                radio->info.network.mac_address[2],
                radio->info.network.mac_address[3],
                radio->info.network.mac_address[4],
                radio->info.network.mac_address[5]);
        strcat(text,line);
        sprintf(line,"\nDevice IP Address: %s on %s (%s)",addr,radio->info.network.interface_name,interface_addr);
        strcat(text,line);

#ifdef USBOZY
      }
#endif
      break;
  }

  label=gtk_label_new(text);
  gtk_label_set_justify(GTK_LABEL(label),GTK_JUSTIFY_LEFT);
  gtk_grid_attach(GTK_GRID(grid),label,1,row,4,1);
  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
