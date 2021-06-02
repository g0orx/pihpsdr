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

#ifndef _TOOLBAR_H
#define _TOOLBAR_H

#include "gpio.h"
#define MAX_FUNCTION 5

extern int function;

void update_toolbar_labels();
void ptt_update(int state);
void mox_update(int state);
void tune_update(int state);
void tune_cb(GtkWidget *widget, gpointer data);

void sim_mox_cb(GtkWidget *widget, gpointer data);
void sim_s1_pressed_cb(GtkWidget *widget, gpointer data);
void sim_s1_released_cb(GtkWidget *widget, gpointer data);
void sim_s2_pressed_cb(GtkWidget *widget, gpointer data);
void sim_s2_released_cb(GtkWidget *widget, gpointer data);
void sim_s3_pressed_cb(GtkWidget *widget, gpointer data);
void sim_s3_released_cb(GtkWidget *widget, gpointer data);
void sim_s4_pressed_cb(GtkWidget *widget, gpointer data);
void sim_s4_released_cb(GtkWidget *widget, gpointer data);
void sim_s5_pressed_cb(GtkWidget *widget, gpointer data);
void sim_s5_released_cb(GtkWidget *widget, gpointer data);
void sim_s6_pressed_cb(GtkWidget *widget, gpointer data);
void sim_s6_released_cb(GtkWidget *widget, gpointer data);
void sim_function_cb(GtkWidget *widget, gpointer data);

GtkWidget *toolbar_init(int my_width, int my_height, GtkWidget* parent);

#endif
