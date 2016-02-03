#include <gtk/gtk.h>

GtkWidget *splash_window;

/* Close the splash screen */
void splash_close()
{
  gtk_widget_destroy(splash_window);
}


void splash_show(char* image_name,int time,int width,int height)
{
  GtkWidget  *image;
  splash_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request(splash_window, width, height);
  gtk_window_set_decorated(GTK_WINDOW(splash_window), FALSE);
  gtk_window_set_position(GTK_WINDOW(splash_window),GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable(GTK_WINDOW(splash_window), FALSE);
  image=gtk_image_new_from_file(image_name);
  gtk_container_add(GTK_CONTAINER(splash_window), image);
  gtk_widget_show_all (splash_window);
}
