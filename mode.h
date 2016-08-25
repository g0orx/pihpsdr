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

#ifndef _MODE_H
#define _MODE_H

#define modeLSB 0
#define modeUSB 1
#define modeDSB 2
#define modeCWL 3
#define modeCWU 4
#define modeFMN 5
#define modeAM 6
#define modeDIGU 7
#define modeSPEC 8
#define modeDIGL 9
#define modeSAM 10
#define modeDRM 11
#ifdef FREEDV
#ifdef PSK
#define modeFREEDV 12
#define modePSK 13
#define MODES 14
#else
#define modeFREEDV 12
#define MODES 13
#endif
#else
#ifdef PSK
#define modePSK 12
#define MODES 13
#else
#define MODES 12
#endif
#endif

int mode;

char *mode_string[MODES];

#endif
