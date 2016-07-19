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
#include "configure.h"
#include "gpio.h"
#include "old_discovery.h"
#include "new_discovery.h"
#ifdef LIMESDR
#include "lime_discovery.h"
#endif
#include "old_protocol.h"
#include "new_protocol.h"
#ifdef LIMESDR
#include "lime_protocol.h"
#endif
#include "wdsp.h"
#include "vfo.h"
#include "menu.h"
#include "meter.h"
#include "panadapter.h"
#include "splash.h"
#include "waterfall.h"
#include "toolbar.h"
#include "sliders.h"
#include "radio.h"
#include "wdsp_init.h"
#include "version.h"
#ifdef FREEDV
#include "mode.h"
#endif

#ifdef raspberrypi
#define INCLUDE_GPIO
#endif
#ifdef odroid
#define INCLUDE_GPIO
#endif

#define VFO_HEIGHT ((display_height/32)*4)
#define VFO_WIDTH ((display_width/32)*16)
#define MENU_HEIGHT VFO_HEIGHT
#define MENU_WIDTH ((display_width/32)*3)
#define METER_HEIGHT VFO_HEIGHT
#define METER_WIDTH ((display_width/32)*13)
#define PANADAPTER_HEIGHT ((display_height/32)*8)
#define SLIDERS_HEIGHT ((display_height/32)*6)
#define TOOLBAR_HEIGHT ((display_height/32)*2)
#define WATERFALL_HEIGHT (display_height-(VFO_HEIGHT+PANADAPTER_HEIGHT+SLIDERS_HEIGHT+TOOLBAR_HEIGHT))

struct utsname unameData;

gint display_width;
gint display_height;
gint full_screen=1;

static gint update_timer_id;

static gint save_timer_id;

static float *samples;

static int start=0;

static GtkWidget *discovery_dialog;

static sem_t wisdom_sem;

static GdkCursor *cursor_arrow;
static GdkCursor *cursor_watch;

gint update(gpointer data) {
    int result;
    double fwd;
    double rev;
    double exciter;
    int channel=CHANNEL_RX0;
    if(isTransmitting()) {
      channel=CHANNEL_TX;
    }
    GetPixels(channel,0,samples,&result);
    if(result==1) {
        if(display_panadapter) {
          panadapter_update(samples,isTransmitting());
        }
        if(!isTransmitting()) {
            if(display_waterfall) {
              waterfall_update(samples);
            }
        }
    }

    if(!isTransmitting()) {
        double m=GetRXAMeter(CHANNEL_RX0, smeter);
        meter_update(SMETER,m,0.0,0.0,0.0);
    } else {

        double alc=GetTXAMeter(CHANNEL_TX, alc);

        DISCOVERED *d=&discovered[selected_device];

        double constant1=3.3;
        double constant2=0.09;

        if(d->protocol==ORIGINAL_PROTOCOL) {
            switch(d->device) {
                case DEVICE_METIS:
                    break;
                case DEVICE_HERMES:
                    //constant2=0.095; HERMES 2
                    break;
                case DEVICE_ANGELIA:
                    break;
                case DEVICE_ORION:
                    constant1=5.0;
                    constant2=0.108;
                    break;
                case DEVICE_HERMES_LITE:
                    break;
            }

            int power=alex_forward_power;
            if(power==0) {
                power=exciter_power;
            }
            double v1;
            v1=((double)power/4095.0)*constant1;
            fwd=(v1*v1)/constant2;

            power=exciter_power;
            v1=((double)power/4095.0)*constant1;
            exciter=(v1*v1)/constant2;

            rev=0.0;
            if(alex_forward_power!=0) {
                power=alex_reverse_power;
                v1=((double)power/4095.0)*constant1;
                rev=(v1*v1)/constant2;
            }
         
        } else {
            switch(d->device) {
                case NEW_DEVICE_ATLAS:
                    constant1=3.3;
                    constant2=0.09;
                    break;
                case NEW_DEVICE_HERMES:
                    constant1=3.3;
                    constant2=0.09;
                    break;
                case NEW_DEVICE_HERMES2:
                    constant1=3.3;
                    constant2=0.095;
                    break;
                case NEW_DEVICE_ANGELIA:
                    constant1=3.3;
                    constant2=0.095;
                    break;
                case NEW_DEVICE_ORION:
                    constant1=5.0;
                    constant2=0.108;
                    break;
                case NEW_DEVICE_ORION2:
                    constant1=5.0;
                    constant2=0.108;
                    break;
                case NEW_DEVICE_HERMES_LITE:
                    constant1=3.3;
                    constant2=0.09;
                    break;
            }
        
            int power=alex_forward_power;
            if(power==0) {
                power=exciter_power;
            }
            double v1;
            v1=((double)power/4095.0)*constant1;
            fwd=(v1*v1)/constant2;

            power=exciter_power;
            v1=((double)power/4095.0)*constant1;
            exciter=(v1*v1)/constant2;

            rev=0.0;
            if(alex_forward_power!=0) {
                power=alex_reverse_power;
                v1=((double)power/4095.0)*constant1;
                rev=(v1*v1)/constant2;
            }
        }

//fprintf(stderr,"drive=%d tune_drive=%d alex_forward_power=%d alex_reverse_power=%d exciter_power=%d fwd=%f rev=%f exciter=%f\n",
//               drive, tune_drive, alex_forward_power, alex_reverse_power, exciter_power, fwd, rev, exciter);
        meter_update(POWER,fwd,rev,exciter,alc);
    }

    return TRUE;
}

static gint save_cb(gpointer data) {
    radioSaveState();
    return TRUE;
}

static void start_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"start_cb\n");
    selected_device=(int)data;
    start=1;
    gtk_widget_destroy(discovery_dialog);
}

static void configure_cb(GtkWidget *widget, gpointer data) {
  DISCOVERED* d;
  d=&discovered[(int)data];
  switch(d->protocol) {
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL:
      sprintf(property_path,"%02X-%02X-%02X-%02X-%02X-%02X.props",
                        d->info.network.mac_address[0],
                        d->info.network.mac_address[1],
                        d->info.network.mac_address[2],
                        d->info.network.mac_address[3],
                        d->info.network.mac_address[4],
                        d->info.network.mac_address[5]);
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      sprintf(property_path,"limesdr.props");
      break;
#endif
  }
  radioRestoreState();
  
  configure(d,splash_window);

}

static pthread_t wisdom_thread_id;

static void* wisdom_thread(void *arg) {
  splash_status("Creating FFTW Wisdom file ...");
  WDSPwisdom ((char *)arg);
  sem_post(&wisdom_sem);
}

gboolean main_delete (GtkWidget *widget) {
#ifdef INCLUDE_GPIO
  gpio_close();
#endif
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      break;
    case NEW_PROTOCOL:
      new_protocol_stop();
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_stop();
      break;
#endif
  }
  radioSaveState();
  _exit(0);
}

gint init(void* arg) {

  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *fixed;
  gint x;
  gint y;
  GtkWidget *vfo;
  GtkWidget *menu;
  GtkWidget *meter;
  GtkWidget *panadapter;
  GtkWidget *waterfall;
  GtkWidget *sliders;
  GtkWidget *toolbar;

  DISCOVERED* d;

  char *res;
  char wisdom_directory[1024];
  char wisdom_file[1024];

  fprintf(stderr,"init\n");

  cursor_arrow=gdk_cursor_new(GDK_ARROW);
  cursor_watch=gdk_cursor_new(GDK_WATCH);

  GdkWindow *gdk_splash_window = gtk_widget_get_window(splash_window);
  gdk_window_set_cursor(gdk_splash_window,cursor_watch);

  init_radio();

  // check if wisdom file exists
  res=getcwd(wisdom_directory, sizeof(wisdom_directory));
  strcpy(&wisdom_directory[strlen(wisdom_directory)],"/");
  strcpy(wisdom_file,wisdom_directory);
  strcpy(&wisdom_file[strlen(wisdom_file)],"wdspWisdom");
  splash_status("Checking FFTW Wisdom file ...");
  if(access(wisdom_file,F_OK)<0) {
      int rc=sem_init(&wisdom_sem, 0, 0);
      rc=pthread_create(&wisdom_thread_id, NULL, wisdom_thread, (void *)wisdom_directory);
      while(sem_trywait(&wisdom_sem)<0) {
        splash_status(wisdom_get_status());
        while (gtk_events_pending ())
          gtk_main_iteration ();
        usleep(100000); // 100ms
      }
  }

  while(!start) {
      gdk_window_set_cursor(gdk_splash_window,cursor_watch);
      selected_device=0;
      devices=0;
      splash_status("Old Protocol ... Discovering Devices");
      old_discovery();
      splash_status("New Protocol ... Discovering Devices");
      new_discovery();
#ifdef LIMESDR
      splash_status("LimeSDR ... Discovering Devices");
      lime_discovery();
#endif
      splash_status("Discovery");
      if(devices==0) {
          gdk_window_set_cursor(gdk_splash_window,cursor_arrow);
          fprintf(stderr,"No devices found!\n");
          GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
          discovery_dialog = gtk_message_dialog_new (GTK_WINDOW(splash_window),
                                 flags,
                                 GTK_MESSAGE_ERROR,
                                 GTK_BUTTONS_OK_CANCEL,
                                 "No devices found! Retry Discovery?");
          gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("Arial 18"));
          gint result=gtk_dialog_run (GTK_DIALOG (discovery_dialog));
          gtk_widget_destroy(discovery_dialog);
          if(result==GTK_RESPONSE_CANCEL) {
               _exit(0);
          }
      } else {
          fprintf(stderr,"%s: found %d devices.\n", (char *)arg, devices);
          gdk_window_set_cursor(gdk_splash_window,cursor_arrow);
          GtkDialogFlags flags=GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
          discovery_dialog = gtk_dialog_new_with_buttons ("Discovered",
                                      GTK_WINDOW(splash_window),
                                      flags,
#ifdef INCLUDE_GPIO
                                      "Configure GPIO",
                                      GTK_RESPONSE_YES,
#endif
                                      "Discover",
                                      GTK_RESPONSE_REJECT,
                                      "Exit",
                                      GTK_RESPONSE_CLOSE,
                                      NULL);

          gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("Arial 18"));
          GtkWidget *content;

          content=gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));

          GtkWidget *grid=gtk_grid_new();
          gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
          gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

          int i;
          char text[128];
          for(i=0;i<devices;i++) {
              d=&discovered[i];
fprintf(stderr,"protocol=%d name=%s\n",d->protocol,d->name);
              switch(d->protocol) {
                case ORIGINAL_PROTOCOL:
                case NEW_PROTOCOL:
                  sprintf(text,"%s (%s %d.%d) %s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n",
                        d->name,
                        d->protocol==ORIGINAL_PROTOCOL?"old":"new",
                        //d->protocol==ORIGINAL_PROTOCOL?d->software_version/10:d->software_version/100,
                        //d->protocol==ORIGINAL_PROTOCOL?d->software_version%10:d->software_version%100,
                        d->software_version/10,
                        d->software_version%10,
                        inet_ntoa(d->info.network.address.sin_addr),
                        d->info.network.mac_address[0],
                        d->info.network.mac_address[1],
                        d->info.network.mac_address[2],
                        d->info.network.mac_address[3],
                        d->info.network.mac_address[4],
                        d->info.network.mac_address[5],
                        d->info.network.interface_name);
                  break;
#ifdef LIMESDR
                case LIMESDR_PROTOCOL:
/*
                  sprintf(text,"%s (%s %d.%d.%d)\n",
                        d->name,
                        "lime",
                        d->software_version/100,
                        (d->software_version%100)/10,
                        d->software_version%10);
*/
                  sprintf(text,"%s\n",
                        d->name);
                  break;
#endif
              }

              GtkWidget *label=gtk_label_new(text);
              gtk_widget_override_font(label, pango_font_description_from_string("Arial 12"));
              gtk_widget_show(label);
              gtk_grid_attach(GTK_GRID(grid),label,0,i,3,1);

              GtkWidget *start_button=gtk_button_new_with_label("Start");
              gtk_widget_override_font(start_button, pango_font_description_from_string("Arial 18"));
              gtk_widget_show(start_button);
              gtk_grid_attach(GTK_GRID(grid),start_button,3,i,1,1);
              g_signal_connect(start_button,"pressed",G_CALLBACK(start_cb),(gpointer *)i);

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

              GtkWidget *configure_button=gtk_button_new_with_label("Configure");
              gtk_widget_override_font(configure_button, pango_font_description_from_string("Arial 18"));
              gtk_widget_show(configure_button);
              gtk_grid_attach(GTK_GRID(grid),configure_button,4,i,1,1);
              g_signal_connect(configure_button,"pressed",G_CALLBACK(configure_cb),(gpointer *)i);
          }

          gtk_container_add (GTK_CONTAINER (content), grid);
          gtk_widget_show_all(discovery_dialog);
          gint result=gtk_dialog_run(GTK_DIALOG(discovery_dialog));

          if(result==GTK_RESPONSE_CLOSE) {
              _exit(0);
          }
         
          gtk_widget_destroy(discovery_dialog);
#ifdef INCLUDE_GPIO
          if(result==GTK_RESPONSE_YES) {
              configure_gpio(splash_window);
          }
#endif
      }
  }

  gdk_window_set_cursor(gdk_splash_window,cursor_watch);

  splash_status("Initializing wdsp ...");

  d=&discovered[selected_device];
  protocol=d->protocol;
  device=d->device;

  switch(d->protocol) {
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL:
      sprintf(property_path,"%02X-%02X-%02X-%02X-%02X-%02X.props",
                        d->info.network.mac_address[0],
                        d->info.network.mac_address[1],
                        d->info.network.mac_address[2],
                        d->info.network.mac_address[3],
                        d->info.network.mac_address[4],
                        d->info.network.mac_address[5]);
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      sprintf(property_path,"limesdr.props");
      break;
#endif
  }

  radioRestoreState();

  samples=malloc(display_width*sizeof(float));

  //splash_status("Initializing wdsp ...");
  wdsp_init(0,display_width,d->protocol);

  switch(d->protocol) {
    case ORIGINAL_PROTOCOL:
      splash_status("Initializing old protocol ...");
      old_protocol_init(0,display_width);
      break;
    case NEW_PROTOCOL:
      splash_status("Initializing new protocol ...");
      new_protocol_init(0,display_width);
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      splash_status("Initializing lime protocol ...");
      lime_protocol_init(0,display_width);
      break;
#endif
  }

  splash_status("Initializing GPIO ...");
#ifdef INCLUDE_GPIO
  gpio_init();
#endif

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "pihpsdr");
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  g_signal_connect (window, "delete-event", G_CALLBACK (main_delete), NULL);

#ifdef GRID_LAYOUT
  grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_container_add(GTK_CONTAINER(window), grid);
#else
  fixed=gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(window), fixed);
  y=0;
#endif

fprintf(stderr,"vfo_height=%d\n",VFO_HEIGHT);
  vfo = vfo_init(VFO_WIDTH,VFO_HEIGHT,window);
#ifdef GRID_LAYOUT
  gtk_grid_attach(GTK_GRID(grid), vfo, 0, y, 16, 1);
#else
  gtk_fixed_put(GTK_FIXED(fixed),vfo,0,0);
#endif


fprintf(stderr,"menu_height=%d\n",MENU_HEIGHT);
  menu = menu_init(MENU_WIDTH,MENU_HEIGHT,window);
#ifdef GRID_LAYOUT
  gtk_grid_attach(GTK_GRID(grid), menu, 16, 0, 1, 1);
#else
  gtk_fixed_put(GTK_FIXED(fixed),menu,VFO_WIDTH,y);
#endif

fprintf(stderr,"meter_height=%d\n",METER_HEIGHT);
  meter = meter_init(METER_WIDTH,METER_HEIGHT,window);
#ifdef GRID_LAYOUT
  gtk_grid_attach(GTK_GRID(grid), meter, 17, 0, 15, 1);
#else
  gtk_fixed_put(GTK_FIXED(fixed),meter,VFO_WIDTH+MENU_WIDTH,y);
  y+=VFO_HEIGHT;
#endif

  if(display_panadapter) {
    int height=PANADAPTER_HEIGHT;
    if(!display_waterfall) {
      height+=WATERFALL_HEIGHT;
      if(!display_sliders) {
        height+=SLIDERS_HEIGHT;
      }
      if(!display_toolbar) {
        height+=TOOLBAR_HEIGHT;
      }
    } else {
      if(!display_sliders) {
        height+=SLIDERS_HEIGHT/2;
      }
    }
fprintf(stderr,"panadapter_height=%d\n",height);
    panadapter = panadapter_init(display_width,height);
#ifdef GRID_LAYOUT
    gtk_grid_attach(GTK_GRID(grid), panadapter, 0, 1, 32, 1);
#else
    gtk_fixed_put(GTK_FIXED(fixed),panadapter,0,VFO_HEIGHT);
    y+=height;
#endif
  }

  if(display_waterfall) {
    int height=WATERFALL_HEIGHT;
    if(!display_panadapter) {
      height+=PANADAPTER_HEIGHT;
    }
    if(!display_sliders) {
      if(display_panadapter) {
        height+=SLIDERS_HEIGHT/2;
      } else {
        height+=SLIDERS_HEIGHT;
      }
    }
    if(!display_toolbar) {
      height+=TOOLBAR_HEIGHT;
    }
fprintf(stderr,"waterfall_height=%d\n",height);
    waterfall = waterfall_init(display_width,height);
#ifdef GRID_LAYOUT
    gtk_grid_attach(GTK_GRID(grid), waterfall, 0, 2, 32, 1);
#else
    gtk_fixed_put(GTK_FIXED(fixed),waterfall,0,y);
    y+=height;
#endif
  }

  if(display_sliders) {
fprintf(stderr,"sliders_height=%d\n",SLIDERS_HEIGHT);
    sliders = sliders_init(display_width,SLIDERS_HEIGHT,window);
#ifdef GRID_LAYOUT
    gtk_grid_attach(GTK_GRID(grid), sliders, 0, 3, 32, 1);
#else
    gtk_fixed_put(GTK_FIXED(fixed),sliders,0,y);
    y+=SLIDERS_HEIGHT;
#endif
  }

  if(display_toolbar) {
fprintf(stderr,"toolbar_height=%d\n",TOOLBAR_HEIGHT);
    toolbar = toolbar_init(display_width,TOOLBAR_HEIGHT,window);
#ifdef GRID_LAYOUT
    gtk_grid_attach(GTK_GRID(grid), toolbar, 0, 4, 32, 1);
#else
    gtk_fixed_put(GTK_FIXED(fixed),toolbar,0,y);
    y+=TOOLBAR_HEIGHT;
#endif
  }

  splash_close();


  gtk_widget_show_all (window);

  if(full_screen) {
    gtk_window_fullscreen(GTK_WINDOW(window));
  }

  GdkWindow *gdk_window = gtk_widget_get_window(window);
  gdk_window_set_cursor(gdk_window,cursor_arrow);

  update_timer_id=gdk_threads_add_timeout(1000/updates_per_second, update, NULL);

  // save every 30 seconds
  save_timer_id=gdk_threads_add_timeout(30000, save_cb, NULL);


  if(protocol!=NEW_PROTOCOL) {
    setFrequency(getFrequency());
  }

  g_idle_add(vfo_update,(gpointer)NULL);

  return 0;
}


int
main (int   argc,
      char *argv[])
{
  gtk_init (&argc, &argv);

  fprintf(stderr,"Build: %s %s\n",build_date,build_time);

  uname(&unameData);
  fprintf(stderr,"sysname: %s\n",unameData.sysname);
  fprintf(stderr,"nodename: %s\n",unameData.nodename);
  fprintf(stderr,"release: %s\n",unameData.release);
  fprintf(stderr,"version: %s\n",unameData.version);
  fprintf(stderr,"machine: %s\n",unameData.machine);

  GdkScreen *screen=gdk_screen_get_default();
  if(screen==NULL) {
    fprintf(stderr,"no default screen!\n");
    _exit(0);
  }


  display_width=gdk_screen_get_width(screen);
  display_height=gdk_screen_get_height(screen);

fprintf(stderr,"width=%d height=%d\n", display_width, display_height);
  if(display_width>800 || display_height>480) {
    display_width=800;
    display_height=480;
    full_screen=0;
  }

  fprintf(stderr,"display_width=%d display_height=%d\n", display_width, display_height);

  splash_show("hpsdr.png", display_width, display_height, full_screen);

  g_idle_add(init,(void *)argv[0]);

  gtk_main();

  return 0;
}
