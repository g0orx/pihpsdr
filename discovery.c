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

#include "discovered.h"
#include "discovery.h"
#include "main.h"
#include "radio.h"
#ifdef USBOZY
#include "ozyio.h"
#endif
#ifdef RADIOBERRY
#include "radioberry_discovery.h"
#endif
#ifdef RADIOBERRY
#include "radioberry.h"
#endif

static GtkWidget *discovery_dialog;
static DISCOVERED *d;

int discovery(void *data);

static gboolean start_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
fprintf(stderr,"start_cb: %p\n",data);
  radio=(DISCOVERED *)data;
  gtk_widget_destroy(discovery_dialog);
  start_radio();
  return TRUE;
}

#ifdef GPIO
static gboolean gpio_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  configure_gpio(discovery_dialog);
  return TRUE;
}
#endif

static gboolean discover_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(discovery_dialog);
  g_idle_add(discovery,NULL);
  return TRUE;
}

static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(discovery_dialog);
  _exit(0);
  return TRUE;
}

int discovery(void *data) {
fprintf(stderr,"discovery\n");
  selected_device=0;
  devices=0;

#ifdef USBOZY
//
// first: look on USB for an Ozy
//
  fprintf(stderr,"looking for USB based OZY devices\n");

  if (ozy_discover() != 0)
  {
    discovered[devices].protocol = ORIGINAL_PROTOCOL;
    discovered[devices].device = DEVICE_OZY;
    discovered[devices].software_version = 10;              // we can't know yet so this isn't a real response
    discovered[devices].status = STATE_AVAILABLE;
    strcpy(discovered[devices].name,"Ozy on USB");

    strcpy(discovered[devices].info.network.interface_name,"USB");
    devices++;
  }
#endif


  status_text("Old Protocol ... Discovering Devices");
  old_discovery();

  status_text("New Protocol ... Discovering Devices");
  new_discovery();

#ifdef LIMESDR
  status_text("LimeSDR ... Discovering Devices");
  lime_discovery();
#endif

#ifdef RADIOBERRY
      status_text("Radioberry SDR ... Discovering Device");
      radioberry_discovery();
#endif
  status_text("Discovery");


  if(devices==0) {
    gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));
    discovery_dialog = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(discovery_dialog),GTK_WINDOW(top_window));
    gtk_window_set_decorated(GTK_WINDOW(discovery_dialog),FALSE);

    gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("FreeMono 16"));

#ifdef FORCE_WHITE_MENU
    GdkRGBA color;
    color.red = 1.0;
    color.green = 1.0;
    color.blue = 1.0;
    color.alpha = 1.0;
    gtk_widget_override_background_color(discovery_dialog,GTK_STATE_FLAG_NORMAL,&color);
#endif

    GtkWidget *content;

    content=gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));

    GtkWidget *grid=gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_row_spacing (GTK_GRID(grid),10);

    GtkWidget *label=gtk_label_new("No devices found!");
    gtk_grid_attach(GTK_GRID(grid),label,0,0,2,1);

    GtkWidget *exit_b=gtk_button_new_with_label("Exit");
    g_signal_connect (exit_b, "button-press-event", G_CALLBACK(exit_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),exit_b,0,1,1,1);

    GtkWidget *discover_b=gtk_button_new_with_label("Retry Discovery");
    g_signal_connect (discover_b, "button-press-event", G_CALLBACK(discover_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),discover_b,1,1,1,1);

    gtk_container_add (GTK_CONTAINER (content), grid);
    gtk_widget_show_all(discovery_dialog);
  } else {
    fprintf(stderr,"discovery: found %d devices\n", devices);
    gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));
    discovery_dialog = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(discovery_dialog),GTK_WINDOW(top_window));
    gtk_window_set_decorated(GTK_WINDOW(discovery_dialog),FALSE);

    gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("FreeMono 16"));

#ifdef FORCE_WHITE_MENU
    GdkRGBA color;
    color.red = 1.0;
    color.green = 1.0;
    color.blue = 1.0;
    color.alpha = 1.0;
    gtk_widget_override_background_color(discovery_dialog,GTK_STATE_FLAG_NORMAL,&color);
#endif

    GtkWidget *content;

    content=gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));

    GtkWidget *grid=gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_row_spacing (GTK_GRID(grid),10);

    int i;
    char version[16];
    char text[128];
    for(i=0;i<devices;i++) {
      d=&discovered[i];
fprintf(stderr,"%p protocol=%d name=%s\n",d,d->protocol,d->name);
      if(d->protocol==ORIGINAL_PROTOCOL) {
        sprintf(version,"%d.%d",
                        d->software_version/10,
                        d->software_version%10);
      } else {
        sprintf(version,"%d.%d",
                        d->software_version/10,
                        d->software_version%10);
      }
      switch(d->protocol) {
        case ORIGINAL_PROTOCOL:
        case NEW_PROTOCOL:
#ifdef USBOZY
          if(d->device==DEVICE_OZY) {
            sprintf(text,"%s (%s) on USB /dev/ozy\n", d->name, d->protocol==ORIGINAL_PROTOCOL?"old":"new");
          } else {
#endif
            sprintf(text,"%s (%s %s) %s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n",
                          d->name,
                          d->protocol==ORIGINAL_PROTOCOL?"old":"new",
                          version,
                          inet_ntoa(d->info.network.address.sin_addr),
                          d->info.network.mac_address[0],
                          d->info.network.mac_address[1],
                          d->info.network.mac_address[2],
                          d->info.network.mac_address[3],
                          d->info.network.mac_address[4],
                          d->info.network.mac_address[5],
                          d->info.network.interface_name);
#ifdef USBOZY
          }
#endif
          break;
#ifdef LIMESDR
        case LIMESDR_PROTOCOL:
          sprintf(text,"%s\n",
                        d->name);
          break;
#endif
#ifdef RADIOBERRY
				case RADIOBERRY_PROTOCOL:
					sprintf(text,"%s\n",d->name);
				break;
#endif
      }

      GtkWidget *label=gtk_label_new(text);
      gtk_widget_override_font(label, pango_font_description_from_string("FreeMono 12"));
      gtk_widget_show(label);
      gtk_grid_attach(GTK_GRID(grid),label,0,i,3,1);

      GtkWidget *start_button=gtk_button_new_with_label("Start");
      gtk_widget_override_font(start_button, pango_font_description_from_string("FreeMono 18"));
      gtk_widget_show(start_button);
      gtk_grid_attach(GTK_GRID(grid),start_button,3,i,1,1);
      g_signal_connect(start_button,"button_press_event",G_CALLBACK(start_cb),(gpointer)d);

      // if not available then cannot start it
      if(d->status!=STATE_AVAILABLE) {
        gtk_button_set_label(GTK_BUTTON(start_button),"In Use");
        gtk_widget_set_sensitive(start_button, FALSE);
      }

      // if not on the same subnet then cannot start it
      if((d->info.network.interface_address.sin_addr.s_addr&d->info.network.interface_netmask.sin_addr.s_addr) != (d->info.network.address.sin_addr.s_addr&d->info.network.interface_netmask.sin_addr.s_addr)) {
        gtk_button_set_label(GTK_BUTTON(start_button),"Subnet!");
        gtk_widget_set_sensitive(start_button, FALSE);
      }

    }
#ifdef GPIO
    GtkWidget *gpio_b=gtk_button_new_with_label("Config GPIO");
    g_signal_connect (gpio_b, "button-press-event", G_CALLBACK(gpio_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),gpio_b,0,i,1,1);
#endif
    GtkWidget *discover_b=gtk_button_new_with_label("Discover");
    g_signal_connect (discover_b, "button-press-event", G_CALLBACK(discover_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),discover_b,1,i,1,1);

    GtkWidget *exit_b=gtk_button_new_with_label("Exit");
    g_signal_connect (exit_b, "button-press-event", G_CALLBACK(exit_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),exit_b,2,i,1,1);

    gtk_container_add (GTK_CONTAINER (content), grid);
    gtk_widget_show_all(discovery_dialog);
fprintf(stderr,"showing device dialog\n");
  }

  return 0;

}


