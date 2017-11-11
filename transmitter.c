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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <wdsp.h>

#include "alex.h"
#include "band.h"
#include "bandstack.h"
#include "channel.h"
#include "main.h"
#include "meter.h"
#include "mode.h"
#include "property.h"
#include "radio.h"
#include "vfo.h"
#include "vox.h"
#include "meter.h"
#include "toolbar.h"
#include "tx_panadapter.h"
#include "waterfall.h"
#include "transmitter.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef FREEDV
#include "freedv.h"
#endif
#include "audio_waterfall.h"

#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

static int filterLow;
static int filterHigh;

static int waterfall_samples=0;
static int waterfall_resample=8;

static gint update_out_of_band(gpointer data) {
  TRANSMITTER *tx=(TRANSMITTER *)data;
  tx->out_of_band=0;
  vfo_update();
  return FALSE;
}

void transmitter_set_out_of_band(TRANSMITTER *tx) {
  tx->out_of_band=1;
  tx->out_of_band_timer_id=gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE,1000,update_out_of_band, tx, NULL);
}

void transmitter_set_deviation(TRANSMITTER *tx) {
  SetTXAFMDeviation(tx->id, (double)tx->deviation);
}

void transmitter_set_am_carrier_level(TRANSMITTER *tx) {
  SetTXAAMCarrierLevel(tx->id, tx->am_carrier_level);
}

void transmitter_set_ctcss(TRANSMITTER *tx,int run,double frequency) {
  tx->ctcss=run;
  tx->ctcss_frequency=frequency;
fprintf(stderr,"transmitter_set_ctcss: ctcss_frequency=%f run=%d\n",tx->ctcss_frequency,tx->ctcss);
  SetTXACTCSSFreq(tx->id, tx->ctcss_frequency);
  SetTXACTCSSRun(tx->id, tx->ctcss);
}

void transmitter_set_compressor_level(TRANSMITTER *tx,double level) {
  tx->compressor_level=level;
  SetTXACompressorGain(tx->id, tx->compressor_level);
}

void transmitter_set_compressor(TRANSMITTER *tx,int state) {
  tx->compressor=state;
  SetTXACompressorRun(tx->id, tx->compressor);
}

void reconfigure_transmitter(TRANSMITTER *tx,int height) {
  gtk_widget_set_size_request(tx->panadapter, tx->width, height);
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
  sprintf(name,"transmitter.%d.input_device",tx->id);
  sprintf(value,"%d",tx->input_device);
  setProperty(name,value);

  sprintf(name,"transmitter.%d.low_latency",tx->id);
  sprintf(value,"%d",tx->low_latency);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.puresignal",tx->id);
  sprintf(value,"%d",tx->puresignal);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.auto_on",tx->id);
  sprintf(value,"%d",tx->auto_on);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.single_on",tx->id);
  sprintf(value,"%d",tx->single_on);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.ctcss",tx->id);
  sprintf(value,"%d",tx->ctcss);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.ctcss_frequency",tx->id);
  sprintf(value,"%f",tx->ctcss_frequency);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.deviation",tx->id);
  sprintf(value,"%d",tx->deviation);
  setProperty(name,value);
  sprintf(name,"transmitter.%d.am_carrier_level",tx->id);
  sprintf(value,"%f",tx->am_carrier_level);
  setProperty(name,value);
#ifdef FREEDV
  if(strlen(tx->freedv_text_data)>0) {
    sprintf(name,"transmitter.%d.freedv_text_data",tx->id);
    sprintf(value,"%s",tx->freedv_text_data);
    setProperty(name,value);
  }
#endif
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
  sprintf(name,"transmitter.%d.input_device",tx->id);
  value=getProperty(name);
  if(value) tx->input_device=atoi(value);
  sprintf(name,"transmitter.%d.low_latency",tx->id);
  value=getProperty(name);
  if(value) tx->low_latency=atoi(value);
  sprintf(name,"transmitter.%d.puresignal",tx->id);
  value=getProperty(name);
  if(value) tx->puresignal=atoi(value);
  sprintf(name,"transmitter.%d.auto_on",tx->id);
  value=getProperty(name);
  if(value) tx->auto_on=atoi(value);
  sprintf(name,"transmitter.%d.single_on",tx->id);
  value=getProperty(name);
  if(value) tx->single_on=atoi(value);
  sprintf(name,"transmitter.%d.ctcss",tx->id);
  value=getProperty(name);
  if(value) tx->ctcss=atoi(value);
  sprintf(name,"transmitter.%d.ctcss_frequency",tx->id);
  value=getProperty(name);
  if(value) tx->ctcss_frequency=atof(value);
  sprintf(name,"transmitter.%d.deviation",tx->id);
  value=getProperty(name);
  if(value) tx->deviation=atoi(value);
  sprintf(name,"transmitter.%d.am_carrier_level",tx->id);
  value=getProperty(name);
  if(value) tx->am_carrier_level=atof(value);
#ifdef FREEDV
  sprintf(name,"transmitter.%d.freedv_text_data",tx->id);
  value=getProperty(name);
  if(value) strcpy(tx->freedv_text_data,value);
#endif
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
}

static gboolean update_display(gpointer data) {
  TRANSMITTER *tx=(TRANSMITTER *)data;
  int rc;
  double fwd;
  double rev;
  double exciter;

  int i;

//fprintf(stderr,"update_display: tx id=%d\n",tx->id);
  if(tx->displaying) {
#ifdef AUDIO_SAMPLES
    if(audio_samples!=NULL) {
      GetPixels(CHANNEL_AUDIO,0,audio_samples,&rc);
      if(rc) {
        audio_waterfall_update();
      }
    }
#endif
    GetPixels(tx->id,0,tx->pixel_samples,&rc);
    if(rc) {
      tx_panadapter_update(tx);
    }

    double alc=GetTXAMeter(tx->id, alc);
    double constant1=3.3;
    double constant2=0.095;

#ifdef RADIOBERRY
	if(protocol==ORIGINAL_PROTOCOL || protocol==RADIOBERRY_PROTOCOL) {
#else
	if(protocol==ORIGINAL_PROTOCOL) {
#endif
      switch(device) {
        case DEVICE_METIS:
          constant1=3.3;
          constant2=0.09;
          break;
        case DEVICE_HERMES:
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
          break;
        case DEVICE_ORION2:
          constant1=5.0;
          constant2=0.108;
          break;
        case DEVICE_HERMES_LITE:
          break;
#ifdef RADIOBERRY
		case RADIOBERRY_SPI_DEVICE:
			break;
#endif
      }

      int power=alex_forward_power;
      if(power==0) {
        power=exciter_power;
      }
      double v1;
      v1=((double)power/4095.0)*constant1;
      fwd=(v1*v1)/constant2;

      power=exciter_power;
      v1=((double)power/4095.0)*constant1;
      exciter=(v1*v1)/constant2;

      rev=0.0;
      if(alex_forward_power!=0) {
        power=alex_reverse_power;
        v1=((double)power/4095.0)*constant1;
        rev=(v1*v1)/constant2;
      }
    } else {
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
          break;
        case NEW_DEVICE_ORION2:
          constant1=5.0;
          constant2=0.108;
          break;
        case NEW_DEVICE_HERMES_LITE:
          constant1=3.3;
          constant2=0.09;
          break;
#ifdef RADIOBERRY
		case RADIOBERRY_SPI_DEVICE:
			constant1=3.3;
			constant2=0.09;
			break;
#endif
      }

      int power=alex_forward_power;
      if(power==0) {
        power=exciter_power;
      }
      double v1;
      v1=((double)power/4095.0)*constant1;
      fwd=(v1*v1)/constant2;

      power=exciter_power;
      v1=((double)power/4095.0)*constant1;
      exciter=(v1*v1)/constant2;

      rev=0.0;
      if(alex_forward_power!=0) {
        power=alex_reverse_power;
        v1=((double)power/4095.0)*constant1;
        rev=(v1*v1)/constant2;
      }
    } 

    meter_update(POWER,fwd,rev,exciter,alc);

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
    int avm = 0;
    double tau = 0.001 * 120.0;
    int calibration_data_set = 0;
    double span_min_freq = 0.0;
    double span_max_freq = 0.0;

    int max_w = fft_size + (int) min(keep_time * (double) tx->fps, keep_time * (double) fft_size * (double) tx->fps);

    overlap = (int)max(0.0, ceil(fft_size - (double)tx->mic_sample_rate / (double)tx->fps));

    fprintf(stderr,"SetAnalyzer id=%d buffer_size=%d overlap=%d\n",tx->id,tx->output_samples,overlap);


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

}

static void create_visual(TRANSMITTER *tx) {
 
  fprintf(stderr,"transmitter: create_visual: id=%d\n",tx->id);

  tx->panel=gtk_fixed_new();
  gtk_widget_set_size_request (tx->panel, tx->width, tx->height);

  if(tx->display_panadapter) {
    tx_panadapter_init(tx,tx->width,tx->height);
    gtk_fixed_put(GTK_FIXED(tx->panel),tx->panadapter,0,0);
  }

}

TRANSMITTER *create_transmitter(int id, int buffer_size, int fft_size, int fps, int width, int height) {
  int rc;

  TRANSMITTER *tx=malloc(sizeof(TRANSMITTER));
  tx->id=id;
  tx->buffer_size=buffer_size;
  tx->fft_size=fft_size;
  tx->fps=fps;

#ifdef RADIOBERRY
	if(protocol==ORIGINAL_PROTOCOL || protocol==RADIOBERRY_PROTOCOL) {
#else
	if(protocol==ORIGINAL_PROTOCOL) {
#endif
    tx->mic_sample_rate=48000;
    tx->mic_dsp_rate=48000;
    tx->iq_output_rate=48000;
    tx->output_samples=tx->buffer_size;
    tx->pixels=width; // to allow 48k to 24k conversion
  } else {
    tx->mic_sample_rate=48000;
    tx->mic_dsp_rate=96000;
    tx->iq_output_rate=192000;
    tx->output_samples=tx->buffer_size*4;
    tx->pixels=width*4; // to allow 192k to 24k conversion
  }

  tx->width=width;
  tx->height=height;
  tx->display_panadapter=1;
  tx->display_waterfall=0;

  tx->panadapter_high=0;
  tx->panadapter_low=-60;

  tx->displaying=0;
  
  tx->alex_antenna=ALEX_TX_ANTENNA_1;

fprintf(stderr,"create_transmitter: id=%d buffer_size=%d mic_sample_rate=%d mic_dsp_rate=%d iq_output_rate=%d output_samples=%d fps=%d\n",tx->id, tx->buffer_size, tx->mic_sample_rate, tx->mic_dsp_rate, tx->iq_output_rate, tx->output_samples,tx->fps);

  tx->filter_low=tx_filter_low;
  tx->filter_high=tx_filter_high;

  tx->out_of_band=0;

  tx->low_latency=0;

  tx->puresignal=0;
  tx->feedback=0;
  tx->twotone=0;
  tx->auto_on=0;
  tx->single_on=0;

  tx->ctcss=0;
  tx->ctcss_frequency=100.0;

  tx->deviation=2500;
  tx->am_carrier_level=0.5;

#ifdef FREEDV
  strcpy(tx->freedv_text_data,"Call, Name and Location");
  tx->freedv_samples=0;
#endif

  tx->drive=50;
  tx->tune_percent=10;
  tx->tune_use_drive=0;

  tx->compressor=0;
  tx->compressor_level=0.0;

  transmitter_restore_state(tx);


  // allocate buffers
fprintf(stderr,"transmitter: allocate buffers: mic_input_buffer=%d iq_output_buffer=%d pixels=%d\n",tx->buffer_size,tx->output_samples,tx->pixels);
  tx->mic_input_buffer=malloc(sizeof(double)*2*tx->buffer_size);
  tx->iq_output_buffer=malloc(sizeof(double)*2*tx->output_samples);
  tx->samples=0;
  tx->pixel_samples=malloc(sizeof(float)*tx->pixels);
fprintf(stderr,"transmitter: allocate buffers: mic_input_buffer=%p iq_output_buffer=%p pixels=%p\n",tx->mic_input_buffer,tx->iq_output_buffer,tx->pixel_samples);

  fprintf(stderr,"create_transmitter: OpenChannel id=%d buffer_size=%d fft_size=%d sample_rate=%d dspRate=%d outputRate=%d\n",
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


  int mode=vfo[VFO_A].mode;
  if(split) {
    mode=vfo[VFO_B].mode;
  }

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

  transmitter_set_ctcss(tx,tx->ctcss,tx->ctcss_frequency);
  SetTXAAMSQRun(tx->id, 0);
  SetTXAosctrlRun(tx->id, 0);

  SetTXAALCAttack(tx->id, 1);
  SetTXAALCDecay(tx->id, 10);
  SetTXAALCSt(tx->id, tx_alc);

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
  SetTXAPostGenToneFreq(tx->id, 0.0);
  SetTXAPostGenRun(tx->id, 0);

  double gain=pow(10.0, mic_gain / 20.0);
  SetTXAPanelGain1(tx->id,gain);
  SetTXAPanelRun(tx->id, 1);

  SetTXAFMDeviation(tx->id, (double)tx->deviation);
  SetTXAAMCarrierLevel(tx->id, tx->am_carrier_level);

  SetTXACompressorGain(tx->id, tx->compressor_level);
  SetTXACompressorRun(tx->id, tx->compressor);

  tx_set_mode(tx,mode);

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
    tx->mode=mode;
    SetTXAMode(tx->id, tx->mode);
    tx_set_filter(tx,tx_filter_low,tx_filter_high);
  }
}

void tx_set_filter(TRANSMITTER *tx,int low,int high) {
  int mode;
  if(split) {
    mode=vfo[1].mode;
  } else {
    mode=vfo[0].mode;
  }
//fprintf(stderr,"tx_set_filter: tx=%p mode=%d low=%d high=%d\n",tx,mode,low,high);
  switch(mode) {
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

  if(split) {
    fl+=vfo[VFO_B].offset;
    fh+=vfo[VFO_B].offset;
  } else {
    fl+=vfo[VFO_A].offset;
    fh+=vfo[VFO_A].offset;
  }
  SetTXABandpassFreqs(tx->id, fl,fh);
}

void tx_set_pre_emphasize(TRANSMITTER *tx,int state) {
  SetTXAFMEmphPosition(tx->id,state);
}

static void full_tx_buffer(TRANSMITTER *tx) {
  long isample;
  long qsample;
  double gain;
  int j;
  int error;
  int mode;

  switch(protocol) {
#ifdef RADIOBERRY
    case RADIOBERRY_PROTOCOL:
#endif
    case ORIGINAL_PROTOCOL:
      gain=32767.0;  // 16 bit
      break;
    case NEW_PROTOCOL:
      gain=8388607.0; // 24 bit
      break;
  }

/*
  if(vox_enabled || vox_setting) {
    if(split) {
      mode=vfo[1].mode;
    } else {
      mode=vfo[0].mode;
    }
    switch(mode) {
      case modeLSB:
      case modeUSB:
      case modeDSB:
      case modeFMN:
      case modeAM:
      case modeSAM:
        update_vox(tx);
        break;
    }
  }
*/
  update_vox(tx);

  fexchange0(tx->id, tx->mic_input_buffer, tx->iq_output_buffer, &error);
  if(error!=0) {
    fprintf(stderr,"full_tx_buffer: id=%d fexchange0: error=%d\n",tx->id,error);
  }

  if(tx->displaying && !(tx->puresignal && tx->feedback)) {
    Spectrum0(1, tx->id, 0, 0, tx->iq_output_buffer);
  }

  if(isTransmitting()) {

    if(radio->device==NEW_DEVICE_ATLAS && atlas_penelope) {
      if(tune && !transmitter->tune_use_drive) {
        gain=gain*((double)transmitter->drive_level*100.0/(double)transmitter->tune_percent);
      } else {
        gain=gain*(double)transmitter->drive_level;
      }
    }

    for(j=0;j<tx->output_samples;j++) {
      //isample=(long)(tx->iq_output_buffer[j*2]*gain);
      //qsample=(long)(tx->iq_output_buffer[(j*2)+1]*gain);
      double is=tx->iq_output_buffer[j*2];
      double qs=tx->iq_output_buffer[(j*2)+1];
      isample=is>=0.0?(long)floor(is*gain+0.5):(long)ceil(is*gain-0.5);
      qsample=qs>=0.0?(long)floor(qs*gain+0.5):(long)ceil(qs*gain-0.5);
      switch(protocol) {
        case ORIGINAL_PROTOCOL:
          old_protocol_iq_samples(isample,qsample);
          break;
        case NEW_PROTOCOL:
          new_protocol_iq_samples(isample,qsample);
          break;
#ifdef RADIOBERRY
        case RADIOBERRY_PROTOCOL:
          radioberry_protocol_iq_samples(isample,qsample);
          break;
#endif
      }
    }
  }

}

void add_mic_sample(TRANSMITTER *tx,short mic_sample) {
  int mode;
  double sample;
  double mic_sample_double;
  int i,s;

  if(split) {
    mode=vfo[1].mode;
  } else {
    mode=vfo[0].mode;
  }

  if(mode==modeCWL || mode==modeCWU || tune) {
    mic_sample_double=0.0;
  } else {
    //long sam=mic_sample<<16;
    //sample=(double)sam;
    //mic_sample_double=(1.0 / 2147483648.0) * sample;
    mic_sample_double=(double)mic_sample/32768.0;
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

#ifdef FREEDV
void add_freedv_mic_sample(TRANSMITTER *tx, short mic_sample) {
  int i,s;

  //if(active_receiver->freedv && isTransmitting() && !tune) {
  if(!tune) {
    if(tx->freedv_samples==0) {
      //int modem_samples=mod_sample_freedv(mic_sample);
      short vs=(short)((double)mic_sample*pow(10.0, mic_gain / 20.0));
      int modem_samples=mod_sample_freedv(vs);
      if(modem_samples!=0) {
        for(s=0;s<modem_samples;s++) {
          for(i=0;i<freedv_resample;i++) { // 8K to 48K
            add_mic_sample(tx,mod_out[s]);
          }
        }
      }
    }
    tx->freedv_samples++;
    if(tx->freedv_samples>=freedv_resample) {
      tx->freedv_samples=0;
    }
  }
}
#endif

void tx_set_displaying(TRANSMITTER *tx,int state) {
  tx->displaying=state;
  if(state) {
    tx->update_timer_id=gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE,1000/tx->fps, update_display, (gpointer)tx, NULL);
  }
}

void tx_set_ps(TRANSMITTER *tx,int state) {
  tx->puresignal=state;
  if(state) {
    SetPSControl(tx->id, 0, 0, 1, 0);
  } else {
    SetPSControl(tx->id, 1, 0, 0, 0);
  }
  vfo_update();
}

void tx_set_twotone(TRANSMITTER *tx,int state) {
  transmitter->twotone=state;
  if(state) {
    SetTXAPostGenMode(transmitter->id, 1);
    SetTXAPostGenRun(transmitter->id, 1);
    mox_update(1);
  } else {
    SetTXAPostGenRun(transmitter->id, 0);
    mox_update(0);
  }
}

void tx_set_ps_sample_rate(TRANSMITTER *tx,int rate) {
  SetPSFeedbackRate (tx->id,rate);
}
