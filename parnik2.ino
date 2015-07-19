#include <GPRS_Shield_Arduino.h>
#include <sim900.h>

#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <Average.h>
#include <dht.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NewPing.h>

Average voltage(N_AVG);
Average temperature(N_AVG);
Average distance(N_AVG);
#include "parnik.h"

const char version[] = "0.3.1"; 

#define TEMP_FANS 27  // temperature for fans switching on
#define TEMP_PUMP 23 // temperature - do not pump water if cold enought
#define BARREL_HEIGHT 74.0 // max distanse from sonar to water surface which 
#define BARREL_DIAMETER 57.0 // 200L

#define ON LOW
#define OFF HIGH // if transistors 

const int tempPin = A0;
const int echoPin = A1;
//const int dhtPin = A2;
const int dividerPin = A5;
const int rxPin = 8; // connect to BT HC-05 TX pin
const int txPin = 9; // connect to BT HC-05 RX pin
const int triggerPin = 10;
const int fanPin = 11;
const int pumpPin = 12;

const float Vpoliv = 1.0; // Liters per centigrade above TEMP_PUMP 
const float Tpoliv = 4; // Watering every 4 hours

const char apn[] = "internet.mts.ru"; 
const char user[] = "mts";
const char pass[] = "mts";
const char host[] = "www.xland.ru";
//char req[] = "GET /cgi-bin/parnik2_upd?ts=1436946564&T=17.25:U:U:U:26:27:U&M=00:00&P=11.7:U&V=173.61:5.92 HTTP/1.0\r\n\r\n";
char buf[240];
GPRS gprs(9600);

SoftwareSerial mySerial(rxPin, txPin); // RX, TX 
//DHT dht = DHT();
// DS18S20 Temperature chip i/o
OneWire ds(tempPin);  // on pin 10
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&ds);
#define MAX_DISTANCE 200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
NewPing sonar(triggerPin, echoPin, MAX_DISTANCE); // NewPing setup of pins and maximum distance.

float dividerValue;
float distValue;
float temp;
float volt;
float water, water0;
boolean fansOn;
boolean pumpOn;
boolean readyToSend;
int pumpState = 0;
int it = 0; // iteration counter;
float workHours, fanHours, pumpHours; // work time (hours)
unsigned long fanMillis, pumpMillis, workMillis, delta, lastTemp, lastDist, lastVolt;
int np = 0; /* poliv session number */
float h; // distance from sonar to water surface, cm.
float barrel_height;
float barrel_volume;

// Time service
unsigned long ts; // secs, current time UNIX timestamp
unsigned long lastTimeSet; //ms


Parnik parnik;
Parnik *pp = &parnik;
Settings settings;
Settings *sp = &settings;

// Data Ring Buffer
#define N_RING 4
Packet pack[N_RING];
Packet *wp, *rp;
int n_ring, iw, ir;
unsigned long lastRingWritten; //ms
unsigned long lastAttempt; // ms - last attempt to send data over GPRS

// EEPROM
int eeAddress = 0;

// Bluetooth 
char bt_buf[80];
char *bp;

void setup(void) {
  byte b;
  //EEPROM[0] = 255;
  Serial.begin(9600);
  while (!Serial) {}
  mySerial.begin(9600);
  if (EEPROM.get(eeAddress, b) == 255) {
//    Serial.println("Setting not set yet, use defaults");
    sp->temp_fans = TEMP_FANS;
    sp->temp_pump = TEMP_PUMP;
    EEPROM.put(eeAddress, settings);
//    Serial.println("Wrote defaults to EEPROM");
  }
  EEPROM.get(eeAddress, settings);
  sensors.begin();
//  dht.attach(dhtPin);
  
  pinMode(fanPin, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  digitalWrite(fanPin, OFF);
  digitalWrite(pumpPin, OFF);
  fansOn = false; 
  fanMillis = 0;
  pumpOn = false;
  pumpMillis = 0; 
  Serial.println(version);
  Serial.print("Settings: temp_fans=");
  Serial.print(sp->temp_fans);
  Serial.print("C temp_pump=");
  Serial.print(sp->temp_pump);
  Serial.println("C");

  gprs.powerUpDown();
  // РїСЂРѕРІРµСЂСЏРµРј, РµСЃС‚СЊ Р»Рё СЃРІСЏР·СЊ СЃ GPRS-СѓСЃС‚СЂРѕР№СЃС‚РІРѕРј
  while (!gprs.init()) {
    // РµСЃР»Рё СЃРІСЏР·Рё РЅРµС‚, Р¶РґС‘Рј 1 СЃРµРєСѓРЅРґСѓ
    // Рё РІС‹РІРѕРґРёРј СЃРѕРѕР±С‰РµРЅРёРµ РѕР± РѕС€РёР±РєРµ;
    // РїСЂРѕС†РµСЃСЃ РїРѕРІС‚РѕСЂСЏРµС‚СЃСЏ РІ С†РёРєР»Рµ,
    // РїРѕРєР° РЅРµ РїРѕСЏРІРёС‚СЃСЏ РѕС‚РІРµС‚ РѕС‚ GPRS-СѓСЃС‚СЂРѕР№СЃС‚РІР°
    delay(1000);
    Serial.print("Init error\r\n");
  }
  // РІС‹РІРѕРґРёРј СЃРѕРѕР±С‰РµРЅРёРµ РѕР± СѓРґР°С‡РЅРѕР№ РёРЅРёС†РёР°Р»РёР·Р°С†РёРё GPRS Shield
  Serial.println("GPRS initialized");
  
  workMillis = 0; // millis();
  lastTemp = lastDist = lastVolt = lastAttempt = 0;
  
  readyToSend = true;
  h = 200.;
  it = 0;
  np = 0;
  barrel_height = BARREL_HEIGHT;
  barrel_volume = BARREL_DIAMETER*BARREL_DIAMETER*3.14/4000*BARREL_HEIGHT;
  // Time service
  ts = 1300000000; // time is unknown yet;
  lastTimeSet = 0; // means never
  // Ring Buffer
  wp = rp = pack;
  n_ring = iw = ir = 0;
  lastRingWritten = 0;

  bp = bt_buf;
}

void loop(void) {
  unsigned int uS;
  unsigned int ms;
  int c; // input char
  
  it++;
  // Timing
  delta = millis() - workMillis;
  workMillis += delta;
  if (fansOn) {
    fanMillis += delta;
  }
  if (pumpOn) {
    pumpMillis += delta;
  }
  workHours = (float)workMillis/3600000.; // Hours;
  fanHours = (float)fanMillis/3600000.;
  pumpHours = (float)pumpMillis/3600000.;

  /* 
   *  Measure temperature
   */
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(0);
  temperature.putValue(temp);  
  ms = millis();
  if (ms - lastTemp > 300) {
    pp->temp1 = temperature.getAverage();
    lastTemp = ms;
  }
  
  /* 
   *  measure water volume
   */
  uS = sonar.ping_median(7);
  if (uS > 0) {
    h = (float)uS / US_ROUNDTRIP_CM;
  }  
  h *= (1 + 0.5*(pp->temp1 - 20)/(pp->temp1 + 273)); // take temperature into account
  distance.putValue(h);
  ms = millis();
  if (ms - lastDist > 300) {
    pp->dist = distance.getAverage();
    pp->vol = toVolume(pp->dist);
    lastDist = ms;
  }
  /* 
   *  Measure voltage
   */
  dividerValue = (float)analogRead(dividerPin);  
  volt = 13.08/516* dividerValue;
  voltage.putValue(volt);
  ms = millis();
  if (ms - lastVolt > 300) {
    pp->volt = voltage.getAverage();
    lastVolt = ms;
  }
  /*
   * Output data
   */
  if (Serial.available() > 0) { 
    Serial.read();
    serial_output();
  }
  /*
   * Translate serial to BT 
   */
   /*
  if(Serial.available() > 0) {
    c = Serial.read();
    mySerial.print((char)c);
    mySerial.flush();
  }
  */
  /*
   * Translate BT input to serial
   */
  if(mySerial.available() > 0) {
    c = mySerial.read();
    if (c == ';') {
      *bp = 0;
      bp = bt_buf;
      bt_cmd(bp);
    } else {
      *bp++ = c;
    }
    Serial.print((char)c);
    Serial.flush();
  }

  /*
   * Fans control 
   */ 
  if (fansOn) {
     if (pp->temp1 < sp->temp_fans - 1.0) {
       digitalWrite(fanPin, OFF);
       fansOn = false;  
       pp->fans = 0;  
     } 
  } else {
     if (pp->temp1 > sp->temp_fans) {
       digitalWrite(fanPin, ON);    
       fansOn = true;
       pp->fans = 1;
     } 
  }  
  /*
   * Pump control 
   */
  if (pumpOn) {
     float V;
     V = (pp->temp1 - sp->temp_pump) * Vpoliv;
     if ((pp->vol < 0.) || (pp->temp1 < sp->temp_pump) || (water0 - pp->vol > V)) {
       digitalWrite(pumpPin, OFF);
       pumpOn = false; 
       pp->pump = 0;   
     } 
  } else {
     if (workHours >= Tpoliv*np) {       
       np++;  
       // Switch on the pump only if warm enought and there is water in the barrel     
       if ((pp->temp1 > sp->temp_pump) && (pp->vol > 0.)) {
         digitalWrite(pumpPin, ON);    
         pumpOn = true;
         pp->pump = 1;
         water0 = pp->vol;
       }
     }  
  }  
  if ((lastRingWritten == 0)||(millis() - lastRingWritten > 300000)) { // its time to write data into ring buffer
    wp->ts = ts + (millis() - lastTimeSet)/1000;
    wp->temp1 = pp->temp1;
    wp->volt = pp->volt;
    wp->vol = pp->vol;
    wp->dist = pp->dist;
    wp->fans = pp->fans;
    wp->pump = pp->pump;
    wp++;
    if (++iw == N_RING) {
      iw = 0;
      wp = pack;
    }
    n_ring++;
    if (n_ring > N_RING) { // overflow
      n_ring = N_RING;
      rp++;
      if (++ir == N_RING) {
        ir = 0;
        rp = 0;
      }
    }
    lastRingWritten = millis();
  }
  /*
   * Send data with GPRS
   */
  if ((n_ring > 0) && (millis() - lastAttempt > 30000)) {  
    lastAttempt = millis();
    if(gprs_send()) {
      String response;
      unsigned long n;
      int i;
      
      Serial.println("sent!");
      response = String(buf);
      Serial.println(response);
      if ((response.indexOf("200 OK") == 9)&&(response.indexOf("success") > 9)) { // sent successfully
        Serial.println("suc");     
        response = response.substring(response.indexOf("ts=")+3);
        n = response.toInt();
        if ((ts < 1400000000) && (n > 1400000000)) {
          ts = n;
          lastTimeSet = millis();
        }
        if ((i = response.indexOf("tf=")) > 0) {
          response = response.substring(i+3);
          n = response.toInt();
          if ((n > 0) && (n < 999)) {
            sp->temp_fans = ((float)n)/10. -30.;
            EEPROM.put(eeAddress, settings);
            Serial.print("s tf=");
            Serial.println(sp->temp_fans);
          }
        }
        if ((i = response.indexOf("tp=")) > 0) {
          response = response.substring(i+3);
          n = response.toInt();
          if ((n > 0) && (n < 999)) {
            sp->temp_pump = ((float)n)/10. -30.;
            EEPROM.put(eeAddress, settings);
            Serial.print("s tp=");
            Serial.println(sp->temp_pump);
          }
        }
        n_ring--;
        rp++;
        if (++ir == N_RING) {
          ir = 0;
          rp = pack;
        }
      }
    }
  } 
}  

/*
 * Given a distance to water surface (in sm.)
 * calculates the volume of water in the barrel (in Liters)
 */
float toVolume(float h) {
  return barrel_volume*(1.0 - h/barrel_height);
}  

void serial_output() {
  Serial.print(" it=");
  Serial.print(ts);
  Serial.print(" T1=");
  Serial.print(pp->temp1);
  Serial.print("C val=");
  Serial.print(dividerValue);
  Serial.print(" U=");
  Serial.print(volt);
  Serial.print(" Uavg=");
  Serial.print(pp->volt);
  Serial.print(" H: ");
  Serial.print(h);
  Serial.print(" cm. Volume: ");
  Serial.print(pp->vol);
  Serial.println(" L.");
}

boolean gprs_send() 
{
  String request = "GET /cgi-bin/parnik2_upd?ts="; 
  unsigned int len;
return false;
  request += rp->ts;
  request += "&T=";
  request += rp->temp1;
  request += ":U:U:U:U:";
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
  request += " HTTP/1.0\r\n\r\n";

  //char req[] = "GET /cgi-bin/parnik_upd2?ts=1435608900&T=17.25:U:U:U:26:27:U&M=00:00&P=11.7:U&V=173.61:5.92 HTTP/1.0\r\n\r\n";
  len = request.length()+1;
  request.toCharArray(buf, len);

  if (gprs.join(apn, user, pass)) {
    Serial.println("s join");
    if (gprs.connect (TCP, host, 80, 100)) {
      Serial.println("Con!"); 
      gprs.send(buf, len);
      Serial.println("Sent: ");
      Serial.println(buf);
      Serial.print("req len=");
      Serial.println(len);
      Serial.println("Wait");
      len = gprs.recv(buf, sizeof(buf)); 
      buf[len+1] = 0;
      //Serial.println("Rcv ");    
      //Serial.println(buf);
      Serial.print("len=");
      Serial.println(len);
    } else {
      gprs.close();
      Serial.println("f conn");
      return false;
    }  
    gprs.close();
    return true;
  } else {
    Serial.println("f join");
    return false;
  }
}

void bt_cmd(char *cmd) {
  String s = String(cmd);
  Serial.println(s);
  if (s.indexOf("par=?") == 0) {
    s = String("t1=");
    s += pp->temp1;
    s += ";vl=";
    s += pp->vol;
    s += ";vt=";
    s += pp->volt;
    s += ";f=";
    s += pp->fans;
    s += ";p=";
    s += pp->pump;
    s += ";";
    mySerial.println(s);
    mySerial.write((byte)0);
  }
}

