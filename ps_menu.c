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
//#include "led.h"
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

static GThread *info_thread_id;

static int running=0;

#define INFO_SIZE 16

static GtkWidget *entry[INFO_SIZE];

// results from GetPSDisp
static double ps_x[4096];
static double ps_ym[4096];
static double ps_yc[4096];
static double ps_ys[4096];
static double ps_cm[64];
static double ps_cc[64];
static double ps_cs[64];

static void destroy_cb(GtkWidget *widget, gpointer data) {
//fprintf(stderr,"ps_menu: destroy_cb\n");
  running=0;
}

static void cleanup() {
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

static int deltadb=0;

static gpointer info_thread(gpointer arg) {
  static int info[16];
  int i;
  gchar *label;
  static int old5=0;
  static int old5_2=0;
  static int old14=0;
  int display;
  int counter=0;
  int state=0;
  int save_auto_on;
  int save_single_on;

  if(transmitter->auto_on) {
    transmitter->attenuation=31;
  } else {
    transmitter->attenuation=0;
  }

  running=1;
  while(running) {
    GetPSInfo(transmitter->id,&info[0]);
    for(i=0;i<INFO_SIZE;i++) {
      label=NULL;
      display=1;
      switch(i) {
        case 0:
          label=g_strdup_printf("%d",info[i]);
          display=0;
          break;
        case 1:
          label=g_strdup_printf("%d",info[i]);
          display=0;
          break;
        case 2:
          label=g_strdup_printf("%d",info[i]);
          display=0;
          break;
        case 3:
          label=g_strdup_printf("%d",info[i]);
          display=0;
          break;
        case 4:
          label=g_strdup_printf("%d",info[i]);
          break;
        case 5:
          label=g_strdup_printf("%d",info[i]);
          if(info[i]!=old5) {
            old5=info[5];
            if(info[4]>181)  {
              gtk_label_set_markup(GTK_LABEL(feedback_l),"<span color='black'>Feedback Lvl</span>");
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
          label=g_strdup_printf("%d",info[i]);
          break;
        case 13:
          label=g_strdup_printf("%d",info[i]);
          break;
        case 14:
          label=g_strdup_printf("%d",info[i]);
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
              label=g_strdup_printf("RESET");
              break;
            case 1:
              label=g_strdup_printf("WAIT");
              break;
            case 2:
              label=g_strdup_printf("MOXDELAY");
              break;
            case 3:
              label=g_strdup_printf("SETUP");
              break;
            case 4:
              label=g_strdup_printf("COLLECT");
              break;
            case 5:
              label=g_strdup_printf("MOXCHECK");
              break;
            case 6:
              label=g_strdup_printf("CALC");
              break;
            case 7:
              label=g_strdup_printf("DELAY");
              break;
            case 8:
              label=g_strdup_printf("STAYON");
              break;
            case 9:
              label=g_strdup_printf("TUNRON");
              break;
            default:
              label=g_strdup_printf("UNKNOWN %d=%d",i,info[i]);
              break;
          }
          break;
        default:
          label=g_strdup_printf("info %d=%d",i,info[i]);
          display=0;
          break;
      }
      if(display) {
        if(entry[i]!=NULL) {
          gtk_entry_set_text(GTK_ENTRY(entry[i]),label);
        } else {
fprintf(stderr,"ps_menu: entry %d is NULL\n", i);
        }
      }

      if(label!=NULL) {
        g_free(label);
        label=NULL;
      }

    }

    double pk;
    gchar *pk_label;

    GetPSMaxTX(transmitter->id,&pk);
    pk_label=g_strdup_printf("%f",pk);
    gtk_entry_set_text(GTK_ENTRY(get_pk),pk_label);
    if(pk_label!=NULL) {
      g_free(pk_label);
      pk_label=NULL;
    }


    counter++;
    if(counter==10) { // every 100ms
      double ddb;
      int newcal=info[5]!=old5_2;
      old5_2=info[5];
      switch(state) {
        case 0:
          if(transmitter->auto_on && newcal && (info[4]>181 || (info[4]<=128 && transmitter->attenuation>0))) {
            if(info[4]<=256) {
              ddb= 20.0 * log10((double)info[4]/152.293);
              if(isnan(ddb)) {
                ddb=31.1;
              }
              if(ddb<-100.0) {
                ddb=-100.0;
              }
              if(ddb > 100.0) {
                ddb=100.0;
              }
            } else {
              ddb=31.1;
            }
            deltadb=(int)ddb;
            save_auto_on=transmitter->auto_on;
            save_single_on=transmitter->single_on;
            SetPSControl(transmitter->id, 1, 0, 0, 0);
            state=1;
          }
          break;
        case 1:
          if((deltadb+transmitter->attenuation)>0) {
            transmitter->attenuation+=deltadb;
          } else {
            transmitter->attenuation=0;
          }
          state=2;
          break;
        case 2:
          state=0;
          SetPSControl(transmitter->id, 0, save_single_on, save_auto_on, 0);
          break;
      }
      counter=0;
    }
    usleep(10000); // 10 ms
  }
  gtk_entry_set_text(GTK_ENTRY(entry[15]),"");
}

static void enable_cb(GtkWidget *widget, gpointer data) {
  g_idle_add(ext_tx_set_ps,(gpointer)(long)gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void auto_cb(GtkWidget *widget, gpointer data) {
  transmitter->auto_on=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  if(transmitter->auto_on) {
    transmitter->attenuation=31;
  } else {
    transmitter->attenuation=0;
  }
}

static void calibrate_cb(GtkWidget *widget, gpointer data) {
  transmitter->puresignal=1;
  SetPSControl(transmitter->id, 0, 1, 0, 0);
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
  transmitter->attenuation=0;
  SetPSControl(transmitter->id, 1, 0, 0, 0);
}

void ps_twotone(int state) {
  tx_set_twotone(transmitter,state);
  if(transmitter->twotone) {
    //set_button_text_color(widget,"red");
  } else {
    //set_button_text_color(widget,"black");
  }
  if(state && transmitter->puresignal) {
    info_thread_id=g_thread_new( "PS info", info_thread, NULL);
  } else {
    running=0;
  }
}

static void twotone_cb(GtkWidget *widget, gpointer data) {
  int state=transmitter->twotone?0:1;
  //g_idle_add(ext_ps_twotone,(gpointer)(long)state);
  tx_set_twotone(transmitter,state);
  if(state) {
    set_button_text_color(widget,"red");
  } else {
    set_button_text_color(widget,"black");
  }
  if(state && transmitter->puresignal) {
    info_thread_id=g_thread_new( "PS info", info_thread, NULL);
  } else {
    running=0;
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

  GtkWidget *reset_b=gtk_button_new_with_label("Reset");
  gtk_widget_show(reset_b);
  gtk_grid_attach(GTK_GRID(grid),reset_b,col,row,1,1);
  g_signal_connect(reset_b,"pressed",G_CALLBACK(reset_cb),NULL);

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
      //case 0:
      //  strcpy(label,"bldr.rx");
      //  break;
      //case 1:
      //  strcpy(label,"bldr.cm");
      //  break;
      //case 2:
      //  strcpy(label,"bldr.cc");
      //  break;
      //case 3:
      //  strcpy(label,"bldr.cs");
      //  break;
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
        sprintf(label,"Info %d:", i);
        break;
    }
    if(display) {
      GtkWidget *lbl=gtk_label_new(label);
      entry[i]=gtk_entry_new();
      gtk_grid_attach(GTK_GRID(grid),lbl,col,row,1,1);
      col++;
      gtk_grid_attach(GTK_GRID(grid),entry[i],col,row,1,1);
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
  col++;

  lbl=gtk_label_new("SetPk");
  gtk_grid_attach(GTK_GRID(grid),lbl,col,row,1,1);
  col++;

  double pk;
  char pk_text[16];
  GetPSHWPeak(transmitter->id,&pk);
  sprintf(pk_text,"%f",pk);
  set_pk=gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(set_pk),pk_text);
  gtk_grid_attach(GTK_GRID(grid),set_pk,col,row,1,1);

  gtk_container_add(GTK_CONTAINER(content),grid);
  sub_menu=dialog;

  //if(transmitter->puresignal) {
  //  info_thread_id=g_thread_new( "PS info", info_thread, NULL);
  //}

  gtk_widget_show_all(dialog);

}
