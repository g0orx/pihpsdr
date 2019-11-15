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
  ENCODER_AF_GAIN_RX1=0,
  ENCODER_RF_GAIN_RX1,
  ENCODER_AF_GAIN_RX2,
  ENCODER_RF_GAIN_RX2,
  ENCODER_AGC_GAIN_RX1,
  ENCODER_AGC_GAIN_RX2,
  ENCODER_IF_WIDTH_RX1,
  ENCODER_IF_WIDTH_RX2,
  ENCODER_ATTENUATION,
  ENCODER_MIC_GAIN,
  ENCODER_DRIVE,
  ENCODER_TUNE_DRIVE,
  ENCODER_RIT_RX1,
  ENCODER_RIT_RX2,
  ENCODER_XIT,
  ENCODER_CW_SPEED,
  ENCODER_CW_FREQUENCY,
  ENCODER_PANADAPTER_HIGH,
  ENCODER_PANADAPTER_LOW,
  ENCODER_SQUELCH,
  ENCODER_COMP,
  ENCODER_DIVERSITY_GAIN,
  ENCODER_DIVERSITY_PHASE,
  ENCODER_ACTIONS
};

extern char *encoder_string[ENCODER_ACTIONS];

enum {
  NO_ACTION=0,
  TUNE,
  MOX,
  PS,
  TWO_TONE,
  NR,
  NB,
  SNB,
  RIT,
  RIT_CLEAR,
  XIT,
  XIT_CLEAR,
  BAND_PLUS,
  BAND_MINUS,
  BANDSTACK_PLUS,
  BANDSTACK_MINUS,
  MODE_PLUS,
  MODE_MINUS,
  FILTER_PLUS,
  FILTER_MINUS,
  A_TO_B,
  B_TO_A,
  A_SWAP_B,
  LOCK,
  CTUN,
  AGC,
  SPLIT,
  DIVERSITY,
  SAT,
  MENU_BAND,
  MENU_BANDSTACK,
  MENU_MODE,
  MENU_FILTER,
  MENU_FREQUENCY,
  MENU_MEMORY,
  MENU_DIVERSITY,
  FUNCTION,
  SWITCH_ACTIONS
};

extern char *sw_string[SWITCH_ACTIONS];


#if !defined (CONTROLLER2_V2) && !defined (CONTROLLER2_V1)
#define SWITCHES 8
enum {
  SW1=0,
  SW2,
  SW3,
  SW4,
  SW5,
  SW6,
  SW7,
  SW8
};
#endif

#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
#define SWITCHES 16
enum {
  SW2=0,
  SW3,
  SW4,
  SW5,
  SW6,
  SW7,
  SW8,
  SW9,
  SW10,
  SW11,
  SW12,
  SW13,
  SW14,
  SW15,
  SW16,
  SW17
};

#endif

extern int sw_action[SWITCHES];

extern int settle_time;


extern int e2_encoder_action;
extern int e3_encoder_action;
extern int e4_encoder_action;
extern int e5_encoder_action;

extern int e2_top_encoder_action;
extern int e3_top_encoder_action;
extern int e4_top_encoder_action;
extern int e5_top_encoder_action;

extern int e2_sw_action;
extern int e3_sw_action;
extern int e4_sw_action;
extern int e5_sw_action;

extern int ENABLE_VFO_ENCODER;
extern int ENABLE_VFO_PULLUP;
extern int VFO_ENCODER_A;
extern int VFO_ENCODER_B;
extern int VFO_ENCODER_A_PIN;
extern int VFO_ENCODER_B_PIN;
extern int ENABLE_E2_ENCODER;
extern int ENABLE_E2_PULLUP;
extern int E2_ENCODER_A;
extern int E2_ENCODER_B;
extern int ENABLE_E3_ENCODER;
extern int ENABLE_E3_PULLUP;
extern int E3_ENCODER_A;
extern int E3_ENCODER_B;
extern int ENABLE_E4_ENCODER;
extern int ENABLE_E4_PULLUP;
extern int E4_ENCODER_A;
extern int E4_ENCODER_B;
#if defined (CONTROLLER2_V2) || defined (CONTROLLER2_V1)
extern int ENABLE_E5_ENCODER;
extern int ENABLE_E5_PULLUP;
extern int E5_ENCODER_A;
extern int E5_ENCODER_B;
#endif
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
#ifdef LOCALCW
extern int CWL_BUTTON;
extern int CWR_BUTTON;
extern int SIDETONE_GPIO;
extern int ENABLE_GPIO_SIDETONE;
extern int ENABLE_CW_BUTTONS;
extern int CW_ACTIVE_LOW;
void gpio_cw_sidetone_set(int level);
int  gpio_left_cw_key();
int  gpio_right_cw_key();
int  gpio_cw_sidetone_enabled();
#endif

void gpio_restore_actions();
void gpio_restore_state();
void gpio_save_state();
void gpio_save_actions();
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
