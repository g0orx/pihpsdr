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
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "discovered.h"
#include "main.h"
#include "agc.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "frequency.h"
#include "new_protocol.h"
#include "radio.h"
#include "vfo.h"
#include "channel.h"
#include "toolbar.h"
#include "wdsp.h"
#include "wdsp_init.h"

static GtkWidget *parent_window;
static int my_width;
static int my_height;

static GtkWidget *vfo;
static cairo_surface_t *vfo_surface = NULL;

static int steps[]={1,10,25,50,100,250,500,1000,2500,5000,6250,9000,10000,12500,15000,20000,25000,30000,50000,100000,0};
static char *step_labels[]={"1Hz","10Hz","25Hz","50Hz","100Hz","250Hz","500Hz","1kHz","2.5kHz","5kHz","6.25kHz","9kHz","10kHz","12.5kHz","15kHz","20kHz","25kHz","30kHz","50kHz","100kHz",0};

static GtkWidget* menu=NULL;
static GtkWidget* band_menu=NULL;

void vfo_step(int steps) {
  if(!locked) {
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    //entry->frequencyA=entry->frequencyA+(steps*step);
    //setFrequency(entry->frequencyA);
    setFrequency(entry->frequencyA+(steps*step));
    vfo_update(NULL);
  }
}

void vfo_move(int hz) {
  if(!locked) {
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    //entry->frequencyA=(entry->frequencyA+hz)/step*step;
    //setFrequency(entry->frequencyA);
    
#ifdef LIMESDR
    if(protocol==LIMESDR_PROTOCOL)
      setFrequency((entry->frequencyA+ddsOffset-hz)/step*step);
    else 
#endif
      setFrequency((entry->frequencyA+ddsOffset+hz)/step*step);
    vfo_update(NULL);
  }
}
void vfo_move_to(int hz) {
  if(!locked) {
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    //entry->frequencyA=(entry->frequencyA+hz)/step*step;
    //setFrequency(entry->frequencyA);

#ifdef LIMESDR
    if(protocol==LIMESDR_PROTOCOL) {
      setFrequency((entry->frequencyA+ddsOffset-hz)/step*step);
    } else {
#endif
      long f=(entry->frequencyA+ddsOffset+hz)/step*step;
      if(mode==modeCWL) {
        f+=cw_keyer_sidetone_frequency;
      } else if(mode==modeCWU) {
        f-=cw_keyer_sidetone_frequency;
      }
      setFrequency(f);
#ifdef LIMESDR
    }
#endif
    vfo_update(NULL);
  }
}

static gboolean
vfo_scroll_event_cb (GtkWidget      *widget,
               GdkEventScroll *event,
               gpointer        data)
{
  int i;
  if(event->direction==GDK_SCROLL_UP) {
    i=1;
  } else {
    i=-1;
  }
  if(event->x>(my_width/2)) {
    if(event->x>((my_width/4)*3)) {
      // rit
      rit+=i;
      if(rit>10000) {
        rit=1000;
      }
      if(rit<-1000) {
        rit=-1000;
      }
    } else {
      // step
    }
  } else {
    // frequency
  }
  vfo_update(NULL);
}


static gboolean vfo_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
fprintf(stderr,"vfo_configure_event_cb: width=%d height=%d\n",
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));
  if (vfo_surface)
    cairo_surface_destroy (vfo_surface);

  vfo_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));

  /* Initialize the surface to black */
  cairo_t *cr;
  cr = cairo_create (vfo_surface);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  g_idle_add(vfo_update,NULL);

  /* We've handled the configure event, no need for further processing. */
  return TRUE;
}

static gboolean vfo_draw_cb (GtkWidget *widget,
 cairo_t   *cr,
 gpointer   data)
{
  cairo_set_source_surface (cr, vfo_surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
}

int vfo_update(void *data) {
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    FILTER* band_filters=filters[entry->mode];
    FILTER* band_filter=&band_filters[entry->filter];
    if(vfo_surface) {
        cairo_t *cr;
        cr = cairo_create (vfo_surface);
        cairo_set_source_rgb (cr, 0, 0, 0);
        cairo_paint (cr);

        cairo_select_font_face(cr, "Arial",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);

        char version[16];
        char text[128];
        if(radio->protocol==ORIGINAL_PROTOCOL) {
            sprintf(version,"%d.%d",
                radio->software_version/10,
                radio->software_version%10);
        } else {
            sprintf(version,"%d.%d.%d",
                radio->software_version/100,
                (radio->software_version%100)/10,
                radio->software_version%10);
        }

        switch(radio->protocol) {
            case ORIGINAL_PROTOCOL:
            case NEW_PROTOCOL:
              sprintf(text,"%s (%s %s) %s",
                    radio->name,
                    radio->protocol==ORIGINAL_PROTOCOL?"old":"new",
                    version,
                    inet_ntoa(radio->info.network.address.sin_addr));
              break;
#ifdef LIMESDR
            case LIMESDR_PROTOCOL:
              sprintf(text,"%s\n",
                    radio->name);
              break;
#endif
        }
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, 5, 15);  
        cairo_show_text(cr, text);

        long long f=entry->frequencyA+ddsOffset;
        char sf[32];
        sprintf(sf,"%0lld.%06lld MHz",f/(long long)1000000,f%(long long)1000000);
        cairo_set_font_size(cr, 28);
        if(isTransmitting()) {
            cairo_set_source_rgb(cr, 1, 0, 0);
        } else {
            cairo_set_source_rgb(cr, 0, 1, 0);
        }
        cairo_move_to(cr, 5, 38);  
        cairo_show_text(cr, sf);

        cairo_set_font_size(cr, 12);

        if(rit==0) {
            cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        } else {
            cairo_set_source_rgb(cr, 0, 1, 0);
        }
        sprintf(sf,"RIT: %d Hz",rit);
        cairo_move_to(cr, (my_width/4)*3, 38);  
        cairo_show_text(cr, sf);

        cairo_set_source_rgb(cr, 0, 1, 0);

        int s=0;
        while(steps[s]!=step && steps[s]!=0) {
          s++;
        }
        sprintf(sf,"Step %s",step_labels[s]);
        cairo_move_to(cr, my_width/2, 15);  
        cairo_show_text(cr, sf);

        cairo_move_to(cr, (my_width/4)*3, 15);  
        cairo_show_text(cr, getFrequencyInfo(f));

        if(locked) {
            cairo_set_source_rgb(cr, 1, 0, 0);
            cairo_move_to(cr, 10, 50);  
            cairo_show_text(cr, "Locked");
        }

        if(function) {
            cairo_set_source_rgb(cr, 1, 0.5, 0);
            cairo_move_to(cr, 70, 50);  
            cairo_show_text(cr, "Function");
        }

        cairo_set_source_rgb(cr, 1, 1, 0);
        cairo_move_to(cr, 130, 50);  
        cairo_show_text(cr, mode_string[entry->mode]);

        cairo_move_to(cr, 190, 50);  
        cairo_show_text(cr, band_filter->title);

        cairo_move_to(cr, 250, 50);  
        if(nr) {
          cairo_show_text(cr, "NR");
        }
        if(nr2) {
          cairo_show_text(cr, "NR2");
        }
        if(nb) {
          cairo_show_text(cr, "NB");
        }
        if(nb2) {
          cairo_show_text(cr, "NB2");
        }
        if(anf) {
          cairo_show_text(cr, "ANF");
        }
        if(snb) {
          cairo_show_text(cr, "SNB");
        }

        cairo_move_to(cr, 310, 50);  
        switch(agc) {
          case AGC_OFF:
            cairo_show_text(cr, "AGC OFF");
            break;
          case AGC_LONG:
            cairo_show_text(cr, "AGC LONG");
            break;
          case AGC_SLOW:
            cairo_show_text(cr, "AGC SLOW");
            break;
          case AGC_MEDIUM:
            cairo_show_text(cr, "AGC MEDIUM");
            break;
          case AGC_FAST:
            cairo_show_text(cr, "AGC FAST");
            break;
        }

        cairo_destroy (cr);
        gtk_widget_queue_draw (vfo);
    } else {
fprintf(stderr,"vfo_update: no surface!\n");
    }
    return 0;
}

static gboolean
vfo_step_select_cb (GtkWidget *widget,
               gpointer        data)
{
  step=steps[(int)data];
  vfo_update(NULL);
}

static gboolean
vfo_press_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{

  if((int)event->x < (my_width/2)) {
    lock_cb(NULL,NULL);
  } else {
    GtkWidget *dialog=gtk_dialog_new_with_buttons("Step",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

    GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid=gtk_grid_new();

    gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

    GtkWidget *step_rb=NULL;
    int i=0;
    while(steps[i]!=0) {
      if(i==0) {
          step_rb=gtk_radio_button_new_with_label(NULL,step_labels[i]);
      } else {
          step_rb=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(step_rb),step_labels[i]);
      }
      gtk_widget_override_font(step_rb, pango_font_description_from_string("Arial 18"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (step_rb), steps[i]==step);
      gtk_widget_show(step_rb);
      gtk_grid_attach(GTK_GRID(grid),step_rb,i%5,i/5,1,1);
      g_signal_connect(step_rb,"pressed",G_CALLBACK(vfo_step_select_cb),(gpointer *)i);
      i++;
    }
  
    gtk_container_add(GTK_CONTAINER(content),grid);
  
    GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
    gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
    gtk_widget_show_all(dialog);

    g_signal_connect_swapped (dialog,
                             "response",
                             G_CALLBACK (gtk_widget_destroy),
                             dialog);
  
    int result=gtk_dialog_run(GTK_DIALOG(dialog));
  }
  return TRUE;
}

GtkWidget* vfo_init(int width,int height,GtkWidget *parent) {

fprintf(stderr,"vfo_init: width=%d height=%d\n", width, height);
  parent_window=parent;
  my_width=width;
  my_height=height;

  vfo = gtk_drawing_area_new ();
  gtk_widget_set_size_request (vfo, width, height);

  /* Signals used to handle the backing surface */
  g_signal_connect (vfo, "draw",
            G_CALLBACK (vfo_draw_cb), NULL);
  g_signal_connect (vfo,"configure-event",
            G_CALLBACK (vfo_configure_event_cb), NULL);

  /* Event signals */
  g_signal_connect (vfo, "button-press-event",
            G_CALLBACK (vfo_press_event_cb), NULL);
  g_signal_connect(vfo,"scroll_event",
            G_CALLBACK(vfo_scroll_event_cb),NULL);
  gtk_widget_set_events (vfo, gtk_widget_get_events (vfo)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_SCROLL_MASK);

fprintf(stderr,"vfo_init: set Frequency,Mode,Filter\n");
  BAND *band=band_get_current_band();
  BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
  setFrequency(entry->frequencyA);
  setMode(entry->mode);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);

  set_alex_rx_antenna(band->alexRxAntenna);
  set_alex_tx_antenna(band->alexTxAntenna);
  set_alex_attenuation(band->alexAttenuation);

  return vfo;
}
