#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <Wire.h>
#include <RtcDS3231.h>

String apiURL = "http://192.168.100.5/IOTEmersedMonitoring/";
String ssid = "ARFAD";
String pass = "polygon.";
const int dry = 2500;
const int ON = 0;
const int OFF = 1;

// pin sensor
const int moisPin = 36;
const int dhtPin = 4;

// pin mesin
const int fanPin = 12;   
const int LEDPin = 14;
const int pumpPin = 27;

void connectWifi();
void getHttp();
char getFormattedTime(RtcDateTime time);
void printDateTime(const RtcDateTime& dt);
void postData(int moisture, int temp);
void runPumpDC(int moisture);
void runLED(RtcDateTime now);
void runFan(int temp);

// object initialize
#define DHTTYPE DHT11
DHT_Unified dht(dhtPin, DHTTYPE);
RtcDS3231<TwoWire> Rtc(Wire);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  pinMode(moisPin, INPUT);
  pinMode(dhtPin, INPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(LEDPin, OUTPUT);
  pinMode(fanPin, OUTPUT);

  connectWifi();
  
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);
}

void loop() {
  RtcDateTime now = Rtc.GetDateTime();
  int moisture = analogRead(moisPin);

  // inisialisasi untuk dht11
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  int farmTemp =  event.temperature;

  runPumpDC(moisture);
  runFan(farmTemp);
  runLED(now);

  printDateTime(now);

  postData(moisture, farmTemp);
  
  Serial.println(" ");
  delay(1500);
}

void connectWifi(){
  WiFi.begin(ssid, pass);
  Serial.println();
  Serial.println("Connecting to wifi");
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(800);
  }
  Serial.println();
  Serial.println("Success Connected to WiFi");
}

void getHttp(){
  String url = apiURL+"realtime";
  HTTPClient http;
  JsonDocument doc;
  String response;

  http.begin(url);
  http.GET();

  response = http.getString();
  deserializeJson(doc, response);
  String moisture = doc[0]["moisture"];
  Serial.println(moisture);

}

void postData(int moisture, int temp){
  String url = apiURL+"post";
  HTTPClient http;
  JsonDocument doc;
  String response;
  String params;
  
  RtcDateTime now = Rtc.GetDateTime();
  doc["moisture"] = moisture;
  doc["temperature"] = temp;
  doc["waktu"] = String(now.Year()) +"-"+ String(now.Month()) +"-"+ 
                 String(now.Day()) +" "+ String(now.Hour()) +":"+ 
                 String(now.Minute()) +":"+ String(now.Second());

  http.begin(url);
  serializeJson(doc, params);
  http.POST(params);

  response = http.getString();
  Serial.println(response);
  delay(1000);
}

void runPumpDC(int moisture){
  if (moisture >= dry)
  {
    Serial.println("---------------");
    Serial.print("Moisture : ");
    Serial.println(moisture);
    Serial.println("Pump DC ON");
    digitalWrite(pumpPin, ON);
  }else{
    Serial.println("---------------");
    Serial.print("Moisture : ");
    Serial.println(moisture);
    Serial.println("Pump DC OFF");
    digitalWrite(pumpPin, OFF);
  }
}

void runLED(RtcDateTime TimeNow){
  const int jamNyala = 06;
  const int jamMati = 18;
  
  if(TimeNow.Hour() >= jamNyala){
    Serial.println("---------------");
    Serial.println("LED ON");
    digitalWrite(LEDPin, ON);
  }
  if(TimeNow.Hour() >= jamMati && TimeNow.Hour() < jamNyala){
    Serial.println("---------------");
    Serial.println("LED ON");
    digitalWrite(LEDPin, OFF);

  }
}

void runFan(int temp){
  if(temp >= 30){
    Serial.println("---------------");
    digitalWrite(fanPin, ON);
    Serial.print("Temperature : ");
    Serial.print(temp);
    Serial.println(" C");
    Serial.println("Fan ON");
  }else{
    Serial.println("---------------");
    digitalWrite(fanPin, OFF);
    Serial.print("Temperature : ");
    Serial.print(temp);
    Serial.println(" C");
    Serial.println("Fan OFF");
  }
}

void printDateTime(const RtcDateTime& dt)
{
	char datestring[26];

	snprintf_P(datestring, 
			countof(datestring),
			PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
			dt.Month(),
			dt.Day(),
			dt.Year(),
			dt.Hour(),
			dt.Minute(),
			dt.Second() );
  Serial.println(datestring);
  Serial.println("=====================");
}


