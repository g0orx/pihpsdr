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
#include "actions.h"
#include "midi.h"
#ifdef LOCALCW
#include "iambic.h"
#endif

void DoTheMidi(int action, enum ACTIONtype type, int val) {

    int new;
    double dnew;
    double *dp;
    int    *ip;
    PROCESS_ACTION *a;

    //
    // Handle cases in alphabetical order of the key words in midi.props
    //

    g_print("%s: action=%d type=%d val=%d\n",__FUNCTION__,action,type,val);

    switch(type) {
      case MIDI_KEY:
        a=g_new(PROCESS_ACTION,1);
        a->action=action;
        a->mode=val?PRESSED:RELEASED;
        g_idle_add(process_action,a);
	break;
      case MIDI_KNOB:
        a=g_new(PROCESS_ACTION,1);
        a->action=action;
        a->mode=ABSOLUTE;
        a->val=val;
        g_idle_add(process_action,a);
        break;
      case MIDI_WHEEL:
        a=g_new(PROCESS_ACTION,1);
        a->action=action;
        a->mode=RELATIVE;
        a->val=val;
        g_idle_add(process_action,a);
        break;
    }

/*
    switch (action) {
	/////////////////////////////////////////////////////////// "A2B"
	case A_TO_B: // only key supported
	    if (type == MIDI_KEY) {
	      a=g_new(PROCESS_ACTION,1);
              a->action=A_TO_B;
              a->mode=PRESSED;
              g_idle_add(process_action,a);
	      //g_idle_add(ext_vfo_a_to_b, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "AFGAIN"
	case AF_GAIN: // knob or wheel supported
            switch (type) {
	      case MIDI_KNOB:
		a=g_new(PROCESS_ACTION,1);
                a->action=AF_GAIN;
                a->mode=ABSOLUTE;
                a->val=val;
                g_idle_add(process_action,a);
		break;
	      case MIDI_WHEEL:	
		a=g_new(PROCESS_ACTION,1);
                a->action=AF_GAIN;
                a->mode=RELATIVE;
                a->val=val;
                g_idle_add(process_action,a);
		break;
	      default:
		// do not change volume
		// we should not come here anyway
		break;
	    }
	    g_idle_add(ext_update_af_gain, NULL);
	    break;
	/////////////////////////////////////////////////////////// "AGCATTACK"
	case AGC: // only key supported
	    // cycle through fast/med/slow AGC attack
	    if (type == MIDI_KEY) {
	      a=g_new(PROCESS_ACTION,1);
              a->action=AGC;
              a->mode=PRESSED;
              g_idle_add(process_action,a);
	    }
	    break;
	/////////////////////////////////////////////////////////// "AGCVAL"
	case AGC_GAIN: // knob or wheel supported
	    switch (type) {
	      case MIDI_KNOB:
		a=g_new(PROCESS_ACTION,1);
                a->action=AGC_GAIN;
                a->mode=ABSOLUTE;
                a->val=val;
                g_idle_add(process_action,a);
		//dnew = -20.0 + 1.4*val;
		break;
	      case MIDI_WHEEL:
		a=g_new(PROCESS_ACTION,1);
                a->action=AGC_GAIN;
                a->mode=RELATIVE;
                a->val=val;
                g_idle_add(process_action,a);
		//dnew=active_receiver->agc_gain + val;
		//if (dnew < -20.0) dnew=-20.0; if (dnew > 120.0) dnew=120.0;
		break;
	      default:
		// do not change value
		// we should not come here anyway
		//dnew=active_receiver->agc_gain;
		break;
	    }
	    //dp=malloc(sizeof(double));
	    //*dp=dnew;
	    //g_idle_add(ext_set_agc_gain, (gpointer) dp);
	    break;
	/////////////////////////////////////////////////////////// "ANF"
	case ANF:	// only key supported
	    if (type == MIDI_KEY) {
	      a=g_new(PROCESS_ACTION,1);
              a->action=ANF;
              a->mode=PRESSED;
              g_idle_add(process_action,a);
	      //g_idle_add(ext_anf_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "ATT"
	case ATTENUATION:	// Key for ALEX attenuator, wheel or knob for slider
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
		new=adc[active_receiver->adc].attenuation + val;
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
	      case MIDI_KNOB:
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
	case B_TO_A: // only key supported
	    if (type == MIDI_KEY) {
	      a=g_new(PROCESS_ACTION,1);
              a->action=B_TO_A;
              a->mode=PRESSED;
              g_idle_add(process_action,a);
	      //g_idle_add(ext_vfo_b_to_a, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "NUMPADxx"
	case NUMPAD_0:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(0));
            break;
	case NUMPAD_1:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(1));
            break;
	case NUMPAD_2:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(2));
            break;
	case NUMPAD_3:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(3));
            break;
	case NUMPAD_4:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(4));
            break;
	case NUMPAD_5:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(5));
            break;
	case NUMPAD_6:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(6));
            break;
	case NUMPAD_7:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(7));
            break;
	case NUMPAD_8:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(8));
            break;
	case NUMPAD_9:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(9));
            break;
	case NUMPAD_CL:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(-1));
            break;
	case NUMPAD_ENTER:
	    g_idle_add(ext_num_pad,GINT_TO_POINTER(-2));
            break;
	    break;

	/////////////////////////////////////////////////////////// "BANDxxx"
        case BAND_10:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band10));
            }
            break;
        case BAND_12:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band12));
            }
            break;
#ifdef SOAPYSDR
        case BAND_1240:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band1240));
            }
            break;
        case BAND_144:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band144));
            }
            break;
#endif
        case BAND_15:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band15));
            }
            break;
        case BAND_160:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band160));
            }
            break;
        case BAND_17:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band17));
            }
            break;
        case BAND_20:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band20));
            }
            break;
#ifdef SOAPYSDR
        case BAND_220:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band220));
            }
            break;
        case BAND_2300:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band2300));
            }
            break;
#endif
        case BAND_30:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band30));
            }
            break;
#ifdef SOAPYSDR
        case BAND_3400:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band3400));
            }
            break;
        case BAND_70:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band70));
            }
            break;
#endif
        case BAND_40:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band40));
            }
            break;
#ifdef SOAPYSDR
        case BAND_430:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band430));
            }
            break;
#endif
        case BAND_6:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band6));
            }
            break;
        case BAND_60:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band60));
            }
            break;
        case BAND_80:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band80));
            }
            break;
#ifdef SOAPYSDR
        case BAND_902:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(band902));
            }
            break;
        case BAND_AIR:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(bandAIR));
            }
            break;
#endif
        case BAND_GEN:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(bandGen));
            }
            break;
        case BAND_WWV:
            if (type == MIDI_KEY) {
              g_idle_add(ext_band_select, GINT_TO_POINTER(bandWWV));
            }
            break;
	/////////////////////////////////////////////////////////// "BANDDOWN"
	/////////////////////////////////////////////////////////// "BANDUP"
	case BAND_MINUS:
	case BAND_PLUS:
	    switch (type) {
	      case MIDI_KEY:
		new=(action == BAND_PLUS) ? 1 : -1;
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
	/////////////////////////////////////////////////////////// "COMPRESSION"
	case COMPRESSION: // wheel or knob
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
	case CTUN: // only key supported
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
	case CW_LEFT: // only key
	case CW_RIGHT: // only key
#ifdef LOCALCW
	    if (type == MIDI_KEY) {
		new=(action == CW_LEFT);
		keyer_event(new,val);
	    }
#else
	    g_print("%s: %s:%d\n",__FUNCTION__,action==CW_LEFT?"CWL":"CWR",val);

#endif
	    break;
	/////////////////////////////////////////////////////////// "CWSPEED"
	case CW_SPEED: // knob or wheel
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
	/////////////////////////////////////////////////////////// "DIVCOARSEGAIN"
	case DIV_GAIN_COARSE:  // knob or wheel supported
	case DIV_GAIN_FINE:    // knob or wheel supported
	case DIV_GAIN:        // knob or wheel supported
            switch (type) {
              case MIDI_KNOB:
                if (action == DIV_GAIN_COARSE || action == DIV_GAIN) {
		  // -25 to +25 dB in steps of 0.5 dB
		  dnew = 10.0*(-25.0 + 0.5*val - div_gain);
		} else {
		  // round gain to a multiple of 0.5 dB and apply a +/- 0.5 dB update
                  new = (int) (2*div_gain + 1.0) / 2;
		  dnew = 10.0*((double) new + 0.01*val - 0.5 - div_gain);
		}
                break;
              case MIDI_WHEEL:
                // coarse: increaments in steps of 0.25 dB, medium: steps of 0.1 dB fine: in steps of 0.01 dB
                if (action == DIV_GAIN) {
		  dnew = val*0.5;
		} else if (action == DIV_GAIN_COARSE) {
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
        case DIV_PHASE_COARSE:   // knob or wheel supported
        case DIV_PHASE_FINE:     // knob or wheel supported
	case DIV_PHASE:		// knob or wheel supported
            switch (type) {
              case MIDI_KNOB:
		// coarse: change phase from -180 to 180
                // fine: change from -5 to 5
                if (action == DIV_PHASE_COARSE || action == DIV_PHASE) {
		  // coarse: change phase from -180 to 180 in steps of 3.6 deg
                  dnew = (-180.0 + 3.6*val - div_phase);
                } else {
		  // fine: round to multiple of 5 deg and apply a +/- 5 deg update
                  new = 5 * ((int) (div_phase+0.5) / 5);
                  dnew =  (double) new + 0.1*val -5.0 -div_phase;
                }
                break;
              case MIDI_WHEEL:
		if (action == DIV_PHASE) {
		  dnew = val*0.5; 
		} else if (action == DIV_PHASE_COARSE) {
		  dnew = val*2.5;
		} else if (action == DIV_PHASE_FINE) {
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
        case DIV:   // only key supported
            if (type == MIDI_KEY) {
                // enable/disable DIVERSITY
                diversity_enabled = diversity_enabled ? 0 : 1;
                g_idle_add(ext_vfo_update, NULL);
            }
            break;
	/////////////////////////////////////////////////////////// "DUP"
        case DUPLEX:
	    if (can_transmit && !isTransmitting()) {
	      duplex=duplex==1?0:1;
              g_idle_add(ext_set_duplex, NULL);
	    }
            break;
	/////////////////////////////////////////////////////////// "FILTERDOWN"
	/////////////////////////////////////////////////////////// "FILTERUP"
	case FILTER_MINUS:
	case FILTER_PLUS:
	    //
	    // In filter.c, the filters are sorted such that the widest one comes first
	    // Therefore let FILTER_UP move down.
	    //
	    switch (type) {
	      case MIDI_KEY:
		new=(action == FILTER_PLUS) ? -1 : 1;
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
	/////////////////////////////////////////////////////////// "MENU_FILTFILTERER"
	case MENU_FILTER:
	    g_idle_add(ext_menu_filter, NULL);
	    break;
	/////////////////////////////////////////////////////////// "MENU_MODE"
	case MENU_MODE:
	    g_idle_add(ext_menu_mode, NULL);
	    break;
	/////////////////////////////////////////////////////////// "LOCK"
	case LOCK: // only key supported
	    if (type == MIDI_KEY) {
	      locked=!locked;
	      g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "MICGAIN"
	case MIC_GAIN: // knob or wheel supported
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
	case MODE_MINUS:
	case MODE_PLUS:
	    switch (type) {
	      case MIDI_KEY:
		new=(action == MODE_PLUS) ? 1 : -1;
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
	case MOX: // only key supported
	    if (type == MIDI_KEY && can_transmit) {
	        new = !mox;
		g_idle_add(ext_mox_update, GINT_TO_POINTER(new));
	    }
	    break;    
        /////////////////////////////////////////////////////////// "MUTE"
        case MUTE:
            if (type == MIDI_KEY) {
              g_idle_add(ext_mute_update,NULL);
	    }
            break;
	/////////////////////////////////////////////////////////// "NOISEBLANKER"
	case NB: // only key supported
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
	case NR: // only key supported
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
	/////////////////////////////////////////////////////////// "PAN"
        case PAN:  // wheel and knob
	    switch (type) {
              case MIDI_WHEEL:
                g_idle_add(ext_pan_update,GINT_TO_POINTER(val));
                break;
	      case MIDI_KNOB:
                g_idle_add(ext_pan_set,GINT_TO_POINTER(val));
                break;
	      default:
		// no action for keys (we should not come here anyway)
		break;
            }
            break;
	/////////////////////////////////////////////////////////// "PANHIGH"
	case PANADAPTER_HIGH:  // wheel or knob
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
	case PANADAPTER_LOW:  // wheel and knob
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
	case PREAMP:	// only key supported
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
	case PS: // only key supported
#ifdef PURESIGNAL
	    // toggle PURESIGNAL
	    if (type == MIDI_KEY) {
	      new=!(transmitter->puresignal);
	      g_idle_add(ext_tx_set_ps,GINT_TO_POINTER(new));
	    }
#endif
	    break;
	/////////////////////////////////////////////////////////// "RFGAIN"
        case RF_GAIN: // knob or wheel supported
            if (type == MIDI_KNOB) {
                new=val;
            } else  if (type == MIDI_WHEEL) {
                //new=(int)active_receiver->rf_gain+val;
                new=(int)adc[active_receiver->id].gain+val;
            }
            g_idle_add(ext_set_rf_gain, GINT_TO_POINTER((int)new));
	    break;
	/////////////////////////////////////////////////////////// "RFPOWER"
	case DRIVE: // knob or wheel supported
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
	case RIT_CLEAR:	  // only key supported
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
                // FALLTHROUGH
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
	case RIT_ENABLE:  // only key supported
	    if (type == MIDI_KEY) {
		// enable/disable RIT
		new=vfo[active_receiver->id].rit_enabled;
		vfo[active_receiver->id].rit_enabled = new ? 0 : 1;
	        g_idle_add(ext_vfo_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "RITVAL"
	case RIT:	// wheel or knob
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
        case SAT:
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
	case SNB:	// only key supported
	    if (type == MIDI_KEY) {
	      g_idle_add(ext_snb_update, NULL);
	    }
	    break;
	/////////////////////////////////////////////////////////// "SPLIT"
	case SPLIT: // only key supported
	    // toggle split mode
	    if (type == MIDI_KEY) {
              g_idle_add(ext_split_toggle, NULL);
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
	//case SWAP_VFO:	// only key supported
	//    if (type == MIDI_KEY) {
//		g_idle_add(ext_vfo_a_swap_b,NULL);
//	    }
//	    break;    
	/////////////////////////////////////////////////////////// "TUNE"
	case TUNE: // only key supported
	    if (type == MIDI_KEY && can_transmit) {
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
        case VFO_STEP_MINUS: // key or wheel supported
        case VFO_STEP_PLUS:
	    switch (type) {
	      case MIDI_KEY:
		new =  (action == VFO_STEP_PLUS) ? 1 : -1;
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
        case XIT_CLEAR:  // only key supported
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
        case XIT:   // wheel and knob supported.
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
	/////////////////////////////////////////////////////////// "ZOOM"
        case ZOOM:  // wheel and knob
            switch (type) {
              case MIDI_WHEEL:
g_print("MIDI_ZOOM: MIDI_WHEEL: val=%d\n",val);
                g_idle_add(ext_zoom_update,GINT_TO_POINTER(val));
                break;
              case MIDI_KNOB:
g_print("MIDI_ZOOM: MIDI_KNOB: val=%d\n",val);
                g_idle_add(ext_zoom_set,GINT_TO_POINTER(val));
                break;
	      default:
		// no action for keys (should not come here anyway)
		break;
            }
            break;
	/////////////////////////////////////////////////////////// "ZOOMDOWN"
	/////////////////////////////////////////////////////////// "ZOOMUP"
        case ZOOM_MINUS:  // key
        case ZOOM_PLUS:  // key
	    switch (type) {
	      case MIDI_KEY:
		new =  (action == ZOOM_PLUS) ? 1 : -1;
                g_idle_add(ext_zoom_update,GINT_TO_POINTER(new));
		break;
	      case MIDI_WHEEL:
		new = (val > 0) ? 1 : -1;
                g_idle_add(ext_zoom_update,GINT_TO_POINTER(new));
		break;
	      default:
		// do nothing
		// we should not come here anyway
		break;
	    }
            break;

	case NO_ACTION:
	    // No error message, this is the "official" action for un-used controller buttons.
	    break;
	default:
	    // This means we have forgotten to implement an action, so we inform on stderr.
	    fprintf(stderr,"Unimplemented MIDI action: A=%d\n", (int) action);
    }
    */
}
