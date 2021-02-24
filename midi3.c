/*
 * Layer-3 of MIDI support
 * 
 * (C) Christoph van Wullen, DL1YCF
 *
 *
 * In most cases, a certain action only makes sense for a specific
 * type. For example, changing the VFO frequency will only be implemeted
 * for MIDI_TYPE_WHEEL, and TUNE off/on only with MIDI_TYPE_KNOB.
 *
 * However, changing the volume makes sense both with MIDI_TYPE_KNOB and MIDI_TYPE_WHEEL.
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
#include "store.h"
#ifdef LOCALCW
#include "iambic.h"
#endif

void DoTheMidi(enum MIDIaction action, enum MIDItype type, int val) {

    int new;
    double dnew;
    double *dp;
    int    *ip;

    //
    // Handle cases in alphabetical order of the key words in midi.props
    //
    switch (action) {
	/////////////////////////////////////////////////////////// "A2B"
	case MIDI_ACTION_VFO_A2B: // only key supported
	    if (type == MIDI_TYPE_KEY) {
	      g_idle_add(ext_vfo_a_to_b, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "AFGAIN"
	case MIDI_ACTION_AF_GAIN: // knob or wheel supported
            switch (type) {
	      case MIDI_TYPE_KNOB:
		active_receiver->volume = 0.01*val;
		break;
	      case MIDI_TYPE_WHEEL:	
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
	case MIDI_ACTION_AGCATTACK: // only key supported
	    // cycle through fast/med/slow AGC attack
	    if (type == MIDI_TYPE_KEY) {
	      new=active_receiver->agc + 1;
	      if (new > AGC_FAST) new=0;
	      active_receiver->agc=new;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "AGCVAL"
	case MIDI_ACTION_AGC: // knob or wheel supported
	    switch (type) {
	      case MIDI_TYPE_KNOB:
		dnew = -20.0 + 1.4*val;
		break;
	      case MIDI_TYPE_WHEEL:
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
	/////////////////////////////////////////////////////////// "ANF"
	case MIDI_ACTION_ANF:	// only key supported
	    if (type == MIDI_TYPE_KEY) {
	      g_idle_add(ext_anf_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "ATT"
	case MIDI_ACTION_ATT:	// Key for ALEX attenuator, wheel or knob for slider
	    switch(type) {
	      case MIDI_TYPE_KEY:
		if (filter_board == ALEX && active_receiver->adc == 0) {
		  new=active_receiver->alex_attenuation + 1;
		  if (new > 3) new=0;
		  g_idle_add(ext_set_alex_attenuation, GINT_TO_POINTER(new));
		  g_idle_add(ext_update_att_preamp, NULL);
		}
		break;
	      case MIDI_TYPE_WHEEL:
		new=adc_attenuation[active_receiver->adc] + val;
		dp=malloc(sizeof(double));
		*dp=(double) new;
                if(have_rx_gain) {
                  if(*dp<-12.0) {
                    *dp=-12.0;
                  } else if(*dp>48.0) {
                    *dp=48.0;
                  }
                } else {
                  if(*dp<0.0) {
                    *dp=0.0;
                  } else if (*dp>31.0) {
                    *dp=31.0;
                  }
                }
		g_idle_add(ext_set_attenuation_value,(gpointer) dp);
		break;
	      case MIDI_TYPE_KNOB:
		dp=malloc(sizeof(double));
                if (have_rx_gain) {
		  *dp=-12.0 + 0.6*(double) val;
                } else {
                  *dp = 0.31 * (double) val;
                }
		g_idle_add(ext_set_attenuation_value,(gpointer) dp);
		break;
	      default:
		// do nothing
		// we should not come here anyway
		break;
	    }
	    break;
	/////////////////////////////////////////////////////////// "B2A"
	case MIDI_ACTION_VFO_B2A: // only key supported
	    if (type == MIDI_TYPE_KEY) {
	      g_idle_add(ext_vfo_b_to_a, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "BANDDOWN"
	/////////////////////////////////////////////////////////// "BANDUP"
	case MIDI_ACTION_BAND_DOWN:
	case MIDI_ACTION_BAND_UP:
	    switch (type) {
	      case MIDI_TYPE_KEY:
		new=(action == MIDI_ACTION_BAND_UP) ? 1 : -1;
		break;
	      case MIDI_TYPE_WHEEL:
		new=val > 0 ? 1 : -1;
		break;
	      case MIDI_TYPE_KNOB:
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
	case MIDI_ACTION_COMPRESS: // wheel or knob
	    switch (type) {
	      case MIDI_TYPE_WHEEL:
		dnew=transmitter->compressor_level + val;
		if (dnew > 20.0) dnew=20.0;
		if (dnew < 0 ) dnew=0;
		break;
	      case MIDI_TYPE_KNOB:
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
	case MIDI_ACTION_CTUN: // only key supported
	    // toggle CTUN
	    if (type == MIDI_TYPE_KEY) {
	      g_idle_add(ext_ctun_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "CURRVFO"
	case MIDI_ACTION_VFO: // only wheel supported
	    if (type == MIDI_TYPE_WHEEL && !locked) {
		g_idle_add(ext_vfo_step, GINT_TO_POINTER(val));
	    }
	    break;
	/////////////////////////////////////////////////////////// "CW"
	case MIDI_ACTION_CWKEY: // only key
	  //
	  // This is a CW key-up/down which uses functions from the keyer
	  // that by-pass the interrupt-driven standard action.
	  // It is intended to
	  // be used with external keyers that send key-up/down messages via
	  // MIDI using this command.
	  //
	  // NO BREAK-IN! The keyer has to take care of sending MIDI PTT
	  // on/off messages at appropriate times.
          //
          // Since this if for immediate key-down, it does not rely on LOCALCW
          //
	    if (type == MIDI_TYPE_KEY) {
              if (val != 0) {
                cw_key_down=960000;  // max. 20 sec to protect hardware
                cw_key_up=0;
                cw_key_hit=1;
              } else {
                cw_key_down=0;
                cw_key_up=0;
              }
            }
	    break;
	/////////////////////////////////////////////////////////// "CWL"
	/////////////////////////////////////////////////////////// "CWR"
	case MIDI_ACTION_CWL: // only key
	case MIDI_ACTION_CWR: // only key
#ifdef LOCALCW
	    if (type == MIDI_TYPE_KEY) {
		new=(action == MIDI_ACTION_CWL);
		keyer_event(new,val);
	    }
#else
	    g_print("%s: %s:%d\n",__FUNCTION__,action==MIDI_ACTION_CWL?"CWL":"CWR",val);

#endif
	    break;
	/////////////////////////////////////////////////////////// "CWSPEED"
	case MIDI_ACTION_CWSPEED: // knob or wheel
            switch (type) {
              case MIDI_TYPE_KNOB:
		// speed between 5 and 35 wpm
                new= (int) (5.0 + (double) val * 0.3);
                break;
              case MIDI_TYPE_WHEEL:
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
	/////////////////////////////////////////////////////////// "DIVCOARSEGAIN"
	case MIDI_ACTION_DIV_COARSEGAIN:  // knob or wheel supported
	case MIDI_ACTION_DIV_FINEGAIN:    // knob or wheel supported
	case MIDI_ACTION_DIV_GAIN:        // knob or wheel supported
            switch (type) {
              case MIDI_TYPE_KNOB:
                if (action == MIDI_ACTION_DIV_COARSEGAIN || action == MIDI_ACTION_DIV_GAIN) {
		  // -25 to +25 dB in steps of 0.5 dB
		  dnew = 10.0*(-25.0 + 0.5*val - div_gain);
		} else {
		  // round gain to a multiple of 0.5 dB and apply a +/- 0.5 dB update
                  new = (int) (2*div_gain + 1.0) / 2;
		  dnew = 10.0*((double) new + 0.01*val - 0.5 - div_gain);
		}
                break;
              case MIDI_TYPE_WHEEL:
                // coarse: increaments in steps of 0.25 dB, medium: steps of 0.1 dB fine: in steps of 0.01 dB
                if (action == MIDI_ACTION_DIV_GAIN) {
		  dnew = val*0.5;
		} else if (action == MIDI_ACTION_DIV_COARSEGAIN) {
		  dnew = val*2.5;
		} else {
		  dnew = val * 0.1;
	 	}
                break;
              default:
                // do not change
                // we should not come here anyway
		dnew = 0.0;
                break;
            }
	    // dnew is the delta times 10
	    dp=malloc(sizeof(double));
	    *dp=dnew;
            g_idle_add(ext_diversity_change_gain, dp);
            break;
        /////////////////////////////////////////////////////////// "DIVPHASE"
        case MIDI_ACTION_DIV_COARSEPHASE:   // knob or wheel supported
        case MIDI_ACTION_DIV_FINEPHASE:     // knob or wheel supported
	case MIDI_ACTION_DIV_PHASE:		// knob or wheel supported
            switch (type) {
              case MIDI_TYPE_KNOB:
		// coarse: change phase from -180 to 180
                // fine: change from -5 to 5
                if (action == MIDI_ACTION_DIV_COARSEPHASE || action == MIDI_ACTION_DIV_PHASE) {
		  // coarse: change phase from -180 to 180 in steps of 3.6 deg
                  dnew = (-180.0 + 3.6*val - div_phase);
                } else {
		  // fine: round to multiple of 5 deg and apply a +/- 5 deg update
                  new = 5 * ((int) (div_phase+0.5) / 5);
                  dnew =  (double) new + 0.1*val -5.0 -div_phase;
                }
                break;
              case MIDI_TYPE_WHEEL:
		if (action == MIDI_ACTION_DIV_PHASE) {
		  dnew = val*0.5; 
		} else if (action == MIDI_ACTION_DIV_COARSEPHASE) {
		  dnew = val*2.5;
		} else if (action == MIDI_ACTION_DIV_FINEPHASE) {
		  dnew = 0.1*val;
		}
                break;
              default:
                // do not change
                // we should not come here anyway
                dnew = 0.0;
                break;
            }
            // dnew is the delta
            dp=malloc(sizeof(double));
            *dp=dnew;
            g_idle_add(ext_diversity_change_phase, dp);
            break;
        /////////////////////////////////////////////////////////// "DIVTOGGLE"
        case MIDI_ACTION_DIV_TOGGLE:   // only key supported
            if (type == MIDI_TYPE_KEY) {
                // enable/disable DIVERSITY
                diversity_enabled = diversity_enabled ? 0 : 1;
                g_idle_add(ext_vfo_update, NULL);
            }
            break;
	/////////////////////////////////////////////////////////// "DUP"
        case MIDI_ACTION_DUP:
	    if (can_transmit && !isTransmitting()) {
	      duplex=duplex==1?0:1;
              g_idle_add(ext_set_duplex, NULL);
	    }
            break;
	/////////////////////////////////////////////////////////// "FILTERDOWN"
	/////////////////////////////////////////////////////////// "FILTERUP"
	case MIDI_ACTION_FILTER_DOWN:
	case MIDI_ACTION_FILTER_UP:
	    //
	    // In filter.c, the filters are sorted such that the widest one comes first
	    // Therefore let MIDI_ACTION_FILTER_UP move down.
	    //
	    switch (type) {
	      case MIDI_TYPE_KEY:
		new=(action == MIDI_ACTION_FILTER_UP) ? -1 : 1;
		break;
	      case MIDI_TYPE_WHEEL:
		new=val > 0 ? -1 : 1;
		break;
	      case MIDI_TYPE_KNOB:
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
	case MIDI_ACTION_LOCK: // only key supported
	    if (type == MIDI_TYPE_KEY) {
	      locked=!locked;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "MICGAIN"
	case MIDI_ACTION_MIC_VOLUME: // knob or wheel supported
	    // TODO: possibly adjust linein value if that is effective
	    switch (type) {
	      case MIDI_TYPE_KNOB:
		dnew=-10.0 + 0.6*val;
		break;
	      case MIDI_TYPE_WHEEL:
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
	case MIDI_ACTION_MODE_DOWN:
	case MIDI_ACTION_MODE_UP:
	    switch (type) {
	      case MIDI_TYPE_KEY:
		new=(action == MIDI_ACTION_MODE_UP) ? 1 : -1;
		break;
	      case MIDI_TYPE_WHEEL:
		new=val > 0 ? 1 : -1;
		break;
	      case MIDI_TYPE_KNOB:
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
	case MIDI_ACTION_MOX: // only key supported
	    // Note this toggles the PTT state without knowing the
            // actual state. See MIDI_ACTION_PTTONOFF for actually
            // *setting* PTT
	    if (type == MIDI_TYPE_KEY && can_transmit) {
	        new = !mox;
		g_idle_add(ext_mox_update, GINT_TO_POINTER(new));
	    }
	    break;    
        /////////////////////////////////////////////////////////// "MUTE"
        case MIDI_ACTION_MUTE:
            if (type == MIDI_TYPE_KEY) {
              g_idle_add(ext_mute_update,NULL);
	    }
            break;
	/////////////////////////////////////////////////////////// "NOISEBLANKER"
	case MIDI_ACTION_NB: // only key supported
	    // cycle through NoiseBlanker settings: OFF, NB, NB2
            if (type == MIDI_TYPE_KEY) {
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
	case MIDI_ACTION_NR: // only key supported
	    // cycle through NoiseReduction settings: OFF, NR1, NR2
	    if (type == MIDI_TYPE_KEY) {
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
	/////////////////////////////////////////////////////////// "PAN"
        case MIDI_ACTION_PAN:  // wheel and knob
	    switch (type) {
              case MIDI_TYPE_WHEEL:
                g_idle_add(ext_pan_update,GINT_TO_POINTER(val*10));
                break;
	      case MIDI_TYPE_KNOB:
                g_idle_add(ext_pan_set,GINT_TO_POINTER(val));
                break;
	      default:
		// no action for keys (we should not come here anyway)
		break;
            }
            break;
	/////////////////////////////////////////////////////////// "PANHIGH"
	case MIDI_ACTION_PAN_HIGH:  // wheel or knob
	    switch (type) {
	      case MIDI_TYPE_WHEEL:
		if (mox) {
		    // TX panadapter affected
		    transmitter->panadapter_high += val;
		} else {
		    active_receiver->panadapter_high += val;
		}
		break;
	    case MIDI_TYPE_KNOB:
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
	case MIDI_ACTION_PAN_LOW:  // wheel and knob
	    switch (type) {
	      case MIDI_TYPE_WHEEL:
		if (isTransmitting()) {
		    // TX panadapter affected
		    transmitter->panadapter_low += val;
		} else {
		    active_receiver->panadapter_low += val;
		}
		break;
	      case MIDI_TYPE_KNOB:
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
	case MIDI_ACTION_PRE:	// only key supported
	    if (type == MIDI_TYPE_KEY) {
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
	/////////////////////////////////////////////////////////// "PTTONOFF"
        case MIDI_ACTION_PTTONOFF:  // key only
            // always use with "ONOFF"
	    if (type == MIDI_TYPE_KEY && can_transmit) {
		g_idle_add(ext_mox_update, GINT_TO_POINTER(val));
	    }
	    break;    
	/////////////////////////////////////////////////////////// "PURESIGNAL"
	case MIDI_ACTION_PS: // only key supported
#ifdef PURESIGNAL
	    // toggle PURESIGNAL
	    if (type == MIDI_TYPE_KEY) {
	      new=!(transmitter->puresignal);
	      g_idle_add(ext_tx_set_ps,GINT_TO_POINTER(new));
	    }
#endif
	    break;
	/////////////////////////////////////////////////////////// "RECALLM[0-4]"
	case MIDI_ACTION_MEM_RECALL_M0:
	case MIDI_ACTION_MEM_RECALL_M1:
	case MIDI_ACTION_MEM_RECALL_M2:
	case MIDI_ACTION_MEM_RECALL_M3:
	case MIDI_ACTION_MEM_RECALL_M4:
            //
	    // only key supported
            //
            if (type == MIDI_TYPE_KEY) {
                new = action - MIDI_ACTION_MEM_RECALL_M0,
		g_idle_add(ext_recall_memory_slot, GINT_TO_POINTER(new));
	    }
            break;
	/////////////////////////////////////////////////////////// "RFGAIN"
        case MIDI_ACTION_RF_GAIN: // knob or wheel supported
            if (type == MIDI_TYPE_KNOB) {
                new=val;
            } else  if (type == MIDI_TYPE_WHEEL) {
                new=(int)active_receiver->rf_gain+val;
            }
            g_idle_add(ext_set_rf_gain, GINT_TO_POINTER((int)new));
	    break;
	/////////////////////////////////////////////////////////// "RFPOWER"
	case MIDI_ACTION_TX_DRIVE: // knob or wheel supported
	    switch (type) {
	      case MIDI_TYPE_KNOB:
		dnew = val;
		break;
	      case MIDI_TYPE_WHEEL:
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
	case MIDI_ACTION_RIT_CLEAR:	  // only key supported
	    if (type == MIDI_TYPE_KEY) {
	      // clear RIT value
	      vfo[active_receiver->id].rit = new;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	/////////////////////////////////////////////////////////// "RITSTEP"
        case MIDI_ACTION_RIT_STEP: // key or wheel supported
            // This cycles between RIT increments 1, 10, 100, 1, 10, 100, ...
            switch (type) {
              case MIDI_TYPE_KEY:
                // key cycles through in upward direction
                val=1;
                /* FALLTHROUGH */
              case MIDI_TYPE_WHEEL:
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
	case MIDI_ACTION_RIT_TOGGLE:  // only key supported
	    if (type == MIDI_TYPE_KEY) {
		// enable/disable RIT
		new=vfo[active_receiver->id].rit_enabled;
		vfo[active_receiver->id].rit_enabled = new ? 0 : 1;
	        g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "RITVAL"
	case MIDI_ACTION_RIT_VAL:	// wheel or knob
	    switch (type) {
	      case MIDI_TYPE_WHEEL:
		// This changes the RIT value incrementally,
	  	// but we restrict the change to +/ 9.999 kHz
		new = vfo[active_receiver->id].rit + val*rit_increment;
		if (new >  9999) new= 9999;
		if (new < -9999) new=-9999;
		vfo[active_receiver->id].rit = new;
		break;
	      case MIDI_TYPE_KNOB:
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
        case MIDI_ACTION_SAT:
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
	/////////////////////////////////////////////////////////// "SNB"
	case MIDI_ACTION_SNB:	// only key supported
	    if (type == MIDI_TYPE_KEY) {
	      g_idle_add(ext_snb_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "SPLIT"
	case MIDI_ACTION_SPLIT: // only key supported
	    // toggle split mode
	    if (type == MIDI_TYPE_KEY) {
              g_idle_add(ext_split_toggle, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "STOREM[0-4]"
	case MIDI_ACTION_MEM_STORE_M0:
	case MIDI_ACTION_MEM_STORE_M1:
	case MIDI_ACTION_MEM_STORE_M2:
	case MIDI_ACTION_MEM_STORE_M3:
	case MIDI_ACTION_MEM_STORE_M4:
            //
	    // only key supported
            //
            if (type == MIDI_TYPE_KEY) {
                new = action - MIDI_ACTION_MEM_STORE_M0;
		g_idle_add(ext_store_memory_slot, GINT_TO_POINTER(new));
	    }
            break;
	/////////////////////////////////////////////////////////// "SWAPRX"
	case MIDI_ACTION_SWAP_RX:	// only key supported
	    if (type == MIDI_TYPE_KEY && receivers == 2) {
		new=active_receiver->id;	// 0 or 1
		new= (new == 1) ? 0 : 1;	// id of currently inactive receiver
		active_receiver=receiver[new];
		g_idle_add(menu_active_receiver_changed,NULL);
		g_idle_add(ext_vfo_update,NULL);
		g_idle_add(sliders_active_receiver_changed,NULL);
	    }
	    break;    
	/////////////////////////////////////////////////////////// "SWAPVFO"
	case MIDI_ACTION_SWAP_VFO:	// only key supported
	    if (type == MIDI_TYPE_KEY) {
		g_idle_add(ext_vfo_a_swap_b,NULL);
	    }
	    break;    
	/////////////////////////////////////////////////////////// "TUNE"
	case MIDI_ACTION_TUNE: // only key supported
	    if (type == MIDI_TYPE_KEY && can_transmit) {
	        new = !tune;
		g_idle_add(ext_tune_update, GINT_TO_POINTER(new));
	    }
	    break;    
	/////////////////////////////////////////////////////////// "VFOA"
	/////////////////////////////////////////////////////////// "VFOB"
	case MIDI_ACTION_VFOA: // only wheel supported
	case MIDI_ACTION_VFOB: // only wheel supported
	    if (type == MIDI_TYPE_WHEEL && !locked) {
	        ip=malloc(2*sizeof(int));
		*ip = (action == MIDI_ACTION_VFOA) ? 0 : 1;
		*(ip+1)=val;
		g_idle_add(ext_vfo_id_step, ip);
	    }
	    break;
	/////////////////////////////////////////////////////////// "VFOSTEPDOWN"
	/////////////////////////////////////////////////////////// "VFOSTEPUP"
        case MIDI_ACTION_VFO_STEP_DOWN: // key or wheel supported
        case MIDI_ACTION_VFO_STEP_UP:
	    switch (type) {
	      case MIDI_TYPE_KEY:
		new =  (action == MIDI_ACTION_VFO_STEP_UP) ? 1 : -1;
		g_idle_add(ext_update_vfo_step, GINT_TO_POINTER(new));
		break;
	      case MIDI_TYPE_WHEEL:
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
	case MIDI_ACTION_VOX: // only key supported
	    // toggle VOX
	    if (type == MIDI_TYPE_KEY) {
	      vox_enabled = !vox_enabled;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "VOXLEVEL"
	case MIDI_ACTION_VOXLEVEL: // knob or wheel supported
            switch (type) {
              case MIDI_TYPE_WHEEL:
                // This changes the value incrementally,
                // but stay within limits (0.0 through 1.0)
                vox_threshold += (double) val * 0.01;
		if (vox_threshold > 1.0) vox_threshold=1.0;
		if (vox_threshold < 0.0) vox_threshold=0.0;
                break;
              case MIDI_TYPE_KNOB:
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
        case MIDI_ACTION_XIT_CLEAR:  // only key supported
            if (type == MIDI_TYPE_KEY) {
                // this clears the XIT value and disables XIT
                if(can_transmit) {
                  transmitter->xit = 0;
                  transmitter->xit_enabled = 0;
                  g_idle_add(ext_vfo_update, NULL);
                }
            }
            break;
	/////////////////////////////////////////////////////////// "XITVAL"
        case MIDI_ACTION_XIT_VAL:   // wheel and knob supported.
	    if (can_transmit) {
              switch (type) {
                case MIDI_TYPE_WHEEL:
                  // This changes the XIT value incrementally,
                  // but we restrict the change to +/ 9.999 kHz
                  new = transmitter->xit + val*rit_increment;
                  if (new >  9999) new= 9999;
                  if (new < -9999) new=-9999;
                  transmitter->xit = new;
                  break;
                case MIDI_TYPE_KNOB:
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
	/////////////////////////////////////////////////////////// "ZOOM"
	/////////////////////////////////////////////////////////// "ZOOMDOWN"
	/////////////////////////////////////////////////////////// "ZOOMUP"
        //
        // The ZOOM key word is redundant but we leave it to maintain
        // backwards compatibility
        //
        case MIDI_ACTION_ZOOM:       // wheel, knob
        case MIDI_ACTION_ZOOM_UP:    // key, wheel, knob
        case MIDI_ACTION_ZOOM_DOWN:  // key, wheel, knob
	    switch (type) {
	      case MIDI_TYPE_KEY:
                if (action == MIDI_ACTION_ZOOM) break;
		new =  (action == MIDI_ACTION_ZOOM_UP) ? 1 :-1;
                g_idle_add(ext_zoom_update,GINT_TO_POINTER(new));
		break;
	      case MIDI_TYPE_WHEEL:
		new = (val > 0) ? 1 : -1;
                g_idle_add(ext_zoom_update,GINT_TO_POINTER(new));
		break;
              case MIDI_TYPE_KNOB:
                g_idle_add(ext_zoom_set,GINT_TO_POINTER(val));
                break;
	      default:
		// do nothing
		// we should not come here anyway
		break;
	    }
            break;

	case MIDI_ACTION_NONE:
	    // No error message, this is the "official" action for un-used controller buttons.
	    break;;
	default:
	    // This means we have forgotten to implement an action, so we inform on stderr.
	    fprintf(stderr,"Unimplemented MIDI action: A=%d\n", (int) action);
    }
}
