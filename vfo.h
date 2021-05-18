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

#ifndef _VFO_H
#define _VFO_H

#include "mode.h"

enum {
  VFO_A=0,
  VFO_B,
  MAX_VFOS
};

struct _vfo {
  int band;
  int bandstack;
  long long frequency;
  int mode;
  int filter;

  int ctun;
  long long ctun_frequency;

  int rit_enabled;
  long long rit;

  long long lo;
  long long offset;

  gboolean entering_frequency;
  gint64 entered_frequency;

};

extern struct _vfo vfo[MAX_VFOS];

//
// Store filter and NR settings on a per-mode basis
// all elements are "on/off"
//
struct _mode_settings {
  int filter;
  int nb;            // Noise blanker #1
  int nb2;           // Noise blanker #2
  int nr;            // Noise reduction #1
  int nr2;           // Noise reduction #2
  int anf;           // Automatic notch filter
  int snb;           // Spectral noise blanker
  int en_txeq;       // TX equalizer on/off
  int en_rxeq;       // RX equalizer on/off
  int txeq[4];       // TX equalizer settings
  int rxeq[4];       // RX equalizer settings
};

extern struct _mode_settings mode_settings[];

typedef struct _set_frequency {
  int vfo;
  long long frequency;
} SET_FREQUENCY;

#define STEPS 15
extern int steps[];
extern char *step_labels[];

extern GtkWidget* vfo_init(int width,int height,GtkWidget *parent);
extern void vfo_step(int steps);
extern void vfo_id_step(int id, int steps);
extern void vfo_move(long long hz,int round);
extern void vfo_id_move(int id,long long hz,int round);
extern void vfo_move_to(long long hz);
extern void vfo_update();

extern void vfo_save_state();
extern void vfo_restore_state();
extern void modesettings_save_state();
extern void modesettings_restore_state();

extern void vfo_band_changed(int id,int b);
extern void vfo_bandstack_changed(int b);
extern void vfo_mode_changed(int m);
extern void vfo_filter_changed(int f);
extern void vfo_a_to_b();
extern void vfo_b_to_a();
extern void vfo_a_swap_b();

extern int get_tx_vfo();
extern int get_tx_mode();
extern long long get_tx_freq();

extern void vfo_xvtr_changed();

extern void vfo_rit_update(int rx);
extern void vfo_rit_clear(int rx);
extern void vfo_rit(int rx,int i);

#endif
