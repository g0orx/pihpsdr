/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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

#include <gtk/gtk.h>

#include "radio.h"
#include "transmitter.h"
#include "vox.h"
#include "vfo.h"
#include "ext.h"

static guint vox_timeout;
static guint trigger_timeout = -1;

static double peak=0.0;
static int cwvox=0;

static int vox_timeout_cb(gpointer data) {
  //setVox(0);
  g_idle_add(ext_vox_changed,(gpointer)0);
  g_idle_add(ext_vfo_update,NULL);
  return FALSE;
}

static int cwvox_timeout_cb(gpointer data) {
  cwvox=0;
  trigger_timeout=-1;
  g_idle_add(ext_vox_changed,(gpointer)0);
  g_idle_add(ext_cw_key     ,(gpointer)0);
  g_idle_add(ext_vfo_update,NULL);
  return FALSE;
}


double vox_get_peak() {
  double result=peak;
  return result;
}

void update_vox(TRANSMITTER *tx) {
  // calculate peak microphone input
  // assumes it is interleaved left and right channel with length samples
  int previous_vox=vox;
  int i;
  double sample;
  peak=0.0;

  if (cwvox) return;  // do not set peak and fiddle around with VOX when in local CW mode

  for(i=0;i<tx->buffer_size;i++) {
    sample=tx->mic_input_buffer[i*2];
    if(sample<0.0) {
      sample=-sample;
    }
    if(sample>peak) {
      peak=sample;
    }
  }

  if(vox_enabled) {
    if(peak>vox_threshold) {
      if(previous_vox) {
        g_source_remove(vox_timeout);
      } else {
        g_idle_add(ext_vox_changed,(gpointer)1);
      }
      vox_timeout=g_timeout_add((int)vox_hang,vox_timeout_cb,NULL);
    }
    if(vox!=previous_vox) {
      g_idle_add(ext_vfo_update,NULL);
    }
  }
}

// vox_trigger activates MOX, vox_enabled does not matter here
// if VOX this is already pending, 
void vox_trigger(int lead_in) {

  // delete any pending hang timers
  if (trigger_timeout >= 0) g_source_remove(trigger_timeout);

  if (!cwvox) {
    cwvox=1;
    g_idle_add(ext_vox_changed,(gpointer)1);
    g_idle_add(ext_cw_key     ,(gpointer)1);
    g_idle_add(ext_vfo_update,NULL);
    usleep((long)lead_in*1000L);  // lead-in time is in ms
  }
}

// vox_untrigger actives the vox hang timer for cwvox
// so an immediately following "KY" CAT command will
// just proceed.
void vox_untrigger() {
  trigger_timeout=g_timeout_add((int)vox_hang,cwvox_timeout_cb,NULL);
}

void vox_cancel() {
  if(vox) {
    g_source_remove(vox_timeout);
    //setVox(0);
    g_idle_add(ext_vox_changed,(gpointer)0);
    g_idle_add(ext_vfo_update,NULL);
  }
}
