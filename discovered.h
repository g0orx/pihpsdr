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

#include <netinet/in.h>

#define MAX_DEVICES 16

#define DEVICE_METIS 0
#define DEVICE_HERMES 1
#define DEVICE_GRIFFIN 2
#define DEVICE_ANGELIA 4
#define DEVICE_ORION 5
#define DEVICE_HERMES_LITE 6

#define NEW_DEVICE_ATLAS 0
#define NEW_DEVICE_HERMES 1
#define NEW_DEVICE_HERMES2 2
#define NEW_DEVICE_ANGELIA 3
#define NEW_DEVICE_ORION 4
#define NEW_DEVICE_ORION2 5
#define NEW_DEVICE_HERMES_LITE 6

#define STATE_AVAILABLE 2
#define STATE_SENDING 3

#define ORIGINAL_PROTOCOL 0
#define NEW_PROTOCOL 1

struct _DISCOVERED {
    int protocol;
    int device;
    char name[16];
    int software_version;
    unsigned char mac_address[6];
    int status;
    int address_length;
    struct sockaddr_in address;
    int interface_length;
    struct sockaddr_in interface_address;
    char interface_name[64];
};

typedef struct _DISCOVERED DISCOVERED;

extern int selected_device;
extern int devices;
extern DISCOVERED discovered[MAX_DEVICES];

