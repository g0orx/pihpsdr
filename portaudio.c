#ifdef PORTAUDIO
//
// Contribution from DL1YCF (Christoph van Wullen)
//
// Alternate "audio" module using PORTAUDIO instead of ALSA
// (e.g. on MacOS)
//
// If PortAudio is NOT used, this file is empty, and audio.c
// is used instead.
//

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

#include "new_protocol.h"
#include "old_protocol.h"
#ifdef RADIOBERRY
#include "radioberry.h"
#endif
#include "radio.h"
#include "receiver.h"
#include "mode.h"
#include "portaudio.h"
#include "math.h"   // for sintab, two-tone generator

static PaStream *record_handle=NULL;

#define MAXDEVICES 12

const char *input_devices[MAXDEVICES];
const char *output_devices[MAXDEVICES];

int n_input_devices=0;
int n_output_devices=0;

static int   in_device_no[MAXDEVICES];
static int   out_device_no[MAXDEVICES];

static unsigned char *mic_buffer=NULL;
static int mic_buffer_size;
static int audio_buffer_size=256;

//
// AUDIO_GET_CARDS
//
// This inits PortAudio and looks for suitable input and output channels
//
void audio_get_cards()
{
  int i, numDevices;
  const PaDeviceInfo *deviceInfo;
  PaStreamParameters inputParameters, outputParameters;

  PaError err;

  err = Pa_Initialize();
  if( err != paNoError )
  {
	fprintf(stderr, "PORTAUDIO ERROR: Pa_Initialize: %s\n", Pa_GetErrorText(err));
	return;
  }
  numDevices = Pa_GetDeviceCount();
  if( numDevices < 0 ) return;

  n_input_devices=0;
  n_output_devices=0;

  for( i=0; i<numDevices; i++ )
  {
	deviceInfo = Pa_GetDeviceInfo( i );

        inputParameters.device = i;
        inputParameters.channelCount = 1;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
        inputParameters.hostApiSpecificStreamInfo = NULL;
        if (Pa_IsFormatSupported(&inputParameters, NULL, 48000.0) == paFormatIsSupported) {
          if (n_input_devices < MAXDEVICES) {
            input_devices[n_input_devices]=deviceInfo->name;
            in_device_no[n_input_devices++] =i;
          }
          fprintf(stderr,"PORTAUDIO INPUT DEVICE, No=%d, Name=%s\n", i, deviceInfo->name);
	}

	outputParameters.device = i;
        outputParameters.channelCount = 1;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
        outputParameters.hostApiSpecificStreamInfo = NULL;
        if (Pa_IsFormatSupported(NULL, &outputParameters, 48000.0) == paFormatIsSupported) {
          if (n_output_devices < MAXDEVICES) {
            output_devices[n_output_devices]=deviceInfo->name;
            out_device_no[n_output_devices++] =i;
          }
          fprintf(stderr,"PORTAUDIO OUTPUT DEVICE, No=%d, Name=%s\n", i, deviceInfo->name);
        }
  }
}

//
// AUDIO_OPEN_INPUT
//
// open a PA stream that connects to the TX microphone
// The PA callback function then sends the data to the transmitter
//

int pa_mic_cb(const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*);
unsigned char *micbuffer = NULL;

int audio_open_input()
{
  PaError err;
  PaStreamParameters inputParameters, outputParameters;
  long framesPerBuffer;
  int i;

  fprintf(stderr,"audio_open_input: %d\n",transmitter->input_device);
  if(transmitter->input_device<0 || transmitter->input_device>=n_input_devices) {
    transmitter->input_device=0;
    return -1;
  }

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      framesPerBuffer = 720;
      break;
    case NEW_PROTOCOL:
      framesPerBuffer = 64;
      break;
#ifdef RADIOBERRY
        case RADIOBERRY_PROTOCOL:
                framesPerBuffer = 1024;
                break;
#endif
    default:
      break;
  }

  bzero( &inputParameters, sizeof( inputParameters ) ); //not necessary if you are filling in all the fields
  inputParameters.channelCount = 1;
  inputParameters.device = in_device_no[transmitter->input_device];
  inputParameters.hostApiSpecificStreamInfo = NULL;
  inputParameters.sampleFormat = paFloat32;
  inputParameters.suggestedLatency = Pa_GetDeviceInfo(in_device_no[transmitter->input_device])->defaultLowInputLatency ;
  inputParameters.hostApiSpecificStreamInfo = NULL; //See you specific host's API docs for info on using this field

  err = Pa_OpenStream(&record_handle, &inputParameters, NULL, 48000.0, framesPerBuffer, paNoFlag, pa_mic_cb, NULL);
  if (err != paNoError) {
    fprintf(stderr, "PORTAUDIO ERROR: AOI open stream: %s\n",Pa_GetErrorText(err));
    return -1;
  }

  err = Pa_StartStream(record_handle);
  if (err != paNoError) {
    fprintf(stderr, "PORTAUDIO ERROR: AOI start stream:%s\n",Pa_GetErrorText(err));
    return -1;
  }
  mic_buffer=(unsigned char *)malloc(2*framesPerBuffer);
  mic_buffer_size=framesPerBuffer;
  return 0;
}

//
// PortAudio call-back function for Audio input
//
int pa_mic_cb(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
             const PaStreamCallbackTimeInfo* timeInfo,
             PaStreamCallbackFlags statusFlags,
             void *userdata)
{
  const float *in = (float *)inputBuffer;
  int i;
  short isample;
  unsigned char *p;

//
// Convert input buffer in paFloat32 into a sequence of 16-bit
// values in the mic buffer.
//
  if (mic_buffer == NULL) return paAbort;

  p=mic_buffer;
  if (in == NULL || framesPerBuffer != mic_buffer_size) {
    for (i=0; i<mic_buffer_size; i++)  {
      *p++ = 0;
      *p++ = 0;
    }
  } else {
    for (i=0; i<framesPerBuffer; i++) {
      isample=(short) (in[i]*32767.0);
      *p++   = (isample & 0xFF);         // LittleEndian
      *p++   = (isample >> 8)& 0xFF;
    }
  }
//
// Call routine to send mic buffer
//
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
  return paContinue;
}

//
// AUDIO_OPEN_OUTPUT
//
// open a PA stream for data from one of the RX
//
int audio_open_output(RECEIVER *rx)
{
  PaError err;
  PaStreamParameters outputParameters;
  long framesPerBuffer=(long) audio_buffer_size;

  int padev = out_device_no[rx->audio_device];
  fprintf(stderr,"audio_open_output: %d PADEV=%d\n",rx->audio_device,padev);
  if(rx->audio_device<0 || rx->audio_device>=n_output_devices) {
    rx->audio_device=-1;
    return -1;
  }
  bzero( &outputParameters, sizeof( outputParameters ) ); //not necessary if you are filling in all the fields
  outputParameters.channelCount = 1;   // Always MONO
  outputParameters.device = padev;
  outputParameters.hostApiSpecificStreamInfo = NULL;
  outputParameters.sampleFormat = paFloat32;
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(padev)->defaultLowOutputLatency ;
  outputParameters.hostApiSpecificStreamInfo = NULL; //See you specific host's API docs for info on using this field

  // Try using AudioWrite without a call-back function

  rx->playback_buffer=malloc(audio_buffer_size*sizeof(float));
  rx->playback_offset=0;
  err = Pa_OpenStream(&(rx->playback_handle), NULL, &outputParameters, 48000.0, framesPerBuffer, paNoFlag, NULL, NULL);
  if (err != paNoError) {
    fprintf(stderr,"PORTAUDIO ERROR: AOO open stream: %s\n",Pa_GetErrorText(err));
    rx->playback_handle = NULL;
    if (rx->playback_buffer) free(rx->playback_buffer);
    rx->playback_buffer = NULL;
    return -1;
  }

  err = Pa_StartStream(rx->playback_handle);
  if (err != paNoError) {
    fprintf(stderr,"PORTAUDIO ERROR: AOO start stream:%s\n",Pa_GetErrorText(err));
    rx->playback_handle=NULL;
    if (rx->playback_buffer) free(rx->playback_buffer);
    rx->playback_buffer = NULL;
    return -1;
  }
  // Write one buffer to avoid under-flow errors
  // (this gives us 5 msec to pass before we have to call audio_write the first time)
  bzero(rx->playback_buffer, (size_t) audio_buffer_size*sizeof(float));
  err=Pa_WriteStream(rx->playback_handle, (void *) rx->playback_buffer, (unsigned long) audio_buffer_size);
  return 0;
}

//
// AUDIO_CLOSE_INPUT
//
// close a TX microphone stream
//
void audio_close_input()
{
  PaError err;

  fprintf(stderr,"AudioCloseInput: %d\n", transmitter->input_device);

  if(record_handle!=NULL) {
    err = Pa_StopStream(record_handle);
    if (err != paNoError) {
      fprintf(stderr,"PORTAUDIO ERROR: ACI stop stream: %s\n",Pa_GetErrorText(err));
    }
    err = Pa_CloseStream(record_handle);
    if (err != paNoError) {
      fprintf(stderr,"PORTAUDIO ERROR: ACI close stream: %s\n",Pa_GetErrorText(err));
    }
    record_handle=NULL;
  }
  if(mic_buffer!=NULL) {
    free(mic_buffer);
    mic_buffer=NULL;
  }
}

//
// AUDIO_CLOSE_OUTPUT
//
// shut down the stream connected with audio from one of the RX
//
void audio_close_output(RECEIVER *rx) {
  PaError err;

  fprintf(stderr,"AudioCloseOutput: %d\n", rx->audio_device);

// free the buffer first, this then indicates to audio_write to do nothing
  if(rx->playback_buffer!=NULL) {
    free(rx->playback_buffer);
    rx->playback_buffer=NULL;
  }

  if(rx->playback_handle!=NULL) {
    err = Pa_StopStream(rx->playback_handle);
    if (err != paNoError) {
      fprintf(stderr,"PORTAUDIO ERROR: ACO stop stream: %s\n",Pa_GetErrorText(err));
    }
    err = Pa_CloseStream(rx->playback_handle);
    if (err != paNoError) {
      fprintf(stderr,"PORTAUDIO ERROR: ACO close stream: %s\n",Pa_GetErrorText(err));
    }
    rx->playback_handle=NULL;
  }
}

//
// AUDIO_WRITE
//
// send RX audio data to a PA output stream
// we have to store the data such that the PA callback function
// can access it.
//
int audio_write (RECEIVER *rx, short r, short l)
{
  PaError err;
  int mode=transmitter->mode;
  //
  // We have to stop the stream here if a CW side tone may occur.
  // This might cause underflows, but we cannot use audio_write
  // and cw_audio_write simultaneously on the same device.
  // Instead, the side tone version will take over.
  // If *not* doing CW, the stream continues because we might wish
  // to listen to this rx while transmitting.
  //
  if (rx == active_receiver && isTransmitting() && (mode==modeCWU || mode==modeCWL)) return 0;

  if (rx->playback_handle != NULL && rx->playback_buffer != NULL) {
    rx->playback_buffer[rx->playback_offset++] = (r + l) *0.000015259;  //   65536 --> 1.0   
    if (rx->playback_offset == audio_buffer_size) {
      rx->playback_offset=0;
      err=Pa_WriteStream(rx->playback_handle, (void *) rx->playback_buffer, (unsigned long) audio_buffer_size);
      //if (err != paNoError) {
      //  fprintf(stderr,"PORTAUDIO ERROR: write stream dev=%d: %s\n",out_device_no[rx->audio_device],Pa_GetErrorText(err));
      //  return -1;
      // }
    }
  }
  return 0;
}

int cw_audio_write(double sample) {
  PaError err;
  RECEIVER *rx = active_receiver;

  if (rx->playback_handle != NULL && rx->playback_buffer != NULL) {
    rx->playback_buffer[rx->playback_offset++] = sample;
    if (rx->playback_offset == audio_buffer_size) {
      rx->playback_offset=0;
      err=Pa_WriteStream(rx->playback_handle, (void *) rx->playback_buffer, (unsigned long) audio_buffer_size);
      //if (err != paNoError) {
      //  fprintf(stderr,"PORTAUDIO ERROR: write stream dev=%d: %s\n",out_device_no[rx->audio_device],Pa_GetErrorText(err));
      //  return -1;
      // }
    }
  }
  return 0;
}

#endif
