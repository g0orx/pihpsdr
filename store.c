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


/*
struct MEM {
    char title[16];     // Begin BAND Struct
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
    int disablePA;
    long long frequency; // Begin BANDSTACK_ENTRY
    int mode;
    int filter;
    int var1Low;
    int var1High;
    int var2Low;
    int var2High;
}*/
MEM mem[NUM_OF_MEMORYS];  // This makes it a compile time option

/*                                           */
/* Memory uses the same format as Band Stack */
/* Implement NUM_OF_MEMORYS memory locations for now... */

void memSaveState() {
    char name[128];
    char value[128];
    int b;

    for(b=0;b<NUM_OF_MEMORYS;b++) {
      if(strlen(mem[b].title)>0) {
        sprintf(name,"mem.%d.title",b);
        setProperty(name,mem[b].title);

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

    //sprintf(value,"%d",band);
    //setProperty("band",value);
}

void memRestoreState() {
    char* value;
    int b;
    char name[128];

    // Initialize the array with default values
    // Allows this to be a compile time option..
    for(b=0; b<NUM_OF_MEMORYS; b++) {
       strcpy(mem[b].title,"10");  
       mem[b].frequency = 28010000LL;
       mem[b].mode = modeCWU;
       mem[b].filter = filterF0;
    }

    fprintf(stderr,"memRestoreState: restore memory\n");

    for(b=0;b<NUM_OF_MEMORYS;b++) {
        sprintf(name,"mem.%d.title",b);
        value=getProperty(name);
        if(value) {
          strcpy(mem[b].title,value);
          fprintf(stderr,"RESTORE: index=%d title=%s\n",b,value);
	}

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

    //value=getProperty("band");
    //if(value) band=atoi(value);
}

