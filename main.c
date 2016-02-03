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
#include "new_discovery.h"
#include "new_protocol.h"
#include "wdsp.h"
#include "vfo.h"
#include "meter.h"
#include "panadapter.h"
#include "splash.h"
#include "waterfall.h"
#include "toolbar.h"
#include "radio.h"

#define VFO_HEIGHT (display_height/8)
#define VFO_WIDTH ((display_width/4)*3)
#define METER_HEIGHT (display_height/8)
#define METER_WIDTH (display_width/4)
#define PANADAPTER_HEIGHT (display_height/4)
#define TOOLBAR_HEIGHT (display_height/4)
#define WATERFALL_HEIGHT (display_height-(VFO_HEIGHT+PANADAPTER_HEIGHT+TOOLBAR_HEIGHT))

struct utsname unameData;

gint display_width;
gint display_height;

static gint update_timer_id;
static gint updates_per_second=10;

static gint save_timer_id;

static float *samples;

static int start=0;

static GtkWidget *discovery_dialog;

gint update(gpointer data) {
    int result;
    GetPixels(isTransmitting()==0?CHANNEL_RX0:CHANNEL_TX,samples,&result);
    if(result==1) {
        panadapter_update(samples,isTransmitting());
        if(!isTransmitting()) {
            waterfall_update(samples);
        }
    }

    if(!isTransmitting()) {
        float m=GetRXAMeter(CHANNEL_RX0, 1/*WDSP.S_AV*/);
        meter_update(SMETER,(double)m,0.0);
    } else {

        DISCOVERED *d=&discovered[selected_device];

        double constant1=5.0;
        double constant2=0.108;

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
            case NEW_DEVICE_ANAN_10E:
                constant1=3.3;
                constant2=0.09;
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
        double fwd;
        double rev;
        v1=((double)power/4095.0)*constant1;
        fwd=(v1*v1)/constant2;

        rev=0.0;
        if(alex_forward_power!=0) {
            power=alex_reverse_power;
            v1=((double)power/4095.0)*constant1;
            rev=(v1*v1)/constant2;
        }
        meter_update(POWER,fwd,rev);
    }

    return TRUE;
}

static gint save_cb(gpointer data) {
    radioSaveState();
    saveProperties(property_path);
    return TRUE;
}

static void start_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"start_cb\n");
    selected_device=(int)data;
    start=1;
    gtk_widget_destroy(discovery_dialog);
}

static void configure_cb(GtkWidget *widget, gpointer data) {
}


gint init(void* arg) {

  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *vfo;
  GtkWidget *meter;
  GtkWidget *panadapter;
  GtkWidget *waterfall;
  GtkWidget *toolbar;

  DISCOVERED* d;

  char wisdom_directory[1024];
  char wisdom_file[1024];

  fprintf(stderr,"init\n");

  // check if wisdom file exists
  getcwd(wisdom_directory, sizeof(wisdom_directory));
  strcpy(&wisdom_directory[strlen(wisdom_directory)],"/");
  strcpy(wisdom_file,wisdom_directory);
  strcpy(&wisdom_file[strlen(wisdom_file)],"wdspWisdom");
  if(access(wisdom_file,F_OK)<0) {
      WDSPwisdom (wisdom_directory);
  }

  while(!start) {
      new_discovery();
      if(devices==0) {
          fprintf(stderr,"No devices found!\n");
          GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
          discovery_dialog = gtk_message_dialog_new (GTK_WINDOW(splash_window),
                                 flags,
                                 GTK_MESSAGE_ERROR,
                                 GTK_BUTTONS_OK_CANCEL,
                                 "No devices found! Retry Discovery?");
          gint result=gtk_dialog_run (GTK_DIALOG (discovery_dialog));
          if(result==GTK_RESPONSE_CANCEL) {
               _exit(0);
          }
      } else if(devices==1) {
          selected_device=0;
          start=1;
      } else {
          fprintf(stderr,"%s: found %d devices.\n", (char *)arg, devices);
          GtkDialogFlags flags=GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
          discovery_dialog = gtk_dialog_new_with_buttons ("Discovered",
                                      GTK_WINDOW(splash_window),
                                      flags,
                                      "Discover",
                                      GTK_RESPONSE_REJECT,
                                      NULL);

          GtkWidget *content;

//          g_signal_connect_swapped(discovery_dialog,"response",G_CALLBACK(gtk_widget_destroy),discovery_dialog);

          content=gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));

          GtkWidget *grid=gtk_grid_new();
          gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
          gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

          int i;
          char text[128];
          for(i=0;i<devices;i++) {
              d=&discovered[i];
              sprintf(text,"%s %s (%02X:%02X:%02X:%02X:%02X:%02X) on interface %s\n",
                        d->name,
                        inet_ntoa(d->address.sin_addr),
                        d->mac_address[0],
                        d->mac_address[1],
                        d->mac_address[2],
                        d->mac_address[3],
                        d->mac_address[4],
                        d->mac_address[5],
                        d->interface_name);

              GtkWidget *label=gtk_label_new(text);
              gtk_widget_show(label);
              gtk_grid_attach(GTK_GRID(grid),label,0,i,3,1);

              GtkWidget *start_button=gtk_button_new_with_label("Start");
              gtk_widget_show(start_button);
              gtk_grid_attach(GTK_GRID(grid),start_button,3,i,1,1);
              g_signal_connect(start_button,"pressed",G_CALLBACK(start_cb),(gpointer *)i);

              GtkWidget *configure_button=gtk_button_new_with_label("Configure");
              gtk_widget_show(configure_button);
              gtk_grid_attach(GTK_GRID(grid),configure_button,4,i,1,1);
              g_signal_connect(configure_button,"pressed",G_CALLBACK(configure_cb),(gpointer *)i);
          }

          gtk_container_add (GTK_CONTAINER (content), grid);
          gtk_widget_show_all(discovery_dialog);
          gint result=gtk_dialog_run(GTK_DIALOG(discovery_dialog));
      }
  }

  d=&discovered[selected_device];
  sprintf(property_path,"%02X-%02X-%02X-%02X-%02X-%02X.props",
                        d->mac_address[0],
                        d->mac_address[1],
                        d->mac_address[2],
                        d->mac_address[3],
                        d->mac_address[4],
                        d->mac_address[5]);

  loadProperties(property_path);
  radioRestoreState();

  samples=malloc(display_width*sizeof(float));

  //selected_device=0;
  setSampleRate(48000);
  new_protocol_init(0,display_width);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "pihpsdr");

  //g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  gtk_container_set_border_width (GTK_CONTAINER (window), 0);

  grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(window), grid);

  vfo = vfo_init(VFO_WIDTH,VFO_HEIGHT,window);
  gtk_grid_attach(GTK_GRID(grid), vfo, 0, 0, 3, 1);

  meter = meter_init(METER_WIDTH,METER_HEIGHT);
  gtk_grid_attach(GTK_GRID(grid), meter, 3, 0, 1, 1);

  panadapter = panadapter_init(display_width,PANADAPTER_HEIGHT);
  gtk_grid_attach(GTK_GRID(grid), panadapter, 0, 1, 4, 1);

  waterfall = waterfall_init(display_width,WATERFALL_HEIGHT);
  gtk_grid_attach(GTK_GRID(grid), waterfall, 0, 2, 4, 1);

  toolbar = toolbar_init(display_width,TOOLBAR_HEIGHT,window);
  gtk_grid_attach(GTK_GRID(grid), toolbar, 0, 3, 4, 1);

  splash_close();


  gtk_widget_show_all (window);

  gtk_window_fullscreen(GTK_WINDOW(window));

  GdkCursor *cursor=gdk_cursor_new(GDK_ARROW);
  GdkWindow *gdk_window = gtk_widget_get_window(window);
  gdk_window_set_cursor(gdk_window,cursor);

/*
  GdkDisplay *display=gdk_display_get_default();
  if(display==NULL) {
    fprintf(stderr,"no default display!\n");
    _exit(0);
  }

  GdkCursor *cursor=gdk_cursor_new_for_display(display,GDK_ARROW);
*/

  update_timer_id=gdk_threads_add_timeout(1000/updates_per_second, update, NULL);

  // save every 30 seconds
  save_timer_id=gdk_threads_add_timeout(30000, save_cb, NULL);

  vfo_update(NULL);

  return 0;
}


int
main (int   argc,
      char *argv[])
{
  gtk_init (&argc, &argv);

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


  //display_width=gdk_screen_get_width(screen);
  //display_height=gdk_screen_get_height(screen);
  display_width=800;
  display_height=480;

  splash_show("splash.png", 0, display_width, display_height);

  g_idle_add(init,(void *)argv[0]);

  gtk_main();

  return 0;
}
