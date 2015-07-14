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

const char version[] = "0.1.3"; 

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
  //EEPROM[0] = 255;
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
  Serial.print("C readout=");
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

