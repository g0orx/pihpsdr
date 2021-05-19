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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <wdsp.h>

#include "adc.h"
#include "dac.h"
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
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#ifdef GPIO
#include "gpio.h"
#endif
#include "vfo.h"
#include "vox.h"
#include "meter.h"
#include "rx_panadapter.h"
#include "tx_panadapter.h"
#include "waterfall.h"
#include "zoompan.h"
#include "sliders.h"
#include "toolbar.h"
#include "rigctl.h"
#include "ext.h"
#ifdef LOCALCW
#include "iambic.h"
#endif
#include "rigctl_menu.h"
#ifdef MIDI
#include "midi.h"
#include "alsa_midi.h"
#include "midi_menu.h"
#endif
#ifdef CLIENT_SERVER
#include "client_server.h"
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
#define ZOOMPAN_HEIGHT (50)
#define SLIDERS_HEIGHT (100)
#define TOOLBAR_HEIGHT (30)
#define WATERFALL_HEIGHT (105)

GtkWidget *fixed;
static GtkWidget *vfo_panel;
static GtkWidget *meter;
static GtkWidget *menu;
static GtkWidget *zoompan;
static GtkWidget *sliders;
static GtkWidget *toolbar;
static GtkWidget *panadapter;
static GtkWidget *waterfall;
static GtkWidget *audio_waterfall;

#ifdef GPIO
static GtkWidget *encoders;
static cairo_surface_t *encoders_surface = NULL;
#endif
	gint sat_mode;

	int region=REGION_OTHER;

	int echo=0;

	int radio_sample_rate;
	gboolean iqswap;

	static gint save_timer_id;

	DISCOVERED *radio=NULL;
#ifdef CLIENT_SERVER
	gboolean radio_is_remote=FALSE;
#endif

	char property_path[128];
        GMutex property_mutex;

RECEIVER *receiver[MAX_RECEIVERS];
RECEIVER *active_receiver;
TRANSMITTER *transmitter;

int buffer_size=1024; // 64, 128, 256, 512, 1024, 2048
int fft_size=2048; // 1024, 2048, 4096, 8192, 16384

int atlas_penelope=0;
int atlas_clock_source_10mhz=0;
int atlas_clock_source_128mhz=0;
int atlas_config=0;
int atlas_mic_source=0;
int atlas_janus=0;

int classE=0;

int tx_out_of_band=0;

int tx_cfir=0;
int tx_leveler=0;
int alc=TXA_ALC_AV;

double tone_level=0.2;

int filter_board=ALEX;
int pa_enabled=PA_ENABLED;
int pa_power=0;
int pa_trim[11];

int updates_per_second=10;

int panadapter_high=-40;
int panadapter_low=-140;

int display_filled=1;
int display_gradient=0;
int display_detector_mode=DETECTOR_MODE_AVERAGE;
int display_average_mode=AVERAGE_MODE_LOG_RECURSIVE;
double display_average_time=120.0;


int waterfall_high=-100;
int waterfall_low=-150;

int display_zoompan=0;
int display_sliders=0;
int display_toolbar=0;

//double volume=0.2;
double mic_gain=0.0;
int binaural=0;

int mic_linein=0;
//
// linein_gain = 0...31 maps onto -34.5dB ... +12 dB
// (in 1.5 dB steps), and 0 dB corresponds to linein_gain=23
//
int linein_gain=23;
int mic_boost=0;
int mic_bias_enabled=0;
int mic_ptt_enabled=0;
int mic_ptt_tip_bias_ring=0;

//double tune_drive=10;
//double drive=50;

//int drive_level=0;
//int tune_drive_level=0;

int receivers=RECEIVERS;

ADC adc[2];
DAC dac[2];
int adc_attenuation[2];

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
int cw_keyer_speed=16; // 1-60 WPM
int cw_keyer_mode=KEYER_MODE_A;
int cw_keyer_weight=50; // 0-100
int cw_keyer_spacing=0; // 0=on 1=off
int cw_keyer_internal=1; // 0=external 1=internal
int cw_keyer_sidetone_volume=50; // 0-127
int cw_keyer_ptt_delay=20; // 0-255ms
int cw_keyer_hang_time=500; // ms
int cw_keyer_sidetone_frequency=800; // Hz
int cw_breakin=1; // 0=disabled 1=enabled

int cw_is_on_vfo_freq=1;   // 1= signal on VFO freq, 0= signal offset by side tone

int vfo_encoder_divisor=15;

int protocol;
int device;
int new_pa_board=0; // Indicates Rev.24 PA board for HERMES/ANGELIA/ORION
int ozy_software_version;
int mercury_software_version;
int penelope_software_version;
int dot;
int dash;
int adc_overload;
int pll_locked;
unsigned int exciter_power;
unsigned int temperature;
unsigned int average_temperature;
unsigned int n_temperature;
unsigned int current;
unsigned int average_current;
unsigned int n_current;
unsigned int tx_fifo_underrun;
unsigned int tx_fifo_overrun;
unsigned int alex_forward_power;
unsigned int alex_reverse_power;
unsigned int alex_forward_power_average=0;
unsigned int alex_reverse_power_average=0;
unsigned int AIN3;
unsigned int AIN4;
unsigned int AIN6;
unsigned int IO1;
unsigned int IO2;
unsigned int IO3;
int supply_volts;
int ptt=0;
int mox=0;
int tune=0;
int memory_tune=0;
int full_tune=0;
int have_rx_gain=0;
int rx_gain_calibration=14;

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

//int local_audio=0;
//int local_microphone=0;

int eer_pwm_min=100;
int eer_pwm_max=800;

int tx_filter_low=150;
int tx_filter_high=2850;

static int pre_tune_mode;
static int pre_tune_cw_internal;
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
int n_adc=1;

int diversity_enabled=0;
double div_cos=1.0;        // I factor for diversity
double div_sin=1.0;	   // Q factor for diversity
double div_gain=0.0;	   // gain for diversity (in dB)
double div_phase=0.0;	   // phase for diversity (in degrees, 0 ... 360)

double meter_calibration=0.0;
double display_calibration=0.0;

int can_transmit=0;

gboolean duplex=FALSE;
gboolean mute_rx_while_transmitting=FALSE;

double drive_max=100;

gboolean display_sequence_errors=TRUE;
gboolean display_swr_protection=FALSE;
gint sequence_errors=0;

gint rx_height;

void radio_stop() {
  if(can_transmit) {
g_print("radio_stop: TX: CloseChannel: %d\n",transmitter->id);
    CloseChannel(transmitter->id);
  }
  set_displaying(receiver[0],0);
g_print("radio_stop: RX0: CloseChannel: %d\n",receiver[0]->id);
  CloseChannel(receiver[0]->id);
  set_displaying(receiver[1],0);
g_print("radio_stop: RX1: CloseChannel: %d\n",receiver[1]->id);
  CloseChannel(receiver[1]->id);
}

void reconfigure_radio() {
  int i;
  int y;
g_print("reconfigure_radio: receivers=%d\n",receivers);
  rx_height=display_height-VFO_HEIGHT;
  if(display_zoompan) {
    rx_height-=ZOOMPAN_HEIGHT;
  }
  if(display_sliders) {
    rx_height-=SLIDERS_HEIGHT;
  }
  if(display_toolbar) {
    rx_height-=TOOLBAR_HEIGHT;
  }

  y=VFO_HEIGHT;
  for(i=0;i<receivers;i++) {
    reconfigure_receiver(receiver[i],rx_height/receivers);
    gtk_fixed_move(GTK_FIXED(fixed),receiver[i]->panel,0,y);
    receiver[i]->x=0;
    receiver[i]->y=y;
    y+=rx_height/receivers;
  }

  if(display_zoompan) {
    if(zoompan==NULL) {
      zoompan = zoompan_init(display_width,ZOOMPAN_HEIGHT);
      gtk_fixed_put(GTK_FIXED(fixed),zoompan,0,y);
    } else {
      gtk_fixed_move(GTK_FIXED(fixed),zoompan,0,y);
    }
    gtk_widget_show_all(zoompan);
    y+=ZOOMPAN_HEIGHT;
  } else {
    if(zoompan!=NULL) {
      gtk_container_remove(GTK_CONTAINER(fixed),zoompan); 
      zoompan=NULL;
    }
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
    y+=SLIDERS_HEIGHT;
  } else {
    if(sliders!=NULL) {
      gtk_container_remove(GTK_CONTAINER(fixed),sliders); 
      sliders=NULL;
    }
  }

  if(display_toolbar) {
    if(toolbar==NULL) {
      toolbar = toolbar_init(display_width,TOOLBAR_HEIGHT,top_window);
      gtk_fixed_put(GTK_FIXED(fixed),toolbar,0,y);
    } else {
      gtk_fixed_move(GTK_FIXED(fixed),toolbar,0,y);
    }
    gtk_widget_show_all(toolbar);
  } else {
    if(toolbar!=NULL) {
      gtk_container_remove(GTK_CONTAINER(fixed),toolbar);
      toolbar=NULL;
    }
  }

  if(can_transmit) {
    if(!duplex) {
      reconfigure_transmitter(transmitter,display_width,rx_height);
    }
  }
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

static void create_visual() {
  int y=0;

  fixed=gtk_fixed_new();
  g_object_ref(grid);  // so it does not get deleted
  gtk_container_remove(GTK_CONTAINER(top_window),grid);
  gtk_container_add(GTK_CONTAINER(top_window), fixed);

//g_print("radio: vfo_init\n");
  vfo_panel = vfo_init(VFO_WIDTH,VFO_HEIGHT,top_window);
  gtk_fixed_put(GTK_FIXED(fixed),vfo_panel,0,y);

//g_print("radio: meter_init\n");
  meter = meter_init(METER_WIDTH,METER_HEIGHT,top_window);
  gtk_fixed_put(GTK_FIXED(fixed),meter,VFO_WIDTH,y);


  GtkWidget *minimize_b=gtk_button_new_with_label("Hide");
  gtk_widget_override_font(minimize_b, pango_font_description_from_string("FreeSans Bold 10"));
  gtk_widget_set_size_request (minimize_b, MENU_WIDTH, MENU_HEIGHT);
  g_signal_connect (minimize_b, "button-press-event", G_CALLBACK(minimize_cb), NULL) ;
  gtk_fixed_put(GTK_FIXED(fixed),minimize_b,VFO_WIDTH+METER_WIDTH,y);
  y+=MENU_HEIGHT;

  GtkWidget *menu_b=gtk_button_new_with_label("Menu");
  gtk_widget_override_font(menu_b, pango_font_description_from_string("FreeSans Bold 10"));
  gtk_widget_set_size_request (menu_b, MENU_WIDTH, MENU_HEIGHT);
  g_signal_connect (menu_b, "button-press-event", G_CALLBACK(menu_cb), NULL) ;
  gtk_fixed_put(GTK_FIXED(fixed),menu_b,VFO_WIDTH+METER_WIDTH,y);
  y+=MENU_HEIGHT;


  rx_height=display_height-VFO_HEIGHT;
  if(display_zoompan) {
    rx_height-=ZOOMPAN_HEIGHT;
  }
  if(display_sliders) {
    rx_height-=SLIDERS_HEIGHT;
  }
  if(display_toolbar) {
    rx_height-=TOOLBAR_HEIGHT;
  }

  //
  // To be on the safe side, we create ALL receiver panels here
  // If upon startup, we only should display one panel, we do the switch below
  //
  for(int i=0;i<RECEIVERS;i++) {
#ifdef CLIENT_SERVER
    if(radio_is_remote) {
      receiver_create_remote(receiver[i]);
    } else {
#endif
      receiver[i]=create_receiver(i, buffer_size, fft_size, display_width, updates_per_second, display_width, rx_height/RECEIVERS);
      setSquelch(receiver[i]);
#ifdef CLIENT_SERVER
    }
#endif
    receiver[i]->x=0;
    receiver[i]->y=y;
    // Upon startup, if RIT or CTUN is active, tell WDSP.
#ifdef CLIENT_SERVER
    if(!radio_is_remote) {
#endif
      set_displaying(receiver[i],1);
      set_offset(receiver[i],vfo[i].offset);
#ifdef CLIENT_SERVER
    }
#endif
    gtk_fixed_put(GTK_FIXED(fixed),receiver[i]->panel,0,y);
    g_object_ref((gpointer)receiver[i]->panel);
    y+=rx_height/RECEIVERS;
  }
  
  //
  // Sanity check: in old protocol, all receivers must have the same sample rate
  //
  if((protocol==ORIGINAL_PROTOCOL) && (RECEIVERS==2) && (receiver[0]->sample_rate!=receiver[1]->sample_rate)) {
    receiver[1]->sample_rate=receiver[0]->sample_rate;
  }

  active_receiver=receiver[0];

// TEMP
#ifdef CLIENT_SERVER
if(!radio_is_remote) {
#endif
  //g_print("Create transmitter\n");
  if(can_transmit) {
    double pk;
    if(duplex) {
      transmitter=create_transmitter(CHANNEL_TX, buffer_size, fft_size, updates_per_second, display_width/4, display_height/2);
    } else {
      int tx_height=display_height-VFO_HEIGHT;
      if(display_zoompan) tx_height-=ZOOMPAN_HEIGHT;
      if(display_sliders) tx_height-=SLIDERS_HEIGHT;
      if(display_toolbar) tx_height-=TOOLBAR_HEIGHT;
      transmitter=create_transmitter(CHANNEL_TX, buffer_size, fft_size, updates_per_second, display_width, tx_height);
    }
    transmitter->x=0;
    transmitter->y=VFO_HEIGHT;

    calcDriveLevel();

#ifdef PURESIGNAL
    tx_set_ps_sample_rate(transmitter,protocol==NEW_PROTOCOL?192000:active_receiver->sample_rate);
    receiver[PS_TX_FEEDBACK]=create_pure_signal_receiver(PS_TX_FEEDBACK, buffer_size,protocol==ORIGINAL_PROTOCOL?active_receiver->sample_rate:192000,display_width);
    receiver[PS_RX_FEEDBACK]=create_pure_signal_receiver(PS_RX_FEEDBACK, buffer_size,protocol==ORIGINAL_PROTOCOL?active_receiver->sample_rate:192000,display_width);
    switch (protocol) {
      case NEW_PROTOCOL:
        pk = 0.2899;
        break;
      case ORIGINAL_PROTOCOL:
        switch (device) {
          case DEVICE_HERMES_LITE2:
            pk = 0.2300;
            break;
          default:
            pk = 0.4067;
            break;
        }
    }
    SetPSHWPeak(transmitter->id, pk);
#endif

  }
#ifdef CLIENT_SERVER
}
#endif

#ifdef AUDIO_WATERFALL
  audio_waterfall=audio_waterfall_init(200,100);
  gtk_fixed_put(GTK_FIXED(fixed),audio_waterfall,0,VFO_HEIGHT+20);
#endif

  gboolean init_gpio=FALSE;
#ifdef LOCALCW
  init_gpio=TRUE;
#endif
#ifdef PTT
  init_gpio=TRUE;
#endif
#ifdef GPIO
  init_gpio=TRUE;
#endif

  if(init_gpio) {
#ifdef GPIO
    if(gpio_init()<0) {
      g_print("GPIO failed to initialize\n");
    }
#endif
  }

#ifdef LOCALCW
  // init local keyer if enabled
  if (cw_keyer_internal == 0) {
	g_print("Initialize keyer.....\n");
    keyer_update();
  }
#endif
  
#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_init(0,display_width,receiver[0]->sample_rate);
      break;
    case NEW_PROTOCOL:
      new_protocol_init(display_width);
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      soapy_protocol_init(0,false);
      break;
#endif
  }
#ifdef CLIENT_SERVER
  }
#endif

  if(display_zoompan) {
    zoompan = zoompan_init(display_width,ZOOMPAN_HEIGHT);
    gtk_fixed_put(GTK_FIXED(fixed),zoompan,0,y);
    y+=ZOOMPAN_HEIGHT;
  }

  if(display_sliders) {
//g_print("create sliders\n");
    sliders = sliders_init(display_width,SLIDERS_HEIGHT);
    gtk_fixed_put(GTK_FIXED(fixed),sliders,0,y);
    y+=SLIDERS_HEIGHT;
  }


  if(display_toolbar) {
    toolbar = toolbar_init(display_width,TOOLBAR_HEIGHT,top_window);
    gtk_fixed_put(GTK_FIXED(fixed),toolbar,0,y);
    y+=TOOLBAR_HEIGHT;
  }

//
// Now, if there should only one receiver be displayed
// at startup, do the change. We must momentarily fake
// the number of receivers otherwise radio_change_receivers
// will do nothing.
//
g_print("create_visual: receivers=%d RECEIVERS=%d\n",receivers,RECEIVERS);
  if (receivers != RECEIVERS) {
    int r=receivers;
    receivers=RECEIVERS;
g_print("create_visual: calling radio_change_receivers: receivers=%d r=%d\n",receivers,r);
    radio_change_receivers(r);
  }

  //gtk_widget_show_all (fixed);
  gtk_widget_show_all (top_window);

}
  
void start_radio() {
  int i;
  int y;
//g_print("start_radio: selected radio=%p device=%d\n",radio,radio->device);
  gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_WATCH));

  int rc;

  protocol=radio->protocol;
  device=radio->device;

  // set the default power output and max drive value
  drive_max=100.0;
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      switch(device) {
        case DEVICE_METIS:
          pa_power=PA_1W;
          break;
        case DEVICE_HERMES:
          pa_power=PA_100W;
          break;
        case DEVICE_GRIFFIN:
          pa_power=PA_100W;
          break;
        case DEVICE_ANGELIA:
          pa_power=PA_100W;
          break;
        case DEVICE_ORION:
          pa_power=PA_100W;
          break;
        case DEVICE_HERMES_LITE:
          pa_power=PA_1W;
          break;
        case DEVICE_HERMES_LITE2:
          pa_power=PA_10W;
          break;
        case DEVICE_ORION2:
          pa_power=PA_200W;
          break;
        case DEVICE_STEMLAB:
          pa_power=PA_100W;
          break;
      }
      break;
    case NEW_PROTOCOL:
      switch(device) {
        case NEW_DEVICE_ATLAS:
          pa_power=PA_1W;
          break;
        case NEW_DEVICE_HERMES:
        case NEW_DEVICE_HERMES2:
          pa_power=PA_100W;
          break;
        case NEW_DEVICE_ANGELIA:
          pa_power=PA_100W;
          break;
        case NEW_DEVICE_ORION:
          pa_power=PA_100W;
          break;
        case NEW_DEVICE_HERMES_LITE:
          pa_power=PA_1W;
          break;
        case NEW_DEVICE_HERMES_LITE2:
          pa_power=PA_10W;
          break;
        case NEW_DEVICE_ORION2:
          pa_power=PA_200W;
          break;
        case DEVICE_STEMLAB:
          pa_power=PA_100W;
          break;
      }
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      if(strcmp(radio->name,"lime")==0) {
        drive_max=64.0;
      } else if(strcmp(radio->name,"plutosdr")==0) {
        drive_max=89.0;
      }
      pa_power=PA_1W;
      break;
#endif
  }

  switch(pa_power) {
    case PA_1W:
      for(i=0;i<11;i++) {
        pa_trim[i]=i*100;
      }
      break;
    case PA_10W:
      for(i=0;i<11;i++) {
        pa_trim[i]=i;
      }
      break;
    case PA_30W:
      for(i=0;i<11;i++) {
        pa_trim[i]=i*3;
      }
      break;
    case PA_50W:
      for(i=0;i<11;i++) {
        pa_trim[i]=i*5;
      }
      break;
    case PA_100W:
      for(i=0;i<11;i++) {
        pa_trim[i]=i*10;
      }
      break;
    case PA_200W:
      for(i=0;i<11;i++) {
        pa_trim[i]=i*20;
      }
      break;
    case PA_500W:
      for(i=0;i<11;i++) {
        pa_trim[i]=i*50;
      }
      break;
  }

  //
  // have_rx_gain determines whether we have "ATT" or "RX Gain" Sliders
  // It is set for HermesLite (and RadioBerry, for that matter)
  //
  have_rx_gain=0;
  switch (protocol) {
    case ORIGINAL_PROTOCOL:
	switch (device) {
	    case DEVICE_HERMES_LITE:
	    case DEVICE_HERMES_LITE2:
		have_rx_gain=1;
		rx_gain_calibration=14;
		break;
	    default:
		have_rx_gain=0;
		rx_gain_calibration=0;
		break;
	}
	break;
    case NEW_PROTOCOL:
	switch (device) {
	    case NEW_DEVICE_HERMES_LITE:
	    case NEW_DEVICE_HERMES_LITE2:
		have_rx_gain=1;
		rx_gain_calibration=14;
		break;
	    default:
		have_rx_gain=0;
		rx_gain_calibration=0;
		break;
	}
	break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
	have_rx_gain=1;
	rx_gain_calibration=10;
        break;
#endif
    default:
	have_rx_gain=0;
	break;
  }

  //
  // can_transmit decides whether we have a transmitter.
  //
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL:
      can_transmit=1;
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      can_transmit=(radio->info.soapy.tx_channels!=0);
      g_print("start_radio: can_transmit=%d tx_channels=%d\n",can_transmit,(int)radio->info.soapy.tx_channels);
      break;
#endif
  }

//
// A semaphore for safely writing to the props file
//
  g_mutex_init(&property_mutex);

//
//  Create text for the top line of the piHPSDR window
//
    char text[1024];
    switch(protocol) {
      case ORIGINAL_PROTOCOL:
      case NEW_PROTOCOL:
#ifdef USBOZY
        if(device==DEVICE_OZY) {
          sprintf(text,"%s (%s) on USB /dev/ozy\n", radio->name, protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2");
        } else {
#endif
          sprintf(text,"Starting %s (%s v%d.%d)",
                        radio->name,
                        protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2",
                        radio->software_version/10,
                        radio->software_version%10);
#ifdef USBOZY
	}
#endif
        break;
    }


  char p[32];
  char version[32];
  char mac[32];
  char ip[32];
  char iface[64];

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      strcpy(p,"Protocol 1");
      sprintf(version,"v%d.%d)",
                   radio->software_version/10,
                   radio->software_version%10);
      sprintf(mac,"%02X:%02X:%02X:%02X:%02X:%02X",
                  radio->info.network.mac_address[0],
                  radio->info.network.mac_address[1],
                  radio->info.network.mac_address[2],
                  radio->info.network.mac_address[3],
                  radio->info.network.mac_address[4],
                  radio->info.network.mac_address[5]);
      sprintf(ip,"%s", inet_ntoa(radio->info.network.address.sin_addr));
      sprintf(iface,"%s", radio->info.network.interface_name);
      break;
    case NEW_PROTOCOL:
      strcpy(p,"Protocol 2");
      sprintf(version,"v%d.%d)",
                   radio->software_version/10,
                   radio->software_version%10);
      sprintf(mac,"%02X:%02X:%02X:%02X:%02X:%02X",
                  radio->info.network.mac_address[0],
                  radio->info.network.mac_address[1],
                  radio->info.network.mac_address[2],
                  radio->info.network.mac_address[3],
                  radio->info.network.mac_address[4],
                  radio->info.network.mac_address[5]);
      sprintf(ip,"%s", inet_ntoa(radio->info.network.address.sin_addr));
      sprintf(iface,"%s", radio->info.network.interface_name);
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      strcpy(p,"SoapySDR");
      sprintf(version,"v%d.%d.%d)",
                   radio->software_version/100,
                   (radio->software_version%100)/10,
                   radio->software_version%10);
      strcpy(mac,"");
      strcpy(ip,"");
      strcpy(iface,"");
      break;
#endif
  }

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL:
#ifdef USBOZY
      if(device==DEVICE_OZY) {
        sprintf(text,"%s (%s) on USB /dev/ozy\n", radio->name, p);
      } else {
#endif
        sprintf(text,"Starting %s (%s %s)",
                      radio->name,
                      p,
                      version);
#ifdef USBOZY
      }
#endif
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      sprintf(text,"Starting %s (%s %s)",
                    radio->name,
                    "SoapySDR",
                    version);
      break;
#endif
    }

  status_text(text);

  switch (protocol) {
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL:
      sprintf(text,"piHPSDR: %s (%s %s) %s (%s) on %s",
                   radio->name,
                   p,
                   version,
                   ip,
                   mac,
                   iface);
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      sprintf(text,"piHPSDR: %s (%s %s)",
                   radio->name,
                   p,
                   version);
      break;
#endif
  }

  gtk_window_set_title (GTK_WINDOW (top_window), text);

//
// determine name of the props file
//
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
    case NEW_PROTOCOL:
      switch(device) {
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
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      sprintf(property_path,"%s.props",radio->name);
      break;
#endif
  }

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      switch(device) {
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
      switch(device) {
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
 
  //
  // Determine number of ADCs in the device
  //
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      switch(device) {
        case DEVICE_METIS: // No support for multiple MERCURY cards on a single ATLAS bus.
        case DEVICE_HERMES:
        case DEVICE_HERMES_LITE:
        case DEVICE_HERMES_LITE2:
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
        case NEW_DEVICE_HERMES_LITE2:
          n_adc=1;
          break;
        default:
          n_adc=2;
          break;
      }
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      if(strcmp(radio->name,"lime")==0) {
        n_adc=2;
      } else {
        n_adc=1;
      }
      break;
#endif
    default:
      break;
  }

  iqswap=0;

#ifdef SOAPYSDR
  if(device==SOAPYSDR_USB_DEVICE) {
    iqswap=1;
    receivers=1;
    filter_board=NONE;
  }
#endif

  adc[0].antenna=ANTENNA_1;
  adc[0].filters=AUTOMATIC;
  adc[0].hpf=HPF_13;
  adc[0].lpf=LPF_30_20;
  adc[0].dither=FALSE;
  adc[0].random=FALSE;
  adc[0].preamp=FALSE;
  if(have_rx_gain) {
    adc[0].attenuation=14;
  } else {
    adc[0].attenuation=0;
  }
#ifdef SOAPYSDR
  adc[0].antenna=0;
  if(device==SOAPYSDR_USB_DEVICE) {
    adc[0].gain=0;
    if(radio->info.soapy.rx_gains>0) {
      adc[0].min_gain=radio->info.soapy.rx_range[0].minimum;
      adc[0].max_gain=radio->info.soapy.rx_range[0].maximum;;
    } else {
      adc[0].min_gain=0.0;
      adc[0].max_gain=100.0;
    }
    adc[0].agc=FALSE;
    dac[0].antenna=1;
    dac[0].gain=0;
  }
#endif


  adc[1].antenna=ANTENNA_1;
  adc[1].filters=AUTOMATIC;
  adc[1].hpf=HPF_9_5;
  adc[1].lpf=LPF_60_40;
  adc[1].dither=FALSE;
  adc[1].random=FALSE;
  adc[1].preamp=FALSE;
  if(have_rx_gain) {
    adc[0].attenuation=14;
  } else {
    adc[1].attenuation=0;
  }
#ifdef SOAPYSDR
  adc[1].antenna=0;
  if(device==SOAPYSDR_USB_DEVICE) {
    adc[1].gain=0;
    if(radio->info.soapy.rx_gains>0) {
      adc[1].min_gain=radio->info.soapy.rx_range[0].minimum;
      adc[1].max_gain=radio->info.soapy.rx_range[0].maximum;;
    } else {
      adc[1].min_gain=0.0;
      adc[1].max_gain=100.0;
    }
    adc[1].max_gain=0;
    adc[1].agc=FALSE;
    dac[1].antenna=1;
    dac[1].gain=0;
  }

  radio_sample_rate=radio->info.soapy.sample_rate;
#endif

//g_print("meter_calibration=%f display_calibration=%f\n", meter_calibration, display_calibration);

#ifdef GPIO
  switch(controller) {
    case CONTROLLER2_V1:
    case CONTROLLER2_V2:
      display_zoompan=1;
      display_sliders=0;
      display_toolbar=0;
      break;
    default:
      display_zoompan=1;
      display_sliders=1;
      display_toolbar=1;
      break;
  }
#else
  display_zoompan=1;
  display_sliders=1;
  display_toolbar=1;
#endif

  temperature=0;
  average_temperature=0;
  n_temperature=0;
  current=0;
  average_current=0;
  tx_fifo_underrun=0;
  tx_fifo_overrun=0;
  n_current=0;

  display_sequence_errors=TRUE;

  radioRestoreState();

//
// It is possible that an option has been read in
// which is not compatible with the hardware.
// Change setting to reasonable value then.
// This could possibly be moved to radioRestoreState().
// 
// Sanity Check #1: restrict buffer size in new protocol
//
  switch (protocol) {
    case ORIGINAL_PROTOCOL:
      if (buffer_size > 2048) buffer_size=2048;
      break;
    case NEW_PROTOCOL:
      if (buffer_size > 512) buffer_size=512;
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      if (buffer_size > 2048) buffer_size=2048;
      break;
#endif
  }
//
// Sanity Check #2: enable diversity only if there are two RX and two ADCs
//
   if (RECEIVERS < 2 || n_adc < 2) {
     diversity_enabled=0;
   }

  radio_change_region(region);

  create_visual();

  // save every 30 seconds
  //save_timer_id=gdk_threads_add_timeout(30000, save_cb, NULL);

  if(rigctl_enable) {
    launch_rigctl();
    if (serial_enable) {
      launch_serial();
    }
  } else {
    // since we do not spawn the serial thread,
    // disable serial
    serial_enable=0;
  }

  if(can_transmit) {
    calcDriveLevel();
    if(transmitter->puresignal) {
      tx_set_ps(transmitter,transmitter->puresignal);
    }
  }

  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }

#ifdef SOAPYSDR
  if(protocol==SOAPYSDR_PROTOCOL) {
    RECEIVER *rx=receiver[0];
    soapy_protocol_create_receiver(rx);
    if(can_transmit) {
      soapy_protocol_create_transmitter(transmitter);
      soapy_protocol_set_tx_antenna(transmitter,dac[0].antenna);
      soapy_protocol_set_tx_gain(transmitter,transmitter->drive);
      soapy_protocol_set_tx_frequency(transmitter);
      soapy_protocol_start_transmitter(transmitter);
    }

    soapy_protocol_set_rx_antenna(rx,adc[0].antenna);
    soapy_protocol_set_rx_frequency(rx,VFO_A);
    soapy_protocol_set_automatic_gain(rx,adc[0].agc);

    if(vfo[0].ctun) {
      setFrequency(vfo[0].ctun_frequency);
    }
    soapy_protocol_start_receiver(rx);

//g_print("radio: set rf_gain=%f\n",rx->rf_gain);
    soapy_protocol_set_gain(rx);

  }
#endif

  g_idle_add(ext_vfo_update,(gpointer)NULL);

  gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));

#ifdef MIDI
  //
  // The MIDI devices could not be opened in midi_restore_state() since MIDI events
  // must not fly in before the radio is fully configured. Therefore midi_restore_state()
  // simply marks the devices to be opened here by hi-jacking the "active" flag. Note that
  // apart from this (ab)use, this flag is updated ONLY in register_midi_device() and
  // close_midi_device().
  //
  for (i=0; i<n_midi_devices; i++) {
    if (midi_devices[i].active) {
      //
      // If device was marked "active" in the props file, open (register) it
      // Note this flag is hi-jacked, so clear it before opening. It will be set
      // if the MIDI device has been opened successfully
      //
      midi_devices[i].active=0;
      register_midi_device(i);
    }
  }
#endif

#ifdef CLIENT_SERVER
  if(hpsdr_server) {
    create_hpsdr_server();
  }
#endif
}

void disable_rigctl() {
   g_print("RIGCTL: disable_rigctl()\n");
   close_rigctl_ports();
}
 

void radio_change_receivers(int r) {
g_print("radio_change_receivers: from %d to %d\n",receivers,r);
  // The button in the radio menu will call this function even if the
  // number of receivers has not changed.
  if (receivers == r) return;
  //
  // When changing the number of receivers, restart the
  // old protocol
  //
#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
    if (protocol == ORIGINAL_PROTOCOL) {
      old_protocol_stop();
    }
#ifdef CLIENT_SERVER
  }
#endif
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
#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
    if (protocol == ORIGINAL_PROTOCOL) {
      old_protocol_run();
    }
#ifdef CLIENT_SERVER
  }
#endif
}

void radio_change_sample_rate(int rate) {
  int i;
  
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      // The radio menu calls this function even if the sample rate
      // has not changed. Do nothing in this case.
      if (receiver[0]->sample_rate != rate) {
        radio_sample_rate=rate;
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
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      soapy_protocol_change_sample_rate(receiver[0]);
      soapy_protocol_set_mic_sample_rate(rate);
      break;
#endif
  }
}

static void rxtx(int state) {
  int i;

  if(state) {
    // switch to tx
#ifdef PURESIGNAL
    RECEIVER *rx_feedback=receiver[PS_RX_FEEDBACK];
    RECEIVER *tx_feedback=receiver[PS_TX_FEEDBACK];

    rx_feedback->samples=0;
    tx_feedback->samples=0;
#endif

    if(!duplex) {
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
    }

    if(duplex) {
      gtk_widget_show_all(transmitter->dialog);
      if(transmitter->dialog_x!=-1 && transmitter->dialog_y!=-1) {
       gtk_window_move(GTK_WINDOW(transmitter->dialog),transmitter->dialog_x,transmitter->dialog_y);
      }
    } else {
      gtk_fixed_put(GTK_FIXED(fixed),transmitter->panel,transmitter->x,transmitter->y);
    }
    
    SetChannelState(transmitter->id,1,0);
    tx_set_displaying(transmitter,1);
    switch(protocol) {
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
        soapy_protocol_set_tx_frequency(transmitter);
        //soapy_protocol_start_transmitter(transmitter);
        break;
#endif
    }
  } else {
    // switch to rx
    switch(protocol) {
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
        //soapy_protocol_stop_transmitter(transmitter);
        break;
#endif
    }
    SetChannelState(transmitter->id,0,1);
    tx_set_displaying(transmitter,0);
    if(duplex) {
      gtk_window_get_position(GTK_WINDOW(transmitter->dialog),&transmitter->dialog_x,&transmitter->dialog_y);
      gtk_widget_hide(transmitter->dialog);
    } else {
      gtk_container_remove(GTK_CONTAINER(fixed), transmitter->panel);
    }
    if(!duplex) {
      for(i=0;i<receivers;i++) {
        gtk_fixed_put(GTK_FIXED(fixed),receiver[i]->panel,receiver[i]->x,receiver[i]->y);
        SetChannelState(receiver[i]->id,1,0);
        set_displaying(receiver[i],1);
      }
    }
  }

#ifdef PURESIGNAL
  if(transmitter->puresignal) {
    SetPSMox(transmitter->id,state);
  }
#endif
}

void setMox(int state) {
  if(!can_transmit) return;
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
  switch(protocol) {
    case NEW_PROTOCOL:
      schedule_high_priority();
      schedule_receive_specific();
      break;
    default:
      break;
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

void frequency_changed(RECEIVER *rx) {
//g_print("frequency_changed: channel=%d frequency=%ld lo=%ld error=%ld ctun=%d offset=%ld\n",rx->channel,rx->frequency_a,rx->lo_a,rx->error_a,rx->ctun,rx->offset);
  if(vfo[0].ctun) {
    SetRXAShiftFreq(rx->id, (double)vfo[0].offset);
    RXANBPSetShiftFrequency(rx->id, (double)vfo[0].offset);
#ifdef SOAPYSDR
    if(protocol==SOAPYSDR_PROTOCOL) {
/*
      if(radio->can_transmit) {
        if(radio->transmitter!=NULL && radio->transmitter->rx==rx) {
          //soapy_protocol_set_tx_frequency(radio->transmitter);
        }
      }
*/
    }
#endif
  } else {
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
#ifdef SOAPYSDR
    } else if(protocol==SOAPYSDR_PROTOCOL) {
      soapy_protocol_set_rx_frequency(rx,VFO_A);
/*
      if(radio->can_transmit) {
        if(radio->transmitter!=NULL && radio->transmitter->rx==rx) {
          soapy_protocol_set_tx_frequency(radio->transmitter);
        }
      }
*/
#endif
    }
    vfo[0].band=get_band_from_frequency(vfo[0].frequency);
  }
}


void setTune(int state) {
  int i;

  if(!can_transmit) return;

  // if state==tune, this function is a no-op

  if(tune!=state) {
    vox_cancel();
    if (vox || mox) {
      rxtx(0);
      vox=0;
      mox=0;
    }
    if(state) {
      if (transmitter->puresignal) {
	//
	// DL1YCF:
	// Some users have reported that especially when having
	// very long (10 hours) operating times with PS, hitting
	// the "TUNE" button makes the PS algorithm crazy, such that
	// it produces a very broad line spectrum. Experimentally, it
	// has been observed that this can be avoided by hitting
	// "Off" in the PS menu before hitting "TUNE", and hitting
	// "Restart" in the PS menu when tuning is complete.
	//
	// It is therefore suggested to to so implicitly when PS
	// is enabled.
	//
	// So before start tuning: Reset PS engine
	//
        SetPSControl(transmitter->id, 1, 0, 0, 0);
	usleep(50000);
      }
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
      if(!duplex) {
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
      }

      int txmode=get_tx_mode();
      pre_tune_mode=txmode;
      pre_tune_cw_internal=cw_keyer_internal;

      //
      // in USB/DIGU/DSB, tune 1000 Hz above carrier
      // in LSB/DIGL,     tune 1000 Hz below carrier
      // all other (CW, AM, FM): tune on carrier freq.
      //
      switch(txmode) {
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

      switch(txmode) {
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
          cw_keyer_internal=pre_tune_cw_internal;
          break;
      }
      if (transmitter->puresignal) {
	//
	// DL1YCF:
	// Since we have done a "PS reset" when we started tuning,
	// resume PS engine now.
	//
	SetPSControl(transmitter->id, 0, 0, 1, 0);
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
  int v=active_receiver->id;
  vfo[v].band=get_band_from_frequency(f);

  switch(protocol) {
    case NEW_PROTOCOL:
    case ORIGINAL_PROTOCOL:
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
#endif
      if(vfo[v].ctun) {
        long long minf=vfo[v].frequency-(long long)(active_receiver->sample_rate/2);
        long long maxf=vfo[v].frequency+(long long)(active_receiver->sample_rate/2);
        if(f<minf) f=minf;
        if(f>maxf) f=maxf;
        vfo[v].ctun_frequency=f;
        vfo[v].offset=f-vfo[v].frequency;
        set_offset(active_receiver,vfo[v].offset);
        return;
      } else {
        vfo[v].frequency=f;
      }
      break;
  }

  switch(protocol) {
    case NEW_PROTOCOL:
      schedule_high_priority();
      break;
    case ORIGINAL_PROTOCOL:
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      if(!vfo[v].ctun) {
        soapy_protocol_set_rx_frequency(active_receiver,v);
        vfo[v].offset=0;
      }
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
  int v=get_tx_vfo();

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

  return level;
}

void calcDriveLevel() {
  transmitter->drive_level=calcLevel(transmitter->drive);
  if(isTransmitting()  && protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
//g_print("calcDriveLevel: drive=%d drive_level=%d\n",transmitter->drive,transmitter->drive_level);
}

void setDrive(double value) {
    transmitter->drive=value;
    switch(protocol) {
      case ORIGINAL_PROTOCOL:
      case NEW_PROTOCOL:
        calcDriveLevel();
        break;
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
        soapy_protocol_set_tx_gain(transmitter,transmitter->drive);
        break;
#endif
    }
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
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
        soapy_protocol_set_gain_element(active_receiver,radio->info.soapy.rx_gain[0],(int)adc[0].gain);
        break;
#endif
    }
}

//
// For HPSDR, only receiver[0]->rx_antenna has an effect
// The antenna is set according to what is stored in the "band" info
// We have to call this routine in the HPSDR case each time a band is switched.
//
void set_alex_rx_antenna() {
    BAND *band;
    switch (protocol) {
      case ORIGINAL_PROTOCOL:
        band=band_get_band(vfo[VFO_A].band);
        receiver[0]->alex_antenna=band->alexRxAntenna;
        break;
      case NEW_PROTOCOL:
        band=band_get_band(vfo[VFO_A].band);
        receiver[0]->alex_antenna=band->alexRxAntenna;
        schedule_high_priority();
        break;
      }
}

//
// For HPSDR, determine which band control the TX and
// set TX antenna accordingly
// We have to call this routine
// in the HPSDR case each time the TX band is switched,
// which is for each band switch, each time "split" is
// changed, and in case of "split", each time the active
// RX changes!
//
void set_alex_tx_antenna() {
    BAND *band;
    if (!can_transmit) return;
    switch (protocol) {
      case ORIGINAL_PROTOCOL:
        band=band_get_band(vfo[get_tx_vfo()].band);
        transmitter->alex_antenna=band->alexTxAntenna;
	break;
      case NEW_PROTOCOL:
        band=band_get_band(vfo[get_tx_vfo()].band);
        transmitter->alex_antenna=band->alexTxAntenna;
        schedule_high_priority();
	break;
    }
}

//
// For HPSDR, only receiver[0]->alex_attenuation has an effect
//
void set_alex_attenuation(int v) {
    receiver[0]->alex_attenuation=v;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
}

void radioRestoreState() {
  char name[32];
  char *value;
  int i;

g_print("radioRestoreState: %s\n",property_path);
  g_mutex_lock(&property_mutex);
  loadProperties(property_path);

  value=getProperty("display_filled");
  if(value) display_filled=atoi(value);
  value=getProperty("display_gradient");
  if(value) display_gradient=atoi(value);
  value=getProperty("display_zoompan");
  if(value) display_zoompan=atoi(value);
  value=getProperty("display_sliders");
  if(value) display_sliders=atoi(value);
  value=getProperty("display_toolbar");
  if(value) display_toolbar=atoi(value);

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
#ifdef CLIENT_SERVER
#endif
  } else {
#endif
    value=getProperty("diversity_enabled");
    if (value) diversity_enabled=atoi(value);
    value=getProperty("diversity_gain");
    if (value) div_gain=atof(value);
    value=getProperty("diversity_phase");
    if (value) div_phase=atof(value);
    value=getProperty("diversity_cos");
    if (value) div_cos=atof(value);
    value=getProperty("diversity_sin");
    if (value) div_sin=atof(value);
    value=getProperty("new_pa_board");
    if (value) new_pa_board=atoi(value);
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
    value=getProperty("pa_enabled");
    if(value) pa_enabled=atoi(value);
    value=getProperty("pa_power");
    if(value) pa_power=atoi(value);
    for(i=0;i<11;i++) {
      sprintf(name,"pa_trim[%d]",i);
      value=getProperty(name);
      if(value) pa_trim[i]=atoi(value);
    }
    value=getProperty("updates_per_second");
    if(value) updates_per_second=atoi(value);
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
    value=getProperty("cw_is_on_vfo_freq");
    if(value) cw_is_on_vfo_freq=atoi(value);
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
    value=getProperty("cw_keyer_internal");
    if(value) cw_keyer_internal=atoi(value);
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
//    value=getProperty("local_audio");
//    if(value) local_audio=atoi(value);
//    value=getProperty("n_selected_output_device");
//    if(value) n_selected_output_device=atoi(value);
//    value=getProperty("local_microphone");
//    if(value) local_microphone=atoi(value);
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

    value=getProperty("receivers");
    if(value) receivers=atoi(value);

    value=getProperty("iqswap");
    if(value) iqswap=atoi(value);

    value=getProperty("rx_gain_calibration");
    if(value) rx_gain_calibration=atoi(value);


    filterRestoreState();
    bandRestoreState();
    memRestoreState();
    vfo_restore_state();
    modesettings_restore_state();
#ifdef GPIO
    gpio_restore_actions();
#endif
    value=getProperty("rigctl_enable");
    if(value) rigctl_enable=atoi(value);
    value=getProperty("rigctl_port_base");
    if(value) rigctl_port_base=atoi(value);
    value=getProperty("rigctl_serial_enable");
    if (value) serial_enable=atoi(value);
    value=getProperty("rigctl_serial_baud_rate");
    if (value) serial_baud_rate=atoi(value);
    value=getProperty("rigctl_serial_port");
    if (value) strcpy(ser_port,value);

    value=getProperty("adc_0_attenuation");
    if(value) adc_attenuation[0]=atoi(value);
    value=getProperty("adc_1_attenuation");
    if(value) adc_attenuation[1]=atoi(value);
	
    value=getProperty("split");
    if(value) split=atoi(value);
    value=getProperty("duplex");
    if(value) duplex=atoi(value);
    value=getProperty("sat_mode");
    if(value) sat_mode=atoi(value);
    value=getProperty("mute_rx_while_transmitting");
    if(value) mute_rx_while_transmitting=atoi(value);

    value=getProperty("radio.adc[0].filters");
    if(value) adc[0].filters=atoi(value);
    value=getProperty("radio.adc[0].hpf");
    if(value) adc[0].hpf=atoi(value);
    value=getProperty("radio.adc[0].lpf");
    if(value) adc[0].lpf=atoi(value);
    value=getProperty("radio.adc[0].antenna");
    if(value) adc[0].antenna=atoi(value);
    value=getProperty("radio.adc[0].dither");
    if(value) adc[0].dither=atoi(value);
    value=getProperty("radio.adc[0].random");
    if(value) adc[0].random=atoi(value);
    value=getProperty("radio.adc[0].preamp");
    if(value) adc[0].preamp=atoi(value);
    value=getProperty("radio.adc[0].attenuation");
    if(value) adc[0].attenuation=atoi(value);
    value=getProperty("radio.adc[0].enable_step_attenuation");
    if(value) adc[0].enable_step_attenuation=atoi(value);
    value=getProperty("radio.adc[0].gain");
    if(value) adc[0].gain=atof(value);
    value=getProperty("radio.adc[0].min_gain");
    if(value) adc[0].min_gain=atof(value);
    value=getProperty("radio.adc[0].max_gain");
    if(value) adc[0].max_gain=atof(value);

#ifdef  SOAPYSDR
    if(device==SOAPYSDR_USB_DEVICE) {
      value=getProperty("radio.adc[0].agc");
      if(value) adc[0].agc=atoi(value);
    }
#endif
    value=getProperty("radio.dac[0].antenna");
    if(value) dac[0].antenna=atoi(value);
    value=getProperty("radio.dac[0].gain");
    if(value) dac[0].gain=atof(value);

    if(receivers>1) {
      value=getProperty("radio.adc[1].filters");
      if(value) adc[1].filters=atoi(value);
      value=getProperty("radio.adc[1].hpf");
      if(value) adc[1].hpf=atoi(value);
      value=getProperty("radio.adc[1].lpf");
      if(value) adc[1].lpf=atoi(value);
      value=getProperty("radio.adc[1].antenna");
      if(value) adc[1].antenna=atoi(value);
      value=getProperty("radio.adc[1].dither");
      if(value) adc[1].dither=atoi(value);
      value=getProperty("radio.adc[1].random");
      if(value) adc[1].random=atoi(value);
      value=getProperty("radio.adc[1].preamp");
      if(value) adc[1].preamp=atoi(value);
      value=getProperty("radio.adc[1].attenuation");
      if(value) adc[1].attenuation=atoi(value);
      value=getProperty("radio.adc[1].enable_step_attenuation");
      if(value) adc[1].enable_step_attenuation=atoi(value);
      value=getProperty("radio.adc[1].gain");
      if(value) adc[1].gain=atof(value);
      value=getProperty("radio.adc[1].min_gain");
      if(value) adc[1].min_gain=atof(value);
      value=getProperty("radio.adc[1].max_gain");
      if(value) adc[1].max_gain=atof(value);


#ifdef  SOAPYSDR
      if(device==SOAPYSDR_USB_DEVICE) {
        value=getProperty("radio.adc[1].agc");
        if(value) adc[1].agc=atoi(value);
      }
#endif
      value=getProperty("radio.dac[1].antenna");
      if(value) dac[1].antenna=atoi(value);
      value=getProperty("radio.dac[1].gain");
      if(value) dac[1].gain=atof(value);

    }

#ifdef MIDI
    midi_restore_state();
#endif

    value=getProperty("radio.display_sequence_errors");
    if(value!=NULL) display_sequence_errors=atoi(value);


	
#ifdef CLIENT_SERVER
  }
#endif

#ifdef CLIENT_SERVER
  value=getProperty("radio.hpsdr_server");
  if(value!=NULL) hpsdr_server=atoi(value);
  value=getProperty("radio.hpsdr_server.listen_port");
  if(value!=NULL) listen_port=atoi(value);
#endif

  g_mutex_unlock(&property_mutex);
}

void radioSaveState() {
  int i;
  char value[80];
  char name[32];


g_print("radioSaveState: %s\n",property_path);
  
  g_mutex_lock(&property_mutex);
  clearProperties();
#ifdef GPIO
  if(controller!=NO_CONTROLLER) {
    gpio_save_actions();
  }
#endif
  sprintf(value,"%d",receivers);
  setProperty("receivers",value);
  for(i=0;i<receivers;i++) {
    receiver_save_state(receiver[i]);
  }

  sprintf(value,"%d",display_filled);
  setProperty("display_filled",value);
  sprintf(value,"%d",display_gradient);
  setProperty("display_gradient",value);
  sprintf(value,"%d",display_zoompan);
  setProperty("display_zoompan",value);
  sprintf(value,"%d",display_sliders);
  setProperty("display_sliders",value);
  sprintf(value,"%d",display_toolbar);
  setProperty("display_toolbar",value);

  if(can_transmit) {
#ifdef PURESIGNAL
    // The only variables of interest in this receiver are
    // the alex_antenna an the adc
    receiver_save_state(receiver[PS_RX_FEEDBACK]);
#endif
    transmitter_save_state(transmitter);
  }

#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
    sprintf(value,"%d",diversity_enabled);
    setProperty("diversity_enabled",value);
    sprintf(value,"%f",div_gain);
    setProperty("diversity_gain",value);
    sprintf(value,"%f",div_phase);
    setProperty("diversity_phase",value);
    sprintf(value,"%f",div_cos);
    setProperty("diversity_cos",value);
    sprintf(value,"%f",div_sin);
    setProperty("diversity_sin",value);
    sprintf(value,"%d",new_pa_board);
    setProperty("new_pa_board",value);
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
    sprintf(value,"%d",pa_enabled);
    setProperty("pa_enabled",value);
    sprintf(value,"%d",pa_power);
    setProperty("pa_power",value);
    for(i=0;i<11;i++) {
      sprintf(name,"pa_trim[%d]",i);
      sprintf(value,"%d",pa_trim[i]);
      setProperty(name,value);
    }

    sprintf(value,"%lld",step);
    setProperty("step",value);
    sprintf(value,"%d",cw_is_on_vfo_freq);
    setProperty("cw_is_on_vfo_freq",value);
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
//    sprintf(value,"%d",local_audio);
//    setProperty("local_audio",value);
//    sprintf(value,"%d",n_selected_output_device);
//    setProperty("n_selected_output_device",value);
//    sprintf(value,"%d",local_microphone);
//    setProperty("local_microphone",value);

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

    sprintf(value,"%d",adc_attenuation[0]);
    setProperty("adc_0_attenuation",value);
    sprintf(value,"%d",adc_attenuation[1]);
    setProperty("adc_1_attenuation",value);
	
    sprintf(value,"%d",rx_gain_calibration);
    setProperty("rx_gain_calibration",value);

    sprintf(value,"%d", adc[0].filters);
    setProperty("radio.adc[0].filters",value);
    sprintf(value,"%d", adc[0].hpf);
    setProperty("radio.adc[0].hpf",value);
    sprintf(value,"%d", adc[0].lpf);
    setProperty("radio.adc[0].lpf",value);
    sprintf(value,"%d", adc[0].antenna);
    setProperty("radio.adc[0].antenna",value);
    sprintf(value,"%d", adc[0].dither);
    setProperty("radio.adc[0].dither",value);
    sprintf(value,"%d", adc[0].random);
    setProperty("radio.adc[0].random",value);
    sprintf(value,"%d", adc[0].preamp);
    setProperty("radio.adc[0].preamp",value);
    sprintf(value,"%d", adc[0].attenuation);
    setProperty("radio.adc[0].attenuation",value);
    sprintf(value,"%d", adc[0].enable_step_attenuation);
    setProperty("radio.adc[0].enable_step_attenuation",value);
    sprintf(value,"%f", adc[0].gain);
    setProperty("radio.adc[0].gain",value);
    sprintf(value,"%f", adc[0].min_gain);
    setProperty("radio.adc[0].min_gain",value);
    sprintf(value,"%f", adc[0].max_gain);
    setProperty("radio.adc[0].max_gain",value);

#ifdef  SOAPYSDR
    if(device==SOAPYSDR_USB_DEVICE) {
      sprintf(value,"%d", soapy_protocol_get_automatic_gain(receiver[0]));
      setProperty("radio.adc[0].agc",value);
    }
#endif

   sprintf(value,"%d", dac[0].antenna);
   setProperty("radio.dac[0].antenna",value);
   sprintf(value,"%f", dac[0].gain);
   setProperty("radio.dac[0].gain",value);

   if(receivers>1) {
      sprintf(value,"%d", adc[1].filters);
      setProperty("radio.adc[1].filters",value);
      sprintf(value,"%d", adc[1].hpf);
      setProperty("radio.adc[1].hpf",value);
      sprintf(value,"%d", adc[1].lpf);
      setProperty("radio.adc[1].lpf",value);
      sprintf(value,"%d", adc[1].antenna);
      setProperty("radio.adc[1].antenna",value);
      sprintf(value,"%d", adc[1].dither);
      setProperty("radio.adc[1].dither",value);
      sprintf(value,"%d", adc[1].random);
      setProperty("radio.adc[1].random",value);
      sprintf(value,"%d", adc[1].preamp);
      setProperty("radio.adc[1].preamp",value);
      sprintf(value,"%d", adc[1].attenuation);
      setProperty("radio.adc[1].attenuation",value);
      sprintf(value,"%d", adc[1].enable_step_attenuation);
      setProperty("radio.adc[1].enable_step_attenuation",value);
      sprintf(value,"%f", adc[1].gain);
      setProperty("radio.adc[1].gain",value);
      sprintf(value,"%f", adc[1].min_gain);
      setProperty("radio.adc[1].min_gain",value);
      sprintf(value,"%f", adc[1].max_gain);
      setProperty("radio.adc[1].max_gain",value);

#ifdef  SOAPYSDR
      if(device==SOAPYSDR_USB_DEVICE) {
        sprintf(value,"%d", soapy_protocol_get_automatic_gain(receiver[1]));
        setProperty("radio.adc[1].agc",value);
      }
#endif

      sprintf(value,"%d", dac[1].antenna);
      setProperty("radio.dac[1].antenna",value);
      sprintf(value,"%f", dac[1].gain);
      setProperty("radio.dac[1].gain",value);
    }

    sprintf(value,"%d",receivers);
    setProperty("receivers",value);
	
    sprintf(value,"%d",iqswap);
    setProperty("iqswap",value);
	
#ifdef CLIENT_SERVER
    sprintf(value,"%d",hpsdr_server);
    setProperty("radio.hpsdr_server",value);
    sprintf(value,"%d",listen_port);
    setProperty("radio.hpsdr_server.listen_port",value);
#endif

    vfo_save_state();
    modesettings_save_state();

    sprintf(value,"%d",duplex);
    setProperty("duplex",value);
    sprintf(value,"%d",split);
    setProperty("split",value);
    sprintf(value,"%d",sat_mode);
    setProperty("sat_mode",value);
    sprintf(value,"%d",mute_rx_while_transmitting);
    setProperty("mute_rx_while_transmitting",value);

    filterSaveState();
    bandSaveState();
    memSaveState();

    sprintf(value,"%d",rigctl_enable);
    setProperty("rigctl_enable",value);
    sprintf(value,"%d",rigctl_port_base);
    setProperty("rigctl_port_base",value);
    sprintf(value,"%d",serial_enable);
    setProperty("rigctl_serial_enable",value);
    sprintf(value,"%d",serial_baud_rate);
    setProperty("rigctl_serial_baud_rate",value);
    setProperty("rigctl_serial_port",ser_port);

    sprintf(value,"%d",display_sequence_errors);
    setProperty("radio.display_sequence_errors",value);
#ifdef CLIENT_SERVER
  }
#endif

#ifdef MIDI
  midi_save_state();
#endif

  saveProperties(property_path);
  g_mutex_unlock(&property_mutex);
}

void calculate_display_average(RECEIVER *rx) {
  double display_avb;
  int display_average;

  double t=0.001*display_average_time;
  display_avb = exp(-1.0 / ((double)rx->fps * t));
  display_average = max(2, (int)fmin(60, (double)rx->fps * t));
  SetDisplayAvBackmult(rx->id, 0, display_avb);
  SetDisplayNumAverage(rx->id, 0, display_average);
}

void set_filter_type(int filter_type) {
  int i;

  //g_print("set_filter_type: %d\n",filter_type);
  for(i=0;i<RECEIVERS;i++) {
    receiver[i]->low_latency=filter_type;
    RXASetMP(receiver[i]->id, filter_type);
  }
  transmitter->low_latency=filter_type;
  TXASetMP(transmitter->id, filter_type);
}

void set_filter_size(int filter_size) {
  int i;

  //g_print("set_filter_size: %d\n",filter_size);
  for(i=0;i<RECEIVERS;i++) {
    receiver[i]->fft_size=filter_size;
    RXASetNC(receiver[i]->id, filter_size);
  }
  transmitter->fft_size=filter_size;
  TXASetNC(transmitter->id, filter_size);
}

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

#ifdef CLIENT_SERVER
int remote_start(void *data) {
  char *server=(char *)data;
  sprintf(property_path,"%s@%s.props",radio->name,server);
  radio_is_remote=TRUE;
#ifdef GPIO
  switch(controller) {
    case CONTROLLER2_V1:
    case CONTROLLER2_V2:
      display_zoompan=1;
      display_sliders=0;
      display_toolbar=0;
      break;
    default:
      display_zoompan=1;
      display_sliders=1;
      display_toolbar=1;
      break;
  }
#else
  display_zoompan=1;
  display_sliders=1;
  display_toolbar=1;
#endif
  radioRestoreState();
  create_visual();
  if(transmitter->local_microphone) {
    if(audio_open_input()!=0) {
      g_print("audio_open_input failed\n");
      transmitter->local_microphone=0;
    }
  }
  for(int i=0;i<receivers;i++) {
    receiver_restore_state(receiver[i]);
    if(receiver[i]->local_audio) {
      if (audio_open_output(receiver[i]) < 0) receiver[i]->local_audio=0;
    }
  }
  reconfigure_radio();
  g_idle_add(ext_vfo_update,(gpointer)NULL);
  gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));
  for(int i=0;i<receivers;i++) {
    gint timer_id=gdk_threads_add_timeout_full(G_PRIORITY_DEFAULT_IDLE,100, start_spectrum, receiver[i], NULL);
  }
  start_vfo_timer();
  remote_started=TRUE;
  return 0;
}
#endif
