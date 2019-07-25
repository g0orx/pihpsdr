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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <math.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <wdsp.h>

#include "audio.h"
#include "discovered.h"
//#include "discovery.h"
#include "filter.h"
#include "main.h"
#include "mode.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "channel.h"
#include "agc.h"
#include "band.h"
#include "property.h"
#include "new_menu.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "store.h"
#ifdef LIMESDR
#include "lime_protocol.h"
#endif
#ifdef FREEDV
#include "freedv.h"
#endif
#include "audio_waterfall.h"
#ifdef GPIO
#include "gpio.h"
#endif
#include "vfo.h"
#include "vox.h"
#include "meter.h"
#include "rx_panadapter.h"
#include "tx_panadapter.h"
#include "waterfall.h"
#include "sliders.h"
#include "toolbar.h"
#include "rigctl.h"
#include "ext.h"
#ifdef LOCALCW
#include "iambic.h"
#endif
#ifdef MIDI
#include "midi.h"
#endif

#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

#define MENU_HEIGHT (30)
#define MENU_WIDTH (64)
#define VFO_HEIGHT (60)
#define VFO_WIDTH (display_width-METER_WIDTH-MENU_WIDTH)
#define METER_HEIGHT (60)
#define METER_WIDTH (200)
#define PANADAPTER_HEIGHT (105)
#define SLIDERS_HEIGHT (90)
#define TOOLBAR_HEIGHT (30)
#define WATERFALL_HEIGHT (105)
#ifdef PSK
#define PSK_WATERFALL_HEIGHT (90)
#define PSK_HEIGHT (display_height-(VFO_HEIGHT+PSK_WATERFALL_HEIGHT+SLIDERS_HEIGHT+TOOLBAR_HEIGHT))
#endif

#ifdef FREEDV
#define FREEDV_WATERFALL_HEIGHT (105)
#endif

static GtkWidget *fixed;
static GtkWidget *vfo_panel;
static GtkWidget *meter;
static GtkWidget *menu;
static GtkWidget *sliders;
static GtkWidget *toolbar;
static GtkWidget *panadapter;
static GtkWidget *waterfall;
#ifdef PSK
static GtkWidget *psk;
static GtkWidget *psk_waterfall;
#endif
static GtkWidget *audio_waterfall;

#ifdef GPIO
static GtkWidget *encoders;
static cairo_surface_t *encoders_surface = NULL;
#endif
#ifdef WIRIINGPI
static GtkWidget *encoders;
static cairo_surface_t *encoders_surface = NULL;
#endif

int region=REGION_OTHER;

int echo=0;

static gint save_timer_id;

DISCOVERED *radio=NULL;

char property_path[128];
#ifdef __APPLE__
sem_t *property_sem;
#else
sem_t property_sem;
#endif

RECEIVER *receiver[MAX_RECEIVERS];
RECEIVER *active_receiver;
TRANSMITTER *transmitter;

int buffer_size=1024; // 64, 128, 256, 512, 1024
int fft_size=2048; // 1024, 2048, 4096, 8192, 16384

int atlas_penelope=0;
int atlas_clock_source_10mhz=0;
int atlas_clock_source_128mhz=0;
int atlas_config=0;
int atlas_mic_source=0;

int classE=0;

int tx_out_of_band=0;

int tx_cfir=0;
int tx_leveler=0;
int alc=TXA_ALC_AV;

double tone_level=0.2;

int filter_board=ALEX;
//int pa=PA_ENABLED;
//int apollo_tuner=0;

int updates_per_second=10;

int panadapter_high=-40;
int panadapter_low=-140;

int display_filled=1;
int display_detector_mode=DETECTOR_MODE_AVERAGE;
int display_average_mode=AVERAGE_MODE_LOG_RECURSIVE;
double display_average_time=120.0;


int waterfall_high=-100;
int waterfall_low=-150;

int display_sliders=1;

//double volume=0.2;
double mic_gain=0.0;
int binaural=0;

int mic_linein=0;
int linein_gain=16; // 0..31
int mic_boost=0;
int mic_bias_enabled=0;
int mic_ptt_enabled=0;
int mic_ptt_tip_bias_ring=0;

//double tune_drive=10;
//double drive=50;

//int drive_level=0;
//int tune_drive_level=0;

int receivers=RECEIVERS;

int locked=0;

long long step=100;

//int rit=0;
int rit_increment=10;

int lt2208Dither = 0;
int lt2208Random = 0;
int attenuation = 0; // 0dB
//unsigned long alex_rx_antenna=0;
//unsigned long alex_tx_antenna=0;
//unsigned long alex_attenuation=0;

int cw_keys_reversed=0; // 0=disabled 1=enabled
int cw_keyer_speed=12; // 1-60 WPM
int cw_keyer_mode=KEYER_STRAIGHT;
int cw_keyer_weight=30; // 0-100
int cw_keyer_spacing=0; // 0=on 1=off
int cw_keyer_internal=1; // 0=external 1=internal
int cw_keyer_sidetone_volume=127; // 0-127
int cw_keyer_ptt_delay=20; // 0-255ms
int cw_keyer_hang_time=300; // ms
int cw_keyer_sidetone_frequency=400; // Hz
int cw_breakin=1; // 0=disabled 1=enabled

int vfo_encoder_divisor=15;

int protocol;
int device;
int ozy_software_version;
int mercury_software_version;
int penelope_software_version;
int dot;
int dash;
int adc_overload;
int pll_locked;
unsigned int exciter_power;
unsigned int alex_forward_power;
unsigned int alex_reverse_power;
unsigned int AIN3;
unsigned int AIN4;
unsigned int AIN6;
unsigned int IO1;
unsigned int IO2;
unsigned int IO3;
int supply_volts;
int mox;
int tune;
int memory_tune=0;
int full_tune=0;

//long long displayFrequency=14250000;
//long long ddsFrequency=14250000;
//long long ddsOffset=0;

long long frequencyB=14250000;
int modeB=modeUSB;
int filterB=5;

int split=0;

unsigned char OCtune=0;
int OCfull_tune_time=2800; // ms
int OCmemory_tune_time=550; // ms
long long tune_timeout;

int analog_meter=0;
int smeter=RXA_S_AV;

int local_audio=0;
int local_microphone=0;

int eer_pwm_min=100;
int eer_pwm_max=800;

int tx_filter_low=150;
int tx_filter_high=2850;

static int pre_tune_mode;
static int pre_tune_filter_low;
static int pre_tune_filter_high;

int enable_tx_equalizer=0;
int tx_equalizer[4]={0,0,0,0};

int enable_rx_equalizer=0;
int rx_equalizer[4]={0,0,0,0};

int pre_emphasize=0;

int vox_setting=0;
int vox_enabled=0;
double vox_threshold=0.001;
double vox_gain=10.0;
double vox_hang=250.0;
int vox=0;
int CAT_cw_is_active=0;
int cw_key_hit=0;
int cw_key_state=0;
int n_adc=1;

int diversity_enabled=0;
double div_cos=1.0;        // I factor for diversity
double div_sin=1.0;	   // Q factor for diversity
double div_gain=0.0;	   // gain for diversity (in dB)
double div_phase=0.0;	   // phase for diversity (in degrees, 0 ... 360)

double meter_calibration=0.0;
double display_calibration=0.0;

void reconfigure_radio() {
  int i;
  int y;
//fprintf(stderr,"reconfigure_radio: receivers=%d\n",receivers);
  int rx_height=display_height-VFO_HEIGHT-TOOLBAR_HEIGHT;
  if(display_sliders) {
    rx_height-=SLIDERS_HEIGHT;
  }
 
  y=VFO_HEIGHT;
  for(i=0;i<receivers;i++) {
    reconfigure_receiver(receiver[i],rx_height/receivers);
    gtk_fixed_move(GTK_FIXED(fixed),receiver[i]->panel,0,y);
    receiver[i]->x=0;
    receiver[i]->y=y;
    y+=rx_height/receivers;
  }

  if(display_sliders) {
    if(sliders==NULL) {
      sliders = sliders_init(display_width,SLIDERS_HEIGHT);
      gtk_fixed_put(GTK_FIXED(fixed),sliders,0,y);
    } else {
      gtk_fixed_move(GTK_FIXED(fixed),sliders,0,y);
    }
    gtk_widget_show_all(sliders);  // ... this shows both C25 and Alex ATT/Preamp sliders
    att_type_changed();            // ... and this hides the „wrong“ ones.
  } else {
    if(sliders!=NULL) {
      gtk_container_remove(GTK_CONTAINER(fixed),sliders); 
      sliders=NULL;
    }
  }

  reconfigure_transmitter(transmitter,rx_height);

}

static gboolean save_cb(gpointer data) {
    radioSaveState();
    return TRUE;
}

static gboolean minimize_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_window_iconify(GTK_WINDOW(top_window));
  return TRUE;
}

static gboolean menu_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  new_menu();
  return TRUE;
}

void start_radio() {
  int i;
  int x;
  int y;
//fprintf(stderr,"start_radio: selected radio=%p device=%d\n",radio,radio->device);
  gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_WATCH));

  int rc;

#ifdef MIDI
  MIDIstartup();
#endif

#ifdef __APPLE__
  sem_unlink("PROPERTY");
  property_sem=sem_open("PROPERTY", O_CREAT | O_EXCL, 0700, 0);
  rc=(property_sem == SEM_FAILED);
#else
  rc=sem_init(&property_sem, 0, 0);
#endif
  if(rc!=0) {
    fprintf(stderr,"start_radio: sem_init failed for property_sem: %d\n", rc);
    exit(-1);
  }
#ifdef __APPLE__
  sem_post(property_sem);
#else
  sem_post(&property_sem);
#endif

  char text[256];
  //for(i=0;i<devices;i++) {
    switch(radio->protocol) {
      case ORIGINAL_PROTOCOL:
      case NEW_PROTOCOL:
#ifdef USBOZY
        if(radio->device==DEVICE_OZY) {
          sprintf(text,"%s (%s) on USB /dev/ozy\n", radio->name, radio->protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2");
        } else {
#endif
          sprintf(text,"Starting %s (%s v%d.%d)",
                        radio->name,
                        radio->protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2",
                        radio->software_version/10,
                        radio->software_version%10);
        break;
    }
  //}



  status_text(text);

  sprintf(text,"piHPSDR: %s (%s v%d.%d) %s (%02X:%02X:%02X:%02X:%02X:%02X) on %s",
                          radio->name,
                          radio->protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2",
                          radio->software_version/10,
                          radio->software_version%10,
                          inet_ntoa(radio->info.network.address.sin_addr),
                          radio->info.network.mac_address[0],
                          radio->info.network.mac_address[1],
                          radio->info.network.mac_address[2],
                          radio->info.network.mac_address[3],
                          radio->info.network.mac_address[4],
                          radio->info.network.mac_address[5],
                          radio->info.network.interface_name);

//fprintf(stderr,"title: length=%d\n", (int)strlen(text));

  gtk_window_set_title (GTK_WINDOW (top_window), text);

  protocol=radio->protocol;
  device=radio->device;

  switch(radio->protocol) {
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL:
      switch(radio->device) {
#ifdef USBOZY
        case DEVICE_OZY:
          sprintf(property_path,"ozy.props");
          break;
#endif
        default:
          sprintf(property_path,"%02X-%02X-%02X-%02X-%02X-%02X.props",
                        radio->info.network.mac_address[0],
                        radio->info.network.mac_address[1],
                        radio->info.network.mac_address[2],
                        radio->info.network.mac_address[3],
                        radio->info.network.mac_address[4],
                        radio->info.network.mac_address[5]);
          break;
      }
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      sprintf(property_path,"limesdr.props");
      break;
#endif
  }

  switch(radio->protocol) {
    case ORIGINAL_PROTOCOL:
      switch(radio->device) {
        case DEVICE_ORION2:
          //meter_calibration=3.0;
          //display_calibration=3.36;
          break;
        default:
          //meter_calibration=-2.44;
          //display_calibration=-2.1;
          break;
      }
      break;
    case NEW_PROTOCOL:
      switch(radio->device) {
        case NEW_DEVICE_ORION2:
          //meter_calibration=3.0;
          //display_calibration=3.36;
          break;
        default:
          //meter_calibration=-2.44;
          //display_calibration=-2.1;
          break;
      }
      break;
  }
 
  // Code moved here from rx_menu because n_adc is of general interest:
  // Determine Number of ADCs.
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      switch(device) {
        case DEVICE_METIS:
          n_adc=1;  // No support for multiple MERCURY cards on a single ATLAS bus.
          break;
        case DEVICE_HERMES:
        case DEVICE_HERMES_LITE:
          n_adc=1;
          break;
        default:
          n_adc=2;
          break;
      }
      break;
    case NEW_PROTOCOL:
      switch(device) {
        case NEW_DEVICE_ATLAS:
          n_adc=1; // No support for multiple MERCURY cards on a single ATLAS bus.
          break;
        case NEW_DEVICE_HERMES:
        case NEW_DEVICE_HERMES2:
        case NEW_DEVICE_HERMES_LITE:
          n_adc=1;
          break;
        default:
          n_adc=2;
          break;
      }
      break;
    default:
      break;
  }

  adc_attenuation[0]=0;
  adc_attenuation[1]=0;
  rx_gain_slider[0] = 0;
  rx_gain_slider[1] = 0;

//fprintf(stderr,"meter_calibration=%f display_calibration=%f\n", meter_calibration, display_calibration);
  radioRestoreState();

//
//  DL1YCF: we send one buffer of TX samples in one shot. For the old
//          protocol, their number is buffer_size, but for the new
//          protocol, the number is 4*buffer_size.
//          Since current hardware has a FIFO of 4096 IQ sample pairs,
//          buffer_size should be limited to 2048 for the old protocol and
//          to 512 for the new protocol.
//
  switch (protocol) {
    case ORIGINAL_PROTOCOL:
      if (buffer_size > 2048) buffer_size=2048;
      break;
    case NEW_PROTOCOL:
      if (buffer_size > 512) buffer_size=512;
      break;
  }

  radio_change_region(region);

  y=0;

  fixed=gtk_fixed_new();
  gtk_container_remove(GTK_CONTAINER(top_window),grid);
  gtk_container_add(GTK_CONTAINER(top_window), fixed);

//fprintf(stderr,"radio: vfo_init\n");
  vfo_panel = vfo_init(VFO_WIDTH,VFO_HEIGHT,top_window);
  gtk_fixed_put(GTK_FIXED(fixed),vfo_panel,0,y);

//fprintf(stderr,"radio: meter_init\n");
  meter = meter_init(METER_WIDTH,METER_HEIGHT,top_window);
  gtk_fixed_put(GTK_FIXED(fixed),meter,VFO_WIDTH,y);


  GtkWidget *minimize_b=gtk_button_new_with_label("Hide");
  gtk_widget_override_font(minimize_b, pango_font_description_from_string("FreeMono Bold 10"));
  gtk_widget_set_size_request (minimize_b, MENU_WIDTH, MENU_HEIGHT);
  g_signal_connect (minimize_b, "button-press-event", G_CALLBACK(minimize_cb), NULL) ;
  gtk_fixed_put(GTK_FIXED(fixed),minimize_b,VFO_WIDTH+METER_WIDTH,y);
  y+=MENU_HEIGHT;

  GtkWidget *menu_b=gtk_button_new_with_label("Menu");
  gtk_widget_override_font(menu_b, pango_font_description_from_string("FreeMono Bold 10"));
  gtk_widget_set_size_request (menu_b, MENU_WIDTH, MENU_HEIGHT);
  g_signal_connect (menu_b, "button-press-event", G_CALLBACK(menu_cb), NULL) ;
  gtk_fixed_put(GTK_FIXED(fixed),menu_b,VFO_WIDTH+METER_WIDTH,y);
  y+=MENU_HEIGHT;


  int rx_height=display_height-VFO_HEIGHT-TOOLBAR_HEIGHT;
  if(display_sliders) {
    rx_height-=SLIDERS_HEIGHT;
  }
  int tx_height=rx_height;
  rx_height=rx_height/RECEIVERS;


  //
  // To be on the safe side, we create ALL receiver panels here
  // If upon startup, we only should display one panel, we do the switch below
  //
  for(i=0;i<RECEIVERS;i++) {
    receiver[i]=create_receiver(i, buffer_size, fft_size, display_width, updates_per_second, display_width, rx_height);
    setSquelch(receiver[i]);
    receiver[i]->x=0;
    receiver[i]->y=y;
    gtk_fixed_put(GTK_FIXED(fixed),receiver[i]->panel,0,y);
    g_object_ref((gpointer)receiver[i]->panel);
    set_displaying(receiver[i],1);
    y+=rx_height;
  }

  if((protocol==ORIGINAL_PROTOCOL) && (RECEIVERS==2) && (receiver[0]->sample_rate!=receiver[1]->sample_rate)) {
    receiver[1]->sample_rate=receiver[0]->sample_rate;
  }

  active_receiver=receiver[0];

  //fprintf(stderr,"Create transmitter\n");
  transmitter=create_transmitter(CHANNEL_TX, buffer_size, fft_size, updates_per_second, display_width, tx_height);
  transmitter->x=0;
  transmitter->y=VFO_HEIGHT;
  //gtk_fixed_put(GTK_FIXED(fixed),transmitter->panel,0,VFO_HEIGHT);

#ifdef PURESIGNAL
  tx_set_ps_sample_rate(transmitter,protocol==NEW_PROTOCOL?192000:active_receiver->sample_rate);
  receiver[PS_TX_FEEDBACK]=create_pure_signal_receiver(PS_TX_FEEDBACK, buffer_size,protocol==ORIGINAL_PROTOCOL?active_receiver->sample_rate:192000,display_width);
  receiver[PS_RX_FEEDBACK]=create_pure_signal_receiver(PS_RX_FEEDBACK, buffer_size,protocol==ORIGINAL_PROTOCOL?active_receiver->sample_rate:192000,display_width);
  SetPSHWPeak(transmitter->id, protocol==ORIGINAL_PROTOCOL? 0.4067 : 0.2899);
#endif

#ifdef AUDIO_WATERFALL
  audio_waterfall=audio_waterfall_init(200,100);
  gtk_fixed_put(GTK_FIXED(fixed),audio_waterfall,0,VFO_HEIGHT+20);
#endif
  
#ifdef GPIO
  if(gpio_init()<0) {
    fprintf(stderr,"GPIO failed to initialize\n");
  }
#endif
#ifdef LOCALCW
  // init local keyer if enabled
  if (cw_keyer_internal == 0) {
	fprintf(stderr,"Initialize keyer.....\n");
    keyer_update();
  }
#endif
  
  switch(radio->protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_init(0,display_width,receiver[0]->sample_rate);
      break;
    case NEW_PROTOCOL:
      new_protocol_init(display_width);
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_init(0,display_width,receiver[0]->sample_rate);
      break;
#endif
  }



//#ifdef I2C
//  i2c_init();
//#endif

  if(display_sliders) {
//fprintf(stderr,"create sliders\n");
    sliders = sliders_init(display_width,SLIDERS_HEIGHT);
    gtk_fixed_put(GTK_FIXED(fixed),sliders,0,y);
    y+=SLIDERS_HEIGHT;
  }


  toolbar = toolbar_init(display_width,TOOLBAR_HEIGHT,top_window);
  gtk_fixed_put(GTK_FIXED(fixed),toolbar,0,y);
  y+=TOOLBAR_HEIGHT;

//
// Now, if there should only one receiver be displayed
// at startup, do the change. We must momentarily fake
// the number of receivers otherwise radio_change_receivers
// will do nothing.
//
  if (receivers != RECEIVERS) {
    i=receivers,
    receivers=RECEIVERS;
    radio_change_receivers(i);
  }

  gtk_widget_show_all (fixed);
//#ifdef FREEDV
//  if(!active_receiver->freedv) {
//    gtk_widget_hide(audio_waterfall);
//  }
//#endif

  

  // save every 30 seconds
  save_timer_id=gdk_threads_add_timeout(30000, save_cb, NULL);

#ifdef PSK
  if(vfo[active_receiver->id].mode==modePSK) {
    show_psk();
  } else {
    show_waterfall();
  }
#endif

  if(rigctl_enable) {
    launch_rigctl();
  }

  calcDriveLevel();

  if(transmitter->puresignal) {
    tx_set_ps(transmitter,transmitter->puresignal);
  }

  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }

  g_idle_add(ext_vfo_update,(gpointer)NULL);

  gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));

}

void disable_rigctl() {
   fprintf(stderr,"RIGCTL: disable_rigctl()\n");
   close_rigctl_ports();
}
 

void radio_change_receivers(int r) {
  // The button in the radio menu will call this function even if the
  // number of receivers has not changed.
  if (receivers == r) return;
  fprintf(stderr,"radio_change_receivers: from %d to %d\n",receivers,r);
  switch(r) {
    case 1:
	set_displaying(receiver[1],0);
	gtk_container_remove(GTK_CONTAINER(fixed),receiver[1]->panel);
	receivers=1;
	break;
    case 2:
	gtk_fixed_put(GTK_FIXED(fixed),receiver[1]->panel,0,0);
	set_displaying(receiver[1],1);
	receivers=2;
	break;
  }
  reconfigure_radio();
  active_receiver=receiver[0];
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

void radio_change_sample_rate(int rate) {
  int i;
  
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      // The radio menu calls this function even if the sample rate
      // has not changed. Do nothing in this case.
      if (receiver[0]->sample_rate != rate) {
        old_protocol_stop();
        for(i=0;i<receivers;i++) {
          receiver_change_sample_rate(receiver[i],rate);
        }
#ifdef PURESIGNAL
        receiver_change_sample_rate(receiver[PS_RX_FEEDBACK],rate);
#endif
        old_protocol_set_mic_sample_rate(rate);
        old_protocol_run();
        tx_set_ps_sample_rate(transmitter,rate);
      }
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_change_sample_rate(rate);
      break;
#endif
  }
}

static void rxtx(int state) {
  int i;

  if(state) {
    // switch to tx
#ifdef FREEDV
    if(active_receiver->freedv) {
      freedv_reset_tx_text_index();
    }
#endif
#ifdef PURESIGNAL
    RECEIVER *rx_feedback=receiver[PS_RX_FEEDBACK];
    RECEIVER *tx_feedback=receiver[PS_TX_FEEDBACK];

    rx_feedback->samples=0;
    tx_feedback->samples=0;
#endif

    for(i=0;i<receivers;i++) {
      // Delivery of RX samples
      // to WDSP via fexchange0() may come to an abrupt stop
      // (especially with PURESIGNAL or DIVERSITY).
      // Therefore, wait for *all* receivers to complete
      // their slew-down before going TX.
      SetChannelState(receiver[i]->id,0,1);
      set_displaying(receiver[i],0);
      g_object_ref((gpointer)receiver[i]->panel);
      g_object_ref((gpointer)receiver[i]->panadapter);
      if(receiver[i]->waterfall!=NULL) {
        g_object_ref((gpointer)receiver[i]->waterfall);
      }
      gtk_container_remove(GTK_CONTAINER(fixed),receiver[i]->panel);
    }
//#ifdef FREEDV
//    if(active_receiver->freedv) {
//      gtk_widget_show(audio_waterfall);
//    }
//#endif
    gtk_fixed_put(GTK_FIXED(fixed),transmitter->panel,transmitter->x,transmitter->y);
    SetChannelState(transmitter->id,1,0);
    tx_set_displaying(transmitter,1);
  } else {
    // switch to rx
    SetChannelState(transmitter->id,0,1);
    tx_set_displaying(transmitter,0);
    g_object_ref((gpointer)transmitter->panel);
    g_object_ref((gpointer)transmitter->panadapter);
    gtk_container_remove(GTK_CONTAINER(fixed),transmitter->panel);
//#ifdef FREEDV
//    if(active_receiver->freedv) {
//      gtk_widget_hide(audio_waterfall);
//    }
//#endif
    for(i=0;i<receivers;i++) {
      gtk_fixed_put(GTK_FIXED(fixed),receiver[i]->panel,receiver[i]->x,receiver[i]->y);
      SetChannelState(receiver[i]->id,1,0);
      set_displaying(receiver[i],1);
    }
//#ifdef FREEDV
//    if(active_receiver->freedv) {
//      gtk_widget_show(audio_waterfall);
//    }
//#endif
  }

#ifdef PURESIGNAL
  if(transmitter->puresignal) {
    SetPSMox(transmitter->id,state);
  }
#endif
}

void setMox(int state) {
  vox_cancel();  // remove time-out
  if(mox!=state) {
    if (state && vox) {
      // Suppress RX-TX transition if VOX was already active
    } else {
      rxtx(state);
    }
    mox=state;
  }
  vox=0;
  if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
      schedule_receive_specific();
  }
}

int getMox() {
    return mox;
}

void vox_changed(int state) {
  if(vox!=state && !tune && !mox) {
    rxtx(state);
  }
  vox=state;
  if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
      schedule_receive_specific();
  }
}


void setTune(int state) {
  int i;

  // if state==tune, this function is a no-op

  if(tune!=state) {
    vox_cancel();
    if (vox || mox) {
      rxtx(0);
      vox=0;
      mox=0;
    }
    if(state) {
      if(full_tune) {
        if(OCfull_tune_time!=0) {
          struct timeval te;
          gettimeofday(&te,NULL);
          tune_timeout=(te.tv_sec*1000LL+te.tv_usec/1000)+(long long)OCfull_tune_time;
        }
      }
      if(memory_tune) {
        if(OCmemory_tune_time!=0) {
          struct timeval te;
          gettimeofday(&te,NULL);
          tune_timeout=(te.tv_sec*1000LL+te.tv_usec/1000)+(long long)OCmemory_tune_time;
        }
      }
    }
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
      //schedule_general();
    }
    if(state) {
      for(i=0;i<receivers;i++) {
        // Delivery of RX samples
        // to WDSP via fexchange0() may come to an abrupt stop
        // (especially with PURESIGNAL or DIVERSITY)
        // Therefore, wait for *all* receivers to complete
        // their slew-down before going TX.
        SetChannelState(receiver[i]->id,0,1);
        set_displaying(receiver[i],0);
        if(protocol==NEW_PROTOCOL) {
          schedule_high_priority();
        }
      }

      int mode=vfo[VFO_A].mode;
      if(split) {
        mode=vfo[VFO_B].mode;
      }
      pre_tune_mode=mode;

      //
      // in USB/DIGU/DSB, tune 1000 Hz above carrier
      // in LSB/DIGL,     tune 1000 Hz below carrier
      // all other (CW, AM, FM): tune on carrier freq.
      //
      switch(mode) {
        case modeLSB:
        case modeDIGL:
          SetTXAPostGenToneFreq(transmitter->id,-(double)1000.0);
          break;
        case modeUSB:
        case modeDSB:
        case modeDIGU:
          SetTXAPostGenToneFreq(transmitter->id,(double)1000.0);
          break;
        default:
          SetTXAPostGenToneFreq(transmitter->id,(double)0.0);
          break;
      }

      SetTXAPostGenToneMag(transmitter->id,0.99999);
      SetTXAPostGenMode(transmitter->id,0);
      SetTXAPostGenRun(transmitter->id,1);

      switch(mode) {
        case modeCWL:
          cw_keyer_internal=0;
          tx_set_mode(transmitter,modeLSB);
          break;
        case modeCWU:
          cw_keyer_internal=0;
          tx_set_mode(transmitter,modeUSB);
          break;
      }
      rxtx(state);
    } else {
      rxtx(state);
      SetTXAPostGenRun(transmitter->id,0);
      switch(pre_tune_mode) {
        case modeCWL:
        case modeCWU:
          tx_set_mode(transmitter,pre_tune_mode);
          cw_keyer_internal=1;
          break;
      }
    }
    tune=state;
  }
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
    schedule_receive_specific();
  }
}

int getTune() {
  return tune;
}

int isTransmitting() {
  return mox | vox | tune;
}

void setFrequency(long long f) {
  BAND *band=band_get_current_band();
  BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
  int v=active_receiver->id;

  switch(protocol) {
    case NEW_PROTOCOL:
    case ORIGINAL_PROTOCOL:
      if(vfo[v].ctun) {
        long long minf=vfo[v].frequency-(long long)(active_receiver->sample_rate/2);
        long long maxf=vfo[v].frequency+(long long)(active_receiver->sample_rate/2);
        if(f<minf) f=minf;
        if(f>maxf) f=maxf;
        vfo[v].offset=f-vfo[v].frequency;
        set_offset(active_receiver,vfo[v].offset);
        return;
      } else {
        //entry->frequency=f;
        vfo[v].frequency=f;
      }
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      {
fprintf(stderr,"setFrequency: %lld\n",f);
      long long minf=vfo[v].frequency-(long long)(active_receiver->sample_rate/2);
      long long maxf=vfo[v].frequency+(long long)(active_receiver->sample_rate/2);
      if(f<minf) f=minf;
      if(f>maxf) f=maxf;
      vfo[v].offset=f-vfo[v].frequency;
      set_offset(active_receiver,vfo[v].offset);
      return;
      }
      break;
#endif
  }

  switch(protocol) {
    case NEW_PROTOCOL:
      schedule_high_priority();
      break;
    case ORIGINAL_PROTOCOL:
      break;
#ifdef LIMESDR
    case LIMESDR_PROTOCOL:
      lime_protocol_set_frequency(f);
      vfo[v].offset=0;
      set_offset(active_receiver,vfo[v].offset);
      break;
#endif
  }
}

long long getFrequency() {
    return vfo[active_receiver->id].frequency;
}

double getDrive() {
    return transmitter->drive;
}

static int calcLevel(double d) {
  int level=0;
  int v=VFO_A;
  if(split) v=VFO_B;

  BAND *band=band_get_band(vfo[v].band);
  double target_dbm = 10.0 * log10(d * 1000.0);
  double gbb=band->pa_calibration;
  target_dbm-=gbb;
  double target_volts = sqrt(pow(10, target_dbm * 0.1) * 0.05);
  double volts=min((target_volts / 0.8), 1.0);
  double actual_volts=volts*(1.0/0.98);

  if(actual_volts<0.0) {
    actual_volts=0.0;
  } else if(actual_volts>1.0) {
    actual_volts=1.0;
  }

  level=(int)(actual_volts*255.0);

//fprintf(stderr,"calcLevel: %f calib=%f level=%d\n",d, gbb, level);
  return level;
}

void calcDriveLevel() {
    transmitter->drive_level=calcLevel(transmitter->drive);
    if(isTransmitting()  && protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
//fprintf(stderr,"calcDriveLevel: drive=%d drive_level=%d\n",transmitter->drive,transmitter->drive_level);
}

void setDrive(double value) {
    transmitter->drive=value;
    calcDriveLevel();
}

double getTuneDrive() {
    return transmitter->tune_percent;
}

void setSquelch(RECEIVER *rx) {
  double am_sq=((rx->squelch/100.0)*160.0)-160.0;
  SetRXAAMSQThreshold(rx->id, am_sq);
  SetRXAAMSQRun(rx->id, rx->squelch_enable);

  double fm_sq=pow(10.0, -2.0*rx->squelch/100.0);
  SetRXAFMSQThreshold(rx->id, fm_sq);
  SetRXAFMSQRun(rx->id, rx->squelch_enable);
}

void set_attenuation(int value) {
    switch(protocol) {
      case NEW_PROTOCOL:
        schedule_high_priority();
        break;
#ifdef LIMESDR
      case LIMESDR_PROTOCOL:
        lime_protocol_set_attenuation(value);
        break;
#endif
    }
}

void set_alex_rx_antenna(int v) {
    if(active_receiver->id==0) {
      active_receiver->alex_antenna=v;
      if(protocol==NEW_PROTOCOL) {
          schedule_high_priority();
      }
    }
#ifdef LIMESDR
    if(protocol==LIMESDR_PROTOCOL) {
        lime_protocol_set_antenna(v);;
    }
#endif
}

void set_alex_tx_antenna(int v) {
    transmitter->alex_antenna=v;
    if(protocol==NEW_PROTOCOL) {
        schedule_high_priority();
    }
}

void set_alex_attenuation(int v) {
    if(active_receiver->id==0) {
      active_receiver->alex_attenuation=v;
      if(protocol==NEW_PROTOCOL) {
          schedule_high_priority();
      }
    }
}

void radioRestoreState() {
    char *value;

fprintf(stderr,"radioRestoreState: %s\n",property_path);
#ifdef __APPLE__
    sem_wait(property_sem);
#else
    sem_wait(&property_sem);
#endif
    loadProperties(property_path);

    value=getProperty("region");
    if(value) region=atoi(value);
    value=getProperty("buffer_size");
    if(value) buffer_size=atoi(value);
    value=getProperty("fft_size");
    if(value) fft_size=atoi(value);
    value=getProperty("atlas_penelope");
    if(value) atlas_penelope=atoi(value);
    value=getProperty("tx_out_of_band");
    if(value) tx_out_of_band=atoi(value);
    value=getProperty("filter_board");
    if(value) filter_board=atoi(value);
/*
    value=getProperty("apollo_tuner");
    if(value) apollo_tuner=atoi(value);
    value=getProperty("pa");
    if(value) pa=atoi(value);
*/
    value=getProperty("updates_per_second");
    if(value) updates_per_second=atoi(value);
    value=getProperty("display_filled");
    if(value) display_filled=atoi(value);
    value=getProperty("display_detector_mode");
    if(value) display_detector_mode=atoi(value);
    value=getProperty("display_average_mode");
    if(value) display_average_mode=atoi(value);
    value=getProperty("display_average_time");
    if(value) display_average_time=atof(value);
    value=getProperty("panadapter_high");
    if(value) panadapter_high=atoi(value);
    value=getProperty("panadapter_low");
    if(value) panadapter_low=atoi(value);
    value=getProperty("display_sliders");
    if(value) display_sliders=atoi(value);
/*
    value=getProperty("display_toolbar");
    if(value) display_toolbar=atoi(value);
*/
    value=getProperty("waterfall_high");
    if(value) waterfall_high=atoi(value);
    value=getProperty("waterfall_low");
    if(value) waterfall_low=atoi(value);
    value=getProperty("mic_gain");
    if(value) mic_gain=atof(value);
    value=getProperty("mic_boost");
    if(value) mic_boost=atof(value);
    value=getProperty("mic_linein");
    if(value) mic_linein=atoi(value);
    value=getProperty("linein_gain");
    if(value) linein_gain=atoi(value);
    value=getProperty("mic_ptt_enabled");
    if(value) mic_ptt_enabled=atof(value);
    value=getProperty("mic_bias_enabled");
    if(value) mic_bias_enabled=atof(value);
    value=getProperty("mic_ptt_tip_bias_ring");
    if(value) mic_ptt_tip_bias_ring=atof(value);

    value=getProperty("tx_filter_low");
    if(value) tx_filter_low=atoi(value);
    value=getProperty("tx_filter_high");
    if(value) tx_filter_high=atoi(value);

    value=getProperty("step");
    if(value) step=atoll(value);
    value=getProperty("cw_keys_reversed");
    if(value) cw_keys_reversed=atoi(value);
    value=getProperty("cw_keyer_speed");
    if(value) cw_keyer_speed=atoi(value);
    value=getProperty("cw_keyer_mode");
    if(value) cw_keyer_mode=atoi(value);
    value=getProperty("cw_keyer_weight");
    if(value) cw_keyer_weight=atoi(value);
    value=getProperty("cw_keyer_spacing");
    if(value) cw_keyer_spacing=atoi(value);
#ifdef LOCALCW
    value=getProperty("cw_keyer_internal");
    if(value) cw_keyer_internal=atoi(value);
#endif
    value=getProperty("cw_keyer_sidetone_volume");
    if(value) cw_keyer_sidetone_volume=atoi(value);
    value=getProperty("cw_keyer_ptt_delay");
    if(value) cw_keyer_ptt_delay=atoi(value);
    value=getProperty("cw_keyer_hang_time");
    if(value) cw_keyer_hang_time=atoi(value);
    value=getProperty("cw_keyer_sidetone_frequency");
    if(value) cw_keyer_sidetone_frequency=atoi(value);
    value=getProperty("cw_breakin");
    if(value) cw_breakin=atoi(value);
    value=getProperty("vfo_encoder_divisor");
    if(value) vfo_encoder_divisor=atoi(value);
    value=getProperty("OCtune");
    if(value) OCtune=atoi(value);
    value=getProperty("OCfull_tune_time");
    if(value) OCfull_tune_time=atoi(value);
    value=getProperty("OCmemory_tune_time");
    if(value) OCmemory_tune_time=atoi(value);
    value=getProperty("analog_meter");
    if(value) analog_meter=atoi(value);
    value=getProperty("smeter");
    if(value) smeter=atoi(value);
    value=getProperty("alc");
    if(value) alc=atoi(value);
#ifdef OLD_AUDIO
    value=getProperty("local_audio");
    if(value) local_audio=atoi(value);
    value=getProperty("n_selected_output_device");
    if(value) n_selected_output_device=atoi(value);
#endif
    value=getProperty("local_microphone");
    if(value) local_microphone=atoi(value);
//    value=getProperty("n_selected_input_device");
//    if(value) n_selected_input_device=atoi(value);
    value=getProperty("enable_tx_equalizer");
    if(value) enable_tx_equalizer=atoi(value);
    value=getProperty("tx_equalizer.0");
    if(value) tx_equalizer[0]=atoi(value);
    value=getProperty("tx_equalizer.1");
    if(value) tx_equalizer[1]=atoi(value);
    value=getProperty("tx_equalizer.2");
    if(value) tx_equalizer[2]=atoi(value);
    value=getProperty("tx_equalizer.3");
    if(value) tx_equalizer[3]=atoi(value);
    value=getProperty("enable_rx_equalizer");
    if(value) enable_rx_equalizer=atoi(value);
    value=getProperty("rx_equalizer.0");
    if(value) rx_equalizer[0]=atoi(value);
    value=getProperty("rx_equalizer.1");
    if(value) rx_equalizer[1]=atoi(value);
    value=getProperty("rx_equalizer.2");
    if(value) rx_equalizer[2]=atoi(value);
    value=getProperty("rx_equalizer.3");
    if(value) rx_equalizer[3]=atoi(value);
    value=getProperty("rit_increment");
    if(value) rit_increment=atoi(value);
    value=getProperty("pre_emphasize");
    if(value) pre_emphasize=atoi(value);

    value=getProperty("vox_enabled");
    if(value) vox_enabled=atoi(value);
    value=getProperty("vox_threshold");
    if(value) vox_threshold=atof(value);
/*
    value=getProperty("vox_gain");
    if(value) vox_gain=atof(value);
*/
    value=getProperty("vox_hang");
    if(value) vox_hang=atof(value);

    value=getProperty("binaural");
    if(value) binaural=atoi(value);

    value=getProperty("frequencyB");
    if(value) frequencyB=atol(value);

    value=getProperty("modeB");
    if(value) modeB=atoi(value);

    value=getProperty("filterB");
    if(value) filterB=atoi(value);

    value=getProperty("tone_level");
    if(value) tone_level=atof(value);

#ifdef GPIO
    value=getProperty("e1_encoder_action");
    if(value) e1_encoder_action=atoi(value);
    value=getProperty("e2_encoder_action");
    if(value) e2_encoder_action=atoi(value);
    value=getProperty("e3_encoder_action");
    if(value) e3_encoder_action=atoi(value);
#endif

    value=getProperty("receivers");
    if(value) receivers=atoi(value);

    filterRestoreState();
    bandRestoreState();
    memRestoreState();
    vfo_restore_state();
    modesettings_restore_state();
#ifdef FREEDV
    freedv_restore_state();
#endif
    value=getProperty("rigctl_enable");
    if(value) rigctl_enable=atoi(value);
    value=getProperty("rigctl_port_base");
    if(value) rigctl_port_base=atoi(value);

    value=getProperty("adc_0_attenuation");
    if(value) adc_attenuation[0]=atoi(value);
    value=getProperty("adc_1_attenuation");
    if(value) adc_attenuation[1]=atoi(value);
	
    value=getProperty("rx1_gain_slider");
    if(value) rx_gain_slider[0]=atoi(value);
    value=getProperty("rx2_gain_slider");
	if(value) rx_gain_slider[1]=atoi(value);
	
#ifdef __APPLE__
    sem_post(property_sem);
#else
    sem_post(&property_sem);
#endif
}

void radioSaveState() {
    int i;
    char value[80];

#ifdef __APPLE__
    sem_wait(property_sem);
#else
    sem_wait(&property_sem);
#endif
    sprintf(value,"%d",region);
    setProperty("region",value);
    sprintf(value,"%d",buffer_size);
    setProperty("buffer_size",value);
    sprintf(value,"%d",fft_size);
    setProperty("fft_size",value);
    sprintf(value,"%d",atlas_penelope);
    setProperty("atlas_penelope",value);
    sprintf(value,"%d",filter_board);
    setProperty("filter_board",value);
    sprintf(value,"%d",tx_out_of_band);
    setProperty("tx_out_of_band",value);
    sprintf(value,"%d",updates_per_second);
    setProperty("updates_per_second",value);
    sprintf(value,"%d",display_filled);
    setProperty("display_filled",value);
    sprintf(value,"%d",display_detector_mode);
    setProperty("display_detector_mode",value);
    sprintf(value,"%d",display_average_mode);
    setProperty("display_average_mode",value);
    sprintf(value,"%f",display_average_time);
    setProperty("display_average_time",value);
    sprintf(value,"%d",panadapter_high);
    setProperty("panadapter_high",value);
    sprintf(value,"%d",panadapter_low);
    setProperty("panadapter_low",value);
    sprintf(value,"%d",display_sliders);
    setProperty("display_sliders",value);
    sprintf(value,"%d",waterfall_high);
    setProperty("waterfall_high",value);
    sprintf(value,"%d",waterfall_low);
    setProperty("waterfall_low",value);
    sprintf(value,"%f",mic_gain);
    setProperty("mic_gain",value);
    sprintf(value,"%d",mic_boost);
    setProperty("mic_boost",value);
    sprintf(value,"%d",mic_linein);
    setProperty("mic_linein",value);
    sprintf(value,"%d",linein_gain);
    setProperty("linein_gain",value);
    sprintf(value,"%d",mic_ptt_enabled);
    setProperty("mic_ptt_enabled",value);
    sprintf(value,"%d",mic_bias_enabled);
    setProperty("mic_bias_enabled",value);
    sprintf(value,"%d",mic_ptt_tip_bias_ring);
    setProperty("mic_ptt_tip_bias_ring",value);
    sprintf(value,"%d",tx_filter_low);
    setProperty("tx_filter_low",value);
    sprintf(value,"%d",tx_filter_high);
    setProperty("tx_filter_high",value);

    sprintf(value,"%lld",step);
    setProperty("step",value);
    sprintf(value,"%d",cw_keys_reversed);
    setProperty("cw_keys_reversed",value);
    sprintf(value,"%d",cw_keyer_speed);
    setProperty("cw_keyer_speed",value);
    sprintf(value,"%d",cw_keyer_mode);
    setProperty("cw_keyer_mode",value);
    sprintf(value,"%d",cw_keyer_weight);
    setProperty("cw_keyer_weight",value);
    sprintf(value,"%d",cw_keyer_spacing);
    setProperty("cw_keyer_spacing",value);
    sprintf(value,"%d",cw_keyer_internal);
    setProperty("cw_keyer_internal",value);
    sprintf(value,"%d",cw_keyer_sidetone_volume);
    setProperty("cw_keyer_sidetone_volume",value);
    sprintf(value,"%d",cw_keyer_ptt_delay);
    setProperty("cw_keyer_ptt_delay",value);
    sprintf(value,"%d",cw_keyer_hang_time);
    setProperty("cw_keyer_hang_time",value);
    sprintf(value,"%d",cw_keyer_sidetone_frequency);
    setProperty("cw_keyer_sidetone_frequency",value);
    sprintf(value,"%d",cw_breakin);
    setProperty("cw_breakin",value);
    sprintf(value,"%d",vfo_encoder_divisor);
    setProperty("vfo_encoder_divisor",value);
    sprintf(value,"%d",OCtune);
    setProperty("OCtune",value);
    sprintf(value,"%d",OCfull_tune_time);
    setProperty("OCfull_tune_time",value);
    sprintf(value,"%d",OCmemory_tune_time);
    setProperty("OCmemory_tune_time",value);
    sprintf(value,"%d",analog_meter);
    setProperty("analog_meter",value);
    sprintf(value,"%d",smeter);
    setProperty("smeter",value);
    sprintf(value,"%d",alc);
    setProperty("alc",value);
#ifdef OLD_AUDIO
    sprintf(value,"%d",local_audio);
    setProperty("local_audio",value);
    sprintf(value,"%d",n_selected_output_device);
    setProperty("n_selected_output_device",value);
#endif
    sprintf(value,"%d",local_microphone);
    setProperty("local_microphone",value);

    sprintf(value,"%d",enable_tx_equalizer);
    setProperty("enable_tx_equalizer",value);
    sprintf(value,"%d",tx_equalizer[0]);
    setProperty("tx_equalizer.0",value);
    sprintf(value,"%d",tx_equalizer[1]);
    setProperty("tx_equalizer.1",value);
    sprintf(value,"%d",tx_equalizer[2]);
    setProperty("tx_equalizer.2",value);
    sprintf(value,"%d",tx_equalizer[3]);
    setProperty("tx_equalizer.3",value);
    sprintf(value,"%d",enable_rx_equalizer);
    setProperty("enable_rx_equalizer",value);
    sprintf(value,"%d",rx_equalizer[0]);
    setProperty("rx_equalizer.0",value);
    sprintf(value,"%d",rx_equalizer[1]);
    setProperty("rx_equalizer.1",value);
    sprintf(value,"%d",rx_equalizer[2]);
    setProperty("rx_equalizer.2",value);
    sprintf(value,"%d",rx_equalizer[3]);
    setProperty("rx_equalizer.3",value);
    sprintf(value,"%d",rit_increment);
    setProperty("rit_increment",value);
    sprintf(value,"%d",pre_emphasize);
    setProperty("pre_emphasize",value);

    sprintf(value,"%d",vox_enabled);
    setProperty("vox_enabled",value);
    sprintf(value,"%f",vox_threshold);
    setProperty("vox_threshold",value);
    sprintf(value,"%f",vox_hang);
    setProperty("vox_hang",value);

    sprintf(value,"%d",binaural);
    setProperty("binaural",value);

    sprintf(value,"%lld",frequencyB);
    setProperty("frequencyB",value);
    sprintf(value,"%d",modeB);
    setProperty("modeB",value);
    sprintf(value,"%d",filterB);
    setProperty("filterB",value);

    sprintf(value,"%f",tone_level);
    setProperty("tone_level",value);

#ifdef GPIO
    sprintf(value,"%d",e1_encoder_action);
    setProperty("e1_encoder_action",value);
    sprintf(value,"%d",e2_encoder_action);
    setProperty("e2_encoder_action",value);
    sprintf(value,"%d",e3_encoder_action);
    setProperty("e3_encoder_action",value);
#endif

    sprintf(value,"%d",adc_attenuation[0]);
    setProperty("adc_0_attenuation",value);
    sprintf(value,"%d",adc_attenuation[1]);
    setProperty("adc_1_attenuation",value);
	
	sprintf(value,"%d",rx_gain_slider[0]);
    setProperty("rx1_gain_slider",value);
    sprintf(value,"%d",rx_gain_slider[1]);
    setProperty("rx2_gain_slider",value);
	
    vfo_save_state();
    modesettings_save_state();
    sprintf(value,"%d",receivers);
    setProperty("receivers",value);
    for(i=0;i<receivers;i++) {
      receiver_save_state(receiver[i]);
    }
#ifdef PURESIGNAL
    // The only variables of interest in this receiver are
    // the alex_antenna an the adc
    receiver_save_state(receiver[PS_RX_FEEDBACK]);
#endif
    transmitter_save_state(transmitter);
#ifdef FREEDV
    freedv_save_state();
#endif

    filterSaveState();
    bandSaveState();
    memSaveState();

    sprintf(value,"%d",rigctl_enable);
    setProperty("rigctl_enable",value);
    sprintf(value,"%d",rigctl_port_base);
    setProperty("rigctl_port_base",value);

    saveProperties(property_path);
#ifdef __APPLE__
    sem_post(property_sem);
#else
    sem_post(&property_sem);
#endif
}

void calculate_display_average(RECEIVER *rx) {
  double display_avb;
  int display_average;

  double t=0.001*display_average_time;
  display_avb = exp(-1.0 / ((double)rx->fps * t));
  display_average = max(2, (int)min(60, (double)rx->fps * t));
  SetDisplayAvBackmult(rx->id, 0, display_avb);
  SetDisplayNumAverage(rx->id, 0, display_average);
}

void set_filter_type(int filter_type) {
  int i;

  //fprintf(stderr,"set_filter_type: %d\n",filter_type);
  for(i=0;i<RECEIVERS;i++) {
    receiver[i]->low_latency=filter_type;
    RXASetMP(receiver[i]->id, filter_type);
  }
  transmitter->low_latency=filter_type;
  TXASetMP(transmitter->id, filter_type);
}

void set_filter_size(int filter_size) {
  int i;

  //fprintf(stderr,"set_filter_size: %d\n",filter_size);
  for(i=0;i<RECEIVERS;i++) {
    receiver[i]->fft_size=filter_size;
    RXASetNC(receiver[i]->id, filter_size);
  }
  transmitter->fft_size=filter_size;
  TXASetNC(transmitter->id, filter_size);
}

#ifdef FREEDV
void set_freedv(int state) {
fprintf(stderr,"set_freedv: rx=%p state=%d\n",active_receiver,state);
  g_mutex_lock(&active_receiver->freedv_mutex);
  active_receiver->freedv=state;
  if(active_receiver->freedv) {
    SetRXAPanelRun(active_receiver->id, 0);
    init_freedv(active_receiver);
    transmitter->freedv_samples=0;
  } else {
    SetRXAPanelRun(active_receiver->id, 1);
    close_freedv(active_receiver);
  }
  g_mutex_unlock(&active_receiver->freedv_mutex);
  g_idle_add(ext_vfo_update,NULL);
}
#endif

void radio_change_region(int r) {
  region=r;
  switch (region) {
    case REGION_UK:
      channel_entries=UK_CHANNEL_ENTRIES;
      band_channels_60m=&band_channels_60m_UK[0];
      bandstack60.entries=UK_CHANNEL_ENTRIES;
      bandstack60.current_entry=0;
      bandstack60.entry=bandstack_entries60_UK;
      break;
    case REGION_OTHER:
      channel_entries=OTHER_CHANNEL_ENTRIES;
      band_channels_60m=&band_channels_60m_OTHER[0];
      bandstack60.entries=OTHER_CHANNEL_ENTRIES;
      bandstack60.current_entry=0;
      bandstack60.entry=bandstack_entries60_OTHER;
      break;
    case REGION_WRC15:
      channel_entries=WRC15_CHANNEL_ENTRIES;
      band_channels_60m=&band_channels_60m_WRC15[0];
      bandstack60.entries=WRC15_CHANNEL_ENTRIES;
      bandstack60.current_entry=0;
      bandstack60.entry=bandstack_entries60_WRC15;
      break;
  }
}
