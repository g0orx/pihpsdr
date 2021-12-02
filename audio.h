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

#ifndef _AUDIO_H
#define _AUDIO_H

#include "receiver.h"

#define MAX_AUDIO_DEVICES 64

#define AUDIO_BUFFER_SIZE 480

typedef struct _audio_devices {
  char *name;
  int index;
  char *description;
} AUDIO_DEVICE;

extern int n_input_devices;
extern AUDIO_DEVICE input_devices[MAX_AUDIO_DEVICES];
extern int n_output_devices;
extern AUDIO_DEVICE output_devices[MAX_AUDIO_DEVICES];
extern GMutex audio_mutex;
extern gint local_microphone_buffer_size;

extern int audio_open_input();
extern void audio_close_input();
extern int audio_open_output(RECEIVER *rx);
extern void audio_close_output(RECEIVER *rx);
extern int audio_write(RECEIVER *rx,float left_sample,float right_sample);
extern int cw_audio_write(RECEIVER *rx,float sample);
extern void audio_get_cards();
char * audio_get_error_string(int err);
float  audio_get_next_mic_sample();
#endif
