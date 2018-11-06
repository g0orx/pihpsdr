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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>
#include "discovery.h"
#include "receiver.h"
#include "sliders.h"
#include "toolbar.h"
#include "band_menu.h"
#include "vfo.h"
#include "radio.h"
#include "new_menu.h"
#ifdef PURESIGNAL
#include "ps_menu.h"
#endif

// The following calls functions can be called usig g_idle_add

int ext_vfo_mode_changed(void * data)
{
  int mode=(uintptr_t) data;
  vfo_mode_changed(mode);
  return 0;
}

int ext_discovery(void *data) {
  discovery();
  return 0;
}

int ext_set_frequency(void *data) {
  setFrequency(*(long long *)data);
  free(data);
  return 0;
}

int ext_vfo_update(void *data) {
  vfo_update();
  return 0;
}

int ext_vfo_filter_changed(void *data) {
  vfo_filter_changed((uintptr_t)data);
  return 0;
}

int ext_band_update(void *data) {
  if(data==NULL) {
    start_band();
  } else {
    band_select_cb(NULL,data);
  }
  return 0;
}

int ext_mode_update(void *data) {
  start_mode();
  // DL1YCF added return statement
  return 0;
}

int ext_filter_update(void *data) {
  start_filter();
  // DL1YCF added return statement
  return 0;
}

int ext_noise_update(void *data) {
  start_noise();
  // DL1YCF added return statement
  return 0;
}

int ext_ptt_update(void *data) {
  ptt_update((uintptr_t)data);
  return 0;
}

int ext_mox_update(void *data) {
  mox_update((uintptr_t)data);
  return 0;
}

int ext_tune_update(void *data) {
  tune_update((uintptr_t)data);
  return 0;
}

int ext_vox_changed(void *data) {
  vox_changed((uintptr_t)data);
  return 0;
}

int ext_update_agc_gain(void *data) {
  update_agc_gain(*(double *)data);
  free(data);
  return 0;
}

int ext_update_af_gain(void *data) {
  update_af_gain();
  return 0;
}

int ext_calc_drive_level(void *data) {
  calcDriveLevel();
  return 0;
}

int ext_vfo_band_changed(void *data) {
  vfo_band_changed((uintptr_t)data);
  return 0;
}

int ext_radio_change_sample_rate(void *data) {
  radio_change_sample_rate((uintptr_t)data);
  return 0;
}

int ext_update_squelch(void *data) {
  set_squelch(active_receiver);
  return 0;
}

int ext_sliders_update(void *data) {
  sliders_update();
  return 0;
}

#ifdef PURESIGNAL
int ext_tx_set_ps(void *data) {
  int state=(uintptr_t) data;
  tx_set_ps(transmitter, state);
  return 0;
}
#endif

int ext_vfo_step(void *data) {
  int step=(int)(long)data;
  vfo_step(step);
  return 0;
}
