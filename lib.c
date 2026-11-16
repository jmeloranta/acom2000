/* 
 * Library to interface with ACOM 2000A via RS232 using libserialport.
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libserialport.h>
#include "extern.h"
#include "config.h"

#define MAXLEN 255
#define DELAY 20000     // RS232 read/write delay in microseconds (20 ms)

static int debug = 0;     // Message debug prints (0 = off, 1 = on)

struct acom2000 acom2000;

void setup_tty(struct sp_port *port) {
  
  if (sp_set_baudrate(port, 1200) != SP_OK ||
      sp_set_bits(port, 8) != SP_OK ||
      sp_set_parity(port, SP_PARITY_NONE) != SP_OK ||
      sp_set_stopbits(port, 1) != SP_OK ||
      sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE) != SP_OK) {
    fprintf(stderr, "Serial port configuration failed.\n");
    exit(1);
  }
}

static int read_until_null(struct sp_port *port, uint8_t *buf, int maxlen) {
  
  int pos = 0, bytes_rd;
  
  while(1) {
    bytes_rd = sp_nonblocking_read(port, buf + pos, 1);
    if(bytes_rd < 0) return -1;
    if(bytes_rd == 0) {
      usleep(DELAY);
      continue;
    }
    if(pos == 0 && buf[0] >= 0x80 && buf[0] <= 0xaf) { // single byte data
      buf[1] = '\0';
      return 1;
    }
    if(pos >= maxlen - 1) return -1;
    if(buf[pos] == '\0') break;
    pos++;
  }
  return pos;
}

static int read_cmd(struct sp_port *port, uint8_t *src, uint8_t *dst, uint8_t *msg) {

  uint8_t buf[MAXLEN];

  for(buf[0] = buf[1] = 0; buf[0] == 0 && buf[1] == 0; ) { // eat up broken messages at the start
    if(read_until_null(port, buf, MAXLEN) < 0) return -1;
  }
  if(strlen((char *)buf) == 1) {
    *dst = *src = 0;
    msg[0] = buf[0];
    msg[1] = buf[1];
  } else {
    *dst = buf[0];
    *src = buf[1];
    strcpy((char *)msg, (char *)(buf + 2));
  }
  return 0;
}

static void write_slow(struct sp_port *port, uint8_t *cmd, int length) {

  if(length >= MAXLEN) {
    fprintf(stderr, "ACOM2000: Attempt to send too long message.\n");
    exit(1);
  }
  for (int i = 0; i < length; i++) {
    if(sp_nonblocking_write(port, &cmd[i], 1) < 0) {
      fprintf(stderr, "Device write error.\n");
      exit(1);
    }
    sp_drain(port);
    usleep(DELAY);
  }
}

/*
 * Addresses:
 *  0x40 - 0x4f: amplifiers
 *  0x50 - 0x5f: antenna controllers
 *  0x60 - 0x6f: transceivers
 *  0x70 - 0x7f: computers
 *
 */

static void send_cmd(struct sp_port *port, uint8_t src, uint8_t dst, uint8_t *msg) {

  uint8_t buf[MAXLEN];

  if(!port) return;
  buf[0] = dst;
  buf[1] = src;
  strcpy((char *)(buf + 2), (char *)msg);
  write_slow(port, buf, strlen((char *)msg) + 3); // include null
}

void set_operate(struct sp_port *port) {

  send_cmd(port, COMPUTER, AMPLIFIER, (uint8_t *)"O");
  acom2000.opr_stby = 1;
  sleep(1); // needed?
}

void set_standby(struct sp_port *port) {

  send_cmd(port, COMPUTER, AMPLIFIER, (uint8_t *)"S");
  acom2000.opr_stby = 0;
  sleep(1); // needed?
}

void process_data(struct sp_port *port) {

  uint8_t src, dst, msg[MAXLEN];
  double tmp, tmp2, tmp3;
  
  if(!port) return;

  while(sp_input_waiting(port) > 0) {
    if(read_cmd(port, &src, &dst, msg) < 0) return;
    if(src == 0 && dst == 0) { // single byte cmd for RCU leds
      if(msg[0] >= 0x80 && msg[0] <= 0x90) acom2000.led_fwd = msg[0] - 0x80;
      else if(msg[0] >= 0xa0 && msg[0] <= 0xa8) acom2000.led_refl = msg[0] - 0xa0;
      else fprintf(stderr, "Unknown single byte command %x\n", msg[0]);
      continue;
    }
    if(src != AMPLIFIER || (dst != COMPUTER && dst != BROADCAST)) {
      fprintf(stderr, "Message from %2x to %2x - ignored.\n", src, dst);
      return;
    }
    tmp = (double) ((msg[1] - '0') * 16 + (msg[2] - '0')); // xx 
    tmp2 = (double) ((msg[3] - '0') * 16 + (msg[4] - '0')); // yy
    tmp3 = (double) ((msg[1] - '0') * 16 * 16 * 16 + (msg[2] - '0') * 16 * 16 + (msg[3] - '0') * 16 + msg[4] - '0'); // xxyy
    switch(msg[0]) {
    case 'O': // Switched to operate
      acom2000.opr_stby = 1;
      if(debug) fprintf(stderr, "Switch to operate.\n");
      break;
    case 'S': // Switched to standby
      acom2000.opr_stby = 0;
      if(debug) fprintf(stderr, "Switch to standby.\n");
      break;
    case 'L':   // Amplifier is on the network identifier
      acom2000.address = src;
      if(debug) fprintf(stderr, "Amplifier network address = %2x\n", src);
      break;
    case 'I':
      tmp = (double) atoi((char *)(msg + 1));
      acom2000.amp_temp = tmp * 10.0;
      if(debug) fprintf(stderr, "Amplifier temperature = %lf\n", acom2000.amp_temp);
      break;
    case 'b': // Bias voltage (V)
      acom2000.bias = tmp / 2.0;
      if(debug) fprintf(stderr, "Bias voltage = %lf V.\n", acom2000.bias);
      break;      
    case 'e': // DC input (W)
      acom2000.dc_input = tmp * tmp2 * 0.16;
      if(debug) fprintf(stderr, "DC input = %lf W.\n", acom2000.dc_input);
      break;      
    case 'f': // drive (W)
      acom2000.drive = tmp * tmp / 512.0;
      if(debug) fprintf(stderr, "Drive = %lf W.\n", acom2000.drive);
      break;      
    case 'h': // frequency (kHz) -- check this! xxyy or xx*yy?
      acom2000.freq = tmp3 * 2.0;
      if(debug) fprintf(stderr, "Frequency = %lf kHz.\n", acom2000.freq);
      break;      
    case 'i': // forward power (W)
      acom2000.fwd_power = tmp * tmp / 16.0;
      if(debug) fprintf(stderr, "Forward power = %lf W.\n", acom2000.fwd_power);
      break;
    case 'j': // filament voltage (V)
      acom2000.fil_volt = tmp / 16.0;
      if(debug) fprintf(stderr, "Filament voltage = %lf V.\n", acom2000.fil_volt);
      break;
    case 'k': // power gain (dB)
      acom2000.power_gain = 10.0 * log10(tmp * tmp / (tmp2 * tmp2) * 32.0);
      if(debug) fprintf(stderr, "Power gain = %lf dB.\n", acom2000.power_gain);
      break;      
    case 'l': // High voltage (V)
      acom2000.hv = tmp * 16.0;
      if(debug) fprintf(stderr, "High voltage = %lf V.\n", acom2000.hv);
      break;            
    case 'm': // IF front tube (A)
      acom2000.if_front = tmp / 16.0;
      if(debug) fprintf(stderr, "IF front = %lf A.\n", acom2000.if_front);
      break;
    case 'n': // IF rear tube (A)
      acom2000.if_rear = tmp / 16.0;
      if(debug) fprintf(stderr, "IF rear = %lf A.\n", acom2000.if_rear);
      break;
    case 'o': // I grid (mA)
      acom2000.grid_i = tmp / 10.0;
      if(debug) fprintf(stderr, "I grid = %lf mA.\n", acom2000.grid_i);
      break;
    case 'p': // I plate (mA)
      acom2000.plate_i = tmp * 10.0;
      if(debug) fprintf(stderr, "I plate = %lf mA.\n", acom2000.plate_i);
      break;
    case 'q': // I screen (mA)
      acom2000.screen_i = tmp;
      if(debug) fprintf(stderr, "I screen = %lf mA.\n", acom2000.screen_i);
      break;
    case 'r': // mains voltage (V)
      acom2000.mains_v = tmp * 10.0 / 8.0;
      if(debug) fprintf(stderr, "Mains voltage = %lf V.\n", acom2000.mains_v);
      break;
    case 's': // plate RF voltage (V)
      acom2000.plate_v = tmp * 12.0;
      if(debug) fprintf(stderr, "Plate RF voltage = %lf V.\n", acom2000.plate_v);
      break;
    case 't': // Reflected power (W)
      acom2000.refl_power = tmp * tmp / 100.0;
      if(debug) fprintf(stderr, "Reflected power = %lf W.\n", acom2000.refl_power);
      break;
    case 'w': // screen voltage (V)
      acom2000.screen_v = tmp * 2.0;
      if(debug) fprintf(stderr, "Screen voltage = %lf V.\n", acom2000.screen_v);
      break;
    case 'x': // front tube temp (C)
      acom2000.temp_f = (tmp - 136.0) * 2.0;
      if(debug) fprintf(stderr, "Front tube temperature = %lf C.\n", acom2000.temp_f);
      break;
    case 'y': // rear tube temp (C)
      acom2000.temp_r = (tmp - 136.0) * 2.0;
      if(debug) fprintf(stderr, "Rear tube temperature = %lf C.\n", acom2000.temp_r);
      break;
    case 'z': // VSWR
      if(tmp != 0.0) {
        acom2000.vswr = (2.5 * tmp + tmp2) / (2.5 * tmp - tmp2);
        if(debug) fprintf(stderr, "VSWR = %lf.\n", acom2000.vswr);
      }
      break;
    case 'U':
      acom2000.rcu_status = tmp;
      if(debug) fprintf(stderr, "RCU status = %2x.\n", acom2000.rcu_status);
      break;
    case 'M':
      if(msg[1] == 'M') acom2000.rcu_active = 1; // RCU is active, can't measure
      if(debug) fprintf(stderr, "RCU active - can't measure from computer.\n");
      break;
    case 'V': // Error state: 0 = no error, 1 = stop drive power , 2 = drive power too high, 3 = loss of user data
              // 4 = battery warning
      acom2000.error = msg[1] - '0';
      if(debug) fprintf(stderr, "Error state = %x.\n", acom2000.error);
      break;
    case 'W':
      if(msg[1] == '1')
        // 2-3 = xx, 4 = a, 5 = u, 6 = b
        acom2000.band = msg[6] - '0'; // band number (1 = 160, 2 = 80, 3 = 40, 4 = 30, 5 = 20, 6 = 17, 7 = 15, 8 = 12, 9 = 10
      if(debug) fprintf(stderr, "Current band = %x.\n", acom2000.band);
      break;
    default:
      break;
    }
  }
}

void start_measurement(struct sp_port *port, uint8_t a, uint8_t b) {

  uint8_t buf[32];

  buf[0] = 'M';
  buf[1] = a;
  buf[2] = b;
  buf[3] = '\0';
  acom2000.rcu_active = 0;
  send_cmd(port, COMPUTER, AMPLIFIER, buf);
}

void stop_measurement(struct sp_port *port) {

  send_cmd(port, COMPUTER, AMPLIFIER, (uint8_t *)"MM");
}

void request_band(struct sp_port *port) {

  send_cmd(port, COMPUTER, AMPLIFIER, (uint8_t *)"W7");
}
