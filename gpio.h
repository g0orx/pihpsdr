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

#ifndef _GPIO_H
#define _GPIO_H

enum {
  ENCODER_AF_GAIN=0,
  ENCODER_AGC_GAIN,
  ENCODER_ATTENUATION,
  ENCODER_MIC_GAIN,
  ENCODER_DRIVE,
//  ENCODER_TUNE_DRIVE,
  ENCODER_RIT,
  ENCODER_CW_SPEED,
  ENCODER_CW_FREQUENCY,
  ENCODER_PANADAPTER_HIGH,
  ENCODER_PANADAPTER_LOW,
  ENCODER_SQUELCH,
  ENCODER_COMP,
  ENCODER_LAST
};



extern char *encoder_string[ENCODER_LAST];

extern int e1_encoder_action;
extern int e2_encoder_action;
extern int e3_encoder_action;

extern int ENABLE_VFO_ENCODER;
extern int ENABLE_VFO_PULLUP;
extern int VFO_ENCODER_A;
extern int VFO_ENCODER_B;
extern int VFO_ENCODER_A_PIN;
extern int VFO_ENCODER_B_PIN;
extern int ENABLE_E1_ENCODER;
extern int ENABLE_E1_PULLUP;
extern int E1_ENCODER_A;
extern int E1_ENCODER_B;
extern int ENABLE_E2_ENCODER;
extern int ENABLE_E2_PULLUP;
extern int E2_ENCODER_A;
extern int E2_ENCODER_B;
extern int ENABLE_E3_ENCODER;
extern int ENABLE_E3_PULLUP;
extern int E3_ENCODER_A;
extern int E3_ENCODER_B;
extern int ENABLE_S1_BUTTON;
extern int S1_BUTTON;
extern int ENABLE_S2_BUTTON;
extern int S2_BUTTON;
extern int ENABLE_S3_BUTTON;
extern int S3_BUTTON;
extern int ENABLE_S4_BUTTON;
extern int S4_BUTTON;
extern int ENABLE_S5_BUTTON;
extern int S5_BUTTON;
extern int ENABLE_S6_BUTTON;
extern int S6_BUTTON;
extern int ENABLE_MOX_BUTTON;
extern int MOX_BUTTON;
extern int ENABLE_FUNCTION_BUTTON;
extern int FUNCTION_BUTTON;
extern int CWL_BUTTON;
extern int CWR_BUTTON;

void gpio_restore_state();
void gpio_save_state();
int gpio_init();
void gpio_close();
int vfo_encoder_get_pos();
int af_encoder_get_pos();
int af_function_get_state();
int rf_encoder_get_pos();
int rf_function_get_state();
int function_get_state();
int band_get_state();
int mode_get_state();
int filter_get_state();
int noise_get_state();
int mox_get_state();

#endif
