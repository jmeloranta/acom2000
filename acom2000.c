/*
 * GUI for ACOM 2000A control.
 *
 * Jussi Eloranta (AA6KJ) - eloranta@aa6kj.hopto.org
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

GtkWidget *band, *fpower, *rpower, *swr, *temp, *button_oper;
int acom_fd = -1;
extern struct acom2000 acom2000;
char serial_port[256]; // serial device path

const char *css = 
  ".btn-red {"
  "  background: red;"
  "  color: black;"
  "}"
  ".btn-yellow {"
  "  background: yellow;"
  "  color: black;"
  "}"
  ".btn-yellow:disabled {"
  "   background: #d3d3d3;"
  "   color: #888888;"
  "}";

void error_dialog_closed(GObject *src, GAsyncResult *res, gpointer data) {

  GtkAlertDialog *dialog = GTK_ALERT_DIALOG(src);
  GtkApplication *app = GTK_APPLICATION(data);
    
  gtk_alert_dialog_choose_finish(dialog, res, NULL);
  g_application_quit(G_APPLICATION(app));
}

void show_err_dialog(char *txt, GtkWindow *win) {

  GtkAlertDialog *dd;
  GtkApplication *app;

  dd = gtk_alert_dialog_new("Error");
  app = gtk_window_get_application(win);
  gtk_alert_dialog_set_message(dd, txt);
  gtk_alert_dialog_set_modal(dd, TRUE);
  gtk_alert_dialog_choose(dd, win, NULL, error_dialog_closed, app);
  
}

void cleanup(int x) {

  if(acom_fd != -1) {
    stop_measurement(acom_fd);
    if(acom2000.led_fwd == 0) set_standby(acom_fd);
    close(acom_fd);
  }
  exit(0);
}  

int amp_init(GtkWindow *win) {

  char *home, buf[256];
  FILE *fp;
    
  if(!(home = getenv("HOME")))
    show_err_dialog("Can't get home directory.", win);
  sprintf(buf, "%s/%s", home, CONFIG);
  
  if(serial_port[0] == '\0') {
    if(!(fp = fopen(buf, "r"))) return -1;
    if(!fgets(serial_port, sizeof(serial_port), fp)) {
      fclose(fp);
      serial_port[0] = '\0';
      return -1;
    }
    fclose(fp);
  }

  bzero(&acom2000, sizeof(struct acom2000));
  if(acom_fd != -1) close(acom_fd);
  if((acom_fd = open(serial_port, O_RDWR)) < 0) return -1;

  if(!(fp = fopen(buf, "w")))
    show_err_dialog("Can't write config file.", win);
  fputs(serial_port, fp);
  fclose(fp);

  setup_tty(acom_fd);
  set_standby(acom_fd);

  if(acom2000.led_fwd > 0) set_standby(acom_fd);
  request_band(acom_fd);

  return 0;
}  

void doconnect(GtkWidget *button, gpointer data) {

  GtkWindow *win = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
  if(amp_init(win) < 0)
    show_err_dialog("Can't connect to amplifier (Port).", win);
}

void dialog_callback(GObject *src, GAsyncResult *result, gpointer data) {

  GtkFileDialog *dialog;
  GError *err = NULL;
  GFile *file;
  GtkApplication *app = GTK_APPLICATION(data);
  
  dialog = GTK_FILE_DIALOG(src);
  file = gtk_file_dialog_open_finish(dialog, result, &err);
  if(file != NULL) {
    char *path;
    path = g_file_get_path(file);
    strcpy(serial_port, path);
    g_free(path);
    g_object_unref(file);
  } else g_application_quit(G_APPLICATION(app)); // cancel
}

void config(GtkWidget *button, gpointer data) {

  GtkFileDialog *dialog;
  GFile *devices;
  GtkWindow *win;
  
  win = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
  dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, "Select serial device");
  gtk_file_dialog_set_modal(dialog, TRUE);  
  if(!g_file_test(SERIAL_PORTS, G_FILE_TEST_IS_DIR))
    show_err_dialog("No serial ports present in the system.", win);

  devices = g_file_new_for_path(SERIAL_PORTS);
  gtk_file_dialog_set_initial_folder (dialog, devices);
  gtk_file_dialog_open(dialog, GTK_WINDOW(win), NULL, dialog_callback, (gpointer *) gtk_window_get_application(win));
  g_object_unref(dialog);
  g_object_unref(devices);
}

void operate(GtkWidget *button, gpointer data) {

  // Do not turn to oper/stdby if there is indication of drive present (trips the amp)
  if(acom2000.led_fwd > 0) return;
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

  if(acom2000.address > 0) gtk_widget_set_sensitive(button_oper, TRUE);
  process_data(acom_fd);
  switch(acom2000.band) {
  case 1:
    gtk_label_set_text(GTK_LABEL(band), "160 m");
    break;
  case 2:
    gtk_label_set_text(GTK_LABEL(band), "80 m");
    break;
  case 3:
    gtk_label_set_text(GTK_LABEL(band), "40 m");
    break;
  case 4:
    gtk_label_set_text(GTK_LABEL(band), "30 m");
    break;
  case 5:
    gtk_label_set_text(GTK_LABEL(band), "20 m");
    break;
  case 6:
    gtk_label_set_text(GTK_LABEL(band), "17 m");
    break;
  case 7:
    gtk_label_set_text(GTK_LABEL(band), "15 m");
    break;
  case 8:
    gtk_label_set_text(GTK_LABEL(band), "12 m");
    break;
  case 9:
    gtk_label_set_text(GTK_LABEL(band), "10 m");
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
    cswr = (1.0 + gamma) / (1.0 - gamma);
    if(cswr < 1.0) sprintf(buf, "----");
    else sprintf(buf, "%.1lf", cswr);
    if(cswr > SWR_WARN) gtk_widget_add_css_class(GTK_WIDGET(swr), "btn-red");
    else gtk_widget_remove_css_class(GTK_WIDGET(swr), "btn-red");
  } else {
    sprintf(buf, "----");
    gtk_widget_remove_css_class(GTK_WIDGET(swr), "btn-red");
  }
  gtk_label_set_text(GTK_LABEL(swr), buf);

  if(acom2000.opr_stby == 1) {
    gtk_button_set_label(GTK_BUTTON(button_oper), "OPER");
    gtk_widget_remove_css_class(GTK_WIDGET(button_oper), "btn-yellow");
    gtk_widget_add_css_class(button_oper, "btn-red");
  } else {
    gtk_button_set_label(GTK_BUTTON(button_oper), "STBY");
    gtk_widget_remove_css_class(GTK_WIDGET(button_oper), "btn-red");
    gtk_widget_add_css_class(button_oper, "btn-yellow");
  }

  sprintf(buf, "%.0lf C", acom2000.amp_temp);
  gtk_label_set_text(GTK_LABEL(temp), buf);
  
  return 1;
}

static void startup_app(GtkApplication *app, gpointer data) {

  GtkWidget *window, *vbox, *hbox, *grid;
  GtkWidget *band_label, *fpower_label, *rpower_label, *swr_label, *temp_label, *button_config, *button_connect;
  GtkCssProvider *prov;
  GdkDisplay *display;
  GtkIconTheme *icon_theme;

  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "ACOM 2000A");
  gtk_window_set_default_size(GTK_WINDOW(window), 200, 120);
  display = gtk_widget_get_display(window);
  icon_theme = gtk_icon_theme_get_for_display(display);
  gtk_icon_theme_add_search_path(icon_theme, ICON_DIR);
  gtk_window_set_default_icon_name ("acom2000");
  
  band_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(band_label), "<b>Band</b>");
  gtk_label_set_xalign(GTK_LABEL(band_label), 0.0); // 0 = left, 1 = right
  band = gtk_label_new("----");
  gtk_widget_set_margin_start(GTK_WIDGET(band), 50);
  gtk_label_set_xalign(GTK_LABEL(band), 0.0); // 0 = left, 1 = right
  
  fpower_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(fpower_label), "<b>Forward power</b>");
  gtk_label_set_xalign(GTK_LABEL(fpower_label), 0.0); // 0 = left, 1 = right
  fpower = gtk_label_new("0.0 W");
  gtk_widget_set_margin_start(GTK_WIDGET(fpower), 50);
  gtk_label_set_xalign(GTK_LABEL(fpower), 0.0); // 0 = left, 1 = right

  rpower_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rpower_label), "<b>Reflected power</b>");
  gtk_label_set_xalign(GTK_LABEL(rpower_label), 0.0); // 0 = left, 1 = right
  rpower = gtk_label_new("0.0 W");
  gtk_widget_set_margin_start(GTK_WIDGET(rpower), 50);
  gtk_label_set_xalign(GTK_LABEL(rpower), 0.0); // 0 = left, 1 = right

  swr_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(swr_label), "<b>SWR</b>");
  gtk_label_set_xalign(GTK_LABEL(swr_label), 0.0); // 0 = left, 1 = right
  swr = gtk_label_new("---");
  gtk_widget_set_margin_start(GTK_WIDGET(swr), 50);
  gtk_label_set_xalign(GTK_LABEL(swr), 0.0); // 0 = left, 1 = right

  temp_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(temp_label), "<b>Temperature</b>");
  gtk_label_set_xalign(GTK_LABEL(temp_label), 0.0); // 0 = left, 1 = right
  temp = gtk_label_new("---");
  gtk_widget_set_margin_start(GTK_WIDGET(temp), 50);
  gtk_label_set_xalign(GTK_LABEL(temp), 0.0); // 0 = left, 1 = right
  
  button_oper = gtk_button_new_with_label("STBY");
  gtk_widget_set_sensitive(button_oper, FALSE);
  gtk_widget_set_size_request(button_oper, 90, -1);
  button_config = gtk_button_new_with_label("Port");
  gtk_widget_set_size_request(button_config, 90, -1);
  button_connect = gtk_button_new_with_label("Connect");
  gtk_widget_set_size_request(button_connect, 90, -1);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  
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

  gtk_box_append(GTK_BOX(hbox), button_oper);
  gtk_widget_set_sensitive(button_oper, FALSE);
  gtk_box_append(GTK_BOX(hbox), button_connect);
  gtk_box_append(GTK_BOX(hbox), button_config);
  
  gtk_box_append(GTK_BOX(vbox), grid);
  gtk_box_append(GTK_BOX(vbox), hbox);
  
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
  g_signal_connect(button_config, "clicked", G_CALLBACK(config), NULL);
  g_signal_connect(button_connect, "clicked", G_CALLBACK(doconnect), NULL);
  gtk_widget_set_visible(window, TRUE);

  amp_init(GTK_WINDOW(window));
}
  
int main(int argc, char *argv[]) {

  GtkApplication *app;
  
  signal(SIGINT, cleanup);
  serial_port[0] = '\0';

  app = gtk_application_new("org.hopto.aa6kj.acom2000", G_APPLICATION_DEFAULT_FLAGS);

  g_signal_connect (app, "activate", G_CALLBACK (startup_app), NULL);
  
  g_timeout_add(UPDATE, update_power, NULL);

  g_application_run (G_APPLICATION (app), argc, argv);  

  cleanup(1);
}
