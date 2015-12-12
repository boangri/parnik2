#include <SIM800.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <Average.h>
//#include <dht.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NewPing.h>

#define DEBUG false

Average voltage(N_AVG);
//Average temperature(N_AVG);
Average distance(N_AVG);
#include "parnik.h"

const char version[] = "0.10.0"; 

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
//const int rxPin = 6; // connect to BT HC-05 TX pin
const int resetPin = 7; // SIM800L reset pin
const int triggerPin = 10;
const int fanPin = 11;
const int pumpPin = 12;

const float Vpoliv = 1.0; // Liters per centigrade above TEMP_PUMP 
const float Tpoliv = 4; // Watering every 4 hours

#define APN "connect"
#define con Serial
static const char* url = "http://www.xland.ru/cgi-bin/parnik_upd2";

CGPRS_SIM800 gprs;
uint32_t count = 0;
uint32_t errors = 0;
GSM_LOCATION loc;

//char apn[] = "internet.mts.ru"; 
//char user[] = "mts";
//char pass[] = "mts";
char host[] = "www.xland.ru";
char buf[220];
//GPRS gprs(4800);

//SoftwareSerial mySerial(rxPin, txPin); // RX, TX 
//DHT dht = DHT();
// DS18S20 Temperature chip i/o
OneWire ds(tempPin);  // on pin 10
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&ds);
int numberOfDevices; 
boolean convInProgress;
unsigned long lastTemp;
boolean useGPRS;
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
//float workHours, fanHours, pumpHours; // work time (hours)
unsigned long delta, lastDist, lastVolt;
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
#define N_RING 96 // 8 hours (12*8) of data backup
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
  Serial.begin(9600);
  //while (!Serial) {}
  Serial2.begin(9600);
  EEPROM.get(0, b);
  if ( b == 255) {
    Serial.println("Not Initialized");
  } 
  EEPROM.get(0, ident);
  EEPROM.get(eeAddress, settings);
  print_settings();
  
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
  //fanMillis = 0;
  pumpOn = false;
  //pumpMillis = 0; 

  Serial.println("Trying initialize GPRS");
  if(setup_gprs()) {
    Serial.println("GPRS initialized");
    useGPRS = true;
  } else {
    Serial.println("GPRS not initialized");
    useGPRS = false;
  }
  Serial.println("Enter loop");
  
  //workMillis = 0; // millis();
  lastDist = lastVolt = lastAttempt = 0;
  pp->timeWork = 0;
  pp->timeFans = 0;
  pp->timePump = 0;
  
  readyToSend = true;
  h = 200.;
  it = 0;
  np = 0;
  barrel_height = sp->barrel_height;
  barrel_volume = sp->barrel_diameter*sp->barrel_diameter*3.14/4000*sp->barrel_height;
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
  delta = millis() - pp->timeWork;
  pp->timeWork += delta;
  if (fansOn) {
    pp->timeFans += delta;
  }
  if (pumpOn) {
    pp->timePump += delta;
  }

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
      if (numberOfDevices > 2) {
        pp->temp3 = sensors.getTempCByIndex(2);
      }
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
  h = 2 + pp->timePump/360000;
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
    Serial2.print((char)c);
    Serial2.flush();
  }
  */
  /*
   * Translate BT input to serial
   */
  while(Serial2.available() > 0){
    c = Serial2.read();
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
     if (pp->timeWork/3600000 >= Tpoliv*np) {       
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
    wp->temp3 = pp->temp3;
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
      response = String(gprs.buffer);
      Serial.println(response);
      //if ((response.indexOf("200 OK") == 9)&&(response.indexOf("success") > 9)) { // sent successfully
      if (response.indexOf("success") >= 0) { // sent successfully
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


