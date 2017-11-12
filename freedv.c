#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <codec2/freedv_api.h>
#include <wdsp.h>

#include "freedv.h"
#include "audio_waterfall.h"
#include "channel.h"
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"

#define min(x,y) (x<y?x:y)

static struct freedv* modem;

int n_speech_samples;
int n_max_modem_samples;
int n_nom_modem_samples;
short *speech_out;
short *demod_in;
short *speech_in;
short *mod_out;
int rx_samples_in;
int tx_samples_in;
int samples_out;
int nin;

int freedv_sync;
float freedv_snr;

int freedv_rate;
int freedv_resample;

int freedv_mode=FREEDV_MODE_1600;

int freedv_sq_enable=1;
double freedv_snr_sq_threshold=0.0;

double freedv_audio_gain=0.2;

static int initialized=0;

static char *mode_string[]={
"1600",
"700",
"700B",
"2400A",
"2400B",
"800XA",
"700C"};

char* freedv_get_mode_string() {
  return mode_string[freedv_mode];
}

void freedv_save_state() {
  char name[128];
  char value[128];

  sprintf(name,"freedv_mode");
  sprintf(value,"%d",freedv_mode);
  setProperty(name,value);
  sprintf(name,"freedv_audio_gain");
  sprintf(value,"%f",freedv_audio_gain);
  setProperty(name,value);
}

void freedv_restore_state() {
  char name[128];
  char *value;

  sprintf(name,"freedv_mode");
  value=getProperty(name);
  if(value) freedv_mode=atoi(value);
  sprintf(name,"freedv_audio_gain");
  value=getProperty(name);
  if(value) freedv_audio_gain=atof(value);
}

void freedv_set_mode(RECEIVER *rx,int m) {
  if(initialized) {
    g_mutex_lock(&rx->freedv_mutex);
    close_freedv(rx);
    freedv_mode=m;
    init_freedv(rx);
    g_mutex_unlock(&rx->freedv_mutex);
  } else {
    freedv_mode=m;
  }
}

static void text_data(char c) {
  int i;
  if(c==0x0D) {
    c='|';
  }
  for(i=0;i<62;i++) {
    active_receiver->freedv_text_data[i]=active_receiver->freedv_text_data[i+1];
  }
  active_receiver->freedv_text_data[62]=c;
}

static void my_put_next_rx_char(void *callback_state, char c) {
  //fprintf(stderr, "freedv: my_put_next_rx_char: %c sync=%d\n", c, freedv_sync);
  if(freedv_sync) {
    text_data(c);
  }
}


static char my_get_next_tx_char(void *callback_state){
  char c=transmitter->freedv_text_data[transmitter->freedv_text_index++];
  if(c==0) {
    c=0x0D;
    transmitter->freedv_text_index=0;
  }
//fprintf(stderr,"freedv: my_get_next_tx_char=%c\n",c);
  //text_data(c);
  return c;
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

void init_freedv(RECEIVER *rx) {
  int i, success;
fprintf(stderr,"init_freedv\n");

  modem=freedv_open(freedv_mode);
  if(modem==NULL) {
    fprintf(stderr,"freedv_open: modem is null\n");
    return;
  }

  freedv_set_snr_squelch_thresh(modem, (float)freedv_snr_sq_threshold);
  freedv_set_squelch_en(modem, freedv_sq_enable);

  n_speech_samples = freedv_get_n_speech_samples(modem);
  n_max_modem_samples = freedv_get_n_max_modem_samples(modem);
  n_nom_modem_samples = freedv_get_n_nom_modem_samples(modem);
fprintf(stderr,"n_speech_samples=%d n_max_modem_samples=%d n_nom_modem_samples=%d\n",n_speech_samples, n_max_modem_samples,n_nom_modem_samples);
  speech_out = (short*)malloc(sizeof(short)*n_speech_samples);
  demod_in = (short*)malloc(sizeof(short)*n_max_modem_samples);
  speech_in = (short*)malloc(sizeof(short)*n_speech_samples);
  mod_out = (short*)malloc(sizeof(short)*n_nom_modem_samples);
  //spectrum = (float*)malloc(sizeof(float)*n_nom_modem_samples);

  for(i=0;i<63;i++) {
    rx->freedv_text_data[i]=' ';
  }
  rx->freedv_text_data[63]=0;

  nin = freedv_nin(modem);
  rx_samples_in=0;
  tx_samples_in=0;
  freedv_set_callback_txt(modem, &my_put_next_rx_char, &my_get_next_tx_char, NULL);
  //freedv_set_callback_protocol(modem, &my_put_next_rx_proto, NULL, NULL);
  //freedv_set_callback_data(modem, my_datarx, my_datatx, NULL);

  freedv_rate=freedv_get_modem_sample_rate(modem);
  freedv_resample=48000/freedv_rate;
fprintf(stderr,"freedv modem sample nin=%d rate=%d resample=%d\n",nin, freedv_rate, freedv_resample);

  SetRXAPanelGain1 (rx->id, freedv_audio_gain);
/*
  XCreateAnalyzer(CHANNEL_AUDIO, &success, 262144, 1, 1, "");
  if (success != 0) {
    fprintf(stderr, "XCreateAnalyzer CHANNEL_AUDIO failed: %d\n" ,success);
  }
  initAnalyzer(rx,CHANNEL_AUDIO,n_nom_modem_samples,200);

  audio_waterfall_setup(freedv_rate,1500);
*/

  initialized=1;
}

void close_freedv(RECEIVER *rx) {
fprintf(stderr,"freedv_close rx=%p\n",rx);
  initialized=0;
  if(modem!=NULL) {
    freedv_close(modem);
  } else {
    fprintf(stderr,"freedv_close: modem is null\n");
  }
  modem=NULL;
/*
  DestroyAnalyzer(CHANNEL_AUDIO);
*/
  SetRXAPanelGain1 (rx->id, rx->volume);
}

int demod_sample_freedv(short sample) {
  int nout=0;
  if(initialized) {
    demod_in[rx_samples_in]=sample;
    rx_samples_in++;
    if(rx_samples_in==nin) {
      nout=freedv_rx(modem,speech_out,demod_in);
      nin=freedv_nin(modem);
      rx_samples_in=0;
      freedv_get_modem_stats(modem, &freedv_sync, &freedv_snr);
    } 
#ifdef AUDIO_WATERFALL
    if(audio_samples!=NULL) {
      audio_samples[audio_samples_index]=(float)sample;
      audio_samples_index++;
      if(audio_samples_index>=AUDIO_WATERFALL_SAMPLES) {
        Spectrum(CHANNEL_AUDIO,0,0,audio_samples,audio_samples);
        audio_samples_index=0;
      }
    }
#endif
  }
  return nout;
}

int mod_sample_freedv(short sample) {
  int i;
  int nout=0;
  if(initialized) {
    speech_in[tx_samples_in]=sample;
    tx_samples_in++;
    if(tx_samples_in==n_speech_samples) {
      freedv_tx(modem,mod_out,speech_in);
      tx_samples_in=0;
      nout=n_nom_modem_samples;
#ifdef AUDIO_WATERFALL
      for(i=0;i<n_nom_modem_samples;i++) {
        audio_samples[audio_samples_index]=(float)mod_out[i];
        audio_samples_index++;
        if(audio_samples_index>=AUDIO_WATERFALL_SAMPLES) {
          Spectrum(CHANNEL_AUDIO,0,0,audio_samples,audio_samples);
          audio_samples_index=0;
        }
      }
#endif
    } 
  }
  return nout;
}

void freedv_reset_tx_text_index() {
  transmitter->freedv_text_index=0;
}

void freedv_set_sq_enable(int state) {
fprintf(stderr,"freedv_set_sq_enable: state=%d modem=%p\n", state,modem);
  freedv_sq_enable=state;
  if(modem!=NULL) {
    freedv_set_squelch_en(modem, freedv_sq_enable);
  }
}

void freedv_set_sq_threshold(double threshold) {
  freedv_snr_sq_threshold=threshold;
  if(modem!=NULL) {
    freedv_set_snr_squelch_thresh(modem, freedv_snr_sq_threshold);
  }
}

void freedv_set_audio_gain(double gain) {
  freedv_audio_gain=gain;
  if(modem!=NULL) {
    SetRXAPanelGain1 (active_receiver->id, freedv_audio_gain);
  }
}
