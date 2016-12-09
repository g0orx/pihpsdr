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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bandstack.h"
#include "band.h"
#include "filter.h"
#include "mode.h"
#include "alex.h"
#include "property.h"

#define LINESDR

int band=band20;
int xvtr_band=BANDS;

/* --------------------------------------------------------------------------*/
/**
* @brief bandstack
*/
/* ----------------------------------------------------------------------------*/
BANDSTACK_ENTRY bandstack_entries136[] =
    {{135800,135800,modeCWL,filterF4,-2800,-200,-2800,-200},
     {137100,137100,modeCWL,filterF4,-2800,-200,-2800,-200}};

BANDSTACK_ENTRY bandstack_entries472[] =
    {{472100LL,472100LL,modeCWL,filterF4,-2800,-200,-2800,-200},
     {475100LL,475100LL,modeCWL,filterF4,-2800,-200,-2800,-200}};

BANDSTACK_ENTRY bandstack_entries160[] =
    {{1810000LL,1810000LL,modeCWL,filterF4,-2800,-200,-2800,-200},
     {1835000LL,1835000LL,modeCWU,filterF0,-2800,-200,-2800,-200},
     {1845000LL,1845000LL,modeUSB,filterF5,-2800,-200,-2800,-200}};

BANDSTACK_ENTRY bandstack_entries80[] =
    {{3501000LL,3501000LL,modeCWL,filterF0,-2800,-200,-2800,-200},
     {3751000LL,3751000LL,modeLSB,filterF5,-2800,-200,-2800,-200},
     {3850000LL,3850000LL,modeLSB,filterF5,-2800,-200,-2800,-200}};

BANDSTACK_ENTRY bandstack_entries60[] =
    {{5330500LL,5330500LL,modeUSB,filterF5,-2800,-200,-2800,-200},
     {5346500LL,5346500LL,modeUSB,filterF5,-2800,-200,-2800,-200},
     {5366500LL,5366500LL,modeUSB,filterF5,-2800,-200,-2800,-200},
     {5371500LL,5371500LL,modeUSB,filterF5,-2800,-200,-2800,-200},
     {5403500LL,5403500LL,modeUSB,filterF5,-2800,-200,-2800,-200}};

BANDSTACK_ENTRY bandstack_entries40[] =
    {{7001000LL,7001000LL,modeCWL,filterF0,-2800,-200,-2800,-200},
     {7152000LL,7152000LL,modeLSB,filterF5,-2800,-200,-2800,-200},
     {7255000LL,7255000LL,modeLSB,filterF5,-2800,-200,-2800,-200}};

BANDSTACK_ENTRY bandstack_entries30[] =
    {{10120000LL,10120000LL,modeCWU,filterF0,200,2800,200,2800},
     {10130000LL,10130000LL,modeCWU,filterF0,200,2800,200,2800},
     {10140000LL,10140000LL,modeCWU,filterF4,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries20[] =
    {{14010000LL,14010000LL,modeCWU,filterF0,200,2800,200,2800},
     {14150000LL,14010000LL,modeUSB,filterF5,200,2800,200,2800},
     {14230000LL,14010000LL,modeUSB,filterF5,200,2800,200,2800},
     {14336000LL,14010000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries18[] =
    {{18068600LL,18068600LL,modeCWU,filterF0,200,2800,200,2800},
     {18125000LL,18068600LL,modeUSB,filterF5,200,2800,200,2800},
     {18140000LL,18068600LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries15[] =
    {{21001000LL,21001000LL,modeCWU,filterF0,200,2800,200,2800},
     {21255000LL,21001000LL,modeUSB,filterF5,200,2800,200,2800},
     {21300000LL,21001000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries12[] =
    {{24895000LL,24895000LL,modeCWU,filterF0,200,2800,200,2800},
     {24900000LL,24895000LL,modeUSB,filterF5,200,2800,200,2800},
     {24910000LL,24895000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries10[] =
    {{28010000LL,28010000LL,modeCWU,filterF0,200,2800,200,2800},
     {28300000LL,28010000LL,modeUSB,filterF5,200,2800,200,2800},
     {28400000LL,28010000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries50[] =
    {{50010000LL,50010000LL,modeCWU,filterF0,200,2800,200,2800},
     {50125000LL,50010000LL,modeUSB,filterF5,200,2800,200,2800},
     {50200000LL,50010000LL,modeUSB,filterF5,200,2800,200,2800}};

#ifdef LIMESDR
BANDSTACK_ENTRY bandstack_entries70[] =
    {{70010000LL,70010000LL,modeCWU,filterF0,200,2800,200,2800},
     {70200000LL,70010000LL,modeUSB,filterF5,200,2800,200,2800},
     {70250000LL,70010000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries144[] =
    {{144010000LL,144010000LL,modeCWU,filterF0,200,2800,200,2800},
     {144200000LL,144010000LL,modeUSB,filterF5,200,2800,200,2800},
     {144250000LL,144010000LL,modeUSB,filterF5,200,2800,200,2800},
     {145600000LL,144010000LL,modeFMN,filterF1,200,2800,200,2800},
     {145725000LL,144010000LL,modeFMN,filterF1,200,2800,200,2800},
     {145900000LL,144010000LL,modeFMN,filterF1,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries220[] =
    {{220010000LL,220010000,modeCWU,filterF0,200,2800,200,2800},
     {220200000LL,220010000,modeUSB,filterF5,200,2800,200,2800},
     {220250000LL,220010000,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries430[] =
    {{430010000LL,430010000LL,modeCWU,filterF0,200,2800,200,2800},
     {432100000LL,430010000LL,modeUSB,filterF5,200,2800,200,2800},
     {432300000LL,430010000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries902[] =
    {{902010000LL,902010000LL,modeCWU,filterF0,200,2800,200,2800},
     {902100000LL,902010000LL,modeUSB,filterF5,200,2800,200,2800},
     {902300000LL,902010000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries1240[] =
    {{1240010000LL,1240010000LL,modeCWU,filterF0,200,2800,200,2800},
     {1240100000LL,1240010000LL,modeUSB,filterF5,200,2800,200,2800},
     {1240300000LL,1240010000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries2300[] =
    {{2300010000LL,2300010000LL,modeCWU,filterF0,200,2800,200,2800},
     {2300100000LL,2300010000LL,modeUSB,filterF5,200,2800,200,2800},
     {2300300000LL,2300010000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entries3400[] =
    {{3400010000LL,3400010000LL,modeCWU,filterF0,200,2800,200,2800},
     {3400100000LL,3400010000LL,modeUSB,filterF5,200,2800,200,2800},
     {3400300000LL,3400010000LL,modeUSB,filterF5,200,2800,200,2800}};

BANDSTACK_ENTRY bandstack_entriesAIR[] =
    {{118800000LL,118800000LL,modeAM,filterF1,200,2800,200,2800},
     {120000000LL,118800000LL,modeAM,filterF1,200,2800,200,2800},
     {121700000LL,118800000LL,modeAM,filterF1,200,2800,200,2800},
     {124100000LL,118800000LL,modeAM,filterF1,200,2800,200,2800},
     {126600000LL,118800000LL,modeAM,filterF1,200,2800,200,2800},
     {136500000LL,118800000LL,modeAM,filterF1,200,2800,200,2800}};
#endif

BANDSTACK_ENTRY bandstack_entriesGEN[] =
    {{909000LL,909000LL,modeAM,filterF6,-6000,6000,-6000,60000},
     {5975000LL,909000LL,modeAM,filterF6,-6000,6000,-6000,60000},
     {13845000LL,909000LL,modeAM,filterF6,-6000,6000,-6000,60000}};

BANDSTACK_ENTRY bandstack_entriesWWV[] =
    {{2500000LL,2500000LL,modeSAM,filterF6,200,2800,200,2800},
     {5000000LL,2500000LL,modeSAM,filterF6,200,2800,200,2800},
     {10000000LL,2500000LL,modeSAM,filterF6,200,2800,200,2800},
     {15000000LL,2500000LL,modeSAM,filterF6,200,2800,200,2800},
     {20000000LL,2500000LL,modeSAM,filterF6,200,2800,200,2800}};

BANDSTACK bandstack160={3,1,bandstack_entries160};
BANDSTACK bandstack80={3,1,bandstack_entries80};
BANDSTACK bandstack60={5,1,bandstack_entries60};
BANDSTACK bandstack40={3,1,bandstack_entries40};
BANDSTACK bandstack30={3,1,bandstack_entries30};
BANDSTACK bandstack20={4,1,bandstack_entries20};
BANDSTACK bandstack18={3,1,bandstack_entries18};
BANDSTACK bandstack15={3,1,bandstack_entries15};
BANDSTACK bandstack12={3,1,bandstack_entries12};
BANDSTACK bandstack10={3,1,bandstack_entries10};
BANDSTACK bandstack50={3,1,bandstack_entries50};
#ifdef LIMESDR
BANDSTACK bandstack70={3,1,bandstack_entries70};
BANDSTACK bandstack144={6,1,bandstack_entries144};
BANDSTACK bandstack220={3,1,bandstack_entries220};
BANDSTACK bandstack430={3,1,bandstack_entries430};
BANDSTACK bandstack902={3,1,bandstack_entries902};
BANDSTACK bandstack1240={3,1,bandstack_entries1240};
BANDSTACK bandstack2300={3,1,bandstack_entries2300};
BANDSTACK bandstack3400={3,1,bandstack_entries3400};
BANDSTACK bandstackAIR={6,1,bandstack_entriesAIR};
#endif
BANDSTACK bandstackGEN={3,1,bandstack_entriesGEN};
BANDSTACK bandstackWWV={5,1,bandstack_entriesWWV};
BANDSTACK bandstack136={2,0,bandstack_entries136};
BANDSTACK bandstack472={2,0,bandstack_entries472};

/* --------------------------------------------------------------------------*/
/**
* @brief xvtr
*/
/* ----------------------------------------------------------------------------*/

BANDSTACK_ENTRY bandstack_entries_xvtr_0[] =
    {{0LL,modeUSB,filterF6,150,2550,150,2550}};
BANDSTACK_ENTRY bandstack_entries_xvtr_1[] =
    {{0LL,modeUSB,filterF6,150,2550,150,2550}};
BANDSTACK_ENTRY bandstack_entries_xvtr_2[] =
    {{0LL,modeUSB,filterF6,150,2550,150,2550}};
BANDSTACK_ENTRY bandstack_entries_xvtr_3[] =
    {{0LL,modeUSB,filterF6,150,2550,150,2550}};
BANDSTACK_ENTRY bandstack_entries_xvtr_4[] =
    {{0LL,modeUSB,filterF6,150,2550,150,2550}};
BANDSTACK_ENTRY bandstack_entries_xvtr_5[] =
    {{0LL,modeUSB,filterF6,150,2550,150,2550}};
BANDSTACK_ENTRY bandstack_entries_xvtr_6[] =
    {{0LL,modeUSB,filterF6,150,2550,150,2550}};
BANDSTACK_ENTRY bandstack_entries_xvtr_7[] =
    {{0LL,modeUSB,filterF6,150,2550,150,2550}};

BANDSTACK bandstack_xvtr_0={1,0,bandstack_entries_xvtr_0};
BANDSTACK bandstack_xvtr_1={1,0,bandstack_entries_xvtr_1};
BANDSTACK bandstack_xvtr_2={1,0,bandstack_entries_xvtr_2};
BANDSTACK bandstack_xvtr_3={1,0,bandstack_entries_xvtr_3};
BANDSTACK bandstack_xvtr_4={1,0,bandstack_entries_xvtr_4};
BANDSTACK bandstack_xvtr_5={1,0,bandstack_entries_xvtr_5};
BANDSTACK bandstack_xvtr_6={1,0,bandstack_entries_xvtr_6};
BANDSTACK bandstack_xvtr_7={1,0,bandstack_entries_xvtr_7};



BAND bands[BANDS+XVTRS] = 
    {{"160",&bandstack160,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,1800000LL,1800000LL,0LL,0},
     {"80",&bandstack80,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,3500000LL,3500000LL,0LL,0},
     {"60",&bandstack60,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,5330500LL,5403500LL,0LL,0},
     {"40",&bandstack40,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,7000000LL,7300000LL,0LL,0},
     {"30",&bandstack30,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,10100000LL,10150000LL,0LL,0},
     {"20",&bandstack20,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,14000000LL,14350000LL,0LL,0},
     {"18",&bandstack18,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,18068000LL,18168000LL,0LL,0},
     {"15",&bandstack15,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,21000000LL,21450000LL,0LL,0},
     {"12",&bandstack12,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,24890000LL,24990000LL,0LL,0},
     {"10",&bandstack10,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,28000000LL,29700000LL,0LL,0},
     {"50",&bandstack50,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,50000000LL,54000000LL,0LL,0},
#ifdef LIMESDR
     {"70",&bandstack70,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"144",&bandstack144,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,144000000LL,148000000LL,0LL,0},
     {"220",&bandstack144,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,222000000LL,224980000LL,0LL,0},
     {"430",&bandstack430,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,420000000LL,450000000LL,0LL,0},
     {"902",&bandstack430,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,902000000LL,928000000LL,0LL,0},
     {"1240",&bandstack1240,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,1240000000LL,1300000000LL,0LL,0},
     {"2300",&bandstack2300,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,2300000000LL,2450000000LL,0LL,0},
     {"3400",&bandstack3400,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,3400000000LL,3410000000LL,0LL,0},
     {"AIR",&bandstackAIR,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
#endif
     {"GEN",&bandstackGEN,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"WWV",&bandstackWWV,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"136kHz",&bandstack136,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,135700LL,137800LL,0LL,0},
     {"472kHz",&bandstack472,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,472000LL,479000LL,0LL,0},
// XVTRS
     {"",&bandstack_xvtr_0,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"",&bandstack_xvtr_1,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"",&bandstack_xvtr_2,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"",&bandstack_xvtr_3,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"",&bandstack_xvtr_4,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"",&bandstack_xvtr_5,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"",&bandstack_xvtr_6,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0},
     {"",&bandstack_xvtr_7,0,0,0,0,0,ALEX_ATTENUATION_0dB,53.0,0LL,0LL,0LL,0}
    };


BANDSTACK_ENTRY *bandstack_entry_get_current() {
    BANDSTACK *bandstack=bands[band].bandstack;
    BANDSTACK_ENTRY *entry=&bandstack->entry[bandstack->current_entry];
    return entry;
}

BANDSTACK_ENTRY *bandstack_entry_next() {
    BANDSTACK *bandstack=bands[band].bandstack;
    bandstack->current_entry++;
    if(bandstack->current_entry>=bandstack->entries) {
        bandstack->current_entry=0;
    }
    BANDSTACK_ENTRY *entry=&bandstack->entry[bandstack->current_entry];
    return entry;
}

BANDSTACK_ENTRY *bandstack_entry_previous() {
    BANDSTACK *bandstack=bands[band].bandstack;
    bandstack->current_entry--;
    if(bandstack->current_entry<0) {
        bandstack->current_entry=bandstack->entries-1;
    }
    BANDSTACK_ENTRY *entry=&bandstack->entry[bandstack->current_entry];
    return entry;
}


int band_get_current() {
    return band;
}

BAND *band_get_current_band() {
    BAND *b=&bands[band];
    return b;
}

BAND *band_get_band(int b) {
    BAND *band=&bands[b];
    return band;
}

BAND *band_set_current(int b) {
    band=b;
    return &bands[b];
}

void bandSaveState() {
    char name[128];
    char value[128];
    int current;
    BANDSTACK_ENTRY* entry;

    int b;
    int stack;
    for(b=0;b<BANDS+XVTRS;b++) {
      if(strlen(bands[b].title)>0) {
        sprintf(name,"band.%d.title",b);
        setProperty(name,bands[b].title);

        sprintf(value,"%d",bands[b].bandstack->entries);
        sprintf(name,"band.%d.entries",b);
        setProperty(name,value);

        sprintf(value,"%d",bands[b].bandstack->current_entry);
        sprintf(name,"band.%d.current",b);
        setProperty(name,value);

        sprintf(value,"%d",bands[b].preamp);
        sprintf(name,"band.%d.preamp",b);
        setProperty(name,value);

        sprintf(value,"%d",bands[b].alexRxAntenna);
        sprintf(name,"band.%d.alexRxAntenna",b);
        setProperty(name,value);

        sprintf(value,"%d",bands[b].alexTxAntenna);
        sprintf(name,"band.%d.alexTxAntenna",b);
        sprintf(name,"band.%d.alexTxAntenna",b);
        setProperty(name,value);

        sprintf(value,"%d",bands[b].alexAttenuation);
        sprintf(name,"band.%d.alexAttenuation",b);
        setProperty(name,value);

        sprintf(value,"%f",bands[b].pa_calibration);
        sprintf(name,"band.%d.pa_calibration",b);
        setProperty(name,value);

        sprintf(value,"%d",bands[b].OCrx);
        sprintf(name,"band.%d.OCrx",b);
        setProperty(name,value);

        sprintf(value,"%d",bands[b].OCtx);
        sprintf(name,"band.%d.OCtx",b);
        setProperty(name,value);

        sprintf(value,"%lld",bands[b].frequencyMin);
        sprintf(name,"band.%d.frequencyMin",b);
        setProperty(name,value);

        sprintf(value,"%lld",bands[b].frequencyMax);
        sprintf(name,"band.%d.frequencyMax",b);
        setProperty(name,value);

        sprintf(value,"%lld",bands[b].frequencyLO);
        sprintf(name,"band.%d.frequencyLO",b);
        setProperty(name,value);

        sprintf(value,"%d",bands[b].disablePA);
        sprintf(name,"band.%d.disablePA",b);
        setProperty(name,value);

        for(stack=0;stack<bands[b].bandstack->entries;stack++) {
            entry=bands[b].bandstack->entry;
            entry+=stack;

            sprintf(value,"%lld",entry->frequencyA);
            sprintf(name,"band.%d.stack.%d.a",b,stack);
            setProperty(name,value);

            sprintf(value,"%d",entry->mode);
            sprintf(name,"band.%d.stack.%d.mode",b,stack);
            setProperty(name,value);

            sprintf(value,"%d",entry->filter);
            sprintf(name,"band.%d.stack.%d.filter",b,stack);
            setProperty(name,value);

            sprintf(value,"%d",entry->var1Low);
            sprintf(name,"band.%d.stack.%d.var1Low",b,stack);
            setProperty(name,value);

            sprintf(value,"%d",entry->var1High);
            sprintf(name,"band.%d.stack.%d.var1High",b,stack);
            setProperty(name,value);

            sprintf(value,"%d",entry->var2Low);
            sprintf(name,"band.%d.stack.%d.var2Low",b,stack);
            setProperty(name,value);

            sprintf(value,"%d",entry->var2High);
            sprintf(name,"band.%d.stack.%d.var2High",b,stack);
            setProperty(name,value);

        }
      }
    }

    sprintf(value,"%d",band);
    setProperty("band",value);
}

void bandRestoreState() {
    char* value;
    int b;
    int stack;
    char name[128];
    BANDSTACK_ENTRY* entry;
    int current;

fprintf(stderr,"bandRestoreState: restore bands\n");

    for(b=0;b<BANDS+XVTRS;b++) {
        sprintf(name,"band.%d.title",b);
        value=getProperty(name);
        if(value) strcpy(bands[b].title,value);

        sprintf(name,"band.%d.entries",b);
        value=getProperty(name);
        if(value) bands[b].bandstack->entries=atoi(value);

        sprintf(name,"band.%d.current",b);
        value=getProperty(name);
        if(value) bands[b].bandstack->current_entry=atoi(value);

        sprintf(name,"band.%d.preamp",b);
        value=getProperty(name);
        if(value) bands[b].preamp=atoi(value);

        sprintf(name,"band.%d.alexRxAntenna",b);
        value=getProperty(name);
        if(value) bands[b].alexRxAntenna=atoi(value);

        sprintf(name,"band.%d.alexTxAntenna",b);
        value=getProperty(name);
        if(value) bands[b].alexTxAntenna=atoi(value);

// fix bug so props file does not have to be deleted
        if(bands[b].alexTxAntenna>2) bands[b].alexTxAntenna=0;

        sprintf(name,"band.%d.alexAttenuation",b);
        value=getProperty(name);
        if(value) bands[b].alexAttenuation=atoi(value);

        sprintf(name,"band.%d.pa_calibration",b);
        value=getProperty(name);
        if(value) {
          bands[b].pa_calibration=strtod(value,NULL);
          if(bands[b].pa_calibration<38.8 || bands[b].pa_calibration>100.0) {
            bands[b].pa_calibration=38.8;
          }
        }

        sprintf(name,"band.%d.OCrx",b);
        value=getProperty(name);
        if(value) bands[b].OCrx=atoi(value);

        sprintf(name,"band.%d.OCtx",b);
        value=getProperty(name);
        if(value) bands[b].OCtx=atoi(value);

        sprintf(name,"band.%d.frequencyMin",b);
        value=getProperty(name);
        if(value) bands[b].frequencyMin=atoll(value);

        sprintf(name,"band.%d.frequencyMax",b);
        value=getProperty(name);
        if(value) bands[b].frequencyMax=atoll(value);

        sprintf(name,"band.%d.frequencyLO",b);
        value=getProperty(name);
        if(value) bands[b].frequencyLO=atoll(value);

        sprintf(name,"band.%d.disablePA",b);
        value=getProperty(name);
        if(value) bands[b].disablePA=atoi(value);

        for(stack=0;stack<bands[b].bandstack->entries;stack++) {
            entry=bands[b].bandstack->entry;
            entry+=stack;

            sprintf(name,"band.%d.stack.%d.a",b,stack);
            value=getProperty(name);
            if(value) entry->frequencyA=atoll(value);

            sprintf(name,"band.%d.stack.%d.mode",b,stack);
            value=getProperty(name);
            if(value) entry->mode=atoi(value);

            sprintf(name,"band.%d.stack.%d.filter",b,stack);
            value=getProperty(name);
            if(value) entry->filter=atoi(value);

            sprintf(name,"band.%d.stack.%d.var1Low",b,stack);
            value=getProperty(name);
            if(value) entry->var1Low=atoi(value);

            sprintf(name,"band.%d.stack.%d.var1High",b,stack);
            value=getProperty(name);
            if(value) entry->var1High=atoi(value);

            sprintf(name,"band.%d.stack.%d.var2Low",b,stack);
            value=getProperty(name);
            if(value) entry->var2Low=atoi(value);

            sprintf(name,"band.%d.stack.%d.var2High",b,stack);
            value=getProperty(name);
            if(value) entry->var2High=atoi(value);

        }
    }

    value=getProperty("band");
    if(value) band=atoi(value);
}

int get_band_from_frequency(long long f) {
  int b;
  int found=-1;
  for(b=0;b<BANDS+XVTRS;b++) {
    BAND *band=band_get_band(b);
    if(strlen(band->title)>0) {
      if(f>=band->frequencyMin && f<=band->frequencyMax) {
        found=b;
        break;
      }
    }
  }
  return found;
}
