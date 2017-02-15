/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
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

#ifndef _OLD_PROTOCOL_H
#define _OLD_PROTOCOL_H

extern void old_protocol_stop();
extern void old_protocol_run();

extern void old_protocol_init(int rx,int pixels,int rate);
extern void old_protocol_set_mic_sample_rate(int rate);

extern void schedule_frequency_changed();
extern void old_protocol_process_local_mic(unsigned char *buffer,int le);
extern void old_protocol_audio_samples(RECEIVER *rx,short left_audio_sample,short right_audio_sample);
extern void old_protocol_iq_samples(int isample,int qsample);
#endif
