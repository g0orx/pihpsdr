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

#define ALEX_RX_ANTENNA_NONE   0x00000000
#define ALEX_RX_ANTENNA_XVTR   0x00000900
#define ALEX_RX_ANTENNA_EXT1   0x00000A00
#define ALEX_RX_ANTENNA_EXT2   0x00000C00
#define ALEX_RX_ANTENNA_BYPASS 0x00000800

#define ALEX_TX_ANTENNA_1      0x01000000
#define ALEX_TX_ANTENNA_2      0x02000000
#define ALEX_TX_ANTENNA_3      0x04000000

#define ALEX_ATTENUATION_0dB   0x00000000
#define ALEX_ATTENUATION_10dB  0x00004000
#define ALEX_ATTENUATION_20dB  0x00002000
#define ALEX_ATTENUATION_30dB  0x00006000

#define ALEX_30_20_LPF         0x00100000
#define ALEX_60_40_LPF         0x00200000
#define ALEX_80_LPF            0x00400000
#define ALEX_160_LPF           0x00800000
#define ALEX_6_BYPASS_LPF      0x20000000
#define ALEX_12_10_LPF         0x40000000
#define ALEX_17_15_LPF         0x80000000

#define ALEX_13MHZ_HPF         0x00000002
#define ALEX_20MHZ_HPF         0x00000004
#define ALEX_9_5MHZ_HPF        0x00000010
#define ALEX_6_5MHZ_HPF        0x00000020
#define ALEX_1_5MHZ_HPF        0x00000040
#define ALEX_BYPASS_HPF        0x00000800

#define ALEX_6M_PREAMP         0x00000008
