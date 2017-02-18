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

#include <soundio/soundio.h>

#include "new_protocol.h"
#include "old_protocol.h"
#include "radio.h"
#include "receiver.h"
#include "audio.h"

int audio = 0;
int audio_buffer_size = 256; // samples (both left and right)
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

char *input_devices[16];
int n_input_devices=0;
int n_selected_input_device=-1;

char *output_devices[16];
int n_output_devices=0;
//int n_selected_output_device=-1;

static double seconds_offset=0.0;
static int want_pause=0;

#define BUFFER_SIZE 8192

static struct SoundIo *soundio;
static struct SoundIoDevice *audio_device;
static struct SoundIoOutStream *outstream;

static short output_left_buffer[BUFFER_SIZE];
static short output_right_buffer[BUFFER_SIZE];
static int insert_index=0;
static int remove_index=0;
static int frames=0;

static void write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max) {
//fprintf(stderr,"write_callback: min=%d max=%d frames=%d insert_index=%d remove_index=%d\n",frame_count_min, frame_count_max, frames, insert_index, remove_index);
  double float_sample_rate = outstream->sample_rate;
  double seconds_per_frame = 1.0 / float_sample_rate;
  struct SoundIoChannelArea *areas;
  int err;
  int frame;
  int channel;

  if(frames!=0) {
    int frames_left = frames;
    if(frames>frame_count_max) {
      frames_left=frame_count_max;
    }
    if ((err = soundio_outstream_begin_write(outstream, &areas, &frames_left))) {
      fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
      exit(1);
    }

    const struct SoundIoChannelLayout *layout = &outstream->layout;

    for (frame=0; frame < frames_left; frame += 1) {
      for (channel = 0; channel < layout->channel_count; channel += 1) {
        int16_t *s=(int16_t *)areas[channel].ptr;
        if(channel==0) {
          *s=output_left_buffer[remove_index];
        } else {
          *s=output_right_buffer[remove_index];
        }
        areas[channel].ptr += areas[channel].step;
      } 
      frames--;
      remove_index++;
      if(remove_index==BUFFER_SIZE) {
        remove_index=0;
      }
    }
    //seconds_offset += seconds_per_frame * frame_count;

    if ((err = soundio_outstream_end_write(outstream))) {
      if (err == SoundIoErrorUnderflow)
         return;
      fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
      exit(1);
    }
  } else {
    //fprintf(stderr,"audio.c: write_callback: underflow: frames=%d insert_index=%d remove_index=%d\n",frames,insert_index,remove_index);
  }
}

static void underflow_callback(struct SoundIoOutStream *outstream) {
    static int count = 0;
    //fprintf(stderr, "underflow %d\n", count++);
}

int audio_open_output(RECEIVER *rx) {

  int err;

fprintf(stderr,"audio_open_output: id=%d device=%d\n", rx->id,rx->audio_device);
  soundio = soundio_create();
  if (!soundio) {
    fprintf(stderr, "audio_open_output: soundio_create failed\n");
    return -1;
  }

  soundio_connect(soundio);

  soundio_flush_events(soundio);

  audio_device = soundio_get_output_device(soundio, rx->audio_device);
  if(!audio_device) {
    fprintf(stderr, "audio_open_output: soundio_get_output_device failed\n");
    return -1;
  }

  if (audio_device->probe_error) {
    fprintf(stderr, "audio_open_output: Cannot probe audio_device: %s\n", soundio_strerror(audio_device->probe_error));
    return -1;
  }

  outstream = soundio_outstream_create(audio_device);
  outstream->write_callback = write_callback;
  outstream->underflow_callback = underflow_callback;
  outstream->name = "pihpsdr:out";
  outstream->software_latency = 0.0;
  outstream->sample_rate = 48000;

  if (soundio_device_supports_format(audio_device, SoundIoFormatS16LE)) {
    outstream->format = SoundIoFormatS16LE;
  } else {
    fprintf(stderr,"audio_open_output: audio_device does not support S16LE\n");
    return -1;
  }

  if ((err = soundio_outstream_open(outstream))) {
    fprintf(stderr, "audio_open_output: unable to open audio_device: %s", soundio_strerror(err));
    return -1;
  }
  fprintf(stderr, "audio_open_output: Software latency: %f\n", outstream->software_latency);

  if (outstream->layout_error) {
    fprintf(stderr, "audio_open_output: unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));
  }

  if ((err = soundio_outstream_start(outstream))) {
    fprintf(stderr, "audio_open_output: unable to start audio_device: %s\n", soundio_strerror(err));
    return -1;
  }

  return 0;
}
	
int audio_open_input() {
/*
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
#ifdef RADIOBERRY
    case RADIOBERRY_PROTOCOL:
      mic_buffer_size = 1024;
      break;
#endif
    case ORIGINAL_PROTOCOL:
      mic_buffer_size = 720;
      break;
    case NEW_PROTOCOL:
      mic_buffer_size = 64;
      break;
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
  mic_read_thread_id = g_thread_new( "local mic", mic_read_thread, NULL);
  if(!mic_read_thread_id )
  {
    fprintf(stderr,"g_thread_new failed on mic_read_thread\n");
  }

*/
  return 0;
}

void audio_close_output(RECEIVER *rx) {
  soundio_outstream_destroy(outstream);
  soundio_device_unref(audio_device);
  soundio_destroy(soundio);
/*
  if(rx->playback_handle!=NULL) {
    snd_pcm_close (rx->playback_handle);
    rx->playback_handle=NULL;
  }
  if(rx->playback_buffer!=NULL) {
    free(rx->playback_buffer);
    rx->playback_buffer=NULL;
  }
*/
}

void audio_close_input() {
/*
  running=FALSE;
  if(record_handle!=NULL) {
    snd_pcm_close (record_handle);
    record_handle=NULL;
  }
  if(mic_buffer!=NULL) {
    free(mic_buffer);
    mic_buffer=NULL;
  }
*/
}

int audio_write(RECEIVER *rx,short left_sample,short right_sample) {
//fprintf(stderr,"audio_write: id=%d frames=%d insert_index=%d remove_index=%d\n",rx->id, frames, insert_index,remove_index);
  if(frames<(BUFFER_SIZE-2)) {
    output_left_buffer[insert_index]=left_sample;
    output_right_buffer[insert_index]=right_sample;
    insert_index++;
    if(insert_index==BUFFER_SIZE) {
      insert_index=0;
    }
    frames++;
  } else {
    fprintf(stderr,"audio_write: buffer_full: frames=%d insert_index-%d remove_index=%d\n",frames,insert_index,remove_index);
  }

/*
  snd_pcm_sframes_t delay;
  int error;
  long trim;

  if(rx->playback_handle!=NULL && rx->playback_buffer!=NULL) {
    rx->playback_buffer[rx->playback_offset++]=right_sample;
    rx->playback_buffer[rx->playback_offset++]=right_sample>>8;
    rx->playback_buffer[rx->playback_offset++]=left_sample;
    rx->playback_buffer[rx->playback_offset++]=left_sample>>8;

    if(rx->playback_offset==OUTPUT_BUFFER_SIZE) {

      trim=0;

      if(snd_pcm_delay(rx->playback_handle,&delay)==0) {
        if(delay>2048) {
          trim=delay-2048;
//fprintf(stderr,"audio delay=%ld trim=%ld\n",delay,trim);
        }
      }

      if ((error = snd_pcm_writei (rx->playback_handle, rx->playback_buffer, audio_buffer_size-trim)) != audio_buffer_size-trim) {
        if(error==-EPIPE) {
          if ((error = snd_pcm_prepare (rx->playback_handle)) < 0) {
            fprintf (stderr, "audio_write: cannot prepare audio interface for use (%s)\n",
                    snd_strerror (error));
            return -1;
          }
          if ((error = snd_pcm_writei (rx->playback_handle, rx->playback_buffer, audio_buffer_size-trim)) != audio_buffer_size) {
            fprintf (stderr, "audio_write: write to audio interface failed (%s)\n",
                    snd_strerror (error));
            return -1;
          }
        }
      }
      rx->playback_offset=0;
    }
  }
*/
  return 0;
}

static gpointer mic_read_thread(gpointer arg) {
/*
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
#ifdef RADIOBERRY
		case RADIOBERRY_PROTOCOL:
		  radioberry_protocol_iq_samples(mic_buffer,1);
		  break;
#endif
        case ORIGINAL_PROTOCOL:
          old_protocol_process_local_mic(mic_buffer,1);
          break;
        case NEW_PROTOCOL:
          new_protocol_process_local_mic(mic_buffer,1);
          break;
        default:
          break;
      }
    }
  }
fprintf(stderr,"mic_read_thread: exiting\n");
*/
}

void audio_get_cards() {
  int i;

  for(i=0;i<n_input_devices;i++) {
      free(input_devices[i]);
  }
  for(i=0;i<n_output_devices;i++) {
    free(output_devices[i]);
  }

  struct SoundIo *soundio = soundio_create();
  if (!soundio) {
    fprintf(stderr, "audio_get_cards: soundio_create failed\n");
    return;
  }

  soundio_connect(soundio);

  soundio_flush_events(soundio);

  n_output_devices = soundio_output_device_count(soundio);
  n_input_devices = soundio_input_device_count(soundio);
  int default_output = soundio_default_output_device_index(soundio);
  int default_input = soundio_default_input_device_index(soundio);


  for(i=0;i<n_output_devices;i++) {
    struct SoundIoDevice *device = soundio_get_output_device(soundio, i);
fprintf(stderr,"output: %d: id=%s name=%s\n",i,device->id,device->name);
    char *device_id=malloc(64);
    strncpy(device_id,device->id,64);
    output_devices[i]=device_id;
    soundio_device_unref(device);
  }

  for(i=0;i<n_input_devices;i++) {
    struct SoundIoDevice *device = soundio_get_input_device(soundio, i);
fprintf(stderr,"input: %d: id=%s name=%s\n",i,device->id,device->name);
    char *device_id=malloc(64);
    strncpy(device_id,device->id,64);
    input_devices[i]=device_id;
    soundio_device_unref(device);
  }

  soundio_destroy(soundio);
}
