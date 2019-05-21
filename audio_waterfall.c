/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
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

#include <wdsp.h>

#include "channel.h"
#include "filter.h"
#include "radio.h"
#include "vfo.h"
#include "audio_waterfall.h"
#include "receiver.h"

#define min(x,y) (x<y?x:y)


static GtkWidget *waterfall;
static GdkPixbuf *pixbuf = NULL;

float *audio_samples=NULL;
int audio_samples_index=0;

static int colorLowR=0; // black
static int colorLowG=0;
static int colorLowB=0;

static int colorMidR=255; // red
static int colorMidG=0;
static int colorMidB=0;

static int colorHighR=255; // yellow
static int colorHighG=255;
static int colorHighB=0;

static int sample_rate;
static gfloat hz_per_pixel;
static int cursor_frequency=0;
static int cursor_x=0;
static int header=20;

// fixed for freedv waterfall?
static int audio_waterfall_low=30;
static int audio_waterfall_high=90;

static int display_width;
static int waterfall_height;

/* Create a new surface of the appropriate size to store our scribbles */
static gboolean
waterfall_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  int width=gtk_widget_get_allocated_width (widget);
  int height=gtk_widget_get_allocated_height (widget);
fprintf(stderr,"audio: waterfall_configure_event: width=%d height=%d\n",width,height);
  pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
  unsigned char *pixels = gdk_pixbuf_get_pixels (pixbuf);

  memset(pixels, 0, width*height*3);

  if(cursor_frequency!=0) {
    hz_per_pixel=(double)(sample_rate/2)/(double)display_width;
    cursor_x=(int)((double)cursor_frequency/hz_per_pixel);
fprintf(stderr,"audio: waterfall_configure_event: cursor_x=%d hz_per_pizel=%f\n",cursor_x,hz_per_pixel);
  }

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
  cairo_text_extents_t extents;
  char text[16];

//fprintf(stderr,"waterfall_draw_cb\n");
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);

  // draw the sursor
  if(cursor_x!=0) {
    cairo_set_source_rgb (cr, 1, 0, 0);
    sprintf(text,"%d Hz",cursor_frequency);
    cairo_select_font_face(cr, "FreeMono",
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, (double)cursor_x-(extents.width/2.0), 18.0);
    cairo_show_text(cr,text);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, cursor_x,  header);
    cairo_line_to(cr, cursor_x,  waterfall_height);
    cairo_stroke(cr);
  }

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_rectangle(cr, 0.0, 0.0, (double)(display_width-1), (double)(waterfall_height-1));
  cairo_stroke(cr);
  return TRUE;
}

void audio_waterfall_update() {

  int i;

  if(pixbuf) {
    unsigned char *pixels = gdk_pixbuf_get_pixels (pixbuf);

    int width=gdk_pixbuf_get_width(pixbuf);
    int height=gdk_pixbuf_get_height(pixbuf);
    int rowstride=gdk_pixbuf_get_rowstride(pixbuf);
    int channels=gdk_pixbuf_get_n_channels(pixbuf);

//fprintf(stderr,"audio_waterfall_update: width=%d height=%d rowsstride=%d channels=%d\n",width,height,rowstride,channels);

    memmove(&pixels[rowstride*(header+1)],&pixels[rowstride*header],(height-(header+1))*rowstride);

    float sample;
    unsigned char *p;
    int average=0;
    p=&pixels[rowstride*header];
    for(i=0;i<width;i++) {
        sample=audio_samples[i];
        if(sample<(float)audio_waterfall_low) {
            *p++=colorLowR;
            *p++=colorLowG;
            *p++=colorLowB;
        } else if(sample>(float)audio_waterfall_high) {
            *p++=colorHighR;
            *p++=colorHighG;
            *p++=colorHighB;
        } else {
            float range=(float)audio_waterfall_high-(float)audio_waterfall_low;
            float offset=sample-(float)audio_waterfall_low;
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
        average+=(int)sample;
        audio_waterfall_low=(average/width)+10;
        audio_waterfall_high=audio_waterfall_low+50;
    }
    gtk_widget_queue_draw (waterfall);
  }
}

void audio_waterfall_setup(int rate,int cursor) {
  sample_rate=rate;
  cursor_frequency=cursor;
  hz_per_pixel=(double)(sample_rate/2)/(double)display_width;
  cursor_x=(int)((double)cursor/hz_per_pixel);
fprintf(stderr,"audio_waterfall_setup: sample_rate=%d cursor=%d width=%d hz_per_pixel=%f\n",sample_rate,cursor,display_width,hz_per_pixel);
}


static void initAnalyzer(RECEIVER *rx,int channel, int buffer_size, int pixels) {
    int flp[] = {0};
    double keep_time = 0.1;
    int n_pixout=1;
    int spur_elimination_ffts = 1;
    int data_type = 0;
    int fft_size = 1024;
    int window_type = 4;
    double kaiser_pi = 14.0;
    int overlap = 0;
    int clip = 0;
    int span_clip_l = 0;
    int span_clip_h = 0;
    int stitches = 1;
    int calibration_data_set = 0;
    double span_min_freq = 0.0;
    double span_max_freq = 0.0;

    int max_w = fft_size + (int) min(keep_time * (double) rx->fps, keep_time * (double) fft_size * (double) rx->fps);

    SetAnalyzer(channel,
            n_pixout,
            spur_elimination_ffts, //number of LO frequencies = number of ffts used in elimination
            data_type, //0 for real input data (I only); 1 for complex input data (I & Q)
            flp, //vector with one elt for each LO frequency, 1 if high-side LO, 0 otherwise
            fft_size, //size of the fft, i.e., number of input samples
            buffer_size, //number of samples transferred for each OpenBuffer()/CloseBuffer()
            window_type, //integer specifying which window function to use
            kaiser_pi, //PiAlpha parameter for Kaiser window
            overlap, //number of samples each fft (other than the first) is to re-use from the previous
            clip, //number of fft output bins to be clipped from EACH side of each sub-span
            span_clip_l, //number of bins to clip from low end of entire span
            span_clip_h, //number of bins to clip from high end of entire span
            pixels, //number of pixel values to return.  may be either <= or > number of bins
            stitches, //number of sub-spans to concatenate to form a complete span
            calibration_data_set, //identifier of which set of calibration data to use
            span_min_freq, //frequency at first pixel value8192
            span_max_freq, //frequency at last pixel value
            max_w //max samples to hold in input ring buffers
    );

}

GtkWidget* audio_waterfall_init(int width,int height) {
  int success;

fprintf(stderr,"audio_waterfall_init: width=%d height=%d\n",width,height);
  display_width=width;
  waterfall_height=height;

  audio_samples=(float *)malloc(sizeof(float)*width);

  //waterfall_frame = gtk_frame_new (NULL);
  waterfall = gtk_drawing_area_new ();
  gtk_widget_set_size_request (waterfall, width, height);

  /* Signals used to handle the backing surface */
  g_signal_connect (waterfall, "draw",
            G_CALLBACK (waterfall_draw_cb), NULL);
  g_signal_connect (waterfall,"configure-event",
            G_CALLBACK (waterfall_configure_event_cb), NULL);

  XCreateAnalyzer(CHANNEL_AUDIO, &success, 262144, 1, 1, "");
  if (success != 0) {
    fprintf(stderr, "XCreateAnalyzer CHANNEL_AUDIO failed: %d\n" ,success);
  }
  initAnalyzer(active_receiver,CHANNEL_AUDIO,AUDIO_WATERFALL_SAMPLES,width);

  return waterfall;
}
