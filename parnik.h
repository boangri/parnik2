struct {
  float temp1;
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
} typedef Settings;

struct {
  unsigned int ts;
  float temp1;
  float volt;
  float vol;
  float dist;
  int fans;
  int pump;
} typedef Packet;


