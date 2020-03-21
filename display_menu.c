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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "new_menu.h"
#include "display_menu.h"
#include "channel.h"
#include "radio.h"
#include "wdsp.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

static void cleanup() {
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

static void detector_mode_cb(GtkToggleButton *widget, gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    display_detector_mode=GPOINTER_TO_UINT(data);
    SetDisplayDetectorMode(active_receiver->id, 0, display_detector_mode);
  }
}

static void average_mode_cb(GtkToggleButton *widget, gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    display_average_mode=GPOINTER_TO_UINT(data);
    SetDisplayAverageMode(active_receiver->id, 0, display_average_mode);
  }
}

static void time_value_changed_cb(GtkWidget *widget, gpointer data) {
  display_average_time=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  calculate_display_average(active_receiver);
}

static void filled_cb(GtkWidget *widget, gpointer data) {
  display_filled=display_filled==1?0:1;
}

static void frames_per_second_value_changed_cb(GtkWidget *widget, gpointer data) {
  updates_per_second=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  active_receiver->fps=updates_per_second;
  calculate_display_average(active_receiver);
  set_displaying(active_receiver, 1);
}

static void panadapter_high_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->panadapter_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_low_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->panadapter_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_step_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->panadapter_step=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_high_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->waterfall_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_low_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->waterfall_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_automatic_cb(GtkWidget *widget, gpointer data) {
  active_receiver->waterfall_automatic=active_receiver->waterfall_automatic==1?0:1;
}

static void display_panadapter_cb(GtkWidget *widget, gpointer data) {
  active_receiver->display_panadapter=active_receiver->display_panadapter==1?0:1;
  reconfigure_radio();
}

static void display_waterfall_cb(GtkWidget *widget, gpointer data) {
  active_receiver->display_waterfall=active_receiver->display_waterfall==1?0:1;
  reconfigure_radio();
}

static void display_zoompan_cb(GtkWidget *widget, gpointer data) {
  display_zoompan=display_zoompan==1?0:1;
  reconfigure_radio();
}

static void display_sliders_cb(GtkWidget *widget, gpointer data) {
  display_sliders=display_sliders==1?0:1;
  reconfigure_radio();
}


static void display_toolbar_cb(GtkWidget *widget, gpointer data) {
  display_toolbar=display_toolbar==1?0:1;
  reconfigure_radio();
}

static void display_sequence_errors_cb(GtkWidget *widget, gpointer data) {
  display_sequence_errors=display_sequence_errors==1?0:1;
}

void display_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Display");
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
  //gtk_grid_set_row_spacing (GTK_GRID(grid),10);
  //gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);

  int col=0;
  int row=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  row++;
  col=0;

  GtkWidget *filled_b=gtk_check_button_new_with_label("Fill Panadapter");
  //gtk_widget_override_font(filled_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filled_b), display_filled);
  gtk_widget_show(filled_b);
  gtk_grid_attach(GTK_GRID(grid),filled_b,col,row,1,1);
  g_signal_connect(filled_b,"toggled",G_CALLBACK(filled_cb),NULL);

  row++;
  col=0;

  GtkWidget *frames_per_second_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(frames_per_second_label), "<b>Frames Per Second:</b>");
  //gtk_widget_override_font(frames_per_second_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(frames_per_second_label);
  gtk_grid_attach(GTK_GRID(grid),frames_per_second_label,col,row,1,1);

  col++;

  GtkWidget *frames_per_second_r=gtk_spin_button_new_with_range(1.0,100.0,1.0);
  //gtk_widget_override_font(frames_per_second_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(frames_per_second_r),(double)updates_per_second);
  gtk_widget_show(frames_per_second_r);
  gtk_grid_attach(GTK_GRID(grid),frames_per_second_r,col,row,1,1);
  g_signal_connect(frames_per_second_r,"value_changed",G_CALLBACK(frames_per_second_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *panadapter_high_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_high_label), "<b>Panadapter High: </b>");
  //gtk_widget_override_font(panadapter_high_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_high_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_high_label,col,row,1,1);

  col++;

  GtkWidget *panadapter_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(panadapter_high_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_high_r),(double)active_receiver->panadapter_high);
  gtk_widget_show(panadapter_high_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_high_r,col,row,1,1);
  g_signal_connect(panadapter_high_r,"value_changed",G_CALLBACK(panadapter_high_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *panadapter_low_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_low_label), "<b>Panadapter Low: </b>");
  //gtk_widget_override_font(panadapter_low_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_low_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_low_label,col,row,1,1);

  col++;

  GtkWidget *panadapter_low_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(panadapter_low_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_low_r),(double)active_receiver->panadapter_low);
  gtk_widget_show(panadapter_low_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_low_r,col,row,1,1);
  g_signal_connect(panadapter_low_r,"value_changed",G_CALLBACK(panadapter_low_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *panadapter_step_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_step_label), "<b>Panadapter Step: </b>");
  //gtk_widget_override_font(panadapter_step_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_step_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_step_label,col,row,1,1);

  col++;

  GtkWidget *panadapter_step_r=gtk_spin_button_new_with_range(1.0,20.0,1.0);
  //gtk_widget_override_font(panadapter_step_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_step_r),(double)active_receiver->panadapter_step);
  gtk_widget_show(panadapter_step_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_step_r,col,row,1,1);
  g_signal_connect(panadapter_step_r,"value_changed",G_CALLBACK(panadapter_step_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *waterfall_automatic_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(waterfall_automatic_label), "<b>Waterfall Automatic: </b>");
  //gtk_widget_override_font(waterfall_automatic_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_automatic_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_automatic_label,col,row,1,1);

  col++;

  GtkWidget *waterfall_automatic_b=gtk_check_button_new();
  //gtk_widget_override_font(waterfall_automatic_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (waterfall_automatic_b), active_receiver->waterfall_automatic);
  gtk_widget_show(waterfall_automatic_b);
  gtk_grid_attach(GTK_GRID(grid),waterfall_automatic_b,col,row,1,1);
  g_signal_connect(waterfall_automatic_b,"toggled",G_CALLBACK(waterfall_automatic_cb),NULL);

  row++;
  col=0;

  GtkWidget *waterfall_high_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(waterfall_high_label), "<b>Waterfall High: </b>");
  //gtk_widget_override_font(waterfall_high_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_high_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_high_label,col,row,1,1);

  col++;

  GtkWidget *waterfall_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(waterfall_high_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(waterfall_high_r),(double)active_receiver->waterfall_high);
  gtk_widget_show(waterfall_high_r);
  gtk_grid_attach(GTK_GRID(grid),waterfall_high_r,col,row,1,1);
  g_signal_connect(waterfall_high_r,"value_changed",G_CALLBACK(waterfall_high_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *waterfall_low_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(waterfall_low_label), "<b>Waterfall Low: </b>");
  //gtk_widget_override_font(waterfall_low_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_low_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_low_label,col,row,1,1);

  col++;

  GtkWidget *waterfall_low_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(waterfall_low_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(waterfall_low_r),(double)active_receiver->waterfall_low);
  gtk_widget_show(waterfall_low_r);
  gtk_grid_attach(GTK_GRID(grid),waterfall_low_r,col,row,1,1);
  g_signal_connect(waterfall_low_r,"value_changed",G_CALLBACK(waterfall_low_value_changed_cb),NULL);

  col=2;
  row=1;

  GtkWidget *detector_mode_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(detector_mode_label), "<b>Detector:</b>");
  //gtk_widget_override_font(detector_mode_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(detector_mode_label);
  gtk_grid_attach(GTK_GRID(grid),detector_mode_label,col,row,1,1);

  row++;

  GtkWidget *detector_mode_peak=gtk_radio_button_new_with_label(NULL,"Peak");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (detector_mode_peak), display_detector_mode==DETECTOR_MODE_PEAK);
  gtk_widget_show(detector_mode_peak);
  gtk_grid_attach(GTK_GRID(grid),detector_mode_peak,col,row,1,1);
  g_signal_connect(detector_mode_peak,"toggled",G_CALLBACK(detector_mode_cb),(gpointer *)DETECTOR_MODE_PEAK);

  row++;

  GtkWidget *detector_mode_rosenfell=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(detector_mode_peak),"Rosenfell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (detector_mode_rosenfell), display_detector_mode==DETECTOR_MODE_ROSENFELL);
  gtk_widget_show(detector_mode_rosenfell);
  gtk_grid_attach(GTK_GRID(grid),detector_mode_rosenfell,col,row,1,1);
  g_signal_connect(detector_mode_rosenfell,"toggled",G_CALLBACK(detector_mode_cb),(gpointer *)DETECTOR_MODE_ROSENFELL);

  row++;

  GtkWidget *detector_mode_average=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(detector_mode_rosenfell),"Average");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (detector_mode_average), display_detector_mode==DETECTOR_MODE_AVERAGE);
  gtk_widget_show(detector_mode_average);
  gtk_grid_attach(GTK_GRID(grid),detector_mode_average,col,row,1,1);
  g_signal_connect(detector_mode_average,"toggled",G_CALLBACK(detector_mode_cb),(gpointer *)DETECTOR_MODE_AVERAGE);

  row++;

  GtkWidget *detector_mode_sample=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(detector_mode_average),"Sample");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (detector_mode_sample), display_detector_mode==DETECTOR_MODE_SAMPLE);
  gtk_widget_show(detector_mode_sample);
  gtk_grid_attach(GTK_GRID(grid),detector_mode_sample,col,row,1,1);
  g_signal_connect(detector_mode_sample,"toggled",G_CALLBACK(detector_mode_cb),(gpointer *)DETECTOR_MODE_SAMPLE);


  col=3;
  row=1;

  GtkWidget *average_mode_label=gtk_label_new("Averaging: ");
  gtk_label_set_markup(GTK_LABEL(average_mode_label), "<b>Averaging:</b>");
  //gtk_widget_override_font(average_mode_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(average_mode_label);
  gtk_grid_attach(GTK_GRID(grid),average_mode_label,col,row,1,1);

  row++;

  GtkWidget *average_mode_none=gtk_radio_button_new_with_label(NULL,"None");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average_mode_none), display_detector_mode==AVERAGE_MODE_NONE);
  gtk_widget_show(average_mode_none);
  gtk_grid_attach(GTK_GRID(grid),average_mode_none,col,row,1,1);
  g_signal_connect(average_mode_none,"toggled",G_CALLBACK(average_mode_cb),(gpointer *)AVERAGE_MODE_NONE);

  row++;

  GtkWidget *average_mode_recursive=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(average_mode_none),"Recursive");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average_mode_recursive), display_average_mode==AVERAGE_MODE_RECURSIVE);
  gtk_widget_show(average_mode_recursive);
  gtk_grid_attach(GTK_GRID(grid),average_mode_recursive,col,row,1,1);
  g_signal_connect(average_mode_recursive,"toggled",G_CALLBACK(average_mode_cb),(gpointer *)AVERAGE_MODE_RECURSIVE);

  row++;

  GtkWidget *average_mode_time_window=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(average_mode_recursive),"Time Window");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average_mode_time_window), display_average_mode==AVERAGE_MODE_TIME_WINDOW);
  gtk_widget_show(average_mode_time_window);
  gtk_grid_attach(GTK_GRID(grid),average_mode_time_window,col,row,1,1);
  g_signal_connect(average_mode_time_window,"toggled",G_CALLBACK(average_mode_cb),(gpointer *)AVERAGE_MODE_TIME_WINDOW);

  row++;

  GtkWidget *average_mode_log_recursive=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(average_mode_time_window),"Log Recursive");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average_mode_log_recursive), display_average_mode==AVERAGE_MODE_LOG_RECURSIVE);
  gtk_widget_show(average_mode_log_recursive);
  gtk_grid_attach(GTK_GRID(grid),average_mode_log_recursive,col,row,1,1);
  g_signal_connect(average_mode_log_recursive,"toggled",G_CALLBACK(average_mode_cb),(gpointer *)AVERAGE_MODE_LOG_RECURSIVE);

  row++;

  GtkWidget *time_label=gtk_label_new("Time (ms): ");
  //gtk_widget_override_font(average_mode_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(time_label);
  gtk_grid_attach(GTK_GRID(grid),time_label,col,row,1,1);

  col++;

  GtkWidget *time_r=gtk_spin_button_new_with_range(1.0,9999.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(time_r),(double)display_average_time);
  gtk_widget_show(time_r);
  gtk_grid_attach(GTK_GRID(grid),time_r,col,row,1,1);
  g_signal_connect(time_r,"value_changed",G_CALLBACK(time_value_changed_cb),NULL);

  row++;
  row++;
  row++;
  col=0;

  
  GtkWidget *b_display_waterfall=gtk_check_button_new_with_label("Display Waterfall");
  //gtk_widget_override_font(b_display_waterfall, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_waterfall), active_receiver->display_waterfall);
  gtk_widget_show(b_display_waterfall);
  gtk_grid_attach(GTK_GRID(grid),b_display_waterfall,col,row,1,1);
  g_signal_connect(b_display_waterfall,"toggled",G_CALLBACK(display_waterfall_cb),(gpointer *)NULL);

  col++;

  GtkWidget *b_display_zoompan=gtk_check_button_new_with_label("Display Zoom/Pan");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_zoompan), display_zoompan);
  gtk_widget_show(b_display_zoompan);
  gtk_grid_attach(GTK_GRID(grid),b_display_zoompan,col,row,1,1);
  g_signal_connect(b_display_zoompan,"toggled",G_CALLBACK(display_zoompan_cb),(gpointer *)NULL);

  col++;

  GtkWidget *b_display_sliders=gtk_check_button_new_with_label("Display Sliders");
  //gtk_widget_override_font(b_display_sliders, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_sliders), display_sliders);
  gtk_widget_show(b_display_sliders);
  gtk_grid_attach(GTK_GRID(grid),b_display_sliders,col,row,1,1);
  g_signal_connect(b_display_sliders,"toggled",G_CALLBACK(display_sliders_cb),(gpointer *)NULL);

  col++;

  GtkWidget *b_display_toolbar=gtk_check_button_new_with_label("Display Toolbar");
  //gtk_widget_override_font(b_display_toolbar, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_toolbar), display_toolbar);
  gtk_widget_show(b_display_toolbar);
  gtk_grid_attach(GTK_GRID(grid),b_display_toolbar,col,row,1,1);
  g_signal_connect(b_display_toolbar,"toggled",G_CALLBACK(display_toolbar_cb),(gpointer *)NULL);

  col++;

  GtkWidget *b_display_sequence_errors=gtk_check_button_new_with_label("Display Seq Errs");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_sequence_errors), display_sequence_errors);
  gtk_widget_show(b_display_sequence_errors);
  gtk_grid_attach(GTK_GRID(grid),b_display_sequence_errors,col,row,1,1);
  g_signal_connect(b_display_sequence_errors,"toggled",G_CALLBACK(display_sequence_errors_cb),(gpointer *)NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

