/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
* 2016 - Steve Wilson, KA6S
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
#include <stdlib.h>
#include <string.h>

#include "bandstack.h"
#include "band.h"
#include "filter.h"
#include "mode.h"
#include "alex.h"
#include "property.h"
#include "store.h"
#include "store_menu.h"
#include "radio.h"
#include "ext.h"
#include "vfo.h"

MEM mem[NUM_OF_MEMORYS];  // This makes it a compile time option

void memSaveState() {
    char name[128];
    char value[128];
    int b;

    for(b=0;b<NUM_OF_MEMORYS;b++) {
      sprintf(value,"%lld",mem[b].frequency);
      sprintf(name,"mem.%d.freqA",b);
      setProperty(name,value);

      sprintf(value,"%d",mem[b].mode);
      sprintf(name,"mem.%d.mode",b);
      setProperty(name,value);

      sprintf(value,"%d",mem[b].filter);
      sprintf(name,"mem.%d.filter",b);
      setProperty(name,value);
    }
}

void memRestoreState() {
    char* value;
    int b;
    char name[128];

    // Initialize the array with default values
    // Allows this to be a compile time option..
    for(b=0; b<NUM_OF_MEMORYS; b++) {
       mem[b].frequency = 28010000LL;
       mem[b].mode = modeCWU;
       mem[b].filter = filterF0;
    }

    fprintf(stderr,"memRestoreState: restore memory\n");

    for(b=0;b<NUM_OF_MEMORYS;b++) {
      sprintf(name,"mem.%d.freqA",b);
      value=getProperty(name);
      if(value) {
         mem[b].frequency=atoll(value);
         fprintf(stderr,"RESTORE MEM:Mem %d=FreqA %11lld\n",b,mem[b].frequency);
      }

      sprintf(name,"mem.%d.mode",b);
      value=getProperty(name);
      if(value) {
        mem[b].mode=atoi(value);
        fprintf(stderr,"RESTORE: index=%d mode=%d\n",b,mem[b].mode);
      }

      sprintf(name,"mem.%d.filter",b);
      value=getProperty(name);
      if(value) {
        mem[b].filter=atoi(value);
        fprintf(stderr,"RESTORE: index=%d filter=%d\n",b,mem[b].filter);
      }
    }
}

void recall_memory_slot(int index) {
    long long new_freq;
    int id=active_receiver->id;

    new_freq = mem[index].frequency;
    fprintf(stderr,"recall_select_cb: Index=%d\n",index);
    fprintf(stderr,"recall_select_cb: freqA=%11lld\n",new_freq);
    fprintf(stderr,"recall_select_cb: mode=%d\n",mem[index].mode);
    fprintf(stderr,"recall_select_cb: filter=%d\n",mem[index].filter);

    //
    // Recalling a memory slot is equivalent to the following actions
    //
    // a) set the new frequency via the "Freq" menu
    // b) set the new mode via the "Mode" menu
    // c) set the new filter via the "Filter" menu
    //
    // Step b) automatically restores the filter, noise reduction, and
    // equalizer settings stored with that mode
    //
    // Step c) will not only change the filter but also store the new setting
    // with that mode.
    //
    set_frequency(active_receiver->id, new_freq);
    vfo_mode_changed(mem[index].mode);
    vfo_filter_changed(mem[index].filter);
    g_idle_add(ext_vfo_update,NULL);
}

void store_memory_slot(int index) {
   char workstr[40];
   int id=active_receiver->id;

   //
   // Store current frequency, mode, and filter in slot #index
   //
   mem[index].frequency = vfo[id].frequency;
   mem[index].mode = vfo[id].mode;
   mem[index].filter=vfo[id].filter;

   fprintf(stderr,"store_select_cb: Index=%d\n",index);
   fprintf(stderr,"store_select_cb: freqA=%11lld\n",mem[index].frequency);
   fprintf(stderr,"store_select_cb: mode=%d\n",mem[index].mode);
   fprintf(stderr,"store_select_cb: filter=%d\n",mem[index].filter);

   memSaveState();
}

