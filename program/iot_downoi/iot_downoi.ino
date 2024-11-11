#include <Arduino.h>
#include <WiFi.h>
// SENSOR CO2 & humidity
#include "MHZ19.h"
#include "DHT.h"
#include <BH1750.h>
#include <Wire.h>
#include <RtcDS3231.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define BAUDRATE 9600                                      // Device to MH-Z19 Serial baudrate (should not be changed)
#define DHTTYPE DHT22
#define DHTPIN 4 
#define solenoid 12
#define mistMaker 13
#define lampu 5

// inisialisasi object
MHZ19 myMHZ19;                                             // Constructor for library
HardwareSerial mySerial(0);                                // On ESP32 we do not require the SoftwareSerial library, since we have 2 USARTS available
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter(0x23);
RtcDS3231<TwoWire> Rtc(Wire);

// String apiURL = "http://192.168.100.5/IOTEmersedMonitoring/";
String apiURL = "https://skripsiarman.my.id/IOTEmersedMonitoring/";
String ssid = "ARFAD";
String pass = "polygon.";

void connectWifi();
void runSolenoid(int CO2);
void runMistMaker(int humidity);
void runLight(float lux, int waktuPencahayaan);

int getCO2();
int getHum();
float getLux();
double hitungLamaHidupLampu(float lux, int waktuPencahayaan);

int startLampuHidup;
double lamaHidup;
int lamaHidupDetik;
void setup()
{
    Serial.begin(9600);                                     
    connectWifi();
    Wire.begin();

    lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    mySerial.begin(BAUDRATE);                               
    myMHZ19.begin(mySerial);                                // *Serial(Stream) reference must be passed to library begin().
    myMHZ19.autoCalibration();                              // Turn auto calibration ON (OFF autoCalibration(false))
    
    dht.begin();
    Rtc.Begin();
    RtcDateTime compiled = RtcDateTime("Nov 11 2024", "06:58:00");
    Rtc.SetDateTime(compiled);

    pinMode(mistMaker, OUTPUT);
    pinMode(solenoid, OUTPUT);
    pinMode(lampu, OUTPUT);

    digitalWrite(mistMaker, LOW);
    digitalWrite(solenoid, LOW);
    digitalWrite(lampu, LOW);
    startLampuHidup = millis()/1000;
}

void loop()
{
  int waktuPencahayaan = 8;
  RtcDateTime now = Rtc.GetDateTime();
  int jamSekarang = now.Hour();
  int menitSekarang = now.Minute();
  int detikSekarang = now.Second();
  Serial.print("waktu saat ini : "); Serial.print(" ");
  Serial.print(jamSekarang); Serial.print(":");
  Serial.print(menitSekarang); Serial.print(":");
  Serial.println(detikSekarang);

  int humidity = getHum();
  runMistMaker(humidity);

  int CO2 = getCO2();

  float lux = getLux();
  
  int waktuBerjalan = millis()/1000;
  if(jamSekarang == 7 && menitSekarang < 1 && detikSekarang <= 10){ // <= 10 detik agar tidak masuk ke badan if ini selama semenit 
    startLampuHidup = millis()/1000;
    lamaHidup = hitungLamaHidupLampu(lux, waktuPencahayaan);
    lamaHidupDetik = lamaHidup * 3600;
    digitalWrite(lampu, HIGH);
    Serial.println("lampu Hidup");
  }

  int waktuHidupLampu = waktuBerjalan - startLampuHidup;
  if(waktuBerjalan - startLampuHidup == lamaHidupDetik){
    digitalWrite(lampu, LOW);
    digitalWrite(solenoid, LOW); // untuk memastikan solenoid mati
    Serial.println("lampu mati");
  }else{
    runSolenoid(CO2);
  }

  int jamPostData[8] = {7,10,13,16,19,22,1,4}; // kirim data per 3 jam sekali
  if(findInArray(jamPostData, 8, jamSekarang) && menitSekarang < 1 && detikSekarang < 5){
    postData(CO2, humidity, lux);
  }else{
    Serial.println("belum waktunya post");
  }
  
  Serial.println();
  Serial.println("=====================================");
  delay(2000);
}

void postData(int co2, int humidity, float lux){
  String url = apiURL+"api/post";
  HTTPClient http;
  JsonDocument doc;
  String response;
  String params;
  
  RtcDateTime now = Rtc.GetDateTime();
  doc["co2"] = co2;
  doc["humidity"] = humidity;
  doc["lux"] = lux;
  doc["waktu"] = String(now.Year()) +"-"+ String(now.Month()) +"-"+ 
                 String(now.Day()) +" "+ String(now.Hour()) +":"+ 
                 String(now.Minute()) +":"+ String(now.Second());

  http.begin(url);
  serializeJson(doc, params);
  http.POST(params);

  response = http.getString();
  Serial.println(response);
  http.end();
  delay(1000);
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

int getCO2(){
  int CO2;
  /* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even
  if below background CO2 levels or above range (useful to validate sensor). You can use the
  usual documented command with getCO2(false) */
  
  CO2 = myMHZ19.getCO2();                             // Request CO2 (as ppm)

  Serial.print("CO2 (ppm): ");
  Serial.println(CO2);
  delay(1000);
  return CO2;
}

int getHum(){
  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return 0;
  }
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.println(" %");
  delay(1000);
  return h;
}

float getLux(){
  float lux = 0;
  if (lightMeter.measurementReady()) {
    float lux = lightMeter.readLightLevel();
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");
    delay(1000);
    return lux; 
  }else{
    delay(1000);
    return lux;
  }
}

void runSolenoid(int CO2){
  if(CO2 <= 1500){
    digitalWrite(solenoid, HIGH);
    Serial.println("co2 hidup");
  }else{
    digitalWrite(solenoid, LOW);
    Serial.println("co2 mati");
  }
}

void runMistMaker(int humidity){
  if(humidity <= 90){
    digitalWrite(mistMaker, HIGH);
    Serial.println("mist maker hidup");
  }else{
    digitalWrite(mistMaker, LOW);
    Serial.println("mist maker Mati");
  }
}

double hitungLamaHidupLampu(float lux, int waktuPencahayaan){
  int maxLux = 15000; // artinya 100 persen = 15000
  int persentase = (100.0/maxLux) * lux; // 0.01 * lux
  int persentaseKekurangan = 100 - persentase;
  double lamaHidup = waktuPencahayaan + (waktuPencahayaan/100.0) * persentaseKekurangan;
  return lamaHidup;
}

bool findInArray(int array[], int size, int target) {
    for (int i = 0; i < size; i++) {
        if (array[i] == target) {
            return true; // Elemen ditemukan
        }
    }
    return false; // Elemen tidak ditemukan
}
