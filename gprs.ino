boolean setup_gprs() {
  byte ret;
  String oper = "unknown";
  con.print("Resetting SIM800 module...");
  while (!gprs.init()) {
    con.write('.');
  }
  con.println("OK");
  
  con.print("Setting up network...");
  ret = gprs.setup(APN);
  if (ret) {
    con.print("Error code:");
    con.println(ret);
    con.println(gprs.buffer);
    return false;
  }
  con.println("OK");
  
  if (gprs.getOperatorName()) {
    con.print("Operator:");
    con.println(gprs.buffer);
    oper = gprs.buffer;
    oper.replace(" ", "%20");
  }
  dB = gprs.getSignalQuality();
  if (dB) {
     con.print("Signal:");
     con.print(dB);
     con.println("dB");
  }

  if (gprs.getLocation(&loc)) {
    con.print("LAT:");
    con.print(loc.lat, 6);
    con.print(" LON:");
    con.print(loc.lon, 6);
    con.print(" DATE:");
    con.print(loc.year);
    con.print('-');
    con.print(loc.month);
    con.print('-');
    con.print(loc.day);
    con.print(" TIME:");
    con.print(loc.hour);
    con.print(':');
    con.print(loc.minute);
    con.print(':');
    con.println(loc.second);
  }

  ts0 = loc2ts(loc.year, loc.month, loc.day, loc.hour, loc.minute, loc.second);
  if (ts0 > 1449990000) {
    ts = ts0;
    lastTimeSet = millis();
  }
  
  Serial.print("ts=");
  Serial.println(ts0);
  
  for (;;) {
    if (gprs.httpInit()) break;
    con.println(gprs.buffer);
    gprs.httpUninit();
    delay(1000);
    con.println("=");
  }
  char mydata[180];
  String coor = "lat=" + String(loc.lat, 6) +
    "&lon=" + String(loc.lon, 6)+ "&op=" + oper;
  
  sprintf(mydata, "&ts=%lu", ts0);
  coor += mydata;
  coor.toCharArray(mydata, 180);
  con.println("[Requesting]");
  con.print(url);
  con.print('?');
  con.println(mydata);
  gprs.httpConnect(url, mydata);

  while (gprs.httpIsConnected() == 0) {
    //con.write('>');
    for (byte n = 0; n < 25 && !gprs.available(); n++) {
      delay(10);
    }
  }

  if (gprs.httpState == HTTP_ERROR) {
    con.println("Connect error");
    return false; 
  }
  
  con.println();
  gprs.httpRead();
  
  while (gprs.httpIsRead() == 0) {
    // can do something here while waiting
  }
  
  if (gprs.httpState == HTTP_ERROR) {
    con.println("Read error");
    return false; 
  }

  // now we have received payload
  con.println("[Received]");
  con.println(gprs.buffer);
  return true;  
}

boolean gprs_send() 
{
  String request = "ts="; 
  unsigned int len;
  if (!useGPRS) return false;
  request += rp->ts;
  request += "&T=";
  request += rp->temp1;
  request += ":";
  request += rp->temp2;
  request += ":";
  request += rp->temp3;
  request += ":U:U:";
  request += sp->temp_fans;
  request += ":";
  request += sp->temp_pump;
  request += "&M=";
  request += rp->fans;
  request += ":";
  request += rp->pump;
  request += "&P=";
  request += rp->volt;
  request += ":U&V=";
  request += rp->vol;
  request += ":";
  request += rp->dist;
  len = request.length()+1;
  request.toCharArray(buf, len);
  
  Serial.println("[Requesting]");
  Serial.print(url);
  Serial.print('?');
  Serial.println(request);
  
  gprs.httpConnect(url, buf);

  while (gprs.httpIsConnected() == 0) {
    //con.write('>');
    for (byte n = 0; n < 25 && !gprs.available(); n++) {
      delay(10);
    }
  }

  if (gprs.httpState == HTTP_ERROR) {
    con.println("Connect error");
    return false; 
  }
  
  con.println();
  gprs.httpRead();
  
  while (gprs.httpIsRead() == 0) {
    // can do something here while waiting
  }
  
  if (gprs.httpState == HTTP_ERROR) {
    con.println("Read error");
    return false; 
  }

  // now we have received payload
  con.println("[Received]");
  con.println(gprs.buffer);
  return true;  
}


