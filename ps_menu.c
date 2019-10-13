/*
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

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wdsp.h>

#include "button_text.h"
#include "new_menu.h"
#include "radio.h"
#include "toolbar.h"
#include "transmitter.h"
#include "new_protocol.h"
#include "vfo.h"
#include "ext.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *feedback_l;
static GtkWidget *correcting_l;
static GtkWidget *get_pk;
static GtkWidget *set_pk;
static GtkWidget *tx_att;

/*
 * PureSignal 2.0 default parameters and declarations
 */

static double ampdelay  = 20e-9;   // 20 nsec
static int    ints      = 16;
static int    spi       = 256;
static int    stbl      = 0;
static int    map       = 1;
static int    pin       = 1;
static double ptol      = 0.8;
static double moxdelay  = 0.1;
static double loopdelay = 0.0;

//
// The following declarations are missing in wdsp.h
// We put them here because sooner or later there will be
// a corrected version of WDSP where this is contained
//
extern void SetPSIntsAndSpi (int channel, int ints, int spi);
extern void SetPSStabilize (int channel, int stbl);
extern void SetPSMapMode (int channel, int map);
extern void SetPSPinMode (int channel, int pin);

//
// Todo: create buttons to change these values
//


static int running=0;

#define INFO_SIZE 16

static GtkWidget *entry[INFO_SIZE];

static void destroy_cb(GtkWidget *widget, gpointer data) {
  running=0;
  // wait for one instance of info_thread to complete
  usleep(100000);
}

static void cleanup() {
  running=0;
  // wait for one instance of info_thread to complete
  usleep(100000);

  if(transmitter->twotone) {
    tx_set_twotone(transmitter,0);
  }
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

//
// This is called every 100 msec and therefore
// must be a state machine
//
static int info_thread(gpointer arg) {
  static int info[INFO_SIZE];
  int i;
  gchar label[20];
  static int old5=0;
  static int old5_2=0;
  static int old14=0;
  int display;
  static int state=0;

  if (!running) return FALSE;

    GetPSInfo(transmitter->id,&info[0]);
    for(i=0;i<INFO_SIZE;i++) {
      display=1;
      sprintf(label,"%d",info[i]);
      switch(i) {
        case 4:
          break;
        case 5:
          if(info[i]!=old5) {
            old5=info[5];
            if(info[4]>181)  {
              gtk_label_set_markup(GTK_LABEL(feedback_l),"<span color='blue'>Feedback Lvl</span>");
            } else if(info[4]>128)  {
              gtk_label_set_markup(GTK_LABEL(feedback_l),"<span color='green'>Feedback Lvl</span>");
            } else if(info[4]>90)  {
              gtk_label_set_markup(GTK_LABEL(feedback_l),"<span color='yellow'>Feedback Lvl</span>");
            } else {
              gtk_label_set_markup(GTK_LABEL(feedback_l),"<span color='red'>Feedback Lvl</span>");
            }
          }
          break;
        case 6:
          break;
        case 13:
          break;
        case 14:
          if(info[14]!=old14) {
            old14=info[14];
            if(info[14]==0) {
              gtk_label_set_markup(GTK_LABEL(correcting_l),"<span color='black'>Correcting</span>");
            } else {
              gtk_label_set_markup(GTK_LABEL(correcting_l),"<span color='green'>Correcting</span>");
            }
          }
          display=0;
          break;
        case 15:
          switch(info[i]) {
            case 0:
              strcpy(label,"RESET");
              break;
            case 1:
              strcpy(label,"WAIT");
              break;
            case 2:
              strcpy(label,"MOXDELAY");
              break;
            case 3:
              strcpy(label,"SETUP");
              break;
            case 4:
              strcpy(label,"COLLECT");
              break;
            case 5:
              strcpy(label,"MOXCHECK");
              break;
            case 6:
              strcpy(label,"CALC");
              break;
            case 7:
              strcpy(label,"DELAY");
              break;
            case 8:
              strcpy(label,"STAYON");
              break;
            case 9:
              strcpy(label,"TURNON");
              break;
            default:
              display=0;
              break;
          }
          break;
        default:
          display=0;
          break;
      }
      if(display && entry[i] != NULL) {
          gtk_entry_set_text(GTK_ENTRY(entry[i]),label);
      }
    }

    sprintf(label,"%d",transmitter->attenuation);
    gtk_entry_set_text(GTK_ENTRY(tx_att),label);

    double pk;

    GetPSMaxTX(transmitter->id,&pk);
    sprintf(label,"%6.3f", pk);
    gtk_entry_set_text(GTK_ENTRY(get_pk),label);

    if (transmitter->auto_on) {
      double ddb;
      static int new_att;
      int newcal=info[5]!=old5_2;
      old5_2=info[5];
      switch(state) {
        case 0:
          if(newcal && (info[4]>181 || (info[4]<=128 && transmitter->attenuation>0))) {
	    if (info[4] > 0) {
              ddb= 20.0 * log10((double)info[4]/152.293);
	    } else {
	      // This happens when the "Drive" slider is moved to zero
	      ddb= -100.0;
	    }
            new_att=transmitter->attenuation + (int)ddb;
	    // keep new value of attenuation in allowed range
	    if (new_att <  0) new_att= 0;
	    if (new_att > 31) new_att=31;
	    // A "PS reset" is only necessary if the attenuation
	    // has actually changed. This prevents firing "reset"
	    // constantly if the SDR board does not have a TX attenuator
	    // (in this case, att will fast reach 31 and stay there if the
	    // feedback level is too high).
	    // Actually, we first adjust the attenuation (state=0),
	    // then do a PS reset (state=1), and then restart PS (state=2).
            if (transmitter->attenuation != new_att) {
              SetPSControl(transmitter->id, 1, 0, 0, 0);
	      transmitter->attenuation=new_att;
	      if (protocol == NEW_PROTOCOL) {
		schedule_transmit_specific();
	      }
              state=1;
	    }
          }
          break;
        case 1:
	  state=2;
          SetPSControl(transmitter->id, 1, 0, 0, 0);
	  break;
        case 2:
          state=0;
          SetPSControl(transmitter->id, 0, 0, 1, 0);
          break;
      }
    }
    return TRUE;
}

//
// select route for PS feedback signal.
// note: we need new code numbers such that we can
//       distinguish "normal RX" and "feedback" use
//       of EXT1. In the latter case, any RX filters have
//       to by bypassed, which is of particular importance
//       on the 6m band.
//       
//
static void ps_ant_cb(GtkWidget *widget, gpointer data) {
  int val = GPOINTER_TO_INT(data);
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    switch (val) {
      case 0:	// Internal
      case 6:	// EXT1; RX filters switched to "BYPASS"
      case 7:	// Bypass
	receiver[PS_RX_FEEDBACK]->alex_antenna = val;
	if (protocol == NEW_PROTOCOL) {
	  schedule_high_priority();
	}
	break;
    }
  }
}

static void enable_cb(GtkWidget *widget, gpointer data) {
  g_idle_add(ext_tx_set_ps,GINT_TO_POINTER(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))));
}

static void auto_cb(GtkWidget *widget, gpointer data) {
  transmitter->auto_on=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  // switching OFF auto sets the attenuation to zero
  if(!transmitter->auto_on) {
    transmitter->attenuation=0;
  }

}

static void resume_cb(GtkWidget *widget, gpointer data) {
  // Set the attenuation to zero if auto-adjusting and resuming.
  // A very high attenuation value here could lead to no PS calculation
  // done in WDSP, and hence no attenuation adjustment.
  // If not auto-adjusting, do not change attenuation value.
  if(transmitter->auto_on) {
    transmitter->attenuation=0;
  }
  if (transmitter->puresignal) {
    SetPSControl(transmitter->id, 0, 0, 1, 0);
  }
}

static void feedback_cb(GtkWidget *widget, gpointer data) {
  if(transmitter->feedback==0)  {
    transmitter->feedback=1;
    set_button_text_color(widget,"red");
  } else {
    transmitter->feedback=0;
    set_button_text_color(widget,"black");
  }
}

static void reset_cb(GtkWidget *widget, gpointer data) {
  if (transmitter->puresignal) {
    SetPSControl(transmitter->id, 1, 0, 0, 0);
  }
}

static void twotone_cb(GtkWidget *widget, gpointer data) {
  int state=transmitter->twotone?0:1;
  tx_set_twotone(transmitter,state);
  if(state) {
    set_button_text_color(widget,"red");
  } else {
    set_button_text_color(widget,"black");
  }
}

void ps_menu(GtkWidget *parent) {
  GtkWidget *b;
  int i;

  parent_window=parent;

  dialog=gtk_dialog_new();
  g_signal_connect (dialog, "destroy", G_CALLBACK(destroy_cb), NULL);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Pure Signal");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();

  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  int row=0;
  int col=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  row++;
  col=0;

  GtkWidget *enable_b=gtk_check_button_new_with_label("Enable PS");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_b), transmitter->puresignal);
  gtk_grid_attach(GTK_GRID(grid),enable_b,col,row,1,1);
  g_signal_connect(enable_b,"toggled",G_CALLBACK(enable_cb),NULL);

  col++;

  GtkWidget *twotone_b=gtk_button_new_with_label("Two Tone");
  gtk_widget_show(twotone_b);
  gtk_grid_attach(GTK_GRID(grid),twotone_b,col,row,1,1);
  g_signal_connect(twotone_b,"pressed",G_CALLBACK(twotone_cb),NULL);
  if(transmitter->twotone) {
    set_button_text_color(twotone_b,"red");
  }

  col++;

  GtkWidget *auto_b=gtk_check_button_new_with_label("Auto Attenuate");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_b), transmitter->auto_on);
  gtk_grid_attach(GTK_GRID(grid),auto_b,col,row,1,1);
  g_signal_connect(auto_b,"toggled",G_CALLBACK(auto_cb),NULL);
  
  col++;

  GtkWidget *reset_b=gtk_button_new_with_label("OFF");
  gtk_widget_show(reset_b);
  gtk_grid_attach(GTK_GRID(grid),reset_b,col,row,1,1);
  g_signal_connect(reset_b,"pressed",G_CALLBACK(reset_cb),NULL);

  col++;

  GtkWidget *resume_b=gtk_button_new_with_label("Restart");
  gtk_grid_attach(GTK_GRID(grid),resume_b,col,row,1,1);
  g_signal_connect(resume_b,"pressed",G_CALLBACK(resume_cb),NULL);

  col++;

  GtkWidget *feedback_b=gtk_button_new_with_label("MON");
  gtk_widget_show(feedback_b);
  gtk_grid_attach(GTK_GRID(grid),feedback_b,col,row,1,1);
  g_signal_connect(feedback_b,"pressed",G_CALLBACK(feedback_cb),NULL);
  if(transmitter->feedback)  {
    set_button_text_color(feedback_b,"red");
  }

  row++;
  col=0;

  //
  // Selection of feedback path for PURESIGNAL
  //
  // AUTO		Using internal feedback (to ADC0)
  // EXT1		Using EXT1 jacket (to ADC0), ANAN-7000: still uses AUTO
  // BYPASS		Using BYPASS. Not available with ANAN-100/200 up to Rev. 16 filter boards
  //
  // In fact, we provide the possibility of using EXT1 only to support these older
  // (before February, 2015) ANAN-100/200 devices.
  //
  GtkWidget *ps_ant_label=gtk_label_new("PS FeedBk ANT:");
  gtk_widget_show(ps_ant_label);
  gtk_grid_attach(GTK_GRID(grid), ps_ant_label, col, row, 1, 1);
  col++;

  GtkWidget *ps_ant_auto=gtk_radio_button_new_with_label(NULL,"Internal");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ps_ant_auto), (receiver[PS_RX_FEEDBACK]->alex_antenna == 0) );
  gtk_widget_show(ps_ant_auto);
  gtk_grid_attach(GTK_GRID(grid), ps_ant_auto, col, row, 1, 1);
  g_signal_connect(ps_ant_auto,"toggled", G_CALLBACK(ps_ant_cb), (gpointer) (long) 0);
  col++;

  //
  // ANAN-7000:           you can use both EXT1 and ByPass (although Bypass is to be preferred),
  // ANAN-100/200 old PA: you can only use EXT1
  // ANAN-100/200 new PA: you can only use ByPass
  //
  // We would need much logic at various places to guarantee that non-reasonable settings
  // cannot become effective, therefore we let the user choose both possibilities on all hardware
  //
  GtkWidget *ps_ant_ext1=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ps_ant_auto),"EXT1");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ps_ant_ext1), (receiver[PS_RX_FEEDBACK]->alex_antenna==6) );
  gtk_widget_show(ps_ant_ext1);
  gtk_grid_attach(GTK_GRID(grid), ps_ant_ext1, col, row, 1, 1);
  g_signal_connect(ps_ant_ext1,"toggled", G_CALLBACK(ps_ant_cb), (gpointer) (long) 6);
  col++;

  GtkWidget *ps_ant_ext2=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ps_ant_auto),"ByPass");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ps_ant_ext2), (receiver[PS_RX_FEEDBACK]->alex_antenna==7) );
  gtk_widget_show(ps_ant_ext2);
  gtk_grid_attach(GTK_GRID(grid), ps_ant_ext2, col, row, 1, 1);
  g_signal_connect(ps_ant_ext2,"toggled", G_CALLBACK(ps_ant_cb), (gpointer) (long) 7);
  col++;

  row++;

  col=0;
  feedback_l=gtk_label_new("Feedback Lvl");
  gtk_widget_show(feedback_l);
  gtk_grid_attach(GTK_GRID(grid),feedback_l,col,row,1,1);

  col++;

  correcting_l=gtk_label_new("Correcting");
  gtk_widget_show(correcting_l);
  gtk_grid_attach(GTK_GRID(grid),correcting_l,col,row,1,1);

  row++;
  col=0;

  for(i=0;i<INFO_SIZE;i++) {
    int display=1;
    char label[16];
    switch(i) {
      case 4:
        strcpy(label,"feedbk");
        break;
      case 5:
        strcpy(label,"cor.cnt");
        break;
      case 6:
        strcpy(label,"sln.chk");
        break;
      case 13:
        strcpy(label,"dg.cnt");
        break;
      case 15:
        strcpy(label,"status");
        break;
      default:
        display=0;
        break;
    }
    if(display) {
      GtkWidget *lbl=gtk_label_new(label);
      entry[i]=gtk_entry_new();
      gtk_entry_set_max_length(GTK_ENTRY(entry[i]), 10);
      gtk_grid_attach(GTK_GRID(grid),lbl,col,row,1,1);
      col++;
      gtk_grid_attach(GTK_GRID(grid),entry[i],col,row,1,1);
      gtk_entry_set_width_chars(GTK_ENTRY(entry[i]), 10);
      col++;
      if(col>=6) {
        row++;
        col=0;
      }
    } else {
      entry[i]=NULL;
    }
  }

  row++;
  col=0;
  GtkWidget *lbl=gtk_label_new("GetPk");
  gtk_grid_attach(GTK_GRID(grid),lbl,col,row,1,1);
  col++;

  get_pk=gtk_entry_new();
  gtk_grid_attach(GTK_GRID(grid),get_pk,col,row,1,1);
  gtk_entry_set_width_chars(GTK_ENTRY(get_pk), 10);
  col++;

  lbl=gtk_label_new("SetPk");
  gtk_grid_attach(GTK_GRID(grid),lbl,col,row,1,1);
  col++;

  double pk;
  char pk_text[16];
  GetPSHWPeak(transmitter->id,&pk);
  sprintf(pk_text,"%6.3f",pk);
  set_pk=gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(set_pk),pk_text);
  gtk_grid_attach(GTK_GRID(grid),set_pk,col,row,1,1);
  gtk_entry_set_width_chars(GTK_ENTRY(set_pk), 10);
  col++;

  lbl=gtk_label_new("TX ATT");
  gtk_grid_attach(GTK_GRID(grid),lbl,col,row,1,1);
  col++;

  tx_att=gtk_entry_new();
  gtk_grid_attach(GTK_GRID(grid),tx_att,col,row,1,1);
  gtk_entry_set_width_chars(GTK_ENTRY(tx_att), 10);

  gtk_container_add(GTK_CONTAINER(content),grid);
  sub_menu=dialog;

// DL1YCF: This is not the default setting,
// but in my experience in behaves better in difficult situations.
// This is commented out because it is not recommended by the
// WDSP manual.
//  ints=8;
//  spi=512;
//  ampdelay=100e-9;

  SetPSIntsAndSpi(transmitter->id, ints, spi);
  SetPSStabilize(transmitter->id, stbl);
  SetPSMapMode(transmitter->id, map);
  SetPSPinMode(transmitter->id, pin);
  SetPSPtol(transmitter->id, ptol);
  SetPSMoxDelay(transmitter->id, moxdelay);
  SetPSTXDelay(transmitter->id, ampdelay);
  SetPSLoopDelay(transmitter->id, loopdelay);

  running=1;
  g_timeout_add((guint) 100, info_thread, NULL);

  gtk_widget_show_all(dialog);

}
