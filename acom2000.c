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

#define UPDATE 50  // in milliseconds

GtkWidget *band, *fpower, *rpower, *swr, *temp, *button_oper;
int acom_fd;
extern struct acom2000 acom2000;

const char *css = 
  ".btn-red {"
  "  background-color: #e01b24;"
  "  color: white;"
  "  background-image: none;"
  "}"
  ".btn-yellow {"
  "  background-color: #f6d32d;"
  "  color: black;"
  "  background-image: none;"
  "}";

void cleanup(int x) {

  stop_measurement(acom_fd);
  close(acom_fd);
  exit(0);
}  

void operate(GtkWidget *win, gpointer data) {

  if(acom2000.led_fwd > 0) return; // Do not turn to oper if there is indication of drive present (trips the amp)
  if(acom2000.opr_stby == 0) {
    set_operate(acom_fd);
    start_measurement(acom_fd, 'i', 't'); // forward power(i) and reflected power(t)
  } else {
    set_standby(acom_fd);
    stop_measurement(acom_fd);
  }
}

static gboolean update_power(void *asd) {

  char buf[128];
  double cswr, gamma;

  process_data(acom_fd);
  switch(acom2000.band) {
  case 1:
    gtk_label_set_text(GTK_LABEL(band), "160");
    break;
  case 2:
    gtk_label_set_text(GTK_LABEL(band), "80");
    break;
  case 3:
    gtk_label_set_text(GTK_LABEL(band), "40");
    break;
  case 4:
    gtk_label_set_text(GTK_LABEL(band), "30");
    break;
  case 5:
    gtk_label_set_text(GTK_LABEL(band), "20");
    break;
  case 6:
    gtk_label_set_text(GTK_LABEL(band), "17");
    break;
  case 7:
    gtk_label_set_text(GTK_LABEL(band), "15");
    break;
  case 8:
    gtk_label_set_text(GTK_LABEL(band), "12");
    break;
  case 9:
    gtk_label_set_text(GTK_LABEL(band), "10");
    break;
  default:
    gtk_label_set_text(GTK_LABEL(band), "----");
  }
  if(acom2000.rcu_active) {
    gtk_label_set_text(GTK_LABEL(fpower), "-----");
    gtk_label_set_text(GTK_LABEL(rpower), "-----");
    return 1;
  }
  sprintf(buf, "%.0lf W", acom2000.fwd_power);
  gtk_label_set_text(GTK_LABEL(fpower), buf);
  sprintf(buf, "%.0lf W", acom2000.refl_power);
  gtk_label_set_text(GTK_LABEL(rpower), buf);
  if(acom2000.refl_power > 0.0 && acom2000.fwd_power > 0.0) {
    gamma = sqrt(acom2000.refl_power / acom2000.fwd_power);
    cswr = (1.0 + gamma) / (1 - gamma);
    if(cswr < 1.0) sprintf(buf, "----");
    else sprintf(buf, "%.1lf", cswr);
  } else sprintf(buf, "----");
  gtk_label_set_text(GTK_LABEL(swr), buf);
  if(acom2000.opr_stby == 1) {
    gtk_button_set_label(GTK_BUTTON(button_oper), "Now ON");
    gtk_widget_remove_css_class(GTK_WIDGET(button_oper), "btn-yellow");
    gtk_widget_add_css_class(button_oper, "btn-red");
  } else {
    gtk_button_set_label(GTK_BUTTON(button_oper), "Now OFF");
    gtk_widget_remove_css_class(GTK_WIDGET(button_oper), "btn-red");
    gtk_widget_add_css_class(button_oper, "btn-yellow");
  }
  sprintf(buf, "%.0lf C", acom2000.amp_temp);
  gtk_label_set_text(GTK_LABEL(temp), buf);
  
  return 1;
}


int main(int argc, char *argv[]) {

  GtkWidget *window, *hbox1, *hbox2, *hbox3, *hbox4, *hbox5, *vbox;
  GtkWidget *band_label, *fpower_label, *rpower_label, *swr_label, *temp_label;
  GtkCssProvider *prov;
    
  signal(SIGINT, cleanup);
  gtk_init();
  window = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(window), "ACOM 2000A");
  gtk_window_set_default_size(GTK_WINDOW(window), 200, 120);

  band_label = gtk_label_new("Current band: ");
  gtk_label_set_xalign(GTK_LABEL(band_label), 0.0); // 0 = left, 1 = right
  band = gtk_label_new("----");
  gtk_label_set_xalign(GTK_LABEL(band), 0.0); // 0 = left, 1 = right
  
  fpower_label = gtk_label_new("Forward power (W): ");
  gtk_label_set_xalign(GTK_LABEL(fpower_label), 0.0); // 0 = left, 1 = right
  fpower = gtk_label_new("0.0 W");
  gtk_label_set_xalign(GTK_LABEL(fpower), 0.0); // 0 = left, 1 = right

  rpower_label = gtk_label_new("Reflected power (W): ");
  gtk_label_set_xalign(GTK_LABEL(rpower_label), 0.0); // 0 = left, 1 = right
  rpower = gtk_label_new("0.0 W");
  gtk_label_set_xalign(GTK_LABEL(rpower), 0.0); // 0 = left, 1 = right

  swr_label = gtk_label_new("SWR               : ");
  gtk_label_set_xalign(GTK_LABEL(swr_label), 0.0); // 0 = left, 1 = right
  swr = gtk_label_new("---");
  gtk_label_set_xalign(GTK_LABEL(swr), 0.0); // 0 = left, 1 = right

  temp_label = gtk_label_new("Temperature: ");
  gtk_label_set_xalign(GTK_LABEL(temp_label), 0.0); // 0 = left, 1 = right
  temp = gtk_label_new("---");
  gtk_label_set_xalign(GTK_LABEL(temp), 0.0); // 0 = left, 1 = right
  
  hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  hbox4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  hbox5 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

  button_oper = gtk_button_new_with_label("Now OFF");

  gtk_box_append(GTK_BOX(hbox1), band_label);
  gtk_box_append(GTK_BOX(hbox1), band);

  gtk_box_append(GTK_BOX(hbox2), fpower_label);
  gtk_box_append(GTK_BOX(hbox2), fpower);
  
  gtk_box_append(GTK_BOX(hbox3), rpower_label);
  gtk_box_append(GTK_BOX(hbox3), rpower);
 
  gtk_box_append(GTK_BOX(hbox4), swr_label);
  gtk_box_append(GTK_BOX(hbox4), swr);

  gtk_box_append(GTK_BOX(hbox5), temp_label);
  gtk_box_append(GTK_BOX(hbox5), temp);
  
  gtk_box_append(GTK_BOX(vbox), hbox1);
  gtk_box_append(GTK_BOX(vbox), hbox2);
  gtk_box_append(GTK_BOX(vbox), hbox3);
  gtk_box_append(GTK_BOX(vbox), hbox4);
  gtk_box_append(GTK_BOX(vbox), hbox5);
  gtk_box_append(GTK_BOX(vbox), button_oper);

  gtk_window_set_child(GTK_WINDOW(window), vbox);

  prov = gtk_css_provider_new();
  gtk_css_provider_load_from_string(prov, css);  
  gtk_style_context_add_provider_for_display(gdk_display_get_default(),
					     GTK_STYLE_PROVIDER(prov),
					     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
					    );
  g_object_unref(prov);
  gtk_widget_add_css_class(button_oper, "btn-yellow");
  
  g_signal_connect(button_oper, "clicked", G_CALLBACK(operate), NULL);

  gtk_widget_set_visible(window, TRUE);

  bzero(&acom2000, sizeof(struct acom2000));
  if((acom_fd = open(SERIAL_PORT, O_RDWR)) < 0) {
    fprintf(stderr, "Can't open device.\n");
    exit(1);
  }
  setup_tty(acom_fd);
  set_standby(acom_fd);
  
  g_timeout_add(UPDATE, update_power, NULL);

  request_band(acom_fd);
  while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0) // Main iteration loop
    g_main_context_iteration(NULL, TRUE);

  cleanup(1);
}
