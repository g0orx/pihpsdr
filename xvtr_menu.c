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
#include <stdlib.h>
#include <string.h>

#include "new_menu.h"
#include "band.h"
#include "filter.h"
#include "mode.h"
#include "xvtr_menu.h"
#include "radio.h"
#include "vfo.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *title[BANDS+XVTRS];
static GtkWidget *min_frequency[BANDS+XVTRS];
static GtkWidget *max_frequency[BANDS+XVTRS];
static GtkWidget *lo_frequency[BANDS+XVTRS];
static GtkWidget *lo_error[BANDS+XVTRS];
static GtkWidget *tx_lo_frequency[BANDS+XVTRS];
static GtkWidget *tx_lo_error[BANDS+XVTRS];
static GtkWidget *disable_pa[BANDS+XVTRS];

static void save_xvtr () {
  int i;
  int b;
  if(dialog!=NULL) {
    const char *t;
    const char *minf;
    const char *maxf;
    const char *lof;
    const char *loerr;
    const char *txlof;
    const char *txloerr;
    for(i=BANDS;i<BANDS+XVTRS;i++) {
      BAND *xvtr=band_get_band(i);
      BANDSTACK *bandstack=xvtr->bandstack;
      t=gtk_entry_get_text(GTK_ENTRY(title[i]));
      strcpy(xvtr->title,t);
      if(strlen(t)!=0) {
        minf=gtk_entry_get_text(GTK_ENTRY(min_frequency[i]));
        xvtr->frequencyMin=(long long)(atof(minf)*1000000.0);
        maxf=gtk_entry_get_text(GTK_ENTRY(max_frequency[i]));
        xvtr->frequencyMax=(long long)(atof(maxf)*1000000.0);
        lof=gtk_entry_get_text(GTK_ENTRY(lo_frequency[i]));
        xvtr->frequencyLO=(long long)(atof(lof)*1000000.0);
        loerr=gtk_entry_get_text(GTK_ENTRY(lo_error[i]));
        xvtr->errorLO=atoll(loerr);
        txlof=gtk_entry_get_text(GTK_ENTRY(tx_lo_frequency[i]));
        xvtr->txFrequencyLO=(long long)(atof(txlof)*1000000.0);
        txloerr=gtk_entry_get_text(GTK_ENTRY(tx_lo_error[i]));
        xvtr->txErrorLO=atoll(txloerr);
        xvtr->disablePA=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(disable_pa[i]));
        for(b=0;b<bandstack->entries;b++) {
          BANDSTACK_ENTRY *entry=&bandstack->entry[b];
          entry->frequency=xvtr->frequencyMin+((xvtr->frequencyMax-xvtr->frequencyMin)/2);
          entry->mode=modeUSB;
          entry->filter=filterF6;
        }
fprintf(stderr,"min=%s:%lld max=%s:%lld lo=%s:%lld\n",minf,xvtr->frequencyMin,maxf,xvtr->frequencyMax,lof,xvtr->frequencyLO);
      } else {
        xvtr->frequencyMin=0;
        xvtr->frequencyMax=0;
        xvtr->frequencyLO=0;
        xvtr->errorLO=0;
        xvtr->disablePA=0;
      }
    }
    vfo_xvtr_changed();
  }
}

void update_receiver(int band,gboolean error) {
  int i;
  RECEIVER *rx=active_receiver;
  gboolean saved_ctun;
//g_print("update_receiver: band=%d error=%d\n",band,error);
  if(vfo[0].band==band) {
    BAND *xvtr=band_get_band(band);
//g_print("update_receiver: found band: %s\n",xvtr->title);
    vfo[0].lo=xvtr->frequencyLO+xvtr->errorLO;
    vfo[0].lo_tx=xvtr->txFrequencyLO+xvtr->txErrorLO;
    saved_ctun=vfo[0].ctun;
    if(saved_ctun) {
      vfo[0].ctun=FALSE;
    }
    frequency_changed(rx);
    if(saved_ctun) {
      vfo[0].ctun=TRUE;
    }

/*
    if(radio->transmitter!=NULL) {
      if(radio->transmitter->rx==rx) {
        update_tx_panadapter(radio);
      }
    }
*/
  }
}

void min_frequency_cb(GtkEditable *editable,gpointer user_data) {
  int band=GPOINTER_TO_INT(user_data);
  BAND *xvtr=band_get_band(band);
  const char* minf=gtk_entry_get_text(GTK_ENTRY(min_frequency[band]));
  xvtr->frequencyMin=(long long)(atof(minf)*1000000.0);
  update_receiver(band,FALSE);
}

void max_frequency_cb(GtkEditable *editable,gpointer user_data) {
  int band=GPOINTER_TO_INT(user_data);
  BAND *xvtr=band_get_band(band);
  const char* maxf=gtk_entry_get_text(GTK_ENTRY(max_frequency[band]));
  xvtr->frequencyMin=(long long)(atof(maxf)*1000000.0);
  update_receiver(band,FALSE);
}

void lo_frequency_cb(GtkEditable *editable,gpointer user_data) {
  int band=GPOINTER_TO_INT(user_data);
  BAND *xvtr=band_get_band(band);
  const char* lof=gtk_entry_get_text(GTK_ENTRY(lo_frequency[band]));
  xvtr->frequencyLO=(long long)(atof(lof)*1000000.0);
  update_receiver(band,FALSE);
}

void lo_error_cb(GtkEditable *editable,gpointer user_data) {
g_print("lo_error_cb\n");
  int band=GPOINTER_TO_INT(user_data);
  BAND *xvtr=band_get_band(band);
  const char* errorf=gtk_entry_get_text(GTK_ENTRY(lo_error[band]));
  xvtr->errorLO=atoll(errorf);
  update_receiver(band,TRUE);
}

void lo_error_update(RECEIVER *rx,long long offset) {
g_print("lo_error_update: band=%d\n",vfo[0].band);
  BAND *xvtr=band_get_band(vfo[0].band);
  if(dialog!=NULL) {
    char temp[32];
    sprintf(temp,"%lld",xvtr->errorLO);
    gtk_entry_set_text(GTK_ENTRY(lo_error[vfo[0].band]),temp);
  }
  xvtr->errorLO=xvtr->errorLO+offset;
  update_receiver(vfo[0].band,TRUE);
}

void tx_lo_frequency_cb(GtkEditable *editable,gpointer user_data) {
  int band=GPOINTER_TO_INT(user_data);
  BAND *xvtr=band_get_band(band);
  const char* lof=gtk_entry_get_text(GTK_ENTRY(tx_lo_frequency[band]));
  xvtr->txFrequencyLO=(long long)(atof(lof)*1000000.0);
  update_receiver(band,FALSE);
}

void tx_lo_error_cb(GtkEditable *editable,gpointer user_data) {
  int band=GPOINTER_TO_INT(user_data);
  BAND *xvtr=band_get_band(band);
  const char* errorf=gtk_entry_get_text(GTK_ENTRY(tx_lo_error[band]));
  xvtr->txErrorLO=atoll(errorf);
  update_receiver(band,TRUE);
}

static void cleanup() {
  save_xvtr();
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

void xvtr_menu(GtkWidget *parent) {
  int i,j;
  char f[16];

fprintf(stderr,"xvtr_menu\n");
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - XVTR");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;

  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *label=gtk_label_new("Title");
  gtk_grid_attach(GTK_GRID(grid),label,0,1,1,1);
  label=gtk_label_new("Min Freq(MHz)");
  gtk_grid_attach(GTK_GRID(grid),label,1,1,1,1);
  label=gtk_label_new("Max Freq(MHz)");
  gtk_grid_attach(GTK_GRID(grid),label,2,1,1,1);
  label=gtk_label_new("LO Freq(MHz)");
  gtk_grid_attach(GTK_GRID(grid),label,3,1,1,1);
  label=gtk_label_new("LO Err(Hz)");
  gtk_grid_attach(GTK_GRID(grid),label,4,1,1,1);
  label=gtk_label_new("TX LO Freq(MHz)");
  gtk_grid_attach(GTK_GRID(grid),label,5,1,1,1);
  label=gtk_label_new("TX LO Err(Hz)");
  gtk_grid_attach(GTK_GRID(grid),label,6,1,1,1);
  label=gtk_label_new("Disable PA");
  gtk_grid_attach(GTK_GRID(grid),label,7,1,1,1);


  for(i=BANDS;i<BANDS+XVTRS;i++) {
    BAND *xvtr=band_get_band(i);

    title[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(title[i]),7);
    gtk_entry_set_text(GTK_ENTRY(title[i]),xvtr->title);
    gtk_grid_attach(GTK_GRID(grid),title[i],0,i+2,1,1);

    min_frequency[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(min_frequency[i]),7);
    sprintf(f,"%5.3f",(double)xvtr->frequencyMin/1000000.0);
    gtk_entry_set_text(GTK_ENTRY(min_frequency[i]),f);
    gtk_grid_attach(GTK_GRID(grid),min_frequency[i],1,i+2,1,1);
    g_signal_connect(min_frequency[i],"changed",G_CALLBACK(min_frequency_cb),GINT_TO_POINTER(i));
    
    max_frequency[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(max_frequency[i]),7);
    sprintf(f,"%5.3f",(double)xvtr->frequencyMax/1000000.0);
    gtk_entry_set_text(GTK_ENTRY(max_frequency[i]),f);
    gtk_grid_attach(GTK_GRID(grid),max_frequency[i],2,i+2,1,1);
    g_signal_connect(max_frequency[i],"changed",G_CALLBACK(max_frequency_cb),GINT_TO_POINTER(i));
    
    lo_frequency[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(lo_frequency[i]),7);
    sprintf(f,"%5.3f",(double)xvtr->frequencyLO/1000000.0);
    gtk_entry_set_text(GTK_ENTRY(lo_frequency[i]),f);
    gtk_grid_attach(GTK_GRID(grid),lo_frequency[i],3,i+2,1,1);
    g_signal_connect(lo_frequency[i],"changed",G_CALLBACK(lo_frequency_cb),GINT_TO_POINTER(i));

    lo_error[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(lo_error[i]),9);
    sprintf(f,"%lld",xvtr->errorLO);
    gtk_entry_set_text(GTK_ENTRY(lo_error[i]),f);
    gtk_grid_attach(GTK_GRID(grid),lo_error[i],4,i+2,1,1);
    g_signal_connect(lo_error[i],"changed",G_CALLBACK(lo_error_cb),GINT_TO_POINTER(i));

    tx_lo_frequency[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(tx_lo_frequency[i]),7);
    sprintf(f,"%5.3f",(double)xvtr->txFrequencyLO/1000000.0);
    gtk_entry_set_text(GTK_ENTRY(tx_lo_frequency[i]),f);
    gtk_grid_attach(GTK_GRID(grid),tx_lo_frequency[i],5,i+2,1,1);
    g_signal_connect(tx_lo_frequency[i],"changed",G_CALLBACK(tx_lo_frequency_cb),GINT_TO_POINTER(i));

    tx_lo_error[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(tx_lo_error[i]),9);
    sprintf(f,"%lld",xvtr->txErrorLO);
    gtk_entry_set_text(GTK_ENTRY(tx_lo_error[i]),f);
    gtk_grid_attach(GTK_GRID(grid),tx_lo_error[i],6,i+2,1,1);
    g_signal_connect(tx_lo_error[i],"changed",G_CALLBACK(tx_lo_error_cb),GINT_TO_POINTER(i));

    disable_pa[i]=gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(disable_pa[i]),xvtr->disablePA);
    gtk_grid_attach(GTK_GRID(grid),disable_pa[i],7,i+2,1,1);
    
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

