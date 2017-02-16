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
#include <gdk/gdk.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "discovered.h"
#include "main.h"
#include "agc.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "frequency.h"
#include "new_protocol.h"
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "channel.h"
#include "toolbar.h"
#include "wdsp.h"
#include "new_menu.h"
#include "rigctl.h"

static GtkWidget *parent_window;
static int my_width;
static int my_height;

static GtkWidget *vfo_panel;
static cairo_surface_t *vfo_surface = NULL;

int steps[]={1,10,25,50,100,250,500,1000,2500,5000,6250,9000,10000,12500,15000,20000,25000,30000,50000,100000,0};
char *step_labels[]={"1Hz","10Hz","25Hz","50Hz","100Hz","250Hz","500Hz","1kHz","2.5kHz","5kHz","6.25kHz","9kHz","10kHz","12.5kHz","15kHz","20kHz","25kHz","30kHz","50kHz","100kHz",0};

static GtkWidget* menu=NULL;
static GtkWidget* band_menu=NULL;


static void vfo_save_bandstack() {
  BANDSTACK *bandstack=bandstack_get_bandstack(vfo[0].band);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[0].bandstack];
  entry->frequency=vfo[0].frequency;
  entry->mode=vfo[0].mode;
  entry->filter=vfo[0].filter;
}

void vfo_save_state() {
  int i;
  char name[80];
  char value[80];

  vfo_save_bandstack();

  for(i=0;i<MAX_VFOS;i++) {
    sprintf(name,"vfo.%d.band",i);
    sprintf(value,"%d",vfo[i].band);
    setProperty(name,value);
    sprintf(name,"vfo.%d.frequency",i);
    sprintf(value,"%lld",vfo[i].frequency);
    setProperty(name,value);
    sprintf(name,"vfo.%d.ctun",i);
    sprintf(value,"%d",vfo[i].ctun);
    setProperty(name,value);
    sprintf(name,"vfo.%d.rit_enabled",i);
    sprintf(value,"%d",vfo[i].rit_enabled);
    setProperty(name,value);
    sprintf(name,"vfo.%d.rit",i);
    sprintf(value,"%lld",vfo[i].rit);
    setProperty(name,value);
    sprintf(name,"vfo.%d.lo",i);
    sprintf(value,"%lld",vfo[i].lo);
    setProperty(name,value);
    sprintf(name,"vfo.%d.ctun_frequency",i);
    sprintf(value,"%lld",vfo[i].ctun_frequency);
    setProperty(name,value);
    sprintf(name,"vfo.%d.offset",i);
    sprintf(value,"%lld",vfo[i].offset);
    setProperty(name,value);
    sprintf(name,"vfo.%d.mode",i);
    sprintf(value,"%d",vfo[i].mode);
    setProperty(name,value);
    sprintf(name,"vfo.%d.filter",i);
    sprintf(value,"%d",vfo[i].filter);
    setProperty(name,value);
  }
}

void vfo_restore_state() {
  int i;
  char name[80];
  char *value;

  for(i=0;i<MAX_VFOS;i++) {

    vfo[i].band=band20;
    vfo[i].bandstack=0;
    vfo[i].frequency=14010000;
    vfo[i].mode=modeCWU;
    vfo[i].filter=6;
    vfo[i].lo=0;
    vfo[i].offset=0;
    vfo[i].rit_enabled=0;
    vfo[i].rit=0;
    vfo[i].ctun=0;

    sprintf(name,"vfo.%d.band",i);
    value=getProperty(name);
    if(value) vfo[i].band=atoi(value);
    sprintf(name,"vfo.%d.frequency",i);
    value=getProperty(name);
    if(value) vfo[i].frequency=atoll(value);
    sprintf(name,"vfo.%d.ctun",i);
    value=getProperty(name);
    if(value) vfo[i].ctun=atoi(value);
    sprintf(name,"vfo.%d.ctun_frequency",i);
    value=getProperty(name);
    if(value) vfo[i].ctun_frequency=atoll(value);
    sprintf(name,"vfo.%d.rit",i);
    value=getProperty(name);
    if(value) vfo[i].rit=atoll(value);
    sprintf(name,"vfo.%d.rit_enabled",i);
    value=getProperty(name);
    if(value) vfo[i].rit_enabled=atoi(value);
    sprintf(name,"vfo.%d.lo",i);
    value=getProperty(name);
    if(value) vfo[i].lo=atoll(value);
    sprintf(name,"vfo.%d.offset",i);
    value=getProperty(name);
    if(value) vfo[i].offset=atoll(value);
    sprintf(name,"vfo.%d.mode",i);
    value=getProperty(name);
    if(value) vfo[i].mode=atoi(value);
    sprintf(name,"vfo.%d.filter",i);
    value=getProperty(name);
    if(value) vfo[i].filter=atoi(value);

  }
}

void vfo_band_changed(int b) {
  BANDSTACK *bandstack;

  int id=active_receiver->id;

  if(id==0) {
    vfo_save_bandstack();
  }
  if(b==vfo[id].band) {
    // same band selected - step to the next band stack
    bandstack=bandstack_get_bandstack(b);
    vfo[id].bandstack++;
    if(vfo[id].bandstack>=bandstack->entries) {
      vfo[id].bandstack=0;
    }
  } else {
    // new band - get band stack entry
    bandstack=bandstack_get_bandstack(b);
    vfo[id].bandstack=bandstack->current_entry;
  }

  BAND *band=band_get_band(b);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[id].bandstack];
  vfo[id].band=b;
  vfo[id].frequency=entry->frequency;
  vfo[id].mode=entry->mode;
  vfo[id].filter=entry->filter;
  vfo[id].lo=band->frequencyLO;

  switch(id) {
    case 0:
      bandstack->current_entry=vfo[id].bandstack;
      receiver_vfo_changed(receiver[id]);
      BAND *band=band_get_band(vfo[id].band);
      set_alex_rx_antenna(band->alexRxAntenna);
      set_alex_tx_antenna(band->alexTxAntenna);
      set_alex_attenuation(band->alexAttenuation);
      receiver_vfo_changed(receiver[0]);
      break;
   case 1:
      if(receivers==2) {
        receiver_vfo_changed(receiver[1]);
      }
      break;
  }

  if(split) {
    tx_set_mode(transmitter,vfo[VFO_B].mode);
  } else {
    tx_set_mode(transmitter,vfo[VFO_A].mode);
  }
  calcDriveLevel();
  calcTuneDriveLevel();
  vfo_update(NULL);
}

void vfo_bandstack_changed(int b) {
  int id=active_receiver->id;
  if(id==0) {
    vfo_save_bandstack();
  }
  vfo[id].bandstack=b;

  BANDSTACK *bandstack=bandstack_get_bandstack(vfo[id].band);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[id].bandstack];
  vfo[id].frequency=entry->frequency;
  vfo[id].mode=entry->mode;
  vfo[id].filter=entry->filter;

  switch(id) {
    case 0:
      bandstack->current_entry=vfo[id].bandstack;
      receiver_vfo_changed(receiver[id]);
      BAND *band=band_get_band(vfo[id].band);
      set_alex_rx_antenna(band->alexRxAntenna);
      set_alex_tx_antenna(band->alexTxAntenna);
      set_alex_attenuation(band->alexAttenuation);
      receiver_vfo_changed(receiver[0]);
      break;
   case 1:
      if(receivers==2) {
        receiver_vfo_changed(receiver[1]);
      }
      break;
  }

  if(split) {
    tx_set_mode(transmitter,vfo[VFO_B].mode);
  } else {
    tx_set_mode(transmitter,vfo[VFO_A].mode);
  }
  calcDriveLevel();
  calcTuneDriveLevel();
  vfo_update(NULL);

}

void vfo_mode_changed(int m) {
  int id=active_receiver->id;
  vfo[id].mode=m;
  switch(id) {
    case 0:
      receiver_mode_changed(receiver[0]);
      receiver_filter_changed(receiver[0]);
      break;
    case 1:
      if(receivers==2) {
        receiver_mode_changed(receiver[1]);
        receiver_filter_changed(receiver[1]);
      }
      break;
  }
  if(split) {
    tx_set_mode(transmitter,vfo[VFO_B].mode);
  } else {
    tx_set_mode(transmitter,vfo[VFO_A].mode);
  }

  vfo_update(NULL);
}

void vfo_filter_changed(int f) {
  int id=active_receiver->id;
  vfo[id].filter=f;
  switch(id) {
    case 0:
      receiver_filter_changed(receiver[0]);
      break;
    case 1:
      if(receivers==2) {
        receiver_filter_changed(receiver[1]);
      }
      break;
  }

  vfo_update(NULL);
}

void vfo_a_to_b() {
  vfo[VFO_B].band=vfo[VFO_A].band;
  vfo[VFO_B].bandstack=vfo[VFO_A].bandstack;
  vfo[VFO_B].frequency=vfo[VFO_A].frequency;
  vfo[VFO_B].mode=vfo[VFO_A].mode;
  vfo[VFO_B].filter=vfo[VFO_A].filter;
  vfo[VFO_B].filter=vfo[VFO_A].filter;
  vfo[VFO_B].lo=vfo[VFO_A].lo;
  vfo[VFO_B].offset=vfo[VFO_A].offset;
  vfo[VFO_B].rit_enabled=vfo[VFO_A].rit_enabled;
  vfo[VFO_B].rit=vfo[VFO_A].rit;

  if(receivers==2) {
    receiver_vfo_changed(receiver[1]);
  }
  if(split) {
    tx_set_mode(transmitter,vfo[VFO_B].mode);
  }
  vfo_update(NULL);
}

void vfo_b_to_a() {
  vfo[VFO_A].band=vfo[VFO_B].band;
  vfo[VFO_A].bandstack=vfo[VFO_B].bandstack;
  vfo[VFO_A].frequency=vfo[VFO_B].frequency;
  vfo[VFO_A].mode=vfo[VFO_B].mode;
  vfo[VFO_A].filter=vfo[VFO_B].filter;
  vfo[VFO_A].lo=vfo[VFO_B].lo;
  vfo[VFO_A].offset=vfo[VFO_B].offset;
  vfo[VFO_A].rit_enabled=vfo[VFO_B].rit_enabled;
  vfo[VFO_A].rit=vfo[VFO_B].rit;
  receiver_vfo_changed(receiver[0]);
  if(!split) {
    tx_set_mode(transmitter,vfo[VFO_B].mode);
  }
  vfo_update(NULL);
}

void vfo_a_swap_b() {
  int temp_band;
  int temp_bandstack;
  long long temp_frequency;
  int temp_mode;
  int temp_filter;
  int temp_lo;
  int temp_offset;
  int temp_rit_enabled;
  int temp_rit;

  temp_band=vfo[VFO_A].band;
  temp_bandstack=vfo[VFO_A].bandstack;
  temp_frequency=vfo[VFO_A].frequency;
  temp_mode=vfo[VFO_A].mode;
  temp_filter=vfo[VFO_A].filter;
  temp_lo=vfo[VFO_A].lo;
  temp_offset=vfo[VFO_A].offset;
  temp_rit_enabled=vfo[VFO_A].rit_enabled;
  temp_rit=vfo[VFO_A].rit;

  vfo[VFO_A].band=vfo[VFO_B].band;
  vfo[VFO_A].bandstack=vfo[VFO_B].bandstack;
  vfo[VFO_A].frequency=vfo[VFO_B].frequency;
  vfo[VFO_A].mode=vfo[VFO_B].mode;
  vfo[VFO_A].filter=vfo[VFO_B].filter;
  vfo[VFO_A].lo=vfo[VFO_B].lo;
  vfo[VFO_A].offset=vfo[VFO_B].offset;
  vfo[VFO_A].rit_enabled=vfo[VFO_B].rit_enabled;
  vfo[VFO_A].rit=vfo[VFO_B].rit;

  vfo[VFO_B].band=temp_band;
  vfo[VFO_B].bandstack=temp_bandstack;
  vfo[VFO_B].frequency=temp_frequency;
  vfo[VFO_B].mode=temp_mode;
  vfo[VFO_B].filter=temp_filter;
  vfo[VFO_B].lo=temp_lo;
  vfo[VFO_B].offset=temp_offset;
  vfo[VFO_B].rit_enabled=temp_rit_enabled;
  vfo[VFO_B].rit=temp_rit;

  receiver_vfo_changed(receiver[0]);
  if(receivers==2) {
    receiver_vfo_changed(receiver[1]);
  }
  if(split) {
    tx_set_mode(transmitter,vfo[VFO_B].mode);
  } else {
    tx_set_mode(transmitter,vfo[VFO_A].mode);
  }
  vfo_update(NULL);
}

void vfo_step(int steps) {
  int id=active_receiver->id;
  if(!locked) {
    if(vfo[id].ctun) {
      vfo[id].ctun_frequency=vfo[id].ctun_frequency+(steps*step);
    } else {
      vfo[id].frequency=vfo[id].frequency+(steps*step);
    }
    receiver_frequency_changed(active_receiver);
#ifdef INCLUDED
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    setFrequency(active_receiver->frequency+(steps*step));
#endif
    vfo_update(NULL);
  }
}

void vfo_move(long long hz) {
  int id=active_receiver->id;
  if(!locked) {
    switch(protocol) {
#ifdef LIMESDR
      case LIMESDR_PROTOCOL:
        break;
#endif
      default:
        if(vfo[id].ctun) {
          vfo[id].ctun_frequency=((vfo[id].ctun_frequency-hz)/step)*step;
        } else {
          vfo[id].frequency=((vfo[id].frequency+hz)/step)*step;
        }
        break;
    }
    receiver_frequency_changed(active_receiver);
    vfo_update(NULL);
  }
}

void vfo_move_to(long long hz) {
  int id=active_receiver->id;
  if(!locked) {
    switch(protocol) {
#ifdef LIMESDR
      case LIMESDR_PROTOCOL:
        break;
#endif
      default:
        if(vfo[id].ctun) {
          vfo[id].ctun_frequency=(vfo[id].frequency+hz)/step*step;
          if(vfo[id].mode==modeCWL) {
            vfo[id].ctun_frequency+=cw_keyer_sidetone_frequency;
          } else if(vfo[id].mode==modeCWU) {
            vfo[id].ctun_frequency-=cw_keyer_sidetone_frequency;
          }
        } else {
          vfo[id].frequency=(vfo[id].frequency+hz)/step*step;
          if(vfo[id].mode==modeCWL) {
            vfo[id].frequency+=cw_keyer_sidetone_frequency;
          } else if(vfo[id].mode==modeCWU) {
            vfo[id].frequency-=cw_keyer_sidetone_frequency;
          }
        }
        break;
    }
    receiver_vfo_changed(active_receiver);

#ifdef INCLUDED

    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();

#ifdef LIMESDR
    if(protocol==LIMESDR_PROTOCOL) {
      setFrequency((entry->frequency+active_receiver->dds_offset-hz)/step*step);
    } else {
#endif
      if(vfo[id].ctun) {
        setFrequency((active_receiver->frequency+hz)/step*step);
      } else {
        long long f=(active_receiver->frequency+active_receiver->dds_offset+hz)/step*step;
        if(vfo[active_receiver->id].mode==modeCWL) {
          f+=cw_keyer_sidetone_frequency;
        } else if(vfo[active_receiver->id].mode==modeCWU) {
          f-=cw_keyer_sidetone_frequency;
        }
        setFrequency(f);
      }
#ifdef LIMESDR
    }
#endif
#endif
    vfo_update(NULL);
  }
}

static gboolean
vfo_scroll_event_cb (GtkWidget      *widget,
               GdkEventScroll *event,
               gpointer        data)
{
  int i;
  if(event->direction==GDK_SCROLL_UP) {
    vfo_move(step);
  } else {
    vfo_move(-step);
  }
}


static gboolean vfo_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  if (vfo_surface)
    cairo_surface_destroy (vfo_surface);

  vfo_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));

  /* Initialize the surface to black */
  cairo_t *cr;
  cr = cairo_create (vfo_surface);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);
  cairo_destroy(cr);
  g_idle_add(vfo_update,NULL);
  return TRUE;
}

static gboolean vfo_draw_cb (GtkWidget *widget,
 cairo_t   *cr,
 gpointer   data)
{
  cairo_set_source_surface (cr, vfo_surface, 0, 0);
  cairo_paint (cr);
  return TRUE;
}

int vfo_update(void *data) {
    
    int id=active_receiver->id;
    FILTER* band_filters=filters[vfo[id].mode];
    FILTER* band_filter=&band_filters[vfo[id].filter];
    if(vfo_surface) {
        char temp_text[32];
        cairo_t *cr;
        cr = cairo_create (vfo_surface);
        cairo_set_source_rgb (cr, 0, 0, 0);
        cairo_paint (cr);

        cairo_select_font_face(cr, "FreeMono",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);

        char version[16];
        char text[128];
        if(radio->protocol==ORIGINAL_PROTOCOL) {
          switch(radio->device) {
#ifdef USBOZY
            case DEVICE_OZY:
              strcpy(version,"");
              break;
#endif
            default:
              sprintf(version,"%d.%d",
                  radio->software_version/10,
                  radio->software_version%10);
              break;
          }
        } else {
            sprintf(version,"%d.%d",
                radio->software_version/10,
                radio->software_version%10);
        }

        switch(radio->protocol) {
            case ORIGINAL_PROTOCOL:
              switch(radio->device) {
#ifdef USBOZY
                case DEVICE_OZY:
                  sprintf(text,"%s", radio->name);
                  break;
#endif
                default:
                  sprintf(text,"%s %s %s",
                    radio->name,
                    version,
                    inet_ntoa(radio->info.network.address.sin_addr));
                  break;
              }
              break;
            case NEW_PROTOCOL:
              sprintf(text,"%s %s %s",
                    radio->name,
                    version,
                    inet_ntoa(radio->info.network.address.sin_addr));
              break;
#ifdef LIMESDR
            case LIMESDR_PROTOCOL:
              sprintf(text,"%s", radio->name);
              break;
#endif
        }
        cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, 5, 15);  
        cairo_show_text(cr, text);

        //long long af=active_receiver->frequency+active_receiver->dds_offset;
        long long af=vfo[0].frequency+vfo[0].offset;
        sprintf(temp_text,"VFO A: %0lld.%06lld",af/(long long)1000000,af%(long long)1000000);
        if(isTransmitting() && !split) {
            cairo_set_source_rgb(cr, 1, 0, 0);
        } else {
            if(active_receiver->id==0) {
              cairo_set_source_rgb(cr, 0, 1, 0);
            } else {
              cairo_set_source_rgb(cr, 0, 0.65, 0);
            }
        }
        cairo_move_to(cr, 5, 38);  
        cairo_set_font_size(cr, 22); 
        cairo_show_text(cr, temp_text);

        //long long bf=frequencyB;
        long long bf=vfo[1].frequency+vfo[1].offset;
        sprintf(temp_text,"VFO B: %0lld.%06lld",bf/(long long)1000000,bf%(long long)1000000);
        if(isTransmitting() && split) {
            cairo_set_source_rgb(cr, 1, 0, 0);
        } else {
            if(active_receiver->id==1) {
              cairo_set_source_rgb(cr, 0, 1, 0);
            } else {
              cairo_set_source_rgb(cr, 0, 0.65, 0);
            }
        }
        cairo_move_to(cr, 260, 38);  
        cairo_show_text(cr, temp_text);

        cairo_set_font_size(cr, 12);

        if(vfo[id].rit_enabled==0) {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        } else {
            cairo_set_source_rgb(cr, 1, 1, 0);
        }
        sprintf(temp_text,"RIT: %d Hz",vfo[id].rit);
        cairo_move_to(cr, 5, 50);  
        cairo_set_font_size(cr, 12);
        cairo_show_text(cr, temp_text);

        cairo_move_to(cr, 190, 15);  
        if(vox_enabled) {
          cairo_set_source_rgb(cr, 1, 0, 0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "VOX");
        
        cairo_move_to(cr, 220, 15);  
        if(locked) {
          cairo_set_source_rgb(cr, 1, 0, 0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "Locked");
        
        cairo_set_source_rgb(cr, 1, 1, 0);
        cairo_move_to(cr, 100, 50);  
        if(vfo[id].mode==modeFMN) {
          if(deviation==2500) {
            sprintf(temp_text,"%s 8k",mode_string[vfo[id].mode]); 
          } else {
            sprintf(temp_text,"%s 16k",mode_string[vfo[id].mode]); 
          }
        } else {
          sprintf(temp_text,"%s %s",mode_string[vfo[id].mode],band_filter->title); 
        }
        cairo_show_text(cr, temp_text);

        cairo_move_to(cr, 170, 50);  
        if(active_receiver->nr) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "NR");

        cairo_move_to(cr, 200, 50);  
        if(active_receiver->nr2) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "NR2");

        cairo_move_to(cr, 230, 50);  
        if(active_receiver->anf) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "ANF");

        cairo_move_to(cr, 260, 50);  
        if(active_receiver->snb) {
          cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "SNB");

        cairo_move_to(cr, 290, 50);  
        switch(active_receiver->agc) {
          case AGC_OFF:
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
            cairo_show_text(cr, "AGC OFF");
            break;
          case AGC_LONG:
            cairo_set_source_rgb(cr, 1, 1, 0);
            cairo_show_text(cr, "AGC LONG");
            break;
          case AGC_SLOW:
            cairo_set_source_rgb(cr, 1, 1, 0);
            cairo_show_text(cr, "AGC SLOW");
            break;
          case AGC_MEDIUM:
            cairo_set_source_rgb(cr, 1, 1, 0);
            cairo_show_text(cr, "AGC MEDIUM");
            break;
          case AGC_FAST:
            cairo_set_source_rgb(cr, 1, 1, 0);
            cairo_show_text(cr, "AGC FAST");
            break;
        }

        int s=0;
        while(steps[s]!=step && steps[s]!=0) {
          s++;
        }
        sprintf(temp_text,"Step %s",step_labels[s]);
        cairo_move_to(cr, 375, 50);
        cairo_set_source_rgb(cr, 1, 1, 0);
        cairo_show_text(cr, temp_text);

        char *info=getFrequencyInfo(af);
/*
        cairo_move_to(cr, (my_width/4)*3, 50);
        cairo_show_text(cr, getFrequencyInfo(af));
*/
         
        cairo_move_to(cr, 460, 50);  
        if(vfo[id].ctun) {
          cairo_set_source_rgb(cr, 1, 1, 0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "CTUN");

        cairo_move_to(cr, 500, 50);  
        if(cat_control>0) {
          cairo_set_source_rgb(cr, 1, 1, 0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "CAT");

        cairo_move_to(cr, 270, 15);
        if(split) {
          cairo_set_source_rgb(cr, 1, 0, 0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairo_show_text(cr, "Split");
       
        cairo_move_to(cr, 310, 15);
        if(vfo[id].mode==modeCWL || vfo[id].mode==modeCWU) {
          cairo_set_source_rgb(cr, 1, 1, 0);
        } else {
          cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        sprintf(temp_text,"CW %d wpm, sidetone %d Hz",cw_keyer_speed,cw_keyer_sidetone_frequency);
        cairo_show_text(cr, temp_text);

        cairo_destroy (cr);
        gtk_widget_queue_draw (vfo_panel);
    } else {
fprintf(stderr,"vfo_update: no surface!\n");
    }
    return 0;
}

/*
static gboolean
vfo_step_select_cb (GtkWidget *widget,
               gpointer        data)
{
  step=steps[(int)data];
  vfo_update(NULL);
}
*/

static gboolean
vfo_press_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
/*
  if((int)event->x < (my_width/4)) {
    //lock_cb(NULL,NULL);
  } else if((int)event->x < (my_width/2) && (int)event->x > (my_width/4)) {
    start_freqent();
  } else {
    start_step();
  }
*/
  start_vfo();
  return TRUE;
}

GtkWidget* vfo_init(int width,int height,GtkWidget *parent) {
  int i;

fprintf(stderr,"vfo_init: width=%d height=%d\n", width, height);

  parent_window=parent;
  my_width=width;
  my_height=height;

  vfo_panel = gtk_drawing_area_new ();
  gtk_widget_set_size_request (vfo_panel, width, height);

  g_signal_connect (vfo_panel,"configure-event",
            G_CALLBACK (vfo_configure_event_cb), NULL);
  g_signal_connect (vfo_panel, "draw",
            G_CALLBACK (vfo_draw_cb), NULL);

  /* Event signals */
  g_signal_connect (vfo_panel, "button-press-event",
            G_CALLBACK (vfo_press_event_cb), NULL);
  g_signal_connect(vfo_panel,"scroll_event",
            G_CALLBACK(vfo_scroll_event_cb),NULL);
  gtk_widget_set_events (vfo_panel, gtk_widget_get_events (vfo_panel)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_SCROLL_MASK);

  return vfo_panel;
}
