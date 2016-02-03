#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "radio.h"
#include "agc.h"
#include "band.h"
#include "discovered.h"
#include "property.h"

char property_path[128];

int sample_rate=48000;
int filter_board=ALEX;
int pa=PA_ENABLED;
int apollo_tuner=0;

int panadapter_high=-80;
int panadapter_low=-160;

int waterfall_high=-100;
int waterfall_low=-150;
int waterfall_automatic=1;

double volume=0.2;
double mic_gain=1.5;

int orion_mic=MIC_BOOST|ORION_MIC_PTT_ENABLED|ORION_MIC_PTT_TIP_BIAS_RING|ORION_MIC_BIAS_ENABLED;

int agc=AGC_MEDIUM;
double agc_gain=60.0;

int nr=0;
int nb=0;
int anf=0;
int snb=0;

int cwPitch=600;

int tune_drive=6;
int drive=60;

int receivers=2;
int adc[2]={0,1};

int locked=0;

int step=100;

int byte_swap=0;

int cw_keys_reversed=0; // 0=disabled 1=enabled
int cw_keyer_speed=12; // 1-60 WPM
int cw_keyer_mode=KEYER_STRAIGHT;
int cw_keyer_weight=30; // 0-100
int cw_keyer_spacing=0; // 0=on 1=off
int cw_keyer_internal=1; // 0=external 1=internal
int cw_keyer_sidetone_volume=127; // 0-127
int cw_keyer_ptt_delay=20; // 0-255ms
int cw_keyer_hang_time=300; // ms
int cw_keyer_sidetone_frequency=400; // Hz
int cw_breakin=1; // 0=disabled 1=enabled

void radioRestoreState() {
    char *value;
    value=getProperty("sample_rate");
    if(value) sample_rate=atoi(value);
    value=getProperty("filter_board");
    if(value) filter_board=atoi(value);
    value=getProperty("apollo_tuner");
    if(value) apollo_tuner=atoi(value);
    value=getProperty("pa");
    if(value) pa=atoi(value);
    value=getProperty("panadapter_high");
    if(value) panadapter_high=atoi(value);
    value=getProperty("panadapter_low");
    if(value) panadapter_low=atoi(value);
    value=getProperty("waterfall_high");
    if(value) waterfall_high=atoi(value);
    value=getProperty("waterfall_low");
    if(value) waterfall_low=atoi(value);
    value=getProperty("waterfall_automatic");
    if(value) waterfall_automatic=atoi(value);
    value=getProperty("volume");
    if(value) volume=atof(value);
    value=getProperty("mic_gain");
    if(value) mic_gain=atof(value);
    value=getProperty("orion_mic");
    if(value) orion_mic=atof(value);
    value=getProperty("nr");
    if(value) nr=atoi(value);
    value=getProperty("nb");
    if(value) nb=atoi(value);
    value=getProperty("anf");
    if(value) anf=atoi(value);
    value=getProperty("snb");
    if(value) snb=atoi(value);
    value=getProperty("agc");
    if(value) agc=atoi(value);
    value=getProperty("agc_gain");
    if(value) agc_gain=atof(value);
    value=getProperty("step");
    if(value) step=atoi(value);
    value=getProperty("byte_swap");
    if(value) byte_swap=atoi(value);
    value=getProperty("cw_keys_reversed");
    if(value) cw_keys_reversed=atoi(value);
    value=getProperty("cw_keyer_speed");
    if(value) cw_keyer_speed=atoi(value);
    value=getProperty("cw_keyer_mode");
    if(value) cw_keyer_mode=atoi(value);
    value=getProperty("cw_keyer_weight");
    if(value) cw_keyer_weight=atoi(value);
    value=getProperty("cw_keyer_spacing");
    if(value) cw_keyer_spacing=atoi(value);
    value=getProperty("cw_keyer_internal");
    if(value) cw_keyer_internal=atoi(value);
    value=getProperty("cw_keyer_sidetone_volume");
    if(value) cw_keyer_sidetone_volume=atoi(value);
    value=getProperty("cw_keyer_ptt_delay");
    if(value) cw_keyer_ptt_delay=atoi(value);
    value=getProperty("cw_keyer_hang_time");
    if(value) cw_keyer_hang_time=atoi(value);
    value=getProperty("cw_keyer_sidetone_frequency");
    if(value) cw_keyer_sidetone_frequency=atoi(value);
    value=getProperty("cw_breakin");
    if(value) cw_breakin=atoi(value);


    bandRestoreState();
}

void radioSaveState() {
    char value[80];
    sprintf(value,"%d",sample_rate);
    setProperty("sample_rate",value);
    sprintf(value,"%d",filter_board);
    setProperty("filter_board",value);
    sprintf(value,"%d",apollo_tuner);
    setProperty("apollo_tuner",value);
    sprintf(value,"%d",pa);
    setProperty("pa",value);
    sprintf(value,"%d",panadapter_high);
    setProperty("panadapter_high",value);
    sprintf(value,"%d",panadapter_low);
    setProperty("panadapter_low",value);
    sprintf(value,"%d",waterfall_high);
    setProperty("waterfall_high",value);
    sprintf(value,"%d",waterfall_low);
    setProperty("waterfall_low",value);
    sprintf(value,"%d",waterfall_automatic);
    setProperty("waterfall_automatic",value);
    sprintf(value,"%f",volume);
    setProperty("volume",value);
    sprintf(value,"%f",mic_gain);
    setProperty("mic_gain",value);
    sprintf(value,"%d",orion_mic);
    setProperty("orion_mic",value);
    sprintf(value,"%d",nr);
    setProperty("nr",value);
    sprintf(value,"%d",nb);
    setProperty("nb",value);
    sprintf(value,"%d",anf);
    setProperty("anf",value);
    sprintf(value,"%d",snb);
    setProperty("snb",value);
    sprintf(value,"%d",agc);
    setProperty("agc",value);
    sprintf(value,"%f",agc_gain);
    setProperty("agc_gain",value);
    sprintf(value,"%d",step);
    setProperty("step",value);
    sprintf(value,"%d",byte_swap);
    setProperty("byte_swap",value);
    sprintf(value,"%d",cw_keys_reversed);
    setProperty("cw_keys_reversed",value);
    sprintf(value,"%d",cw_keyer_speed);
    setProperty("cw_keyer_speed",value);
    sprintf(value,"%d",cw_keyer_mode);
    setProperty("cw_keyer_mode",value);
    sprintf(value,"%d",cw_keyer_weight);
    setProperty("cw_keyer_weight",value);
    sprintf(value,"%d",cw_keyer_spacing);
    setProperty("cw_keyer_spacing",value);
    sprintf(value,"%d",cw_keyer_internal);
    setProperty("cw_keyer_internal",value);
    sprintf(value,"%d",cw_keyer_sidetone_volume);
    setProperty("cw_keyer_sidetone_volume",value);
    sprintf(value,"%d",cw_keyer_ptt_delay);
    setProperty("cw_keyer_ptt_delay",value);
    sprintf(value,"%d",cw_keyer_hang_time);
    setProperty("cw_keyer_hang_time",value);
    sprintf(value,"%d",cw_keyer_sidetone_frequency);
    setProperty("cw_keyer_sidetone_frequency",value);
    sprintf(value,"%d",cw_breakin);
    setProperty("cw_breakin",value);

    bandSaveState();
}
