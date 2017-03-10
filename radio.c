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
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef RADIOBERRY
#include "radioberry.h"
#endif
#include "store.h"
#ifdef LIMESDR
#include "lime_protocol.h"
#endif
#ifdef FREEDV
#include "freedv.h"
#endif
#ifdef GPIO
#include "gpio.h"
#endif
#include "vfo.h"
#include "meter.h"
#include "rx_panadapter.h"
#include "tx_panadapter.h"
#include "waterfall.h"
#include "sliders.h"
#include "toolbar.h"

#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

#define DISPLAY_INCREMENT (display_height/32)
#define MENU_HEIGHT (DISPLAY_INCREMENT*2)
#define MENU_WIDTH ((display_width/32)*3)
#define VFO_HEIGHT (DISPLAY_INCREMENT*4)
#define VFO_WIDTH (display_width-METER_WIDTH-MENU_WIDTH)
#define METER_HEIGHT (DISPLAY_INCREMENT*4)
#define METER_WIDTH ((display_width/32)*8)
#define PANADAPTER_HEIGHT (DISPLAY_INCREMENT*8)
#define SLIDERS_HEIGHT (DISPLAY_INCREMENT*6)
#define TOOLBAR_HEIGHT (DISPLAY_INCREMENT*2)
#define WATERFALL_HEIGHT (display_height-(VFO_HEIGHT+PANADAPTER_HEIGHT+SLIDERS_HEIGHT+TOOLBAR_HEIGHT))
#ifdef PSK
#define PSK_WATERFALL_HEIGHT (DISPLAY_INCREMENT*6)
#define PSK_HEIGHT (display_height-(VFO_HEIGHT+PSK_WATERFALL_HEIGHT+SLIDERS_HEIGHT+TOOLBAR_HEIGHT))
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

#ifdef GPIO
static GtkWidget *encoders;
static cairo_surface_t *encoders_surface = NULL;
#endif

int echo=0;

static gint save_timer_id;

DISCOVERED *radio;

char property_path[128];
sem_t property_sem;

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
int tx_alc=1;
int tx_leveler=0;

double tone_level=0.0;

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
int waterfall_automatic=1;

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

double tune_drive=10;
double drive=50;

int drive_level=0;
int tune_drive_level=0;

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
int cw_active_level=1; // 0=active_low 1=active_high

int vfo_encoder_divisor=15;

int protocol;
int device;
int ozy_software_version;
int mercury_software_version;
int penelope_software_version;
int ptt;
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

int smeter=RXA_S_AV;
int alc=TXA_ALC_PK;

int local_audio=0;
int local_microphone=0;

int eer_pwm_min=100;
int eer_pwm_max=800;

int tx_filter_low=150;
int tx_filter_high=2850;

#ifdef FREEDV
char freedv_tx_text_data[64];
#endif

static int pre_tune_filter_low;
static int pre_tune_filter_high;

int enable_tx_equalizer=0;
int tx_equalizer[4]={0,0,0,0};

int enable_rx_equalizer=0;
int rx_equalizer[4]={0,0,0,0};

int deviation=2500;
int pre_emphasize=0;

int vox_setting=0;
int vox_enabled=0;
double vox_threshold=0.001;
double vox_gain=10.0;
double vox_hang=250.0;
int vox=0;

int diversity_enabled=0;
double i_rotate[2]={1.0,1.0};
double q_rotate[2]={0.0,0.0};

void reconfigure_radio() {
  int i;
  int y;
  int rx_height=display_height-VFO_HEIGHT-TOOLBAR_HEIGHT;
  if(display_sliders) {
    rx_height-=SLIDERS_HEIGHT;
  }
 
  y=VFO_HEIGHT;
  for(i=0;i<receivers;i++) {
    reconfigure_receiver(receiver[i],rx_height/receivers);
    gtk_fixed_move(GTK_FIXED(fixed),receiver[i]->panel,0,y);
    y+=rx_height/receivers;
  }

  if(display_sliders) {
    if(sliders==NULL) {
      sliders = sliders_init(display_width,SLIDERS_HEIGHT);
      gtk_fixed_put(GTK_FIXED(fixed),sliders,0,y);
    } else {
      gtk_fixed_move(GTK_FIXED(fixed),sliders,0,y);
    }
    gtk_widget_show_all(sliders);
    // force change of sliders for mic or linein
    g_idle_add(linein_changed,NULL);
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
  new_menu(top_window);
  return TRUE;
}

void start_radio() {
  int i;
  int x;
  int y;
fprintf(stderr,"start_radio: selected radio=%p device=%d\n",radio,radio->device);
  gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_WATCH));

  int rc;
  rc=sem_init(&property_sem, 0, 0);
  if(rc!=0) {
    fprintf(stderr,"start_radio: sem_init failed for property_sem: %d\n", rc);
    exit(-1);
  }
  sem_post(&property_sem);

  status_text("starting radio ...");
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

  radioRestoreState();

  y=0;

  fixed=gtk_fixed_new();
  gtk_container_remove(GTK_CONTAINER(top_window),grid);
  gtk_container_add(GTK_CONTAINER(top_window), fixed);

fprintf(stderr,"radio: vfo_init\n");
  vfo_panel = vfo_init(VFO_WIDTH,VFO_HEIGHT,top_window);
  gtk_fixed_put(GTK_FIXED(fixed),vfo_panel,0,y);

fprintf(stderr,"radio: meter_init\n");
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
  rx_height=rx_height/receivers;


fprintf(stderr,"Create %d receivers: height=%d\n",receivers,rx_height);
  for(i=0;i<MAX_RECEIVERS;i++) {
    receiver[i]=create_receiver(i, buffer_size, fft_size, display_width, updates_per_second, display_width, rx_height);
    g_object_ref((gpointer)receiver[i]->panel);
    if(i<receivers) {
      gtk_fixed_put(GTK_FIXED(fixed),receiver[i]->panel,0,y);
fprintf(stderr,"receiver %d: height=%d y=%d\n",receiver[i]->id,rx_height,y);
      set_displaying(receiver[i],1);
      y+=rx_height;
    } else {
      set_displaying(receiver[i],0);
    }
  }
  active_receiver=receiver[0];

  fprintf(stderr,"Create transmitter\n");
  transmitter=create_transmitter(CHANNEL_TX, buffer_size, fft_size, updates_per_second, display_width, tx_height);
  g_object_ref((gpointer)transmitter->panel);

  
  #ifdef GPIO
  if(gpio_init()<0) {
    fprintf(stderr,"GPIO failed to initialize\n");
  }
#ifdef LOCALCW
  // init local keyer if enabled
  else if (cw_keyer_internal == 0)
    keyer_update();
#endif
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
#ifdef RADIOBERRY
	case RADIOBERRY_PROTOCOL:
		radioberry_protocol_init(0,display_width);
		break;
#endif
  }



#ifdef I2C
  i2c_init();
#endif

  if(display_sliders) {
fprintf(stderr,"create sliders\n");
    sliders = sliders_init(display_width,SLIDERS_HEIGHT);
    gtk_fixed_put(GTK_FIXED(fixed),sliders,0,y);
    y+=SLIDERS_HEIGHT;
  }

  toolbar = toolbar_init(display_width,TOOLBAR_HEIGHT,top_window);
  gtk_fixed_put(GTK_FIXED(fixed),toolbar,0,y);
  y+=TOOLBAR_HEIGHT;

  gtk_widget_show_all (fixed);

  // force change of sliders for mic or linein
  g_idle_add(linein_changed,NULL);

  // save every 30 seconds
fprintf(stderr,"start save timer\n");
  save_timer_id=gdk_threads_add_timeout(30000, save_cb, NULL);

#ifdef PSK
  if(vfo[active_receiver->id].mode==modePSK) {
    show_psk();
  } else {
    show_waterfall();
  }
#endif

  launch_rigctl();

  calcDriveLevel();
  calcTuneDriveLevel();

  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }

  g_idle_add(vfo_update,(gpointer)NULL);

fprintf(stderr,"set cursor\n");
  gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));

for(i=0;i<MAX_VFOS;i++) {
  fprintf(stderr,"start_radio: vfo %d band=%d bandstack=%d frequency=%lld mode=%d filter=%d rit=%lld lo=%lld offset=%lld\n",
    i,
    vfo[i].band,
    vfo[i].bandstack,
    vfo[i].frequency,
    vfo[i].mode,
    vfo[i].filter,
    vfo[i].rit,
    vfo[i].lo,
    vfo[i].offset);
}
}


void radio_change_receivers(int r) {
  switch(r) {
    case 1:
      if(receivers==2) {
        set_displaying(receiver[1],0);
        gtk_container_remove(GTK_CONTAINER(fixed),receiver[1]->panel);
      }
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
}

void radio_change_sample_rate(int rate) {
  int i;
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      old_protocol_stop();
      for(i=0;i<receivers;i++) {
        receiver_change_sample_rate(receiver[i],rate);
      }
      old_protocol_set_mic_sample_rate(rate);
      old_protocol_run();
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
  int y=VFO_HEIGHT;

fprintf(stderr,"rxtx: state=%d\n",state);

  if(state) {
    // switch to tx
    for(i=0;i<receivers;i++) {
      SetChannelState(receiver[i]->id,0,i==(receivers-1));
      set_displaying(receiver[i],0);
      if(protocol==NEW_PROTOCOL) {
        schedule_high_priority();
      }
      gtk_container_remove(GTK_CONTAINER(fixed),receiver[i]->panel);
    }
    gtk_fixed_put(GTK_FIXED(fixed),transmitter->panel,0,y);
    SetChannelState(transmitter->id,1,0);
    tx_set_displaying(transmitter,1);
#ifdef FREEDV
    if(active_receiver->mode==modeFREEDV) {
      freedv_reset_tx_text_index();
    }
#endif
  } else {
    SetChannelState(transmitter->id,0,1);
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
    tx_set_displaying(transmitter,0);
    gtk_container_remove(GTK_CONTAINER(fixed),transmitter->panel);
    int rx_height=display_height-VFO_HEIGHT-TOOLBAR_HEIGHT;
    if(display_sliders) {
      rx_height-=SLIDERS_HEIGHT;
    }
    for(i=0;i<receivers;i++) {
      SetChannelState(receiver[i]->id,1,0);
      set_displaying(receiver[i],1);
      gtk_fixed_put(GTK_FIXED(fixed),receiver[i]->panel,0,y);
      y+=(rx_height/receivers);
    }
  }

  gtk_widget_show_all(fixed);
  g_idle_add(linein_changed,NULL);
}

void setMox(int state) {
fprintf(stderr,"setMox: %d\n",state);
  if(mox!=state) {
    mox=state;
    if(vox_enabled && vox) {
      vox_cancel();
    } else {
      rxtx(state);
    }
  }
}

int getMox() {
    return mox;
}

void setVox(int state) {
  if(vox!=state && !tune) {
    vox=state;
    rxtx(state);
  }
  g_idle_add(vfo_update,(gpointer)NULL);
}

int vox_changed(void *data) {
  setVox((int)data);
  return 0;
}


void setTune(int state) {
  int i;

  if(tune!=state) {
    tune=state;
    if(vox_enabled && vox) {
      vox_cancel();
    }
    if(tune) {
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
    if(tune) {
      for(i=0;i<receivers;i++) {
        SetChannelState(receiver[i]->id,0,i==(receivers-1));
        set_displaying(receiver[i],0);
        if(protocol==NEW_PROTOCOL) {
          schedule_high_priority();
        }
      }

      int mode=vfo[VFO_A].mode;;
      if(split) {
        mode=vfo[VFO_B].mode;
      }
      double freq=(double)cw_keyer_sidetone_frequency;
      
      pre_tune_filter_low=transmitter->filter_low;
      pre_tune_filter_high=transmitter->filter_high;

      switch(mode) {
        case modeUSB:
        case modeCWU:
        case modeDIGU:
          SetTXAPostGenToneFreq(transmitter->id,(double)cw_keyer_sidetone_frequency);
          transmitter->filter_low=cw_keyer_sidetone_frequency-100;
          transmitter->filter_high=cw_keyer_sidetone_frequency+100;
          freq=(double)(cw_keyer_sidetone_frequency+100);
          break;
        case modeLSB:
        case modeCWL:
        case modeDIGL:
          SetTXAPostGenToneFreq(transmitter->id,-(double)cw_keyer_sidetone_frequency);
          transmitter->filter_low=-cw_keyer_sidetone_frequency-100;
          transmitter->filter_high=-cw_keyer_sidetone_frequency+100;
          freq=(double)(-cw_keyer_sidetone_frequency-100);
          break;
        case modeDSB:
          SetTXAPostGenToneFreq(transmitter->id,(double)cw_keyer_sidetone_frequency);
          transmitter->filter_low=cw_keyer_sidetone_frequency-100;
          transmitter->filter_high=cw_keyer_sidetone_frequency+100;
          freq=(double)(cw_keyer_sidetone_frequency+100);
          break;
        case modeAM:
        case modeSAM:
        case modeFMN:
          SetTXAPostGenToneFreq(transmitter->id,(double)cw_keyer_sidetone_frequency);
          transmitter->filter_low=cw_keyer_sidetone_frequency-100;
          transmitter->filter_high=cw_keyer_sidetone_frequency+100;
          freq=(double)(cw_keyer_sidetone_frequency+100);
          break;
      }

      SetTXABandpassFreqs(transmitter->id,transmitter->filter_low,transmitter->filter_high);

      
      SetTXAMode(transmitter->id,modeDIGU);
      SetTXAPostGenMode(transmitter->id,0);
      SetTXAPostGenToneMag(transmitter->id,0.99999);
      SetTXAPostGenRun(transmitter->id,1);
    } else {
      SetTXAPostGenRun(transmitter->id,0);
      SetTXAMode(transmitter->id,transmitter->mode);
      transmitter->filter_low=pre_tune_filter_low;
      transmitter->filter_high=pre_tune_filter_high;
      SetTXABandpassFreqs(transmitter->id,transmitter->filter_low,transmitter->filter_high);
    }
    rxtx(tune);
  }
}

int getTune() {
  return tune;
}

int isTransmitting() {
  return ptt || mox || vox || tune;
}

void setFrequency(long long f) {
  BAND *band=band_get_current_band();
  BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
  int v=active_receiver->id;

  switch(protocol) {
    case NEW_PROTOCOL:
    case ORIGINAL_PROTOCOL:
#ifdef RADIOBERRY
	case RADIOBERRY_PROTOCOL:
#endif
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
#ifdef RADIOBERRY
	case RADIOBERRY_PROTOCOL:
#endif
      schedule_frequency_changed();
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
    return drive;
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
  return level;
}

void calcDriveLevel() {
    drive_level=calcLevel(drive);
    if(mox && protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
}

void setDrive(double value) {
    drive=value;
    calcDriveLevel();
}

double getTuneDrive() {
    return tune_drive;
}

void calcTuneDriveLevel() {
    tune_drive_level=calcLevel(tune_drive);
    if(tune  && protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
}

void setTuneDrive(double value) {
    tune_drive=value;
    calcTuneDriveLevel();
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

int get_attenuation() {
    return active_receiver->attenuation;
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
    sem_wait(&property_sem);
    loadProperties(property_path);

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
    value=getProperty("waterfall_automatic");
    if(value) waterfall_automatic=atoi(value);
//    value=getProperty("volume");
//    if(value) volume=atof(value);
    value=getProperty("drive");
    if(value) drive=atof(value);
    value=getProperty("tune_drive");
    if(value) tune_drive=atof(value);
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
    value=getProperty("cw_active_level");
    if(value) cw_active_level=atoi(value);
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
#ifdef FREEDV
    strcpy(freedv_tx_text_data,"NO TEXT DATA");
    value=getProperty("freedv_tx_text_data");
    if(value) strcpy(freedv_tx_text_data,value);
#endif
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
    value=getProperty("deviation");
    if(value) deviation=atoi(value);
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

    sem_post(&property_sem);
}

void radioSaveState() {
    int i;
    char value[80];

    sem_wait(&property_sem);
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
/*
    sprintf(value,"%d",apollo_tuner);
    setProperty("apollo_tuner",value);
    sprintf(value,"%d",pa);
    setProperty("pa",value);
*/
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
/*
    sprintf(value,"%d",display_toolbar);
    setProperty("display_toolbar",value);
*/
    sprintf(value,"%d",waterfall_high);
    setProperty("waterfall_high",value);
    sprintf(value,"%d",waterfall_low);
    setProperty("waterfall_low",value);
    sprintf(value,"%d",waterfall_automatic);
    setProperty("waterfall_automatic",value);
//    sprintf(value,"%f",volume);
//    setProperty("volume",value);
    sprintf(value,"%f",mic_gain);
    setProperty("mic_gain",value);
    sprintf(value,"%f",drive);
    setProperty("drive",value);
    sprintf(value,"%f",tune_drive);
    setProperty("tune_drive",value);
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
    sprintf(value,"%d",cw_active_level);
    setProperty("cw_active_level",value);
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
#ifdef FREEDV
    if(strlen(freedv_tx_text_data)>0) {
      setProperty("freedv_tx_text_data",freedv_tx_text_data);
    }
#endif
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
//    sprintf(value,"%d",n_selected_input_device);
//    setProperty("n_selected_input_device",value);

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
    sprintf(value,"%d",deviation);
    setProperty("deviation",value);
    sprintf(value,"%d",pre_emphasize);
    setProperty("pre_emphasize",value);

    sprintf(value,"%d",vox_enabled);
    setProperty("vox_enabled",value);
    sprintf(value,"%f",vox_threshold);
    setProperty("vox_threshold",value);
/*
    sprintf(value,"%f",vox_gain);
    setProperty("vox_gain",value);
*/
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

#ifdef GPIO
    sprintf(value,"%d",e1_encoder_action);
    setProperty("e1_encoder_action",value);
    sprintf(value,"%d",e2_encoder_action);
    setProperty("e2_encoder_action",value);
    sprintf(value,"%d",e3_encoder_action);
    setProperty("e3_encoder_action",value);
#endif


    vfo_save_state();
    sprintf(value,"%d",receivers);
    setProperty("receivers",value);
    for(i=0;i<receivers;i++) {
      receiver_save_state(receiver[i]);
    }
    transmitter_save_state(transmitter);

    filterSaveState();
    bandSaveState();
    memSaveState();
    saveProperties(property_path);
    sem_post(&property_sem);
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

  fprintf(stderr,"set_filter_type: %d\n",filter_type);
  for(i=0;i<RECEIVERS;i++) {
    receiver[i]->low_latency=filter_type;
    RXASetMP(receiver[i]->id, filter_type);
  }
  transmitter->low_latency=filter_type;
  TXASetMP(transmitter->id, filter_type);
}

void set_filter_size(int filter_size) {
  int i;

  fprintf(stderr,"set_filter_size: %d\n",filter_size);
  for(i=0;i<RECEIVERS;i++) {
    receiver[i]->fft_size=filter_size;
    RXASetNC(receiver[i]->id, filter_size);
  }
  transmitter->fft_size=filter_size;
  TXASetNC(transmitter->id, filter_size);
}
