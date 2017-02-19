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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <wdsp.h>

#include "agc.h"
#include "band.h"
#include "bandstack.h"
#include "discovered.h"
#include "filter.h"
#include "main.h"
#include "meter.h"
#include "mode.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef LIME_PROTOOCL
#include "lime_protocol.h"
#endif
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "meter.h"
#include "rx_panadapter.h"
#include "sliders.h"
#include "waterfall.h"
#ifdef FREEDV
#include "freedv.h"
#endif
#ifdef RADIOBERRY
#include "radioberry.h"
#endif


#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

#ifdef FREEDV
static int freedv_samples=0;
static int freedv_resample=6;  // convert from 48000 to 8000
#endif
#ifdef PSK
static int psk_samples=0;
static int psk_resample=6;  // convert from 48000 to 8000
#endif

static gint last_x;
static gboolean has_moved=FALSE;
static gboolean pressed=FALSE;
static gboolean making_active=FALSE;

gboolean receiver_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  if(rx==active_receiver) {
    int x=(int)event->x;
    if (event->button == 1) {
      last_x=(int)event->x;
      has_moved=FALSE;
      pressed=TRUE;
    }
  } else {
    making_active=TRUE;
  }
  return TRUE;
}

gboolean receiver_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  if(making_active) {
    active_receiver=rx;
    making_active=FALSE;
    g_idle_add(vfo_update,NULL);
    g_idle_add(active_receiver_changed,NULL);
  } else {
    int display_width=gtk_widget_get_allocated_width (rx->panadapter);
    int display_height=gtk_widget_get_allocated_height (rx->panadapter);
    if(pressed) {
      int x=(int)event->x;
      if (event->button == 1) {
        if(has_moved) {
          // drag
          vfo_move((long long)((float)(x-last_x)*rx->hz_per_pixel));
        } else {
          // move to this frequency
          vfo_move_to((long long)((float)(x-(display_width/2))*rx->hz_per_pixel));
        }
        last_x=x;
        pressed=FALSE;
      }
    }
  }
  return TRUE;
}

gboolean receiver_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
  int x, y;
  GdkModifierType state;
  RECEIVER *rx=(RECEIVER *)data;
  if(!making_active) {
    gdk_window_get_device_position (event->window,
                                event->device,
                                &x,
                                &y,
                                &state);
    if((state & GDK_BUTTON1_MASK == GDK_BUTTON1_MASK) || pressed) {
      int moved=last_x-x;
      vfo_move((long long)((float)moved*rx->hz_per_pixel));
      last_x=x;
      has_moved=TRUE;
    }
  }

  return TRUE;
}

gboolean receiver_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer data) {
  if(event->direction==GDK_SCROLL_UP) {
    if(vfo[active_receiver->id].ctun) {
      vfo_move(-step);
    } else {
      vfo_move(step);
    }
  } else {
    if(vfo[active_receiver->id].ctun) {
      vfo_move(step);
    } else {
      vfo_move(-step);
    }
  }
  return TRUE;
}

void receiver_save_state(RECEIVER *rx) {
  char name[128];
  char value[128];

  sprintf(name,"receiver.%d.sample_rate",rx->id);
  sprintf(value,"%d",rx->sample_rate);
  setProperty(name,value);
  sprintf(name,"receiver.%d.adc",rx->id);
  sprintf(value,"%d",rx->adc);
  setProperty(name,value);
  sprintf(name,"receiver.%d.filter_low",rx->id);
  sprintf(value,"%d",rx->filter_low);
  setProperty(name,value);
  sprintf(name,"receiver.%d.filter_high",rx->id);
  sprintf(value,"%d",rx->filter_high);
  setProperty(name,value);
  sprintf(name,"receiver.%d.fps",rx->id);
  sprintf(value,"%d",rx->fps);
  setProperty(name,value);
  sprintf(name,"receiver.%d.panadapter_low",rx->id);
  sprintf(value,"%d",rx->panadapter_low);
  setProperty(name,value);
  sprintf(name,"receiver.%d.panadapter_high",rx->id);
  sprintf(value,"%d",rx->panadapter_high);
  setProperty(name,value);
  sprintf(name,"receiver.%d.display_waterfall",rx->id);
  sprintf(value,"%d",rx->display_waterfall);
  setProperty(name,value);
  sprintf(name,"receiver.%d.waterfall_low",rx->id);
  sprintf(value,"%d",rx->waterfall_low);
  setProperty(name,value);
  sprintf(name,"receiver.%d.waterfall_high",rx->id);
  sprintf(value,"%d",rx->waterfall_high);
  setProperty(name,value);
  sprintf(name,"receiver.%d.waterfall_automatic",rx->id);
  sprintf(value,"%d",rx->waterfall_automatic);
  setProperty(name,value);
  
  sprintf(name,"receiver.%d.alex_antenna",rx->id);
  sprintf(value,"%d",rx->alex_antenna);
  setProperty(name,value);
  sprintf(name,"receiver.%d.alex_attenuation",rx->id);
  sprintf(value,"%d",rx->alex_attenuation);
  setProperty(name,value);
  sprintf(name,"receiver.%d.volume",rx->id);
  sprintf(value,"%f",rx->volume);
  setProperty(name,value);
  sprintf(name,"receiver.%d.agc",rx->id);
  sprintf(value,"%d",rx->agc);
  setProperty(name,value);
  sprintf(name,"receiver.%d.agc_gain",rx->id);
  sprintf(value,"%f",rx->agc_gain);
  setProperty(name,value);
  sprintf(name,"receiver.%d.agc_slope",rx->id);
  sprintf(value,"%f",rx->agc_slope);
  setProperty(name,value);
  sprintf(name,"receiver.%d.agc_hang_threshold",rx->id);
  sprintf(value,"%f",rx->agc_hang_threshold);
  setProperty(name,value);
  sprintf(name,"receiver.%d.attenuation",rx->id);
  sprintf(value,"%d",rx->attenuation);
  setProperty(name,value);

  sprintf(name,"receiver.%d.dither",rx->id);
  sprintf(value,"%d",rx->dither);
  setProperty(name,value);
  sprintf(name,"receiver.%d.random",rx->id);
  sprintf(value,"%d",rx->random);
  setProperty(name,value);
  sprintf(name,"receiver.%d.preamp",rx->id);
  sprintf(value,"%d",rx->preamp);
  setProperty(name,value);
  sprintf(name,"receiver.%d.nr",rx->id);
  sprintf(value,"%d",rx->nr);
  setProperty(name,value);
  sprintf(name,"receiver.%d.nr2",rx->id);
  sprintf(value,"%d",rx->nr2);
  setProperty(name,value);
  sprintf(name,"receiver.%d.anf",rx->id);
  sprintf(value,"%d",rx->anf);
  setProperty(name,value);
  sprintf(name,"receiver.%d.snb",rx->id);
  sprintf(value,"%d",rx->snb);
  setProperty(name,value);
  sprintf(name,"receiver.%d.nr_agc",rx->id);
  sprintf(value,"%d",rx->nr_agc);
  setProperty(name,value);
  sprintf(name,"receiver.%d.nr2_gain_method",rx->id);
  sprintf(value,"%d",rx->nr2_gain_method);
  setProperty(name,value);
  sprintf(name,"receiver.%d.nr2_npe_method",rx->id);
  sprintf(value,"%d",rx->nr2_npe_method);
  setProperty(name,value);
  sprintf(name,"receiver.%d.nr2_ae",rx->id);
  sprintf(value,"%d",rx->nr2_ae);
  setProperty(name,value);

  sprintf(name,"receiver.%d.audio_channel",rx->id);
  sprintf(value,"%d",rx->audio_channel);
  setProperty(name,value);
  sprintf(name,"receiver.%d.local_audio",rx->id);
  sprintf(value,"%d",rx->local_audio);
  setProperty(name,value);
  sprintf(name,"receiver.%d.mute_when_not_active",rx->id);
  sprintf(value,"%d",rx->mute_when_not_active);
  setProperty(name,value);
  sprintf(name,"receiver.%d.audio_device",rx->id);
  sprintf(value,"%d",rx->audio_device);
  setProperty(name,value);

}

void receiver_restore_state(RECEIVER *rx) {
  char name[128];
  char *value;

fprintf(stderr,"receiver_restore_state: id=%d\n",rx->id);
  sprintf(name,"receiver.%d.sample_rate",rx->id);
  value=getProperty(name);
  if(value) rx->sample_rate=atoi(value);
  sprintf(name,"receiver.%d.adc",rx->id);
  value=getProperty(name);
  if(value) rx->adc=atoi(value);
  sprintf(name,"receiver.%d.filter_low",rx->id);
  value=getProperty(name);
  if(value) rx->filter_low=atoi(value);
  sprintf(name,"receiver.%d.filter_high",rx->id);
  value=getProperty(name);
  if(value) rx->filter_high=atoi(value);
  sprintf(name,"receiver.%d.fps",rx->id);
  value=getProperty(name);
  if(value) rx->fps=atoi(value);
/*
  sprintf(name,"receiver.%d.frequency",rx->id);
  value=getProperty(name);
  if(value) rx->frequency=atoll(value);
  sprintf(name,"receiver.%d.display_frequency",rx->id);
  value=getProperty(name);
  if(value) rx->display_frequency=atoll(value);
  sprintf(name,"receiver.%d.dds_frequency",rx->id);
  value=getProperty(name);
  if(value) rx->dds_frequency=atoll(value);
  sprintf(name,"receiver.%d.dds_offset",rx->id);
  value=getProperty(name);
  if(value) rx->dds_offset=atoll(value);
  sprintf(name,"receiver.%d.rit",rx->id);
  value=getProperty(name);
  if(value) rx->rit=atoi(value);
*/
  sprintf(name,"receiver.%d.panadapter_low",rx->id);
  value=getProperty(name);
  if(value) rx->panadapter_low=atoi(value);
  sprintf(name,"receiver.%d.panadapter_high",rx->id);
  value=getProperty(name);
  if(value) rx->panadapter_high=atoi(value);
  sprintf(name,"receiver.%d.display_waterfall",rx->id);
  value=getProperty(name);
  if(value) rx->display_waterfall=atoi(value);
  sprintf(name,"receiver.%d.waterfall_low",rx->id);
  value=getProperty(name);
  if(value) rx->waterfall_low=atoi(value);
  sprintf(name,"receiver.%d.waterfall_high",rx->id);
  value=getProperty(name);
  if(value) rx->waterfall_high=atoi(value);
  sprintf(name,"receiver.%d.waterfall_automatic",rx->id);
  value=getProperty(name);
  if(value) rx->waterfall_automatic=atoi(value);

  sprintf(name,"receiver.%d.alex_antenna",rx->id);
  value=getProperty(name);
  if(value) rx->alex_antenna=atoi(value);
  sprintf(name,"receiver.%d.alex_attenuation",rx->id);
  value=getProperty(name);
  if(value) rx->alex_attenuation=atoi(value);
  sprintf(name,"receiver.%d.volume",rx->id);
  value=getProperty(name);
  if(value) rx->volume=atof(value);
  sprintf(name,"receiver.%d.agc",rx->id);
  value=getProperty(name);
  if(value) rx->agc=atoi(value);
  sprintf(name,"receiver.%d.agc_gain",rx->id);
  value=getProperty(name);
  if(value) rx->agc_gain=atof(value);
  sprintf(name,"receiver.%d.agc_slope",rx->id);
  value=getProperty(name);
  if(value) rx->agc_slope=atof(value);
  sprintf(name,"receiver.%d.agc_hang_threshold",rx->id);
  value=getProperty(name);
  if(value) rx->agc_hang_threshold=atof(value);
  sprintf(name,"receiver.%d.attenuation",rx->id);
  value=getProperty(name);
  if(value) rx->attenuation=atoi(value);

  sprintf(name,"receiver.%d.dither",rx->id);
  value=getProperty(name);
  if(value) rx->dither=atoi(value);
  sprintf(name,"receiver.%d.random",rx->id);
  value=getProperty(name);
  if(value) rx->random=atoi(value);
  sprintf(name,"receiver.%d.preamp",rx->id);
  value=getProperty(name);
  if(value) rx->preamp=atoi(value);
  sprintf(name,"receiver.%d.nr",rx->id);
  value=getProperty(name);
  if(value) rx->nr=atoi(value);
  sprintf(name,"receiver.%d.nr2",rx->id);
  value=getProperty(name);
  if(value) rx->nr2=atoi(value);
  sprintf(name,"receiver.%d.anf",rx->id);
  value=getProperty(name);
  if(value) rx->anf=atoi(value);
  sprintf(name,"receiver.%d.snb",rx->id);
  value=getProperty(name);
  if(value) rx->snb=atoi(value);
  sprintf(name,"receiver.%d.nr_agc",rx->id);
  value=getProperty(name);
  if(value) rx->nr_agc=atoi(value);
  sprintf(name,"receiver.%d.nr2_gain_method",rx->id);
  value=getProperty(name);
  if(value) rx->nr2_gain_method=atoi(value);
  sprintf(name,"receiver.%d.nr2_npe_method",rx->id);
  value=getProperty(name);
  if(value) rx->nr2_npe_method=atoi(value);
  sprintf(name,"receiver.%d.ae",rx->id);
  value=getProperty(name);
  if(value) rx->nr2_ae=atoi(value);

  sprintf(name,"receiver.%d.audio_channel",rx->id);
  value=getProperty(name);
  if(value) rx->audio_channel=atoi(value);
  sprintf(name,"receiver.%d.local_audio",rx->id);
  value=getProperty(name);
  if(value) rx->local_audio=atoi(value);
  sprintf(name,"receiver.%d.mute_when_not_active",rx->id);
  value=getProperty(name);
  if(value) rx->mute_when_not_active=atoi(value);
  sprintf(name,"receiver.%d.audio_device",rx->id);
  value=getProperty(name);
  if(value) rx->audio_device=atoi(value);
}

void reconfigure_receiver(RECEIVER *rx,int height) {
  int y=0;

  rx->height=height;

  if(rx->display_panadapter) {
    int height=rx->height;
    if(rx->display_waterfall) {
      height=height/2;
    }
    if(rx->panadapter==NULL) {
fprintf(stderr,"reconfigure_receiver: panadapter_init: width:%d height:%d\n",rx->width,height);
      rx_panadapter_init(rx, rx->width,height);
      gtk_fixed_put(GTK_FIXED(rx->panel),rx->panadapter,0,y);
    } else {
       // set the size
fprintf(stderr,"reconfigure_receiver: panadapter set_size_request: width:%d height:%d\n",rx->width,height);
      gtk_widget_set_size_request(rx->panadapter, rx->width, height);
      // move the current one
      gtk_fixed_move(GTK_FIXED(rx->panel),rx->panadapter,0,y);
    }
    y+=height;
  } else {
    if(rx->panadapter!=NULL) {
      gtk_container_remove(GTK_CONTAINER(rx->panel),rx->panadapter);
      gtk_widget_destroy(rx->panadapter);
      rx->panadapter=NULL;
    }
  }

  if(rx->display_waterfall) {
    int height=rx->height;
    if(rx->display_panadapter) {
      height=height/2;
    }
    if(rx->waterfall==NULL) {
fprintf(stderr,"reconfigure_receiver: waterfall_init: width:%d height:%d\n",rx->width,height);
      waterfall_init(rx,rx->width,height);
      gtk_fixed_put(GTK_FIXED(rx->panel),rx->waterfall,0,y);
    } else {
      // set the size
fprintf(stderr,"reconfigure_receiver: waterfall set_size_request: width:%d height:%d\n",rx->width,height);
      gtk_widget_set_size_request(rx->waterfall, rx->width, height);
      // move the current one
      gtk_fixed_move(GTK_FIXED(rx->panel),rx->waterfall,0,y);
    }
  } else {
    if(rx->waterfall!=NULL) {
      gtk_container_remove(GTK_CONTAINER(rx->panel),rx->waterfall);
      gtk_widget_destroy(rx->waterfall);
      rx->waterfall=NULL;
    }
  }

  gtk_widget_show_all(rx->panel);
}

static gint update_display(gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  int rc;

  if(rx->displaying) {
    GetPixels(rx->id,0,rx->pixel_samples,&rc);
    if(rc) {
      if(rx->display_panadapter) {
        switch(vfo[rx->id].mode) {
#ifdef PSK
          case modePSK:
            psk_waterfall_update(rx);
            break;
#endif
          default:
            rx_panadapter_update(rx);
            break;
        }
      }
      if(rx->display_waterfall) {
        waterfall_update(rx);
      }
    }

    if(active_receiver==rx) {
      double m=GetRXAMeter(rx->id,smeter);
      meter_update(SMETER,m,0.0,0.0,0.0);
    }
    return TRUE;
  }
  return FALSE;
}

void set_displaying(RECEIVER *rx,int state) {
  rx->displaying=state;
  if(state) {
    rx->update_timer_id=gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE,1000/rx->fps, update_display, rx, NULL);
  }
}

void set_mode(RECEIVER *rx,int m) {
  int local_mode=m;
#ifdef FREEDV
  if(vfo[rx->d].mode!=modeFREEDV && m==modeFREEDV) {
    local_mode=modeUSB;
    init_freedv();
  } else if(vfo[rx->id].mode==modeFREEDV && m!=modeFREEDV) {
    close_freedv();
  }
#endif
#ifdef PSK
  if(vfo[rx->id].mode!=modePSK && m==modePSK) {
    local_mode=modeUSB;
    //init_psk();
    show_psk();
  } else if(vfo[rx->id].mode==modePSK && m!=modePSK) {
    //close_psk();
    show_waterfall();
  }
#endif
  vfo[rx->id].mode=m;
  SetRXAMode(rx->id, vfo[rx->id].mode);
}

void set_filter(RECEIVER *rx,int low,int high) {
  if(vfo[rx->id].mode==modeCWL) {
    rx->filter_low=-cw_keyer_sidetone_frequency-low;
    rx->filter_high=-cw_keyer_sidetone_frequency+high;
  } else if(vfo[rx->id].mode==modeCWU) {
    rx->filter_low=cw_keyer_sidetone_frequency-low;
    rx->filter_high=cw_keyer_sidetone_frequency+high;
  } else {
    rx->filter_low=low;
    rx->filter_high=high;
  }

  RXASetPassband(rx->id,(double)rx->filter_low,(double)rx->filter_high);
}

void set_deviation(RECEIVER *rx,double deviation) {
  SetRXAFMDeviation(rx->id, deviation);
}

void set_agc(RECEIVER *rx, int agc) {
 
  SetRXAAGCMode(rx->id, agc);
  //SetRXAAGCThresh(rx->id, agc_thresh_point, 4096.0, rx->sample_rate);
  SetRXAAGCSlope(rx->id,rx->agc_slope);
  SetRXAAGCTop(rx->id,rx->agc_gain);
  switch(agc) {
    case AGC_OFF:
      break;
    case AGC_LONG:
      SetRXAAGCAttack(rx->id,2);
      SetRXAAGCHang(rx->id,2000);
      SetRXAAGCDecay(rx->id,2000);
      SetRXAAGCHangThreshold(rx->id,(int)rx->agc_hang_threshold);
      break;
    case AGC_SLOW:
      SetRXAAGCAttack(rx->id,2);
      SetRXAAGCHang(rx->id,1000);
      SetRXAAGCDecay(rx->id,500);
      SetRXAAGCHangThreshold(rx->id,(int)rx->agc_hang_threshold);
      break;
    case AGC_MEDIUM:
      SetRXAAGCAttack(rx->id,2);
      SetRXAAGCHang(rx->id,0);
      SetRXAAGCDecay(rx->id,250);
      SetRXAAGCHangThreshold(rx->id,100);
      break;
    case AGC_FAST:
      SetRXAAGCAttack(rx->id,2);
      SetRXAAGCHang(rx->id,0);
      SetRXAAGCDecay(rx->id,50);
      SetRXAAGCHangThreshold(rx->id,100);
      break;
  }
}

void set_offset(RECEIVER *rx,long long offset) {
//fprintf(stderr,"set_offset: id=%d ofset=%lld\n",rx->id,offset);
//fprintf(stderr,"set_offset: frequency=%lld ctun_freqeuncy=%lld offset=%lld\n",vfo[rx->id].frequency,vfo[rx->id].ctun_frequency,vfo[rx->id].offset);
  if(offset==0) {
    SetRXAShiftFreq(rx->id, (double)offset);
    RXANBPSetShiftFrequency(rx->id, (double)offset);
    SetRXAShiftRun(rx->id, 0);
  } else {
    SetRXAShiftFreq(rx->id, (double)offset);
    RXANBPSetShiftFrequency(rx->id, (double)offset);
    SetRXAShiftRun(rx->id, 1);
  }
}

static void init_analyzer(RECEIVER *rx) {
    int flp[] = {0};
    double keep_time = 0.1;
    int n_pixout=1;
    int spur_elimination_ffts = 1;
    int data_type = 1;
    int fft_size = 8192;
    int window_type = 4;
    double kaiser_pi = 14.0;
    int overlap = 2048;
    int clip = 0;
    int span_clip_l = 0;
    int span_clip_h = 0;
    int pixels=rx->pixels;
    int stitches = 1;
    int avm = 0;
    double tau = 0.001 * 120.0;
    int calibration_data_set = 0;
    double span_min_freq = 0.0;
    double span_max_freq = 0.0;

    int max_w = fft_size + (int) min(keep_time * (double) rx->fps, keep_time * (double) fft_size * (double) rx->fps);

    overlap = (int)max(0.0, ceil(fft_size - (double)rx->sample_rate / (double)rx->fps));

    fprintf(stderr,"SetAnalyzer id=%d buffer_size=%d overlap=%d\n",rx->id,rx->buffer_size,overlap);


    SetAnalyzer(rx->id,
            n_pixout,
            spur_elimination_ffts, //number of LO frequencies = number of ffts used in elimination
            data_type, //0 for real input data (I only); 1 for complex input data (I & Q)
            flp, //vector with one elt for each LO frequency, 1 if high-side LO, 0 otherwise
            fft_size, //size of the fft, i.e., number of input samples
            rx->buffer_size, //number of samples transferred for each OpenBuffer()/CloseBuffer()
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

static void create_visual(RECEIVER *rx) {
  int y=0;

fprintf(stderr,"receiver: create_visual: id=%d width=%d height=%d\n",rx->id, rx->width, rx->height);
  rx->panel=gtk_fixed_new();
  gtk_widget_set_size_request (rx->panel, rx->width, rx->height);

  rx->panadapter=NULL;
  rx->waterfall=NULL;

  int height=rx->height;
  if(rx->display_waterfall) {
    height=height/2;
  }

fprintf(stderr,"receiver: panadapter_init: height=%d y=%d\n",height,y);
  rx_panadapter_init(rx, rx->width,height);
  gtk_fixed_put(GTK_FIXED(rx->panel),rx->panadapter,0,y);
  y+=height;

  if(rx->display_waterfall) {
fprintf(stderr,"receiver: waterfall_init: height=%d y=%d\n",height,y);
    waterfall_init(rx,rx->width,height);
    gtk_fixed_put(GTK_FIXED(rx->panel),rx->waterfall,0,y);
  }
}

RECEIVER *create_receiver(int id, int buffer_size, int fft_size, int pixels, int fps, int width, int height) {
fprintf(stderr,"create_receiver: id=%d buffer_size=%d fft_size=%d pixels=%d fps=%d\n",id,buffer_size, fft_size, pixels, fps);
  RECEIVER *rx=malloc(sizeof(RECEIVER));
  rx->id=id;
  switch(id) {
    case 0:
      rx->adc=0;
      break;
    default:
      switch(protocol) {
        case ORIGINAL_PROTOCOL:
#ifdef RADIOBERRY
		case RADIOBERRY_PROTOCOL:
#endif
          switch(device) {
#ifdef RADIOBERRY
			case RADIOBERRY_SPI_DEVICE:
#endif
            case DEVICE_METIS:
            case DEVICE_HERMES:
            case DEVICE_HERMES_LITE:			
              rx->adc=0;
              break;
            default:
              rx->adc=1;
              break;
          }
          break;
        default:
          switch(device) {
            case NEW_DEVICE_ATLAS:
            case NEW_DEVICE_HERMES:
              rx->adc=0;
              break;
            default:
              rx->adc=1;
              break;
          }
          break;
      }
  }
fprintf(stderr,"create_receiver: id=%d default adc=%d\n",rx->id, rx->adc);
  rx->sample_rate=48000;
  rx->buffer_size=buffer_size;
  rx->fft_size=fft_size;
  rx->pixels=pixels;
  rx->fps=fps;

  BAND *b=band_get_band(vfo[rx->id].band);

//  rx->dds_offset=0;
//  rx->rit=0;

  rx->width=width;
  rx->height=height;
  rx->display_panadapter=1;
  rx->display_waterfall=1;

  // allocate buffers
  rx->iq_input_buffer=malloc(sizeof(double)*2*rx->buffer_size);
  rx->audio_buffer=malloc(AUDIO_BUFFER_SIZE);
  rx->audio_sequence=0L;
  rx->pixel_samples=malloc(sizeof(float)*pixels);

  rx->samples=0;
  rx->displaying=0;
  rx->display_panadapter=1;
  rx->display_waterfall=1;

  rx->panadapter_high=-40;
  rx->panadapter_low=-140;

  rx->volume=0.2;
  rx->attenuation=0;

  rx->dither=0;
  rx->random=0;
  rx->preamp=0;

  rx->nr=0;
  rx->nr2=0;
  rx->anf=0;
  rx->snb=0;

  rx->nr_agc=0;
  rx->nr2_gain_method=2;
  rx->nr2_npe_method=0;
  rx->nr2_ae=1;
  
  rx->alex_antenna=b->alexRxAntenna;
  rx->alex_attenuation=b->alexAttenuation;

  rx->agc=AGC_MEDIUM;
  rx->agc_gain=80.0;
  rx->agc_slope=35.0;
  rx->agc_hang_threshold=0.0;
  
  rx->playback_handle=NULL;
  rx->local_audio=0;
  rx->mute_when_not_active=0;
  rx->audio_channel=STEREO;
  rx->audio_device=-1;

  receiver_restore_state(rx);

  int scale=rx->sample_rate/48000;
  rx->output_samples=rx->buffer_size/scale;
  rx->audio_output_buffer=malloc(sizeof(double)*2*rx->output_samples);
fprintf(stderr,"create_receiver: id=%d output_samples=%d\n",rx->id,rx->output_samples);

  rx->hz_per_pixel=(double)rx->sample_rate/(double)rx->width;
  // setup wdsp for this receiver

fprintf(stderr,"create_receiver: id=%d after restore adc=%d\n",rx->id, rx->adc);

fprintf(stderr,"create_receiver: OpenChannel id=%d buffer_size=%d fft_size=%d sample_rate=%d\n",rx->id,rx->buffer_size, rx->fft_size, rx->sample_rate);
  OpenChannel(rx->id,
              rx->buffer_size,
              rx->fft_size,
              rx->sample_rate,
              48000, // dsp rate
              48000, // output rate
              0, // receive
              1, // run
              0.010, 0.025, 0.0, 0.010, 0);

  b=band_get_band(vfo[rx->id].band);
  BANDSTACK *bs=b->bandstack;
  BANDSTACK_ENTRY *entry=&bs->entry[vfo[rx->id].bandstack];
  FILTER *band_filters=filters[vfo[rx->id].mode];
  FILTER *band_filter=&band_filters[vfo[rx->id].filter];
  set_filter(rx,band_filter->low,band_filter->high);
  
  SetRXAFMDeviation(rx->id,(double)deviation);

  set_agc(rx, rx->agc);

  SetRXAAMDSBMode(rx->id, 0);
  SetRXAShiftRun(rx->id, 0);

  SetRXAEMNRPosition(rx->id, rx->nr_agc);
  SetRXAEMNRgainMethod(rx->id, rx->nr2_gain_method);
  SetRXAEMNRnpeMethod(rx->id, rx->nr2_npe_method);
  SetRXAEMNRRun(rx->id, rx->nr2);
  SetRXAEMNRaeRun(rx->id, rx->nr2_ae);

  SetRXAANRVals(rx->id, 64, 16, 16e-4, 10e-7); // defaults
  SetRXAANRRun(rx->id, rx->nr);
  SetRXAANFRun(rx->id, rx->anf);
  SetRXASNBARun(rx->id, rx->snb);

  SetRXAPanelGain1(rx->id, rx->volume);
  SetRXAPanelBinaural(rx->id, binaural);
  SetRXAPanelRun(rx->id, 1);

  if(enable_rx_equalizer) {
    SetRXAGrphEQ(rx->id, rx_equalizer);
    SetRXAEQRun(rx->id, 1);
  } else {
    SetRXAEQRun(rx->id, 0);
  }

  // setup for diversity
  create_divEXT(0,0,2,rx->buffer_size);
  SetEXTDIVRotate(0, 2, &i_rotate, &q_rotate);
  SetEXTDIVRun(0,diversity_enabled);
  

  int result;
  XCreateAnalyzer(rx->id, &result, 262144, 1, 1, "");
  if(result != 0) {
    fprintf(stderr, "XCreateAnalyzer id=%d failed: %d\n", rx->id, result);
  } else {
    init_analyzer(rx);
  }

  SetDisplayDetectorMode(rx->id, 0, display_detector_mode);
  SetDisplayAverageMode(rx->id, 0,  display_average_mode);
   
  calculate_display_average(rx);

  create_visual(rx);

  if(rx->local_audio) {
    audio_open_output(rx);
  }

  return rx;
}

void receiver_change_adc(RECEIVER *rx,int adc) {
  rx->adc=adc;
}

void receiver_change_sample_rate(RECEIVER *rx,int sample_rate) {

  rx->sample_rate=sample_rate;

  SetChannelState(rx->id,0,1);

  int scale=rx->sample_rate/48000;
  rx->output_samples=rx->buffer_size/scale;
  free(rx->audio_output_buffer);
  rx->audio_output_buffer=malloc(sizeof(double)*2*rx->output_samples);
  rx->audio_buffer=malloc(AUDIO_BUFFER_SIZE);
  rx->hz_per_pixel=(double)rx->sample_rate/(double)rx->width;

  SetInputSamplerate(rx->id, sample_rate);

  init_analyzer(rx);

fprintf(stderr,"receiver_change_sample_rate: id=%d rate=%d buffer_size=%d output_samples=%d\n",rx->id, rx->sample_rate, rx->buffer_size, rx->output_samples);
  SetChannelState(rx->id,1,0);
}

void receiver_frequency_changed(RECEIVER *rx) {
  int id=rx->id;

  if(vfo[id].ctun) {
    vfo[id].offset=vfo[id].ctun_frequency-vfo[id].frequency;
    set_offset(rx,vfo[id].offset);
  } else if(protocol==NEW_PROTOCOL) {
    schedule_high_priority(); // send new frequency
  }
}

void receiver_mode_changed(RECEIVER *rx) {
  set_mode(rx,vfo[rx->id].mode);
}

void receiver_filter_changed(RECEIVER *rx) {
  FILTER *mode_filters=filters[vfo[rx->id].mode];
  FILTER *filter=&mode_filters[vfo[rx->id].filter];
  set_filter(rx,filter->low,filter->high);
}

void receiver_vfo_changed(RECEIVER *rx) {
  receiver_frequency_changed(rx);
  receiver_mode_changed(rx);
  receiver_filter_changed(rx);
}

#ifdef FREEDV
static void process_freedv_rx_buffer(RECEIVER *rx) {
  short left_audio_sample;
  short right_audio_sample;
  int i;
  int demod_samples;
  for(i=0;i<rx->output_samples;i++) {
    if(freedv_samples==0) {
      left_audio_sample=(short)(rx->audio_output_buffer[i*2]*32767.0);
      right_audio_sample=(short)(rx->audio_output_buffer[(i*2)+1]*32767.0);
      demod_samples=demod_sample_freedv(left_audio_sample);
      if(demod_samples!=0) {
        int s;
        int t;
        for(s=0;s<demod_samples;s++) {
          if(freedv_sync) {
            left_audio_sample=right_audio_sample=(short)((double)speech_out[s]);
          } else {
            left_audio_sample=right_audio_sample=0;
          }
          for(t=0;t<6;t++) { // 8k to 48k
            if(local_audio) {
                audio_write(left_audio_sample,right_audio_sample);
                left_audio_sample=0;
                right_audio_sample=0;
            }

            switch(protocol) {
              case ORIGINAL_PROTOCOL:
                old_protocol_audio_samples(left_audio_sample,right_audio_sample);
                break;
              case NEW_PROTOCOL:
                new_protocol_audio_samples(left_audio_sample,right_audio_sample);
                break;
#ifdef LIMESDR
              case LIMESDR_PROTOCOL:
                break;
#endif
#ifdef RADIOBERRY
			case RADIOBERRY_PROTOCOL:
				//no audio stream to radioberry hardware, using local audio of rpi.
				break;
#endif
            }
          }
        }
      }
      freedv_samples++;
      if(freedv_samples==freedv_resample) {
        freedv_samples=0;
      }
    }
  }
}
#endif

static void process_rx_buffer(RECEIVER *rx) {
  short left_audio_sample;
  short right_audio_sample;
  int i;
  for(i=0;i<rx->output_samples;i++) {
    if(isTransmitting()) {
      left_audio_sample=0;
      right_audio_sample=0;
    } else {
      left_audio_sample=(short)(rx->audio_output_buffer[i*2]*32767.0);
      right_audio_sample=(short)(rx->audio_output_buffer[(i*2)+1]*32767.0);
#ifdef PSK
      if(vfo[rx->id].mode==modePSK) {
        if(psk_samples==0) {
          psk_demod((double)((left_audio_sample+right_audio_sample)/2));
        }
        psk_samples++;
        if(psk_samples==psk_resample) {
          psk_samples=0;
        }
      }
#endif
    }

    if(rx->local_audio) {
      if(rx!=active_receiver && rx->mute_when_not_active) {
        audio_write(rx,0,0);
      } else {
        switch(rx->audio_channel) {
          case STEREO:
            audio_write(rx,left_audio_sample,right_audio_sample);
            break;
          case LEFT:
            audio_write(rx,left_audio_sample,0);
            break;
          case RIGHT:
            audio_write(rx,0,right_audio_sample);
            break;
        }
      }
    }

    if(rx==active_receiver) {
      switch(protocol) {
        case ORIGINAL_PROTOCOL:
          old_protocol_audio_samples(rx,left_audio_sample,right_audio_sample);
          break;
        case NEW_PROTOCOL:
          if(!(echo&&isTransmitting())) {
            new_protocol_audio_samples(rx,left_audio_sample,right_audio_sample);
          }
          break;
#ifdef LIMESDR
        case LIMESDR_PROTOCOL:
          break;
#endif
#ifdef RADIOBERRY
	case RADIOBERRY_PROTOCOL:
		//no audio stream to radioberry hardware, using local audio of rpi.
		break;
#endif
      }
    }

  }
}

void full_rx_buffer(RECEIVER *rx) {
  int j;
  int error;

  fexchange0(rx->id, rx->iq_input_buffer, rx->audio_output_buffer, &error);
  if(error!=0) {
    fprintf(stderr,"full_rx_buffer: id=%d fexchange0: error=%d\n",rx->id,error);
  }

  if(rx->displaying) {
    Spectrum0(1, rx->id, 0, 0, rx->iq_input_buffer);
  }

  switch(vfo[rx->id].mode) {
#ifdef FREEDV
    case modeFREEDV:
      process_freedv_rx_buffer(rx);
      break;
#endif
    default:
      process_rx_buffer(rx);
      break;
  }
}

void add_iq_samples(RECEIVER *rx, double i_sample,double q_sample) {
  rx->iq_input_buffer[rx->samples*2]=i_sample;
  rx->iq_input_buffer[(rx->samples*2)+1]=q_sample;
  rx->samples=rx->samples+1;
  if(rx->samples==rx->buffer_size) {
    full_rx_buffer(rx);
    rx->samples=0;
  }
}
