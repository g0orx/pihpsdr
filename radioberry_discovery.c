/* Copyright (C)
* 2017 - Johan Maas, PA3GSB
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

#include <stdio.h>
#include <string.h>
#include "discovered.h"
#include "radioberry_discovery.h"

void radioberry_discovery() {
  
  //check availability of RADIOBERRY for instance by si570 i2c address; 
  //for initial version when compiling with radioberry option; select it.
  
  
  // setting clock and loading rbf file..... done before starting
  
  discovered[devices].protocol=RADIOBERRY_PROTOCOL;
  discovered[devices].device=RADIOBERRY_SPI_DEVICE;
  strcpy(discovered[devices].name, "RadioBerry");
  discovered[devices].status=STATE_AVAILABLE;
  discovered[devices].software_version=100;
  devices++;
}
