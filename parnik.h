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
  float temp_fans;          // tf
  float temp_pump;          // tp 
  float barrel_diameter;    // bd
  float barrel_height;      // bh
  float water_min_volume;   // wn
  float water_max_volume;   // wx
  float water_per_grad;     // wg
  float water_min_temp;     // tw
  float water_start;        // ws
  float water_period;       // wp
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

