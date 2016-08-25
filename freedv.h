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

extern int n_speech_samples;
extern int n_max_modem_samples;
extern short *demod_in;
extern short *speech_out;
extern short *speech_in;
extern short *mod_out;

extern int freedv_sync;
extern float freedv_snr;

extern char freedv_text_data[64];

void init_freedv();
void close_freedv();
int demod_sample_freedv(short sample);
int mod_sample_freedv(short sample);
void reset_freedv_tx_text_index();

#endif
