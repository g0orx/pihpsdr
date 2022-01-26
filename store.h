/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
* 2016 - Steve Wilson, KA6S
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

#ifndef _STORE_H
#define _STORE_H

#include <gtk/gtk.h>
#include "bandstack.h"


/* --------------------------------------------------------------------------*/
/**
* @brief Band definition
*/
struct _MEM_STORE {
    char title[16];     // Begin BAND Struct
    long long frequency; // Begin BANDSTACK_ENTRY
    int mode;
    int filter;
};

typedef struct _MEM_STORE MEM; 

extern MEM mem[];
void memRestoreState(void); 
void memSaveState(void); 

#endif
