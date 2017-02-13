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

extern char *input_devices[];
extern int n_input_devices;
//extern int n_selected_input_device;

extern char *output_devices[];
extern int n_output_devices;
//extern int n_selected_output_device;

extern int audio_open_input();
extern void audio_close_input();
extern int audio_open_output(RECEIVER *rx);
extern void audio_close_output(RECEIVER *rx);
extern int audio_write(RECEIVER *rx,short left_sample,short right_sample);
extern void audio_get_cards();
#endif
