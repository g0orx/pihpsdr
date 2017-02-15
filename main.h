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

#ifndef _MAIN_H
#define _MAIN_H

#define DISPLAY_INCREMENT (display_height/32)
#define MENU_HEIGHT (DISPLAY_INCREMENT*2)
#define MENU_WIDTH ((display_width/32)*3)
#define VFO_HEIGHT (DISPLAY_INCREMENT*4)
#define VFO_WIDTH (display_width-METER_WIDTH-MENU_WIDTH)
#define METER_HEIGHT (DISPLAY_INCREMENT*4)
#define METER_WIDTH ((display_width/32)*8)
#define PANADAPTER_HEIGHT (DISPLAY_INCREMENT*8)
#define SLIDERS_HEIGHT (DISPLAY_INCREMENT*6)
#define TOOLBAR_HEIGHT (DISPLAY_INCREMENT*2)
#define WATERFALL_HEIGHT (display_height-(VFO_HEIGHT+PANADAPTER_HEIGHT+SLIDERS_HEIGHT+TOOLBAR_HEIGHT))
#ifdef PSK
#define PSK_WATERFALL_HEIGHT (DISPLAY_INCREMENT*6)
#define PSK_HEIGHT (display_height-(VFO_HEIGHT+PSK_WATERFALL_HEIGHT+SLIDERS_HEIGHT+TOOLBAR_HEIGHT))
#endif

#include <sys/utsname.h>
extern struct utsname unameData;

extern gint display_width;
extern gint display_height;
extern gint full_screen;
extern GtkWidget *top_window;
extern GtkWidget *grid;
extern void status_text(char *text);

#endif
