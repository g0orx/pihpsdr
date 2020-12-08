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

#define MAX_ENCODERS 5
#define MAX_SWITCHES 16
#define MAX_FUNCTIONS 6

enum {
  NO_CONTROLLER=0,
  CONTROLLER1,
  CONTROLLER2_V1,
  CONTROLLER2_V2,
};

extern int controller;
  
typedef struct _encoder {
  gboolean bottom_encoder_enabled;
  gboolean bottom_encoder_pullup;
  gint bottom_encoder_address_a;
  gint bottom_encoder_a_value;
  gint bottom_encoder_address_b;
  gint bottom_encoder_b_value;
  gint bottom_encoder_pos;
  gint bottom_encoder_function;
  gint top_encoder_enabled;
  gboolean top_encoder_pullup;
  gint top_encoder_address_a;
  gint top_encoder_a_value;
  gint top_encoder_address_b;
  gint top_encoder_b_value;
  gint top_encoder_pos;
  gint top_encoder_function;
  gboolean switch_enabled;
  gboolean switch_pullup;
  gint switch_address;
  gint switch_function;
} ENCODER;

extern ENCODER *encoders;

typedef struct _switch {
  gboolean switch_enabled;
  gboolean switch_pullup;
  gint switch_address;
  gint switch_function;
} SWITCH;

extern SWITCH switches_no_controller[MAX_SWITCHES];
extern SWITCH switches_controller1[MAX_FUNCTIONS][MAX_SWITCHES];
extern SWITCH switches_controller2_v1[MAX_SWITCHES];
extern SWITCH switches_controller2_v2[MAX_SWITCHES];

extern SWITCH *switches;

extern int *sw_action;

extern long settle_time;

extern int process_function_switch(void *data);
extern void gpio_set_defaults(int ctrlr);
extern void gpio_restore_actions();
extern void gpio_restore_state();
extern void gpio_save_state();
extern void gpio_save_actions();
extern int gpio_init();
extern void gpio_close();
#endif
