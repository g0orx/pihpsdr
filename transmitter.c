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

#include "alex.h"
#include "band.h"
#include "bandstack.h"
#include "channel.h"
#include "main.h"
#include "receiver.h"
#include "meter.h"
#include "filter.h"
#include "mode.h"
#include "property.h"
#include "radio.h"
#include "vfo.h"
#include "vox.h"
#include "meter.h"
#include "toolbar.h"
#include "tx_panadapter.h"
#include "waterfall.h"
#include "receiver.h"
#include "transmitter.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "audio.h"
#include "ext.h"

double getNextSideToneSample();
double getNextInternalSideToneSample();

#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

static int waterfall_samples=0;
static int waterfall_resample=8;

//
// CW (CAT-CW and LOCALCW) in the "old protocol" is timed by the
// heart-beat of the mic samples. The communication with rigctl.c
// and iambic.c is done via some global variables. Their use is:
//
// cw_key_up/cw_key_down: set number of samples for next key-down/key-up sequence
//                        Any of these variable will only be set from outside if
//			  both have value 0.
// cw_not_ready:          set to 0 if transmitting in CW mode. This is used to
//                        abort pending CAT CW messages if MOX or MODE is switched
//                        manually.
int cw_key_up = 0;
int cw_key_down = 0;
int cw_not_ready=1;

//
// In the old protocol, the CW signal is generated within pihpsdr,
// and the pulses must be shaped. This is done via "cw_shape_buffer".
// The TX mic samples buffer could possibly be used for this as well.
//
static double *cw_shape_buffer48 = NULL;
static double *cw_shape_buffer192 = NULL;
static int cw_shape = 0;
//
// cwramp is the function defining the "ramp" of the CW pulse.
// an array with RAMPLEN+1 entries. To change the ramp width,
// new arrays cwramp48[] and cwramp192[] have to be provided
// in cwramp.c
//
#define RAMPLEN 250			// 200: 4 msec ramp width, 250: 5 msec ramp width
extern double cwramp48[];		// see cwramp.c, for 48 kHz sample rate
extern double cwramp192[];		// see cwramp.c, for 192 kHz sample rate

double ctcss_frequencies[CTCSS_FREQUENCIES]= {
  67.0,71.9,74.4,77.0,79.7,82.5,85.4,88.5,91.5,94.8,
  97.4,100.0,103.5,107.2,110.9,114.8,118.8,123.0,127.3,131.8,
  136.5,141.3,146.2,151.4,156.7,162.2,167.9,173.8,179.9,186.2,
  192.8,203.5,210.7,218.1,225.7,233.6,241.8,250.3
};

static void init_analyzer(TRANSMITTER *tx);

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  // ignore delete event
  return TRUE;
}

static gint update_out_of_band(gpointer data) {
  TRANSMITTER *tx=(TRANSMITTER *)data;
  tx->out_of_band=0;
  g_idle_add(ext_vfo_update,NULL);
  return FALSE;
}

void transmitter_set_out_of_band(TRANSMITTER *tx) {
  tx->out_of_band=1;
  g_idle_add(ext_vfo_update,NULL);
  tx->out_of_band_timer_id=gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE,1000,update_out_of_band, tx, NULL);
}

void transmitter_set_deviation(TRANSMITTER *tx) {
  SetTXAFMDeviation(tx->id, (double)tx->deviation);
}

void transmitter_set_am_carrier_level(TRANSMITTER *tx) {
  SetTXAAMCarrierLevel(tx->id, tx->am_carrier_level);
}

void transmitter_set_ctcss(TRANSMITTER *tx,int state,int i) {
g_print("transmitter_set_ctcss: state=%d i=%d frequency=%0.1f\n",state,i,ctcss_frequencies[i]);
  tx->ctcss_enabled=state;
  tx->ctcss=i;
  SetTXACTCSSFreq(tx->id, ctcss_frequencies[tx->ctcss]);
  SetTXACTCSSRun(tx->id, tx->ctcss_enabled);
}

void transmitter_set_compressor_level(TRANSMITTER *tx,double level) {
  tx->compressor_level=level;
  SetTXACompressorGain(tx->id, tx->compressor_level);
}

void transmitter_set_compressor(TRANSMITTER *tx,int state) {
  tx->compressor=state;
  SetTXACompressorRun(tx->id, tx->compressor);
}

void reconfigure_transmitter(TRANSMITTER *tx,int width,int height) {
g_print("reconfigure_transmitter: width=%d height=%d\n",width,height);
  if(width!=tx->width) {
    tx->width=width;
    tx->height=height; 
    int ratio=tx->iq_output_rate/tx->mic_sample_rate;
    tx->pixels=display_width*ratio*2;
    g_free(tx->pixel_samples);
    tx->pixel_samples=g_new(float,tx->pixels);
    init_analyzer(tx);
  }
  gtk_widget_set_size_request(tx->panadapter, width, height);
}

void transmitter_save_state(TRANSMITTER *tx) {
  char name[128];
  char value[128];

  sprintf(name,"transmitter.%d.fps",tx->id);
  sprintf(value,"%d",tx->fps);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.filter_low",tx->id);
  sprintf(value,"%d",tx->filter_low);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.filter_high",tx->id);
  sprintf(value,"%d",tx->filter_high);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.use_rx_filter",tx->id);
  sprintf(value,"%d",tx->use_rx_filter);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.alex_antenna",tx->id);
  sprintf(value,"%d",tx->alex_antenna);
  setProperty(name,value);

  sprintf(name,"transmitter.%d.panadapter_low",tx->id);
  sprintf(value,"%d",tx->panadapter_low);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.panadapter_high",tx->id);
  sprintf(value,"%d",tx->panadapter_high);
  setProperty(name,value);

  sprintf(name,"transmitter.%d.local_microphone",tx->id);
  sprintf(value,"%d",tx->local_microphone);
  setProperty(name,value);
  if(tx->microphone_name!=NULL) {
    sprintf(name,"transmitter.%d.microphone_name",tx->id);
    sprintf(value,"%s",tx->microphone_name);
    setProperty(name,value);
  }
  sprintf(name,"transmitter.%d.low_latency",tx->id);
  sprintf(value,"%d",tx->low_latency);
  setProperty(name,value);
#ifdef PURESIGNAL
  sprintf(name,"transmitter.%d.puresignal",tx->id);
  sprintf(value,"%d",tx->puresignal);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.auto_on",tx->id);
  sprintf(value,"%d",tx->auto_on);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.single_on",tx->id);
  sprintf(value,"%d",tx->single_on);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.feedback",tx->id);
  sprintf(value,"%d",tx->feedback);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.attenuation",tx->id);
  sprintf(value,"%d",tx->attenuation);
  setProperty(name,value);
#endif
  sprintf(name,"transmitter.%d.ctcss_enabled",tx->id);
  sprintf(value,"%d",tx->ctcss_enabled);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.ctcss",tx->id);
  sprintf(value,"%d",tx->ctcss);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.deviation",tx->id);
  sprintf(value,"%d",tx->deviation);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.am_carrier_level",tx->id);
  sprintf(value,"%f",tx->am_carrier_level);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.drive",tx->id);
  sprintf(value,"%d",tx->drive);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.tune_percent",tx->id);
  sprintf(value,"%d",tx->tune_percent);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.tune_use_drive",tx->id);
  sprintf(value,"%d",tx->tune_use_drive);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.drive_level",tx->id);
  sprintf(value,"%d",tx->drive_level);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.compressor",tx->id);
  sprintf(value,"%d",tx->compressor);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.compressor_level",tx->id);
  sprintf(value,"%f",tx->compressor_level);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.xit_enabled",tx->id);
  sprintf(value,"%d",tx->xit_enabled);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.xit",tx->id);
  sprintf(value,"%lld",tx->xit);
  setProperty(name,value);

  sprintf(name,"transmitter.%d.dialog_x",tx->id);
  sprintf(value,"%d",tx->dialog_x);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.dialog_y",tx->id);
  sprintf(value,"%d",tx->dialog_y);
  setProperty(name,value);
}

void transmitter_restore_state(TRANSMITTER *tx) {
  char name[128];
  char *value;

  sprintf(name,"transmitter.%d.fps",tx->id);
  value=getProperty(name);
  if(value) tx->fps=atoi(value);
  sprintf(name,"transmitter.%d.filter_low",tx->id);
  value=getProperty(name);
  if(value) tx->filter_low=atoi(value);
  sprintf(name,"transmitter.%d.filter_high",tx->id);
  value=getProperty(name);
  if(value) tx->filter_high=atoi(value);
  sprintf(name,"transmitter.%d.use_rx_filter",tx->id);
  value=getProperty(name);
  if(value) tx->use_rx_filter=atoi(value);
  sprintf(name,"transmitter.%d.alex_antenna",tx->id);
  value=getProperty(name);
  if(value) tx->alex_antenna=atoi(value);

  sprintf(name,"transmitter.%d.panadapter_low",tx->id);
  value=getProperty(name);
  if(value) tx->panadapter_low=atoi(value);
  sprintf(name,"transmitter.%d.panadapter_high",tx->id);
  value=getProperty(name);
  if(value) tx->panadapter_high=atoi(value);

  sprintf(name,"transmitter.%d.local_microphone",tx->id);
  value=getProperty(name);
  if(value) tx->local_microphone=atoi(value);
  sprintf(name,"transmitter.%d.microphone_name",tx->id);
  value=getProperty(name);
  if(value) {
    tx->microphone_name=g_new(gchar,strlen(value)+1);
    strcpy(tx->microphone_name,value);
  }
  sprintf(name,"transmitter.%d.low_latency",tx->id);
  value=getProperty(name);
  if(value) tx->low_latency=atoi(value);
#ifdef PURESIGNAL
  sprintf(name,"transmitter.%d.puresignal",tx->id);
  value=getProperty(name);
  if(value) tx->puresignal=atoi(value);
  sprintf(name,"transmitter.%d.auto_on",tx->id);
  value=getProperty(name);
  if(value) tx->auto_on=atoi(value);
  sprintf(name,"transmitter.%d.single_on",tx->id);
  value=getProperty(name);
  if(value) tx->single_on=atoi(value);
  sprintf(name,"transmitter.%d.feedback",tx->id);
  value=getProperty(name);
  if(value) tx->feedback=atoi(value);
  sprintf(name,"transmitter.%d.attenuation",tx->id);
  value=getProperty(name);
  if(value) tx->attenuation=atoi(value);
#endif
  sprintf(name,"transmitter.%d.ctcss_enabled",tx->id);
  value=getProperty(name);
  if(value) tx->ctcss_enabled=atoi(value);
  sprintf(name,"transmitter.%d.ctcss",tx->id);
  value=getProperty(name);
  if(value) tx->ctcss=atoi(value);
  sprintf(name,"transmitter.%d.deviation",tx->id);
  value=getProperty(name);
  if(value) tx->deviation=atoi(value);
  sprintf(name,"transmitter.%d.am_carrier_level",tx->id);
  value=getProperty(name);
  if(value) tx->am_carrier_level=atof(value);
  sprintf(name,"transmitter.%d.drive",tx->id);
  value=getProperty(name);
  if(value) tx->drive=atoi(value);
  sprintf(name,"transmitter.%d.tune_percent",tx->id);
  value=getProperty(name);
  if(value) tx->tune_percent=atoi(value);
  sprintf(name,"transmitter.%d.tune_use_drive",tx->id);
  value=getProperty(name);
  if(value) tx->tune_use_drive=atoi(value);
  sprintf(name,"transmitter.%d.drive_level",tx->id);
  value=getProperty(name);
  if(value) tx->drive_level=atoi(value);
  sprintf(name,"transmitter.%d.compressor",tx->id);
  value=getProperty(name);
  if(value) tx->compressor=atoi(value);
  sprintf(name,"transmitter.%d.compressor_level",tx->id);
  value=getProperty(name);
  if(value) tx->compressor_level=atof(value);
  sprintf(name,"transmitter.%d.xit_enabled",tx->id);
  value=getProperty(name);
  if(value) tx->xit_enabled=atoi(value);
  sprintf(name,"transmitter.%d.xit",tx->id);
  value=getProperty(name);
  if(value) tx->xit=atoll(value);
  sprintf(name,"transmitter.%d.dialog_x",tx->id);
  value=getProperty(name);
  if(value) tx->dialog_x=atoi(value);
  sprintf(name,"transmitter.%d.dialog_y",tx->id);
  value=getProperty(name);
  if(value) tx->dialog_y=atoi(value);
}

static double compute_power(double p) {
  double interval=10.0;
  switch(pa_power) {
    case PA_1W:
      interval=100.0; // mW
      break;
    case PA_10W:
      interval=1.0; // W
      break;
    case PA_30W:
      interval=3.0; // W
      break;
    case PA_50W:
      interval=5.0; // W
      break;
    case PA_100W:
      interval=10.0; // W
      break;
    case PA_200W:
      interval=20.0; // W
      break;
    case PA_500W:
      interval=50.0; // W
      break;
  }
  int i=0;
  if(p>(double)pa_trim[10]) {
    i=9;
  } else {
    while(p>(double)pa_trim[i]) {
      i++;
    }
    if(i>0) i--;
  }

  double frac = (p - (double)pa_trim[i]) / ((double)pa_trim[i + 1] - (double)pa_trim[i]);
  return interval * ((1.0 - frac) * (double)i + frac * (double)(i + 1));
}

static gboolean update_display(gpointer data) {
  TRANSMITTER *tx=(TRANSMITTER *)data;
  int rc;

//fprintf(stderr,"update_display: tx id=%d\n",tx->id);
  if(tx->displaying) {
    // if "MON" button is active (tx->feedback is TRUE),
    // then obtain spectrum pixels from PS_RX_FEEDBACK,
    // that is, display the (attenuated) TX signal from the "antenna"
    //
    // POSSIBLE MISMATCH OF SAMPLE RATES IN ORIGINAL PROTOCOL:
    // TX sample rate is fixed 48 kHz, but RX sample rate can be
    // 2*, 4*, or even 8* larger. The analyzer has been set up to use
    // more pixels in this case, so we just need to copy the
    // inner part of the spectrum.
    // If both spectra have the same number of pixels, this code
    // just copies all of them
    //
#ifdef PURESIGNAL
    if(tx->puresignal && tx->feedback) {
      RECEIVER *rx_feedback=receiver[PS_RX_FEEDBACK];
      g_mutex_lock(&rx_feedback->mutex);
      GetPixels(rx_feedback->id,0,rx_feedback->pixel_samples,&rc);
      int full  = rx_feedback->pixels;  // number of pixels in the feedback spectrum
      int width = tx->pixels;           // number of pixels to copy from the feedback spectrum
      int start = (full-width) /2;      // Copy from start ... (end-1) 
      float *tfp=tx->pixel_samples;
      float *rfp=rx_feedback->pixel_samples+start;
      float offset;
      int i;
      //
      // The TX panadapter shows a RELATIVE signal strength. A CW or single-tone signal at
      // full drive appears at 0dBm, the two peaks of a full-drive two-tone signal appear
      // at -6 dBm each. THIS DOES NOT DEPEND ON THE POSITION OF THE DRIVE LEVEL SLIDER.
      // The strength of the feedback signal, however, depends on the drive, on the PA and
      // on the attenuation effective in the feedback path.
      // We try to normalize the feeback signal such that is looks like a "normal" TX
      // panadapter if the feedback is optimal for PURESIGNAL (that is, if the attenuation
      // is optimal). The correction (offset) depends on the protocol (different peak levels in the TX
      // feedback channel.
      switch (protocol) {
        case ORIGINAL_PROTOCOL:
	  // TX dac feedback peak = 0.406, on HermesLite2 0.230
          offset = (device == DEVICE_HERMES_LITE2) ? 17.0 : 12.0;
          break;
        case NEW_PROTOCOL:
          // TX dac feedback peak = 0.2899
	  offset = 15.0;
          break;
        default:
          // we probably never come here
          offset = 0.0;
          break;
      }
      for (i=0; i<width; i++) {
        *tfp++ =*rfp++ + offset;
      }
      g_mutex_unlock(&rx_feedback->mutex);
    } else {
#endif
      GetPixels(tx->id,0,tx->pixel_samples,&rc);
#ifdef PURESIGNAL
    }
#endif
    if(rc) {
      tx_panadapter_update(tx);
    }

    transmitter->alc=GetTXAMeter(tx->id, alc);
    double constant1=3.3;
    double constant2=0.095;
    int fwd_cal_offset=6;

    int fwd_power;
    int rev_power;
    int ex_power;
    double v1;

    fwd_power=alex_forward_power;
    rev_power=alex_reverse_power;
    if(device==DEVICE_HERMES_LITE || device==DEVICE_HERMES_LITE2) {
      ex_power=0;
    } else {
      ex_power=exciter_power;
    }
    switch(protocol) {
      case ORIGINAL_PROTOCOL:
        switch(device) {
          case DEVICE_METIS:
            constant1=3.3;
            constant2=0.09;
            break;
          case DEVICE_HERMES:
          case DEVICE_STEMLAB:
            constant1=3.3;
            constant2=0.095;
            break;
          case DEVICE_ANGELIA:
            constant1=3.3;
            constant2=0.095;
            break;
          case DEVICE_ORION:
            constant1=5.0;
            constant2=0.108;
            fwd_cal_offset=4;
            break;
          case DEVICE_ORION2:
            constant1=5.0;
            constant2=0.08;
            fwd_cal_offset=18;
            break;
          case DEVICE_HERMES_LITE:
          case DEVICE_HERMES_LITE2:
            // possible reversed depending polarity of current sense transformer
            if(rev_power>fwd_power) {
              fwd_power=alex_reverse_power;
              rev_power=alex_forward_power;
            }
            constant1=3.3;
            constant2=1.4;
            fwd_cal_offset=6;
            break;
        }

        if(fwd_power==0) {
          fwd_power=ex_power;
        }
        fwd_power=fwd_power-fwd_cal_offset;
        v1=((double)fwd_power/4095.0)*constant1;
        transmitter->fwd=(v1*v1)/constant2;

        if(device==DEVICE_HERMES_LITE || device==DEVICE_HERMES_LITE2) {
          transmitter->exciter=0.0;
        } else {
          ex_power=ex_power-fwd_cal_offset;
          v1=((double)ex_power/4095.0)*constant1;
          transmitter->exciter=(v1*v1)/constant2;
        }

        transmitter->rev=0.0;
        if(fwd_power!=0) {
          v1=((double)rev_power/4095.0)*constant1;
          transmitter->rev=(v1*v1)/constant2;
        }
        break;
      case NEW_PROTOCOL:
        switch(device) {
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
            fwd_cal_offset=4;
            break;
          case NEW_DEVICE_ORION2:
            constant1=5.0;
            constant2=0.08;
            fwd_cal_offset=18;
            break;
          case NEW_DEVICE_HERMES_LITE:
          case NEW_DEVICE_HERMES_LITE2:
            constant1=3.3;
            constant2=0.09;
            break;
        }

        fwd_power=alex_forward_power;
        if(fwd_power==0) {
          fwd_power=exciter_power;
        }
        fwd_power=fwd_power-fwd_cal_offset;
        v1=((double)fwd_power/4095.0)*constant1;
        transmitter->fwd=(v1*v1)/constant2;

        ex_power=exciter_power;
        ex_power=ex_power-fwd_cal_offset;
        v1=((double)ex_power/4095.0)*constant1;
        transmitter->exciter=(v1*v1)/constant2;

        transmitter->rev=0.0;
        if(alex_forward_power!=0) {
          rev_power=alex_reverse_power;
          v1=((double)rev_power/4095.0)*constant1;
          transmitter->rev=(v1*v1)/constant2;
        }
        break;

#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
        transmitter->fwd=0.0;
        transmitter->exciter=0.0;
        transmitter->rev=0.0;
        break;
#endif
    }

    double fwd=compute_power(transmitter->fwd);
    double rev=compute_power(transmitter->rev);

//g_print("transmitter: meter_update: fwd:%f->%f rev:%f->%f ex_fwd=%d alex_fwd=%d alex_rev=%d\n",transmitter->fwd,fwd,transmitter->rev,rev,exciter_power,alex_forward_power,alex_reverse_power);

    if(!duplex) {
      meter_update(active_receiver,POWER,/*transmitter->*/fwd,/*transmitter->*/rev,transmitter->exciter,transmitter->alc);
    }

    return TRUE; // keep going
  }
  return FALSE; // no more timer events
}


static void init_analyzer(TRANSMITTER *tx) {
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
    int pixels=tx->pixels;
    int stitches = 1;
    int calibration_data_set = 0;
    double span_min_freq = 0.0;
    double span_max_freq = 0.0;

    int max_w = fft_size + (int) min(keep_time * (double) tx->fps, keep_time * (double) fft_size * (double) tx->fps);

    overlap = (int)max(0.0, ceil(fft_size - (double)tx->mic_sample_rate / (double)tx->fps));

    fprintf(stderr,"SetAnalyzer id=%d buffer_size=%d overlap=%d pixels=%d\n",tx->id,tx->output_samples,overlap,tx->pixels);


    SetAnalyzer(tx->id,
            n_pixout,
            spur_elimination_ffts, //number of LO frequencies = number of ffts used in elimination
            data_type, //0 for real input data (I only); 1 for complex input data (I & Q)
            flp, //vector with one elt for each LO frequency, 1 if high-side LO, 0 otherwise
            fft_size, //size of the fft, i.e., number of input samples
            tx->output_samples, //number of samples transferred for each OpenBuffer()/CloseBuffer()
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
   //
   // This cannot be changed for the TX panel,
   // use peak mode
   //
   SetDisplayDetectorMode(tx->id, 0, DETECTOR_MODE_PEAK);
   SetDisplayAverageMode(tx->id, 0,  AVERAGE_MODE_NONE);

}

void create_dialog(TRANSMITTER *tx) {
g_print("create_dialog\n");
  tx->dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(tx->dialog),GTK_WINDOW(top_window));
  gtk_window_set_title(GTK_WINDOW(tx->dialog),"TX");
  g_signal_connect (tx->dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(tx->dialog));
g_print("create_dialog: add tx->panel\n");
  gtk_widget_set_size_request (tx->panel, display_width/4, display_height/2);
  gtk_container_add(GTK_CONTAINER(content),tx->panel);

  gtk_widget_add_events(tx->dialog, GDK_KEY_PRESS_MASK);
  g_signal_connect(tx->dialog, "key_press_event", G_CALLBACK(keypress_cb), NULL);
}

static void create_visual(TRANSMITTER *tx) {
 
  fprintf(stderr,"transmitter: create_visual: id=%d width=%d height=%d\n",tx->id, tx->width,tx->height);
  
  tx->dialog=NULL;

  tx->panel=gtk_fixed_new();
  gtk_widget_set_size_request (tx->panel, tx->width, tx->height);

  if(tx->display_panadapter) {
    tx_panadapter_init(tx,tx->width,tx->height);
    gtk_fixed_put(GTK_FIXED(tx->panel),tx->panadapter,0,0);
  }

  gtk_widget_show_all(tx->panel);
  g_object_ref((gpointer)tx->panel);

  if(duplex) {
    create_dialog(tx);
  }

}

TRANSMITTER *create_transmitter(int id, int buffer_size, int fft_size, int fps, int width, int height) {
  int rc;

  TRANSMITTER *tx=g_new(TRANSMITTER,1);
  tx->id=id;
  tx->dac=0;
  tx->buffer_size=buffer_size;
  tx->fft_size=fft_size;
  tx->fps=fps;

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      tx->mic_sample_rate=48000;
      tx->mic_dsp_rate=48000;
      tx->iq_output_rate=48000;
      break;
    case NEW_PROTOCOL:
      tx->mic_sample_rate=48000;
      tx->mic_dsp_rate=96000;
      tx->iq_output_rate=192000;
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      tx->mic_sample_rate=48000;
      tx->mic_dsp_rate=96000;
      tx->iq_output_rate=radio_sample_rate;
      break;
#endif

  }
  int ratio=tx->iq_output_rate/tx->mic_sample_rate;
  tx->output_samples=tx->buffer_size*ratio;
  //tx->pixels=width*ratio*4;
  tx->pixels=display_width*ratio*2;

  tx->width=width;
  tx->height=height;
  tx->display_panadapter=1;
  tx->display_waterfall=0;

  tx->panadapter_high=0;
  tx->panadapter_low=-70;
  tx->panadapter_step=10;

  tx->displaying=0;
  
  tx->alex_antenna=ALEX_TX_ANTENNA_1;

fprintf(stderr,"create_transmitter: id=%d buffer_size=%d mic_sample_rate=%d mic_dsp_rate=%d iq_output_rate=%d output_samples=%d fps=%d width=%d height=%d\n",tx->id, tx->buffer_size, tx->mic_sample_rate, tx->mic_dsp_rate, tx->iq_output_rate, tx->output_samples,tx->fps,tx->width,tx->height);

  tx->filter_low=tx_filter_low;
  tx->filter_high=tx_filter_high;
  tx->use_rx_filter=FALSE;

  tx->out_of_band=0;

  tx->low_latency=0;

  tx->twotone=0;
  tx->puresignal=0;
  tx->feedback=0;
  tx->auto_on=0;
  tx->single_on=0;

  tx->attenuation=0;
  tx->ctcss=11;
  tx->ctcss_enabled=FALSE;

  tx->deviation=2500;
  tx->am_carrier_level=0.5;

  tx->drive=50;
  tx->tune_percent=10;
  tx->tune_use_drive=0;

  tx->compressor=0;
  tx->compressor_level=0.0;

  tx->local_microphone=0;
  tx->microphone_name=NULL;

  tx->xit_enabled=FALSE;
  tx->xit=0LL;

  tx->dialog_x=-1;
  tx->dialog_y=-1;

  transmitter_restore_state(tx);


  // allocate buffers
fprintf(stderr,"transmitter: allocate buffers: mic_input_buffer=%d iq_output_buffer=%d pixels=%d\n",tx->buffer_size,tx->output_samples,tx->pixels);
  tx->mic_input_buffer=g_new(double,2*tx->buffer_size);
  tx->iq_output_buffer=g_new(double,2*tx->output_samples);
  tx->samples=0;
  tx->pixel_samples=g_new(float,tx->pixels);
  if (cw_shape_buffer48) g_free(cw_shape_buffer48);
  if (cw_shape_buffer192) g_free(cw_shape_buffer192);
  switch (protocol) {
    case ORIGINAL_PROTOCOL:
      //
      // We need no buffer for the IQ sample amplitudes because
      // we make dual use of the buffer for the audio amplitudes
      // (TX sample rate ==  mic sample rate)
      //
      cw_shape_buffer48=g_new(double,tx->buffer_size);
      break;
   case NEW_PROTOCOL:
#ifdef SOAPYSDR
   case SOAPYSDR_PROTOCOL:
#endif
      //
      // We need two buffers: one for the audio sample amplitudes
      // and another one for the TX IQ amplitudes
      // (TX and mic sample rate are usually different).
      //
      cw_shape_buffer48=g_new(double,tx->buffer_size);
      cw_shape_buffer192=g_new(double,tx->output_samples);
      break;
  }
  g_print("transmitter: allocate buffers: mic_input_buffer=%p iq_output_buffer=%p pixels=%p\n",
          tx->mic_input_buffer,tx->iq_output_buffer,tx->pixel_samples);

  g_print("create_transmitter: OpenChannel id=%d buffer_size=%d fft_size=%d sample_rate=%d dspRate=%d outputRate=%d\n",
          tx->id,
          tx->buffer_size,
          2048, // tx->fft_size,
          tx->mic_sample_rate,
          tx->mic_dsp_rate,
          tx->iq_output_rate);

  OpenChannel(tx->id,
              tx->buffer_size,
              2048, // tx->fft_size,
              tx->mic_sample_rate,
              tx->mic_dsp_rate,
              tx->iq_output_rate,
              1, // transmit
              0, // run
              0.010, 0.025, 0.0, 0.010, 0);

  TXASetNC(tx->id, tx->fft_size);
  TXASetMP(tx->id, tx->low_latency);


  SetTXABandpassWindow(tx->id, 1);
  SetTXABandpassRun(tx->id, 1);

  SetTXAFMEmphPosition(tx->id,pre_emphasize);

  SetTXACFIRRun(tx->id, protocol==NEW_PROTOCOL?1:0); // turned on if new protocol
  if(enable_tx_equalizer) {
    SetTXAGrphEQ(tx->id, tx_equalizer);
    SetTXAEQRun(tx->id, 1);
  } else {
    SetTXAEQRun(tx->id, 0);
  }

  transmitter_set_ctcss(tx,tx->ctcss_enabled,tx->ctcss);
  SetTXAAMSQRun(tx->id, 0);
  SetTXAosctrlRun(tx->id, 0);

  SetTXAALCAttack(tx->id, 1);
  SetTXAALCDecay(tx->id, 10);
  SetTXAALCSt(tx->id, 1); // turn it on (always on)

  SetTXALevelerAttack(tx->id, 1);
  SetTXALevelerDecay(tx->id, 500);
  SetTXALevelerTop(tx->id, 5.0);
  SetTXALevelerSt(tx->id, tx_leveler);

  SetTXAPreGenMode(tx->id, 0);
  SetTXAPreGenToneMag(tx->id, 0.0);
  SetTXAPreGenToneFreq(tx->id, 0.0);
  SetTXAPreGenRun(tx->id, 0);

  SetTXAPostGenMode(tx->id, 0);
  SetTXAPostGenToneMag(tx->id, tone_level);
  SetTXAPostGenTTMag(tx->id, tone_level,tone_level);
  SetTXAPostGenToneFreq(tx->id, 0.0);
  SetTXAPostGenRun(tx->id, 0);

  SetTXAPanelGain1(tx->id,pow(10.0, mic_gain/20.0));
  SetTXAPanelRun(tx->id, 1);

  SetTXAFMDeviation(tx->id, (double)tx->deviation);
  SetTXAAMCarrierLevel(tx->id, tx->am_carrier_level);

  SetTXACompressorGain(tx->id, tx->compressor_level);
  SetTXACompressorRun(tx->id, tx->compressor);

  tx_set_mode(tx,get_tx_mode());

  XCreateAnalyzer(tx->id, &rc, 262144, 1, 1, "");
  if (rc != 0) {
    fprintf(stderr, "XCreateAnalyzer id=%d failed: %d\n",tx->id,rc);
  } else {
    init_analyzer(tx);
  }

  create_visual(tx);

  return tx;
}

void tx_set_mode(TRANSMITTER* tx,int mode) {
  if(tx!=NULL) {
    int filter_low, filter_high;
    tx->mode=mode;
    SetTXAMode(tx->id, tx->mode);
    if(tx->use_rx_filter) {
      int m=vfo[active_receiver->id].mode;
      if(m==modeFMN) {
        if(active_receiver->deviation==2500) {
         filter_low=-4000;
         filter_high=4000;
        } else {
         filter_low=-8000;
         filter_high=8000;
        }
      } else {
        FILTER *mode_filters=filters[m];
        FILTER *filter=&mode_filters[vfo[active_receiver->id].filter];
        filter_low=filter->low;
        filter_high=filter->high;
      }
    } else {
      filter_low=tx_filter_low;
      filter_high=tx_filter_high;
    }
    tx_set_filter(tx,filter_low,filter_high);
  }
}

void tx_set_filter(TRANSMITTER *tx,int low,int high) {
  int txmode=get_tx_mode();

  switch(txmode) {
    case modeLSB:
    case modeCWL:
    case modeDIGL:
      tx->filter_low=-high;
      tx->filter_high=-low;
      break;
    case modeUSB:
    case modeCWU:
    case modeDIGU:
      tx->filter_low=low;
      tx->filter_high=high;
      break;
    case modeDSB:
    case modeAM:
    case modeSAM:
      tx->filter_low=-high;
      tx->filter_high=high;
      break;
    case modeFMN:
      if(tx->deviation==2500) {
        tx->filter_low=-4000;
        tx->filter_high=4000;
      } else {
        tx->filter_low=-8000;
        tx->filter_high=8000;
      }
      break;
    case modeDRM:
      tx->filter_low=7000;
      tx->filter_high=17000;
      break;
  }

  double fl=tx->filter_low;
  double fh=tx->filter_high;

  SetTXABandpassFreqs(tx->id,fl,fh);
}

void tx_set_pre_emphasize(TRANSMITTER *tx,int state) {
  SetTXAFMEmphPosition(tx->id,state);
}

static void full_tx_buffer(TRANSMITTER *tx) {
  long isample;
  long qsample;
  double gain, sidevol, ramp;
  double *dp;
  int j;
  int error;
  int cwmode;
  int sidetone=0;
  static int txflag=0;
  static long last_qsample=0;

  // It is important to query tx->mode and tune only *once* within this function, to assure that
  // the two "if (cwmode)" clauses give the same result.
  // cwmode only valid in the old protocol, in the new protocol we use a different mechanism

  cwmode = (tx->mode == modeCWL || tx->mode == modeCWU) && !tune && !tx->twotone;

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      gain=32767.0;  // 16 bit
      break;
    case NEW_PROTOCOL:
      gain=8388607.0; // 24 bit
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      gain=32767.0;  // 16 bit
      break;
#endif
  }

  if (cwmode) {
    //
    // do not update VOX in CW mode in case we have just switched to CW
    // and tx->mic_input_buffer is non-empty. WDSP (fexchange0) is not
    // needed because we directly produce the I/Q samples (see below).
    // What we do, however, is to create the iq_output_buffer for the
    // sole purpose to display the spectrum of our CW signal. Then,
    // the difference between poorly-shaped and well-shaped CW pulses
    // also becomes visible on *our* TX spectrum display.
    //
    dp=tx->iq_output_buffer;
    // These are the I/Q samples that describe our CW signal
    // The only use we make of it is displaying the spectrum.
    switch (protocol) {
      case ORIGINAL_PROTOCOL:
        for (j = 0; j < tx->output_samples; j++) {
	    *dp++ = 0.0;
	    *dp++ = cw_shape_buffer48[j];
        }
	break;
      case NEW_PROTOCOL:
        for (j = 0; j < tx->output_samples; j++) {
	    *dp++ = 0.0;
	    *dp++ = cw_shape_buffer192[j];
        }
	break;
    }
  } else {
    update_vox(tx);

    fexchange0(tx->id, tx->mic_input_buffer, tx->iq_output_buffer, &error);
    if(error!=0) {
      fprintf(stderr,"full_tx_buffer: id=%d fexchange0: error=%d\n",tx->id,error);
    }
  }

  if(tx->displaying && !(tx->puresignal && tx->feedback)) {
    Spectrum0(1, tx->id, 0, 0, tx->iq_output_buffer);
  }

  if (isTransmitting()) {

    if(  (    (protocol == NEW_PROTOCOL && radio->device==NEW_DEVICE_ATLAS) 
           || (protocol==ORIGINAL_PROTOCOL && radio->device==DEVICE_METIS)
         ) && atlas_penelope) {
      //
      // On these boards, drive level changes are performed by
      // scaling the TX IQ samples. In the other cases, DriveLevel
      // as sent in the C&C frames becomes effective and the IQ
      // samples are sent with full amplitude.
      // DL1YCF: include factor 0.00392 since DriveLevel == 255 means full amplitude
      //
      if(tune && !transmitter->tune_use_drive) {
        double fac=sqrt((double)transmitter->tune_percent * 0.01);
        gain=gain*(double)transmitter->drive_level*fac*0.00392;
      } else {
        gain=gain*(double)transmitter->drive_level*0.00392;
      }
    }

    if (txflag == 0 && protocol == NEW_PROTOCOL) {
	//
	// this is the first time (after a pause) that we send TX samples
	// so send some "silence" to prevent FIFO underflows
	//
	for (j=0; j< 480; j++) {
	      new_protocol_iq_samples(0,0);
	}	
    }
    txflag=1;
//
//  When doing CW, we do not need WDSP since Q(t) = cw_shape_buffer(t) and I(t)=0
//  For the old protocol where the IQ and audio samples are tied together, we can
//  easily generate a synchronous side tone (and use the function
//  old_protocol_iq_samples_with_sidetone for this purpose).
//
//  Note that the CW shape buffer is tied to the mic sample rate (48 kHz).
//
    if (cwmode) {
	//
	// "pulse shape case":
	// directly produce the I/Q samples. For a continuous zero-frequency
	// carrier (as needed for CW) I(t)=1 and Q(t)=0 everywhere. We shape I(t)
	// with the pulse envelope. We also produce a side tone with same shape.
	// Note that tx->iq_output_buffer is not used. Therefore, all the
        // SetTXAPostGen functions are not needed for CW!
	//
	// In the new protocol, we just put "silence" into the TX IQ buffer
	//
	switch (protocol) {
	  case ORIGINAL_PROTOCOL:
	    //
	    // tx->output_samples equals tx->buffer_size
	    // Take TX envelope from the 48kHz shape buffer
	    //
            sidevol= 64.0 * cw_keyer_sidetone_volume;  // between 0.0 and 8128.0
	    isample=0;				    // will be constantly zero
            for(j=0;j<tx->output_samples;j++) {
	      ramp=cw_shape_buffer48[j];	    	    // between 0.0 and 1.0
	      qsample=floor(gain*ramp+0.5);         // always non-negative, isample is just the pulse envelope
	      sidetone=sidevol * ramp * getNextInternalSideToneSample();
	      old_protocol_iq_samples_with_sidetone(isample,qsample,sidetone);
	    }
	    break;
	  case NEW_PROTOCOL:
	    //
	    // tx->output_samples is four times tx->buffer_size
	    // Take TX envelope from the 192kHz shape buffer
	    //
	    isample=0;
            for(j=0;j<tx->output_samples;j++) {
	      ramp=cw_shape_buffer192[j];	    		// between 0.0 and 1.0
	      qsample=floor(gain*ramp+0.5);         	    	// always non-negative, isample is just the pulse envelope
	      new_protocol_iq_samples(isample,qsample);
	    }
	    break;
#ifdef SOAPYSDR
          case SOAPYSDR_PROTOCOL:
            //
            // the only difference to the P2 treatment is that we do not
            // generate audio samples to be sent to the radio
            //
	    isample=0;
            for(j=0;j<tx->output_samples;j++) {
	      ramp=cw_shape_buffer192[j];	    		// between 0.0 and 1.0
	      qsample=floor(gain*ramp+0.5);         	    	// always non-negative, isample is just the pulse envelope
              soapy_protocol_iq_samples((float)isample,(float)qsample);
	    }
	    break;
#endif
	}
    } else {
	//
	// Original code without pulse shaping and without side tone
	//
	for(j=0;j<tx->output_samples;j++) {
            double is,qs;
            if(iqswap) {
	      qs=tx->iq_output_buffer[j*2];
	      is=tx->iq_output_buffer[(j*2)+1];
            } else {
	      is=tx->iq_output_buffer[j*2];
	      qs=tx->iq_output_buffer[(j*2)+1];
            }
	    isample=is>=0.0?(long)floor(is*gain+0.5):(long)ceil(is*gain-0.5);
	    qsample=qs>=0.0?(long)floor(qs*gain+0.5):(long)ceil(qs*gain-0.5);
	    switch(protocol) {
		case ORIGINAL_PROTOCOL:
		    old_protocol_iq_samples(isample,qsample);
		    break;
		case NEW_PROTOCOL:
		    new_protocol_iq_samples(isample,qsample);
		    break;
#ifdef SOAPYSDR
                case SOAPYSDR_PROTOCOL:
                    soapy_protocol_iq_samples((float)isample,(float)qsample);
                    break;
#endif
	    }
	}
    }
  } else {
    //
    // When the buffer has not been sent because MOX has gone,
    // instead flush the current TX IQ buffer
    //
    if (txflag == 1 && protocol == NEW_PROTOCOL) new_protocol_flush_iq_samples();
    txflag=0;
  }
}

void add_mic_sample(TRANSMITTER *tx,float mic_sample) {
  int mode=tx->mode;
  float cwsample;
  double mic_sample_double, ramp;
  int i,s;
  int updown;

//
// silence TX audio if tuning, or when doing CW.
// (in order not to fire VOX)
//

  if (tune || mode==modeCWL || mode==modeCWU) {
    mic_sample_double=0.0;
  } else {
    mic_sample_double=(double)mic_sample;
  }

//
// shape CW pulses when doing CW and transmitting, else nullify them
//
  if((mode==modeCWL || mode==modeCWU) && isTransmitting()) {
//
//	RigCtl CW sets the variables cw_key_up and cw_key_down
//	to the number of samples for the next down/up sequence.
//	cw_key_down can be zero, for inserting some space
//
//	We HAVE TO shape the signal to avoid hard clicks to be
//	heard way beside our frequency. The envelope (ramp function)
//      is stored in cwramp48[0::RAMPLEN], so we "move" cw_shape between these
//      values. The ramp width is RAMPLEN/48000 seconds.
//
//      In the new protocol, we use this ramp for the side tone, but
//      must use values from cwramp192 for the TX iq signal.
//
//      Note that usually, the pulse is much broader than the ramp,
//      that is, cw_key_down and cw_key_up are much larger than RAMPLEN.
//
	cw_not_ready=0;
	if (cw_key_down > 0 ) {
	  if (cw_shape < RAMPLEN) cw_shape++;	// walk up the ramp
	  cw_key_down--;			// decrement key-up counter
	  updown=1;
	} else {
	  // dig into this even if cw_key_up is already zero, to ensure
	  // that we reach the bottom of the ramp for very small pauses
	  if (cw_shape > 0) cw_shape--;	// walk down the ramp
	  if (cw_key_up > 0) cw_key_up--; // decrement key-down counter
	  updown=0;
	}
	//
	// store the ramp value in cw_shape_buffer, but also use it for shaping the "local"
	// side tone
	ramp=cwramp48[cw_shape];
	cwsample=0.00197 * getNextSideToneSample() * cw_keyer_sidetone_volume * ramp;
	cw_audio_write(cwsample);
        cw_shape_buffer48[tx->samples]=ramp;
	//
	// In the new protocol, we MUST maintain a constant flow of audio samples to the radio
	// (at least for ANAN-200D and ANAN-7000 internal side tone generation)
	// So we ship out audio: silence if CW is internal, side tone if CW is local.
	//
	// Furthermore, for each audio sample we have to create four TX samples. If we are at
	// the beginning of the ramp, these are four zero samples, if we are at the, it is
	// four unit samples, and in-between, we use the values from cwramp192.
	// Note that this ramp has been extended a little, such that it begins with four zeros
	// and ends with four times 1.0.
	//
        if (protocol == NEW_PROTOCOL) {
 	    s=0;
 	    if (!cw_keyer_internal || CAT_cw_is_active) s=(int) (cwsample * 32767.0);
	    new_protocol_cw_audio_samples(s, s);
	    s=4*cw_shape;
	    i=4*tx->samples;
	    // The 192kHz-ramp is constructed such that for cw_shape==0 or cw_shape==RAMPLEN,
	    // the two following cases create the same shape.
	    if (updown) {
	      // climbing up...
	      cw_shape_buffer192[i+0]=cwramp192[s+0];
	      cw_shape_buffer192[i+1]=cwramp192[s+1];
	      cw_shape_buffer192[i+2]=cwramp192[s+2];
	      cw_shape_buffer192[i+3]=cwramp192[s+3];
	   } else {
	      // descending...
	      cw_shape_buffer192[i+0]=cwramp192[s+3];
	      cw_shape_buffer192[i+1]=cwramp192[s+2];
	      cw_shape_buffer192[i+2]=cwramp192[s+1];
	      cw_shape_buffer192[i+3]=cwramp192[s+0];
	   }
	}
#ifdef SOAPYSDR
        if (protocol == SOAPYSDR_PROTOCOL) {
          //
          // The ratio between the TX and microphone sample rate can be any value, so
          // it is difficult to construct a general ramp here. We may at least *assume*
          // that the ratio is integral. We can extrapolate from the shapes calculated
          // for 48 and 192 kHz sample rate.
          //
          // At any rate, we *must* produce tx->outputsamples IQ samples from an input
          // buffer of size tx->buffer_size.
          //
          int ratio = tx->output_samples / tx->buffer_size;
          int j;
          i=ratio*tx->samples;  // current position in TX IQ buffer
          if (updown) {
            //
            // Climb up the ramp
            //
            if (ratio % 4 == 0) {
              // simple adaptation from the 192 kHz ramp
              ratio = ratio / 4;
	      s=4*cw_shape;
	      for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp192[s+0];
	      for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp192[s+1];
	      for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp192[s+2];
	      for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp192[s+3];
            } else {
              // simple adaptation from the 48 kHz ramp
              for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp48[cw_shape];
            }
          } else {
            //
            // Walk down the ramp
            //
            if (ratio % 4 == 0) {
              // simple adaptation from the 192 kHz ramp
              ratio = ratio / 4;
	      s=4*cw_shape;
	      for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp192[s+3];
	      for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp192[s+2];
	      for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp192[s+1];
	      for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp192[s+0];
            } else {
              // simple adaptation from the 48 kHz ramp
              for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=cwramp48[cw_shape];
            }
          }
        }
#endif
  } else {
//
//	If no longer transmitting, or no longer doing CW: reset pulse shaper.
//	This will also swallow any pending CW in rigtl CAT CW and wipe out
//      cw_shape_buffer very quickly. In order to tell rigctl etc. that CW should be
//	aborted, we also use the cw_not_ready flag.
//
	cw_not_ready=1;
	cw_key_up=0;
	cw_key_down=0;
	cw_shape=0;
        // insert "silence" in CW audio and TX IQ buffers
  	cw_shape_buffer48[tx->samples]=0.0;
	if (protocol == NEW_PROTOCOL) {
	  cw_shape_buffer192[4*tx->samples+0]=0.0;
	  cw_shape_buffer192[4*tx->samples+1]=0.0;
	  cw_shape_buffer192[4*tx->samples+2]=0.0;
	  cw_shape_buffer192[4*tx->samples+3]=0.0;
	}
#ifdef SOAPYSDR
        if (protocol == SOAPYSDR_PROTOCOL) {
          //
          // this essentially the P2 code, where the ratio
          // is fixed to 4
          //
          int ratio = tx->output_samples / tx->buffer_size;
          int i=ratio*tx->samples;
          int j;
          for (j=0; j<ratio; j++) cw_shape_buffer192[i++]=0.0;
        }
#endif
  }
  tx->mic_input_buffer[tx->samples*2]=mic_sample_double;
  tx->mic_input_buffer[(tx->samples*2)+1]=0.0; //mic_sample_double;
  tx->samples++;
  if(tx->samples==tx->buffer_size) {
    full_tx_buffer(tx);
    tx->samples=0;
  }
  
#ifdef AUDIO_WATERFALL
  if(audio_samples!=NULL && isTransmitting()) {
    if(waterfall_samples==0) {
       audio_samples[audio_samples_index]=(float)mic_sample;
       audio_samples_index++;
       if(audio_samples_index>=AUDIO_WATERFALL_SAMPLES) {
         //Spectrum(CHANNEL_AUDIO,0,0,audio_samples,audio_samples);
         audio_samples_index=0;
     }
     }
     waterfall_samples++;
     if(waterfall_samples==waterfall_resample) {
       waterfall_samples=0;
     }
   }
#endif
}

void add_ps_iq_samples(TRANSMITTER *tx, double i_sample_tx,double q_sample_tx, double i_sample_rx, double q_sample_rx) {
//
// If not compiled for PURESIGNAL, make this a dummy function
//
#ifdef PURESIGNAL
  RECEIVER *tx_feedback=receiver[PS_TX_FEEDBACK];
  RECEIVER *rx_feedback=receiver[PS_RX_FEEDBACK];

//fprintf(stderr,"add_ps_iq_samples: samples=%d i_rx=%f q_rx=%f i_tx=%f q_tx=%f\n",rx_feedback->samples, i_sample_rx,q_sample_rx,i_sample_tx,q_sample_tx);

  tx_feedback->iq_input_buffer[tx_feedback->samples*2]=i_sample_tx;
  tx_feedback->iq_input_buffer[(tx_feedback->samples*2)+1]=q_sample_tx;
  rx_feedback->iq_input_buffer[rx_feedback->samples*2]=i_sample_rx;
  rx_feedback->iq_input_buffer[(rx_feedback->samples*2)+1]=q_sample_rx;

  tx_feedback->samples=tx_feedback->samples+1;
  rx_feedback->samples=rx_feedback->samples+1;

  if(rx_feedback->samples>=rx_feedback->buffer_size) {
    if(isTransmitting()) {
      pscc(transmitter->id, rx_feedback->buffer_size, tx_feedback->iq_input_buffer, rx_feedback->iq_input_buffer);
      if(transmitter->displaying && transmitter->feedback) {
        Spectrum0(1, rx_feedback->id, 0, 0, rx_feedback->iq_input_buffer);
      }
    }
    rx_feedback->samples=0;
    tx_feedback->samples=0;
  }
#endif
}

void tx_set_displaying(TRANSMITTER *tx,int state) {
  tx->displaying=state;
  if(state) {
    tx->update_timer_id=gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE,1000/tx->fps, update_display, (gpointer)tx, NULL);
  }
}

void tx_set_ps(TRANSMITTER *tx,int state) {
#ifdef PURESIGNAL
  //
  // Switch PURESIGNAL on (state !=0) or off (state==0)
  //
  // The following rules must be obeyed:
  //
  // a.) do not call SetPSControl unless you know the feedback
  //     data streams are flowing. Otherwise, these calls may
  //     be have no effect (experimental observation)
  //
  // b.  in the old protocol, do not change the value of
  //     tx->puresignal unless the protocol is stopped.
  //     (to have a safe re-configuration of the number of
  //     RX streams)
  //
  if (!state) {
    // if switching off, stop PS engine first, keep feedback
    // streams flowing for a while to be sure SetPSControl works.
    SetPSControl(tx->id, 1, 0, 0, 0);
    usleep(100000);
  }
  switch (protocol) {
    case ORIGINAL_PROTOCOL:
      // stop protocol, change PS state, restart protocol
      old_protocol_stop();
      usleep(100000);
      tx->puresignal = state ? 1 : 0;
      old_protocol_run();
      break;
    case NEW_PROTOCOL:
      // change PS state and tell radio about it
      tx->puresignal = state ? 1 : 0;
      schedule_high_priority();
      schedule_receive_specific();
#ifdef SOAPY_SDR
    case SOAPY_PROTOCOL:
      // are there feedback channels in SOAPY?
      break;
#endif
  }
  if(state) {
    // if switching on: wait a while to get the feedback
    // streams flowing, then start PS engine
    usleep(100000);
    SetPSControl(tx->id, 0, 0, 1, 0);
  }
  // update screen
  g_idle_add(ext_vfo_update,NULL);
#endif
}

void tx_set_twotone(TRANSMITTER *tx,int state) {
  transmitter->twotone=state;
  if(state) {
    // set frequencies and levels
    switch(tx->mode) {
      case modeCWL:
      case modeLSB:
      case modeDIGL:
	SetTXAPostGenTTFreq(transmitter->id, -900.0, -1700.0);
        break;
      default:
	SetTXAPostGenTTFreq(transmitter->id, 900.0, 1700.0);
	break;
    }
    SetTXAPostGenTTMag (transmitter->id, 0.49, 0.49);
    SetTXAPostGenMode(transmitter->id, 1);
    SetTXAPostGenRun(transmitter->id, 1);
  } else {
    SetTXAPostGenRun(transmitter->id, 0);
  }
  g_idle_add(ext_mox_update,GINT_TO_POINTER(state));
}

void tx_set_ps_sample_rate(TRANSMITTER *tx,int rate) {
#ifdef PURESIGNAL
  SetPSFeedbackRate (tx->id,rate);
#endif
}

// Sine tone generator:
// somewhat improved, and provided two siblings
// for generating side tones simultaneously on the
// HPSDR board and local audio.

#define TWOPIOVERSAMPLERATE 0.0001308996938995747;  // 2 Pi / 48000

static long asteps = 0;
static long bsteps = 0;

double getNextSideToneSample() {
  	double angle = (asteps*cw_keyer_sidetone_frequency)*TWOPIOVERSAMPLERATE;
	if (++asteps == 48000) asteps = 0;
	return sin(angle);
}

double getNextInternalSideToneSample() {
	double angle = (bsteps*cw_keyer_sidetone_frequency)*TWOPIOVERSAMPLERATE;
	if (++bsteps == 48000) bsteps = 0;
	return sin(angle);
}
