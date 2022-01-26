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
//
struct _mode_settings {
  int filter;
  int nb;
  int nb2;
  int nr;
  int nr2;
  int anf;
  int snb;
};

extern struct _mode_settings mode_settings[];

typedef struct _set_frequency {
  int vfo;
  long long frequency;
} SET_FREQUENCY;

#define STEPS 15
extern char *step_labels[];

extern GtkWidget* vfo_init(int width,int height,GtkWidget *parent);
extern int  vfo_get_stepindex(void);
extern void vfo_set_step_from_index(int index);
extern void vfo_set_stepsize(int newstep);
extern int  vfo_get_step_from_index(int index);
extern void vfo_step(int steps);
extern void vfo_id_step(int id, int steps);
extern void vfo_move(long long hz,int round);
extern void vfo_id_move(int id,long long hz,int round);
extern void vfo_move_to(long long hz);
extern void vfo_update(void);
extern void set_frequency(void);

extern void vfo_save_state(void);
extern void vfo_restore_state(void);
extern void modesettings_save_state(void);
extern void modesettings_restore_state(void);

extern void vfo_band_changed(int id,int b);
extern void vfo_bandstack_changed(int b);
extern void vfo_mode_changed(int m);
extern void vfo_filter_changed(int f);
extern void vfo_a_to_b(void);
extern void vfo_b_to_a(void);
extern void vfo_a_swap_b(void);

extern int get_tx_vfo(void);
extern int get_tx_mode(void);
extern long long get_tx_freq(void);

extern void vfo_xvtr_changed(void);

extern void vfo_rit_update(int rx);
extern void vfo_rit_clear(int rx);
extern void vfo_rit(int rx,int i);

#endif
