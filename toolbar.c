#include <gtk/gtk.h>
#include <semaphore.h>
#include <stdio.h>

#include "toolbar.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "discovered.h"
#include "new_protocol.h"
#include "rotary_encoder.h"
#include "vfo.h"
#include "alex.h"
#include "agc.h"
#include "channel.h"
#include "wdsp.h"
#include "radio.h"
#include "property.h"

static int width;
static int height;

static int column=0;

static GtkWidget *parent_window;
static GtkWidget *toolbar;
static GtkWidget *toolbar_bottom;
static GtkWidget *toolbar_top;

static GtkWidget *last_band;
static GtkWidget *last_mode;
static GtkWidget *last_filter;

static GtkWidget *af_gain_label;
static GtkWidget *audio_scale;
static GtkWidget *agc_gain_label;
static GtkWidget *agc_scale;
static GtkWidget *mic_gain_label;
static GtkWidget *mic_scale;
static GtkWidget *drive_scale;
static GtkWidget *tune_scale;

static GdkRGBA white;
static GdkRGBA gray;

static void band_select_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *label;
  int b=(int)data;
  BANDSTACK_ENTRY *entry;
  if(b==band_get_current()) {
    entry=bandstack_entry_next();
  } else {
    BAND* band=band_set_current(b);
    entry=bandstack_entry_get_current();
    gtk_widget_override_background_color(last_band, GTK_STATE_NORMAL, &white);
    last_band=widget;
    gtk_widget_override_background_color(last_band, GTK_STATE_NORMAL, &gray);
  }
  setFrequency(entry->frequencyA);
  setMode(entry->mode);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);

  BAND *band=band_get_current_band();
  set_alex_rx_antenna(band->alexRxAntenna);
  set_alex_tx_antenna(band->alexTxAntenna);
  set_alex_attenuation(band->alexAttenuation);
  vfo_update(NULL);
}

static void band_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Band",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  GtkWidget *b;
  int i;
  for(i=0;i<BANDS;i++) {
    BAND* band=band_get_band(i);
    GtkWidget *b=gtk_button_new_with_label(band->title);
    gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &white);
    gtk_widget_override_font(b, pango_font_description_from_string("Arial 20"));
    if(i==band_get_current()) {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &gray);
      last_band=b;
    }
    gtk_widget_show(b);
    gtk_grid_attach(GTK_GRID(grid),b,i%5,i/5,1,1);
    g_signal_connect(b,"clicked",G_CALLBACK(band_select_cb),(gpointer *)i);
  }
  
  gtk_container_add(GTK_CONTAINER(content),grid);

  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));
  
}

static void mode_select_cb(GtkWidget *widget, gpointer data) {
  int m=(int)data;
  BANDSTACK_ENTRY *entry;
  entry=bandstack_entry_get_current();
  entry->mode=m;
  setMode(entry->mode);
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);
  gtk_widget_override_background_color(last_mode, GTK_STATE_NORMAL, &white);
  last_mode=widget;
  gtk_widget_override_background_color(last_mode, GTK_STATE_NORMAL, &gray);
  vfo_update(NULL);
}

static void mode_cb(GtkWidget *widget, gpointer data) {
  BANDSTACK_ENTRY *entry=bandstack_entry_get_current();
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Mode",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *b;
  int i;
  for(i=0;i<MODES;i++) {
    GtkWidget *b=gtk_button_new_with_label(mode_string[i]);
    if(i==entry->mode) {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &gray);
      last_mode=b;
    } else {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &white);
    }
    gtk_widget_override_font(b, pango_font_description_from_string("Arial 20"));
    gtk_widget_show(b);
    gtk_grid_attach(GTK_GRID(grid),b,i%5,i/5,1,1);
    g_signal_connect(b,"pressed",G_CALLBACK(mode_select_cb),(gpointer *)i);
  }
  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}

static void filter_select_cb(GtkWidget *widget, gpointer data) {
  int f=(int)data;
  BANDSTACK_ENTRY *entry;
  entry=bandstack_entry_get_current();
  entry->filter=f;
  FILTER* band_filters=filters[entry->mode];
  FILTER* band_filter=&band_filters[entry->filter];
  setFilter(band_filter->low,band_filter->high);
  gtk_widget_override_background_color(last_filter, GTK_STATE_NORMAL, &white);
  last_filter=widget;
  gtk_widget_override_background_color(last_filter, GTK_STATE_NORMAL, &gray);
  vfo_update(NULL);
}

static void filter_cb(GtkWidget *widget, gpointer data) {
  BANDSTACK_ENTRY *entry=bandstack_entry_get_current();
  FILTER* band_filters=filters[entry->mode];
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Mode",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *b;
  int i;
  for(i=0;i<FILTERS;i++) {
    FILTER* band_filter=&band_filters[i];
    GtkWidget *b=gtk_button_new_with_label(band_filters[i].title);
    gtk_widget_override_font(b, pango_font_description_from_string("Arial 20"));
    if(i==entry->filter) {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &gray);
      last_filter=b;
    } else {
      gtk_widget_override_background_color(b, GTK_STATE_NORMAL, &white);
    }
    gtk_widget_show(b);
    gtk_grid_attach(GTK_GRID(grid),b,i%5,i/5,1,1);
    g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer *)i);
  }
  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}

static void agc_select_cb(GtkWidget *widget, gpointer data) {
  agc=(int)data;
  SetRXAAGCMode(CHANNEL_RX0, agc);
}

static void agcgain_value_changed_cb(GtkWidget *widget, gpointer data) {
  agc_gain=gtk_range_get_value(GTK_RANGE(widget));
  SetRXAAGCTop(CHANNEL_RX0, agc_gain);
}

void set_agc_gain(double value) {
    agc_gain=value;
    gtk_range_set_value (GTK_RANGE(agc_scale),agc_gain);
    SetRXAAGCTop(CHANNEL_RX0, agc_gain);
}

static void afgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    volume=gtk_range_get_value(GTK_RANGE(widget));
}

void set_af_gain(double value) {
    volume=value;
    gtk_range_set_value (GTK_RANGE(audio_scale),volume);
}

static void micgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    mic_gain=gtk_range_get_value(GTK_RANGE(widget));
}

static void nr_cb(GtkWidget *widget, gpointer data) {
  nr=nr==0?1:0;
  SetRXAANRRun(CHANNEL_RX0, nr);
}

static void nb_cb(GtkWidget *widget, gpointer data) {
  nb=nb==0?1:0;
  SetRXAEMNRRun(CHANNEL_RX0, nb);
}

static void anf_cb(GtkWidget *widget, gpointer data) {
  anf=anf==0?1:0;
  SetRXAANFRun(CHANNEL_RX0, anf);
}

static void snb_cb(GtkWidget *widget, gpointer data) {
  snb=snb==0?1:0;
  SetRXASNBARun(CHANNEL_RX0, snb);
}

static void linein_cb(GtkWidget *widget, gpointer data) {
  if((orion_mic&0x01)==LINE_IN) {
      orion_mic=orion_mic&0xFE;
  } else {
      orion_mic=orion_mic|LINE_IN;
  }
}

static void micboost_cb(GtkWidget *widget, gpointer data) {
  if((orion_mic&0x02)==MIC_BOOST) {
      orion_mic=orion_mic&0xFD;
  } else {
      orion_mic=orion_mic|MIC_BOOST;
  }
}

static void byteswap_cb(GtkWidget *widget, gpointer data) {
  byte_swap=byte_swap?0:1;
}

static void ptt_cb(GtkWidget *widget, gpointer data) {
  if((orion_mic&0x04)==ORION_MIC_PTT_ENABLED) {
      orion_mic=orion_mic|ORION_MIC_PTT_DISABLED;
  } else {
      orion_mic=orion_mic&0xFB;
  }
}

static void ptt_ring_cb(GtkWidget *widget, gpointer data) {
  if((orion_mic&0x08)==ORION_MIC_PTT_RING_BIAS_TIP) {
      orion_mic=orion_mic|ORION_MIC_PTT_TIP_BIAS_RING;
  } else {
      orion_mic=orion_mic&0xF7;
  }
}

static void bias_cb(GtkWidget *widget, gpointer data) {
  if((orion_mic&0x10)==ORION_MIC_BIAS_DISABLED) {
      orion_mic=orion_mic|ORION_MIC_BIAS_ENABLED;
  } else {
      orion_mic=orion_mic&0xEF;
  }
}

static void audio_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Audio",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *b_off=gtk_radio_button_new_with_label(NULL,"Off"); 
  gtk_widget_override_font(b_off, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_off), agc==AGC_OFF);
  gtk_widget_show(b_off);
  gtk_grid_attach(GTK_GRID(grid),b_off,0,0,2,1);
  g_signal_connect(b_off,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_OFF);

  GtkWidget *b_long=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_off),"Long"); 
  gtk_widget_override_font(b_long, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_long), agc==AGC_LONG);
  gtk_widget_show(b_long);
  gtk_grid_attach(GTK_GRID(grid),b_long,0,1,2,1);
  g_signal_connect(b_long,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_LONG);

  GtkWidget *b_slow=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_long),"Slow"); 
  gtk_widget_override_font(b_slow, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_slow), agc==AGC_SLOW);
  gtk_widget_show(b_slow);
  gtk_grid_attach(GTK_GRID(grid),b_slow,0,2,2,1);
  g_signal_connect(b_slow,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_SLOW);

  GtkWidget *b_medium=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_slow),"Medium"); 
  gtk_widget_override_font(b_medium, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_medium), agc==AGC_MEDIUM);
  gtk_widget_show(b_medium);
  gtk_grid_attach(GTK_GRID(grid),b_medium,0,3,2,1);
  g_signal_connect(b_medium,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_MEDIUM);

  GtkWidget *b_fast=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(b_medium),"Fast"); 
  gtk_widget_override_font(b_fast, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_fast), agc==AGC_FAST);
  gtk_widget_show(b_fast);
  gtk_grid_attach(GTK_GRID(grid),b_fast,0,4,2,1);
  g_signal_connect(b_fast,"pressed",G_CALLBACK(agc_select_cb),(gpointer *)AGC_FAST);


  GtkWidget *b_nr=gtk_check_button_new_with_label("NR");
  gtk_widget_override_font(b_nr, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nr), nr==1);
  gtk_widget_show(b_nr);
  gtk_grid_attach(GTK_GRID(grid),b_nr,2,0,2,1);
  g_signal_connect(b_nr,"toggled",G_CALLBACK(nr_cb),NULL);

  GtkWidget *b_nb=gtk_check_button_new_with_label("NB");
  gtk_widget_override_font(b_nb, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_nb), nb==1);
  gtk_widget_show(b_nb);
  gtk_grid_attach(GTK_GRID(grid),b_nb,2,1,2,1);
  g_signal_connect(b_nb,"toggled",G_CALLBACK(nb_cb),NULL);

  GtkWidget *b_anf=gtk_check_button_new_with_label("ANF");
  gtk_widget_override_font(b_anf, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_anf), anf==1);
  gtk_widget_show(b_anf);
  gtk_grid_attach(GTK_GRID(grid),b_anf,2,2,2,1);
  g_signal_connect(b_anf,"toggled",G_CALLBACK(anf_cb),NULL);

  GtkWidget *b_snb=gtk_check_button_new_with_label("SNB");
  gtk_widget_override_font(b_snb, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_snb), snb==1);
  gtk_widget_show(b_snb);
  gtk_grid_attach(GTK_GRID(grid),b_snb,2,3,2,1);
  g_signal_connect(b_snb,"toggled",G_CALLBACK(snb_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}

void set_drive(double value) {
  setDrive(value);
  gtk_range_set_value (GTK_RANGE(drive_scale),value);
}

static void drive_value_changed_cb(GtkWidget *widget, gpointer data) {
  setDrive(gtk_range_get_value(GTK_RANGE(widget)));
}

void set_tune(double value) {
  setTuneDrive(value);
  gtk_range_set_value (GTK_RANGE(tune_scale),value);
}

static void tune_value_changed_cb(GtkWidget *widget, gpointer data) {
  setTuneDrive(gtk_range_get_value(GTK_RANGE(widget)));
}

static void yes_cb(GtkWidget *widget, gpointer data) {
  encoder_close();
  _exit(0);
}

static void halt_cb(GtkWidget *widget, gpointer data) {
  encoder_close();
  system("shutdown -h -P now");
  _exit(0);
}

static void exit_cb(GtkWidget *widget, gpointer data) {

  radioSaveState();
  saveProperties(property_path);

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Audio",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *label=gtk_label_new("Exit?");
  gtk_widget_override_font(label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid),label,1,0,1,1);

  GtkWidget *b_yes=gtk_button_new_with_label("Yes (to CLI)");
  gtk_widget_override_font(b_yes, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(b_yes);
  gtk_grid_attach(GTK_GRID(grid),b_yes,0,1,1,1);
  g_signal_connect(b_yes,"pressed",G_CALLBACK(yes_cb),NULL);

  GtkWidget *b_halt=gtk_button_new_with_label("Halt System");
  gtk_widget_override_font(b_halt, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(b_halt);
  gtk_grid_attach(GTK_GRID(grid),b_halt,2,1,1,1);
  g_signal_connect(b_halt,"pressed",G_CALLBACK(halt_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Cancel",GTK_RESPONSE_OK);
  gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

}

static void apollo_cb(GtkWidget *widget, gpointer data);

static void alex_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"alex_cb\n");
  if(filter_board==ALEX) {
fprintf(stderr,"alex_cb: was ALEX setting NONE\n");
    filter_board=NONE;
  } else if(filter_board==NONE) {
fprintf(stderr,"alex_cb: was NONE setting ALEX\n");
    filter_board=ALEX;
  } else if(filter_board==APOLLO) {
fprintf(stderr,"alex_cb: was APOLLO setting ALEX\n");
    GtkWidget *w=(GtkWidget *)data;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    filter_board=ALEX;
  }
  filter_board_changed();
}

static void apollo_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"apollo_cb\n");
  if(filter_board==APOLLO) {
fprintf(stderr,"apollo_cb: was APOLLO setting NONE\n");
    filter_board=NONE;
  } else if(filter_board==NONE) {
fprintf(stderr,"apollo_cb: was NONE setting APOLLO\n");
    filter_board=APOLLO;
  } else if(filter_board==ALEX) {
fprintf(stderr,"apollo_cb: was ALEX setting APOLLO\n");
    GtkWidget *w=(GtkWidget *)data;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    filter_board=APOLLO;
  }
  filter_board_changed();
}

static void apollo_tuner_cb(GtkWidget *widget, gpointer data) {
  apollo_tuner=apollo_tuner==1?0:1;
  tuner_changed();
}

static void pa_cb(GtkWidget *widget, gpointer data) {
  pa=pa==1?0:1;
  pa_changed();
}

static void cw_keyer_internal_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_internal=cw_keyer_internal==1?0:1;
  cw_changed();
}

static void cw_keyer_speed_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_speed=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_breakin_cb(GtkWidget *widget, gpointer data) {
  cw_breakin=cw_breakin==1?0:1;
  cw_changed();
}

static void cw_keyer_hang_time_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_hang_time=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_keys_reversed_cb(GtkWidget *widget, gpointer data) {
  cw_keys_reversed=cw_keys_reversed==1?0:1;
  cw_changed();
}

static void cw_keyer_mode_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_mode=(int)data;
  cw_changed();
}

static void config_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog=gtk_dialog_new_with_buttons("Audio",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *linein_b=gtk_check_button_new_with_label("Line In");
  gtk_widget_override_font(linein_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (linein_b), (orion_mic&0x01)==LINE_IN);
  gtk_widget_show(linein_b);
  gtk_grid_attach(GTK_GRID(grid),linein_b,0,0,1,1);
  g_signal_connect(linein_b,"toggled",G_CALLBACK(linein_cb),NULL);

  GtkWidget *micboost_b=gtk_check_button_new_with_label("Boost");
  gtk_widget_override_font(micboost_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (micboost_b), (orion_mic&0x02)==MIC_BOOST);
  gtk_widget_show(micboost_b);
  gtk_grid_attach(GTK_GRID(grid),micboost_b,0,1,1,1);
  g_signal_connect(micboost_b,"toggled",G_CALLBACK(micboost_cb),NULL);

  GtkWidget *byteswap_b=gtk_check_button_new_with_label("Byte swap");
  gtk_widget_override_font(byteswap_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (byteswap_b), byte_swap);
  gtk_widget_show(byteswap_b);
  gtk_grid_attach(GTK_GRID(grid),byteswap_b,0,2,1,1);
  g_signal_connect(byteswap_b,"toggled",G_CALLBACK(byteswap_cb),NULL);

  DISCOVERED* d=&discovered[selected_device];
  if(d->device==NEW_DEVICE_ORION) {
    GtkWidget *ptt_b=gtk_check_button_new_with_label("PTT Enabled");
    gtk_widget_override_font(ptt_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_b), (orion_mic&0x04)==ORION_MIC_PTT_ENABLED);
    gtk_widget_show(ptt_b);
    gtk_grid_attach(GTK_GRID(grid),ptt_b,0,3,1,1);
    g_signal_connect(ptt_b,"toggled",G_CALLBACK(ptt_cb),NULL);

    GtkWidget *ptt_ring_b=gtk_check_button_new_with_label("PTT On Ring");
    gtk_widget_override_font(ptt_ring_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_ring_b), (orion_mic&0x08)==ORION_MIC_PTT_RING_BIAS_TIP);
    gtk_widget_show(ptt_ring_b);
    gtk_grid_attach(GTK_GRID(grid),ptt_ring_b,0,4,1,1);
    g_signal_connect(ptt_ring_b,"toggled",G_CALLBACK(ptt_ring_cb),NULL);

    GtkWidget *bias_b=gtk_check_button_new_with_label("BIAS Enabled");
    gtk_widget_override_font(bias_b, pango_font_description_from_string("Arial 18"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bias_b), (orion_mic&0x10)==ORION_MIC_BIAS_ENABLED);
    gtk_widget_show(bias_b);
    gtk_grid_attach(GTK_GRID(grid),bias_b,0,5,1,1);
    g_signal_connect(bias_b,"toggled",G_CALLBACK(bias_cb),NULL);
  }

  GtkWidget *alex_b=gtk_check_button_new_with_label("ALEX");
  gtk_widget_override_font(alex_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alex_b), filter_board==ALEX);
  gtk_widget_show(alex_b);
  gtk_grid_attach(GTK_GRID(grid),alex_b,1,0,1,1);

  GtkWidget *apollo_b=gtk_check_button_new_with_label("APOLLO");
  gtk_widget_override_font(apollo_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apollo_b), filter_board==APOLLO);
  gtk_widget_show(apollo_b);
  gtk_grid_attach(GTK_GRID(grid),apollo_b,1,1,1,1);

  g_signal_connect(alex_b,"toggled",G_CALLBACK(alex_cb),apollo_b);
  g_signal_connect(apollo_b,"toggled",G_CALLBACK(apollo_cb),alex_b);

  GtkWidget *apollo_tuner_b=gtk_check_button_new_with_label("Auto Tuner");
  gtk_widget_override_font(apollo_tuner_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apollo_tuner_b), apollo_tuner);
  gtk_widget_show(apollo_tuner_b);
  gtk_grid_attach(GTK_GRID(grid),apollo_tuner_b,1,2,1,1);
  g_signal_connect(apollo_tuner_b,"toggled",G_CALLBACK(apollo_tuner_cb),NULL);

  GtkWidget *pa_b=gtk_check_button_new_with_label("PA");
  gtk_widget_override_font(pa_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pa_b), pa);
  gtk_widget_show(pa_b);
  gtk_grid_attach(GTK_GRID(grid),pa_b,1,3,1,1);
  g_signal_connect(pa_b,"toggled",G_CALLBACK(pa_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));
}

static void cw_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog=gtk_dialog_new_with_buttons("CW",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);


  GtkWidget *cw_keyer_internal_b=gtk_check_button_new_with_label("CW Internal - Speed (WPM)");
  gtk_widget_override_font(cw_keyer_internal_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_internal_b), cw_keyer_internal);
  gtk_widget_show(cw_keyer_internal_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_internal_b,0,0,1,1);
  g_signal_connect(cw_keyer_internal_b,"toggled",G_CALLBACK(cw_keyer_internal_cb),NULL);

  GtkWidget *cw_keyer_speed_b=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  gtk_widget_override_font(cw_keyer_speed_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_speed_b),(double)cw_keyer_speed);
  gtk_widget_show(cw_keyer_speed_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_speed_b,1,0,1,1);
  g_signal_connect(cw_keyer_speed_b,"value_changed",G_CALLBACK(cw_keyer_speed_value_changed_cb),NULL);

  GtkWidget *cw_breakin_b=gtk_check_button_new_with_label("CW Break In - Delay (ms)");
  gtk_widget_override_font(cw_breakin_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_breakin_b), cw_breakin);
  gtk_widget_show(cw_breakin_b);
  gtk_grid_attach(GTK_GRID(grid),cw_breakin_b,0,1,1,1);
  g_signal_connect(cw_breakin_b,"toggled",G_CALLBACK(cw_breakin_cb),NULL);

  GtkWidget *cw_keyer_hang_time_b=gtk_spin_button_new_with_range(0.0,1000.0,1.0);
  gtk_widget_override_font(cw_keyer_hang_time_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_hang_time_b),(double)cw_keyer_hang_time);
  gtk_widget_show(cw_keyer_hang_time_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_hang_time_b,1,1,1,1);
  g_signal_connect(cw_keyer_hang_time_b,"value_changed",G_CALLBACK(cw_keyer_hang_time_value_changed_cb),NULL);
  
  GtkWidget *cw_keyer_straight=gtk_radio_button_new_with_label(NULL,"CW KEYER STRAIGHT");
  gtk_widget_override_font(cw_keyer_straight, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_straight), cw_keyer_mode==KEYER_STRAIGHT);
  gtk_widget_show(cw_keyer_straight);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_straight,0,2,1,1);
  g_signal_connect(cw_keyer_straight,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_STRAIGHT);

  GtkWidget *cw_keyer_mode_a=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(cw_keyer_straight),"CW KEYER MODE A");
  gtk_widget_override_font(cw_keyer_mode_a, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_mode_a), cw_keyer_mode==KEYER_MODE_A);
  gtk_widget_show(cw_keyer_mode_a);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_mode_a,0,3,1,1);
  g_signal_connect(cw_keyer_mode_a,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_MODE_A);

  GtkWidget *cw_keyer_mode_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(cw_keyer_mode_a),"CW KEYER MODE B");
  gtk_widget_override_font(cw_keyer_mode_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_mode_b), cw_keyer_mode==KEYER_MODE_B);
  gtk_widget_show(cw_keyer_mode_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_mode_b,0,4,1,1);
  g_signal_connect(cw_keyer_mode_b,"pressed",G_CALLBACK(cw_keyer_mode_cb),(gpointer *)KEYER_MODE_B);

/*
int cw_keyer_speed=12; // 1-60 WPM
int cw_keyer_weight=30; // 0-100
int cw_keyer_spacing=0; // 0=on 1=off
int cw_keyer_sidetone_volume=127; // 0-127
int cw_keyer_ptt_delay=20; // 0-255ms
int cw_keyer_hang_time=10; // ms
int cw_keyer_sidetone_frequency=400; // Hz
*/

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));
}

static void adc_cb(GtkWidget *widget, gpointer data) {
  int adc0=adc[0];
  adc[0]=adc[1];
  adc[1]=adc0;

  char label[16];
  gtk_grid_remove_row(GTK_GRID(toolbar_top),0);
  
  sprintf(label,"RX0=%d",adc[0]);
  GtkWidget *rx0=gtk_label_new(label);
  gtk_widget_override_font(rx0, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(rx0);
  gtk_grid_attach(GTK_GRID(toolbar_top),rx0,0,0,1,1);
  
  sprintf(label,"RX1=%d",adc[1]);
  GtkWidget *rx1=gtk_label_new(label);
  gtk_widget_override_font(rx1, pango_font_description_from_string("Arial 16"));
  gtk_widget_show(rx1);
  gtk_grid_attach(GTK_GRID(toolbar_top),rx1,1,0,1,1);
}

static void lock_cb(GtkWidget *widget, gpointer data) {
  locked=locked==1?0:1;
  vfo_update(NULL);
}

static void mox_cb(GtkWidget *widget, gpointer data) {
  gtk_grid_remove_row (GTK_GRID(toolbar_top),0);
  if(getTune()==1) {
    setTune(0);
  }
  setMox(getMox()==0?1:0);
  vfo_update(NULL);
  if(getMox()) {
    mic_gain_label=gtk_label_new("Mic Gain:");
    gtk_widget_override_font(mic_gain_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(mic_gain_label);
    gtk_grid_attach(GTK_GRID(toolbar_top),mic_gain_label,0,0,1,1);

    mic_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 1.0, 0.01);
    gtk_range_set_value (GTK_RANGE(mic_scale),mic_gain);
    gtk_widget_show(mic_scale);
    gtk_grid_attach(GTK_GRID(toolbar_top),mic_scale,1,0,2,1);
    g_signal_connect(G_OBJECT(mic_scale),"value_changed",G_CALLBACK(micgain_value_changed_cb),NULL);

    GtkWidget *drive_label=gtk_label_new("Drive:");
    gtk_widget_override_font(drive_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(drive_label);
    gtk_grid_attach(GTK_GRID(toolbar_top),drive_label,3,0,1,1);

    drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 1.0, 0.01);
    gtk_range_set_value (GTK_RANGE(drive_scale),getDrive());
    gtk_widget_show(drive_scale);
    gtk_grid_attach(GTK_GRID(toolbar_top),drive_scale,4,0,2,1);
    g_signal_connect(G_OBJECT(drive_scale),"value_changed",G_CALLBACK(drive_value_changed_cb),NULL);
  } else {
    af_gain_label=gtk_label_new("AF:");
    gtk_widget_override_font(af_gain_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(af_gain_label);
    gtk_grid_attach(GTK_GRID(toolbar_top),af_gain_label,0,0,1,1);
  
    audio_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 1.0, 0.01);
    gtk_range_set_value (GTK_RANGE(audio_scale),volume);
    gtk_widget_show(audio_scale);
    gtk_grid_attach(GTK_GRID(toolbar_top),audio_scale,1,0,2,1);
    g_signal_connect(G_OBJECT(audio_scale),"value_changed",G_CALLBACK(afgain_value_changed_cb),NULL);

    agc_gain_label=gtk_label_new("AGC:");
    gtk_widget_override_font(agc_gain_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(agc_gain_label);
    gtk_grid_attach(GTK_GRID(toolbar_top),agc_gain_label,3,0,1,1);

    agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 120.0, 1.0);
    gtk_range_set_value (GTK_RANGE(agc_scale),agc_gain);
    gtk_widget_show(agc_scale);
    gtk_grid_attach(GTK_GRID(toolbar_top),agc_scale,4,0,2,1);
    g_signal_connect(G_OBJECT(agc_scale),"value_changed",G_CALLBACK(agcgain_value_changed_cb),NULL);

  }
  //gtk_widget_queue_draw(toolbar_top);
}

int ptt_update(void *data) {
  BANDSTACK_ENTRY *entry;
  entry=bandstack_entry_get_current();
  int ptt=(int)data;
  if((entry->mode==modeCWL || entry->mode==modeCWU) && cw_keyer_internal==1) {
    if(ptt!= getMox()) {
      mox_cb(NULL,NULL);
    }
  } else {
    mox_cb(NULL,NULL);
  }
  return 0;
}

static void tune_cb(GtkWidget *widget, gpointer data) {
  gtk_grid_remove_row (GTK_GRID(toolbar_top),0);
  if(getMox()==1) {
    setMox(0);
  }
  setTune(getTune()==0?1:0);
  vfo_update(NULL);
  if(getTune()) {
    GtkWidget *tune_label=gtk_label_new("Tune Drive:");
    gtk_widget_override_font(tune_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(tune_label);
    gtk_grid_attach(GTK_GRID(toolbar_top),tune_label,0,0,1,1);

    tune_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 1.0, 0.01);
    gtk_range_set_value (GTK_RANGE(tune_scale),getTuneDrive());
    gtk_widget_show(tune_scale);
    gtk_grid_attach(GTK_GRID(toolbar_top),tune_scale,1,0,2,1);
    g_signal_connect(G_OBJECT(tune_scale),"value_changed",G_CALLBACK(tune_value_changed_cb),NULL);
  } else {
    af_gain_label=gtk_label_new("AF:");
    gtk_widget_override_font(af_gain_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(af_gain_label);
    gtk_grid_attach(GTK_GRID(toolbar_top),af_gain_label,0,0,1,1);

    audio_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 1.0, 0.01);
    gtk_range_set_value (GTK_RANGE(audio_scale),volume);
    gtk_widget_show(audio_scale);
    gtk_grid_attach(GTK_GRID(toolbar_top),audio_scale,1,0,2,1);
    g_signal_connect(G_OBJECT(audio_scale),"value_changed",G_CALLBACK(afgain_value_changed_cb),NULL);

    agc_gain_label=gtk_label_new("AGC:");
    gtk_widget_override_font(agc_gain_label, pango_font_description_from_string("Arial 18"));
    gtk_widget_show(agc_gain_label);
    gtk_grid_attach(GTK_GRID(toolbar_top),agc_gain_label,3,0,1,1);

    agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 120.0, 1.0);
    gtk_range_set_value (GTK_RANGE(agc_scale),agc_gain);
    gtk_widget_show(agc_scale);
    gtk_grid_attach(GTK_GRID(toolbar_top),agc_scale,4,0,2,1);
    g_signal_connect(G_OBJECT(agc_scale),"value_changed",G_CALLBACK(agcgain_value_changed_cb),NULL);
  }
}

GtkWidget *toolbar_init(int my_width, int my_height, GtkWidget* parent) {
    width=my_width;
    height=my_height;
    parent_window=parent;

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
    gtk_grid_set_row_homogeneous(GTK_GRID(toolbar), TRUE);

    toolbar_top=gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(toolbar_top),TRUE);

    toolbar_bottom=gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(toolbar_bottom),FALSE);
    
    GtkWidget *band=gtk_button_new_with_label("Band");
    gtk_widget_override_font(band, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(band),"clicked",G_CALLBACK(band_cb),NULL);
    gtk_widget_show(band);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),band,column,0,1,1);
    column++;

    GtkWidget *mode=gtk_button_new_with_label("Mode");
    gtk_widget_override_font(mode, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(mode),"clicked",G_CALLBACK(mode_cb),NULL);
    gtk_widget_show(mode);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),mode,column,0,1,1);
    column++;

    GtkWidget *filter=gtk_button_new_with_label("Filter");
    gtk_widget_override_font(filter, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(filter),"clicked",G_CALLBACK(filter_cb),NULL);
    gtk_widget_show(filter);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),filter,column,0,1,1);
    column++;

    GtkWidget *audio=gtk_button_new_with_label("Audio");
    gtk_widget_override_font(audio, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(audio),"clicked",G_CALLBACK(audio_cb),NULL);
    gtk_widget_show(audio);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),audio,column,0,1,1);
    column++;

    GtkWidget *config=gtk_button_new_with_label("Config");
    gtk_widget_override_font(config, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(config),"clicked",G_CALLBACK(config_cb),NULL);
    gtk_widget_show(config);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),config,column,0,1,1);
    column++;

    GtkWidget *cw=gtk_button_new_with_label("CW");
    gtk_widget_override_font(cw, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(cw),"clicked",G_CALLBACK(cw_cb),NULL);
    gtk_widget_show(cw);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),cw,column,0,1,1);
    column++;

    GtkWidget *exit=gtk_button_new_with_label("Exit");
    gtk_widget_override_font(exit, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(exit),"clicked",G_CALLBACK(exit_cb),NULL);
    gtk_widget_show(exit);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),exit,column,0,1,1);
    column++;

    GtkWidget *lock=gtk_button_new_with_label("Lock");
    gtk_widget_override_font(lock, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(lock),"clicked",G_CALLBACK(lock_cb),NULL);
    gtk_widget_show(lock);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),lock,column,0,1,1);
    column++;

    GtkWidget *tune=gtk_button_new_with_label("Tune");
    gtk_widget_override_font(tune, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(tune),"clicked",G_CALLBACK(tune_cb),NULL);
    gtk_widget_show(tune);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),tune,column,0,1,1);
    column++;

    GtkWidget *tx=gtk_button_new_with_label("Mox");
    gtk_widget_override_font(tx, pango_font_description_from_string("Arial 20"));
    g_signal_connect(G_OBJECT(tx),"clicked",G_CALLBACK(mox_cb),NULL);
    gtk_widget_show(tx);
    gtk_grid_attach(GTK_GRID(toolbar_bottom),tx,column,0,1,1);
    column++;


  // default to receive controls on top bar
  af_gain_label=gtk_label_new("AF:");
  gtk_widget_override_font(af_gain_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(af_gain_label);
  gtk_grid_attach(GTK_GRID(toolbar_top),af_gain_label,0,0,1,1);

  audio_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 1.0, 0.01);
  gtk_range_set_value (GTK_RANGE(audio_scale),volume);
  gtk_widget_show(audio_scale);
  gtk_grid_attach(GTK_GRID(toolbar_top),audio_scale,1,0,2,1);
  g_signal_connect(G_OBJECT(audio_scale),"value_changed",G_CALLBACK(afgain_value_changed_cb),NULL);

  agc_gain_label=gtk_label_new("AGC:");
  gtk_widget_override_font(agc_gain_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(agc_gain_label);
  gtk_grid_attach(GTK_GRID(toolbar_top),agc_gain_label,3,0,1,1);

  agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 120.0, 1.0);
  gtk_range_set_value (GTK_RANGE(agc_scale),agc_gain);
  gtk_widget_show(agc_scale);
  gtk_grid_attach(GTK_GRID(toolbar_top),agc_scale,4,0,2,1);
  g_signal_connect(G_OBJECT(agc_scale),"value_changed",G_CALLBACK(agcgain_value_changed_cb),NULL);

/*

  GtkWidget *drive_label=gtk_label_new("Drive:");
  gtk_widget_override_font(drive_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(drive_label);
  gtk_grid_attach(toolbar_top,drive_label,6,0,1,1);

  drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 1.0, 0.01);
  gtk_range_set_value (drive_scale,getTuneDrive());
  gtk_widget_show(drive_scale);
  gtk_grid_attach(toolbar_top,drive_scale,7,0,2,1);
  g_signal_connect(G_OBJECT(drive_scale),"value_changed",G_CALLBACK(drive_value_changed_cb),NULL);

  GtkWidget *tune_label=gtk_label_new("Tune:");
  gtk_widget_override_font(tune_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(tune_label);
  gtk_grid_attach(toolbar_top,tune_label,9,0,1,1);

  tune_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 1.0, 0.01);
  gtk_range_set_value (tune_scale,getTuneDrive());
  gtk_widget_show(tune_scale);
  gtk_grid_attach(toolbar_top,tune_scale,10,0,2,1);
  g_signal_connect(G_OBJECT(tune_scale),"value_changed",G_CALLBACK(tune_value_changed_cb),NULL);
*/

  gtk_grid_attach(GTK_GRID(toolbar),toolbar_top,0,0,1,1);
  gtk_widget_show(toolbar_top);
  gtk_grid_attach(GTK_GRID(toolbar),toolbar_bottom,0,1,1,1);
  gtk_widget_show(toolbar_bottom);

  return toolbar;
}
