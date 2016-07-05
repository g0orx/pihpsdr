#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <codec2/freedv_api.h>

#include "radio.h"

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

int freedv_tx_text_index=0;

char freedv_rx_text_data[64];
int freedv_rx_text_data_index=0;

static void my_put_next_rx_char(void *callback_state, char c) {
  int i;
  fprintf(stderr, "freedv: my_put_next_rx_char: %c sync=%d\n", c, freedv_sync);
  if(freedv_sync) {
    if(c==0x0D) {
      c='|';
    }
    for(i=0;i<62;i++) {
      freedv_rx_text_data[i]=freedv_rx_text_data[i+1];
    }
    freedv_rx_text_data[62]=c;
  }
}


static char my_get_next_tx_char(void *callback_state){
  char c=freedv_tx_text_data[freedv_tx_text_index++];
  if(c==0) {
    c=0x0D;
    freedv_tx_text_index=0;
  }
fprintf(stderr,"freedv: my_get_next_tx_char=%c\n",c);
  return c;
}

void init_freedv() {
  int i;
fprintf(stderr,"init_freedv\n");

  modem=freedv_open(FREEDV_MODE_1600);
  if(modem==NULL) {
    fprintf(stderr,"freedv_open: modem is null\n");
    return;
  }

  freedv_set_snr_squelch_thresh(modem, -100.0);
  freedv_set_squelch_en(modem, 1);

  n_speech_samples = freedv_get_n_speech_samples(modem);
  n_max_modem_samples = freedv_get_n_max_modem_samples(modem);
  n_nom_modem_samples = freedv_get_n_nom_modem_samples(modem);
fprintf(stderr,"n_speech_samples=%d n_max_modem_samples=%d\n",n_speech_samples, n_max_modem_samples);
  speech_out = (short*)malloc(sizeof(short)*n_speech_samples);
  demod_in = (short*)malloc(sizeof(short)*n_max_modem_samples);
  speech_in = (short*)malloc(sizeof(short)*n_speech_samples);
  mod_out = (short*)malloc(sizeof(short)*n_nom_modem_samples);

  nin = freedv_nin(modem);
  rx_samples_in=0;
  tx_samples_in=0;
  freedv_set_callback_txt(modem, &my_put_next_rx_char, &my_get_next_tx_char, NULL);
  //freedv_set_callback_protocol(modem, &my_put_next_rx_proto, NULL, NULL);
  //freedv_set_callback_data(modem, my_datarx, my_datatx, NULL);

  int rate=freedv_get_modem_sample_rate(modem);
fprintf(stderr,"freedv modem sample rate=%d\n",rate);

  for(i=0;i<63;i++) {
    freedv_rx_text_data[i]=' ';
  }
  freedv_rx_text_data[63]=0;
}

void close_freedv() {
fprintf(stderr,"freedv_close\n");
  if(modem!=NULL) {
    freedv_close(modem);
  } else {
    fprintf(stderr,"freedv_close: modem is null\n");
  }
}

int demod_sample_freedv(short sample) {
  int nout=0;
  demod_in[rx_samples_in]=sample;
  rx_samples_in++;
  if(rx_samples_in==nin) {
    nout=freedv_rx(modem,speech_out,demod_in);
    nin=freedv_nin(modem);
    rx_samples_in=0;
    freedv_get_modem_stats(modem, &freedv_sync, &freedv_snr);
  } 
  return nout;
}

int mod_sample_freedv(short sample) {
  int nout=0;
  speech_in[tx_samples_in]=sample;
  tx_samples_in++;
  if(tx_samples_in==n_speech_samples) {
    freedv_tx(modem,mod_out,speech_in);
    tx_samples_in=0;
    nout=n_nom_modem_samples;
  } 
  return nout;
}

void freedv_reset_tx_text_index() {
  freedv_tx_text_index=0;
}
