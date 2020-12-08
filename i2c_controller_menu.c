#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "actions.h"
#include "i2c_controller.h"
#include "main.h"
#include "new_menu.h"

typedef struct _choice {
  int id;
  int action;
  GtkWidget *button;
} CHOICE;

static void response_event(GtkWidget *dialog,gint id,gpointer user_data) {
  g_print("%s: id=%d\n",__FUNCTION__,id);
  if(id==GTK_RESPONSE_ACCEPT) {
    g_print("%s: ACCEPT\n",__FUNCTION__);
  }
  gtk_widget_destroy(dialog);
  dialog=NULL;
  active_menu=NO_MENU;
  sub_menu=NULL;
}

static void encoder_select_cb(GtkWidget *widget,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  encoder[choice->id].encoder_function=choice->action;
  gtk_button_set_label(GTK_BUTTON(choice->button),encoder_string[choice->action]);
}

static gboolean encoder_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int encoder=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<ENCODER_ACTIONS;i++) {
    GtkWidget *menu_item=gtk_menu_item_new_with_label(encoder_string[i]);
    CHOICE *choice=g_new0(CHOICE,1);
    choice->id=encoder;
    choice->action=i;
    choice->button=widget;
    g_signal_connect(menu_item,"activate",G_CALLBACK(encoder_select_cb),choice);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
  }
  gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
  gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
// the following line of code is to work around the problem of the popup menu not having scroll bars.
  gtk_menu_reposition(GTK_MENU(menu));
#else
  gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
#endif

  return TRUE;
}

static void push_sw_select_cb(GtkWidget *widget, gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  encoder[choice->id].push_sw_function=choice->action;
  gtk_button_set_label(GTK_BUTTON(choice->button),sw_string[choice->action]);
}

static gboolean push_sw_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  int sw=GPOINTER_TO_INT(data);
  int i;

  GtkWidget *menu=gtk_menu_new();
  for(i=0;i<SWITCH_ACTIONS;i++) {
    GtkWidget *menu_item=gtk_menu_item_new_with_label(sw_string[i]);
    CHOICE *choice=g_new0(CHOICE,1);
    choice->id=sw;
    choice->action=i;
    choice->button=widget;
    g_signal_connect(menu_item,"activate",G_CALLBACK(push_sw_select_cb),choice);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
fprintf(stderr,"%d=%s\n",i,sw_string[i]);
  }
  gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
  gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
// the following line of code is to work around the problem of the popup menu not having scroll bars.
  gtk_menu_reposition(GTK_MENU(menu));
#else
  gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
#endif
  return TRUE;
}

void i2c_controller_menu(GtkWidget *parent_window) {
  gint row=0;
  gint col=0;

  GtkWidget *dialog=gtk_dialog_new_with_buttons("piHPSDR - I2C Controller",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,("OK"),GTK_RESPONSE_ACCEPT,"Cancel",GTK_RESPONSE_REJECT,NULL);

  g_signal_connect (dialog, "response", G_CALLBACK (response_event), NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *notebook=gtk_notebook_new();


  // Encoders
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),2);
  gtk_grid_set_row_spacing (GTK_GRID(grid),2);

  for(int i=0;i<MAX_I2C_ENCODERS;i++) {
    row=i%3;
    col=(i/3)*4;

    GtkWidget *address=gtk_label_new(NULL);
    gchar addr[16];
    g_sprintf(addr,"<b>0X%02X</b>",encoder[i].address);
    gtk_label_set_markup (GTK_LABEL(address), addr);
    gtk_grid_attach(GTK_GRID(grid),address,col,row,1,1);
    col++;

    GtkWidget *enable=gtk_check_button_new_with_label("Enable");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable), encoder[i].enabled);
    gtk_grid_attach(GTK_GRID(grid),enable,col,row,1,1);
    col++;

    GtkWidget *function=gtk_button_new_with_label(encoder_string[encoder[i].encoder_function]);
    g_signal_connect(function,"button_press_event",G_CALLBACK(encoder_cb),GINT_TO_POINTER(i));
    gtk_grid_attach(GTK_GRID(grid),function,col,row,1,1);
    col++;
  }
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid,gtk_label_new("Encoders"));

  // Encoder Push buttons
  grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),2);
  gtk_grid_set_row_spacing (GTK_GRID(grid),2);

  for(int i=0;i<MAX_I2C_ENCODERS;i++) {
    row=i%3;
    col=(i/3)*4;

    GtkWidget *address=gtk_label_new(NULL);
    gchar addr[16];
    g_sprintf(addr,"<b>0X%02X</b>",encoder[i].address);
    gtk_label_set_markup (GTK_LABEL(address), addr);
    gtk_grid_attach(GTK_GRID(grid),address,col,row,1,1);
    col++;

    GtkWidget *push_sw_enable=gtk_check_button_new();
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (push_sw_enable), encoder[i].push_sw_enabled);
    gtk_grid_attach(GTK_GRID(grid),push_sw_enable,col,row,1,1);
    col++;

    GtkWidget *push_sw_function=gtk_button_new_with_label(sw_string[encoder[i].push_sw_function]);
    g_signal_connect(push_sw_function,"button_press_event",G_CALLBACK(push_sw_cb),GINT_TO_POINTER(i));
    gtk_grid_attach(GTK_GRID(grid),push_sw_function,col,row,1,1);
    col++;
  }
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid,gtk_label_new("Encoder Switches"));

  gtk_container_add(GTK_CONTAINER(content),notebook);

  sub_menu=dialog;
  gtk_widget_show_all(dialog);
}

