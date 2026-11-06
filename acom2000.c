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
#define SWR_WARN 2.0 // warn above this SWR

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

  if(acom2000.led_fwd > 0) return; // Do not turn to oper/stdby if there is indication of drive present (trips the amp)
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
    if(cswr > SWR_WARN) gtk_widget_add_css_class(GTK_WIDGET(swr), "btn-red");
    else gtk_widget_remove_css_class(GTK_WIDGET(swr), "btn-red");
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

  GtkWidget *window, *grid, *vbox;
  GtkWidget *band_label, *fpower_label, *rpower_label, *swr_label, *temp_label;
  GtkCssProvider *prov;
    
  signal(SIGINT, cleanup);
  gtk_init();
  window = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(window), "ACOM 2000A");
  gtk_window_set_default_size(GTK_WINDOW(window), 200, 120);

  band_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(band_label), "<b>Band</b>");
  gtk_label_set_xalign(GTK_LABEL(band_label), 0.0); // 0 = left, 1 = right
  band = gtk_label_new("----");
  gtk_label_set_xalign(GTK_LABEL(band), 0.0); // 0 = left, 1 = right
  
  fpower_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(fpower_label), "<b>Forward power (W)</b>");
  gtk_label_set_xalign(GTK_LABEL(fpower_label), 0.0); // 0 = left, 1 = right
  fpower = gtk_label_new("0.0 W");
  gtk_label_set_xalign(GTK_LABEL(fpower), 0.0); // 0 = left, 1 = right

  rpower_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rpower_label), "<b>Reflected power (W)</b>");
  gtk_label_set_xalign(GTK_LABEL(rpower_label), 0.0); // 0 = left, 1 = right
  rpower = gtk_label_new("0.0 W");
  gtk_label_set_xalign(GTK_LABEL(rpower), 0.0); // 0 = left, 1 = right

  swr_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(swr_label), "<b>SWR</b>");
  gtk_label_set_xalign(GTK_LABEL(swr_label), 0.0); // 0 = left, 1 = right
  swr = gtk_label_new("---");
  gtk_label_set_xalign(GTK_LABEL(swr), 0.0); // 0 = left, 1 = right

  temp_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(temp_label), "<b>Temperature</b>");
  gtk_label_set_xalign(GTK_LABEL(temp_label), 0.0); // 0 = left, 1 = right
  temp = gtk_label_new("---");
  gtk_label_set_xalign(GTK_LABEL(temp), 0.0); // 0 = left, 1 = right
  
  button_oper = gtk_button_new_with_label("Now OFF");

  grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(grid), 10); // space between columns
  gtk_grid_set_row_spacing(GTK_GRID(grid), 2);    // space between rows    
  gtk_widget_set_margin_top(grid, 1);
  gtk_widget_set_margin_bottom(grid, 1);
  gtk_widget_set_margin_start(grid, 1);
  gtk_widget_set_margin_end(grid, 1);
  
  gtk_grid_attach(GTK_GRID(grid), band_label, 0, 0, 1, 1); // col, row, width, height
  gtk_grid_attach(GTK_GRID(grid), band, 1, 0, 1, 1);

  gtk_grid_attach(GTK_GRID(grid), fpower_label, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), fpower, 1, 1, 1, 1);

  gtk_grid_attach(GTK_GRID(grid), rpower_label, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), rpower, 1, 2, 1, 1);

  gtk_grid_attach(GTK_GRID(grid), swr_label, 0, 3, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), swr, 1, 3, 1, 1);

  gtk_grid_attach(GTK_GRID(grid), temp_label, 0, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), temp, 1, 4, 1, 1);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_append(GTK_BOX(vbox), grid);
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

  if(acom2000.led_fwd > 0) set_standby(acom_fd);
  request_band(acom_fd);

  while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0) // Main iteration loop
    g_main_context_iteration(NULL, TRUE);

  cleanup(1);
}
