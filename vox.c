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

static guint vox_timeout=0;

static double peak=0.0;

static int vox_timeout_cb(gpointer data) {
  //
  // First set vox_timeout to zero indicating no "hanging" timeout
  // then, remove VOX and update display
  //
  vox_timeout=0;
  g_idle_add(ext_vox_changed,(gpointer)0);
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
  int i;
  double sample;
  peak=0.0;
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
    if(peak > vox_threshold) {
      // we use the value of vox_timeout to determine whether
      // the time-out is "hanging". We cannot use the value of vox
      // since this may be set with a delay, and we MUST NOT miss
      // a "hanging" timeout. Note that if a time-out fires, vox_timeout
      // is set to zero.
      if(vox_timeout) {
        g_source_remove(vox_timeout);
      } else {
	//
	// no hanging time-out, assume that we just fired VOX
        g_idle_add(ext_vox_changed,(gpointer)1);
        g_idle_add(ext_vfo_update,NULL);
      }
      // re-init "vox hang" time
      vox_timeout=g_timeout_add((int)vox_hang,vox_timeout_cb,NULL);
    }
    // if peak is not above threshold, do nothing (this shall be done later in the timeout event
  }
}

//
// If no vox time-out is hanging, this function is a no-op
//
void vox_cancel() {
  if(vox_timeout) {
    g_source_remove(vox_timeout);
  }
}
