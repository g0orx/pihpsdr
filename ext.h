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

#ifdef CLIENT_SERVER
#include "client_server.h"
#endif

//
// The following calls functions can be called usig g_idle_add
// Use these calls from within the rigclt daemon, or the GPIO or MIDI stuff
//

#ifdef CLIENT_SERVER
extern int ext_remote_command(void *data);
extern int ext_receiver_remote_update_display(void *data);
#endif

extern void local_set_frequency(int v,long long f);
extern int ext_discovery(void *data);
extern int ext_vfo_update(void *data);
extern int ext_set_frequency(void *data);
extern int ext_vfo_filter_changed(void *data);
extern int ext_band_update(void *data);
extern int ext_bandstack_update(void *data);
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

extern int ext_ps_update(void *data);
extern int ext_two_tone(void *data);
extern int ext_nr_update(void *data);
extern int ext_nb_update(void *data);
extern int ext_snb_update(void *data);
extern int ext_anf_update(void *data);
extern void band_plus(int id);
extern int ext_band_plus(void *data);
extern void band_minus(int id);
extern int ext_band_minus(void *data);
extern int ext_bandstack_plus(void *data);
extern int ext_bandstack_minus(void *data);
extern int ext_a_to_b(void *data);
extern int ext_lock_update(void *data);
extern int ext_rit_update(void *data);
extern int ext_rit_clear(void *data);
extern int ext_xit_update(void *data);
extern int ext_xit_clear(void *data);
extern int ext_filter_plus(void *data);
extern int ext_filter_minus(void *data);
extern int ext_mode_plus(void *data);
extern int ext_mode_minus(void *data);
extern int ext_b_to_a(void *data);
extern int ext_a_swap_b(void *data);
extern void ctun_update(int id,int state);
extern int ext_ctun_update(void *data);
extern int ext_agc_update(void *data);
extern int ext_split_toggle(void *data);
extern int ext_set_split(void *data);


extern int ext_cw_setup();
extern int ext_cw_key(void *data);

extern int ext_update_squelch(void *data);

extern int ext_sliders_update(void *data);

extern int ext_mode_update(void *data);
extern int ext_filter_update(void *data);
extern int ext_noise_update(void *data);

extern int ext_frequency_update(void *data);
extern int ext_memory_update(void *data);
extern int ext_function_update(void *data);

#ifdef PURESIGNAL
extern int ext_tx_set_ps(void *data);
#endif

extern int ext_update_vfo_step(void *data);
extern int ext_vfo_step(void *data);
extern int ext_vfo_id_step(void *data);
extern int ext_vfo_mode_changed(void *data);
extern int ext_set_af_gain(void *data);
extern int ext_set_mic_gain(void *data);
extern int ext_set_agc_gain(void *data);
extern int ext_set_drive(void *data);
extern int ext_vfo_a_swap_b(void *data);
extern int ext_vfo_a_to_b(void *data);
extern int ext_vfo_b_to_a(void *data);
extern int ext_update_att_preamp(void *data);
extern int ext_set_alex_attenuation(void *data);
extern int ext_set_attenuation_value(void *data);
extern int ext_set_compression(void *data);

extern int ext_start_rx(void *data);
extern int ext_start_tx(void *data);
extern int ext_diversity_update(void *data);
extern int ext_diversity_change_gain(void *data);
extern int ext_diversity_change_phase(void *data);
extern int ext_sat_update(void *data);
extern int ext_set_rf_gain(void *data);
extern int ext_set_duplex(void *data);

extern int ext_update_noise(void *data);
extern int ext_update_eq(void *data);
#ifdef PURESIGNAL
extern int ext_start_ps(void *data);
#endif

extern int ext_mute_update(void *data);

extern int ext_zoom_update(void *data);
extern int ext_zoom_set(void *data);
extern int ext_pan_update(void *data);
extern int ext_pan_set(void *data);

extern int ext_remote_set_zoom(void *data);
extern int ext_remote_set_pan(void *data);
extern int ext_set_title(void *data);


