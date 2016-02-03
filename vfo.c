#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "new_protocol.h"
#include "rotary_encoder.h"
#include "radio.h"
#include "vfo.h"
#include "channel.h"
#include "wdsp.h"

static GtkWidget *parent_window;

static GtkWidget *vfo;
static cairo_surface_t *vfo_surface = NULL;

static pthread_t rotary_encoder_thread_id;

static int steps[]={1,10,25,50,100,250,500,1000,2500,5000,6250,9000,10000,12500,15000,20000,25000,30000,50000,100000,0};
static char *step_labels[]={"1Hz","10Hz","25Hz","50Hz","100Hz","250Hz","500Hz","1kHz","2.5kHz","5kHz","6.25kHz","9kHz","10kHz","12.5kHz","15kHz","20kHz","25kHz","30kHz","50kHz","100kHz",0};
static int af_function=0;
static int previous_af_function=0;
static int rf_function=0;
static int previous_rf_function=0;
static int band_button=0;
static int previous_band_button=0;

static GtkWidget* menu=NULL;
static GtkWidget* band_menu=NULL;

void vfo_step(int steps) {
  if(!locked) {
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    entry->frequencyA=entry->frequencyA+(steps*step);
    setFrequency(entry->frequencyA);
    vfo_update(NULL);
  }
}

void vfo_move(int hz) {
  if(!locked) {
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    entry->frequencyA=(entry->frequencyA+hz)/step*step;
    setFrequency(entry->frequencyA);
    vfo_update(NULL);
  }
}

static int rotary_encoder_changed(void *data) {
  if(!locked) {
    int pos=*(int*)data;
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    entry->frequencyA=entry->frequencyA+(pos*step);
    setFrequency(entry->frequencyA);
    vfo_update(NULL);
  }
  free(data);
  return 0;
}

static int af_encoder_changed(void *data) {
  int pos=*(int*)data;
  if(pos!=0) {
    if(af_function) {
      // agc gain
      double gain=agc_gain;
      gain+=(double)pos;
      if(gain<0.0) {
        gain=0.0;
      } else if(gain>120.0) {
        gain=120.0;
      }
      set_agc_gain(gain);
    } else {
      // af gain
      double gain=volume;
      gain+=(double)pos/20.0;
      if(gain<0.0) {
        gain=0.0;
      } else if(gain>1.0) {
        gain=1.0;
      }
      set_af_gain(gain);
    }
  }
  free(data);
  return 0;
}

static int rf_encoder_changed(void *data) {
  int pos=*(int*)data;
  if(pos!=0) {
    if(rf_function) {
      // tune drive
      double d=getTuneDrive();
      d+=(double)pos/10.0;
      if(d<0.0) {
        d=0.0;
      } else if(d>1.0) {
        d=1.0;
      }
      set_tune(d);
    } else {
      // drive
      double d=getDrive();
      d+=(double)pos/10.0;
      if(d<0.0) {
        d=0.0;
      } else if(d>1.0) {
        d=1.0;
      }
      set_drive(d);
    }
  }
  free(data);
  return 0;
}

static int band_pressed(void *data) {
  int function=*(int*)data;

  BANDSTACK_ENTRY *entry;
  if(function) {
    entry=bandstack_entry_next();
  } else {
    int b=band_get_current();
    b++;
    if(b>=BANDS) {
      b=0;
    }
    BAND* band=band_set_current(b);
    entry=bandstack_entry_get_current();
  }

  setFrequency(entry->frequencyA);
  setMode(entry->mode);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);

  BAND *band=band_get_current_band();
  set_alex_rx_antenna(band->alexRxAntenna);
  set_alex_tx_antenna(band->alexTxAntenna);
  set_alex_attenuation(band->alexAttenuation);
  vfo_update(NULL);


  free(data);
  return 0;
}

static void* rotary_encoder_thread(void *arg) {
    int pos;
    while(1) {

        pos=vfo_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(rotary_encoder_changed,(gpointer)p);
        }

        af_function=af_function_get_state();
        if(af_function!=previous_af_function) {
            fprintf(stderr,"af_function: %d\n",af_function);
            previous_af_function=af_function;
        }
        pos=af_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(af_encoder_changed,(gpointer)p);
        }

        rf_function=rf_function_get_state();
        if(rf_function!=previous_rf_function) {
            fprintf(stderr,"rf_function: %d\n",rf_function);
            previous_rf_function=rf_function;
        }
        pos=rf_encoder_get_pos();
        if(pos!=0) {
            int *p=malloc(sizeof(int));
            *p=pos;
            g_idle_add(rf_encoder_changed,(gpointer)p);
        }

        int band_button=band_get_state();
        if(band_button!=previous_band_button) {
            fprintf(stderr,"band_button: %d\n",band_button);
            previous_band_button=band_button;
            if(band_button) {
                int function=function_get_state();
                g_idle_add(band_pressed,(gpointer)function);
            }
        }

#ifdef raspberrypi
        gpioDelay(100000); // 10 per second
#endif
#ifdef odroid
        usleep(100000);
#endif
    }
}

static gboolean vfo_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
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
        cairo_set_font_size(cr, 36);

        if(isTransmitting()) {
            cairo_set_source_rgb(cr, 1, 0, 0);
        } else {
            cairo_set_source_rgb(cr, 0, 1, 0);
        }

        char sf[32];
        sprintf(sf,"%0lld.%06lld MHz %s %s",entry->frequencyA/(long long)1000000,entry->frequencyA%(long long)1000000,mode_string[entry->mode],band_filter->title);
        cairo_move_to(cr, 130, 45);  
        cairo_show_text(cr, sf);

        cairo_set_font_size(cr, 18);
        sprintf(sf,"Step %dHz",step);
        cairo_move_to(cr, 10, 25);  
        cairo_show_text(cr, sf);

        if(locked) {
            cairo_set_source_rgb(cr, 1, 0, 0);
            cairo_move_to(cr, 10, 50);  
            cairo_show_text(cr, "Locked");
        }

        cairo_destroy (cr);
        gtk_widget_queue_draw (vfo);
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
  GtkWidget *dialog=gtk_dialog_new_with_buttons("VFO",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

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
    gtk_grid_attach(GTK_GRID(grid),step_rb,i/5,i%5,1,1);
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
  
  return TRUE;
}

GtkWidget* vfo_init(int width,int height,GtkWidget *parent) {

  parent_window=parent;

  vfo = gtk_drawing_area_new ();
  /* set a minimum size */
  gtk_widget_set_size_request (vfo, width, height);

  /* Signals used to handle the backing surface */
  g_signal_connect (vfo, "draw",
            G_CALLBACK (vfo_draw_cb), NULL);
  g_signal_connect (vfo,"configure-event",
            G_CALLBACK (vfo_configure_event_cb), NULL);

  if(encoder_init() == 0) {
    int rc=pthread_create(&rotary_encoder_thread_id, NULL, rotary_encoder_thread, NULL);
    if(rc<0) {
      fprintf(stderr,"pthread_create for rotary_encoder_thread failed %d\n",rc);
    }
  } else {
    fprintf(stderr,"encoder_init failed\n");
  }

  /* Event signals */
  g_signal_connect (vfo, "button-press-event",
            G_CALLBACK (vfo_press_event_cb), NULL);
  gtk_widget_set_events (vfo, gtk_widget_get_events (vfo)
                     | GDK_BUTTON_PRESS_MASK);

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
