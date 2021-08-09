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

    //g_print("%s: action=%d type=%d val=%d\n",__FUNCTION__,action,type,val);

    switch(type) {
      case MIDI_KEY:
	//
	// CW_LEFT, CW_RIGHT, and CW_KEYER have to be handled with
	// minimum latency, so these are not put to the GTK idle queue
	// but rather handled immediately
	//
	if(action==CW_LEFT || action==CW_RIGHT) {
#ifdef LOCALCW
	  keyer_event(action==CW_LEFT,val);
#else
	  g_print("MIDI CW key but compiled without LOCALCW\n");
#endif
        } else if (action == CW_KEYER) {
          //
          // This is a CW key-up/down which uses functions from the keyer
          // that by-pass the interrupt-driven standard action.
          // It is intended to
          // be used with external keyers that send key-up/down messages via
          // MIDI using this command.
          //
          // NO BREAK-IN! The keyer has to take care of sending MIDI PTT
          // on/off messages at appropriate times, or the operator has to
          // manually engage/disengage PTT
          //
          // Since this if for immediate key-down, it does not rely on LOCALCW
          //
          if (val != 0 && cw_keyer_internal == 0) {
            cw_key_down=960000;  // max. 20 sec to protect hardware
            cw_key_up=0;
            cw_key_hit=1;
          } else {
            cw_key_down=0;
            cw_key_up=0;
          }
	} else {
	  a=g_new(PROCESS_ACTION,1);
	  a->action=action;
	  a->mode=val?PRESSED:RELEASED;
	  g_idle_add(process_action,a);
	}
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
}
