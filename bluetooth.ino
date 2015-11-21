void bt_cmd(char *cmd) {
  String c = String(cmd);
  String s, q;

  if (c.indexOf("par=?") == 0) {
    s = String("t1=");
    s += pp->temp1;
    s += ";t2=";
    s += pp->temp2;
    s += ";t3=";
    s += pp->temp3;
    s += ";vl=";
    s += pp->vol;
    s += ";vt=";
    s += pp->volt;
    s += ";f=";
    s += pp->fans;
    s += ";p=";
    s += pp->pump;
    s += ";";
    Serial2.println(s);
    Serial2.write((byte)0);
    return;
  }
  if (c.indexOf("set=?") == 0) {
    s = String("tf=");
    s += sp->temp_fans;
    s += ";tp=";
    s += sp->temp_pump;
    s += ";tw=";
    s += sp->water_min_temp;
    s += ";bh=";
    s += sp->barrel_height;
    s += ";bd=";
    s += sp->barrel_diameter;
    s += ";wg=";
    s += sp->water_per_grad;
    s += ";wn=";
    s += sp->water_min_volume;
    s += ";wx=";
    s += sp->water_max_volume;   
    s += ";ws=";
    s += sp->water_start;
    s += ";wp=";
    s += sp->water_period;
    s += ";";
    Serial2.println(s);
    Serial2.write((byte)0);
    return;
  }
  if (c.indexOf("ver=?") == 0) {
    Serial2.println(version);
    Serial2.write((byte)0);
    return;
  }
  if (c.indexOf("id=?") == 0) {
    Serial2.println(idp->id);
    Serial2.write((byte)0);
    return;
  }
  /*
   * Read statistics
   */
  if (c.indexOf("sta=?") == 0) {
    s = String("sw=");
    s += msec2str(pp->timeWork);
    s += ";sf=";
    s += msec2str(pp->timeFans);
    s += ";sp=";
    s += msec2str(pp->timePump);
    s += ";";
    Serial2.println(s);
    Serial2.write((byte)0);
    return;
  }
  /*
   * set parameters
   */
  if (c.indexOf("set=") == 0) {
    int i;
    
    s = c.substring(4);
    while ((i = s.indexOf(':')) > 0) {
      q = s.substring(0,i);
      if (q.startsWith("bh=")) {
        sp->barrel_height = (float)(q.substring(3).toInt());
      }
      if (q.startsWith("bd=")) {
        sp->barrel_diameter = (float)(q.substring(3).toInt());
      }  
      if (q.startsWith("tf=")) {
        sp->temp_fans = (float)(q.substring(3).toInt());
      }  
      if (q.startsWith("tp=")) {
        sp->temp_pump = (float)(q.substring(3).toInt());
      }  
      if (q.startsWith("tw=")) {
        sp->water_min_temp = (float)(q.substring(3).toInt());
      }  
      if (q.startsWith("ws=")) {
        sp->water_start = (float)(q.substring(3).toInt());
      }  
      if (q.startsWith("wp=")) {
        sp->water_period = (float)(q.substring(3).toInt());
      }  
      if (q.startsWith("wn=")) {
        sp->water_min_volume = (float)(q.substring(3).toInt());
      }  
      if (q.startsWith("wx=")) {
        sp->water_max_volume = (float)(q.substring(3).toInt());
      }  
      if (q.startsWith("wg=")) {
        sp->water_per_grad = (float)(q.substring(3).toInt());
      }      
      s = s.substring(i+1);
    }
    EEPROM.put(eeAddress, settings);
    Serial2.print("ok");
    Serial2.write((byte)0);
    return;
  }
  
}

// Convert millisec to time in format h:mm:ss 
String msec2str(long msec) {
  long secs = msec/1000;
  int s,m,h,mins;
  String str;
  
  s = secs % 60; //seconds
  mins = (secs - s)/60;
  m = mins % 60;
  h = (mins - m)/60;
  str = String("");
  str += h;
  str += (m < 10) ? ":0" : ":"; 
  str += m;
  str += (s < 10) ? ":0" : ":";
  str += s;
  return str;
}  

