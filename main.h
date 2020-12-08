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

#include <sys/utsname.h>
extern struct utsname unameData;

enum {
  NO_CONTROLLER,
  CONTROLLER1,
  CONTROLLER2_V1,
  CONTROLLER2_V2,
  CONTROLLER_I2C
};

extern gint controller;

extern gint display_width;
extern gint display_height;
extern gint full_screen;
extern GtkWidget *top_window;
extern GtkWidget *grid;
extern void status_text(char *text);

extern gboolean keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
#endif
