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

#ifndef _PROPERTY_H
#define _PROPERTY_H

#define PROPERTY_VERSION 3.0

typedef struct _PROPERTY PROPERTY;

/* --------------------------------------------------------------------------*/
/**
* @brief Property structure
*/
struct _PROPERTY {
    char* name;
    char* value;
    PROPERTY* next_property;
};

extern void clearProperties();
extern void loadProperties(char* filename);
extern char* getProperty(char* name);
extern void setProperty(char* name,char* value);
extern void saveProperties(char* filename);

#endif
