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

#ifndef _BAND_H
#define _BAND_H

#include <gtk/gtk.h>
#include "bandstack.h"

#define band160 0
#define band80 1
#define band60 2
#define band40 3
#define band30 4
#define band20 5
#define band17 6
#define band15 7
#define band12 8
#define band10 9
#define band6 10
#ifdef SOAPYSDR
#define band70 11
#define band220 13
#define band430 14
#define band902 15
#define band1240 16
#define band2300 17
#define band3400 18
#define bandAIR 19
#define bandGen 20
#define bandWWV 21
#define band136 22
#define band472 23
#define BANDS 24
#else
#define bandGen 11
#define bandWWV 12
#define band136 13
#define band472 14
#define BANDS 15
#endif

#define XVTRS 8

/* --------------------------------------------------------------------------*/
/**
* @brief Band definition
*/
struct _BAND {
    char title[16];
    BANDSTACK *bandstack;
    unsigned char OCrx;
    unsigned char OCtx;
    int preamp;
    int alexRxAntenna;
    int alexTxAntenna;
    int alexAttenuation;
    double pa_calibration;
    long long frequencyMin;
    long long frequencyMax;
    long long frequencyLO;
    long long errorLO;
    long long txFrequencyLO;
    long long txErrorLO;
    int disablePA;
};

typedef struct _BAND BAND;

struct _CHANNEL {
    long long frequency;
    long long width;
};

typedef struct _CHANNEL CHANNEL;



int band;
gboolean displayHF;

#define UK_CHANNEL_ENTRIES 11
#define OTHER_CHANNEL_ENTRIES 5
#define WRC15_CHANNEL_ENTRIES 3

extern int channel_entries;
extern CHANNEL *band_channels_60m;

extern CHANNEL band_channels_60m_UK[UK_CHANNEL_ENTRIES];
extern CHANNEL band_channels_60m_OTHER[OTHER_CHANNEL_ENTRIES];
extern CHANNEL band_channels_60m_WRC15[WRC15_CHANNEL_ENTRIES];

extern BANDSTACK bandstack60;
extern BANDSTACK_ENTRY bandstack_entries60_OTHER[];
extern BANDSTACK_ENTRY bandstack_entries60_WRC15[];
extern BANDSTACK_ENTRY bandstack_entries60_UK[];

extern int band_get_current();
extern BAND *band_get_current_band();
extern BAND *band_get_band(int b);
extern BAND *band_set_current(int b);
extern int get_band_from_frequency(long long f);

extern BANDSTACK *bandstack_get_bandstack(int band);
extern BANDSTACK_ENTRY *bandstack_get_bandstack_entry(int band,int entry);

extern BANDSTACK_ENTRY *bandstack_entry_next();
extern BANDSTACK_ENTRY *bandstack_entry_previous();
extern BANDSTACK_ENTRY *bandstack_entry_get_current();

extern void bandSaveState();
extern void bandRestoreState();

#endif
