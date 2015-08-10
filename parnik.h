struct {
  float temp1;
  float temp2;
  float temp_pump;
  float volt;
  float vol;
  float dist;
  int fans;
  int pump;
} typedef Parnik;

struct {
  float temp_fans;
  float temp_pump;
  float barrel_diameter;
  float barrel_height;
  float water_min_volume;
  float water_max_volume;
  float water_per_grad;
  float water_min_temp;
  float water_start;
  float water_period;
} typedef Settings;

struct {
  unsigned long ts;
  float temp1;
  float temp2;
  float volt;
  float vol;
  float dist;
  int fans;
  int pump;
} typedef Packet;

struct {
  unsigned long id; //unique device ID
  char secret[8]; // secret
} typedef Ident;

