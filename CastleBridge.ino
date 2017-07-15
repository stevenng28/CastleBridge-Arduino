/*
 * This program sends HTML PUT requests designed for common use cases of a door.
 * It will send a request whenever the door's status changes, and every 5 minutes when the door is open.
 * 
 * Written for WeMos D1 R2 Mini (ESP8266)
 * Requires two sensors to be installed on pins D3 and D4 to detect the door status
 * Require WiFi connection
 * 
 * Fill out CastleBridgeSettings.h with WiFi name, password and the URL to make the PUT requests to.
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <CastleBridgeSettings.h>

#define STATUS_CLOSED 1
#define STATUS_OPENING 2
#define STATUS_OPEN 3
#define STATUS_CLOSING 4
#define STATUS_ERROR 10

// Global variables
static const int delayAmount = 1000;
ESP8266WiFiMulti WiFiMulti;
String loggedStatusURL;
int openTimer = 0;
int previousD3 = HIGH;
int previousD4 = LOW;
short previousStatus = 1;
unsigned long lastDebounceTime = 0;


void setup() {
  Serial.begin(9600);
  Serial.println("Setting up WiFi connection...");
  WiFiMulti.addAP(wifiName, wifiPass);
  while(WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("Connected on IP ");
  Serial.println(WiFi.localIP());
  
  pinMode(D3, INPUT_PULLUP);
  pinMode(D4, INPUT_PULLUP);
}

void loop() {
  int sensorD3 = debouncedRead(D3, previousD3);
  int sensorD4 = debouncedRead(D4, previousD4);
  int currentStatus = determineCurrentStatus(sensorD3, sensorD4);
  bool statusChanged = (previousStatus != currentStatus);

  switch(currentStatus){
    case STATUS_CLOSED:  if(statusChanged){
                           bool httpSuccess = requestToURL("{\"status\":{\"closed\":\"true\"}}");
                           if(httpSuccess){ Serial.println("Sent closed"); }
                         }
                         break;
    case STATUS_CLOSING: if(statusChanged){
                           bool httpSuccess = requestToURL("{\"status\":{\"closing\":\"true\"}}");
                           if(httpSuccess){ Serial.println("Sent closing"); }
                         }
                         break;
    case STATUS_OPENING: if(statusChanged){
                           bool httpSuccess = requestToURL("{\"status\":{\"opening\":\"true\"}}");
                           if(httpSuccess){ Serial.println("Sent opening"); }
                         }
                         break;
    case STATUS_OPEN:    if(statusChanged){
                           bool httpSuccess = requestToURL("{\"status\":{\"open\":\"true\"}}");
                           if(httpSuccess){ Serial.println("Sent open"); }
                         }
                         //Send open if 5 minutes elapse
                         else if(openTimer >= 300000){
                           bool httpSuccess = requestToURL("{\"status\":{\"open\":\"true\"}}");
                           if(httpSuccess){ Serial.println("Sent open"); }
                           openTimer = 0;                     
                         }
                         else{
                           openTimer += delayAmount;
                         }
                         break;
    default:             Serial.println("ERROR: Current status unknown");
  }
  
  previousStatus = currentStatus;
  delay(delayAmount);
}

/*
 * Determine current status based on sensor readings and previous status
 * Pin Truth Table:  Pin D3 | Pin D4 | Door Status
 * -----------------------------------------------------
 *                      1   |   1    | Opening or closing
 *                      0   |   1    | Open
 *                      1   |   0    | Closed
 */
int determineCurrentStatus(int sensorD3, int sensorD4){
  if(sensorD3 == HIGH && sensorD4 == LOW){
    return STATUS_CLOSED;
  }
  if(sensorD3 == LOW && sensorD4 == HIGH){
    return STATUS_OPEN;
  }
  if(sensorD3 == HIGH && sensorD4 == HIGH){
    switch(previousStatus){
      case STATUS_CLOSED: return STATUS_OPENING;
                          break;
      case STATUS_OPEN:   return STATUS_CLOSING;
                          break;
      default:            return previousStatus;
                          break;        
    }
  }
  return STATUS_ERROR;
}

// Return true if http code is 200
bool requestToURL(String payload){
  HTTPClient http;

  //First connection to wake up server
  Serial.print("Attempting to connect to url: ");
  Serial.println(urlRecord);
  http.begin(urlRecord);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK){
    Serial.println("Server may be asleep, waiting...");
    delay(5000);
  }
  http.end();

  //Second connection to issue PUT request
  http.begin(urlRecord);
  http.addHeader("Content-Type","application/json");
  httpCode = http.PUT(payload);
  delay(250);
  Serial.print("HTTP Code: ");
  Serial.println(httpCode);
  http.end();
  return ((httpCode == HTTP_CODE_OK) ? true : false);
}

int debouncedRead(int sensorID, int previousValue){
    int sensor = digitalRead(sensorID);
    if(sensor != previousValue){
      delay(50);
    }
    sensor = digitalRead(sensorID);
    return sensor;
}

