/* Copyright (C)
* 2020 - John Melton, G0ORX/N6LYT
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

#ifndef I2C_CONTROLLER_H
#define I2C_CONTROLLER_H


// Encoder register definition
#define REG_GCONF 0x00
#define REG_GP1CONF 0x01
#define REG_GP2CONF 0x02
#define REG_GP3CONF 0x03
#define REG_INTCONF 0x04
#define REG_ESTATUS 0x05
#define REG_I2STATUS 0x06
#define REG_FSTATUS 0x07
#define REG_CVALB4 0x08
#define REG_CVALB3 0x09
#define REG_CVALB2 0x0A
#define REG_CVALB1 0x0B
#define REG_CMAXB4 0x0C
#define REG_CMAXB3 0x0D
#define REG_CMAXB2 0x0E
#define REG_CMAXB1 0x0F
#define REG_CMINB4 0x10
#define REG_CMINB3 0x11
#define REG_CMINB2 0x12
#define REG_CMINB1 0x13
#define REG_ISTEPB4 0x14
#define REG_ISTEPB3 0x15
#define REG_ISTEPB2 0x16
#define REG_ISTEPB1 0x17
#define REG_RLED 0x18
#define REG_GLED 0x19
#define REG_BLED 0x1A
#define REG_GP1REG 0x1B
#define REG_GP2REG 0x1C
#define REG_GP3REG 0x1D
#define REG_ANTBOUNC 0x1E
#define REG_DPPERIOD 0x1F
#define REG_FADERGB 0x20
#define REG_FADEGP 0x21
#define REG_GAMRLED 0x27
#define REG_GAMGLED 0x28
#define REG_GAMBLED 0x29
#define REG_GAMMAGP1 0x2A
#define REG_GAMMAGP2 0x2B
#define REG_GAMMAGP3 0x2C
#define REG_GCONF2 0x30
#define REG_IDCODE 0x70
#define REG_VERSION 0x71
#define REG_EEPROMS 0x80


// Encoder configuration bit. Use with GCONF
#define FLOAT_DATA 0x0001
#define INT_DATA 0x0000
#define WRAP_ENABLE 0x0002
#define WRAP_DISABLE 0x0000
#define DIRE_LEFT 0x0004
#define DIRE_RIGHT 0x0000
#define IPUP_DISABLE 0x0008
#define IPUP_ENABLE 0x0000
#define RMOD_X2 0x0010
#define RMOD_X1 0x0000
#define RGB_ENCODER 0x0020
#define STD_ENCODER 0x0000
#define EEPROM_BANK1 0x0040
#define EEPROM_BANK2 0x0000
#define RESET 0x0080
#define CLK_STRECH_ENABLE 0x0100
#define CLK_STRECH_DISABLE 0x0000
#define REL_MODE_ENABLE 0x0200
#define REL_MODE_DISABLE 0x0000

// Encoder status bits and setting. Use with: INTCONF for set and with ESTATUS for read the bits
#define PUSHR 0x01
#define PUSHP 0x02
#define PUSHD 0x04
#define RINC 0x08
#define RDEC 0x10
#define RMAX 0x20
#define RMIN 0x40
#define INT_2 0x80

// Encoder Int2 bits. Use to read the bits of I2STATUS 
#define GP1_POS 0x01
#define GP1_NEG 0x02
#define GP2_POS 0x04
#define GP2_NEG 0x08
#define GP3_POS 0x10
#define GP3_NEG 0x20
#define FADE_INT 0x40

// Encoder Fade status bits. Use to read the bits of FSTATUS 
#define FADE_R 0x01
#define FADE_G 0x02
#define FADE_B 0x04
#define FADE_GP1 0x08
#define FADE_GP2 0x10
#define FADE_GP3 0x20

// GPIO Configuration. USe with GP1CONF,GP2CONF,GP3CONF
#define GP_PWM 0x00
#define GP_OUT 0x01
#define GP_AN 0x02
#define GP_IN 0x03

#define GP_PULL_EN 0x04
#define GP_PULL_DI 0x00

#define GP_INT_DI 0x00
#define GP_INT_PE 0x08
#define GP_INT_NE 0x10
#define GP_INT_BE 0x18

// Gamma configuration

#define GAMMA_OFF 0
#define GAMMA_1 1,
#define GAMMA_1_8 2
#define GAMMA_2 3
#define GAMMA_2_2 4
#define GAMMA_2_4 5
#define GAMMA_2_6 6
#define GAMMA_2_8 7

enum {
  ENCODER_NO_ACTION=0,
  ENCODER_AF_GAIN,
  ENCODER_AF_GAIN_RX1,
  ENCODER_AF_GAIN_RX2,
  ENCODER_AGC_GAIN,
  ENCODER_AGC_GAIN_RX1,
  ENCODER_AGC_GAIN_RX2,
  ENCODER_ATTENUATION,
  ENCODER_COMP,
  ENCODER_CW_FREQUENCY,
  ENCODER_CW_SPEED,
  ENCODER_DIVERSITY_GAIN,
  ENCODER_DIVERSITY_GAIN_COARSE,
  ENCODER_DIVERSITY_GAIN_FINE,
  ENCODER_DIVERSITY_PHASE,
  ENCODER_DIVERSITY_PHASE_COARSE,
  ENCODER_DIVERSITY_PHASE_FINE,
  ENCODER_DRIVE,
  ENCODER_IF_SHIFT,
  ENCODER_IF_SHIFT_RX1,
  ENCODER_IF_SHIFT_RX2,
  ENCODER_IF_WIDTH,
  ENCODER_IF_WIDTH_RX1,
  ENCODER_IF_WIDTH_RX2,
  ENCODER_MIC_GAIN,
  ENCODER_PAN,
  ENCODER_PANADAPTER_HIGH,
  ENCODER_PANADAPTER_LOW,
  ENCODER_PANADAPTER_STEP,
  ENCODER_RF_GAIN,
  ENCODER_RF_GAIN_RX1,
  ENCODER_RF_GAIN_RX2,
  ENCODER_RIT,
  ENCODER_RIT_RX1,
  ENCODER_RIT_RX2,
  ENCODER_SQUELCH,
  ENCODER_SQUELCH_RX1,
  ENCODER_SQUELCH_RX2,
  ENCODER_TUNE_DRIVE,
  ENCODER_VFO,
  ENCODER_WATERFALL_HIGH,
  ENCODER_WATERFALL_LOW,
  ENCODER_XIT,
  ENCODER_ZOOM,
  ENCODER_ACTIONS
};

extern char *encoder_string[ENCODER_ACTIONS];

enum {
  NO_ACTION=0,
  A_TO_B,
  A_SWAP_B,
  AGC,
  ANF,
  B_TO_A,
  BAND_MINUS,
  BAND_PLUS,
  BANDSTACK_MINUS,
  BANDSTACK_PLUS,
  CTUN,
  DIVERSITY,
  FILTER_MINUS,
  FILTER_PLUS,
  FUNCTION,
  LOCK,
  MENU_BAND,
  MENU_BANDSTACK,
  MENU_DIVERSITY,
  MENU_FILTER,
  MENU_FREQUENCY,
  MENU_MEMORY,
  MENU_MODE,
  MENU_PS,
  MODE_MINUS,
  MODE_PLUS,
  MOX,
  MUTE,
  NB,
  NR,
  PAN_MINUS,
  PAN_PLUS,
  PS,
  RIT,
  RIT_CLEAR,
  SAT,
  SNB,
  SPLIT,
  TUNE,
  TWO_TONE,
  XIT,
  XIT_CLEAR,
  ZOOM_MINUS,
  ZOOM_PLUS,
  SWITCH_ACTIONS
};

extern char *sw_string[SWITCH_ACTIONS];

typedef struct _encoder {
  gboolean enabled;
  gint address;
  gint pos;
  gint encoder_function;
  gboolean push_sw_enabled;
  gint push_sw_function;
  gboolean gp1_enabled; 
  gint gp1_function;
  gboolean gp2_enabled; 
  gint gp2_function;
  gboolean gp3_enabled; 
  gint gp3_function;
} ENCODER;

#define MAX_ENCODERS 7

extern ENCODER encoder[MAX_ENCODERS];

extern int i2c_controller_init();
#endif
