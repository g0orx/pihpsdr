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
#include <semaphore.h>
#include <string.h>
#include "radio.h"
#include "vfo.h"
#include "waterfall.h"

static GtkWidget *waterfall;
static GdkPixbuf *pixbuf = NULL;

static int colorLowR=0; // black
static int colorLowG=0;
static int colorLowB=0;

static int colorMidR=255; // red
static int colorMidG=0;
static int colorMidB=0;

static int colorHighR=255; // yellow
static int colorHighG=255;
static int colorHighB=0;


static gint first_x;
static gint last_x;
static gboolean has_moved=FALSE;
static gboolean pressed=FALSE;

static gfloat hz_per_pixel;

#define BANDS 7

static long long frequency[BANDS];
static gint mode[BANDS];
static gint band=4;

static int display_width;
static int display_height;

/* Create a new surface of the appropriate size to store our scribbles */
static gboolean
waterfall_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  display_width=gtk_widget_get_allocated_width (widget);
  display_height=gtk_widget_get_allocated_height (widget);
  pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, display_width, display_height);

  char *pixels = gdk_pixbuf_get_pixels (pixbuf);

  memset(pixels, 0, display_width*display_height*3);

  return TRUE;
}

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean
waterfall_draw_cb (GtkWidget *widget,
 cairo_t   *cr,
 gpointer   data)
{
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  return TRUE;
}

static gboolean
waterfall_button_press_event_cb (GtkWidget      *widget,
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
waterfall_button_release_event_cb (GtkWidget      *widget,
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
      vfo_move_to((int)((float)(x-(display_width/2))*hz_per_pixel));
    }
    last_x=x;
    pressed=FALSE;
  }
  return TRUE;
}

static gboolean waterfall_motion_notify_event_cb (GtkWidget      *widget,
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

static gboolean
waterfall_scroll_event_cb (GtkWidget      *widget,
               GdkEventScroll *event,
               gpointer        data)
{
  if(event->direction==GDK_SCROLL_UP) {
    vfo_move(step);
  } else {
    vfo_move(-step);
  }
}

void waterfall_update(float *data) {

  int i;

  hz_per_pixel=(double)getSampleRate()/(double)display_width;

  if(pixbuf) {
    char *pixels = gdk_pixbuf_get_pixels (pixbuf);

    int width=gdk_pixbuf_get_width(pixbuf);
    int height=gdk_pixbuf_get_height(pixbuf);
    int rowstride=gdk_pixbuf_get_rowstride(pixbuf);
    int channels=gdk_pixbuf_get_n_channels(pixbuf);

    memmove(&pixels[rowstride],pixels,(height-1)*rowstride);

    float sample;
    int average=0;
    char *p;
    p=pixels;
    for(i=0;i<width;i++) {
            sample=data[i]+get_attenuation();
            average+=(int)sample;
            if(sample<(float)waterfall_low) {
                *p++=colorLowR;
                *p++=colorLowG;
                *p++=colorLowB;
            } else if(sample>(float)waterfall_high) {
                *p++=colorHighR;
                *p++=colorHighG;
                *p++=colorHighB;
            } else {
                float range=(float)waterfall_high-(float)waterfall_low;
                float offset=sample-(float)waterfall_low;
                float percent=offset/range;
                if(percent<(2.0f/9.0f)) {
                    float local_percent = percent / (2.0f/9.0f);
                    *p++ = (int)((1.0f-local_percent)*colorLowR);
                    *p++ = (int)((1.0f-local_percent)*colorLowG);
                    *p++ = (int)(colorLowB + local_percent*(255-colorLowB));
                } else if(percent<(3.0f/9.0f)) {
                    float local_percent = (percent - 2.0f/9.0f) / (1.0f/9.0f);
                    *p++ = 0;
                    *p++ = (int)(local_percent*255);
                    *p++ = 255;
                } else if(percent<(4.0f/9.0f)) {
                     float local_percent = (percent - 3.0f/9.0f) / (1.0f/9.0f);
                     *p++ = 0;
                     *p++ = 255;
                     *p++ = (int)((1.0f-local_percent)*255);
                } else if(percent<(5.0f/9.0f)) {
                     float local_percent = (percent - 4.0f/9.0f) / (1.0f/9.0f);
                     *p++ = (int)(local_percent*255);
                     *p++ = 255;
                     *p++ = 0;
                } else if(percent<(7.0f/9.0f)) {
                     float local_percent = (percent - 5.0f/9.0f) / (2.0f/9.0f);
                     *p++ = 255;
                     *p++ = (int)((1.0f-local_percent)*255);
                     *p++ = 0;
                } else if(percent<(8.0f/9.0f)) {
                     float local_percent = (percent - 7.0f/9.0f) / (1.0f/9.0f);
                     *p++ = 255;
                     *p++ = 0;
                     *p++ = (int)(local_percent*255);
                } else {
                     float local_percent = (percent - 8.0f/9.0f) / (1.0f/9.0f);
                     *p++ = (int)((0.75f + 0.25f*(1.0f-local_percent))*255.0f);
                     *p++ = (int)(local_percent*255.0f*0.5f);
                     *p++ = 255;
                }
            }
        
    }

    
    if(waterfall_automatic) {
      waterfall_low=average/display_width;
      waterfall_high=waterfall_low+50;
    }

    gtk_widget_queue_draw (waterfall);
  }
}

GtkWidget* waterfall_init(int width,int height) {
  display_width=width;
  display_height=height;

  hz_per_pixel=(double)getSampleRate()/(double)display_width;

  //waterfall_frame = gtk_frame_new (NULL);
  waterfall = gtk_drawing_area_new ();
  gtk_widget_set_size_request (waterfall, width, height);

  /* Signals used to handle the backing surface */
  g_signal_connect (waterfall, "draw",
            G_CALLBACK (waterfall_draw_cb), NULL);
  g_signal_connect (waterfall,"configure-event",
            G_CALLBACK (waterfall_configure_event_cb), NULL);


  /* Event signals */
  g_signal_connect (waterfall, "motion-notify-event",
            G_CALLBACK (waterfall_motion_notify_event_cb), NULL);
  g_signal_connect (waterfall, "button-press-event",
            G_CALLBACK (waterfall_button_press_event_cb), NULL);
  g_signal_connect (waterfall, "button-release-event",
            G_CALLBACK (waterfall_button_release_event_cb), NULL);
  g_signal_connect(waterfall,"scroll_event",
            G_CALLBACK(waterfall_scroll_event_cb),NULL);

  /* Ask to receive events the drawing area doesn't normally
   * subscribe to. In particular, we need to ask for the
   * button press and motion notify events that want to handle.
   */
  gtk_widget_set_events (waterfall, gtk_widget_get_events (waterfall)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_BUTTON_RELEASE_MASK
                     | GDK_BUTTON1_MOTION_MASK
                     | GDK_SCROLL_MASK
                     | GDK_POINTER_MOTION_MASK
                     | GDK_POINTER_MOTION_HINT_MASK);

  return waterfall;
}
