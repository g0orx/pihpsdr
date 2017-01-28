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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>

#include "agc.h"
#include "band.h"
#include "alex.h"
#include "new_protocol.h"
#include "channel.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "wdsp.h"
#include "radio.h"
#include "vfo.h"
#include "toolbar.h"
#include "wdsp_init.h"
#ifdef FREEDV
#include "freedv.h"
#endif
#ifdef PSK
#include "main.h"
#include "psk.h"
#endif

#define PI 3.1415926535897932F
#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

static int receiver;
static int running=0;

static int buffer_size=BUFFER_SIZE;
static int tx_buffer_size=BUFFER_SIZE;
static int fft_size=4096;
static int dspRate=48000;
static int outputRate=48000;
static int dvOutputRate=8000;

static int micSampleRate=48000;
static int micDspRate=48000;
static int micOutputRate=192000;

static int spectrumWIDTH=800;
static int SPECTRUM_UPDATES_PER_SECOND=10;

static void initAnalyzer(int channel,int buffer_size);

static void calc_tx_buffer_size() {
  if(protocol==ORIGINAL_PROTOCOL | protocol==RADIOBERRY_PROTOCOL) {
    switch(sample_rate) {
      case 48000:
        tx_buffer_size=BUFFER_SIZE;
        break;
      case 96000:
        tx_buffer_size=BUFFER_SIZE/2;
        break;
      case 192000:
        tx_buffer_size=BUFFER_SIZE/4;
        break;
      case 384000:
        tx_buffer_size=BUFFER_SIZE/8;
        break;
    }
  } else {
    tx_buffer_size=BUFFER_SIZE; // input always 192K
  }
}

void setRXMode(int rx,int m) {
  SetRXAMode(rx, m);
}

void setTXMode(int tx,int m) {
  SetTXAMode(tx, m);
}

void setMode(int m) {
int local_mode=m;
#ifdef FREEDV
    if(mode!=modeFREEDV && m==modeFREEDV) {
      local_mode=modeUSB;
      init_freedv();
    } else if(mode==modeFREEDV && m!=modeFREEDV) {
      close_freedv();
    }
#endif
#ifdef PSK
    if(mode!=modePSK && m==modePSK) {
      local_mode=modeUSB;
      //init_psk();
      show_psk();
    } else if(mode==modePSK && m!=modePSK) {
      //close_psk();
      show_waterfall();
    }
#endif
    setRXMode(receiver,local_mode);
    setTXMode(CHANNEL_TX,local_mode);
    mode=m;
}

int getMode() {
    return mode;
}

void setFilter(int low,int high) {

    if(mode==modeCWL) {
        filterLow=-cw_keyer_sidetone_frequency-low;
        filterHigh=-cw_keyer_sidetone_frequency+high;
    } else if(mode==modeCWU) {
        filterLow=cw_keyer_sidetone_frequency-low;
        filterHigh=cw_keyer_sidetone_frequency+high;
    } else {
        filterLow=low;
        filterHigh=high;
    }

    RXASetPassband(receiver,(double)filterLow,(double)filterHigh);

    double fl=filterLow+ddsOffset;
    double fh=filterHigh+ddsOffset;
    SetTXABandpassFreqs(CHANNEL_TX, fl,fh);

}

int getFilterLow() {
    return filterLow;
}

int getFilterHigh() {
    return filterHigh;
}

void wdsp_set_agc(int rx, int agc) {
  
  SetRXAAGCMode(rx, agc);
  //SetRXAAGCThresh(rx, agc_thresh_point, 4096.0, sample_rate);
  SetRXAAGCSlope(rx,agc_slope);
  SetRXAAGCTop(rx,agc_gain);
  switch(agc) {
    case AGC_OFF:
      break;
    case AGC_LONG:
      SetRXAAGCAttack(rx,2);
      SetRXAAGCHang(rx,2000);
      SetRXAAGCDecay(rx,2000);
      SetRXAAGCHangThreshold(rx,(int)agc_hang_threshold);
      break;
    case AGC_SLOW:
      SetRXAAGCAttack(rx,2);
      SetRXAAGCHang(rx,1000);
      SetRXAAGCDecay(rx,500);
      SetRXAAGCHangThreshold(rx,(int)agc_hang_threshold);
      break;
    case AGC_MEDIUM:
      SetRXAAGCAttack(rx,2);
      SetRXAAGCHang(rx,0);
      SetRXAAGCDecay(rx,250);
      SetRXAAGCHangThreshold(rx,100);
      break;
    case AGC_FAST:
      SetRXAAGCAttack(rx,2);
      SetRXAAGCHang(rx,0);
      SetRXAAGCDecay(rx,50);
      SetRXAAGCHangThreshold(rx,100);
      break;
  }
}

void wdsp_set_offset(long long offset) {
  if(offset==0) {
    SetRXAShiftFreq(receiver, (double)offset);
    RXANBPSetShiftFrequency(receiver, (double)offset);
    SetRXAShiftRun(receiver, 0);
  } else {
    SetRXAShiftFreq(receiver, (double)offset);
    RXANBPSetShiftFrequency(receiver, (double)offset);
    SetRXAShiftRun(receiver, 1);
  }
}

void wdsp_set_input_rate(double rate) {
    SetInputSamplerate(receiver, (int)rate);
}

static void setupRX(int rx) {
    setRXMode(rx,mode);
    SetRXABandpassFreqs(rx, (double)filterLow, (double)filterHigh);
    
    SetRXAFMDeviation(rx,(double)deviation);
    //SetRXAAGCMode(rx, agc);
    //SetRXAAGCTop(rx,agc_gain);
    wdsp_set_agc(rx, agc);

    SetRXAAMDSBMode(rx, 0);
    SetRXAShiftRun(rx, 0);

    SetRXAEMNRPosition(rx, nr_agc);
    SetRXAEMNRgainMethod(rx, nr2_gain_method);
    SetRXAEMNRnpeMethod(rx, nr2_npe_method);
    SetRXAEMNRRun(rx, nr2);
    SetRXAEMNRaeRun(rx, nr2_ae);

    SetRXAANRVals(rx, 64, 16, 16e-4, 10e-7); // defaults
    SetRXAANRRun(rx, nr);
    SetRXAANFRun(rx, anf);
    SetRXASNBARun(rx, snb);

    SetRXAPanelGain1(rx, volume);
    SetRXAPanelBinaural(rx, binaural);

    if(enable_rx_equalizer) {
      SetRXAGrphEQ(rx, rx_equalizer);
      SetRXAEQRun(rx, 1);
    } else {
      SetRXAEQRun(rx, 0);
    }

    // setup for diversity
    create_divEXT(0,0,2,BUFFER_SIZE);
    SetEXTDIVRotate(0, 2, &i_rotate, &q_rotate);
    SetEXTDIVRun(0,diversity_enabled);
}

static void setupTX(int tx) {
    setTXMode(tx,mode);
    SetTXABandpassFreqs(tx, (double)filterLow, (double)filterHigh);
    SetTXABandpassWindow(tx, 1);
    SetTXABandpassRun(tx, 1);

    SetTXAFMDeviation(tx,(double)deviation);
    SetTXAFMEmphPosition(tx,pre_emphasize);

    SetTXACFIRRun(tx, protocol==NEW_PROTOCOL?1:0); // turned in if new protocol
    if(enable_tx_equalizer) {
      SetTXAGrphEQ(tx, tx_equalizer);
      SetTXAEQRun(tx, 1);
    } else {
      SetTXAEQRun(tx, 0);
    }
    SetTXACTCSSRun(tx, 0);
    SetTXAAMSQRun(tx, 0);
    SetTXACompressorRun(tx, 0);
    SetTXAosctrlRun(tx, 0);

    SetTXAALCAttack(tx, 1);
    SetTXAALCDecay(tx, 10);
    SetTXAALCSt(tx, tx_alc);

    SetTXALevelerAttack(tx, 1);
    SetTXALevelerDecay(tx, 500);
    SetTXALevelerTop(tx, 5.0);
    SetTXALevelerSt(tx, tx_leveler);

    SetTXAPreGenMode(tx, 0);
    SetTXAPreGenToneMag(tx, 0.0);
    SetTXAPreGenToneFreq(tx, 0.0);
    SetTXAPreGenRun(tx, 0);

    SetTXAPostGenMode(tx, 0);
    SetTXAPostGenToneMag(tx, tone_level);
    SetTXAPostGenToneFreq(tx, 0.0);
    SetTXAPostGenRun(tx, 0);

    double gain=pow(10.0, mic_gain / 20.0);
    SetTXAPanelGain1(tx,gain);
    SetTXAPanelRun(tx, 1);

    //SetChannelState(tx,1,0);
}

void wdsp_init(int rx,int pixels,int protocol) {
    int rc;
    receiver=rx;
    spectrumWIDTH=pixels;

    fprintf(stderr,"wdsp_init: rx=%d pixels=%d protocol=%d\n",rx,pixels,protocol);
   
    if(protocol==ORIGINAL_PROTOCOL | protocol==RADIOBERRY_PROTOCOL) {
        micSampleRate=sample_rate;
        micOutputRate=48000;
    } else {
        micSampleRate=48000;
        micOutputRate=192000;
    }

    while (gtk_events_pending ())
      gtk_main_iteration ();

    fprintf(stderr,"OpenChannel %d buffer_size=%d fft_size=%d sample_rate=%d dspRate=%d outputRate=%d\n",
                rx,
                buffer_size,
                fft_size,
                sample_rate,
                dspRate,
                outputRate);

    OpenChannel(rx,
                buffer_size,
                fft_size,
                sample_rate,
                dspRate,
                outputRate,
                0, // receive
                0, // run
                0.010, 0.025, 0.0, 0.010, 0);


    while (gtk_events_pending ())
      gtk_main_iteration ();

    calc_tx_buffer_size();

    fprintf(stderr,"OpenChannel %d buffer_size=%d fft_size=%d sample_rate=%d dspRate=%d outputRate=%d\n",
                CHANNEL_TX,
                tx_buffer_size,
                fft_size,
                micSampleRate,
                micDspRate,
                micOutputRate);

    OpenChannel(CHANNEL_TX,
                tx_buffer_size,
                fft_size,
                micSampleRate,
                micDspRate,
                micOutputRate,
                1, // transmit
                0, // run
                0.010, 0.025, 0.0, 0.010, 0);

    while (gtk_events_pending ())
      gtk_main_iteration ();

    fprintf(stderr,"XCreateAnalyzer %d\n",rx);
    int success;
    XCreateAnalyzer(rx, &success, 262144, 1, 1, "");
        if (success != 0) {
            fprintf(stderr, "XCreateAnalyzer %d failed: %d\n" ,rx,success);
        }
    initAnalyzer(rx,buffer_size);

    SetDisplayDetectorMode(rx, 0, display_detector_mode);
    SetDisplayAverageMode(rx, 0,  display_average_mode);
    
    calculate_display_average();
    //SetDisplayAvBackmult(rx, 0, display_avb);
    //SetDisplayNumAverage(rx, 0, display_average);

    while (gtk_events_pending ())
      gtk_main_iteration ();

    XCreateAnalyzer(CHANNEL_TX, &success, 262144, 1, 1, "");
        if (success != 0) {
            fprintf(stderr, "XCreateAnalyzer CHANNEL_TX failed: %d\n" ,success);
        }
    initAnalyzer(CHANNEL_TX,tx_buffer_size);

    setupRX(rx);
    setupTX(CHANNEL_TX);

#ifdef PSK
    XCreateAnalyzer(CHANNEL_PSK, &success, PSK_BUFFER_SIZE, 1, 1, "");
        if (success != 0) {
            fprintf(stderr, "XCreateAnalyzer CHANNEL_PSK failed: %d\n" ,success);
        }
    initAnalyzer(CHANNEL_PSK,PSK_BUFFER_SIZE);
#endif

    calcDriveLevel();
    calcTuneDriveLevel();

}

void wdsp_new_sample_rate(int rate) {

  if(protocol==ORIGINAL_PROTOCOL || protocol==RADIOBERRY_PROTOCOL) {
    SetChannelState(CHANNEL_TX,0,0);
    calc_tx_buffer_size();
    initAnalyzer(CHANNEL_TX,tx_buffer_size);
    SetInputSamplerate(CHANNEL_TX,rate);
    SetChannelState(CHANNEL_TX,1,0);
  }

  SetChannelState(receiver,0,0);
  SetInputSamplerate(receiver,rate);
  SetChannelState(receiver,1,0);
}

void wdsp_set_deviation(double deviation) {
  SetRXAFMDeviation(CHANNEL_RX0, deviation);
  SetTXAFMDeviation(CHANNEL_TX, deviation);
}

void wdsp_set_pre_emphasize(int state) {
  SetTXAFMEmphPosition(CHANNEL_TX,state);
}

static void initAnalyzer(int channel,int buffer_size) {
    int flp[] = {0};
    double KEEP_TIME = 0.1;
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
    int pixels=spectrumWIDTH;
    int stitches = 1;
    int avm = 0;
    double tau = 0.001 * 120.0;
    int MAX_AV_FRAMES = 60;
    int display_average = MAX(2, (int) MIN((double) MAX_AV_FRAMES, (double) SPECTRUM_UPDATES_PER_SECOND * tau));
    double avb = exp(-1.0 / (SPECTRUM_UPDATES_PER_SECOND * tau));
    int calibration_data_set = 0;
    double span_min_freq = 0.0;
    double span_max_freq = 0.0;

    int max_w = fft_size + (int) MIN(KEEP_TIME * (double) SPECTRUM_UPDATES_PER_SECOND, KEEP_TIME * (double) fft_size * (double) SPECTRUM_UPDATES_PER_SECOND);

    fprintf(stderr,"SetAnalyzer channel=%d buffer_size=%d\n",channel,buffer_size);
#ifdef PSK
    if(channel==CHANNEL_PSK) {
      data_type=0;
      fft_size=1024;
      overlap=0;
      pixels=spectrumWIDTH;
    }
#endif
    if(channel==CHANNEL_TX && protocol==NEW_PROTOCOL) {
      buffer_size=buffer_size*4;
      pixels=spectrumWIDTH*4; // allows 192 -> 48 easy
    }
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
/*
            avm, //averaging mode
            display_average, //number of spans to (moving) average for pixel result
            avb, //back multiplier for weighted averaging
*/
            calibration_data_set, //identifier of which set of calibration data to use
            span_min_freq, //frequency at first pixel value8192
            span_max_freq, //frequency at last pixel value
            max_w //max samples to hold in input ring buffers
    );
}
