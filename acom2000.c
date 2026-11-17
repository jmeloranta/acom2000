/*
 * GUI for ACOM 2000A control.
 *
 * Jussi Eloranta (AA6KJ) - eloranta@aa6kj.hopto.org
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <libserialport.h>
#include "extern.h"
#include "config.h"

GtkWidget *band, *fpower, *rpower, *swr, *temp, *button_oper;
struct sp_port *acom_port = NULL;
extern struct acom2000 acom2000;
char device_id[256]; // Persistent hardware identifier (or raw port path fallback)

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

/*
 * Builds a persistent hardware ID string for a given sp_port:
 *  - Priority 1: USB Serial Number -> "SN:<serial>"
 *  - Priority 2: USB VID:PID + Product -> "USB:<vid>:<pid>:<product>"
 *  - Priority 3: Fallback to OS port name -> "PORT:<name>"
 */
static void get_port_persistent_id(struct sp_port *port, char *out, size_t out_len) {
  const char *serial = sp_get_port_usb_serial(port);
  int vid = 0, pid = 0;

  if (serial != NULL && serial[0] != '\0') {
    snprintf(out, out_len, "SN:%s", serial);
    return;
  }

  if (sp_get_port_usb_vid_pid(port, &vid, &pid) == SP_OK && (vid != 0 || pid != 0)) {
    const char *prod = sp_get_port_usb_product(port);
    snprintf(out, out_len, "USB:%04x:%04x:%s", vid, pid, prod ? prod : "");
    return;
  }

  const char *name = sp_get_port_name(port);
  snprintf(out, out_len, "PORT:%s", name ? name : "");
}

/*
 * Builds a human-readable label for the dropdown list showing both
 * the OS port name and its hardware ID / USB details.
 */
static void format_port_display(struct sp_port *port, char *out, size_t out_len) {
  const char *name = sp_get_port_name(port);
  const char *desc = sp_get_port_description(port);
  const char *serial = sp_get_port_usb_serial(port);
  const char *mfg = sp_get_port_usb_manufacturer(port);
  int vid = 0, pid = 0;

  if (sp_get_port_usb_vid_pid(port, &vid, &pid) == SP_OK && (vid != 0 || pid != 0)) {
    snprintf(out, out_len, "%s — %s%s%s [VID:PID %04X:%04X | ID: %s]",
             name ? name : "Unknown",
             mfg ? mfg : "",
             (mfg && desc) ? " " : "",
             desc ? desc : "USB Serial",
             vid, pid,
             (serial && serial[0] != '\0') ? serial : "None");
  } else {
    snprintf(out, out_len, "%s — %s [Hardware Port]",
             name ? name : "Unknown",
             desc ? desc : "Serial Port");
  }
}

/*
 * Resolves a saved device_id (or legacy port path) back into an sp_port*.
 */
static struct sp_port *find_port_by_id(const char *target_id) {
  struct sp_port **ports = NULL;
  struct sp_port *matched = NULL;

  if (target_id == NULL || target_id[0] == '\0') return NULL;

  if (sp_list_ports(&ports) == SP_OK && ports != NULL) {
    char current_id[256];
    for (int i = 0; ports[i] != NULL; i++) {
      get_port_persistent_id(ports[i], current_id, sizeof(current_id));
      const char *raw_name = sp_get_port_name(ports[i]);

      if (strcmp(current_id, target_id) == 0 ||
          (raw_name && strcmp(raw_name, target_id) == 0)) {
        sp_copy_port(ports[i], &matched);
        break;
      }
    }
    sp_free_port_list(ports);
  }

  // Fallback for direct symlink paths (e.g., /dev/serial/by-id/... on Linux)
  if (matched == NULL && strncmp(target_id, "SN:", 3) != 0 &&
      strncmp(target_id, "USB:", 4) != 0 && strncmp(target_id, "PORT:", 5) != 0) {
    if (sp_get_port_by_name(target_id, &matched) != SP_OK) {
      matched = NULL;
    }
  }

  return matched;
}

static char *get_config_filepath(void) {

  const char *config_dir = g_get_user_config_dir();

  g_mkdir_with_parents(config_dir, 0755);
  return g_build_filename(config_dir, CONFIG, NULL);
}

void error_dialog_closed(GObject *src, GAsyncResult *res, gpointer data) {

  (void)data;

  GtkAlertDialog *dialog = GTK_ALERT_DIALOG(src);
  gtk_alert_dialog_choose_finish(dialog, res, NULL);
}

void show_err_dialog(const char *txt, GtkWindow *win) {

  GtkAlertDialog *dd = gtk_alert_dialog_new("Error");

  gtk_alert_dialog_set_message(dd, txt);
  gtk_alert_dialog_set_modal(dd, TRUE);
  gtk_alert_dialog_choose(dd, win, NULL, error_dialog_closed, NULL);
}

void cleanup(int x) {
  (void)x;
  if(acom_port != NULL) {
    stop_measurement(acom_port);
    if(acom2000.led_fwd == 0) set_standby(acom_port);
    sp_close(acom_port);
    sp_free_port(acom_port);
    acom_port = NULL;
  }
  exit(0);
}  

int amp_init(GtkWindow *win) {
  char *cfg_path = get_config_filepath();
  FILE *fp;
    
  if(device_id[0] == '\0') {
    if(!(fp = fopen(cfg_path, "r"))) {
      g_free(cfg_path);
      return -1;
    }
    if(!fgets(device_id, sizeof(device_id), fp)) {
      fclose(fp);
      g_free(cfg_path);
      device_id[0] = '\0';
      return -1;
    }
    fclose(fp);
    device_id[strcspn(device_id, "\r\n")] = '\0';
  }

  memset(&acom2000, 0, sizeof(struct acom2000));
  if(acom_port != NULL) {
    sp_close(acom_port);
    sp_free_port(acom_port);
    acom_port = NULL;
  }

  acom_port = find_port_by_id(device_id);
  if(acom_port == NULL) {
    g_free(cfg_path);
    return -1;
  }

  if(sp_open(acom_port, SP_MODE_READ_WRITE) != SP_OK) {
    sp_free_port(acom_port);
    acom_port = NULL;
    g_free(cfg_path);
    return -1;
  }

  if(!(fp = fopen(cfg_path, "w"))) {
    show_err_dialog("Can't write config file.", win);
  } else {
    fputs(device_id, fp);
    fclose(fp);
  }
  g_free(cfg_path);

  setup_tty(acom_port);
  set_standby(acom_port);

  if(acom2000.led_fwd > 0) set_standby(acom_port);
  request_band(acom_port);

  return 0;
}  

void doconnect(GtkWidget *button, gpointer data) {

  (void) data;
  GtkWindow *win = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));

  if(amp_init(win) < 0)
    show_err_dialog("Can't connect to amplifier. Check Port selection.", win);
}

/* --- Custom Cross-Platform Port Selection Dialog --- */

typedef struct {
  GtkWidget *dialog;
  GtkWidget *dropdown;
  GtkWindow *parent_win;
  char **ids;
  int count;
} PortDialogData;

static void free_port_dialog_data(PortDialogData *pdata) {

  if (!pdata) return;
  for (int i = 0; i < pdata->count; i++)
    g_free(pdata->ids[i]);
  g_free(pdata->ids);
  g_free(pdata);
}

static void on_port_dialog_cancel(GtkWidget *btn, gpointer user_data) {

  (void) btn;
  PortDialogData *pdata = (PortDialogData *)user_data;

  gtk_window_destroy(GTK_WINDOW(pdata->dialog));
  free_port_dialog_data(pdata);
}

static void on_port_dialog_ok(GtkWidget *btn, gpointer user_data) {

  (void) btn;
  PortDialogData *pdata = (PortDialogData *) user_data;
  guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(pdata->dropdown));

  if (selected != GTK_INVALID_LIST_POSITION && (int)selected < pdata->count) {
    strncpy(device_id, pdata->ids[selected], sizeof(device_id) - 1);
    device_id[sizeof(device_id) - 1] = '\0';
  }

  GtkWindow *parent = pdata->parent_win;
  gtk_window_destroy(GTK_WINDOW(pdata->dialog));
  free_port_dialog_data(pdata);

  if (amp_init(parent) < 0) {
    show_err_dialog("Selected port could not be opened.", parent);
  }
}

void config(GtkWidget *button, gpointer data) {

  (void) data;
  GtkWindow *win = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
  struct sp_port **ports = NULL;
  GtkWidget *dialog, *vbox, *lbl, *dropdown, *hbox, *btn_cancel, *btn_ok;
  int i, count;
  guint active_idx = 0;
  PortDialogData *pdata;
  const char **labels;
  char **allocated_labels;
  
  if (sp_list_ports(&ports) != SP_OK || ports == NULL || ports[0] == NULL) {
    if (ports) sp_free_port_list(ports);
    show_err_dialog("No serial ports detected on this system.", win);
    return;
  }

  count = 0;
  while (ports[count] != NULL) count++;

  pdata = g_new0(PortDialogData, 1);
  pdata->parent_win = win;
  pdata->count = count;
  pdata->ids = g_new0(char *, count);

  labels = g_new0(const char *, count + 1);
  allocated_labels = g_new0(char *, count);

  for (i = 0; i < count; i++) {
    char id_buf[256];
    char disp_buf[512];

    get_port_persistent_id(ports[i], id_buf, sizeof(id_buf));
    format_port_display(ports[i], disp_buf, sizeof(disp_buf));

    pdata->ids[i] = g_strdup(id_buf);
    allocated_labels[i] = g_strdup(disp_buf);
    labels[i] = allocated_labels[i];

    if (device_id[0] != '\0' && strcmp(device_id, id_buf) == 0)
      active_idx = (guint) i;
  }
  labels[count] = NULL;
  sp_free_port_list(ports);

  dialog = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Select Serial Device");
  gtk_window_set_transient_for(GTK_WINDOW(dialog), win);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 130);
  pdata->dialog = dialog;

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_margin_top(vbox, 12);
  gtk_widget_set_margin_bottom(vbox, 12);
  gtk_widget_set_margin_start(vbox, 12);
  gtk_widget_set_margin_end(vbox, 12);

  lbl = gtk_label_new("Select Serial Adapter (Port & Hardware ID):");
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);

  dropdown = gtk_drop_down_new_from_strings(labels);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), active_idx);
  pdata->dropdown = dropdown;

  for (i = 0; i < count; i++)
    g_free(allocated_labels[i]);
  g_free(allocated_labels);
  g_free(labels);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(hbox, GTK_ALIGN_END);
  btn_cancel = gtk_button_new_with_label("Cancel");
  btn_ok = gtk_button_new_with_label("Save & Connect");

  g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_port_dialog_cancel), pdata);
  g_signal_connect(btn_ok, "clicked", G_CALLBACK(on_port_dialog_ok), pdata);

  gtk_box_append(GTK_BOX(hbox), btn_cancel);
  gtk_box_append(GTK_BOX(hbox), btn_ok);

  gtk_box_append(GTK_BOX(vbox), lbl);
  gtk_box_append(GTK_BOX(vbox), dropdown);
  gtk_box_append(GTK_BOX(vbox), hbox);
  gtk_window_set_child(GTK_WINDOW(dialog), vbox);
  gtk_window_present(GTK_WINDOW(dialog));
}

void operate(GtkWidget *button, gpointer data) {

  (void) button;
  (void) data;

  // Do not turn to oper/stdby if there is indication of drive present (trips the amp)
  if(acom2000.led_fwd > 0) return;
  if(acom2000.opr_stby == 0) {
    set_operate(acom_port);
    start_measurement(acom_port, 'i', 't'); // forward power(i) and reflected power(t)
  } else {
    set_standby(acom_port);
    stop_measurement(acom_port);
  }
}

static gboolean update_power(void *asd) {

  (void) asd;
  char buf[128];
  double cswr, gamma;

  if(acom2000.address > 0) gtk_widget_set_sensitive(button_oper, TRUE);
  process_data(acom_port);
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

  (void) data;
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
#ifdef ICON_DIR
  gtk_icon_theme_add_search_path(icon_theme, ICON_DIR);
#endif
  gtk_window_set_default_icon_name ("acom2000");

  band_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(band_label), "Band");
  gtk_label_set_xalign(GTK_LABEL(band_label), 0.0);
  band = gtk_label_new("----");
  gtk_widget_set_margin_start(GTK_WIDGET(band), 50);
  gtk_label_set_xalign(GTK_LABEL(band), 0.0);

  fpower_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(fpower_label), "Forward power");
  gtk_label_set_xalign(GTK_LABEL(fpower_label), 0.0);
  fpower = gtk_label_new("0.0 W");
  gtk_widget_set_margin_start(GTK_WIDGET(fpower), 50);
  gtk_label_set_xalign(GTK_LABEL(fpower), 0.0);

  rpower_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rpower_label), "Reflected power");
  gtk_label_set_xalign(GTK_LABEL(rpower_label), 0.0);
  rpower = gtk_label_new("0.0 W");
  gtk_widget_set_margin_start(GTK_WIDGET(rpower), 50);
  gtk_label_set_xalign(GTK_LABEL(rpower), 0.0);

  swr_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(swr_label), "SWR");
  gtk_label_set_xalign(GTK_LABEL(swr_label), 0.0);
  swr = gtk_label_new("---");
  gtk_widget_set_margin_start(GTK_WIDGET(swr), 50);
  gtk_label_set_xalign(GTK_LABEL(swr), 0.0);

  temp_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(temp_label), "Temperature");
  gtk_label_set_xalign(GTK_LABEL(temp_label), 0.0);
  temp = gtk_label_new("---");
  gtk_widget_set_margin_start(GTK_WIDGET(temp), 50);
  gtk_label_set_xalign(GTK_LABEL(temp), 0.0);

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
  gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
  gtk_widget_set_margin_top(grid, 1);
  gtk_widget_set_margin_bottom(grid, 1);
  gtk_widget_set_margin_start(grid, 1);
  gtk_widget_set_margin_end(grid, 1);

  gtk_grid_attach(GTK_GRID(grid), band_label, 0, 0, 1, 1);
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
					     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
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
  device_id[0] = '\0';
  app = gtk_application_new("org.hopto.aa6kj.acom2000", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(startup_app), NULL);
  g_timeout_add(UPDATE, update_power, NULL);
  g_application_run(G_APPLICATION(app), argc, argv);
  cleanup(1);
}

