#include <gtk/gtk.h>

#include "main.h"
#include "discovery.h"
#include "receiver.h"
#include "sliders.h"
#include "toolbar.h"
#include "band_menu.h"
#include "diversity_menu.h"
#include "vfo.h"
#include "radio.h"
#include "radio_menu.h"
#include "new_menu.h"
#include "new_protocol.h"
#ifdef PURESIGNAL
#include "ps_menu.h"
#endif
#include "agc.h"
#include "filter.h"
#include "mode.h"
#include "band.h"
#include "bandstack.h"
#include "noise_menu.h"
#include "wdsp.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "ext.h"
#include "zoompan.h"
#include "actions.h"
#include "gpio.h"

char *encoder_string[ENCODER_ACTIONS] = {
  "NO ACTION",
  "AF GAIN",
  "AF GAIN RX1",
  "AF GAIN RX2",
  "AGC GAIN",
  "AGC GAIN RX1",
  "AGC GAIN RX2",
  "ATTENUATION/RX GAIN",
  "COMP",
  "CW FREQUENCY",
  "CW SPEED",
  "DIVERSITY GAIN",
  "DIVERSITY GAIN (coarse)",
  "DIVERSITY GAIN (fine)",
  "DIVERSITY PHASE",
  "DIVERSITY PHASE (coarse)",
  "DIVERSITY PHASE (fine)",
  "DRIVE",
  "IF SHIFT",
  "IF SHIFT RX1",
  "IF SHIFT RX2",
  "IF WIDTH",
  "IF WIDTH RX1",
  "IF WIDTH RX2",
  "MIC GAIN",
  "PAN",
  "PANADAPTER HIGH",
  "PANADAPTER LOW",
  "PANADAPTER STEP",
  "RF GAIN",
  "RF GAIN RX1",
  "RF GAIN RX2",
  "RIT",
  "RIT RX1",
  "RIT RX2",
  "SQUELCH",
  "SQUELCH RX1",
  "SQUELCH RX2",
  "TUNE DRIVE",
  "VFO",
  "WATERFALL HIGH",
  "WATERFALL LOW",
  "XIT",
  "ZOOM"
};

char *sw_string[SWITCH_ACTIONS] = {
  "",
  "A TO B",
  "A SWAP B",
  "AGC",
  "ANF",
  "B TO A",
  "BAND -",
  "BAND +",
  "BSTACK -",
  "BSTACK +",
  "CTUN",
  "DIV",
  "DUPLEX",
  "FILTER -",
  "FILTER +",
  "FUNCTION",
  "LOCK",
  "MENU AGC",
  "MENU BAND",
  "MENU BSTACK",
  "MENU DIV",
  "MENU FILTER",
  "MENU FREQUENCY",
  "MENU MEMORY",
  "MENU MODE",
  "MENU NOISE",
  "MENU PS",
  "MODE -",
  "MODE +",
  "MOX",
  "MUTE",
  "NB",
  "NR",
  "PAN -",
  "PAN +",
  "PS",
  "RIT",
  "RIT CL",
  "RIT -",
  "RIT +",
  "RSAT",
  "SAT",
  "SNB",
  "SPLIT",
  "TUNE",
  "TUNE FULL",
  "TUNE MEMORY",
  "TWO TONE",
  "XIT",
  "XIT CL",
  "XIT -",
  "XIT +",
  "ZOOM -",
  "ZOOM +"
};

char *sw_cap_string[SWITCH_ACTIONS] = {
  "",
  "A>B",
  "A<>B",
  "AGC",
  "ANF",
  "B>A",
  "BAND -",
  "BAND +",
  "BST-",
  "BST+",
  "CTUN",
  "DIV",
  "DUP",
  "FILT -",
  "FILT +",
  "FUNC",
  "LOCK",
  "AGC",
  "BAND",
  "BSTACK",
  "DIV",
  "FILTER",
  "FREQ",
  "MEM",
  "MODE",
  "NOISE",
  "PS",
  "MODE -",
  "MODE +",
  "MOX",
  "MUTE",
  "NB",
  "NR",
  "PAN-",
  "PAN+",
  "PS",
  "RIT",
  "RIT CL",
  "RIT -",
  "RIT +",
  "RSAT",
  "SAT",
  "SNB",
  "SPLIT",
  "TUNE",
  "TUN-F",
  "TUN-M",
  "2TONE",
  "XIT",
  "XIT CL",
  "XIT -",
  "XIT +",
  "ZOOM-",
  "ZOOM+",
};


int encoder_action(void *data) {
  ENCODER_ACTION *a=(ENCODER_ACTION *)data;
  double value;
  int mode;
  int id;
  FILTER * band_filters=filters[vfo[active_receiver->id].mode];
  FILTER *band_filter;
  FILTER *filter;
  int new_val;

  switch(a->action) {
    case ENCODER_VFO:
      vfo_step(a->val);
      break;
    case ENCODER_AF_GAIN:
      value=active_receiver->volume;
      value+=a->val/100.0;
      if(value<0.0) {
        value=0.0;
      } else if(value>1.0) {
        value=1.0;
      }
      set_af_gain(active_receiver->id,value);
      break;
    case ENCODER_AF_GAIN_RX1:
      value=receiver[0]->volume;
      value+=a->val/100.0;
      if(value<0.0) {
        value=0.0;
      } else if(value>1.0) {
        value=1.0;
      }
      set_af_gain(0,value);
      break;
    case ENCODER_AF_GAIN_RX2:
      value=receiver[1]->volume;
      value+=a->val/100.0;
      if(value<0.0) {
        value=0.0;
      } else if(value>1.0) {
        value=1.0;
      }
      set_af_gain(1,value);
      break;
    case ENCODER_RF_GAIN:
      value=active_receiver->rf_gain;
      value+=a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>100.0) {
        value=100.0;
      }
      set_rf_gain(active_receiver->id,value);
      break;
    case ENCODER_RF_GAIN_RX1:
      value=receiver[0]->rf_gain;
      value+=a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>100.0) {
        value=100.0;
      }
      set_rf_gain(0,value);
      break;
    case ENCODER_RF_GAIN_RX2:
      value=receiver[1]->rf_gain;
      value+=a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>71.0) {
        value=71.0;
      }
      set_rf_gain(1,value);
      break;
    case ENCODER_AGC_GAIN:
      value=active_receiver->agc_gain;
      value+=a->val;
      if(value<-20.0) {
        value=-20.0;
      } else if(value>120.0) {
        value=120.0;
      }
      set_agc_gain(active_receiver->id,value);
      break;
    case ENCODER_AGC_GAIN_RX1:
      value=receiver[0]->agc_gain;
      value+=a->val;
      if(value<-20.0) {
        value=-20.0;
      } else if(value>120.0) {
        value=120.0;
      }
      set_agc_gain(0,value);
      break;
    case ENCODER_AGC_GAIN_RX2:
      value=receiver[1]->agc_gain;
      value+=a->val;
      if(value<-20.0) {
        value=-20.0;
      } else if(value>120.0) {
        value=120.0;
      }
      set_agc_gain(1,value);
      break;
    case ENCODER_IF_WIDTH:
      filter_width_changed(active_receiver->id,a->val);
      break;
    case ENCODER_IF_WIDTH_RX1:
      filter_width_changed(0,a->val);
      break;
    case ENCODER_IF_WIDTH_RX2:
      filter_width_changed(1,a->val);
      break;
    case ENCODER_IF_SHIFT:
      filter_shift_changed(active_receiver->id,a->val);
      break;
    case ENCODER_IF_SHIFT_RX1:
      filter_shift_changed(0,a->val);
      break;
    case ENCODER_IF_SHIFT_RX2:
      filter_shift_changed(1,a->val);
      break;
    case ENCODER_ATTENUATION:
      value=(double)adc_attenuation[active_receiver->adc];
      value+=(double)a->val;
      if(have_rx_gain) {
        if(value<-12.0) {
          value=-12.0;
        } else if(value>48.0) {
          value=48.0;
        }
      } else {
        if(value<0.0) {
          value=0.0;
        } else if (value>31.0) {
          value=31.0;
        }
      }
      set_attenuation_value(value);
      break;
    case ENCODER_MIC_GAIN:
      value=mic_gain;
      value+=(double)a->val;
      if(value<-12.0) {
        value=-12.0;
      } else if(value>50.0) {
        value=50.0;
      }
      set_mic_gain(value);
      break;
    case ENCODER_DRIVE:
      value=getDrive();
      value+=(double)a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>drive_max) {
        value=drive_max;
      }
      set_drive(value);
      break;
    case ENCODER_RIT:
      vfo_rit(active_receiver->id,a->val);
      break;
    case ENCODER_RIT_RX1:
      vfo_rit(receiver[0]->id,a->val);
      break;
    case ENCODER_RIT_RX2:
      vfo_rit(receiver[1]->id,a->val);
      break;
    case ENCODER_XIT:
      value=(double)transmitter->xit;
      value+=(double)(a->val*rit_increment);
      if(value<-10000.0) {
        value=-10000.0;
      } else if(value>10000.0) {
        value=10000.0;
      }
      transmitter->xit=(int)value;
      if(protocol==NEW_PROTOCOL) {
        schedule_high_priority();
      }
      g_idle_add(ext_vfo_update,NULL);
      break;
    case ENCODER_CW_SPEED:
      value=(double)cw_keyer_speed;
      value+=(double)a->val;
      if(value<1.0) {
        value=1.0;
      } else if(value>60.0) {
        value=60.0;
      }
      cw_keyer_speed=(int)value;
      g_idle_add(ext_vfo_update,NULL);
      break;
    case ENCODER_CW_FREQUENCY:
      value=(double)cw_keyer_sidetone_frequency;
      value+=(double)a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>1000.0) {
        value=1000.0;
      }
      cw_keyer_sidetone_frequency=(int)value;
      g_idle_add(ext_vfo_update,NULL);
      break;
    case ENCODER_PANADAPTER_HIGH:
      value=(double)active_receiver->panadapter_high;
      value+=(double)a->val;
      active_receiver->panadapter_high=(int)value;
      break;
    case ENCODER_PANADAPTER_LOW:
      value=(double)active_receiver->panadapter_low;
      value+=(double)a->val;
      active_receiver->panadapter_low=(int)value;
      break;
    case ENCODER_PANADAPTER_STEP:
      value=(double)active_receiver->panadapter_step;
      value+=(double)a->val;
      active_receiver->panadapter_step=(int)value;
      break;
    case ENCODER_WATERFALL_HIGH:
      value=(double)active_receiver->waterfall_high;
      value+=(double)a->val;
      active_receiver->waterfall_high=(int)value;
      break;
    case ENCODER_WATERFALL_LOW:
      value=(double)active_receiver->waterfall_low;
      value+=(double)a->val;
      active_receiver->waterfall_low=(int)value;
      break;
    case ENCODER_SQUELCH:
      value=active_receiver->squelch;
      value+=(double)a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>100.0) {
        value=100.0;
      }
      active_receiver->squelch=value;
      set_squelch(active_receiver);
      break;
    case ENCODER_SQUELCH_RX1:
      value=receiver[0]->squelch;
      value+=(double)a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>100.0) {
        value=100.0;
      }
      receiver[0]->squelch=value;
      set_squelch(receiver[0]);
      break;
    case ENCODER_SQUELCH_RX2:
      value=receiver[1]->squelch;
      value+=(double)a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>100.0) {
        value=100.0;
      }
      receiver[1]->squelch=value;
      set_squelch(receiver[1]);
      break;
    case ENCODER_COMP:
      value=(double)transmitter->compressor_level;
      value+=(double)a->val;
      if(value<0.0) {
        value=0.0;
      } else if(value>20.0) {
        value=20.0;
      }
      transmitter->compressor_level=(int)value;
      set_compression(transmitter);
      break;
    case ENCODER_DIVERSITY_GAIN:
      update_diversity_gain((double)a->val * 0.5);
      break;
    case ENCODER_DIVERSITY_GAIN_COARSE:
      update_diversity_gain((double)a->val * 2.5);
      break;
    case ENCODER_DIVERSITY_GAIN_FINE:
      update_diversity_gain((double)a->val * 0.1);
      break;
    case ENCODER_DIVERSITY_PHASE:
      update_diversity_phase((double)a->val* 0.5);
      break;
    case ENCODER_DIVERSITY_PHASE_COARSE:
      update_diversity_phase((double)a->val*2.5);
      break;
    case ENCODER_DIVERSITY_PHASE_FINE:
      update_diversity_phase((double)a->val*0.1);
      break;
    case ENCODER_ZOOM:
      update_zoom((double)a->val);
      break;
    case ENCODER_PAN:
      update_pan((double)a->val*100);
      break;
  }
  g_free(data);
  return 0;
}

int switch_action(void *data) {
  SWITCH_ACTION *a=(SWITCH_ACTION *)data;
  if(a->state==PRESSED) {
    switch(a->action) {
      case FUNCTION:
        if(controller==NO_CONTROLLER || controller==CONTROLLER1) {
          function++;
          if(function>=MAX_FUNCTIONS) {
            function=0;
          }
          switches=switches_controller1[function];
          update_toolbar_labels();
        }
        break;
      case TUNE:
	if(getMox()==1) {
          setMox(0);
        }
        if(getTune()==0) {
          if(canTransmit() || tx_out_of_band) {
            setTune(1);
          } else {
            transmitter_set_out_of_band(transmitter);
          }
        } else {
          setTune(0);
        }
        g_idle_add(ext_vfo_update,NULL);
        break;
      case MOX:
        if(getTune()==1) {
          setTune(0);
        }
        if(getMox()==0) {
          if(canTransmit() || tx_out_of_band) {
            setMox(1);
          } else {
            transmitter_set_out_of_band(transmitter);
          }
        } else {
          setMox(0);
        }
        g_idle_add(ext_vfo_update,NULL);
        break;
      case PS:
#ifdef PURESIGNAL
        if(can_transmit) {
          if(transmitter->puresignal==0) {
            tx_set_ps(transmitter,1);
          } else {
            tx_set_ps(transmitter,0);
          }
        }
#endif
        break;
      case TWO_TONE:
        if(can_transmit) {
          int state=transmitter->twotone?0:1;
          tx_set_twotone(transmitter,state);
        }
        break;
      case NR:
        if(active_receiver->nr==0 && active_receiver->nr2==0) {
          active_receiver->nr=1;
          active_receiver->nr2=0;
          mode_settings[vfo[active_receiver->id].mode].nr=1;
          mode_settings[vfo[active_receiver->id].mode].nr2=0;
        } else if(active_receiver->nr==1 && active_receiver->nr2==0) {
          active_receiver->nr=0;
          active_receiver->nr2=1;
          mode_settings[vfo[active_receiver->id].mode].nr=0;
          mode_settings[vfo[active_receiver->id].mode].nr2=1;
        } else if(active_receiver->nr==0 && active_receiver->nr2==1) {
          active_receiver->nr=0;
          active_receiver->nr2=0;
          mode_settings[vfo[active_receiver->id].mode].nr=0;
          mode_settings[vfo[active_receiver->id].mode].nr2=0;
        }
        update_noise();
        break;
      case NB:
        if(active_receiver->nb==0 && active_receiver->nb2==0) {
          active_receiver->nb=1;
          active_receiver->nb2=0;
          mode_settings[vfo[active_receiver->id].mode].nb=1;
          mode_settings[vfo[active_receiver->id].mode].nb2=0;
        } else if(active_receiver->nb==1 && active_receiver->nb2==0) {
          active_receiver->nb=0;
          active_receiver->nb2=1;
          mode_settings[vfo[active_receiver->id].mode].nb=0;
          mode_settings[vfo[active_receiver->id].mode].nb2=1;
        } else if(active_receiver->nb==0 && active_receiver->nb2==1) {
          active_receiver->nb=0;
          active_receiver->nb2=0;
          mode_settings[vfo[active_receiver->id].mode].nb=0;
          mode_settings[vfo[active_receiver->id].mode].nb2=0;
        }
        update_noise();
        break;
      case SNB:
        if(active_receiver->snb==0) {
          active_receiver->snb=1;
          mode_settings[vfo[active_receiver->id].mode].snb=1;
        } else {
          active_receiver->snb=0;
          mode_settings[vfo[active_receiver->id].mode].snb=0;
        }
        update_noise();
        break;
      case RIT:
        vfo_rit_update(active_receiver->id);
        break;
      case RIT_CLEAR:
        vfo_rit_clear(active_receiver->id);
        break;
      case RIT_MINUS:
	vfo_rit(active_receiver->id,-1);
        break;
      case RIT_PLUS:
	vfo_rit(active_receiver->id,1);
        break;
      case XIT:
        if(can_transmit) {
          transmitter->xit_enabled=transmitter->xit_enabled==1?0:1;
          if(protocol==NEW_PROTOCOL) {
            schedule_high_priority();
          }
        }
        g_idle_add(ext_vfo_update, NULL);
        break;
      case XIT_CLEAR:
        if(can_transmit) {
          transmitter->xit=0;
          g_idle_add(ext_vfo_update, NULL);
        }
        break;
      case XIT_MINUS:
        if(can_transmit) {
          double value=(double)transmitter->xit;
          value-=(double)rit_increment;
          if(value<-10000.0) {
            value=-10000.0;
          } else if(value>10000.0) {
            value=10000.0;
          }
          transmitter->xit=(int)value;
          if(protocol==NEW_PROTOCOL) {
            schedule_high_priority();
          }
          g_idle_add(ext_vfo_update,NULL);
	}
	break;
      case XIT_PLUS:
        if(can_transmit) {
          double value=(double)transmitter->xit;
          value+=(double)rit_increment;
          if(value<-10000.0) {
            value=-10000.0;
          } else if(value>10000.0) {
            value=10000.0;
          }
          transmitter->xit=(int)value;
          if(protocol==NEW_PROTOCOL) {
            schedule_high_priority();
          }
          g_idle_add(ext_vfo_update,NULL);
        }
        break;
      case BAND_PLUS:
        {
        long long frequency_min=radio->frequency_min;
        long long frequency_max=radio->frequency_max;
        int b=vfo[active_receiver->id].band;
        BAND *band;
        int found=0;
        while(!found) {
          b++;
          if(b>=BANDS+XVTRS) b=0;
          band=(BAND*)band_get_band(b);
          if(strlen(band->title)>0) {
            if(b<BANDS) {
              if(!(band->frequencyMin==0.0 && band->frequencyMax==0.0)) {
                if(band->frequencyMin<frequency_min || band->frequencyMax>frequency_max) {
                  continue;
                }
              }
            }
            vfo_band_changed(active_receiver->id,b);
            found=1;
          }
        }
        }
        break;
      case BAND_MINUS:
        {
        long long frequency_min=radio->frequency_min;
        long long frequency_max=radio->frequency_max;
        int b=vfo[active_receiver->id].band;
        BAND *band;
        int found=0;
        while(!found) {
          b--;
          if(b<0) b=BANDS+XVTRS-1;
          band=(BAND*)band_get_band(b);
          if(strlen(band->title)>0) {
            if(b<BANDS) {
              if(band->frequencyMin<frequency_min || band->frequencyMax>frequency_max) {
                continue;
              }
            }
            vfo_band_changed(active_receiver->id,b);
            found=1;
          }
        }
        }
        break;
      case BANDSTACK_PLUS:
        {
        BAND *band=band_get_band(vfo[active_receiver->id].band);
        BANDSTACK *bandstack=band->bandstack;
        int b=vfo[active_receiver->id].bandstack+1;
        if(b>=bandstack->entries) b=0;
        vfo_bandstack_changed(b);
        }
        break;
      case BANDSTACK_MINUS:
        {
        BAND *band=band_get_band(vfo[active_receiver->id].band);
        BANDSTACK *bandstack=band->bandstack;
        int b=vfo[active_receiver->id].bandstack-1;
        if(b<0) b=bandstack->entries-1;;
        vfo_bandstack_changed(b);
        }
        break;
      case MODE_PLUS:
        {
        int mode=vfo[active_receiver->id].mode;
        mode++;
        if(mode>=MODES) mode=0;
        vfo_mode_changed(mode);
        }
        break;
      case MODE_MINUS:
        {
        int mode=vfo[active_receiver->id].mode;
        mode--;
        if(mode<0) mode=MODES-1;
        vfo_mode_changed(mode);
        }
        break;
      case FILTER_PLUS:
        {
        int f=vfo[active_receiver->id].filter-1;
        if(f<0) f=FILTERS-1;
        vfo_filter_changed(f);
        }
        break;
      case FILTER_MINUS:
        {
        int f=vfo[active_receiver->id].filter+1;
        if(f>=FILTERS) f=0;
        vfo_filter_changed(f);
        }
        break;
      case A_TO_B:
        vfo_a_to_b();
        break;
      case B_TO_A:
        vfo_b_to_a();
        break;
      case A_SWAP_B:
        vfo_a_swap_b();
        break;
      case LOCK:
#ifdef CLIENT_SERVER
        if(radio_is_remote) {
          send_lock(client_socket,locked==1?0:1);
        } else {
#endif
          locked=locked==1?0:1;
          g_idle_add(ext_vfo_update, NULL);
#ifdef CLIENT_SERVER
        }
#endif
        break;
      case CTUN:
        vfo[active_receiver->id].ctun=!vfo[active_receiver->id].ctun;
        if(!vfo[active_receiver->id].ctun) {
          vfo[active_receiver->id].offset=0;
        }
        vfo[active_receiver->id].ctun_frequency=vfo[active_receiver->id].frequency;
        set_offset(receiver[active_receiver->id],vfo[active_receiver->id].offset);
        g_idle_add(ext_vfo_update, NULL);
        break;
      case AGC:
        active_receiver->agc++;
        if(active_receiver->agc>+AGC_LAST) {
          active_receiver->agc=0;
        }
        set_agc(active_receiver, active_receiver->agc);
        g_idle_add(ext_vfo_update, NULL);
        break;
      case SPLIT:
        if(can_transmit) {
          split=split==1?0:1;
          tx_set_mode(transmitter,get_tx_mode());
          g_idle_add(ext_vfo_update, NULL);
        }
        break;
      case DIVERSITY:
        diversity_enabled=diversity_enabled==1?0:1;
        if (protocol == NEW_PROTOCOL) {
          schedule_high_priority();
          schedule_receive_specific();
        }
        g_idle_add(ext_vfo_update, NULL);
        break;
      case SAT:
        switch(sat_mode) {
          case SAT_NONE:
            sat_mode=SAT_MODE;
            break;
          case SAT_MODE:
            sat_mode=RSAT_MODE;
            break;
          case RSAT_MODE:
            sat_mode=SAT_NONE;
            break;
        }
        g_idle_add(ext_vfo_update, NULL);
        break;
      case MENU_AGC:
        start_agc();
        break;
      case MENU_BAND:
        start_band();
        break;
      case MENU_BANDSTACK:
        start_bandstack();
        break;
      case MENU_MODE:
        start_mode();
        break;
      case MENU_NOISE:
        start_noise();
        break;
      case MENU_FILTER:
        start_filter();
        break;
      case MENU_FREQUENCY:
        start_vfo(active_receiver->id);
        break;
      case MENU_MEMORY:
        start_store();
        break;
      case MENU_DIVERSITY:
        start_diversity();
        break;
#ifdef PURESIGNAL
      case MENU_PS:
        start_ps();
        break;
#endif
      case MUTE:
        active_receiver->mute_radio=!active_receiver->mute_radio;
        break;
      case PAN_MINUS:
        update_pan(-100.0);
        break;
      case PAN_PLUS:
        update_pan(+100.0);
        break;
      case ZOOM_MINUS:
        update_zoom(-1);
        break;
      case ZOOM_PLUS:
        update_zoom(+1);
        break;
      default:
        g_print("%s: UNKNOWN PRESSED SWITCH ACTION %d\n",__FUNCTION__,a->action);
        break;
    }
  } else if(a->state==RELEASED) {
    // only switch functions that increment/decrement while pressed
    switch(a->action) {
      default:
        break;
    }
  }
  g_free(data);
  return 0;
}

