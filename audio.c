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


#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>

#include <alsa/asoundlib.h>

#include "audio.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef RADIOBERRY
#include "radioberry.h"
#endif
#include "radio.h"

int audio = 0;
int audio_buffer_size = 256; // samples (both left and right)
int mic_buffer_size = 720; // samples (both left and right)

static snd_pcm_t *playback_handle=NULL;
static snd_pcm_t *record_handle=NULL;

// each buffer contains 63 samples of left and right audio at 16 bits
#define AUDIO_SAMPLES 63
#define AUDIO_SAMPLE_SIZE 2
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFERS 10
#define AUDIO_BUFFER_SIZE (AUDIO_SAMPLE_SIZE*AUDIO_CHANNELS*audio_buffer_size)

#define MIC_BUFFER_SIZE (AUDIO_SAMPLE_SIZE*AUDIO_CHANNELS*mic_buffer_size)

static unsigned char *audio_buffer=NULL;
static int audio_offset=0;

static unsigned char *mic_buffer=NULL;

static pthread_t mic_read_thread_id;

static int running=FALSE;

static void *mic_read_thread(void *arg);

char *input_devices[16];
int n_input_devices=0;
int n_selected_input_device=-1;

char *output_devices[16];
int n_output_devices=0;
int n_selected_output_device=-1;

int audio_open_output() {
  int err;
  snd_pcm_hw_params_t *hw_params;
  int rate=48000;
  int dir=0;


  if(n_selected_output_device<0 || n_selected_output_device>=n_output_devices) {
    n_selected_output_device=-1;
    return -1;
  }

  int i;
  char hw[16];
  char *selected=output_devices[n_selected_output_device];
 
fprintf(stderr,"audio_open_output: selected=%d:%s\n",n_selected_output_device,selected);

  i=0;
  while(selected[i]!=' ') {
    hw[i]=selected[i];
    i++;
  }
  hw[i]='\0';
  
  if ((err = snd_pcm_open (&playback_handle, hw, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf (stderr, "audio_open_output: cannot open audio device %s (%s)\n", 
            hw,
            snd_strerror (err));
    return -1;
  }


  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot allocate hardware parameter structure (%s)\n",
            snd_strerror (err));
    return -1;
  }

  if ((err = snd_pcm_hw_params_any (playback_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot initialize hardware parameter structure (%s)\n",
            snd_strerror (err));
    return -1;
  }

  if ((err = snd_pcm_hw_params_set_access (playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set access type (%s)\n",
            snd_strerror (err));
    return -1;
}
	
  if ((err = snd_pcm_hw_params_set_format (playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set sample format (%s)\n",
            snd_strerror (err));
    return -1;
  }
	

  if ((err = snd_pcm_hw_params_set_rate_near (playback_handle, hw_params, &rate, &dir)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set sample rate (%s)\n",
            snd_strerror (err));
    return -1;
  }
	
  if ((err = snd_pcm_hw_params_set_channels (playback_handle, hw_params, 2)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set channel count (%s)\n",
            snd_strerror (err));
    return -1;
  }
	
  if ((err = snd_pcm_hw_params (playback_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set parameters (%s)\n",
            snd_strerror (err));
    return -1;
  }
	
  snd_pcm_hw_params_free (hw_params);

  audio_offset=0;
  audio_buffer=(unsigned char *)malloc(AUDIO_BUFFER_SIZE);

  return 0;
}
	
int audio_open_input() {
  int err;
  snd_pcm_hw_params_t *hw_params;
  int rate=48000;
  int dir=0;

fprintf(stderr,"audio_open_input: %d\n",n_selected_input_device);
  if(n_selected_input_device<0 || n_selected_input_device>=n_input_devices) {
    n_selected_input_device=-1;
    return -1;
  }

  int i;
  char hw[16];
  char *selected=input_devices[n_selected_input_device];
  fprintf(stderr,"audio_open_input: selected=%d:%s\n",n_selected_input_device,selected);
  
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      mic_buffer_size = 720;
      break;
    case NEW_PROTOCOL:
      mic_buffer_size = 64;
      break;
#ifdef RADIOBERRY
	case RADIOBERRY_PROTOCOL:
		mic_buffer_size = 1024;
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
    return -1;
  }

  if ((err = snd_pcm_hw_params_any (record_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_input: cannot initialize hardware parameter structure (%s)\n",
            snd_strerror (err));
    return -1;
  }

  if ((err = snd_pcm_hw_params_set_access (record_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set access type (%s)\n",
            snd_strerror (err));
    return -1;
}

  if ((err = snd_pcm_hw_params_set_format (record_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set sample format (%s)\n",
            snd_strerror (err));
    return -1;
  }

  if ((err = snd_pcm_hw_params_set_rate_near (record_handle, hw_params, &rate, &dir)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set sample rate (%s)\n",
            snd_strerror (err));
    return -1;
  }

  if ((err = snd_pcm_hw_params_set_channels (record_handle, hw_params, 1)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set channel count (%s)\n",
            snd_strerror (err));
    return -1;
  }

  if ((err = snd_pcm_hw_params (record_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_input: cannot set parameters (%s)\n",
            snd_strerror (err));
    return -1;
  }

  snd_pcm_hw_params_free (hw_params);

  mic_buffer=(unsigned char *)malloc(MIC_BUFFER_SIZE);

  running=TRUE;
  err=pthread_create(&mic_read_thread_id,NULL,mic_read_thread,NULL);
  if(err != 0) {
    fprintf(stderr,"pthread_create failed on mic_read_thread: rc=%d\n", err);
    return -1;
  }

  return 0;
}

void audio_close_output() {
  if(playback_handle!=NULL) {
    snd_pcm_close (playback_handle);
    playback_handle=NULL;
  }
  if(audio_buffer!=NULL) {
    free(audio_buffer);
    audio_buffer=NULL;
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

int audio_write(short left_sample,short right_sample) {
  snd_pcm_sframes_t delay;
  int error;
  long trim;

  if(playback_handle!=NULL && audio_buffer!=NULL) {
    audio_buffer[audio_offset++]=left_sample;
    audio_buffer[audio_offset++]=left_sample>>8;
    audio_buffer[audio_offset++]=right_sample;
    audio_buffer[audio_offset++]=right_sample>>8;

    if(audio_offset==AUDIO_BUFFER_SIZE) {

      trim=0;

      if(snd_pcm_delay(playback_handle,&delay)==0) {
        if(delay>2048) {
          trim=delay-2048;
//fprintf(stderr,"audio delay=%ld trim=%ld\n",delay,trim);
        }
      }

      if ((error = snd_pcm_writei (playback_handle, audio_buffer, audio_buffer_size-trim)) != audio_buffer_size-trim) {
        if(error==-EPIPE) {
          if ((error = snd_pcm_prepare (playback_handle)) < 0) {
            fprintf (stderr, "audio_write: cannot prepare audio interface for use (%s)\n",
                    snd_strerror (error));
            return -1;
          }
          if ((error = snd_pcm_writei (playback_handle, audio_buffer, audio_buffer_size)) != audio_buffer_size) {
            fprintf (stderr, "audio_write: write to audio interface failed (%s)\n",
                    snd_strerror (error));
            return -1;
          }
        }
      }
      audio_offset=0;
    }
  }
  return 0;
}

static void *mic_read_thread(void *arg) {
  int rc;
  if ((rc = snd_pcm_prepare (record_handle)) < 0) {
    fprintf (stderr, "mic_read_thread: cannot prepare audio interface for use (%s)\n",
            snd_strerror (rc));
    return;
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
#ifdef RADIOBERRY
		case RADIOBERRY_PROTOCOL:
			radioberry_protocol_process_local_mic(mic_buffer,1);
			break;
#endif
        default:
          break;
      }
    }
  }
fprintf(stderr,"mic_read_thread: exiting\n");

}

void audio_get_cards(int input) {
  snd_ctl_card_info_t *info;
  snd_pcm_info_t *pcminfo;
  snd_ctl_card_info_alloca(&info);
  snd_pcm_info_alloca(&pcminfo);
  int i;

  if(input) {
    for(i=0;i<n_input_devices;i++) {
      free(input_devices[i]);
    }
    n_input_devices=0;
  } else {
    for(i=0;i<n_output_devices;i++) {
      free(output_devices[i]);
    }
    n_output_devices=0;
  }
  int card = -1;
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
      snd_pcm_info_set_stream(pcminfo, input ? SND_PCM_STREAM_CAPTURE
          : SND_PCM_STREAM_PLAYBACK);
      if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
        continue;
      }

      char *device_id=malloc(64);
      
      snprintf(device_id, 64, "plughw:%d,%d %s", card, dev, snd_ctl_card_info_get_name(info));
      if(input) {
fprintf(stderr,"input_device: %s\n",device_id);
        input_devices[n_input_devices++]=device_id;
      } else {
        output_devices[n_output_devices++]=device_id;
fprintf(stderr,"output_device: %s\n",device_id);
      }

    }

    snd_ctl_close(handle);

  }
}
