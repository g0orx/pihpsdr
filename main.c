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

// Define maximum window size. 
// Standard values 800 and 480: suitable for RaspberryBi 7-inch screen

#define MAX_DISPLAY_WIDTH  800
#define MAX_DISPLAY_HEIGHT 480

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "audio.h"
#include "band.h"
#include "bandstack.h"
#include "main.h"
#include "channel.h"
#include "discovered.h"
#include "configure.h"
#include "gpio.h"
#include "wdsp.h"
#include "new_menu.h"
#include "radio.h"
#include "version.h"
#include "button_text.h"
#ifdef I2C
#include "i2c.h"
#endif
#include "discovery.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "frequency.h"   // for canTransmit
#include "ext.h"

struct utsname unameData;

gint display_width;
gint display_height;
gint full_screen=1;

static GtkWidget *discovery_dialog;

#ifdef __APPLE__
static sem_t *wisdom_sem;
#else
static sem_t wisdom_sem;
#endif

static GdkCursor *cursor_arrow;
static GdkCursor *cursor_watch;

static GtkWidget *splash;

GtkWidget *top_window;
GtkWidget *grid;

static DISCOVERED* d;

static GtkWidget *status;

void status_text(char *text) {
  //fprintf(stderr,"splash_status: %s\n",text);
  gtk_label_set_text(GTK_LABEL(status),text);
  usleep(10000);
  while (gtk_events_pending ())
    gtk_main_iteration ();
}

static gint save_cb(gpointer data) {
    radioSaveState();
    return TRUE;
}

static pthread_t wisdom_thread_id;

static void* wisdom_thread(void *arg) {
  fprintf(stderr,"Securing wisdom file in directory: %s\n", (char *)arg);
  status_text("Creating FFTW Wisdom file ...");
  WDSPwisdom ((char *)arg);
#ifdef __APPLE__
  sem_post(wisdom_sem);
#else
  sem_post(&wisdom_sem);
#endif
  return NULL;
}

//
// handler for key press events.
// SpaceBar presses toggle MOX, everything else downstream
// code to switch mox copied from mox_cb() in toolbar.c,
// but added the correct return values.
//
gboolean keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {

  if (event->keyval == GDK_KEY_space && radio != NULL) {
    if(getTune()==1) {
      setTune(0);
    }
    if(getMox()==1) {
      setMox(0);
    } else if(canTransmit() || tx_out_of_band) {
      setMox(1);
    } else {
      transmitter_set_out_of_band(transmitter);
    }
    g_idle_add(ext_vfo_update,NULL);
    return TRUE;
  }
  return FALSE;
}

gboolean main_delete (GtkWidget *widget) {
  if(radio!=NULL) {
#ifdef GPIO
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
  }
  _exit(0);
}

static int init(void *data) {
  char *res;
  char wisdom_directory[1024];
  char wisdom_file[1024];
  int rc;

  fprintf(stderr,"init\n");

  audio_get_cards();

  cursor_arrow=gdk_cursor_new(GDK_ARROW);
  cursor_watch=gdk_cursor_new(GDK_WATCH);

  gdk_window_set_cursor(gtk_widget_get_window(top_window),cursor_watch);

  //
  // Let WDSP (via FFTW) check for wisdom file in current dir
  // If there is one, the "wisdom thread" takes no time
  // Depending on the WDSP version, the file is wdspWisdom or wdspWisdom00.
  //
  res=getcwd(wisdom_directory, sizeof(wisdom_directory));
  strcpy(&wisdom_directory[strlen(wisdom_directory)],"/");
  strcpy(wisdom_file,wisdom_directory);
#ifdef __APPLE__
  wisdom_sem=sem_open("WISDOM", O_CREAT, 0700, 0);
#else
  rc=sem_init(&wisdom_sem, 0, 0);
#endif
  rc=pthread_create(&wisdom_thread_id, NULL, wisdom_thread, (void *)wisdom_directory);
#ifdef __APPLE__
  while(sem_trywait(wisdom_sem)<0) {
#else
  while(sem_trywait(&wisdom_sem)<0) {
#endif
      status_text("WDSP wisdom done.");
      while (gtk_events_pending ())
        gtk_main_iteration ();
      usleep(100000); // 100ms
  }

  g_idle_add(ext_discovery,NULL);
  return 0;
}

static void activate_pihpsdr(GtkApplication *app, gpointer data) {


  //gtk_init (&argc, &argv);

  fprintf(stderr,"Build: %s %s\n",build_date,version);

  fprintf(stderr,"GTK+ version %d.%d.%d\n", gtk_major_version, gtk_minor_version, gtk_micro_version);
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

  if(display_width>MAX_DISPLAY_WIDTH || display_height>MAX_DISPLAY_HEIGHT) {
    display_width=MAX_DISPLAY_WIDTH;
    display_height=MAX_DISPLAY_HEIGHT;
    full_screen=0;
  }

fprintf(stderr,"display_width=%d display_height=%d\n", display_width, display_height);

  fprintf(stderr,"create top level window\n");
  top_window = gtk_application_window_new (app);
  if(full_screen) {
fprintf(stderr,"full screen\n");
    gtk_window_fullscreen(GTK_WINDOW(top_window));
  }
  gtk_widget_set_size_request(top_window, display_width, display_height);
  gtk_window_set_title (GTK_WINDOW (top_window), "piHPSDR");
  gtk_window_set_position(GTK_WINDOW(top_window),GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable(GTK_WINDOW(top_window), FALSE);
  fprintf(stderr,"setting top window icon\n");
  GError *error;
  if(!gtk_window_set_icon_from_file (GTK_WINDOW(top_window), "hpsdr.png", &error)) {
    fprintf(stderr,"Warning: failed to set icon for top_window\n");
    if(error!=NULL) {
      fprintf(stderr,"%s\n",error->message);
    }
  }
  g_signal_connect (top_window, "delete-event", G_CALLBACK (main_delete), NULL);
  //g_signal_connect (top_window,"draw", G_CALLBACK (main_draw_cb), NULL);

  //
  // We want to use the space-bar as an alternative to go to TX
  //
  gtk_widget_add_events(top_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(top_window, "key_press_event", G_CALLBACK(keypress_cb), NULL);


//fprintf(stderr,"create fixed container\n");
  //fixed=gtk_fixed_new();
  //gtk_container_add(GTK_CONTAINER(top_window), fixed);

fprintf(stderr,"create grid\n");
  grid = gtk_grid_new();
  gtk_widget_set_size_request(grid, display_width, display_height);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
fprintf(stderr,"add grid\n");
  gtk_container_add (GTK_CONTAINER (top_window), grid);

fprintf(stderr,"create image\n");
  GtkWidget  *image=gtk_image_new_from_file("hpsdr.png");
fprintf(stderr,"add image to grid\n");
  gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 4);

fprintf(stderr,"create pi label\n");
  char build[64];
  sprintf(build,"build: %s %s",build_date, version);
  GtkWidget *pi_label=gtk_label_new("piHPSDR by John Melton g0orx/n6lyt");
  gtk_label_set_justify(GTK_LABEL(pi_label),GTK_JUSTIFY_LEFT);
  gtk_widget_show(pi_label);
fprintf(stderr,"add pi label to grid\n");
  gtk_grid_attach(GTK_GRID(grid),pi_label,1,0,1,1);

fprintf(stderr,"create build label\n");
  GtkWidget *build_date_label=gtk_label_new(build);
  gtk_label_set_justify(GTK_LABEL(build_date_label),GTK_JUSTIFY_LEFT);
  gtk_widget_show(build_date_label);
fprintf(stderr,"add build label to grid\n");
  gtk_grid_attach(GTK_GRID(grid),build_date_label,1,1,1,1);

fprintf(stderr,"create status\n");
  status=gtk_label_new("");
  gtk_label_set_justify(GTK_LABEL(status),GTK_JUSTIFY_LEFT);
  //gtk_widget_override_font(status, pango_font_description_from_string("FreeMono 18"));
  gtk_widget_show(status);
fprintf(stderr,"add status to grid\n");
  gtk_grid_attach(GTK_GRID(grid), status, 1, 3, 1, 1);

/*
fprintf(stderr,"create exit button\n");
  GtkWidget *button = gtk_button_new_with_label ("Exit");
  //g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), top_window);
fprintf(stderr,"add exit button to grid\n");
  gtk_grid_attach(GTK_GRID(grid), button, 1, 4, 1, 1);
*/

  gtk_widget_show_all(top_window);

  g_idle_add(init,NULL);
  //g_idle_add(discovery,NULL);


}

int main(int argc,char **argv) {
  GtkApplication *pihpsdr;
  int status;

  char name[1024];

  sprintf(name,"org.g0orx.pihpsdr.pid%d",getpid());

fprintf(stderr,"gtk_application_new: %s\n",name);

  pihpsdr=gtk_application_new(name, G_APPLICATION_FLAGS_NONE);
  g_signal_connect(pihpsdr, "activate", G_CALLBACK(activate_pihpsdr), NULL);
  status=g_application_run(G_APPLICATION(pihpsdr), argc, argv);
fprintf(stderr,"exiting ...\n");
  g_object_unref(pihpsdr);
  return status;
}
