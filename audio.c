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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "audio.h"
#include "radio.h"

int audio = 0;
//int audio_buffer_size = 2016; // samples (both left and right)
int audio_buffer_size = 256; // samples (both left and right)

static pa_simple *stream;

// each buffer contains 63 samples of left and right audio at 16 bits
#define AUDIO_SAMPLES 63
#define AUDIO_SAMPLE_SIZE 2
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFERS 10
#define AUDIO_BUFFER_SIZE (AUDIO_SAMPLE_SIZE*AUDIO_CHANNELS*audio_buffer_size)

static unsigned char *audio_buffer;
static int audio_offset=0;

int audio_init() {

    static const pa_sample_spec spec= {
        .format = PA_SAMPLE_S16RE,
        .rate =  48000,
        .channels = 2
    };

    int error;

fprintf(stderr,"audio_init audio_buffer_size=%d\n",audio_buffer_size);

    audio_buffer=(unsigned char *)malloc(AUDIO_BUFFER_SIZE);

    if (!(stream = pa_simple_new(NULL, "pihpsdr", PA_STREAM_PLAYBACK, NULL, "playback", &spec, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        return -1;
    }

    return 0;
}

void audio_close() {
  pa_simple_free(stream);
}

void audio_write(double* buffer,int samples) {
    int i;
    int result;
    int error;

    for(i=0;i<samples;i++) {
        int source_index=i*2;
        short left_sample=(short)(buffer[source_index]*32767.0*volume);
        short right_sample=(short)(buffer[source_index+1]*32767.0*volume);
        audio_buffer[audio_offset++]=left_sample>>8;
        audio_buffer[audio_offset++]=left_sample;
        audio_buffer[audio_offset++]=right_sample>>8;
        audio_buffer[audio_offset++]=right_sample;

        if(audio_offset==AUDIO_BUFFER_SIZE) {
            result=pa_simple_write(stream, audio_buffer, (size_t)AUDIO_BUFFER_SIZE, &error);
            if(result< 0) {
                fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
                //_exit(1);
            }
            audio_offset=0;
        }
    }

}
