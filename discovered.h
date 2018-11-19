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

#ifndef _DISCOVERED_H
#define _DISCOVERED_H

#include <netinet/in.h>
#ifdef LIMESDR
#include <SoapySDR/Device.h>
#endif

#define MAX_DEVICES 16


#define DEVICE_METIS 0
#define DEVICE_HERMES 1
#define DEVICE_GRIFFIN 2
#define DEVICE_ANGELIA 4
#define DEVICE_ORION 5
#define DEVICE_HERMES_LITE 6
// 8000DLE uses 10 as the device type in old protocol
#define DEVICE_ORION2 10 
// Newer STEMlab hpsdr emulators use 100 instead of 1
#define DEVICE_STEMLAB 100

#ifdef USBOZY
#define DEVICE_OZY 7
#endif

#define NEW_DEVICE_ATLAS 0
#define NEW_DEVICE_HERMES 1
#define NEW_DEVICE_HERMES2 2
#define NEW_DEVICE_ANGELIA 3
#define NEW_DEVICE_ORION 4
#define NEW_DEVICE_ORION2 5
#define NEW_DEVICE_HERMES_LITE 6

#ifdef LIMESDR
#define LIMESDR_USB_DEVICE 0
#endif

#define STATE_AVAILABLE 2
#define STATE_SENDING 3

#define ORIGINAL_PROTOCOL 0
#define NEW_PROTOCOL 1
#ifdef LIMESDR
#define LIMESDR_PROTOCOL 2
#endif

#ifdef REMOTE
#define REMOTE_PROTOCOL 4
#endif
#ifdef STEMLAB_DISCOVERY
// A STEMlab discovered via Avahi will have this protocol until the SDR
// application itself is started, at which point it will be changed to the old
// protocol and proceed to be handled just like a normal HPSDR radio.
#define STEMLAB_PROTOCOL 5
// Since there are multiple HPSDR applications for the STEMlab, but not all
// are always installed, we need to keep track of which are installed, so the
// user can choose which one should be started.
// The software version field will be abused for this purpose
#define STEMLAB_PAVEL_RX 1
#define STEMLAB_PAVEL_TRX 2
#define STEMLAB_RP_TRX 4
#define HAMLAB_RP_TRX 8
#endif


struct _DISCOVERED {
    int protocol;
    int device;
    char name[64];
    int software_version;
    int status;
    union {
      struct network {
        unsigned char mac_address[6];
        int address_length;
        struct sockaddr_in address;
        int interface_length;
        struct sockaddr_in interface_address;
        struct sockaddr_in interface_netmask;
        char interface_name[64];
      } network;
#ifdef LIMESDR
      struct soapy {
        SoapySDRKwargs *args;
      } soapy;
#endif
    } info;
};

typedef struct _DISCOVERED DISCOVERED;

extern int selected_device;
extern int devices;
extern DISCOVERED discovered[MAX_DEVICES];

#endif
