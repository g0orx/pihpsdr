/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
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


//
// The following calls functions can be called usig g_idle_add
// Use these calls from within the rigclt daemon, or the GPIO or MIDI stuff
//

extern int ext_discovery(void *data);
extern int ext_vfo_update(void *data);
extern int ext_set_frequency(void *data);
extern int ext_vfo_filter_changed(void *data);
extern int ext_band_update(void *data);
extern int ext_ptt_update(void *data);
extern int ext_mox_update(void *data);
extern int ext_tune_update(void *data);
extern int ext_linein_changed(void *data);
extern int ext_vox_changed(void *data);
extern int ext_sliders_mode_changed(void *data);
extern int ext_update_agc_gain(void *data);
extern int ext_update_af_gain(void *data);
extern int ext_calc_drive_level(void *data);
extern int ext_vfo_band_changed(void *data);
extern int ext_radio_change_sample_rate(void *data);

extern int ext_cw_setup();
extern int ext_cw_key(void *data);

extern int ext_update_squelch(void *data);

extern int ext_sliders_update(void *data);

extern int ext_mode_update(void *data);
extern int ext_filter_update(void *data);
extern int ext_noise_update(void *data);

#ifdef PURESIGNAL
extern int ext_tx_set_ps(void *data);
extern int ext_ps_twotone(void *data);
#endif

int ext_vfo_step(void *data);
int ext_vfo_mode_changed(void *data);
int ext_set_af_gain(void *data);
int ext_set_mic_gain(void *data);
int ext_set_agc_gain(void *data);
int ext_set_drive(void *data);
int ext_vfo_a_swap_b(void *data);
int ext_vfo_a_to_b(void *data);
int ext_vfo_b_to_a(void *data);
int ext_update_att_preamp(void *data);
int ext_set_alex_attenuation(void *data);
int ext_set_attenuation_value(void *data);
