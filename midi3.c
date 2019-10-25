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
#include <gtk/gtk.h>

#include "radio.h"
#include "vfo.h"
#include "filter.h"
#include "band.h"
#include "mode.h"
#include "new_menu.h"
#include "sliders.h"
#include "ext.h"
#include "agc.h"
#include "midi.h"

void DoTheMidi(enum MIDIaction action, enum MIDItype type, int val) {

    int new;
    double dnew;
    double *dp;
    int    *ip;

    switch (action) {
	case SWAP_VFO:	// only key supported
	    if (type == MIDI_KEY) {
		g_idle_add(ext_vfo_a_swap_b,NULL);
	    }
	    break;    
	case VFO: // only wheel supported
	    if (type == MIDI_WHEEL && !locked) {
		g_idle_add(ext_vfo_step, GINT_TO_POINTER(val));
	    }
	    break;
	case VFOA: // only wheel supported
	case VFOB: // only wheel supported
	    if (type == MIDI_WHEEL && !locked) {
	        ip=malloc(2*sizeof(int));
		*ip = (action == VFOA) ? 0 : 1;   // could use (action - VFOA) to support even more VFOs
		*(ip+1)=val;
		g_idle_add(ext_vfo_id_step, ip);
	    }
	    break;
	case MIDI_TUNE: // only key supported
	    if (type == MIDI_KEY) {
	        new = !tune;
		g_idle_add(ext_tune_update, GINT_TO_POINTER(new));
	    }
	    break;    
	case MIDI_MOX: // only key supported
	    if (type == MIDI_KEY) {
	        new = !mox;
		g_idle_add(ext_mox_update, GINT_TO_POINTER(new));
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
	case MIDI_AGC: // knob or wheel supported
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
	    g_idle_add(ext_vfo_band_changed, GINT_TO_POINTER(new));
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
	    g_idle_add(ext_vfo_filter_changed, GINT_TO_POINTER(new));
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
	    g_idle_add(ext_vfo_mode_changed, GINT_TO_POINTER(new));
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
	case MIDI_RIT_CLEAR:  // only key supported
	    if (type == MIDI_KEY) {
		// this clears the RIT value and disables RIT
		vfo[active_receiver->id].rit = new;
		vfo[active_receiver->id].rit_enabled = 0;
	        g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case RIT_VAL:	// only wheel supported
	    if (type == MIDI_WHEEL) {
		// This changes the RIT value. If a value of 0 is reached,
		// RIT is disabled
		new = vfo[active_receiver->id].rit + val*rit_increment;
		if (new >  9999) new= 9999;
		if (new < -9999) new=-9999;
		vfo[active_receiver->id].rit = new;
		vfo[active_receiver->id].rit_enabled = (new != 0);
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
	case PRE:	// only key supported, and only CHARLY25
	    if (filter_board == CHARLY25 && type == MIDI_KEY) {
		//
		// For hardware other than CHARLY25, we do not
		// switch preamps
		//
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
	    }
	    break;
	case ATT:	// Key for ALEX attenuator, wheel or knob for slider
	    switch(type) {
		case MIDI_KEY:
		    if (filter_board == ALEX && active_receiver->adc == 0) {
		      new=active_receiver->alex_attenuation + 1;
		      if (new > 3) new=0;
		      g_idle_add(ext_set_alex_attenuation, GINT_TO_POINTER(new));
		      g_idle_add(ext_update_att_preamp, NULL);
		    }
		    break;
		case MIDI_WHEEL:
		case MIDI_KNOB:
		    if (type == MIDI_WHEEL) {
		      new=adc_attenuation[active_receiver->adc] + val;
		      if (new > 31) new=31;
		      if (new < 0 ) new=0;
	
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
	    // Use values in the range 0 ... 20
	    if (type == MIDI_WHEEL) {
              dnew=transmitter->compressor_level + val;
	      if (dnew > 20.0) dnew=20.0;
	      if (dnew < 0 ) dnew=0;
	    } else if (type == MIDI_KNOB){
	      dnew=(20.0*val)/100.0;
	    } else {
		break;
	    }
	    transmitter->compressor_level=dnew;
	    if (dnew < 0.5) transmitter->compressor=0;
	    if (dnew > 0.5) transmitter->compressor=1;
	    g_idle_add(ext_set_compression, NULL);
	    break;
	case MIDI_NB: // only key supported
	    // cycle through NoiseBlanker settings
            if (type == MIDI_KEY) {
	      if (active_receiver->nb) {
		active_receiver->nb = 0;
		active_receiver->nb2= 1;
	      } else if (active_receiver->nb2) {
		active_receiver->nb = 0;
		active_receiver->nb2= 0;
	      } else {
		active_receiver->nb = 1;
		active_receiver->nb2= 0;
	      }
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case MIDI_NR: // only key supported
	    // cycle through NoiseReduction settings
	    if (type == MIDI_KEY) {
	      if (active_receiver->nr) {
		active_receiver->nr = 0;
		active_receiver->nr2= 1;
	      } else if (active_receiver->nr2) {
		active_receiver->nr = 0;
		active_receiver->nr2= 0;
	      } else {
		active_receiver->nr = 1;
		active_receiver->nr2= 0;
	      }
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case VOX: // only key supported
	    // toggle VOX
	    if (type == MIDI_KEY) {
	      vox_enabled = !vox_enabled;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case MIDI_CTUN: // only key supported
	    // toggle CTUN
	    if (type == MIDI_KEY) {
	      new=active_receiver->id;
	      if(!vfo[new].ctun) {
		vfo[new].ctun=1;
		vfo[new].offset=0;
	      } else {
		vfo[new].ctun=0;
	      }
	      vfo[new].ctun_frequency=vfo[new].frequency;
	      set_offset(active_receiver,vfo[new].offset);
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case MIDI_PS: // only key supported
#ifdef PURESIGNAL
	    // toggle PURESIGNAL
	    if (type == MIDI_KEY) {
	      new=!(transmitter->puresignal);
	      g_idle_add(ext_tx_set_ps,GINT_TO_POINTER(new));
	    }
#endif
	    break;
	case MIDI_SPLIT: // only key supported
	    // toggle split mode
	    if (type == MIDI_KEY) {
	      if(!split) {
		split=1;
		tx_set_mode(transmitter,vfo[VFO_B].mode);
	      } else {
		split=0;
		tx_set_mode(transmitter,vfo[VFO_A].mode);
	      }
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case VFO_A2B: // only key supported
	    if (type == MIDI_KEY) {
	      g_idle_add(ext_vfo_a_to_b, NULL);
	    }
	    break;
	case VFO_B2A: // only key supported
	    if (type == MIDI_KEY) {
	      g_idle_add(ext_vfo_b_to_a, NULL);
	    }
	    break;
	case MIDI_LOCK: // only key supported
	    if (type == MIDI_KEY) {
	      locked=!locked;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case AGCATTACK: // only key supported
	    // cycle through fast/med/slow AGC attack
	    if (type == MIDI_KEY) {
	      new=active_receiver->agc + 1;
	      if (new > AGC_FAST) new=0;
	      active_receiver->agc=new;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	case ACTION_NONE:
	    // No error message, this is the "official" action for un-used controller buttons.
	    break;
	default:
	    // This means we have forgotten to implement an action
	    fprintf(stderr,"Unimplemented MIDI action: A=%d\n", (int) action);
    }
}
