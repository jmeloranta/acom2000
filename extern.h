#include <libserialport.h>

struct acom2000 {
  uint8_t address;     // Amplifier address from broadcasted L message
  uint8_t opr_stby;    // 0 = standby, 1 = operate
  uint8_t led_fwd;     // Forward power LED bar (0 - 16)
  uint8_t led_refl;    // Reflected power LED bar (0 - 8)
  double amp_temp;     // Amplifier temperature (C)
  double bias;         // bias voltage (V)
  double dc_input;     // DC input (W)
  double drive;        // Drive (W)
  double freq;         // Frequency (kHz)
  double fwd_power;    // Forward power (W)
  double fil_volt;     // Filament voltage (V)
  double power_gain;   // Power gain (dB)
  double hv;           // High voltage (V)
  double if_front;     // Front tube current (A)
  double if_rear;      // Rear tube current (A)
  double grid_i;       // Grid current (mA)
  double plate_i;      // Plate current (mA)
  double screen_i;     // Screen current (mA)
  double mains_v;      // Mains voltage (V)
  double plate_v;      // Plate voltage (V)
  double refl_power;   // Reflected power (W)
  double screen_v;     // Screen voltage (V)
  double temp_f;       // Temperature of the front tube (C)
  double temp_r;       // Temperature of the rear tube (C)
  double vswr;         // VSWR
  uint8_t rcu_status;  // RCU status indicators
  uint8_t error;       // Amplifier error state
  uint8_t rcu_active;  // RCU is actively measuring - can't do measure on the computer
  uint8_t band;        // Current band (1 - 9)
};

extern void set_operate(struct sp_port *), set_standby(struct sp_port *), process_data(struct sp_port *), start_measurement(struct sp_port *, uint8_t, uint8_t);
extern void stop_measurement(struct sp_port *), setup_tty(struct sp_port *), request_band(struct sp_port *);
