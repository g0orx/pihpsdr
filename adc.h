/* Copyright (C)
* 2018 - John Melton, G0ORX/N6LYT
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

#ifndef ADC_H
#define ADC_H

#include <gtk/gtk.h>

enum {
  AUTOMATIC=0,
  MANUAL
};

enum {
  BYPASS=0,
  HPF_1_5,
  HPF_6_5,
  HPF_9_5,
  HPF_13,
  HPF_20
};

enum {
  LPF_160=0,
  LPF_80,
  LPF_60_40,
  LPF_30_20,
  LPF_17_15,
  LPF_12_10,
  LPF_6
};

enum {
  ANTENNA_1=0,
  ANTENNA_2,
  ANTENNA_3,
  ANTENNA_XVTR,
  ANTENNA_EXT1,
  ANTENNA_EXT2
};

typedef struct _adc {
  gint filters;
  gint hpf;
  gint lpf;
  gint antenna;
  gboolean dither;
  gboolean random;
  gboolean preamp;
  gint attenuation;
  gboolean enable_step_attenuation;
#ifdef SOAPYSDR
  gdouble gain;
  gdouble min_gain;
  gdouble max_gain;
  gboolean agc;
#endif
} ADC;

#endif
