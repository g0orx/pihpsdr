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
  NO_CONTROLLER=0,
  CONTROLLER1,
  CONTROLLER2_V1,
  CONTROLLER2_V2,
  CUSTOM_CONTROLLER
};

extern int controller;
  
enum {
  ENCODER_NO_ACTION=0,
  ENCODER_AF_GAIN,
  ENCODER_RF_GAIN,
  ENCODER_AGC_GAIN,
  ENCODER_IF_WIDTH,
  ENCODER_IF_SHIFT,
  ENCODER_AF_GAIN_RX1,
  ENCODER_RF_GAIN_RX1,
  ENCODER_AF_GAIN_RX2,
  ENCODER_RF_GAIN_RX2,
  ENCODER_AGC_GAIN_RX1,
  ENCODER_AGC_GAIN_RX2,
  ENCODER_IF_WIDTH_RX1,
  ENCODER_IF_WIDTH_RX2,
  ENCODER_IF_SHIFT_RX1,
  ENCODER_IF_SHIFT_RX2,
  ENCODER_ATTENUATION,
  ENCODER_MIC_GAIN,
  ENCODER_DRIVE,
  ENCODER_TUNE_DRIVE,
  ENCODER_RIT,
  ENCODER_RIT_RX1,
  ENCODER_RIT_RX2,
  ENCODER_XIT,
  ENCODER_CW_SPEED,
  ENCODER_CW_FREQUENCY,
  ENCODER_PANADAPTER_HIGH,
  ENCODER_PANADAPTER_LOW,
  ENCODER_PANADAPTER_STEP,
  ENCODER_WATERFALL_HIGH,
  ENCODER_WATERFALL_LOW,
  ENCODER_SQUELCH,
  ENCODER_SQUELCH_RX1,
  ENCODER_SQUELCH_RX2,
  ENCODER_COMP,
  ENCODER_DIVERSITY_GAIN,
  ENCODER_DIVERSITY_GAIN_COARSE,
  ENCODER_DIVERSITY_GAIN_FINE,
  ENCODER_DIVERSITY_PHASE,
  ENCODER_DIVERSITY_PHASE_COARSE,
  ENCODER_DIVERSITY_PHASE_FINE,
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
  MENU_PS,
  FUNCTION,
  SWITCH_ACTIONS
};

extern char *sw_string[SWITCH_ACTIONS];


enum {
  CONTROLLER1_SW1=0,
  CONTROLLER1_SW2,
  CONTROLLER1_SW3,
  CONTROLLER1_SW4,
  CONTROLLER1_SW5,
  CONTROLLER1_SW6,
  CONTROLLER1_SW7,
  CONTROLLER1_SW8,
  CONTROLLER1_SWITCHES
};

enum {
  CONTROLLER2_SW2=0,
  CONTROLLER2_SW3,
  CONTROLLER2_SW4,
  CONTROLLER2_SW5,
  CONTROLLER2_SW6,
  CONTROLLER2_SW7,
  CONTROLLER2_SW8,
  CONTROLLER2_SW9,
  CONTROLLER2_SW10,
  CONTROLLER2_SW11,
  CONTROLLER2_SW12,
  CONTROLLER2_SW13,
  CONTROLLER2_SW14,
  CONTROLLER2_SW15,
  CONTROLLER2_SW16,
  CONTROLLER2_SW17,
  CONTROLLER2_SWITCHES
};

extern int *sw_action;

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

// uses wiringpi pin numbers
extern int ENABLE_VFO_ENCODER;
extern int ENABLE_VFO_PULLUP;
extern int VFO_ENCODER_A;
extern int VFO_ENCODER_B;
extern int ENABLE_E2_ENCODER;
extern int ENABLE_E2_PULLUP;
extern int E2_ENCODER_A;
extern int E2_ENCODER_B;
extern int E2_TOP_ENCODER_A;
extern int E2_TOP_ENCODER_B;
extern int E2_FUNCTION;
extern int ENABLE_E3_ENCODER;
extern int ENABLE_E3_PULLUP;
extern int E3_ENCODER_A;
extern int E3_ENCODER_B;
extern int E3_TOP_ENCODER_A;
extern int E3_TOP_ENCODER_B;
extern int E3_FUNCTION;
extern int ENABLE_E4_ENCODER;
extern int ENABLE_E4_PULLUP;
extern int E4_ENCODER_A;
extern int E4_ENCODER_B;
extern int E4_TOP_ENCODER_A;
extern int E4_TOP_ENCODER_B;
extern int E4_FUNCTION;
extern int ENABLE_E5_ENCODER;
extern int ENABLE_E5_PULLUP;
extern int E5_ENCODER_A;
extern int E5_ENCODER_B;
extern int E5_TOP_ENCODER_A;
extern int E5_TOP_ENCODER_B;
extern int E5_FUNCTION;

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
extern void gpio_cw_sidetone_set(int level);
extern int  gpio_left_cw_key();
extern int  gpio_right_cw_key();
extern int  gpio_cw_sidetone_enabled();
#endif

extern void gpio_set_defaults(int ctrlr);
extern void gpio_restore_actions();
extern void gpio_restore_state();
extern void gpio_save_state();
extern void gpio_save_actions();
extern int gpio_init();
extern void gpio_close();
extern int vfo_encoder_get_pos();
extern int af_encoder_get_pos();
extern int af_function_get_state();
extern int rf_encoder_get_pos();
extern int rf_function_get_state();
extern int function_get_state();
extern int band_get_state();
extern int mode_get_state();
extern int filter_get_state();
extern int noise_get_state();
extern int mox_get_state();

#endif
