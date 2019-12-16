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
#ifdef LOCALCW
#include "iambic.h"
#endif

void DoTheMidi(enum MIDIaction action, enum MIDItype type, int val) {

    int new;
    double dnew;
    double *dp;
    int    *ip;

    //
    // Handle cases in alphabetical order of the key words in midi.inp
    //
    switch (action) {
	/////////////////////////////////////////////////////////// "A2B"
	case VFO_A2B: // only key supported
	    if (type == MIDI_KEY) {
	      g_idle_add(ext_vfo_a_to_b, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "AFGAIN"
	case AF_GAIN: // knob or wheel supported
            switch (type) {
	      case MIDI_KNOB:
		active_receiver->volume = 0.01*val;
		break;
	      case MIDI_WHEEL:	
		dnew=active_receiver->volume += 0.01*val;
		if (dnew < 0.0) dnew=0.0; if (dnew > 1.0) dnew=1.0;
		active_receiver->volume = dnew;
		break;
	      default:
		// do not change volume
		// we should not come here anyway
		break;
	    }
	    g_idle_add(ext_update_af_gain, NULL);
	    break;
	/////////////////////////////////////////////////////////// "AGCATTACK"
	case AGCATTACK: // only key supported
	    // cycle through fast/med/slow AGC attack
	    if (type == MIDI_KEY) {
	      new=active_receiver->agc + 1;
	      if (new > AGC_FAST) new=0;
	      active_receiver->agc=new;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "AGCVAL"
	case MIDI_AGC: // knob or wheel supported
	    switch (type) {
	      case MIDI_KNOB:
		dnew = -20.0 + 1.4*val;
		break;
	      case MIDI_WHEEL:
		dnew=active_receiver->agc_gain + val;
		if (dnew < -20.0) dnew=-20.0; if (dnew > 120.0) dnew=120.0;
		break;
	      default:
		// do not change value
		// we should not come here anyway
		dnew=active_receiver->agc_gain;
		break;
	    }
	    dp=malloc(sizeof(double));
	    *dp=dnew;
	    g_idle_add(ext_set_agc_gain, (gpointer) dp);
	    break;
	/////////////////////////////////////////////////////////// "ATT"
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
		  new=adc_attenuation[active_receiver->adc] + val;
		  if (new > 31) new=31;
		  if (new < 0 ) new=0;
		  dp=malloc(sizeof(double));
		  *dp=new;
		  g_idle_add(ext_set_attenuation_value,(gpointer) dp);
		  break;
	      case MIDI_KNOB:
		new=(31*val)/100;
		dp=malloc(sizeof(double));
		*dp=new;
		g_idle_add(ext_set_attenuation_value,(gpointer) dp);
		break;
	      default:
		// do nothing
		// we should not come here anyway
		break;
	    }
	    break;
	/////////////////////////////////////////////////////////// "B2A"
	case VFO_B2A: // only key supported
	    if (type == MIDI_KEY) {
	      g_idle_add(ext_vfo_b_to_a, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "BANDDOWN"
	/////////////////////////////////////////////////////////// "BANDUP"
	case BAND_DOWN:
	case BAND_UP:
	    switch (type) {
	      case MIDI_KEY:
		new=(action == BAND_UP) ? 1 : -1;
		break;
	      case MIDI_WHEEL:
		new=val > 0 ? 1 : -1;
		break;
	      case MIDI_KNOB:
		// cycle through the bands
		new = ((BANDS-1) * val) / 100 - vfo[active_receiver->id].band;
		break;
	      default:
		// do not change
		// we should not come here anyway
		new=0;
		break;
	    }
	    //
	    // If the band has not changed, do nothing. Otherwise
	    // vfo.c will loop through the band stacks
	    //
	    if (new != 0) {
	      new+=vfo[active_receiver->id].band;
	      if (new >= BANDS) new=0;
	      if (new < 0) new=BANDS-1;
	      g_idle_add(ext_vfo_band_changed, GINT_TO_POINTER(new));
	    }
	    break;
	/////////////////////////////////////////////////////////// "COMPRESS"
	case COMPRESS: // wheel or knob
	    switch (type) {
	      case MIDI_WHEEL:
		dnew=transmitter->compressor_level + val;
		if (dnew > 20.0) dnew=20.0;
		if (dnew < 0 ) dnew=0;
		break;
	      case MIDI_KNOB:
		dnew=(20.0*val)/100.0;
		break;
	      default:
		// do not change
		// we should not come here anyway
		dnew=transmitter->compressor_level;
		break;
	    }
	    transmitter->compressor_level=dnew;
	    // automatically engange compressor if level > 0.5
	    if (dnew < 0.5) transmitter->compressor=0;
	    if (dnew > 0.5) transmitter->compressor=1;
	    g_idle_add(ext_set_compression, NULL);
	    break;
	/////////////////////////////////////////////////////////// "CTUN"
	case MIDI_CTUN: // only key supported
	    // toggle CTUN
	    if (type == MIDI_KEY) {
	      g_idle_add(ext_ctun_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "CURRVFO"
	case VFO: // only wheel supported
	    if (type == MIDI_WHEEL && !locked) {
		g_idle_add(ext_vfo_step, GINT_TO_POINTER(val));
	    }
	    break;
	/////////////////////////////////////////////////////////// "CWL"
	/////////////////////////////////////////////////////////// "CWR"
	case CWL: // only key
	case CWR: // only key
#ifdef LOCALCW
	    if (type == MIDI_KEY) {
		new=(action == CWL);
		keyer_event(new,val);
	    }
#endif
	    break;
	/////////////////////////////////////////////////////////// "CWSPEED"
	case CWSPEED: // knob or wheel
            switch (type) {
              case MIDI_KNOB:
		// speed between 5 and 35 wpm
                new= (int) (5.0 + (double) val * 0.3);
                break;
              case MIDI_WHEEL:
		// here we allow from 1 to 60 wpm
                new = cw_keyer_speed + val;
		if (new <  1) new=1;
		if (new > 60) new=60;
                break;
              default:
                // do not change
                // we should not come here anyway
                new = cw_keyer_speed;
                break;
            }
	    cw_keyer_speed=new;
#ifdef LOCALCW
	    keyer_update();
#endif
            g_idle_add(ext_vfo_update, NULL);
	    break;
	/////////////////////////////////////////////////////////// "DUP"
        case MIDI_DUP:
            if(duplex) {
              duplex=0;
            } else {
              duplex=1;
            }
            g_idle_add(ext_vfo_update, NULL);
            break;
	/////////////////////////////////////////////////////////// "FILTERDOWN"
	/////////////////////////////////////////////////////////// "FILTERUP"
	case FILTER_DOWN:
	case FILTER_UP:
	    //
	    // In filter.c, the filters are sorted such that the widest one comes first
	    // Therefore let FILTER_UP move down.
	    //
	    switch (type) {
	      case MIDI_KEY:
		new=(action == FILTER_UP) ? -1 : 1;
		break;
	      case MIDI_WHEEL:
		new=val > 0 ? -1 : 1;
		break;
	      case MIDI_KNOB:
		// cycle through all the filters: val=100 maps to filter #0
		new = ((FILTERS-1) * (val-100)) / 100 - vfo[active_receiver->id].filter;
		break;
	      default:
		// do not change filter setting
		// we should not come here anyway
		new=0;
		break;
	    }
	    if (new != 0) {
	      new+=vfo[active_receiver->id].filter;
	      if (new >= FILTERS) new=0;
	      if (new <0) new=FILTERS-1;
	      g_idle_add(ext_vfo_filter_changed, GINT_TO_POINTER(new));
	    }
	    break;
	/////////////////////////////////////////////////////////// "LOCK"
	case MIDI_LOCK: // only key supported
	    if (type == MIDI_KEY) {
	      locked=!locked;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "MICGAIN"
	case MIC_VOLUME: // knob or wheel supported
	    // TODO: possibly adjust linein value if that is effective
	    switch (type) {
	      case MIDI_KNOB:
		dnew=-10.0 + 0.6*val;
		break;
	      case MIDI_WHEEL:
		dnew = mic_gain + val;
		if (dnew < -10.0) dnew=-10.0; if (dnew > 50.0) dnew=50.0;
		break;
	      default:
		// do not change mic gain
		// we should not come here anyway
		dnew = mic_gain;
		break;
	    }
	    dp=malloc(sizeof(double));
	    *dp=dnew;
	    g_idle_add(ext_set_mic_gain, (gpointer) dp);
	    break;
	/////////////////////////////////////////////////////////// "MODEDOWN"
	/////////////////////////////////////////////////////////// "MODEUP"
	case MODE_DOWN:
	case MODE_UP:
	    switch (type) {
	      case MIDI_KEY:
		new=(action == MODE_UP) ? 1 : -1;
		break;
	      case MIDI_WHEEL:
		new=val > 0 ? 1 : -1;
		break;
	      case MIDI_KNOB:
		// cycle through all the modes
		new = ((MODES-1) * val) / 100 - vfo[active_receiver->id].mode;
		break;
	      default:
		// do not change
		// we should not come here anyway
		new=0;
		break;
	    }
	    if (new != 0) {
	      new+=vfo[active_receiver->id].mode;
	      if (new >= MODES) new=0;
	      if (new <0) new=MODES-1;
	      g_idle_add(ext_vfo_mode_changed, GINT_TO_POINTER(new));
	    }
	    break;
	/////////////////////////////////////////////////////////// "MOX"
	case MIDI_MOX: // only key supported
	    if (type == MIDI_KEY) {
	        new = !mox;
		g_idle_add(ext_mox_update, GINT_TO_POINTER(new));
	    }
	    break;    
	/////////////////////////////////////////////////////////// "NOISEBLANKER"
	case MIDI_NB: // only key supported
	    // cycle through NoiseBlanker settings: OFF, NB, NB2
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
	/////////////////////////////////////////////////////////// "NOISEREDUCTION"
	case MIDI_NR: // only key supported
	    // cycle through NoiseReduction settings: OFF, NR1, NR2
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
	      g_idle_add(ext_update_noise, NULL);
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "PANHIGH"
	case PAN_HIGH:  // wheel or knob
	    switch (type) {
	      case MIDI_WHEEL:
		if (mox) {
		    // TX panadapter affected
		    transmitter->panadapter_high += val;
		} else {
		    active_receiver->panadapter_high += val;
		}
		break;
	    case MIDI_KNOB:
		// Adjust "high water" in the range -50 ... 0 dBm
		new = -50 + val/2;
		if (mox) {
		    transmitter->panadapter_high = new;
		} else {
		    active_receiver->panadapter_high = new;
		}
		break;
	      default:
		// do nothing
		// we should not come here anyway
		break;
	    }
	    g_idle_add(ext_vfo_update, NULL);
	    break;
	/////////////////////////////////////////////////////////// "PANLOW"
	case PAN_LOW:  // wheel and knob
	    switch (type) {
	      case MIDI_WHEEL:
		if (isTransmitting()) {
		    // TX panadapter affected
		    transmitter->panadapter_low += val;
		} else {
		    active_receiver->panadapter_low += val;
		}
		break;
	      case MIDI_KNOB:
		if (isTransmitting()) {
		    // TX panadapter: use values -100 through -50
		    new = -100 + val/2;
		    transmitter->panadapter_low =new;
		} else {
		    // RX panadapter: use values -140 through -90
		    new = -140 + val/2;
		    active_receiver->panadapter_low = new;
		}
		break;
	      default:
		// do nothing
		// we should not come here anyway
		break;
	    }
	    g_idle_add(ext_vfo_update, NULL);
	    break;
	/////////////////////////////////////////////////////////// "PREAMP"
	case PRE:	// only key supported
	    if (type == MIDI_KEY) {
		//
		// Normally on/off, but for CHARLY25, cycle through three
		// possible states. Current HPSDR hardware does no have
		// switch'able preamps.
		//
		int c25= (filter_board == CHARLY25);
		new = active_receiver->preamp + active_receiver->dither;
		new++;
		if (c25) {
		  if (new >2) new=0;
		} else {
		  if (new >1) new=0;
		}
		switch (new) {
		    case 0:
			active_receiver->preamp=0;
			if (c25) active_receiver->dither=0;
			break;
		    case 1:
			active_receiver->preamp=1;
			if (c25) active_receiver->dither=0;
			break;
		    case 2:
			active_receiver->preamp=1;
			if (c25) active_receiver->dither=1;
			break;
		}
		g_idle_add(ext_update_att_preamp, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "PURESIGNAL"
	case MIDI_PS: // only key supported
#ifdef PURESIGNAL
	    // toggle PURESIGNAL
	    if (type == MIDI_KEY) {
	      new=!(transmitter->puresignal);
	      g_idle_add(ext_tx_set_ps,GINT_TO_POINTER(new));
	    }
#endif
	    break;
	/////////////////////////////////////////////////////////// "RFGAIN"
        case MIDI_RF_GAIN: // knob or wheel supported
            if (type == MIDI_KNOB) {
                new=val;
            } else  if (type == MIDI_WHEEL) {
                new=(int)active_receiver->rf_gain+val;
            }
            g_idle_add(ext_set_rf_gain, GINT_TO_POINTER((int)val));
	    break;
	/////////////////////////////////////////////////////////// "RFPOWER"
	case TX_DRIVE: // knob or wheel supported
	    switch (type) {
	      case MIDI_KNOB:
		dnew = val;
		break;
	      case MIDI_WHEEL:
		dnew=transmitter->drive + val;
		if (dnew < 0.0) dnew=0.0; if (dnew > 100.0) dnew=100.0;
		break;
	      default:
		// do not change value
		// we should not come here anyway
		dnew=transmitter->drive;
		break;
	    }
	    dp=malloc(sizeof(double));
	    *dp=dnew;
	    g_idle_add(ext_set_drive, (gpointer) dp);
	    break;
	/////////////////////////////////////////////////////////// "RITCLEAR"
	case MIDI_RIT_CLEAR:	  // only key supported
	    if (type == MIDI_KEY) {
	      // clear RIT value
	      vfo[active_receiver->id].rit = new;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	/////////////////////////////////////////////////////////// "RITSTEP"
        case RIT_STEP: // key or wheel supported
            // This cycles between RIT increments 1, 10, 100, 1, 10, 100, ...
            switch (type) {
              case MIDI_KEY:
                // key cycles through in upward direction
                val=1;
                /* FALLTHROUGH */
              case MIDI_WHEEL:
                // wheel cycles upward or downward
                if (val > 0) {
                  rit_increment=10*rit_increment;
                } else {
                  rit_increment=rit_increment/10;
                }
                if (rit_increment < 1) rit_increment=100;
                if (rit_increment > 100) rit_increment=1;
                break;
              default:
                // do nothing
                break;
            }
            g_idle_add(ext_vfo_update, NULL);
            break;
	/////////////////////////////////////////////////////////// "RITTOGGLE"
	case RIT_TOGGLE:  // only key supported
	    if (type == MIDI_KEY) {
		// enable/disable RIT
		new=vfo[active_receiver->id].rit_enabled;
		vfo[active_receiver->id].rit_enabled = new ? 0 : 1;
	        g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "RITVAL"
	case RIT_VAL:	// wheel or knob
	    switch (type) {
	      case MIDI_WHEEL:
		// This changes the RIT value incrementally,
	  	// but we restrict the change to +/ 9.999 kHz
		new = vfo[active_receiver->id].rit + val*rit_increment;
		if (new >  9999) new= 9999;
		if (new < -9999) new=-9999;
		vfo[active_receiver->id].rit = new;
		break;
	      case MIDI_KNOB:
	 	// knob: adjust in the range +/ 50*rit_increment
		new = (val-50) * rit_increment;
		vfo[active_receiver->id].rit = new;
		break;
	      default:
		// do nothing
		// we should not come here anyway
		break;
	    }
	    // enable/disable RIT according to RIT value
	    vfo[active_receiver->id].rit_enabled = (vfo[active_receiver->id].rit == 0) ? 0 : 1;
	    g_idle_add(ext_vfo_update, NULL);
	    break;
	/////////////////////////////////////////////////////////// "SAT"
        case MIDI_SAT:
	    switch (sat_mode) {
		case SAT_NONE:
		  sat_mode=SAT_MODE;
		  break;
		case SAT_MODE:
		  sat_mode=RSAT_MODE;
		  break;
		case RSAT_MODE:
		default:
		  sat_mode=SAT_NONE;
		  break;
	    }
	    g_idle_add(ext_vfo_update, NULL);
            break;
	/////////////////////////////////////////////////////////// "SPLIT"
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
	/////////////////////////////////////////////////////////// "SWAPRX"
	case SWAP_RX:	// only key supported
	    if (type == MIDI_KEY && receivers == 2) {
		new=active_receiver->id;	// 0 or 1
		new= (new == 1) ? 0 : 1;	// id of currently inactive receiver
		active_receiver=receiver[new];
		g_idle_add(menu_active_receiver_changed,NULL);
		g_idle_add(ext_vfo_update,NULL);
		g_idle_add(sliders_active_receiver_changed,NULL);
	    }
	    break;    
	/////////////////////////////////////////////////////////// "SWAPVFO"
	case SWAP_VFO:	// only key supported
	    if (type == MIDI_KEY) {
		g_idle_add(ext_vfo_a_swap_b,NULL);
	    }
	    break;    
	/////////////////////////////////////////////////////////// "TUNE"
	case MIDI_TUNE: // only key supported
	    if (type == MIDI_KEY) {
	        new = !tune;
		g_idle_add(ext_tune_update, GINT_TO_POINTER(new));
	    }
	    break;    
	/////////////////////////////////////////////////////////// "VFOA"
	/////////////////////////////////////////////////////////// "VFOB"
	case VFOA: // only wheel supported
	case VFOB: // only wheel supported
	    if (type == MIDI_WHEEL && !locked) {
	        ip=malloc(2*sizeof(int));
		*ip = (action == VFOA) ? 0 : 1;   // could use (action - VFOA) to support even more VFOs
		*(ip+1)=val;
		g_idle_add(ext_vfo_id_step, ip);
	    }
	    break;
	/////////////////////////////////////////////////////////// "VFOSTEPDOWN"
	/////////////////////////////////////////////////////////// "VFOSTEPUP"
        case VFO_STEP_DOWN: // key or wheel supported
        case VFO_STEP_UP:
	    switch (type) {
	      case MIDI_KEY:
		new =  (action == VFO_STEP_UP) ? 1 : -1;
		g_idle_add(ext_update_vfo_step, GINT_TO_POINTER(new));
		break;
	      case MIDI_WHEEL:
		new = (val > 0) ? 1 : -1;
		g_idle_add(ext_update_vfo_step, GINT_TO_POINTER(new));
		break;
	      default:
		// do nothing
		// we should not come here anyway
		break;
	    }
            break;
	/////////////////////////////////////////////////////////// "VOX"
	case VOX: // only key supported
	    // toggle VOX
	    if (type == MIDI_KEY) {
	      vox_enabled = !vox_enabled;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "VOXLEVEL"
	case VOXLEVEL: // knob or wheel supported
            switch (type) {
              case MIDI_WHEEL:
                // This changes the value incrementally,
                // but stay within limits (0.0 through 1.0)
                vox_threshold += (double) val * 0.01;
		if (vox_threshold > 1.0) vox_threshold=1.0;
		if (vox_threshold < 0.0) vox_threshold=0.0;
                break;
              case MIDI_KNOB:
                vox_threshold = 0.01 * (double) val;
                break;
              default:
                // do nothing
                // we should not come here anyway
                break;
            }
	    // VOX level not shown on screen, hence no VFO update
	    break;
	/////////////////////////////////////////////////////////// "XITCLEAR"
        case MIDI_XIT_CLEAR:  // only key supported
            if (type == MIDI_KEY) {
                // this clears the XIT value and disables XIT
                if(can_transmit) {
                  transmitter->xit = 0;
                  transmitter->xit_enabled = 0;
                  g_idle_add(ext_vfo_update, NULL);
                }
            }
            break;
	/////////////////////////////////////////////////////////// "XITVAL"
        case XIT_VAL:   // wheel and knob supported.
	    if (can_transmit) {
              switch (type) {
                case MIDI_WHEEL:
                  // This changes the XIT value incrementally,
                  // but we restrict the change to +/ 9.999 kHz
                  new = transmitter->xit + val*rit_increment;
                  if (new >  9999) new= 9999;
                  if (new < -9999) new=-9999;
                  transmitter->xit = new;
                  break;
                case MIDI_KNOB:
                  // knob: adjust in the range +/ 50*rit_increment
                  new = (val-50) * rit_increment;
                  transmitter->xit = new;
                  break;
                default:
                  // do nothing
                  // we should not come here anyway
                  break;
              }
              // enable/disable XIT according to XIT value
              transmitter->xit_enabled = (transmitter->xit == 0) ? 0 : 1;
              g_idle_add(ext_vfo_update, NULL);
	    }
            break;
	case ACTION_NONE:
	    // No error message, this is the "official" action for un-used controller buttons.
	    break;
	default:
	    // This means we have forgotten to implement an action, so we inform on stderr.
	    fprintf(stderr,"Unimplemented MIDI action: A=%d\n", (int) action);
    }
}
