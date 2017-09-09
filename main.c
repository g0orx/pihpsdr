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

#include "audio.h"
#include "band.h"
#include "bandstack.h"
#include "main.h"
#include "channel.h"
#include "discovered.h"
#include "configure.h"
#include "gpio.h"
#ifdef RADIOBERRY
#include "radioberry.h"
#endif
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
#include "ext.h"

struct utsname unameData;

gint display_width;
gint display_height;
gint full_screen=1;

static GtkWidget *discovery_dialog;

static sem_t wisdom_sem;

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
  status_text("Creating FFTW Wisdom file ...");
  WDSPwisdom ((char *)arg);
  sem_post(&wisdom_sem);
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
#ifdef RADIOBERRY
      case RADIOBERRY_PROTOCOL:
        radioberry_protocol_stop();
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

  fprintf(stderr,"init\n");

  audio_get_cards();

  cursor_arrow=gdk_cursor_new(GDK_ARROW);
  cursor_watch=gdk_cursor_new(GDK_WATCH);

  gdk_window_set_cursor(gtk_widget_get_window(top_window),cursor_watch);

  // check if wisdom file exists
  res=getcwd(wisdom_directory, sizeof(wisdom_directory));
  strcpy(&wisdom_directory[strlen(wisdom_directory)],"/");
  strcpy(wisdom_file,wisdom_directory);
  strcpy(&wisdom_file[strlen(wisdom_file)],"wdspWisdom");
  status_text("Checking FFTW Wisdom file ...");
  if(access(wisdom_file,F_OK)<0) {
      int rc=sem_init(&wisdom_sem, 0, 0);
      rc=pthread_create(&wisdom_thread_id, NULL, wisdom_thread, (void *)wisdom_directory);
      while(sem_trywait(&wisdom_sem)<0) {
        status_text(wisdom_get_status());
        while (gtk_events_pending ())
          gtk_main_iteration ();
        usleep(100000); // 100ms
      }
  }

  g_idle_add(ext_discovery,NULL);
  return 0;
}

static void activate_pihpsdr(GtkApplication *app, gpointer data) {


  //gtk_init (&argc, &argv);

  fprintf(stderr,"Build: %s %s\n",build_date,version);

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
/*
    if(display_width>1600) {
      display_width=1600;
    } else {
      display_width=800;
    }
    if(display_height>960) {
      display_height=960;
    } else {
      display_height=480;
    }
*/
    display_width=800;
    display_height=480;
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
  gtk_window_set_title (GTK_WINDOW (top_window), "pihpsdr");
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

  pihpsdr=gtk_application_new("org.g0orx.pihpsdr", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(pihpsdr, "activate", G_CALLBACK(activate_pihpsdr), NULL);
  status=g_application_run(G_APPLICATION(pihpsdr), argc, argv);
fprintf(stderr,"exiting ...\n");
  g_object_unref(pihpsdr);
  return status;
}
