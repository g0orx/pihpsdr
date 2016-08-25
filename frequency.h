/** 
* @file frequency.h
* @brief Header files for the frequency 
* @author John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
* @version 0.1
* @date 2009-04-11
*/

/* Copyright (C) 
* 2009 - John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
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

#ifndef _FREQUENCY_H
#define _FREQUENCY_H

/* --------------------------------------------------------------------------*/
/** 
* @brief Frequency information 
*/
struct frequency_info {
        long long minFrequency;
        long long maxFrequency;
        char* info;
        int band;
        int transmit;
    };


char* getFrequencyInfo(long long frequency);
int getBand(long long frequency);
int canTransmit();

#endif
