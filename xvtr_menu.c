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
#include <string.h>

#include "new_menu.h"
#include "band.h"
#include "filter.h"
#include "mode.h"
#include "xvtr_menu.h"
#include "radio.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *title[BANDS+XVTRS];
static GtkWidget *min_frequency[BANDS+XVTRS];
static GtkWidget *max_frequency[BANDS+XVTRS];
static GtkWidget *lo_frequency[BANDS+XVTRS];
static GtkWidget *disable_pa[BANDS+XVTRS];

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  int i;
  if(dialog!=NULL) {
    const char *t;
    const char *minf;
    const char *maxf;
    const char *lof;
    for(i=BANDS;i<BANDS+XVTRS;i++) {
      BAND *xvtr=band_get_band(i);
      BANDSTACK *bandstack=xvtr->bandstack;
      BANDSTACK_ENTRY *entry=bandstack->entry;
      t=gtk_entry_get_text(GTK_ENTRY(title[i]));
      strcpy(xvtr->title,t);
      if(strlen(t)!=0) {
        minf=gtk_entry_get_text(GTK_ENTRY(min_frequency[i]));
        xvtr->frequencyMin=atoll(minf);
        maxf=gtk_entry_get_text(GTK_ENTRY(max_frequency[i]));
        xvtr->frequencyMax=atoll(maxf);
        lof=gtk_entry_get_text(GTK_ENTRY(lo_frequency[i]));
        xvtr->frequencyLO=atoll(lof);
        xvtr->disablePA=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(disable_pa[i]));
        entry->frequencyA=xvtr->frequencyMin;
        entry->frequencyB=xvtr->frequencyMin;
        entry->mode=modeUSB;
        entry->filter=filterF6;
      } else {
        xvtr->frequencyMin=0;
        xvtr->frequencyMax=0;
        xvtr->frequencyLO=0;
        xvtr->disablePA=0;
      }
    }
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
  return TRUE;
}

void xvtr_menu(GtkWidget *parent) {
  int i,j;
  char f[16];

fprintf(stderr,"xvtr_menu\n");
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;

  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  //gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *close_b=gtk_button_new_with_label("Close XVTR");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *label=gtk_label_new("Title");
  gtk_grid_attach(GTK_GRID(grid),label,0,1,1,1);
  label=gtk_label_new("Min Freq (Hz)");
  gtk_grid_attach(GTK_GRID(grid),label,1,1,1,1);
  label=gtk_label_new("Max Freq (Hz)");
  gtk_grid_attach(GTK_GRID(grid),label,2,1,1,1);
  label=gtk_label_new("LO Freq (Hz)");
  gtk_grid_attach(GTK_GRID(grid),label,3,1,1,1);
  label=gtk_label_new("Disable PA");
  gtk_grid_attach(GTK_GRID(grid),label,4,1,1,1);



  for(i=BANDS;i<BANDS+XVTRS;i++) {
fprintf(stderr,"xvtr_menu: band: %d\n",i);
    BAND *xvtr=band_get_band(i);
fprintf(stderr,"xvtr_menu: band: %s\n",xvtr->title);

    title[i]=gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(title[i]),xvtr->title);
    gtk_grid_attach(GTK_GRID(grid),title[i],0,i+2,1,1);

    min_frequency[i]=gtk_entry_new();
    sprintf(f,"%lld",xvtr->frequencyMin);
    gtk_entry_set_text(GTK_ENTRY(min_frequency[i]),f);
    gtk_grid_attach(GTK_GRID(grid),min_frequency[i],1,i+2,1,1);
    
    max_frequency[i]=gtk_entry_new();
    sprintf(f,"%lld",xvtr->frequencyMax);
    gtk_entry_set_text(GTK_ENTRY(max_frequency[i]),f);
    gtk_grid_attach(GTK_GRID(grid),max_frequency[i],2,i+2,1,1);
    
    lo_frequency[i]=gtk_entry_new();
    sprintf(f,"%lld",xvtr->frequencyLO);
    gtk_entry_set_text(GTK_ENTRY(lo_frequency[i]),f);
    gtk_grid_attach(GTK_GRID(grid),lo_frequency[i],3,i+2,1,1);

    disable_pa[i]=gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(disable_pa[i]),xvtr->disablePA);
    gtk_grid_attach(GTK_GRID(grid),disable_pa[i],4,i+2,1,1);
    
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

