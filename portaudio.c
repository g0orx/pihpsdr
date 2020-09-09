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

//
// We now use callback functions to provide the "headphone" audio data,
// and therefore can control the latency.
// RX audio samples are put into a ring buffer and "fetched" therefreom
// by the portaudio "headphone" callback.
//
// We choose a ring buffer of 9600 samples that is kept about half-full
// during RX (latency: 0.1 sec) which should be more than enough.
// If the buffer falls below 1800, half a buffer length of silence is
// inserted. This usually only happens after TX/RX transitions
//
// If we go TX in CW mode, cw_audio_write() is called. If it is called for
// the first time, the ring buffer is cleared and only 256 samples of silence
// are put into it. During the TX phase, the buffer filling remains low
// which we need for small CW sidetone latencies. If we then go to RX again
// a "low water mark" condition is detected in the first call to audio_write()
// and half a buffer length of silence is inserted again.
//
// Experiments indicate that we can indeed keep the ring buffer about half full
// during RX and quite empty during CW-TX.
//

#define MY_AUDIO_BUFFER_SIZE 256
#define MY_RING_BUFFER_SIZE  9600
#define MY_RING_LOW_WATER    800
#define MY_RING_HIGH_WATER   8800

//
// Ring buffer for "local microphone" samples stored locally here.
// NOTE: lead large buffer for some "loopback" devices which produce
//       samples in large chunks if fed from digimode programs.
//
float  *mic_ring_buffer=NULL;
int     mic_ring_outpt=0;
int     mic_ring_inpt=0;

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
int pa_out_cb(const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*);

int audio_open_input()
{
  PaError err;
  PaStreamParameters inputParameters;
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

  bzero( &inputParameters, sizeof( inputParameters ) ); //not necessary if you are filling in all the fields
  inputParameters.channelCount = 1;   // MONO
  inputParameters.device = padev;
  inputParameters.hostApiSpecificStreamInfo = NULL;
  inputParameters.sampleFormat = paFloat32;
  inputParameters.suggestedLatency = Pa_GetDeviceInfo(padev)->defaultLowInputLatency ;
  inputParameters.hostApiSpecificStreamInfo = NULL; //See you specific host's API docs for info on using this field

  err = Pa_OpenStream(&record_handle, &inputParameters, NULL, 48000.0, (unsigned long) MY_AUDIO_BUFFER_SIZE,
                      paNoFlag, pa_mic_cb, NULL);
  if (err != paNoError) {
    fprintf(stderr, "PORTAUDIO ERROR: AOI open stream: %s\n",Pa_GetErrorText(err));
    return -1;
  }

  mic_ring_buffer=(float *) g_new(float,MY_RING_BUFFER_SIZE);
  mic_ring_outpt = mic_ring_inpt=0;
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
// PortAudio call-back function for Audio output
//
int pa_out_cb(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
             const PaStreamCallbackTimeInfo* timeInfo,
             PaStreamCallbackFlags statusFlags,
             void *userdata)
{
  float *out = (float *)outputBuffer;
  RECEIVER *rx = (RECEIVER *)userdata;
  int i, newpt;
  float *buffer=rx->local_audio_buffer;

  if (out == NULL) {
    fprintf(stderr,"PortAudio error: bogus audio buffer in callback\n");
    return paContinue;
  }
  newpt=rx->local_audio_buffer_outpt;
  for (i=0; i< framesPerBuffer; i++) {
    if (rx->local_audio_buffer_inpt == newpt) {
      // Ring buffer empty, send zero sample
      *out++ = 0.0;
    } else {
      *out++ = buffer[newpt++];
      if (newpt >= MY_RING_BUFFER_SIZE) newpt=0;
      rx->local_audio_buffer_outpt=newpt;
    }
  }
  return paContinue;
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
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
#endif
	//
	// put sample into ring buffer
	//
	if (mic_ring_buffer != NULL) {
          // the "existence" of the ring buffer is now guaranteed for 1 msec,
          // see audio_close_input(),
	  newpt=mic_ring_inpt +1;
	  if (newpt == MY_RING_BUFFER_SIZE) newpt=0;
	  if (newpt != mic_ring_outpt) {
	    // buffer space available, do the write
	    mic_ring_buffer[mic_ring_inpt]=sample;
	    // atomic update of mic_ring_inpt
	    mic_ring_inpt=newpt;
	  }
	}
	break;
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
  if ((mic_ring_buffer == NULL) || (mic_ring_outpt == mic_ring_inpt)) {
    // no buffer, or nothing in buffer: insert silence
    sample=0.0;
  } else {
    // the "existence" of the ring buffer is now guaranteed for 1 msec,
    // see audio_close_input(),
    newpt = mic_ring_outpt+1;
    if (newpt == MY_RING_BUFFER_SIZE) newpt=0;
    sample=mic_ring_buffer[mic_ring_outpt];
    // atomic update of read pointer
    mic_ring_outpt=newpt;
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

  //
  // This is now a ring buffer much larger than a single audio buffer
  //
  rx->local_audio_buffer=g_new(float,MY_RING_BUFFER_SIZE);
  rx->local_audio_buffer_inpt=0;
  rx->local_audio_buffer_outpt=0;
  rx->local_audio_cw=0;

  err = Pa_OpenStream(&(rx->playback_handle), NULL, &outputParameters, 48000.0, (unsigned long) MY_AUDIO_BUFFER_SIZE,
                      paNoFlag, pa_out_cb, rx);
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
// Note that the check on isTransmitting() takes care that "blocking"
// by the mutex can only occur in the moment of a RX/TX transition if
// both audio_write() and cw_audio_write() get a "go".
//
// So mutex locking/unlocking should only cost few CPU cycles in
// normal operation.
//
int audio_write (RECEIVER *rx, float left, float right)
{
  int mode=modeUSB;
  float *buffer = rx->local_audio_buffer;
  int oldpt,newpt;
  int i,avail;

  if (can_transmit) {
    mode=transmitter->mode;
  }
  if (rx == active_receiver && isTransmitting() && (mode==modeCWU || mode==modeCWL)) {
    //
    // If a CW side tone may occur, quickly return
    //
    return 0;
  }

  g_mutex_lock(&rx->local_audio_mutex);
  if (rx->playback_handle != NULL && buffer != NULL) {
    if (rx->local_audio_cw == 1) {
      //
      // We come from a TX->RX transition:
      // Clear buffer and insert half a buffer length of silence
      // 
      rx->local_audio_cw=0;
      bzero(buffer, sizeof(float)*(MY_RING_BUFFER_SIZE/2));
      rx->local_audio_buffer_inpt=MY_RING_BUFFER_SIZE/2;
      rx->local_audio_buffer_outpt=0;
    }
    avail = rx->local_audio_buffer_inpt - rx->local_audio_buffer_outpt;
    if (avail < 0) avail += MY_RING_BUFFER_SIZE;
    if (avail <  MY_RING_LOW_WATER) {
      //
      // fill half a ring buffer's length of silence
      //
      oldpt=rx->local_audio_buffer_inpt;
      for (i=0; i< MY_RING_BUFFER_SIZE/2; i++) {
        buffer[oldpt++]=0.0;
        if (oldpt >= MY_RING_BUFFER_SIZE) oldpt=0;
      }
      rx->local_audio_buffer_inpt=oldpt;
      //This is triggered after each RX/TX transition unless we operate CW and/or DUPLEX.
      //g_print("audio_write: buffer was nearly empty, inserted silence\n");
    }
    if (avail > MY_RING_HIGH_WATER) {
      //
      // Running the RX for a very long time (without doing TX)
      // and with audio hardware whose "48000 Hz" are a little slower than the "48000 Hz" of
      // the SDR will very slowly fill the buffer. This should be the only situation where
      // this "buffer overrun" condition should occur. We recover from this by brutally
      // deleting half a buffer size of audio, such that the next overrun is in the distant
      // future.
      //
      oldpt=rx->local_audio_buffer_inpt;
      for (i=0; i< MY_RING_BUFFER_SIZE/2; i++) {
        oldpt--;
        if (oldpt < 0) oldpt = MY_RING_BUFFER_SIZE;
      }
      rx->local_audio_buffer_inpt=oldpt;
      g_print("audio_write: buffer was nearly full, deleted audio\n");
    }
    //
    // put sample into ring buffer
    //
    oldpt=rx->local_audio_buffer_inpt;
    newpt=oldpt+1;
    if (newpt == MY_RING_BUFFER_SIZE) newpt=0;
    if (newpt != rx->local_audio_buffer_outpt) {
      //
      // buffer space available
      //
      buffer[oldpt] = (left+right)*0.5;  //   mix to MONO   
      rx->local_audio_buffer_inpt=newpt;
    }
  }
  g_mutex_unlock(&rx->local_audio_mutex);
  return 0;
}

int cw_audio_write(float sample) {
  RECEIVER *rx = active_receiver;
  float *buffer = rx->local_audio_buffer;
  int oldpt, newpt;

  g_mutex_lock(&rx->local_audio_mutex);
  if (rx->playback_handle != NULL && buffer != NULL) {
    if (rx->local_audio_cw == 0) {
      //
      // First time producing CW audio after RX/TX transition:
      // empty audio buffer, insert one batch of silence, and
      // continue with small latency.
      //
      rx->local_audio_cw=1;
      bzero(buffer, sizeof(float)*MY_AUDIO_BUFFER_SIZE);
      rx->local_audio_buffer_inpt=MY_AUDIO_BUFFER_SIZE;
      rx->local_audio_buffer_outpt=0;
    }
    // 
    // put sample into ring buffer
    //
    oldpt=rx->local_audio_buffer_inpt;
    newpt=oldpt+1;
    if (newpt == MY_RING_BUFFER_SIZE) newpt=0; 
    if (newpt != rx->local_audio_buffer_outpt) {
      //
      // buffer space available
      //
      buffer[oldpt] = sample;
      rx->local_audio_buffer_inpt=newpt;
    }
  }
  g_mutex_unlock(&rx->local_audio_mutex);
  return 0;
}

#endif
