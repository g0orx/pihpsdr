/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "new_menu.h"
#include "band.h"
#include "filter.h"
#include "mode.h"
#include "radio.h"
#include "vfo.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *label;

#define BUF_SIZE 88

static char *btn_labels[] = {"1","2","3","4",
                             "5","6","7","8",
                             "9","0",".","BS",
                             "HZ","KZ","MZ","CR"
                            };

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if(dialog!=NULL) {
        gtk_widget_destroy(dialog);
        dialog=NULL;
        sub_menu=NULL;
    }
    return TRUE;
}

static gboolean freqent_select_cb (GtkWidget *widget, gpointer data) {
    char *str = (char *) data;
    const char *labelText;
    char output[BUF_SIZE], buffer[BUF_SIZE];
    int  len;
    double  mult;
    long long f;
    static int set = 0;

    if (set) {
        set = 0;
        strcpy (buffer, "0");
        sprintf(output, "<big>%s</big>", buffer);
        gtk_label_set_markup (GTK_LABEL (label), output);
        len = 1;
    } else {
        labelText = gtk_label_get_text (GTK_LABEL (label));
        strcpy (buffer, labelText);
        len = strlen (buffer);
    }

    if (isdigit (str[0]) || str[0] == '.') {

        buffer[len] = (gchar) str[0];
        buffer[len+1] = (gchar) 0;

        len = (buffer[0] == '0') ? 1 : 0;

        sprintf(output, "<big>%s</big>", buffer+len);
        gtk_label_set_markup (GTK_LABEL (label), output);
    } else {

        if (strcmp (str, "BS") == 0) {
            /* --- Remove the last character on it. --- */
            if (len > 0) buffer[len-1] = (gchar) 0;

            /* --- Remove digit from field. --- */
            sprintf(output, "<big>%s</big>", buffer);
            gtk_label_set_markup (GTK_LABEL (label), output);

        /* --- clear? --- */
        } else if (strcmp (str, "CR") == 0) {
            strcpy (buffer, "0");
            sprintf(output, "<big>%s</big>", buffer);
            gtk_label_set_markup (GTK_LABEL (label), output);
        } else if (str[1] == 'Z') {
            switch(str[0]) {
            case 'M':
                mult = 10000000.0;
                break;
            case 'K':
                mult = 10000.0;
                break;
            default :
                mult = 10.0;
            }
            f = ((long long)(atof(buffer)*mult)+5)/10;
            sprintf(output, "<big>%lld</big>", f);
            gtk_label_set_markup (GTK_LABEL (label), output);
            int b=get_band_from_frequency(f);
            if(b<0) {
              fprintf(stderr,"get_band_from_frequency: failed for f=%lld\n",f);
              b=bandGen;
            }
            if(b!=band_get_current()) {
              BAND *band=band_set_current(b);
              BANDSTACK_ENTRY *entry=bandstack_entry_get_current();
              setMode(entry->mode);
              FILTER* band_filters=filters[entry->mode];
              FILTER* band_filter=&band_filters[entry->filter];
              setFilter(band_filter->low,band_filter->high);
              set_alex_rx_antenna(band->alexRxAntenna);
              set_alex_tx_antenna(band->alexTxAntenna);
              set_alex_attenuation(band->alexAttenuation);

            }
            setFrequency(f);
            vfo_update(NULL);
      
            set = 1;
        }
    }
    vfo_update(NULL);
}

static GtkWidget *last_mode;

void freqent_menu(GtkWidget *parent) {
    int i;

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

    gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_spacing (GTK_GRID(grid),4);
    gtk_grid_set_row_spacing (GTK_GRID(grid),4);

    GtkWidget *close_b=gtk_button_new_with_label("Close FreqEntry");
    g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), "<big>0</big>");
    gtk_misc_set_alignment (GTK_MISC (label), 1, .5);
    gtk_grid_attach(GTK_GRID(grid),label,1,0,1,1);

    GtkWidget *step_rb=NULL;
    for (i=0; i<16; i++) {
        GtkWidget *b=gtk_button_new_with_label(btn_labels[i]);
        set_button_text_color(b,"black");
        gtk_widget_show(b);
        gtk_grid_attach(GTK_GRID(grid),b,i%4,1+(i/4),1,1);
        g_signal_connect(b,"pressed",G_CALLBACK(freqent_select_cb),(gpointer *)btn_labels[i]);
    }

    gtk_container_add(GTK_CONTAINER(content),grid);

    sub_menu=dialog;

    gtk_widget_show_all(dialog);

}
