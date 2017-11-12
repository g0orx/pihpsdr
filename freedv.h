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

#ifndef _FREEDV_H
#define _FREEDV_H

#include "receiver.h"

extern int n_speech_samples;
extern int n_max_modem_samples;
extern short *demod_in;
extern short *speech_out;
extern short *speech_in;
extern short *mod_out;

extern int freedv_sync;
extern float freedv_snr;

extern int freedv_rate;
extern int freedv_resample;

extern int freedv_mode;

extern char freedv_text_data[64];

extern int freedv_sq_enable;
extern double freedv_snr_sq_threshold;

extern double freedv_audio_gain;

extern float *freedv_samples;

extern void freedv_save_state();
extern void freedv_restore_state();
extern void init_freedv(RECEIVER *rx);
extern void close_freedv(RECEIVER *rx);
extern int demod_sample_freedv(short sample);
extern int mod_sample_freedv(short sample);
extern void freedv_reset_tx_text_index();
extern void freedv_set_mode(RECEIVER *rx,int m);
extern void init_freedv(RECEIVER *rx);
extern void close_freedv(RECEIVER *rx);
extern int demod_sample_freedv(short sample);
extern int mod_sample_freedv(short sample);
extern void freedv_reset_tx_text_index();
extern void freedv_set_mode(RECEIVER *rx,int m);
extern void freedv_set_sq_enable(int state);
extern void freedv_set_sq_threshold(double threshold);
extern void freedv_set_audio_gain(double gain);
extern char* freedv_get_mode_string();

#endif
