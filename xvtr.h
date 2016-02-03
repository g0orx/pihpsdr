/**
* @file xvtr.h
* @brief XVTR definition files
* @author John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
* @version 0.1
* @date 2009-04-11
*/
// xvtr.h

/* Copyright (C)
* This program is free software; you can redistribute it and/or2009 - John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
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

/* --------------------------------------------------------------------------*/
/**
* @brief XVTR definition
*/
struct _XVTR_ENTRY {
    char name[32];
    long long rxFrequency;
    long long rxFrequencyMin;
    long long rxFrequencyMax;
    long long rxFrequencyLO;
    long long txFrequency;
    long long txFrequencyMin;
    long long txFrequencyMax;
    long long txFrequencyLO;
    int fullDuplex;
    int mode;
    int filter;
    int var1Low;
    int var1High;
    int var2Low;
    int var2High;
    int step;
    int preamp;
    int alexRxAntenna;
    int alexTxAntenna;
    int alexAttenuation;
};

typedef struct _XVTR_ENTRY XVTR_ENTRY;

