#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include "new_protocol.h"
#include "panadapter.h"
#include "vfo.h"

static GtkWidget *panadapter;
static cairo_surface_t *panadapter_surface = NULL;

static float* samples;

static gint last_x;
static gboolean has_moved=FALSE;
static gboolean pressed=FALSE;

static float panadapter_max=-60.0;
static float panadapter_min=-160.0;

static gfloat hz_per_pixel;
static gfloat filter_left;
static gfloat filter_right;

static int display_width;
static int panadapter_height;

static void
panadapter_clear_surface (void)
{
  cairo_t *cr;

  cr = cairo_create (panadapter_surface);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
}

/* Create a new surface of the appropriate size to store our scribbles */
static gboolean
panadapter_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  if (panadapter_surface)
    cairo_surface_destroy (panadapter_surface);

  panadapter_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));

  /* Initialize the surface to white */
  panadapter_clear_surface ();

  /* We've handled the configure event, no need for further processing. */
  return TRUE;
}

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean
panadapter_draw_cb (GtkWidget *widget,
 cairo_t   *cr,
 gpointer   data)
{
  cairo_set_source_surface (cr, panadapter_surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
}

static gboolean
panadapter_button_press_event_cb (GtkWidget      *widget,
               GdkEventButton *event,
               gpointer        data)
{
  int x=(int)event->x;
  if (event->button == 1) {
    last_x=(int)event->x;
    has_moved=FALSE;
    pressed=TRUE;
  }
  return TRUE;
}

static gboolean
panadapter_button_release_event_cb (GtkWidget      *widget,
               GdkEventButton *event,
               gpointer        data)
{
  int x=(int)event->x;
  if (event->button == 1) {
    if(has_moved) {
      // drag
      vfo_move((int)((float)(x-last_x)*hz_per_pixel));
    } else {
      // move to this frequency
      vfo_move((int)((float)(x-(display_width/2))*hz_per_pixel));
    }
    last_x=x;
    pressed=FALSE;
  }
  return TRUE;
}

static gboolean
panadapter_motion_notify_event_cb (GtkWidget      *widget,
                GdkEventMotion *event,
                gpointer        data)
{
  int x, y;
  GdkModifierType state;
  gdk_window_get_device_position (event->window,
                                event->device,
                                &x,
                                &y,
                                &state);
  if((state & GDK_BUTTON1_MASK == GDK_BUTTON1_MASK) || pressed) {
    int moved=last_x-x;
    vfo_move((int)((float)moved*hz_per_pixel));
    last_x=x;
    has_moved=TRUE;
  }

  return TRUE;
}

static void
close_window (void)
{
  if (panadapter_surface)
    cairo_surface_destroy (panadapter_surface);

  gtk_main_quit ();
}

void panadapter_update(float *data,int tx) {
    int i;
    int result;
    
    float saved_max;
    float saved_min;
    gfloat saved_hz_per_pixel;

    samples=data;
    //if(result==1) {
        if(panadapter_surface) {

            if(tx) {
                saved_max=panadapter_max;
                saved_min=panadapter_min;
                saved_hz_per_pixel=hz_per_pixel;

                panadapter_max=30;
                panadapter_min=-100;
                hz_per_pixel=192000.0/(double)display_width;
            }

            //clear_panadater_surface();
            cairo_t *cr;
            cr = cairo_create (panadapter_surface);
            cairo_set_source_rgb (cr, 0, 0, 0);
            cairo_paint (cr);

            // filter
            cairo_set_source_rgb (cr, 0.25, 0.25, 0.25);
            filter_left=(double)display_width/2.0+((double)getFilterLow()/hz_per_pixel);
            filter_right=(double)display_width/2.0+((double)getFilterHigh()/hz_per_pixel);
            cairo_rectangle(cr, filter_left, 0.0, filter_right-filter_left, (double)panadapter_height);
            cairo_fill(cr);

            // plot the levels
            int V = (int)(panadapter_max - panadapter_min);
            int numSteps = V / 20;
            for (i = 1; i < numSteps; i++) {
                int num = panadapter_max - i * 20;
                int y = (int)floor((panadapter_max - num) * panadapter_height / V);

                cairo_set_source_rgb (cr, 0, 1, 1);
                cairo_set_line_width(cr, 1.0);
                cairo_move_to(cr,0.0,(double)y);
                cairo_line_to(cr,(double)display_width,(double)y);

                cairo_set_source_rgb (cr, 0, 1, 1);
                cairo_select_font_face(cr, "Arial",
                    CAIRO_FONT_SLANT_NORMAL,
                    CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 12);
                char v[32];
                sprintf(v,"%d dBm",num);
                cairo_move_to(cr, 1, (double)y);  
                cairo_show_text(cr, v);
            }
            cairo_stroke(cr);

            // plot frequency markers
            long f;
            for(i=0;i<display_width;i++) {
                f = getFrequency() - ((long) getSampleRate() / 2) + (long) (hz_per_pixel * i);
                if (f > 0) {
                    if ((f % 20000) < (long) hz_per_pixel) {
                        cairo_set_source_rgb (cr, 0, 1, 1);
                        cairo_set_line_width(cr, 1.0);
                        cairo_move_to(cr,(double)i,0.0);
                        cairo_line_to(cr,(double)i,(double)panadapter_height);

                        cairo_set_source_rgb (cr, 0, 1, 1);
                        cairo_select_font_face(cr, "Arial",
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
                        cairo_set_font_size(cr, 12);
                        char v[32];
                        sprintf(v,"%0ld.%03ld",f/1000000,(f%1000000)/1000);
                        cairo_move_to(cr, (double)i, (double)(panadapter_height-10));  
                        cairo_show_text(cr, v);
                    }
                }
            }
            cairo_stroke(cr);

            // cursor
            cairo_set_source_rgb (cr, 1, 0, 0);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr,(double)display_width/2.0,0.0);
            cairo_line_to(cr,(double)display_width/2.0,(double)panadapter_height);
            cairo_stroke(cr);

            // signal
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_set_line_width(cr, 1.0);

            double s1,s2;
            samples[0]=panadapter_min-20;
            samples[display_width-1]=panadapter_min-20;
            for(i=1;i<display_width;i++) {
                s1=samples[i-1]+get_attenuation();
                s1 = floor((panadapter_max - s1)
                            * (double) panadapter_height
                            / (panadapter_max - panadapter_min));
                s2=samples[i]+get_attenuation();
                s2 = floor((panadapter_max - s2)
                            * (double) panadapter_height
                            / (panadapter_max - panadapter_min));
                cairo_move_to(cr, (double)i-1, s1);
                cairo_line_to(cr, (double)i, s2);
            }
            cairo_stroke(cr);
            //cairo_close_path(cr);
            //cairo_fill(cr);

            cairo_destroy (cr);
            gtk_widget_queue_draw (panadapter);

            if(tx) {
                panadapter_max=saved_max;
                panadapter_min=saved_min;
                hz_per_pixel=saved_hz_per_pixel;
            }

        }
    //}
}

GtkWidget* panadapter_init(int width,int height) {
  //GtkWidget *panadapter_frame;

  display_width=width;
  panadapter_height=height;

  samples=malloc(display_width*sizeof(float));
  hz_per_pixel=(double)getSampleRate()/(double)display_width;

  //panadapter_frame = gtk_frame_new (NULL);
  panadapter = gtk_drawing_area_new ();
  gtk_widget_set_size_request (panadapter, width, height);

  //gtk_container_add (GTK_CONTAINER (panadapter_frame), panadapter);

  /* Signals used to handle the backing surface */
  g_signal_connect (panadapter, "draw",
            G_CALLBACK (panadapter_draw_cb), NULL);
  g_signal_connect (panadapter,"configure-event",
            G_CALLBACK (panadapter_configure_event_cb), NULL);

  /* Event signals */
  g_signal_connect (panadapter, "motion-notify-event",
            G_CALLBACK (panadapter_motion_notify_event_cb), NULL);
  g_signal_connect (panadapter, "button-press-event",
            G_CALLBACK (panadapter_button_press_event_cb), NULL);
  g_signal_connect (panadapter, "button-release-event",
            G_CALLBACK (panadapter_button_release_event_cb), NULL);

  /* Ask to receive events the drawing area doesn't normally
   * subscribe to. In particular, we need to ask for the
   * button press and motion notify events that want to handle.
   */
  gtk_widget_set_events (panadapter, gtk_widget_get_events (panadapter)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_BUTTON_RELEASE_MASK
                     | GDK_BUTTON1_MOTION_MASK
                     | GDK_POINTER_MOTION_MASK
                     | GDK_POINTER_MOTION_HINT_MASK);

  //return panadapter_frame;
  return panadapter;
}
