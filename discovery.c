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
#include "old_discovery.h"
#include "new_discovery.h"
#ifdef SOAPYSDR
#include "soapy_discovery.h"
#endif
#include "main.h"
#include "radio.h"
#ifdef USBOZY
#include "ozyio.h"
#endif
#ifdef STEMLAB_DISCOVERY
#include "stemlab_discovery.h"
#endif
#include "ext.h"
#ifdef GPIO
#include "actions.h"
#include "gpio.h"
#include "configure.h"
#endif
#include "protocols.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "property.h"

static GtkWidget *discovery_dialog;
static DISCOVERED *d;

#ifdef STEMLAB_DISCOVERY
static GtkWidget *apps_combobox[MAX_DEVICES];
#endif

GtkWidget *tcpaddr;
#define IPADDR_LEN 20
static char ipaddr_tcp_buf[IPADDR_LEN] = "10.10.10.10";
char *ipaddr_tcp = &ipaddr_tcp_buf[0];

#ifdef CLIENT_SERVER
GtkWidget *host_addr_entry;
static char host_addr_buffer[128]="g0orx.ddns.net";
char *host_addr = &host_addr_buffer[0];
GtkWidget *host_port_spinner;
gint host_port=45000;
#endif

//
// This is a variable for the second phase of STEMlab discovery.
// Here we know we have a stemlab and will re-discover only P1
//
static int discover_only_p1 = 0;

static gboolean delete_event_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  _exit(0);
}

static gboolean start_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  radio=(DISCOVERED *)data;
#ifdef STEMLAB_DISCOVERY
  // We need to start the STEMlab app before destroying the dialog, since
  // we otherwise lose the information about which app has been selected.
  if (radio->protocol == STEMLAB_PROTOCOL) {
    const int device_id = radio - discovered;
    int ret;
    if (radio->software_version & BARE_REDPITAYA) {
	// Start via the simple web interface
	ret=alpine_start_app(gtk_combo_box_get_active_id(GTK_COMBO_BOX(apps_combobox[device_id])));
    } else {
	// Start via the STEMlab "bazaar" interface
	ret=stemlab_start_app(gtk_combo_box_get_active_id(GTK_COMBO_BOX(apps_combobox[device_id])));
    }
    //
    // To make this bullet-proof, we do another "discover" now
    // and proceeding this way is the only way to choose between UDP and TCP connection
    // Since we have just started the app, we temporarily deactivate STEMlab detection
    //
    stemlab_cleanup();
    sleep(2);          // let Stemlab SDR app start
    discover_only_p1=1;
    gtk_widget_destroy(discovery_dialog);
    g_idle_add(ext_discovery,NULL);
    return TRUE;
  }
#endif
  gtk_widget_destroy(discovery_dialog);
  start_radio();
  return TRUE;
}

static gboolean protocols_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  configure_protocols(discovery_dialog);
  return TRUE;
}

#ifdef GPIO
static gboolean gpio_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  configure_gpio(discovery_dialog);
  return TRUE;
}

static void gpio_changed_cb(GtkWidget *widget, gpointer data) {
  controller=gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  gpio_set_defaults(controller);
  gpio_save_state();
}
#endif

static gboolean discover_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(discovery_dialog);
  g_idle_add(ext_discovery,NULL);
  return TRUE;
}

static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(discovery_dialog);
  _exit(0);
  return TRUE;
}

static gboolean tcp_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
    strncpy(ipaddr_tcp, gtk_entry_get_text(GTK_ENTRY(tcpaddr)), IPADDR_LEN);
    ipaddr_tcp[IPADDR_LEN-1]=0;
    // remove possible trailing newline chars in ipaddr_tcp
	    int len=strnlen(ipaddr_tcp,IPADDR_LEN);
	    while (--len >= 0) {
	      if (ipaddr_tcp[len] != '\n') break;
	      ipaddr_tcp[len]=0;
	    }
	    //fprintf(stderr,"New TCP addr = %s.\n", ipaddr_tcp);
	    // save this value to config file
	    FILE *fp = fopen("ip.addr", "w");
	    if (fp) {
		fprintf(fp,"%s\n",ipaddr_tcp);
		fclose(fp);
	    }
	    gtk_widget_destroy(discovery_dialog);
	    g_idle_add(ext_discovery,NULL);
	    return TRUE;
	}

#ifdef CLIENT_SERVER
static gboolean connect_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  // connect to remote host running piHPSDR
  strncpy(host_addr, gtk_entry_get_text(GTK_ENTRY(host_addr_entry)), 30);
  host_port=gtk_spin_button_get_value(GTK_SPIN_BUTTON(host_port_spinner));
g_print("connect_cb: %s:%d\n",host_addr,host_port);
  setProperty("host",host_addr);
  char temp[16];
  sprintf(temp,"%d",host_port);
  setProperty("port",temp);
  saveProperties("remote.props");
  if(radio_connect_remote(host_addr,host_port)==0) {
    gtk_widget_destroy(discovery_dialog);
  } else {
    // dialog box to display connection error
    GtkWidget *dialog=gtk_dialog_new_with_buttons("Remote Connect",GTK_WINDOW(discovery_dialog),GTK_DIALOG_DESTROY_WITH_PARENT,"OK",GTK_RESPONSE_NONE,NULL);
    GtkWidget *content_area=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    char message[128];
    sprintf(message,"Connection failed to %s:%d",host_addr,host_port);
    GtkWidget *label=gtk_label_new(message);
    g_signal_connect_swapped(dialog,"response",G_CALLBACK(gtk_widget_destroy),dialog);
    gtk_container_add(GTK_CONTAINER(content_area),label);
    gtk_widget_show_all(dialog);
  }
  return TRUE;
}
#endif

void discovery() {
//fprintf(stderr,"discovery\n");

  protocols_restore_state();

  selected_device=0;
  devices=0;

  // Try to locate IP addr
  FILE *fp=fopen("ip.addr","r");
  if (fp) {
    char *c=fgets(ipaddr_tcp, IPADDR_LEN,fp);
    fclose(fp);
    ipaddr_tcp[IPADDR_LEN-1]=0;
    // remove possible trailing newline char in ipaddr_tcp
    int len=strnlen(ipaddr_tcp,IPADDR_LEN);
    while (--len >= 0) {
      if (ipaddr_tcp[len] != '\n') break;
      ipaddr_tcp[len]=0;
    }
  }
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

#ifdef STEMLAB_DISCOVERY
  if(enable_stemlab && ! discover_only_p1) {
#ifdef NO_AVAHI
    status_text("Looking for STEMlab WEB apps");
#else
    status_text("STEMlab (Avahi) ... Discovering Devices");
#endif
    stemlab_discovery();
  }
#endif

  if(enable_protocol_1) {
    status_text("Protocol 1 ... Discovering Devices");
    old_discovery();
  }

  if(enable_protocol_2 && ! discover_only_p1) {
    status_text("Protocol 2 ... Discovering Devices");
    new_discovery();
  }

#ifdef SOAPYSDR
  if(enable_soapy_protocol && ! discover_only_p1) {
    status_text("SoapySDR ... Discovering Devices");
    soapy_discovery();
  }
#endif

  // subsequent discoveries check all protocols enabled.
  discover_only_p1=0;

  status_text("Discovery");
  
    fprintf(stderr,"discovery: found %d devices\n", devices);
    gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));

    discovery_dialog = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(discovery_dialog),GTK_WINDOW(top_window));
    gtk_window_set_title(GTK_WINDOW(discovery_dialog),"piHPSDR - Discovery");
    //gtk_window_set_decorated(GTK_WINDOW(discovery_dialog),FALSE);

    //gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("FreeMono 16"));
    g_signal_connect(discovery_dialog, "delete_event", G_CALLBACK(delete_event_cb), NULL);

    GdkRGBA color;
    color.red = 1.0;
    color.green = 1.0;
    color.blue = 1.0;
    color.alpha = 1.0;
    gtk_widget_override_background_color(discovery_dialog,GTK_STATE_FLAG_NORMAL,&color);

    GtkWidget *content;

    content=gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));

    GtkWidget *grid=gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_row_spacing (GTK_GRID(grid),10);

    int row=0;
    if(devices==0) {
      GtkWidget *label=gtk_label_new("No local devices found!");
      gtk_grid_attach(GTK_GRID(grid),label,0,row,3,1);
      row++;
    } else {
      char version[16];
      char text[256];
      for(row=0;row<devices;row++) {
        d=&discovered[row];
fprintf(stderr,"%p Protocol=%d name=%s\n",d,d->protocol,d->name);
        sprintf(version,"v%d.%d",
                          d->software_version/10,
                          d->software_version%10);
        switch(d->protocol) {
          case ORIGINAL_PROTOCOL:
          case NEW_PROTOCOL:
#ifdef USBOZY
            if(d->device==DEVICE_OZY) {
              sprintf(text,"%s (%s) on USB /dev/ozy", d->name, d->protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2");
            } else {
#endif
              sprintf(text,"%s (%s %s) %s (%02X:%02X:%02X:%02X:%02X:%02X) on %s: ",
                            d->name,
                            d->protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2",
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
#ifdef SOAPYSDR
          case SOAPYSDR_PROTOCOL:
            sprintf(text,"%s (Protocol SOAPY_SDR %s) on %s",d->name,d->info.soapy.version,d->info.soapy.address);
            break;

#endif
#ifdef STEMLAB_DISCOVERY
          case STEMLAB_PROTOCOL:
#ifdef NO_AVAHI
            sprintf(text,"Choose RedPitaya App from %s and re-discover: ",inet_ntoa(d->info.network.address.sin_addr));
#else
            sprintf(text, "STEMlab (%02X:%02X:%02X:%02X:%02X:%02X) on %s",
                           d->info.network.mac_address[0],
                           d->info.network.mac_address[1],
                           d->info.network.mac_address[2],
                           d->info.network.mac_address[3],
                           d->info.network.mac_address[4],
                           d->info.network.mac_address[5],
                           d->info.network.interface_name);
#endif
#endif
        }

        GtkWidget *label=gtk_label_new(text);
        gtk_widget_override_font(label, pango_font_description_from_string("Sans 11"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_show(label);
        gtk_grid_attach(GTK_GRID(grid),label,0,row,3,1);

        GtkWidget *start_button=gtk_button_new_with_label("Start");
        gtk_widget_override_font(start_button, pango_font_description_from_string("Sans 16"));
        gtk_widget_show(start_button);
        gtk_grid_attach(GTK_GRID(grid),start_button,3,row,1,1);
        g_signal_connect(start_button,"button_press_event",G_CALLBACK(start_cb),(gpointer)d);

        // if not available then cannot start it
        if(d->status!=STATE_AVAILABLE) {
          gtk_button_set_label(GTK_BUTTON(start_button),"In Use");
          gtk_widget_set_sensitive(start_button, FALSE);
        }

#ifdef SOAPYSDR
        if(d->device!=SOAPYSDR_USB_DEVICE) {
#endif
          // if not on the same subnet then cannot start it
	  //
	  // DL1YCF: APIPA modification
	  // so-called APIPA addresses are of the numerical form 169.254.xxx.yyy and these addresses are used
	  // by many radios if they do not get a DHCP address. These addresses are valid even if outside the
	  // netmask of the (physical) interface making the connection - so do not complain in this case!
	  //
	  // If the radio has a valid IP address but the computer only has an APIPA address, this also
	  // leads to a radio address outside the netmask and can be ignored.
	  //
          if (strncmp(inet_ntoa(d->info.network.address.sin_addr),"169.254.",8) &&
              strncmp(inet_ntoa(d->info.network.interface_address.sin_addr),"169.254.",8)) {
            if((d->info.network.interface_address.sin_addr.s_addr&d->info.network.interface_netmask.sin_addr.s_addr) != (d->info.network.address.sin_addr.s_addr&d->info.network.interface_netmask.sin_addr.s_addr)) {
              gtk_button_set_label(GTK_BUTTON(start_button),"Subnet!");
              gtk_widget_set_sensitive(start_button, FALSE);
            }
	  }
#ifdef SOAPYSDR
        }
#endif

#ifdef STEMLAB_DISCOVERY
        if (d->protocol == STEMLAB_PROTOCOL) {
          if (d->software_version == 0) {
            gtk_button_set_label(GTK_BUTTON(start_button), "Not installed");
            gtk_widget_set_sensitive(start_button, FALSE);
          } else {
            apps_combobox[row] = gtk_combo_box_text_new();
            gtk_widget_override_font(apps_combobox[row], pango_font_description_from_string("Sans 11"));
            // We want the default selection priority for the STEMlab app to be
            // RP-Trx > HAMlab-Trx > Pavel-Trx > Pavel-Rx, so we add in decreasing order and
            // always set the newly added entry to be active.
            if ((d->software_version & STEMLAB_PAVEL_RX) != 0) {
              gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[row]),
                  "sdr_receiver_hpsdr", "Pavel-Rx");
              gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[row]),
                  "sdr_receiver_hpsdr");
            }
            if ((d->software_version & STEMLAB_PAVEL_TRX) != 0) {
              gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[row]),
                  "sdr_transceiver_hpsdr", "Pavel-Trx");
              gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[row]),
                  "sdr_transceiver_hpsdr");
            }
            if ((d->software_version & HAMLAB_RP_TRX) != 0) {
              gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[row]),
                  "hamlab_sdr_transceiver_hpsdr", "HAMlab-Trx");
              gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[row]),
                  "hamlab_sdr_transceiver_hpsdr");
            }
            if ((d->software_version & STEMLAB_RP_TRX) != 0) {
              gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[row]),
                "stemlab_sdr_transceiver_hpsdr", "STEMlab-Trx");
              gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[row]),
                  "stemlab_sdr_transceiver_hpsdr");
            }
            gtk_widget_show(apps_combobox[row]);
            gtk_grid_attach(GTK_GRID(grid), apps_combobox[row], 4, row, 1, 1);
          }
        }
#endif

      }
    }



#ifdef CLIENT_SERVER

    loadProperties("remote.props");
    char *value;
    value=getProperty("host");
    if(value!=NULL) strcpy(host_addr_buffer,value);
    value=getProperty("port");
    if(value!=NULL) host_port=atoi(value);

    GtkWidget *connect_b=gtk_button_new_with_label("Connect to Server");
    g_signal_connect (connect_b, "button-press-event", G_CALLBACK(connect_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),connect_b,0,row,1,1);

    host_addr_entry=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(host_addr_entry), 30);
    gtk_grid_attach(GTK_GRID(grid),host_addr_entry,1,row,1,1);
    gtk_entry_set_text(GTK_ENTRY(host_addr_entry), host_addr);

    GtkWidget *host_port_label =gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(host_port_label), "<b>Server Port</b>");
    gtk_widget_show(host_port_label);
    gtk_grid_attach(GTK_GRID(grid),host_port_label,2,row,1,1);

    host_port_spinner =gtk_spin_button_new_with_range(45000,55000,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(host_port_spinner),(double)host_port);
    gtk_widget_show(host_port_spinner);
    gtk_grid_attach(GTK_GRID(grid),host_port_spinner,3,row,1,1);

    row++;
#endif

#ifdef GPIO
    controller=NO_CONTROLLER;
    gpio_set_defaults(controller);
    gpio_restore_state();

    GtkWidget *gpio=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio),NULL,"No Controller");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio),NULL,"Controller1");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio),NULL,"Controller2 V1");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio),NULL,"Controller2 V2");
    gtk_grid_attach(GTK_GRID(grid),gpio,0,row,1,1);

    gtk_combo_box_set_active(GTK_COMBO_BOX(gpio),controller);
    g_signal_connect(gpio,"changed",G_CALLBACK(gpio_changed_cb),NULL);
#endif

    GtkWidget *discover_b=gtk_button_new_with_label("Discover");
    g_signal_connect (discover_b, "button-press-event", G_CALLBACK(discover_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),discover_b,1,row,1,1);

    GtkWidget *protocols_b=gtk_button_new_with_label("Protocols");
    g_signal_connect (protocols_b, "button-press-event", G_CALLBACK(protocols_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),protocols_b,2,row,1,1);

    row++;

#ifdef GPIO
    GtkWidget *gpio_b=gtk_button_new_with_label("Configure GPIO");
    g_signal_connect (gpio_b, "button-press-event", G_CALLBACK(gpio_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),gpio_b,0,row,1,1);
#endif

    GtkWidget *tcp_b=gtk_button_new_with_label("Use new TCP Addr:");
    g_signal_connect (tcp_b, "button-press-event", G_CALLBACK(tcp_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),tcp_b,1,row,1,1);

    tcpaddr=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(tcpaddr), 20);
    gtk_grid_attach(GTK_GRID(grid),tcpaddr,2,row,1,1);
    gtk_entry_set_text(GTK_ENTRY(tcpaddr), ipaddr_tcp);

    GtkWidget *exit_b=gtk_button_new_with_label("Exit");
    g_signal_connect (exit_b, "button-press-event", G_CALLBACK(exit_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),exit_b,3,row,1,1);

    gtk_container_add (GTK_CONTAINER (content), grid);
    gtk_widget_show_all(discovery_dialog);
fprintf(stderr,"showing device dialog\n");

    // autostart if one device and autostart enabled
    g_print("%s: devices=%d autostart=%d\n",__FUNCTION__,devices,autostart);

    if(devices==1 && autostart) {
        d=&discovered[0];
	if(d->status==STATE_AVAILABLE) {
          if(start_cb(NULL,NULL,(gpointer)d)) return;
	}
    }
}


