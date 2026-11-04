/*
 * GUI for ACOM 2000A control.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <fcntl.h>
#include "extern.h"
#include "config.h"

#define UPDATE 50

GtkWidget *fpower, *rpower, *button_oper;
int acom_fd, oper_mode = 0;
extern struct acom2000 acom2000;

void cleanup(int x) {

  stop_measurement(acom_fd);
  close(acom_fd);
  exit(0);
}  

void operate(GtkWidget *win, gpointer data) {

  if(oper_mode == 0) {
    gtk_button_set_label(GTK_BUTTON(button_oper), "OFF");
    set_operate(acom_fd);
    oper_mode = 1;
  } else {
    gtk_button_set_label(GTK_BUTTON(button_oper), "ON");
    set_standby(acom_fd);
    oper_mode = 0;
  }
}

static gboolean update_power(void *asd) {

  char buf[128];

  process_data(acom_fd);
  sprintf(buf, "%.0lf W", acom2000.fwd_power);
  gtk_label_set_text(GTK_LABEL(fpower), buf);
  sprintf(buf, "%.0lf W", acom2000.refl_power);
  gtk_label_set_text(GTK_LABEL(rpower), buf);
  return 1;
}


int main(int argc, char *argv[]) {

  GtkWidget *window, *hbox1, *hbox2, *vbox;
  GtkWidget *fpower_label, *rpower_label;
  
  signal(SIGINT, cleanup);
  gtk_init();
  window = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(window), "ACOM 2000A");
  gtk_window_set_default_size(GTK_WINDOW(window), 100, 80);

  fpower_label = gtk_label_new("Forward power (W): ");
  gtk_label_set_xalign(GTK_LABEL(fpower_label), 0.0); // 0 = left, 1 = right
  fpower = gtk_label_new("0.0 W");
  gtk_label_set_xalign(GTK_LABEL(fpower), 0.0); // 0 = left, 1 = right

  rpower_label = gtk_label_new("Reflected power (W): ");
  gtk_label_set_xalign(GTK_LABEL(rpower_label), 0.0); // 0 = left, 1 = right
  rpower = gtk_label_new("0.0 W");
  gtk_label_set_xalign(GTK_LABEL(rpower), 0.0); // 0 = left, 1 = right

  hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

  button_oper = gtk_button_new_with_label("ON");

  gtk_box_append(GTK_BOX(hbox1), fpower_label);
  gtk_box_append(GTK_BOX(hbox1), fpower);
  
  gtk_box_append(GTK_BOX(hbox2), rpower_label);
  gtk_box_append(GTK_BOX(hbox2), rpower);
 
  gtk_box_append(GTK_BOX(vbox), hbox1);
  gtk_box_append(GTK_BOX(vbox), hbox2);
  gtk_box_append(GTK_BOX(vbox), button_oper);

  gtk_window_set_child(GTK_WINDOW(window), vbox);

  g_signal_connect(button_oper, "clicked", G_CALLBACK(operate), NULL);

  gtk_widget_set_visible(window, TRUE);

  if((acom_fd = open(SERIAL_PORT, O_RDWR)) < 0) {
    fprintf(stderr, "Can't open device.\n");
    exit(1);
  }
  setup_tty(acom_fd);
  set_standby(acom_fd);
  oper_mode = 0;
  
  start_measurement(acom_fd, 'i', 't'); // forward power(i) and reflected power(t)
  g_timeout_add(UPDATE, update_power, NULL);

  while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0) // Main iteration loop
    g_main_context_iteration(NULL, TRUE);

  cleanup(1);
}
