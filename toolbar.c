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

#include <gtk/gtk.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef GPIO
#include "gpio.h"
#endif
#include "toolbar.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "discovered.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "vfo.h"
#include "alex.h"
#include "agc.h"
#include "channel.h"
#include "wdsp.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "property.h"
#include "new_menu.h"
#include "button_text.h"
#include "ext.h"	

int function=0;

static int width;
static int height;

static GtkWidget *parent_window;
static GtkWidget *toolbar;

static GtkWidget *last_dialog;

static GtkWidget *sim_mox;
static GtkWidget *sim_s1;
static GtkWidget *sim_s2;
static GtkWidget *sim_s3;
static GtkWidget *sim_s4;
static GtkWidget *sim_s5;
static GtkWidget *sim_s6;
static GtkWidget *sim_function;


static GtkWidget *last_band;
static GtkWidget *last_bandstack;
static GtkWidget *last_mode;
static GtkWidget *last_filter;

static GdkRGBA white;
static GdkRGBA gray;

static gint rit_plus_timer=-1;
static gint rit_minus_timer=-1;
static gint xit_plus_timer=-1;
static gint xit_minus_timer=-1;

static gboolean rit_timer_cb(gpointer data) {
  int i=GPOINTER_TO_INT(data);
  vfo[active_receiver->id].rit+=(i*rit_increment);
  if(vfo[active_receiver->id].rit>10000) vfo[active_receiver->id].rit=10000;
  if(vfo[active_receiver->id].rit<-10000) vfo[active_receiver->id].rit=-10000;
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
  g_idle_add(ext_vfo_update,NULL);
  return TRUE;
}

static gboolean xit_timer_cb(gpointer data) {
  int i=GPOINTER_TO_INT(data);
  transmitter->xit+=(i*rit_increment);
  if(transmitter->xit>10000) transmitter->xit=10000;
  if(transmitter->xit<-10000) transmitter->xit=-10000;
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
  g_idle_add(ext_vfo_update,NULL);
  return TRUE;
}

void update_toolbar_labels() {
  switch(function) {
    case 0:
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"Mox");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"");
      }
      gtk_button_set_label(GTK_BUTTON(sim_s1),"Band");
      gtk_button_set_label(GTK_BUTTON(sim_s2),"BStack");
      gtk_button_set_label(GTK_BUTTON(sim_s3),"Mode");
      gtk_button_set_label(GTK_BUTTON(sim_s4),"Filter");
      gtk_button_set_label(GTK_BUTTON(sim_s5),"Noise");
      gtk_button_set_label(GTK_BUTTON(sim_s6),"AGC");
      set_button_text_color(sim_s1,"black");
      set_button_text_color(sim_s2,"black");
      break;
    case 1:
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"Mox");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"");
      }
      gtk_button_set_label(GTK_BUTTON(sim_s1),"Lock");
      gtk_button_set_label(GTK_BUTTON(sim_s2),"CTUN");
      gtk_button_set_label(GTK_BUTTON(sim_s3),"A>B");
      gtk_button_set_label(GTK_BUTTON(sim_s4),"A<B");
      gtk_button_set_label(GTK_BUTTON(sim_s5),"A<>B");
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_s6),"Split");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_s6),"");
      }
      break;
    case 2:
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"Tune");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"");
      }
      gtk_button_set_label(GTK_BUTTON(sim_s1),"Freq");
      gtk_button_set_label(GTK_BUTTON(sim_s2),"Mem");
      gtk_button_set_label(GTK_BUTTON(sim_s3),"RIT");
      gtk_button_set_label(GTK_BUTTON(sim_s4),"RIT+");
      gtk_button_set_label(GTK_BUTTON(sim_s5),"RIT-");
      gtk_button_set_label(GTK_BUTTON(sim_s6),"RIT CL");
      break;
    case 3:
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"Mox");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"");
      }
      gtk_button_set_label(GTK_BUTTON(sim_s1),"Freq");
      gtk_button_set_label(GTK_BUTTON(sim_s2),"Mem");
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_s3),"XIT");
        gtk_button_set_label(GTK_BUTTON(sim_s4),"XIT+");
        gtk_button_set_label(GTK_BUTTON(sim_s5),"XIT-");
        gtk_button_set_label(GTK_BUTTON(sim_s6),"XIT CL");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_s3),"");
        gtk_button_set_label(GTK_BUTTON(sim_s4),"");
        gtk_button_set_label(GTK_BUTTON(sim_s5),"");
        gtk_button_set_label(GTK_BUTTON(sim_s6),"");
      }
      break;
    case 4:
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"Mox");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"");
      }
      gtk_button_set_label(GTK_BUTTON(sim_s1),"Freq");
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_s2),"Split");
        gtk_button_set_label(GTK_BUTTON(sim_s3),"Duplex");
        gtk_button_set_label(GTK_BUTTON(sim_s4),"SAT");
        gtk_button_set_label(GTK_BUTTON(sim_s5),"RSAT");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_s2),"");
        gtk_button_set_label(GTK_BUTTON(sim_s3),"");
        gtk_button_set_label(GTK_BUTTON(sim_s4),"");
        gtk_button_set_label(GTK_BUTTON(sim_s5),"");
      }
      gtk_button_set_label(GTK_BUTTON(sim_s6),"");
      break;
    case 5:
      if(can_transmit) {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"Tune");
        if(OCtune!=0 && OCfull_tune_time!=0) {
          gtk_button_set_label(GTK_BUTTON(sim_s1),"Full");
        } else {
          gtk_button_set_label(GTK_BUTTON(sim_s1),"");
        }
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_mox),"");
        gtk_button_set_label(GTK_BUTTON(sim_s1),"");
      }
      if(OCtune!=0 && OCmemory_tune_time!=0) {
        gtk_button_set_label(GTK_BUTTON(sim_s2),"Memory");
      } else {
        gtk_button_set_label(GTK_BUTTON(sim_s2),"");
      }
      gtk_button_set_label(GTK_BUTTON(sim_s3),"Band");
      gtk_button_set_label(GTK_BUTTON(sim_s4),"Mode");
      gtk_button_set_label(GTK_BUTTON(sim_s5),"Filter");
      gtk_button_set_label(GTK_BUTTON(sim_s6),"");
      if(full_tune) {
        set_button_text_color(sim_s1,"red");
      }
      if(memory_tune) {
        set_button_text_color(sim_s2,"red");
      }
      break;
  }
}

static void close_cb(GtkWidget *widget, gpointer data) {
  gtk_widget_destroy(last_dialog);
  last_dialog=NULL;
}

void band_cb(GtkWidget *widget, gpointer data) {
  start_band();
}

void bandstack_cb(GtkWidget *widget, gpointer data) {
  start_bandstack();
}

void mode_cb(GtkWidget *widget, gpointer data) {
  start_mode();
}

void filter_cb(GtkWidget *widget, gpointer data) {
  start_filter();
}

void agc_cb(GtkWidget *widget, gpointer data) {
  start_agc();
}

void noise_cb(GtkWidget *widget, gpointer data) {
  start_noise();
}

void ctun_cb (GtkWidget *widget, gpointer data) {
  int id=active_receiver->id;
  vfo[id].ctun=vfo[id].ctun==1?0:1;
  if(!vfo[id].ctun) {
    vfo[id].offset=0;
  }
  vfo[id].ctun_frequency=vfo[id].frequency;
  set_offset(active_receiver,vfo[id].offset);
  g_idle_add(ext_vfo_update,NULL);
}

static void atob_cb (GtkWidget *widget, gpointer data) {
  vfo_a_to_b();
}

static void btoa_cb (GtkWidget *widget, gpointer data) {
  vfo_b_to_a();
}

static void aswapb_cb (GtkWidget *widget, gpointer data) {
  vfo_a_swap_b();
}

static void split_cb (GtkWidget *widget, gpointer data) {
  g_idle_add(ext_split_toggle,NULL);
}

static void duplex_cb (GtkWidget *widget, gpointer data) {
  if(can_transmit && !isTransmitting()) {
    duplex=(duplex==1)?0:1;
    g_idle_add(ext_set_duplex,NULL);
  }
}

static void sat_cb (GtkWidget *widget, gpointer data) {
  if(can_transmit) {
    if(sat_mode==SAT_MODE) {
      sat_mode=SAT_NONE;
    } else {
      sat_mode=SAT_MODE;
    }
    g_idle_add(ext_vfo_update,NULL);
  }
}

static void rsat_cb (GtkWidget *widget, gpointer data) {
  if(can_transmit) {
    if(sat_mode==RSAT_MODE) {
      sat_mode=SAT_NONE;
    } else {
      sat_mode=RSAT_MODE;
    }
    g_idle_add(ext_vfo_update,NULL);
  }
}

static void rit_enable_cb(GtkWidget *widget, gpointer data) {
  vfo[active_receiver->id].rit_enabled=vfo[active_receiver->id].rit_enabled==1?0:1;
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
  g_idle_add(ext_vfo_update,NULL);
}

static void rit_cb(GtkWidget *widget, gpointer data) {
  int i=GPOINTER_TO_INT(data);
  vfo[active_receiver->id].rit+=i*rit_increment;
  if(vfo[active_receiver->id].rit>10000) vfo[active_receiver->id].rit=10000;
  if(vfo[active_receiver->id].rit<-10000) vfo[active_receiver->id].rit=-10000;
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
  g_idle_add(ext_vfo_update,NULL);
  if(i<0) {
    rit_minus_timer=g_timeout_add(200,rit_timer_cb,GINT_TO_POINTER(i));
  } else {
    rit_plus_timer=g_timeout_add(200,rit_timer_cb,GINT_TO_POINTER(i));
  }
}

static void rit_clear_cb(GtkWidget *widget, gpointer data) {
  vfo[active_receiver->id].rit=0;
  g_idle_add(ext_vfo_update,NULL);
}

static void xit_enable_cb(GtkWidget *widget, gpointer data) {
  if(can_transmit) {
    transmitter->xit_enabled=transmitter->xit_enabled==1?0:1;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
    g_idle_add(ext_vfo_update,NULL);
  }
}

static void xit_cb(GtkWidget *widget, gpointer data) {
  if(can_transmit) {
    int i=GPOINTER_TO_INT(data);
    transmitter->xit+=i*rit_increment;
    if(transmitter->xit>10000) transmitter->xit=10000;
    if(transmitter->xit<-10000) transmitter->xit=-10000;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
    g_idle_add(ext_vfo_update,NULL);
    if(i<0) {
      xit_minus_timer=g_timeout_add(200,xit_timer_cb,GINT_TO_POINTER(i));
    } else {
      xit_plus_timer=g_timeout_add(200,xit_timer_cb,GINT_TO_POINTER(i));
    }
  }
}

static void xit_clear_cb(GtkWidget *widget, gpointer data) {
  if(can_transmit) {
    transmitter->xit=0;
    g_idle_add(ext_vfo_update,NULL);
  }
}

static void freq_cb(GtkWidget *widget, gpointer data) {
  start_vfo(active_receiver->id);
}

static void mem_cb(GtkWidget *widget, gpointer data) {
  start_store();
}

static void vox_cb(GtkWidget *widget, gpointer data) {
  vox_enabled=vox_enabled==1?0:1;
  g_idle_add(ext_vfo_update,NULL);
}

static void stop() {
  if(protocol==ORIGINAL_PROTOCOL) {
    old_protocol_stop();
  } else {
    new_protocol_stop();
  }
#ifdef GPIO
  gpio_close();
#endif
#ifdef WIRIINGPI
  gpio_close();
#endif
}

static void yes_cb(GtkWidget *widget, gpointer data) {
  stop();
  _exit(0);
}

static void halt_cb(GtkWidget *widget, gpointer data) {
  stop();
  system("shutdown -h -P now");
  _exit(0);
}

static void exit_cb(GtkWidget *widget, gpointer data) {

  radioSaveState();

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Exit",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *label=gtk_label_new("Exit?");
  //gtk_widget_override_font(label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid),label,1,0,1,1);

  GtkWidget *b_yes=gtk_button_new_with_label("Yes");
  //gtk_widget_override_font(b_yes, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(b_yes);
  gtk_grid_attach(GTK_GRID(grid),b_yes,0,1,1,1);
  g_signal_connect(b_yes,"pressed",G_CALLBACK(yes_cb),NULL);

  GtkWidget *b_halt=gtk_button_new_with_label("Halt System");
  //gtk_widget_override_font(b_halt, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(b_halt);
  gtk_grid_attach(GTK_GRID(grid),b_halt,2,1,1,1);
  g_signal_connect(b_halt,"pressed",G_CALLBACK(halt_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Cancel",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  gtk_dialog_run(GTK_DIALOG(dialog));

}

void lock_cb(GtkWidget *widget, gpointer data) {
  locked=locked==1?0:1;
  g_idle_add(ext_vfo_update,NULL);
}

void mox_cb(GtkWidget *widget, gpointer data) {

  if(getTune()==1) {
    setTune(0);
  }
  if(getMox()==1) {
    setMox(0);
  } else if(canTransmit() || tx_out_of_band) {
    setMox(1);
  } else {
    transmitter_set_out_of_band(transmitter);
  }
  g_idle_add(ext_vfo_update,NULL);
}

void mox_update(int state) {
//fprintf(stderr,"mox_update: state=%d\n",state);
  if(getTune()==1) {
    setTune(0);
  }
  if(state) {
    if(canTransmit() || tx_out_of_band) {
      setMox(state);
    } else {
      transmitter_set_out_of_band(transmitter);
    }
  } else {
    setMox(state);
  }
  g_idle_add(ext_vfo_update,NULL);
}

void tune_cb(GtkWidget *widget, gpointer data) {
  if(getMox()==1) {
    setMox(0);
  }
  if(getTune()==1) {
    setTune(0);
  } else if(canTransmit() || tx_out_of_band) {
    setTune(1);
  } else {
    transmitter_set_out_of_band(transmitter);
  }
  g_idle_add(ext_vfo_update,NULL);
}

void tune_update(int state) {
  if(getMox()==1) {
    setMox(0);
  }
  if(state) {
    setTune(0);
    if(canTransmit() || tx_out_of_band) {
      setTune(1);
    } else {
      transmitter_set_out_of_band(transmitter);
    }
  } else {
    setTune(state);
  }
  g_idle_add(ext_vfo_update,NULL);
}

void sim_s1_pressed_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      band_cb(widget,data);
      break;
    case 1:
      lock_cb(widget,data);
      break;
    case 2:
      freq_cb(widget,data);
      break;
    case 3:
      freq_cb(widget,data);
      break;
    case 4:
      freq_cb(widget,data);
      break;
    case 5:
      full_tune=full_tune==1?0:1;
      if(full_tune) {
        set_button_text_color(sim_s2,"black");
        set_button_text_color(sim_s1,"red");
      } else {
        set_button_text_color(sim_s1,"black");
      }
      break;
  }
}

void sim_s1_released_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;
    case 4:
      break;
    case 5:
      break;
  }
}

void sim_s2_pressed_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      bandstack_cb(widget,data);
      break;
    case 1:
      ctun_cb(widget,data);
      break;
    case 2:
      mem_cb(widget,data);
      break;
    case 3:
      mem_cb(widget,data);
      break;
    case 4:
      split_cb(widget,data);
      break;
    case 5:
      memory_tune=memory_tune==1?0:1;
      if(memory_tune) {
        set_button_text_color(sim_s1,"black");
        set_button_text_color(sim_s2,"red");
      } else {
        set_button_text_color(sim_s2,"black");
      }
      break;
  }
}

void sim_s2_released_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;
    case 4:
      break;
    case 5:
      break;
  }
}


void sim_s3_pressed_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      mode_cb(widget,data);
      break;
    case 1:
      // A>B
      atob_cb(widget,data);
      break;
    case 2:
      rit_enable_cb(widget,data);
      break;
    case 3:
      xit_enable_cb(widget,data);
      break;
    case 4:
      duplex_cb(widget,data);
      break;
    case 5:
      band_cb(widget,data);
      break;
  }
}

void sim_s3_released_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;
    case 4:
      break;
    case 5:
      break;
  }
}

void sim_s4_pressed_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      filter_cb(widget,data);
      break;
    case 1:
      // A<B
      btoa_cb(widget,data);
      break;
    case 2:
      if(rit_minus_timer==-1 && rit_plus_timer==-1) {
        rit_cb(widget,(void *)1);
      }
      break;
    case 3:
      if(xit_minus_timer==-1 && xit_plus_timer==-1) {
        xit_cb(widget,(void *)1);
      }
      break;
    case 4:
      sat_cb(widget,data);
      break;
    case 5:
      mode_cb(widget,data);
      break;
  }
}

void sim_s4_released_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      break;
    case 1:
      break;
    case 2:
      if(rit_plus_timer!=-1) {
        g_source_remove(rit_plus_timer);
        rit_plus_timer=-1;
      }
      break;
    case 3:
      if(xit_plus_timer!=-1) {
        g_source_remove(xit_plus_timer);
        xit_plus_timer=-1;
      }
      break;
    case 4:
      break;
    case 5:
      break;
  }
}

void sim_s5_pressed_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      noise_cb(widget,data);
      break;
    case 1:
      // A<>B
      aswapb_cb(widget,data);
      break;
    case 2:
      if(rit_minus_timer==-1 && rit_plus_timer==-1) {
        rit_cb(widget,(void *)-1);
      }
      break;
    case 3:
      if(xit_minus_timer==-1 && xit_plus_timer==-1) {
        xit_cb(widget,(void *)-1);
      }
      break;
    case 4:
      rsat_cb(widget,data);
      break;
    case 5:
      filter_cb(widget,data);
      break;
  }
}

void sim_s5_released_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      break;
    case 1:
      break;
    case 2:
      if(rit_minus_timer!=-1) {
        g_source_remove(rit_minus_timer);
        rit_minus_timer=-1;
      }
      break;
    case 3:
      if(xit_minus_timer!=-1) {
        g_source_remove(xit_minus_timer);
        xit_minus_timer=-1;
      }
      break;
    case 4:
      break;
    case 5:
      break;
  }
}

void sim_s6_pressed_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      agc_cb(widget,data);
      break;
    case 1:
      split_cb(widget,data);
      break;
    case 2:
      rit_clear_cb(widget,NULL);
      break;
    case 3:
      xit_clear_cb(widget,NULL);
      break;
    case 4:
      break;
    case 5:
      break;
  }
}

void sim_s6_released_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;
    case 4:
      break;
    case 5:
      break;
  }
}

void sim_mox_cb(GtkWidget *widget, gpointer data) {
  switch(function) {
    case 0:
    case 1:
    case 3:
    case 4:
      mox_cb((GtkWidget *)NULL, (gpointer)NULL);
      break;
    case 2:
    case 5:
      tune_cb((GtkWidget *)NULL, (gpointer)NULL);
      break;
  }
}

void sim_function_cb(GtkWidget *widget, gpointer data) {
  function++;
  if(function>MAX_FUNCTION) {
    function=0;
  }
  update_toolbar_labels();
  g_idle_add(ext_vfo_update,NULL);
}

GtkWidget *toolbar_init(int my_width, int my_height, GtkWidget* parent) {
    width=my_width;
    height=my_height;
    parent_window=parent;

    int button_width=width/8;

    fprintf(stderr,"toolbar_init: width=%d height=%d button_width=%d\n", width,height,button_width);

    white.red=1.0;
    white.green=1.0;
    white.blue=1.0;
    white.alpha=0.0;

    gray.red=0.25;
    gray.green=0.25;
    gray.blue=0.25;
    gray.alpha=0.0;

    toolbar=gtk_grid_new();
    gtk_widget_set_size_request (toolbar, width, height);
    gtk_grid_set_column_homogeneous(GTK_GRID(toolbar),TRUE);

    if(can_transmit) {
      sim_mox=gtk_button_new_with_label("Mox");
    } else {
      sim_mox=gtk_button_new_with_label("");
    }
    //gtk_widget_override_font(sim_mox, pango_font_description_from_string("Sans 11"));
    g_signal_connect(G_OBJECT(sim_mox),"clicked",G_CALLBACK(sim_mox_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_mox,0,0,4,1);

    sim_s1=gtk_button_new_with_label("Band");
    gtk_widget_set_size_request (sim_s1, button_width, 0);
    //gtk_widget_override_font(sim_s1, pango_font_description_from_string("Sans 11"));
    g_signal_connect(G_OBJECT(sim_s1),"pressed",G_CALLBACK(sim_s1_pressed_cb),NULL);
    g_signal_connect(G_OBJECT(sim_s1),"released",G_CALLBACK(sim_s1_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_s1,4,0,4,1);

    sim_s2=gtk_button_new_with_label("BStack");
    gtk_widget_set_size_request (sim_s2, button_width, 0);
    //gtk_widget_override_font(sim_s2, pango_font_description_from_string("Sans 11"));
    g_signal_connect(G_OBJECT(sim_s2),"pressed",G_CALLBACK(sim_s2_pressed_cb),NULL);
    g_signal_connect(G_OBJECT(sim_s2),"released",G_CALLBACK(sim_s2_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_s2,8,0,4,1);

    sim_s3=gtk_button_new_with_label("Mode");
    //gtk_widget_override_font(sim_s3, pango_font_description_from_string("Sans 11"));
    g_signal_connect(G_OBJECT(sim_s3),"pressed",G_CALLBACK(sim_s3_pressed_cb),NULL);
    g_signal_connect(G_OBJECT(sim_s3),"released",G_CALLBACK(sim_s3_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_s3,12,0,4,1);

    sim_s4=gtk_button_new_with_label("Filter");
    //gtk_widget_override_font(sim_s4, pango_font_description_from_string("Sans 11"));
    g_signal_connect(G_OBJECT(sim_s4),"pressed",G_CALLBACK(sim_s4_pressed_cb),NULL);
    g_signal_connect(G_OBJECT(sim_s4),"released",G_CALLBACK(sim_s4_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_s4,16,0,4,1);

    sim_s5=gtk_button_new_with_label("Noise");
    //gtk_widget_override_font(sim_s5, pango_font_description_from_string("Sans 11"));
    g_signal_connect(G_OBJECT(sim_s5),"pressed",G_CALLBACK(sim_s5_pressed_cb),NULL);
    g_signal_connect(G_OBJECT(sim_s5),"released",G_CALLBACK(sim_s5_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_s5,20,0,4,1);

    sim_s6=gtk_button_new_with_label("AGC");
    //gtk_widget_override_font(sim_s6, pango_font_description_from_string("Sans 11"));
    g_signal_connect(G_OBJECT(sim_s6),"pressed",G_CALLBACK(sim_s6_pressed_cb),NULL);
    g_signal_connect(G_OBJECT(sim_s6),"released",G_CALLBACK(sim_s6_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_s6,24,0,4,1);

    sim_function=gtk_button_new_with_label("Function");
    //gtk_widget_override_font(sim_function, pango_font_description_from_string("Sans 11"));
    g_signal_connect(G_OBJECT(sim_function),"clicked",G_CALLBACK(sim_function_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_function,28,0,4,1);

    //update_toolbar_labels();

    last_dialog=NULL;

    gtk_widget_show_all(toolbar);

  return toolbar;
}
