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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "main.h"
#include "channel.h"
#include "discovered.h"
#include "radio.h"
#include "gpio.h"

#ifdef GPIO
void configure_gpio(GtkWidget *parent) {
  gpio_restore_state();

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Configure GPIO",GTK_WINDOW(parent),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();
  //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);


  GtkWidget *b_enable_vfo_encoder=gtk_check_button_new_with_label("Enable VFO");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_vfo_encoder), ENABLE_VFO_ENCODER);
  gtk_widget_show(b_enable_vfo_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_vfo_encoder,0,0,1,1);

  GtkWidget *vfo_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(vfo_a_label);
  gtk_grid_attach(GTK_GRID(grid),vfo_a_label,1,0,1,1);

  GtkWidget *vfo_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(vfo_a),VFO_ENCODER_A);
  gtk_widget_show(vfo_a);
  gtk_grid_attach(GTK_GRID(grid),vfo_a,2,0,1,1);

  GtkWidget *vfo_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(vfo_b_label);
  gtk_grid_attach(GTK_GRID(grid),vfo_b_label,3,0,1,1);

  GtkWidget *vfo_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(vfo_b),VFO_ENCODER_B);
  gtk_widget_show(vfo_b);
  gtk_grid_attach(GTK_GRID(grid),vfo_b,4,0,1,1);

  GtkWidget *b_enable_vfo_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_vfo_pullup), ENABLE_VFO_PULLUP);
  gtk_widget_show(b_enable_vfo_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_vfo_pullup,5,0,1,1);



  GtkWidget *b_enable_af_encoder=gtk_check_button_new_with_label("Enable AF");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_af_encoder), ENABLE_AF_ENCODER);
  gtk_widget_show(b_enable_af_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_af_encoder,0,1,1,1);

  GtkWidget *af_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(af_a_label);
  gtk_grid_attach(GTK_GRID(grid),af_a_label,1,1,1,1);

  GtkWidget *af_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(af_a),AF_ENCODER_A);
  gtk_widget_show(af_a);
  gtk_grid_attach(GTK_GRID(grid),af_a,2,1,1,1);

  GtkWidget *af_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(af_b_label);
  gtk_grid_attach(GTK_GRID(grid),af_b_label,3,1,1,1);

  GtkWidget *af_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(af_b),AF_ENCODER_B);
  gtk_widget_show(af_b);
  gtk_grid_attach(GTK_GRID(grid),af_b,4,1,1,1);

  GtkWidget *b_enable_af_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_af_pullup), ENABLE_AF_PULLUP);
  gtk_widget_show(b_enable_af_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_af_pullup,5,1,1,1);



  GtkWidget *b_enable_rf_encoder=gtk_check_button_new_with_label("Enable RF");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_rf_encoder), ENABLE_RF_ENCODER);
  gtk_widget_show(b_enable_rf_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_rf_encoder,0,2,1,1);

  GtkWidget *rf_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(rf_a_label);
  gtk_grid_attach(GTK_GRID(grid),rf_a_label,1,2,1,1);

  GtkWidget *rf_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(rf_a),RF_ENCODER_A);
  gtk_widget_show(rf_a);
  gtk_grid_attach(GTK_GRID(grid),rf_a,2,2,1,1);

  GtkWidget *rf_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(rf_b_label);
  gtk_grid_attach(GTK_GRID(grid),rf_b_label,3,2,1,1);

  GtkWidget *rf_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(rf_b),RF_ENCODER_B);
  gtk_widget_show(rf_b);
  gtk_grid_attach(GTK_GRID(grid),rf_b,4,2,1,1);

  GtkWidget *b_enable_rf_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_rf_pullup), ENABLE_RF_PULLUP);
  gtk_widget_show(b_enable_rf_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_rf_pullup,5,2,1,1);



  GtkWidget *b_enable_agc_encoder=gtk_check_button_new_with_label("Enable AGC");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_agc_encoder), ENABLE_AGC_ENCODER);
  gtk_widget_show(b_enable_agc_encoder);
  gtk_grid_attach(GTK_GRID(grid),b_enable_agc_encoder,0,3,1,1);

  GtkWidget *agc_a_label=gtk_label_new("GPIO A:");
  gtk_widget_show(agc_a_label);
  gtk_grid_attach(GTK_GRID(grid),agc_a_label,1,3,1,1);

  GtkWidget *agc_a=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(agc_a),AGC_ENCODER_A);
  gtk_widget_show(agc_a);
  gtk_grid_attach(GTK_GRID(grid),agc_a,2,3,1,1);

  GtkWidget *agc_b_label=gtk_label_new("GPIO B:");
  gtk_widget_show(agc_b_label);
  gtk_grid_attach(GTK_GRID(grid),agc_b_label,3,3,1,1);

  GtkWidget *agc_b=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(agc_b),AGC_ENCODER_B);
  gtk_widget_show(agc_b);
  gtk_grid_attach(GTK_GRID(grid),agc_b,4,3,1,1);

  GtkWidget *b_enable_agc_pullup=gtk_check_button_new_with_label("Enable Pull-up");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_agc_pullup), ENABLE_AGC_PULLUP);
  gtk_widget_show(b_enable_agc_pullup);
  gtk_grid_attach(GTK_GRID(grid),b_enable_agc_pullup,5,3,1,1);



  GtkWidget *b_enable_band=gtk_check_button_new_with_label("Enable Band");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_band), ENABLE_BAND_BUTTON);
  gtk_widget_show(b_enable_band);
  gtk_grid_attach(GTK_GRID(grid),b_enable_band,0,4,1,1);

  GtkWidget *band_label=gtk_label_new("GPIO:");
  gtk_widget_show(band_label);
  gtk_grid_attach(GTK_GRID(grid),band_label,1,4,1,1);

  GtkWidget *band=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(band),BAND_BUTTON);
  gtk_widget_show(band);
  gtk_grid_attach(GTK_GRID(grid),band,2,4,1,1);


  GtkWidget *b_enable_mode=gtk_check_button_new_with_label("Enable Mode");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_mode), ENABLE_MODE_BUTTON);
  gtk_widget_show(b_enable_mode);
  gtk_grid_attach(GTK_GRID(grid),b_enable_mode,0,5,1,1);

  GtkWidget *mode_label=gtk_label_new("GPIO:");
  gtk_widget_show(mode_label);
  gtk_grid_attach(GTK_GRID(grid),mode_label,1,5,1,1);

  GtkWidget *mode=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(mode),MODE_BUTTON);
  gtk_widget_show(mode);
  gtk_grid_attach(GTK_GRID(grid),mode,2,5,1,1);


  GtkWidget *b_enable_filter=gtk_check_button_new_with_label("Enable Filter");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_filter), ENABLE_FILTER_BUTTON);
  gtk_widget_show(b_enable_filter);
  gtk_grid_attach(GTK_GRID(grid),b_enable_filter,0,6,1,1);

  GtkWidget *filter_label=gtk_label_new("GPIO:");
  gtk_widget_show(filter_label);
  gtk_grid_attach(GTK_GRID(grid),filter_label,1,6,1,1);

  GtkWidget *filter=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(filter),FILTER_BUTTON);
  gtk_widget_show(filter);
  gtk_grid_attach(GTK_GRID(grid),filter,2,6,1,1);


  GtkWidget *b_enable_noise=gtk_check_button_new_with_label("Enable Noise");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_noise), ENABLE_NOISE_BUTTON);
  gtk_widget_show(b_enable_noise);
  gtk_grid_attach(GTK_GRID(grid),b_enable_noise,0,7,1,1);

  GtkWidget *noise_label=gtk_label_new("GPIO:");
  gtk_widget_show(noise_label);
  gtk_grid_attach(GTK_GRID(grid),noise_label,1,7,1,1);

  GtkWidget *noise=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(noise),NOISE_BUTTON);
  gtk_widget_show(noise);
  gtk_grid_attach(GTK_GRID(grid),noise,2,7,1,1);


  GtkWidget *b_enable_agc=gtk_check_button_new_with_label("Enable AGC");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_agc), ENABLE_AGC_BUTTON);
  gtk_widget_show(b_enable_agc);
  gtk_grid_attach(GTK_GRID(grid),b_enable_agc,0,8,1,1);

  GtkWidget *agc_label=gtk_label_new("GPIO:");
  gtk_widget_show(agc_label);
  gtk_grid_attach(GTK_GRID(grid),agc_label,1,8,1,1);

  GtkWidget *agc=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(agc),AGC_BUTTON);
  gtk_widget_show(agc);
  gtk_grid_attach(GTK_GRID(grid),agc,2,8,1,1);


  GtkWidget *b_enable_mox=gtk_check_button_new_with_label("Enable MOX");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_mox), ENABLE_MOX_BUTTON);
  gtk_widget_show(b_enable_mox);
  gtk_grid_attach(GTK_GRID(grid),b_enable_mox,0,9,1,1);

  GtkWidget *mox_label=gtk_label_new("GPIO:");
  gtk_widget_show(mox_label);
  gtk_grid_attach(GTK_GRID(grid),mox_label,1,9,1,1);

  GtkWidget *mox=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(mox),MOX_BUTTON);
  gtk_widget_show(mox);
  gtk_grid_attach(GTK_GRID(grid),mox,2,9,1,1);


  GtkWidget *b_enable_function=gtk_check_button_new_with_label("Enable Function");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_enable_function), ENABLE_FUNCTION_BUTTON);
  gtk_widget_show(b_enable_function);
  gtk_grid_attach(GTK_GRID(grid),b_enable_function,0,10,1,1);

  GtkWidget *function_label=gtk_label_new("GPIO:");
  gtk_widget_show(function_label);
  gtk_grid_attach(GTK_GRID(grid),function_label,1,10,1,1);

  GtkWidget *function=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(function),FUNCTION_BUTTON);
  gtk_widget_show(function);
  gtk_grid_attach(GTK_GRID(grid),function,2,10,1,1);

  GtkWidget *cwl_label=gtk_label_new("CWL");
  gtk_widget_show(cwl_label);
  gtk_grid_attach(GTK_GRID(grid),cwl_label,0,11,1,1);

  GtkWidget *cwl_gpio_label=gtk_label_new("GPIO:");
  gtk_widget_show(cwl_gpio_label);
  gtk_grid_attach(GTK_GRID(grid),cwl_gpio_label,1,11,1,1);

  GtkWidget *cwl=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(cwl),CWL_BUTTON);
  gtk_widget_show(cwl);
  gtk_grid_attach(GTK_GRID(grid),cwl,2,11,1,1);

  GtkWidget *cwr_label=gtk_label_new("CWR");
  gtk_widget_show(cwr_label);
  gtk_grid_attach(GTK_GRID(grid),cwr_label,0,12,1,1);

  GtkWidget *cwr_gpio_label=gtk_label_new("GPIO:");
  gtk_widget_show(cwr_gpio_label);
  gtk_grid_attach(GTK_GRID(grid),cwr_gpio_label,1,12,1,1);

  GtkWidget *cwr=gtk_spin_button_new_with_range (0.0,100.0,1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(cwr),CWR_BUTTON);
  gtk_widget_show(cwr);
  gtk_grid_attach(GTK_GRID(grid),cwr,2,12,1,1);



  gtk_container_add(GTK_CONTAINER(content),grid);

  gtk_container_add(GTK_CONTAINER(content),grid);

  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Close",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 20"));
  gtk_widget_show_all(dialog);

  int result=gtk_dialog_run(GTK_DIALOG(dialog));

  ENABLE_VFO_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_vfo_encoder))?1:0;
  VFO_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vfo_a));
  VFO_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vfo_b));
  ENABLE_VFO_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_vfo_pullup))?1:0;
  ENABLE_AF_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_af_encoder))?1:0;
  AF_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(af_a));
  AF_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(af_b));
  ENABLE_AF_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_af_pullup))?1:0;
  ENABLE_RF_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_rf_encoder))?1:0;
  RF_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rf_a));
  RF_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rf_b));
  ENABLE_RF_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_rf_pullup))?1:0;
  ENABLE_AGC_ENCODER=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_agc_encoder))?1:0;
  AGC_ENCODER_A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(agc_a));
  AGC_ENCODER_B=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(agc_b));
  ENABLE_AGC_PULLUP=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_agc_pullup))?1:0;
  ENABLE_BAND_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_band))?1:0;
  BAND_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(band));
  ENABLE_MODE_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_mode))?1:0;
  MODE_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(mode));
  ENABLE_FILTER_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_filter))?1:0;
  FILTER_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(filter));
  ENABLE_NOISE_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_noise))?1:0;
  NOISE_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(noise));
  ENABLE_AGC_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_agc))?1:0;
  AGC_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(agc));
  ENABLE_MOX_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_mox))?1:0;
  MOX_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(mox));
  ENABLE_FUNCTION_BUTTON=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_enable_function))?1:0;
  FUNCTION_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(function));
  CWL_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cwl));
  CWR_BUTTON=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cwr));

  gtk_widget_destroy(dialog);

  gpio_save_state();
}
#endif

