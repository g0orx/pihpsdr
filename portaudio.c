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
// It seems that a PortAudio device can only be opened once,
// therefore there is no need to support stereo output.
// (cw_)audio_write therefore put the sum of left and right
// channel to a (mono) output stream
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
#include "radio.h"
#include "receiver.h"
#include "mode.h"
#include "portaudio.h"
#include "audio.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif

static PaStream *record_handle=NULL;

int n_input_devices;
AUDIO_DEVICE input_devices[MAX_AUDIO_DEVICES];
int n_output_devices;
AUDIO_DEVICE output_devices[MAX_AUDIO_DEVICES];

int n_input_devices=0;
int n_output_devices=0;

#define BUFFER_SIZE 256

//
// Ring buffer for "local microphone" samples
// NOTE: lead large buffer for some "loopback" devices which produce
//       samples in large chunks if fed from digimode programs.
//
#define MICRINGLEN 6000
float  *mic_ring_buffer=NULL;
int     mic_ring_read_pt=0;
int     mic_ring_write_pt=0;

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
          if (n_input_devices < MAX_AUDIO_DEVICES) {
	    //
	    // probably not necessary with portaudio, but to be on the safe side,
	    // we copy the device name to local storage. This is referenced both
	    // by the name and description element.
	    //
            input_devices[n_input_devices].name=input_devices[n_input_devices].description=g_new0(char,strlen(deviceInfo->name)+1);
	    strcpy(input_devices[n_input_devices].name, deviceInfo->name);
            input_devices[n_input_devices].index=i;
            n_input_devices++;
          }
          fprintf(stderr,"PORTAUDIO INPUT DEVICE, No=%d, Name=%s\n", i, deviceInfo->name);
	}

	outputParameters.device = i;
        outputParameters.channelCount = 1;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
        outputParameters.hostApiSpecificStreamInfo = NULL;
        if (Pa_IsFormatSupported(NULL, &outputParameters, 48000.0) == paFormatIsSupported) {
          if (n_output_devices < MAX_AUDIO_DEVICES) {
            output_devices[n_output_devices].name=output_devices[n_output_devices].description=g_new0(char,strlen(deviceInfo->name)+1);
            strcpy(output_devices[n_output_devices].name, deviceInfo->name);
            output_devices[n_output_devices].index=i;
            n_output_devices++;
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
  PaStreamParameters inputParameters;
  long framesPerBuffer;
  int i;
  int padev;

  if(transmitter->microphone_name==NULL) {
    transmitter->local_microphone=0;
    return -1;
  }
  //
  // Look up device name and determine device ID
  //
  padev=-1;
  for (i=0; i<n_input_devices; i++) {
    if (!strcmp(transmitter->microphone_name, input_devices[i].name)) {
	padev=input_devices[i].index;
	break;
    }
  }
  fprintf(stderr,"audio_open_input: name=%s PADEV=%d\n",transmitter->microphone_name,padev);
  //
  // Should not occur, but possibly device name not found
  //
  if (padev < 0) {
    return -1;
  }

  framesPerBuffer = BUFFER_SIZE;  // is this for either protocol

  bzero( &inputParameters, sizeof( inputParameters ) ); //not necessary if you are filling in all the fields
  inputParameters.channelCount = 1;   // MONO
  inputParameters.device = padev;
  inputParameters.hostApiSpecificStreamInfo = NULL;
  inputParameters.sampleFormat = paFloat32;
  inputParameters.suggestedLatency = Pa_GetDeviceInfo(padev)->defaultLowInputLatency ;
  inputParameters.hostApiSpecificStreamInfo = NULL; //See you specific host's API docs for info on using this field

  err = Pa_OpenStream(&record_handle, &inputParameters, NULL, 48000.0, framesPerBuffer, paNoFlag, pa_mic_cb, NULL);
  if (err != paNoError) {
    fprintf(stderr, "PORTAUDIO ERROR: AOI open stream: %s\n",Pa_GetErrorText(err));
    return -1;
  }

  mic_ring_buffer=(float *) g_new(float,MICRINGLEN);
  mic_ring_read_pt = mic_ring_write_pt=0;
  if (mic_ring_buffer == NULL) {
    return -1;
  }

  err = Pa_StartStream(record_handle);
  if (err != paNoError) {
    fprintf(stderr, "PORTAUDIO ERROR: AOI start stream:%s\n",Pa_GetErrorText(err));
    return -1;
  }
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
  float *in = (float *)inputBuffer;
  int i, newpt;
  float sample;

  if (in == NULL) {
    // This should not happen, so we do not send silence etc.
    fprintf(stderr,"PortAudio error: bogus audio buffer in callback\n");
    return paContinue;
  }
  //
  // send the samples in the buffer
  //
  for (i=0; i<framesPerBuffer; i++) {
    sample=in[i];
    switch(protocol) {
      case ORIGINAL_PROTOCOL:
      case NEW_PROTOCOL:
	//
	// put sample into ring buffer
	//
	if (mic_ring_buffer != NULL) {
          // the "existence" of the ring buffer is now guaranteed for 1 msec,
          // see audio_close_input(),
	  newpt=mic_ring_write_pt +1;
	  if (newpt == MICRINGLEN) newpt=0;
	  if (newpt != mic_ring_read_pt) {
	    // buffer space available, do the write
	    mic_ring_buffer[mic_ring_write_pt]=sample;
	    // atomic update of mic_ring_write_pt
	    mic_ring_write_pt=newpt;
	  }
	}
	break;
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
	// Note that this call ends up deeply in the TX engine
	soapy_protocol_process_local_mic(sample);
	break;
#endif
      default:
	break;
    }
  }
  return paContinue;
}

//
// Utility function for retrieving mic samples
// from ring buffer
//
float audio_get_next_mic_sample() {
  int newpt;
  float sample;
  if ((mic_ring_buffer == NULL) || (mic_ring_read_pt == mic_ring_write_pt)) {
    // no buffer, or nothing in buffer: insert silence
    sample=0.0;
  } else {
    // the "existence" of the ring buffer is now guaranteed for 1 msec,
    // see audio_close_input(),
    newpt = mic_ring_read_pt+1;
    if (newpt == MICRINGLEN) newpt=0;
    sample=mic_ring_buffer[mic_ring_read_pt];
    // atomic update of read pointer
    mic_ring_read_pt=newpt;
  }
  return sample;
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
  long framesPerBuffer=BUFFER_SIZE;
  int padev;
  int i;

  if(rx->audio_name==NULL) {
    rx->local_audio=0;
    return -1;
  }
  //
  // Look up device name and determine device ID
  //
  padev=-1;
  for (i=0; i<n_output_devices; i++) {
    if (!strcmp(rx->audio_name, output_devices[i].name)) {
        padev=output_devices[i].index;
        break;
    }
  }
  fprintf(stderr,"audio_open_output: name=%s PADEV=%d\n",rx->audio_name,padev);
  //
  // Should not occur, but possibly device name not found
  //
  if (padev < 0) {
    return -1;
  }

  g_mutex_lock(&rx->local_audio_mutex);
  bzero( &outputParameters, sizeof( outputParameters ) ); //not necessary if you are filling in all the fields
  outputParameters.channelCount = 1;   // Always MONO
  outputParameters.device = padev;
  outputParameters.hostApiSpecificStreamInfo = NULL;
  outputParameters.sampleFormat = paFloat32;
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(padev)->defaultLowOutputLatency ;
  outputParameters.hostApiSpecificStreamInfo = NULL; //See you specific host's API docs for info on using this field

  // Do not use call-back function, just stream it

  rx->local_audio_buffer=g_new(float,BUFFER_SIZE);
  rx->local_audio_buffer_offset=0;
  err = Pa_OpenStream(&(rx->playback_handle), NULL, &outputParameters, 48000.0, framesPerBuffer, paNoFlag, NULL, NULL);
  if (err != paNoError || rx->local_audio_buffer == NULL) {
    fprintf(stderr,"PORTAUDIO ERROR: out open stream: %s\n",Pa_GetErrorText(err));
    rx->playback_handle = NULL;
    if (rx->local_audio_buffer) g_free(rx->local_audio_buffer);
    rx->local_audio_buffer = NULL;
    g_mutex_unlock(&rx->local_audio_mutex);
    return -1;
  }

  err = Pa_StartStream(rx->playback_handle);
  if (err != paNoError) {
    fprintf(stderr,"PORTAUDIO ERROR: out start stream:%s\n",Pa_GetErrorText(err));
    rx->playback_handle=NULL;
    if (rx->local_audio_buffer) g_free(rx->local_audio_buffer);
    rx->local_audio_buffer = NULL;
    g_mutex_unlock(&rx->local_audio_mutex);
    return -1;
  }
  // Write one buffer to avoid under-flow errors
  // (this gives us 5 msec to pass before we have to call audio_write the first time)
  bzero(rx->local_audio_buffer, (size_t) BUFFER_SIZE*sizeof(float));
  Pa_WriteStream(rx->playback_handle, rx->local_audio_buffer, (unsigned long) BUFFER_SIZE);
  g_mutex_unlock(&rx->local_audio_mutex);
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
  void *p;

  fprintf(stderr,"AudioCloseInput: %s\n", transmitter->microphone_name);

  if(record_handle!=NULL) {
    err = Pa_StopStream(record_handle);
    if (err != paNoError) {
      fprintf(stderr,"PORTAUDIO ERROR: in stop stream: %s\n",Pa_GetErrorText(err));
    }
    err = Pa_CloseStream(record_handle);
    if (err != paNoError) {
      fprintf(stderr,"PORTAUDIO ERROR: in close stream: %s\n",Pa_GetErrorText(err));
    }
    record_handle=NULL;
  }
  //
  // We do not want to do a mutex lock/unlock for every single mic sample
  // accessed. Since only the ring buffer is maintained by the functions
  // audio_get_next_mic_sample() and in the "mic callback" function,
  // it is more than enough to wait 2 msec after setting mic_ring_buffer to NULL
  // before actually releasing the storage.
  //
  if (mic_ring_buffer != NULL) {
    p=mic_ring_buffer;
    mic_ring_buffer=NULL;
    usleep(2);
    g_free(p);
  }
}

//
// AUDIO_CLOSE_OUTPUT
//
// shut down the stream connected with audio from one of the RX
//
void audio_close_output(RECEIVER *rx) {
  PaError err;

  fprintf(stderr,"AudioCloseOutput: %s\n", rx->audio_name);

  g_mutex_lock(&rx->local_audio_mutex);
  if(rx->local_audio_buffer!=NULL) {
    g_free(rx->local_audio_buffer);
    rx->local_audio_buffer=NULL;
  }

  if(rx->playback_handle!=NULL) {
    err = Pa_StopStream(rx->playback_handle);
    if (err != paNoError) {
      fprintf(stderr,"PORTAUDIO ERROR: out stop stream: %s\n",Pa_GetErrorText(err));
    }
    err = Pa_CloseStream(rx->playback_handle);
    if (err != paNoError) {
      fprintf(stderr,"PORTAUDIO ERROR: out close stream: %s\n",Pa_GetErrorText(err));
    }
    rx->playback_handle=NULL;
  }
  g_mutex_unlock(&rx->local_audio_mutex);
}

//
// AUDIO_WRITE
//
// send RX audio data to a PA output stream
// we have to store the data such that the PA callback function
// can access it.
//
int audio_write (RECEIVER *rx, float left, float right)
{
  PaError err;
  int mode=modeUSB;
  float *buffer = rx->local_audio_buffer;

  if (can_transmit) {
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
    return 0;
  }

  g_mutex_lock(&rx->local_audio_mutex);
  if (rx->playback_handle != NULL && buffer != NULL) {
    buffer[rx->local_audio_buffer_offset++] = (left+right)*0.5;  //   mix to MONO   
    if (rx->local_audio_buffer_offset == BUFFER_SIZE) {
      Pa_WriteStream(rx->playback_handle, buffer, (unsigned long) BUFFER_SIZE);
      rx->local_audio_buffer_offset=0;
      // do not check on errors, there will be underflows every now and then
    }
  }
  g_mutex_unlock(&rx->local_audio_mutex);
  return 0;
}

int cw_audio_write(float sample) {
  PaError err;
  RECEIVER *rx = active_receiver;
  float *buffer = rx->local_audio_buffer;

  if (rx->playback_handle != NULL && rx->local_audio_buffer != NULL) {
    buffer[rx->local_audio_buffer_offset++] = sample;
    if (rx->local_audio_buffer_offset == BUFFER_SIZE) {
      Pa_WriteStream(rx->playback_handle, rx->local_audio_buffer, (unsigned long) BUFFER_SIZE);
      // do not check on errors, there will be underflows every now and then
      rx->local_audio_buffer_offset=0;
    }
  }
  return 0;
}

#endif
