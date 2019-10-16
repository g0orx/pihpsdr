/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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

//
// If PortAudio is used instead of ALSA (e.g. on MacOS),
// this file is not used (and replaced by portaudio.c).

#ifndef PORTAUDIO 

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <semaphore.h>

#include <alsa/asoundlib.h>

#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "audio.h"
#include "mode.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif

int audio = 0;
long audio_buffer_size = 256; // samples (both left and right)
int mic_buffer_size = 720; // samples (both left and right)

//static snd_pcm_t *playback_handle=NULL;
static snd_pcm_t *record_handle=NULL;

// each buffer contains 63 samples of left and right audio at 16 bits
#define AUDIO_SAMPLES 63
#define AUDIO_SAMPLE_SIZE 2
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFERS 10
#define OUTPUT_BUFFER_SIZE (AUDIO_SAMPLE_SIZE*AUDIO_CHANNELS*audio_buffer_size)

#define MIC_BUFFER_SIZE (AUDIO_SAMPLE_SIZE*AUDIO_CHANNELS*mic_buffer_size)

//static unsigned char *audio_buffer=NULL;
//static int audio_offset=0;

static unsigned char *mic_buffer=NULL;

static GThread *mic_read_thread_id;

static int running=FALSE;

static void *mic_read_thread(void *arg);

char *input_devices[32];
int n_input_devices=0;
//int n_selected_input_device=-1;

char *output_devices[32];
int n_output_devices=0;
//int n_selected_output_device=-1;

int audio_open_output(RECEIVER *rx) {
  int err;
  snd_pcm_hw_params_t *hw_params;
  int rate=48000;
  int dir=0;


fprintf(stderr,"audio_open_output: rx=%d audio_device=%d\n",rx->id,rx->audio_device);
  if(rx->audio_device<0 || rx->audio_device>=n_output_devices) {
    rx->audio_device=-1;
    return -1;
  }

  int i;
  char hw[64];
  char *selected=output_devices[rx->audio_device];
 
fprintf(stderr,"audio_open_output: selected=%d:%s\n",rx->audio_device,selected);

  i=0;
  while(selected[i]!=' ') {
    hw[i]=selected[i];
    i++;
  }
  hw[i]='\0';
  
  if ((err = snd_pcm_open (&rx->playback_handle, hw, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf (stderr, "audio_open_output: cannot open audio device %s (%s)\n", 
            hw,
            snd_strerror (err));
    return -1;
  }

fprintf(stderr,"audio_open_output: handle=%p\n",rx->playback_handle);

  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot allocate hardware parameter structure (%s)\n",
            snd_strerror (err));
    audio_close_output(rx);
    return -1;
  }

  if ((err = snd_pcm_hw_params_any (rx->playback_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot initialize hardware parameter structure (%s)\n",
            snd_strerror (err));
    audio_close_output(rx);
    return -1;
  }

  if ((err = snd_pcm_hw_params_set_access (rx->playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set access type (%s)\n",
            snd_strerror (err));
    audio_close_output(rx);
    return -1;
}
	
  if ((err = snd_pcm_hw_params_set_format (rx->playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set sample format (%s)\n",
            snd_strerror (err));
    audio_close_output(rx);
    return -1;
  }
	

  if ((err = snd_pcm_hw_params_set_rate_near (rx->playback_handle, hw_params, &rate, &dir)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set sample rate (%s)\n",
            snd_strerror (err));
    audio_close_output(rx);
    return -1;
  }
	
  if ((err = snd_pcm_hw_params_set_channels (rx->playback_handle, hw_params, 2)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set channel count (%s)\n",
            snd_strerror (err));
    audio_close_output(rx);
    return -1;
  }
	
  if ((err = snd_pcm_hw_params (rx->playback_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set parameters (%s)\n",
            snd_strerror (err));
    audio_close_output(rx);
    return -1;
  }
	
  snd_pcm_hw_params_free (hw_params);

  rx->playback_offset=0;
  rx->playback_buffer=(unsigned char *)malloc(OUTPUT_BUFFER_SIZE);
  
  fprintf(stderr,"audio_open_output: rx=%d audio_device=%d handle=%p buffer=%p\n",rx->id,rx->audio_device,rx->playback_handle,rx->playback_buffer);
  return 0;
}
	
int audio_open_input() {
  int err;
  snd_pcm_hw_params_t *hw_params;
  int rate=48000;
  int dir=0;

fprintf(stderr,"audio_open_input: %d\n",transmitter->input_device);
  if(transmitter->input_device<0 || transmitter->input_device>=n_input_devices) {
    transmitter->input_device=0;
    return -1;
  }

  int i;
  char hw[64];
  char *selected=input_devices[transmitter->input_device];
  fprintf(stderr,"audio_open_input: selected=%d:%s\n",transmitter->input_device,selected);
  
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      mic_buffer_size = 720;
      break;
    case NEW_PROTOCOL:
      mic_buffer_size = 64;
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      mic_buffer_size = 720;
      break;
#endif
    default:
      break;
  }
  
  fprintf(stderr,"audio_open_input: mic_buffer_size=%d\n",mic_buffer_size);
  i=0;
  while(selected[i]!=' ') {
    hw[i]=selected[i];
    i++;
  }
  hw[i]='\0';

  fprintf(stderr,"audio_open_input: hw=%s\n",hw);

  if ((err = snd_pcm_open (&record_handle, hw, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
    fprintf (stderr, "audio_open_input: cannot open audio device %s (%s)\n",
            hw,
            snd_strerror (err));
    return -1;
  }

  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
    fprintf (stderr, "audio_open_input: cannot allocate hardware parameter structure (%s)\n",
            snd_strerror (err));
    audio_close_input();
    return -1;
  }

  if ((err = snd_pcm_hw_params_any (record_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_input: cannot initialize hardware parameter structure (%s)\n",
            snd_strerror (err));
    audio_close_input();
    return -1;
  }

  if ((err = snd_pcm_hw_params_set_access (record_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set access type (%s)\n",
            snd_strerror (err));
    audio_close_input();
    return -1;
}

  if ((err = snd_pcm_hw_params_set_format (record_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set sample format (%s)\n",
            snd_strerror (err));
    audio_close_input();
    return -1;
  }

  if ((err = snd_pcm_hw_params_set_rate_near (record_handle, hw_params, &rate, &dir)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set sample rate (%s)\n",
            snd_strerror (err));
    audio_close_input();
    return -1;
  }

  if ((err = snd_pcm_hw_params_set_channels (record_handle, hw_params, 1)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set channel count (%s)\n",
            snd_strerror (err));
    audio_close_input();
    return -1;
  }

  if ((err = snd_pcm_hw_params (record_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set parameters (%s)\n",
            snd_strerror (err));
    audio_close_input();
    return -1;
  }

  snd_pcm_hw_params_free (hw_params);

  mic_buffer=(unsigned char *)malloc(MIC_BUFFER_SIZE);

  running=TRUE;
  mic_read_thread_id = g_thread_new( "local mic", mic_read_thread, NULL);
  if(!mic_read_thread_id )
  {
    fprintf(stderr,"g_thread_new failed on mic_read_thread\n");
  }


  return 0;
}

void audio_close_output(RECEIVER *rx) {
fprintf(stderr,"audio_close_output: rx=%d handle=%p buffer=%p\n",rx->id,rx->playback_handle,rx->playback_buffer);
  if(rx->playback_handle!=NULL) {
    snd_pcm_close (rx->playback_handle);
    rx->playback_handle=NULL;
  }
  if(rx->playback_buffer!=NULL) {
    free(rx->playback_buffer);
    rx->playback_buffer=NULL;
  }
}

void audio_close_input() {
  running=FALSE;
  if(record_handle!=NULL) {
    snd_pcm_close (record_handle);
    record_handle=NULL;
  }
  if(mic_buffer!=NULL) {
    free(mic_buffer);
    mic_buffer=NULL;
  }
}

//
// This is for writing a CW side tone. It is essentially
// a copy of audio_write using the active receiver.
// Note that audio_write must be switched off for the
// active_receiver when transmitting.
//
int cw_audio_write(double sample){
  snd_pcm_sframes_t delay;
  long rc;
  long trim;
  short shortsample;
	
  RECEIVER *rx = active_receiver;
 
  if(rx->playback_handle!=NULL && rx->playback_buffer!=NULL) {
    shortsample = (short) (sample * 32767.0);
    rx->playback_buffer[rx->playback_offset++]=shortsample;
    rx->playback_buffer[rx->playback_offset++]=shortsample>>8;
    rx->playback_buffer[rx->playback_offset++]=shortsample;
    rx->playback_buffer[rx->playback_offset++]=shortsample>>8;

    if(rx->playback_offset==OUTPUT_BUFFER_SIZE) {

      trim=0;

/*
      if(snd_pcm_delay(rx->playback_handle,&delay)==0) {
        if(delay>2048) {
          trim=delay-2048;
//fprintf(stderr,"audio delay=%ld trim=%ld\n",delay,trim);
        }
      }
*/
      if ((rc = snd_pcm_writei (rx->playback_handle, rx->playback_buffer, audio_buffer_size-trim)) != audio_buffer_size-trim) {
        if(rc<0) {
          if(rc==-EPIPE) {
            if ((rc = snd_pcm_prepare (rx->playback_handle)) < 0) {
              fprintf (stderr, "audio_write: cannot prepare audio interface for use %d (%s)\n", rc, snd_strerror (rc));
              return -1;
            } else {
              // ignore short write
            }
          }
        }
      }
      rx->playback_offset=0;
    }
  }
  return 0;
}

//
// if rx == active_receiver and while transmitting, DO NOTHING
// since cw_audio_write may be active
//
int audio_write(RECEIVER *rx,short left_sample,short right_sample) {
  snd_pcm_sframes_t delay;
  long rc;
  long trim;
  int mode=modeUSB;
  if(can_transmit) {
    mode=transmitter->mode;
  }
  //
  // We have to stop the stream here if a CW side tone may occur.
  // This might cause underflows, but we cannot use audio_write
  // and cw_audio_write simultaneously on the same device.
  // Instead, the side tone version will take over.
  // If *not* doing CW, the stream continues because we might wish
  // to listen to this rx while transmitting.
  //

  if (rx == active_receiver && isTransmitting() && (mode==modeCWU || mode==modeCWL)) {
    fprintf(stderr,"returning from audio_write\n");
    rx->playback_offset=0;
    return 0;
  }

  if(rx->playback_handle!=NULL && rx->playback_buffer!=NULL) {
    rx->playback_buffer[rx->playback_offset++]=right_sample;
    rx->playback_buffer[rx->playback_offset++]=right_sample>>8;
    rx->playback_buffer[rx->playback_offset++]=left_sample;
    rx->playback_buffer[rx->playback_offset++]=left_sample>>8;

    if(rx->playback_offset==OUTPUT_BUFFER_SIZE) {

      trim=0;

/*
      if(snd_pcm_delay(rx->playback_handle,&delay)==0) {
        if(delay>2048) {
          trim=delay-2048;
fprintf(stderr,"audio delay=%ld trim=%ld audio_buffer_size=%d\n",delay,trim,audio_buffer_size);
          if(trim>=audio_buffer_size) {
            rx->playback_offset=0;
            return 0;
          }
        }
      }
*/
      if ((rc = snd_pcm_writei (rx->playback_handle, rx->playback_buffer, audio_buffer_size-trim)) != audio_buffer_size-trim) {
        if(rc<0) {
          if(rc==-EPIPE) {
            if ((rc = snd_pcm_prepare (rx->playback_handle)) < 0) {
              fprintf (stderr, "audio_write: cannot prepare audio interface for use %d (%s)\n", rc, snd_strerror (rc));
              rx->playback_offset=0;
              return -1;
            }
          } else {
            // ignore short write
          }
        }
      }
      rx->playback_offset=0;
    }
  }
  return 0;
}

static void *mic_read_thread(gpointer arg) {
  int rc;
  if ((rc = snd_pcm_prepare (record_handle)) < 0) {
    fprintf (stderr, "mic_read_thread: cannot prepare audio interface for use (%s)\n",
            snd_strerror (rc));
    return NULL;
  }
fprintf(stderr,"mic_read_thread: mic_buffer_size=%d\n",mic_buffer_size);
  while(running) {
    if ((rc = snd_pcm_readi (record_handle, mic_buffer, mic_buffer_size)) != mic_buffer_size) {
      if(running) {
        if(rc<0) {
          fprintf (stderr, "mic_read_thread: read from audio interface failed (%s)\n",
                  snd_strerror (rc));
          running=FALSE;
        } else {
          fprintf(stderr,"mic_read_thread: read %d\n",rc);
        }
      }
    } else {
      // process the mic input
      switch(protocol) {
        case ORIGINAL_PROTOCOL:
          old_protocol_process_local_mic(mic_buffer,1);
          break;
        case NEW_PROTOCOL:
          new_protocol_process_local_mic(mic_buffer,1);
          break;
#ifdef SOAPYSDR
        case SOAPYSDR_PROTOCOL:
          soapy_protocol_process_local_mic(mic_buffer,1);
          break;
#endif
        default:
          break;
      }
    }
  }
fprintf(stderr,"mic_read_thread: exiting\n");
  return NULL;
}

void audio_get_cards() {
  snd_ctl_card_info_t *info;
  snd_pcm_info_t *pcminfo;
  snd_ctl_card_info_alloca(&info);
  snd_pcm_info_alloca(&pcminfo);
  int i;
  char *device_id;
  int card = -1;

fprintf(stderr,"audio_get_cards\n");

  for(i=0;i<n_input_devices;i++) {
    free(input_devices[i]);
  }
  n_input_devices=0;

  for(i=0;i<n_output_devices;i++) {
    free(output_devices[i]);
  }
  n_output_devices=0;

  while (snd_card_next(&card) >= 0 && card >= 0) {
    int err = 0;
    snd_ctl_t *handle;
    char name[20];
    snprintf(name, sizeof(name), "hw:%d", card);
    if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
      continue;
    }

    if ((err = snd_ctl_card_info(handle, info)) < 0) {
      snd_ctl_close(handle);
      continue;
    }

    int dev = -1;

    while (snd_ctl_pcm_next_device(handle, &dev) >= 0 && dev >= 0) {
      snd_pcm_info_set_device(pcminfo, dev);
      snd_pcm_info_set_subdevice(pcminfo, 0);

      // input devices
      snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_CAPTURE);
      if ((err = snd_ctl_pcm_info(handle, pcminfo)) == 0) {
        device_id=malloc(64);
        snprintf(device_id, 64, "plughw:%d,%d %s", card, dev, snd_ctl_card_info_get_name(info));
        input_devices[n_input_devices++]=device_id;
fprintf(stderr,"input_device: %s\n",device_id);
      }

      // ouput devices
      snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);
      if ((err = snd_ctl_pcm_info(handle, pcminfo)) == 0) {
        device_id=malloc(64);
        snprintf(device_id, 64, "plughw:%d,%d %s", card, dev, snd_ctl_card_info_get_name(info));
        output_devices[n_output_devices++]=device_id;
fprintf(stderr,"output_device: %s\n",device_id);
      }
    }

    snd_ctl_close(handle);
   
  }

  // look for dmix
  void **hints, **n;
  char *name, *descr, *io;

  if (snd_device_name_hint(-1, "pcm", &hints) < 0)
    return;
  n = hints;
  while (*n != NULL) {
    name = snd_device_name_get_hint(*n, "NAME");
    descr = snd_device_name_get_hint(*n, "DESC");
    io = snd_device_name_get_hint(*n, "IOID");
    
    if(strncmp("dmix:", name, 5)==0/* || strncmp("pulse", name, 5)==0*/) {
      fprintf(stderr,"name=%s descr=%s io=%s\n",name, descr, io);
      device_id=malloc(64);
      
      snprintf(device_id, 64, "%s", name);
      output_devices[n_output_devices++]=device_id;
fprintf(stderr,"output_device: %s\n",device_id);
    }

    if (name != NULL)
      free(name);
    if (descr != NULL)
      free(descr);
    if (io != NULL)
      free(io);
    n++;
  }
  snd_device_name_free_hint(hints);
}
#endif
