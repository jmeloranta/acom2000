/* 
 * Library to interface with ACOM 2000A via RS232.
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include "extern.h"
#include "config.h"

#define MAXLEN 255

static int debug = 0;     // Message debug prints (0 = off, 1 = on)

struct acom2000 acom2000;

void setup_tty(int fd) {
  
  struct termios param;
  
  if(tcgetattr(fd, &param) < 0) {
    fprintf(stderr, "Status get failed.\n");
    exit(1);
  }
  
  param.c_cflag &= ~PARENB;
  param.c_cflag &= ~CSTOPB;
  param.c_cflag &= ~CSIZE; 
  param.c_cflag |= CS8;
  param.c_cflag &= ~CRTSCTS;
  param.c_cflag |= CREAD | CLOCAL;
  param.c_lflag &= ~ICANON;
  param.c_lflag &= ~ECHO;
  param.c_lflag &= ~ECHOE;
  param.c_lflag &= ~ECHONL;
  param.c_lflag &= ~ISIG;
  param.c_iflag &= ~(IXON | IXOFF | IXANY);
  param.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);  
  param.c_oflag &= ~OPOST;
  param.c_oflag &= ~ONLCR;
  
  cfsetispeed(&param, B1200);
  cfsetospeed(&param, B1200);
  
  if(tcsetattr(fd, TCSANOW, &param) < 0) {
    fprintf(stderr, "Status set failed.\n");
    exit(1);
  }
}

static int read_until_null(int fd, uint8_t *buf, int maxlen) {
  
  int pos = 0, bytes_rd;
  
  while(1) {
    if((bytes_rd = read(fd, buf + pos, 1)) < 0) return -1;
    if(bytes_rd == 0) {
      usleep(10000);
      continue;
    }
    if(pos == maxlen) return -1;
    if(buf[pos] == '\0') break;
    pos++;
  }
  return pos;
}

static int read_cmd(int fd, uint8_t *src, uint8_t *dst, uint8_t *msg) {

  uint8_t buf[MAXLEN];

  for(buf[0] = buf[1] = 0; buf[0] == 0 && buf[1] == 0; ) // eat up broken messages at the start
    read_until_null(fd, buf, MAXLEN);
  *dst = buf[0];
  *src = buf[1];
  strcpy(msg, buf+2);
}

static void write_slow(int fd, uint8_t *cmd, int length) {

  if(length >= MAXLEN) {
    fprintf(stderr, "ACOM2000: Attempt to send too long message.\n");
    exit(1);
  }
  for (int i = 0; i < length; i++) {
    write(fd, &cmd[i], 1);
    tcdrain(fd);   // is this needed?
    usleep(10000); // -"-
  }
}

static void send_cmd(int fd, uint8_t src, uint8_t dst, uint8_t *msg) {

  uint8_t buf[MAXLEN];

  buf[0] = dst;
  buf[1] = src;
  strcpy(buf+2, msg);
  write_slow(fd, buf, strlen(msg) + 3); // include null
}

void set_operate(int fd) {

  send_cmd(fd, COMPUTER, AMPLIFIER, "O");
  sleep(1); // needed?
}

void set_standby(int fd) {

  send_cmd(fd, COMPUTER, AMPLIFIER, "S");
  sleep(1); // needed?
}

void process_data(int fd) {

  fd_set fds;
  struct timeval timeout;
  uint8_t src, dst, msg[MAXLEN];
  double tmp, tmp2, tmp3;
  
  while(1) {
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;   // 10 ms
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if(!select(fd+1, &fds, NULL, NULL, &timeout)) return; // no more data
    read_cmd(fd, &src, &dst, msg);
    if(src != AMPLIFIER || (dst != COMPUTER && dst != BROADCAST)) {
      fprintf(stderr, "Message from %2x to %2x - ignored.\n", src, dst);
      return;
    }
    if(!FD_ISSET(fd, &fds)) return;
    tmp = (double) ((msg[1] - '0') * 16 + (msg[2] - '0'));
    tmp2 = (double) ((msg[3] - '0') * 16 + (msg[4] - '0'));
    tmp3 = (double) ((msg[1] - '0') * 16 * 16 * 16 + (msg[2] - '0') * 16 * 16 + (msg[3] - '0') * 16 + msg[4] - '0');
    switch(msg[0]) {
    case 'L':   // Amplifier is on the network identifier
      acom2000.address = src;
      if(debug) fprintf(stderr, "Amplifier network address = %2x\n", src);
      break;
    case 'I':
      tmp = (double) atoi(msg+1);
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
      acom2000.if_rear = tmp / 10.0;
      if(debug) fprintf(stderr, "I grid = %lf mA.\n", acom2000.grid_i);
      break;
    case 'p': // I plate (mA)
      acom2000.plate_i = tmp * 10.0;
      if(debug) fprintf(stderr, "I plate = %lf mA.\n", acom2000.plate_i);
      break;
    case 'q': // I scree (mA)
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
    default:
      // ignore
    }
  }
}

void start_measurement(int fd, uint8_t a, uint8_t b) {

  uint8_t buf[32];

  buf[0] = 'M';
  buf[1] = a;
  buf[2] = b;
  buf[3] = '\0';
  send_cmd(fd, COMPUTER, AMPLIFIER, buf);
}

void stop_measurement(int fd) {

  send_cmd(fd, COMPUTER, AMPLIFIER, "MM");
}

