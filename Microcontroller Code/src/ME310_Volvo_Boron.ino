/*
 * Project Digital_Dirt
 * Description:
 * Author: Song Ling Shin, Cole Greene
 * Date: 26 April 2021
 */

#include <TinyGPS.h>
#include <queue>


// States of the microcontroller
typedef enum {
  STANDBY,
  MOISTURE_MODE,
  IN_TRANSIT,
} States_t;


// Global variables
unsigned long tnow, tnext, tcheck;
float lat, lon;
int nextEventTime, nextCheckTime, moistureReading;
byte isGeolocationValid;
TinyGPS gps;
States_t state, prevState;
std::queue<String> localQueue;


// Set pin numbers
int moisturePin = A0;
int adapterPin = D2;
int switchButton = D3;
int activeLED = D7;
int standbyLED = D8;


// moisture value constants
const int wetValue = 2000;
const int dryValue = 3480;

// Functions
void checkBeaconState();
void getGeolocation();
void handleStandby();
void handleMoistureMode();
void handleInTransit();
void handleDelivered();
void publishData();
String generateData();


// setup() runs once, when the device is first turned on.
void setup() {
  // Put initialization like pinMode and begin functions here.  
  Serial.begin(57600); // connect serial monitor
  Serial1.begin(9600); // connect gps sensor
  // Time.zone(-7);       // switch the time zone to PST (UTC-7)
  Time.setFormat(TIME_FORMAT_ISO8601_FULL); // setup time format
  
  pinMode(moisturePin, INPUT);
  pinMode(adapterPin, INPUT_PULLUP);
  pinMode(switchButton, INPUT);
  pinMode(activeLED, OUTPUT);
  pinMode(standbyLED, OUTPUT);
  state = STANDBY;
  prevState = STANDBY;

  isGeolocationValid = 0;

  nextEventTime = 500;
  tnow = millis();
  tnext = tnow + nextEventTime;

  nextCheckTime = 500;
  tcheck = tnow + nextCheckTime;
}


// main loop
void loop() {
  tnow = millis(); 
  if (tnow >= tcheck) {
    checkBeaconState();
    tcheck = tnow + nextCheckTime;
  }
}


void checkBeaconState() {
  // determine the states
  if (digitalRead(switchButton) == 0){
    digitalWrite(standbyLED, HIGH);
    digitalWrite(activeLED, LOW);
    state = STANDBY;
  } else {
    digitalWrite(standbyLED, LOW);
    digitalWrite(activeLED, HIGH);
    if (digitalRead(adapterPin) == 0) {
      state = MOISTURE_MODE;
    } else {
      state = IN_TRANSIT;
    }
  }
  
  if (tnow >= tnext || state != prevState) {
    // switch to different states 
    switch (state) {
    case STANDBY:
      handleStandby();
      break;
    case MOISTURE_MODE:
      handleMoistureMode();
      break;
    case IN_TRANSIT:
      handleInTransit();
      break;
    default:
      break;
    }
  prevState = state;

  // update the time for the next event to end
  tnext = tnow + nextEventTime;
  }
}


void getGeolocation() {
  // Get GPS location;
  while (Serial1.available()) {
    if (gps.encode(Serial1.read())) {
      gps.f_get_position(&lat, &lon);
      isGeolocationValid = 1;
    }
  }
}


void handleStandby() {
  Serial.print("Standing by...");
  nextEventTime = 1000;
  // while (!digitalRead(switchButton)) {
  //   Serial.print(".");
  //   delay(1000);
  // }
  // state = MOISTURE_MODE;
}


void handleMoistureMode() {
  Serial.println("Moisture Mode!");
  nextEventTime = 30 * 1000;
  // update moisture level
  moistureReading = analogRead(moisturePin);
  moistureReading = map(moistureReading, dryValue, wetValue, 0, 100);
  Serial.println(moistureReading);
  // still need to work on mapping the analog reading to the actual moisture level
  getGeolocation();
  // delayTime = 60 * 60 * 1000; // wait for 60 minutes
  publishData();
}


void handleInTransit() {
  Serial.println("In-transit!");
  nextEventTime = 30 * 1000;
  getGeolocation();
  publishData();
}


// void handleDelivered() {
//   Serial.println("Delivered!");
//   nextEventTime = 5 * 1000;
//   // Do something to "Standby" or "Sleep"
//   getGeolocation();
//   // publishData();
//   state = STANDBY;
// }


String generateData() {
  lat = 37.426224;
  lon = -122.171621;

  // Data inputs;
  String beaconID = "\"beaconID\":1,";
  String moisture = "\"moistureLevel\":" + String(moistureReading) + ",";
  String geoLat   = "\"geoLat\":" + String(lat, 6) + ",";
  String geoLong  = "\"geoLong\":" + String(lon, 6) + ",";
  // String pingTime = "\"pingTime\":" + Time.timeStr();
  String distanceTraveled = "\"distanceTraveled\":" + String(0);

  // combine all the data into a single string
  String data = "{" + beaconID + moisture + geoLat + geoLong + distanceTraveled + "}";
  
  return data;
}


void publishData() {
  String data = generateData();
  isGeolocationValid = 1;

  // make sure the GPS is getting the correct geolocations
  if (isGeolocationValid){

    // check internet connection
    if (Cellular.connecting()) {               // if no internet connection, save the data into a local queue
      Serial.println("No internet conenction, saving data locally...");
      localQueue.push(data);
    } else {                                   // once the internet connection is back
      while (!localQueue.empty()) {
        String localData = localQueue.front(); // get data from the local queue
        localQueue.pop();                      // remove the data once it is retrieved

        Serial.println("Publishing local data:...");
        Serial.println(localData);
        Particle.publish("getdata", localData, PRIVATE);
      }

      // If internet connection is good and there is no data in the local queue
      Serial.println(data);
      Particle.publish("getdata", data, PRIVATE);
    }

  } else {
    // restart the loop with a smaller delay time if the geolocation is invalid
    Serial.println("No valid Geolocation");
    nextEventTime = 500;
  }
}
