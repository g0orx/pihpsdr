#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pskcoresdr.h>
#include <wdsp.h>

#include "psk.h"
#include "radio.h"
#include "channel.h"
#include "wdsp_init.h"

static void *detector;

static double buffer[PSK_BUFFER_SIZE];
static float spectrum_buffer[PSK_BUFFER_SIZE];
static int psk_index;

#define PSK_TEXT_SIZE 128
static char psk_text[PSK_TEXT_SIZE];

GtkWidget *dialog;
GtkWidget *psk_label;

#define TEXT_LINES 10
#define TEXT_LINE_LENGTH 80
char text_label[TEXT_LINES][TEXT_LINE_LENGTH+1];
int first_text_line=0;
int current_text_line=9;
int current_text_offset=0;

static char temp_text[TEXT_LINES*(TEXT_LINE_LENGTH+1)];

static void build_temp_text() {
  int i;
  int offset=0;
  i=first_text_line;
  do {
    strcpy(&temp_text[offset],text_label[i]);
    offset+=strlen(text_label[i]);
    temp_text[offset]=0x0D;
    offset++;
    i++;
    if(i==TEXT_LINES) {
      i=0;
    }
  } while (i!=first_text_line);

  // replace last \r with \0
  temp_text[offset-1]='\0';
}

static void add_text(char *text, int length) {
  int i;

  for(i=0;i<length;i++) {
    if(text[i]==0x0D  || text[i]==0x11 || current_text_offset>=TEXT_LINE_LENGTH) {
      current_text_line++;
      if(current_text_line==TEXT_LINES) {
        current_text_line=0;
      }
      first_text_line++;
      if(first_text_line==TEXT_LINES) {
        first_text_line=0;
      }
      current_text_offset=0;
    }
    //if(text[i]!=0x0D && text[i]!=0x0A) {
    if(text[i]>0x1F) {
      text_label[current_text_line][current_text_offset++]=text[i];
    }
    text_label[current_text_line][current_text_offset]=0x00;
  }
}

GtkWidget *init_psk() {
  int i;

  psk_index=0;
  detector=createPSKDet(8000);
  if(detector==NULL) {
    fprintf(stderr,"init_psk: createPSKDet return null!\n");
    return NULL;
  }
  SetRXFrequency(detector,1500);
  ResetDetector(detector);

  for(i=0;i<TEXT_LINES;i++) {
    text_label[i][0]='\0';
  }
  build_temp_text();
  psk_label=gtk_label_new(temp_text);
  gtk_label_set_single_line_mode (GTK_LABEL(psk_label), FALSE);
  gtk_label_set_width_chars (GTK_LABEL(psk_label), 80);
  gtk_label_set_lines (GTK_LABEL(psk_label), TEXT_LINES);
  gtk_label_set_line_wrap (GTK_LABEL(psk_label), TRUE);
  gtk_misc_set_alignment (GTK_MISC(psk_label), 0, 0);
  
  return psk_label;
}

void close_psk() {
  if(detector!=NULL) {
    freePSKDet(detector);
    gtk_widget_destroy (dialog);
  }
}

void psk_set_frequency(int f) {
  if(detector!=NULL) {
    SetRXFrequency(detector,f);
    ResetDetector(detector);
  }
}

int psk_get_frequency() {
  return GetRXFrequency(detector);
}

int psk_get_signal_level() {
  return GetSignalLevel(detector);
}

static gint psk_update(void* arg) {
  gtk_label_set_text(GTK_LABEL(psk_label),temp_text);
  return 0;
}

int psk_demod(double sample) {
  int i;
  int c;
  int new_lines;
  if(detector!=NULL) {
    buffer[psk_index]=sample;
    spectrum_buffer[psk_index]=(float)sample;
    psk_index++;
    if(psk_index==PSK_BUFFER_SIZE) {
      int detected=runPSKDet(detector,buffer,PSK_BUFFER_SIZE,1,psk_text,PSK_TEXT_SIZE);
      psk_text[detected]='\0';
      if(detected>0) {
        add_text(psk_text,detected);
        build_temp_text();
        g_idle_add(psk_update,(gpointer)NULL);
      }
      Spectrum(CHANNEL_PSK,0,0,spectrum_buffer,spectrum_buffer);
      psk_index=0;
    }
  }
}
