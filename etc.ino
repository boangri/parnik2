void print_settings () {
  Serial.println(version);
  Serial.print("N_AVG="); Serial.print(N_AVG);
  Serial.print(" N_RING="); Serial.println(N_RING);
  Serial.print("ID="); Serial.println(idp->id);
  Serial.print("temp_fans="); Serial.print(sp->temp_fans); Serial.print(" temp_pump="); Serial.println(sp->temp_pump);
  Serial.print("barrel_diameter="); Serial.print(sp->barrel_diameter); 
  Serial.print(" barrel_height="); Serial.println(sp->barrel_height);
  Serial.print("water_min_volume="); Serial.print(sp->water_min_volume); 
  Serial.print(" water_max_volume="); Serial.println(sp->water_max_volume);
  Serial.print("water_per_grad="); Serial.println(sp->water_per_grad);
  Serial.print("water_min_temp="); Serial.println(sp->water_min_temp);
  Serial.print("water_start="); Serial.println(sp->water_start);   
  Serial.print("water_period="); Serial.println(sp->water_period);
}

//struct {
//  float temp_fans;          // tf
//  float temp_pump;          // tp 
//  float barrel_diameter;    // bd
//  float barrel_height;      // bh
//  float water_min_volume;   // wn
//  float water_max_volume;   // wx
//  float water_per_grad;     // wg
//  float water_min_temp;     // tw
//  float water_start;        // ws
//  float water_period;       // wp
//} typedef Settings;

void serial_output() {
  Serial.print(it);
  Serial.print(" T1=");
  Serial.print(pp->temp1);
  Serial.print(" T2=");
  Serial.print(pp->temp2);
  Serial.print(" T3=");
  Serial.print(pp->temp3);
  Serial.print(" U=");
  Serial.print(volt);
  Serial.print(" Ua=");
  Serial.print(pp->volt);
  Serial.print(" H: ");
  Serial.print(h);
  Serial.print(" cm. Vol: ");
  Serial.print(pp->vol);
  Serial.println(" L.");
}

/*
 * Given a distance to water surface (in sm.)
 * calculates the volume of water in the barrel (in Liters)
 */
float toVolume(float h) {
  return barrel_volume*(1.0 - h/barrel_height);
}  

uint32_t mon_days[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

uint32_t loc2ts(uint8_t yy, uint8_t month, uint8_t day, 
uint8_t hour, uint8_t minute, uint8_t second) {
  uint32_t leap_days;
  uint32_t year, year_sec, leap_sec, mon_sec, day_sec, hour_sec, min_sec;
  
  year = yy + 2000;
  year_sec = 31536000*(year - 1970); 
  leap_days = (year - 1973)/4 + 1;
  leap_sec = leap_days * 86400;
  if (((year % 4) == 0) && (month > 2)) leap_sec += 86400;
  mon_sec = 86400*mon_days[month -1];
  day_sec = 86400*(day - 1);
  hour_sec = 3600*(uint32_t)hour;
  min_sec = 60*(uint32_t)minute;
  return year_sec + leap_sec + mon_sec + day_sec + hour_sec + min_sec + second;   
}
 

