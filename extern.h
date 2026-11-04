struct acom2000 {
  uint8_t address;     // Amplifier address from broadcasted L message
  uint8_t opr_stby;    // 0 = standby, 1 = operate
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
  double refl_power;   // Reflected power (V)
  double screen_v;     // Screen voltage (V)
  double temp_f;       // Temperature of the front tube (C)
  double temp_r;       // Temperature of the rear tube (C)
  double vswr;         // VSWR
};

extern void set_operate(int), set_standby(int), process_data(int), start_measurement(int, uint8_t, uint8_t);
extern void stop_measurement(int), setup_tty(int);

