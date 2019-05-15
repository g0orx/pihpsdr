/*
 * Layer-3 of MIDI support
 * 
 * (C) Christoph van Wullen, DL1YCF
 *
 *
 * In most cases, a certain action only makes sense for a specific
 * type. For example, changing the VFO frequency will only be implemeted
 * for MIDI_WHEEL, and TUNE off/on only with MIDI_KNOB.
 *
 * However, changing the volume makes sense both with MIDI_KNOB and MIDI_WHEEL.
 */
#include "radio.h"
#include "vfo.h"
#include "filter.h"
#include "band.h"
#include "mode.h"
#include "new_menu.h"
#include "sliders.h"
#include "ext.h"
#include "midi.h"

void DoTheMidi(enum MIDIaction action, enum MIDItype type, int val) {

    int new;
    double dnew;
    double *dp;

    switch (action) {
	case SWAP_VFO:	// only key supported
	    if (type == MIDI_KEY) {
		g_idle_add(ext_vfo_a_swap_b,NULL);
	    }
	    break;    
	case VFO: // only wheel supported
	    if (type == MIDI_WHEEL) g_idle_add(ext_vfo_step, (gpointer)(uintptr_t) val);
	    break;
	case TUNE: // only key supported
	    if (type == MIDI_KEY) {
	        new = !tune;
		g_idle_add(ext_tune_update, (gpointer)(long) new);
	    }
	    break;    
	case MOX: // only key supported
	    if (type == MIDI_KEY) {
	        new = !mox;
		g_idle_add(ext_mox_update, (gpointer)(long) new);
	    }
	    break;    
	case AF_GAIN: // knob or wheel supported
	    if (type == MIDI_KNOB) {
		active_receiver->volume = 0.01*val;
	    } else  if (type == MIDI_WHEEL) {
		dnew=active_receiver->volume += 0.01*val;
		if (dnew < 0.0) dnew=0.0; if (dnew > 1.0) dnew=1.0;
		active_receiver->volume = dnew;
	    } else {
		break;
	    }
	    g_idle_add(ext_update_af_gain, NULL);
	    break;
	case MIC_VOLUME: // knob or wheel supported
	    if (type == MIDI_KNOB) {
		dnew=-10.0 + 0.6*val;
	    } else if (type == MIDI_WHEEL) {
		dnew = mic_gain + val;
		if (dnew < -10.0) dnew=-10.0; if (dnew > 50.0) dnew=50.0;
	    } else {
		break;
	    }
	    dp=malloc(sizeof(double));
	    *dp=dnew;
	    g_idle_add(ext_set_mic_gain, (gpointer) dp);
	    break;
	case AGC: // knob or wheel supported
	    if (type == MIDI_KNOB) {
		dnew = -20.0 + 1.4*val;
	    } else if (type == MIDI_WHEEL) {
		dnew=active_receiver->agc_gain + val;
		if (dnew < -20.0) dnew=-20.0; if (dnew > 120.0) dnew=120.0;
	    } else {
		break;
	    }
	    dp=malloc(sizeof(double));
	    *dp=dnew;
	    g_idle_add(ext_set_agc_gain, (gpointer) dp);
	    break;
	case TX_DRIVE: // knob or wheel supported
	    if (type == MIDI_KNOB) {
		dnew = val;
	    } else if (type == MIDI_WHEEL) {
		dnew=transmitter->drive + val;
		if (dnew < 0.0) dnew=0.0; if (dnew > 100.0) dnew=100.0;
	    } else {
		break;
	    }
	    dp=malloc(sizeof(double));
	    *dp=dnew;
	    g_idle_add(ext_set_drive, (gpointer) dp);
	    break;
	case BAND_UP:     // key or wheel supported
	case BAND_DOWN:   // key or wheel supported
	    if (type == MIDI_KEY) {
		new=(action == BAND_UP) ? 1 : -1;
	    } else if (type == MIDI_WHEEL) {
		new=val;
	    } else {
		break;
	    }
            new+=vfo[active_receiver->id].band;
            if (new >= BANDS) new=0;
            if (new < 0) new=BANDS-1;
	    g_idle_add(ext_vfo_band_changed, (gpointer) (uintptr_t) new);
	    break;
	case FILTER_UP:      // key or wheel supported
	case FILTER_DOWN:    // key or wheel supported
	    if (type == MIDI_KEY) {
		new=(action == FILTER_UP) ? 1 : -1;
	    } else if (type == MIDI_WHEEL) {
		new=val;
	    } else {
		break;
	    }
	    new+=vfo[active_receiver->id].filter;
	    if (new >= FILTERS) new=0;
	    if (new <0) new=FILTERS-1;
	    g_idle_add(ext_vfo_filter_changed, (gpointer) (uintptr_t) new);
	    break;
	case MODE_UP:      // key or wheel supported
	case MODE_DOWN:    // key or wheel supported
	    if (type == MIDI_KEY) {
		new=(action == MODE_UP) ? 1 : -1;
	    } else if (type == MIDI_WHEEL) {
		new=val;
	    } else {
		break;
	    }
	    new+=vfo[active_receiver->id].mode;
	    if (new >= MODES) new=0;
	    if (new <0) new=MODES-1;
	    g_idle_add(ext_vfo_mode_changed, (gpointer) (uintptr_t) new);
	    break;
	case PAN_LOW:  // only wheel supported
	    if (type == MIDI_WHEEL) {
		if (isTransmitting()) {
		    // TX panadapter affected
		    transmitter->panadapter_low += val;
		} else {
		    active_receiver->panadapter_low += val;
		}
	    }
	    break;
	case RIT_ONOFF:  // only key supported
	    if (type == MIDI_KEY) {
		vfo[active_receiver->id].rit_enabled = !vfo[active_receiver->id].rit_enabled;
	        g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case RIT_VAL:	// only wheel supported
	    if (type == MIDI_WHEEL) {
		new = vfo[active_receiver->id].rit + val*rit_increment;
		if (new >  9999) new= 9999;
		if (new < -9999) new=-9999;
		vfo[active_receiver->id].rit = new;
	        g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case PAN_HIGH:  // only wheel supported
	    if (type == MIDI_WHEEL) {
		if (mox) {
		    // TX panadapter affected
		    transmitter->panadapter_high += val;
		} else {
		    active_receiver->panadapter_high += val;
		}
	    }
	    break;
	case PRE:	// only key supported
	    if (filter_board == CHARLY25) {
		new = active_receiver->preamp + active_receiver->dither;
		new++;
		if (new >2) new=0;
		switch (new) {
		    case 0:
			active_receiver->preamp=0;
			active_receiver->dither=0;
			break;
		    case 1:
			active_receiver->preamp=1;
			active_receiver->dither=0;
			break;
		    case 2:
			active_receiver->preamp=1;
			active_receiver->dither=1;
			break;
		}
		g_idle_add(ext_update_att_preamp, NULL);
	    } else {
		new=active_receiver->preamp+1;
		if (new > 1) new=0;
		active_receiver->preamp= (new == 1);
	    }
	    break;
	case ATT:	// Key for ALEX attenuator, wheel or knob for slider
	    switch(type) {
		case MIDI_KEY:
		    new=active_receiver->alex_attenuation + 1;
		    if (new > 3) new=0;
		    g_idle_add(ext_set_alex_attenuation, (gpointer)(uintptr_t)new);
		    g_idle_add(ext_update_att_preamp, NULL);
		    break;
		case MIDI_WHEEL:
		case MIDI_KNOB:
		    if (type == MIDI_WHEEL) {
		      new=adc_attenuation[active_receiver->adc] + val;
		      if (new > 31) new=31;
		      if (new < 0 ) new=0;
		    } else {
		      new=(31*val)/100;
		    }
		    dp=malloc(sizeof(double));
		    *dp=new;
		    g_idle_add(ext_set_attenuation_value,(gpointer) dp);
		    break;
		default:
		    break;
	    }
	    break;
	case COMPRESS:
	case ACTION_NONE:
            fprintf(stderr,"Unimplemented in DoTheMidi: A=%d T=%d val=%d\n", action, type, val);
	    break;
    }
}
