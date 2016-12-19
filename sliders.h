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

#ifndef _SLIDERS_H
#define _SLIDERS_H

extern void linein_changed();
extern int update_agc_gain(void *);
extern int update_af_gain(void *);
extern int update_mic_gain(void *);
extern int update_drive(void *);

void set_agc_gain(double value);
void set_af_gain(double value);
void set_mic_gain(double value);
void set_drive(double drive);
void set_tune(double tune);
void set_attenuation_value(double attenuation);
GtkWidget *sliders_init(int my_width, int my_height, GtkWidget* parent);

#endif
