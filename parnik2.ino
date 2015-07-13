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

const char version[] = "0.1.1"; 

#define TEMP_FANS 27  // temperature for fans switching off
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
int fanState = 0;
int pumpState = 0;
int it = 0; // iteration counter;
float workHours, fanHours, pumpHours; // work time (hours)
unsigned long fanMillis, pumpMillis, workMillis, delta, lastTemp, lastDist, lastVolt;
int np = 0; /* poliv session number */
float h; // distance from sonar to water surface, cm.
float barrel_height;
float barrel_volume;

Parnik parnik;
Parnik *pp = &parnik;
Settings settings;
Settings *sp = &settings;
int eeAddress = 0;

void setup(void) {
  byte b;
  EEPROM[0] = 255;
  Serial.begin(9600);
  while (!Serial) {}
  mySerial.begin(9600);
  if (EEPROM.get(eeAddress, b) == 255) {
    Serial.println("Setting not set yet, use defaults");
    sp->temp_fans = TEMP_FANS;
    sp->temp_pump = TEMP_PUMP;
    EEPROM.put(eeAddress, settings);
    Serial.println("Wrote defaults to EEPROM");
  }
  EEPROM.get(eeAddress, settings);
  sensors.begin();
//  dht.attach(dhtPin);
  
  pinMode(fanPin, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  digitalWrite(fanPin, OFF);
  digitalWrite(pumpPin, OFF);
  fanState = 0; 
  fanMillis = 0;
  pumpState = 0;
  pumpMillis = 0; 
  pp->temp_hi = TEMP_FANS;
  pp->temp_lo = pp->temp_hi - 1.0;
  pp->temp_pump = TEMP_PUMP;
  Serial.println(version);
  workMillis = 0; // millis();
  lastTemp = lastDist = lastVolt = millis();
  h = 200.;
  it = 0;
  np = 0;
  barrel_height = BARREL_HEIGHT;
  barrel_volume = BARREL_DIAMETER*BARREL_DIAMETER*3.14/4000*BARREL_HEIGHT;
}

void loop(void) {
  unsigned int uS;
  unsigned int ms;
  int c; // input char
  
  it++;
  // Timing
  delta = millis() - workMillis;
  workMillis += delta;
  if (fanState) {
    fanMillis += delta;
  }
  if (pumpState) {
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
  if (ms - lastTemp > 3000) {
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
  if (ms - lastDist > 3000) {
    pp->dist = distance.getAverage();
    water = toVolume(pp->dist);
    pp->vol = water;
    lastDist = ms;
  }
  /* 
   *  Measure voltage
   */
  dividerValue = (float)analogRead(dividerPin);  
  volt = 55.52/1023.* dividerValue;
  voltage.putValue(volt);
  ms = millis();
  if (ms - lastVolt > 3000) {
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
  if(Serial.available() > 0) {
    c = Serial.read();
    mySerial.print((char)c);
    mySerial.flush();
  }
  /*
   * Translate BT input to serial
   */
  if(mySerial.available() > 0) {
    c = mySerial.read();
    Serial.print((char)c);
    Serial.flush();
  }
  /*
   * Fans control 
   */ 
  if (fanState == 1) {
     if (temp < pp->temp_lo) {
       digitalWrite(fanPin, OFF);
       fanState = 0;  
       pp->fans = fanState;  
     } 
  } else {
     if (temp > pp->temp_hi) {
       digitalWrite(fanPin, ON);    
       fanState = 1;
       pp->fans = fanState;
     } 
  }  
  /*
   * Pump control 
   */
  if (pumpState == 1) {
    float V;
    V = (temp - pp->temp_pump) * Vpoliv;
    //V = np == 1 ? 0.5 : V;  // First poliv - just for test
     if ((water < 0.) || (temp < pp->temp_pump) || (water0 - water > V)) {
       digitalWrite(pumpPin, OFF);
       pumpState = 0; 
       pp->pump = pumpState;   
     } 
  } else {
     if (workHours >= Tpoliv*np) {       
       np++;  
       // Switch on the pump only if warm enought and there is water in the barrel     
       if ((temp > pp->temp_pump) && (water > 0.)) {
         digitalWrite(pumpPin, ON);    
         pumpState = 1;
         pp->pump = pumpState;
         water0 = water;
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
  Serial.print(it);
  Serial.print(" T1=");
  Serial.print(pp->temp1);
  Serial.print("C U=");
  Serial.print(volt);
  Serial.print(" Uavg=");
  Serial.print(pp->volt);
  Serial.print(" H: ");
  Serial.print(h);
  Serial.print(" cm. Volume: ");
  Serial.print(pp->vol);
  Serial.println(" L.");
}

