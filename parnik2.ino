#include <GPRS_Shield_Arduino.h>
#include <sim900.h>

#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <Average.h>
#include <dht.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NewPing.h>

#define USE_GPRS false
#define DEBUG true

Average voltage(N_AVG);
//Average temperature(N_AVG);
Average distance(N_AVG);
#include "parnik.h"

const char version[] = "0.5.3"; 

#define TEMP_FANS 27  // temperature for fans switching on
#define TEMP_PUMP 23 // temperature - do not pump water if cold enought
#define BARREL_HEIGHT 74.0 // max distanse from sonar to water surface which 
#define BARREL_DIAMETER 57.0 // 200L

/* related to watering */
#define WATER_MIN_VOLUME 0 // L, 
#define WATER_MAX_VOLUME 20 // L
#define WATER_PER_GRAD 1.0 // L per gradus
#define WATER_MIN_TEMP 20 // gradus
#define WATER_START 180 // minutes
#define WATER_PERIOD 180 // minutes

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
char buf[120];
GPRS gprs(9600);

SoftwareSerial mySerial(rxPin, txPin); // RX, TX 
//DHT dht = DHT();
// DS18S20 Temperature chip i/o
OneWire ds(tempPin);  // on pin 10
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&ds);
int numberOfDevices; 
boolean convInProgress;
unsigned long lastTemp;

unsigned long lastLed;
boolean ledState;

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
unsigned long it = 0; // iteration counter;
float workHours, fanHours, pumpHours; // work time (hours)
unsigned long fanMillis, pumpMillis, workMillis, delta, lastDist, lastVolt;
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
Ident ident;
Ident *idp = &ident;

// Data Ring Buffer
#define N_RING 1
Packet pack[N_RING];
Packet *wp, *rp;
int n_ring, iw, ir;
unsigned long lastRingWritten; //ms
unsigned long lastAttempt; // ms - last attempt to send data over GPRS

// EEPROM
int eeAddress = sizeof(ident);

// Bluetooth 
char bt_buf[80];
char *bp;

void setup(void) {
  byte b;
  //EEPROM[0] = 255;
  Serial.begin(9600);
  //while (!Serial) {}
  mySerial.begin(9600);
  if (EEPROM.get(eeAddress, b) == 255) {
//    Serial.println("Setting not set yet, use defaults");
    sp->temp_fans = TEMP_FANS;
    sp->temp_pump = TEMP_PUMP;
    sp->barrel_diameter = BARREL_DIAMETER;
    sp->barrel_height = BARREL_HEIGHT;
    sp->water_min_volume = WATER_MIN_VOLUME;
    sp->water_max_volume = WATER_MAX_VOLUME;
    sp->water_per_grad = WATER_PER_GRAD;
    sp->water_min_temp = WATER_MIN_TEMP;
    sp->water_start = WATER_START;
    sp->water_period = WATER_PERIOD;
    EEPROM.put(eeAddress, settings);
//    Serial.println("Wrote defaults to EEPROM");
  }
  EEPROM.get(0, ident);
  EEPROM.get(eeAddress, settings);
  sensors.begin();
  convInProgress = false;
  lastTemp = 0;
  numberOfDevices = sensors.getDeviceCount();
  Serial.print("Locating devices...");
  
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");
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
/*
  delay(1000);
  gprs.powerUpDown();
  while (!gprs.init()) {
    delay(1000);
    Serial.println(".");
  }
  Serial.println("GPRS initialized");
  */
  
  workMillis = 0; // millis();
  lastDist = lastVolt = lastAttempt = 0;
  
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

  pinMode(13, OUTPUT);
  lastLed = millis();
  ledState = false;
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
  if(!convInProgress) { //launch temp converson
    sensors.setWaitForConversion(false);  // makes it async
    sensors.requestTemperatures();
    sensors.setWaitForConversion(true);
    lastTemp = millis();
    convInProgress = true;   
  }
  if(millis() - lastTemp > 750) { // data should be ready
    pp->temp1 = sensors.getTempCByIndex(0);
    if (numberOfDevices > 1) {
      pp->temp2 = sensors.getTempCByIndex(1);
    }
    convInProgress = false;
  } 
  
  /* 
   *  measure water volume
   */
if (!DEBUG) {   
  uS = sonar.ping_median(7);
  if (uS > 0) {
    h = (float)uS / US_ROUNDTRIP_CM;
  }  
} else { 
  h = 2 + 10*pumpHours;
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
  volt = 13.06/883* dividerValue;
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
  if(mySerial.available() > 0){
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
    wp->temp2 = pp->temp2;
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
  /*
   * Led 13 switching on and off - 
   */
   if (millis() - lastLed > 100) {
      if (ledState) {
        digitalWrite(13, LOW);
        ledState = false;
      } else {
        digitalWrite(13, HIGH);
        ledState = true;
      }
      lastLed = millis();
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
  //Serial.print(" it=");
  Serial.print(it);
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
  request += ":";
  request += rp->temp2;
  request += ":U:U:U:";
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
  String c = String(cmd);
  String s, q;

  if (c.indexOf("par=?") == 0) {
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
    return;
  }
  if (c.indexOf("set=?") == 0) {
    s = String("tf=");
    s += sp->temp_fans;
    s += ";tp=";
    s += sp->temp_pump;
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
    mySerial.println(s);
    mySerial.write((byte)0);
    return;
  }
  if (c.indexOf("ver=?") == 0) {
    mySerial.println(version);
    mySerial.write((byte)0);
    return;
  }
  if (c.indexOf("id=?") == 0) {
    mySerial.println(idp->id);
    mySerial.write((byte)0);
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
      s = s.substring(i+1);
    }
    EEPROM.put(eeAddress, settings);
    mySerial.println("ok");
    mySerial.write((byte)0);
    return;
  }
  
}

