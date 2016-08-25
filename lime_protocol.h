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

#ifndef _LIME_PROTOCOL_H
#define _LIME_PROTOCOL_H

#define BUFFER_SIZE 1024

void lime_protocol_init(int rx,int pixels);
void lime_protocol_stop();
void lime_protocol_set_frequency(long long f);
void lime_protocol_set_antenna(int ant);
void lime_protocol_set_attenuation(int attenuation);

#endif
