#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266Ping.h>
#include <ESP8266HTTPClient.h>
#include <FirebaseESP8266.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>
#include <String.h>
#include <Wire.h>
#include "SH1106Wire.h"

ESP8266WebServer server(80);

#define SENSOR  14
#define DETECTOR  16
#define RST  9
#define CRE  10
#define VALVE  13

#define POST_LITRE_URL "https://smart-water-meter-system.vercel.app/api/addlitre"
#define POST_STREAM_NOTHING_URL "https://smart-water-meter-system.vercel.app/api/stream/nothing"
#define FIREBASE_HOST "smart-water-meter-v1-default-rtdb.firebaseio.com" //Without http:// or https:// schemes
#define FIREBASE_SECRET "62pyh8BmfqlXYRTCBopoxtGkFxxYUqHGcTtGbSJk"

SH1106Wire display(0x3c, SDA, SCL);

//Define FirebaseESP8266 data object
FirebaseData firebaseData;

int r[7] = {16,48,80,112,144,176,208}; //(ssid,pass,data_litre,state_valve,}
unsigned w[7] = {0x010,0x030,0x050,0x070,0x090,0x0B0,0x0D0};
const byte DEVADDR = 0x50;

const char* hotspot_ssid = "Smart Water Meter";
const char* hotspot_password = "Water@123";
String _SSID = "Wireless Fidelity@ClassicTech";
String _pass = "1Qazxsw2#edc";

boolean rstVar = false;
boolean creVar = false;
boolean creFirstTime = true;
boolean valv = false;

int initialBeginVar = 1;
int interval = 1000;
long currentMillis = 0;
long previousMillis = 0;
int interval2 = 10*60*1000;
long currentMillis2 = 0;
long previousMillis2 = millis() - interval2 + 60000;
float calibrationFactor = 8.50;
volatile byte pulseCount = 0;
byte pulse1Sec = 0;
float flowRate = 0.0;
unsigned long flowMilliLitres = 0;
unsigned int totalMilliLitres = 0;
float flowLitres = 0;
float totalLitres = 0;


void IRAM_ATTR pulseCounter(){
  if(!digitalRead(DETECTOR)) {
    pulseCount++;
  }
}

void IRAM_ATTR rstLitre(){
  rstVar = true;
}

void IRAM_ATTR creHotspot(){
  creVar = !creVar;
}

void displaychar(String x, int i, int j, int k = 1){
  if (k == 1) display.setFont(ArialMT_Plain_10);
  else if (k == 2)  display.setFont(ArialMT_Plain_16);
  else if (k == 3)  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(i, j, x);
  display.display();
}

void clearLitre(){
  display.setColor(BLACK);
  display.drawRect(0, 10, 200, 200);
  display.fillRect(0, 10, 200, 200);
  display.setColor(WHITE);
}

void displayTestWifi(void){
  if (WiFi.status() == WL_CONNECTED){
    Serial.println("Connected");
    displaychar("Connected", 35,50,1);
  }
  else{
    Serial.println("No Connection");
    displaychar("No Wifi", 40,50,1);
  }
}

String mac_address(){
  String temp = String(WiFi.macAddress());
  byte len = temp.length();
  temp.replace(":", "");
  return temp;
}

bool testWifi(){
  if (WiFi.status() == WL_CONNECTED)
    return true;    
  return false;
}

String dataS(String x){
  int len = x.length();
  for(int i = 0; i < 32-len; i++){
    x += "*";
  }
  Serial.println(x);
  return x;
}

void eeprom_write_page(byte deviceaddress, unsigned eeaddr,
    const String data, byte length){
    // Three lsb of Device address byte are bits 8-10 of eeaddress
    byte devaddr = deviceaddress | ((eeaddr >> 8) & 0x07);
    byte addr = eeaddr;
    Wire.beginTransmission(devaddr);
    Wire.write(int(addr));
    for (int i = 0; i < 16; i++) {
        Wire.write(data[i]);
    }
    Wire.endTransmission();
    delay(5);
    eeaddr += 0x010;
    devaddr = deviceaddress | ((eeaddr >> 8) & 0x07);
    addr = eeaddr;
    Wire.beginTransmission(devaddr);
    Wire.write(int(addr));
    for (int i = 16; i < length; i++) {
        Wire.write(data[i]);
    }
    Wire.endTransmission();
    delay(5);
}

int eeprom_read_byte(byte deviceaddress, unsigned eeaddr) {
    byte rdata = -1;
    // Three lsb of Device address byte are bits 8-10 of eeaddress
    byte devaddr = deviceaddress | ((eeaddr >> 8) & 0x07);
    byte addr = eeaddr;

    Wire.beginTransmission(devaddr);
    Wire.write(int(addr));
    Wire.endTransmission();
    Wire.requestFrom(int(devaddr), 1);
    if (Wire.available()) {
      rdata = Wire.read();
    }
    return rdata;
}

String hexToString(String x) {
    String y = "";
    char tmp[] = "12", c = 'x'; // two char tmp - third is zeroed out already
    int i = 0, len = x.length(), n = 0;
    for (i = 0; i < len; i += 2) {
        tmp[0] = x[i];
        tmp[1] = x[i + 1];
        // tmp now has two char hex value
        n = strtol(tmp, NULL, 16); // n has numeric value
        y += (char) n;
    }
    return y;
}

String readData(byte i) {
    String x = "";
    for (byte k = i; k < i+32; k++) {
        byte b = eeprom_read_byte(DEVADDR, k);
        //Serial.println(b);
        if (b == 42) {
            break;
        }
        x += String(b, HEX);
    }
    x = hexToString(x);
    return x;
}

void setCredentials(){
  String data = server.arg("plain");
  StaticJsonBuffer<200> jBuffer;
  JsonObject& jObject = jBuffer.parseObject(data);
  String access_point = jObject["ssid"];
  String pass = jObject["pass"];
  eeprom_write_page(DEVADDR, w[0], dataS(access_point), 32);
  eeprom_write_page(DEVADDR, w[1], dataS(pass), 32);
  server.send(200,"Credential Changed");
}

void postData(String apiurl, String jsondata) {
  Serial.println(apiurl);
  Serial.println(jsondata);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  http.begin(client, apiurl); //HTTP
  http.addHeader("Content-Type", "application/json");
  Serial.print("[HTTP] POST...\n");
  int httpCode = http.POST(jsondata);
  // httpCode will be negative on error
  if (httpCode > 0) {
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      const String& payload = http.getString();
      Serial.println("received payload:\n<<");
      Serial.println(payload);
      Serial.println(">>");
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void handleStreamResult(FirebaseData &data)
{
  String stream;
  String deviceId = mac_address();
  if (data.dataType() == "string") {
    stream = data.stringData();
    Serial.println(stream);
  }
  if(stream == "VALVE_OFF") {
    postData(POST_STREAM_NOTHING_URL, "{\"deviceId\":\"" + deviceId + "\"}");
    digitalWrite(VALVE, LOW);
  }
  else if(stream == "PAYMENT_SUCCESS"){
    digitalWrite(VALVE, HIGH);
    eeprom_write_page(DEVADDR, w[2], dataS("0.00"), 32);
    clearLitre();
    String data_Litre = readData(r[2]);
    displaychar("V: "+data_Litre+" L", 0,10,2);
    displaychar("U: "+String(data_Litre.toFloat()/1000,4), 0,30,2);
    displayTestWifi();
    postData(POST_STREAM_NOTHING_URL, "{\"deviceId\":\"" + deviceId + "\"}");
  }  
}

void firebaseBeginStream() {
  String path = "/devices/" + mac_address() + "/stream";
  if (!Firebase.beginStream(firebaseData, path))
  {
    Serial.println("Can't begin stream connection...");
    Serial.println("REASON: " + firebaseData.errorReason());
  } else {
    initialBeginVar=0;
  }
}

void setup(){
  delay(3000);
  Serial.begin(115200);
  Wire.begin();
  
  display.init();
  display.flipScreenVertically();
  display.clear();

  displaychar("Smart", 18,10,3);
  displaychar("Water Meter", 18,40,2);
  
  pinMode(SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
  pinMode(RST, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RST), rstLitre, FALLING);
  pinMode(CRE, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CRE), creHotspot, FALLING);
  pinMode(VALVE, OUTPUT);
  digitalWrite(VALVE, HIGH);
  pinMode (DETECTOR, INPUT);

  String ssid = readData(r[0]), password = readData(r[1]);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("SSID: "+ssid+"Pass: "+password);

  Firebase.begin(FIREBASE_HOST, FIREBASE_SECRET);
  Firebase.reconnectWiFi(true);

  Serial.print("Output Liquid Quantity: ");
  Serial.print(totalMilliLitres);
  Serial.print("mL / ");
  Serial.print(totalLitres);
  Serial.println("L");

  delay(7000);
  Serial.println(mac_address());

  display.clear();
  displaychar("Smart Water Meter", 20,0,1);
  String data_Litre = readData(r[2]);
  displaychar("V: "+data_Litre+" L", 0,10,2);
  displaychar("U: "+String(data_Litre.toFloat()/1000,4), 0,30,2);
  displayTestWifi();
}

void loop() {
  if(creVar) {
    Serial.println("Cre clicked");
    delay(500);
    if(creFirstTime) {
      digitalWrite(VALVE, LOW);
      Serial.println("Cre First Time");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(hotspot_ssid, hotspot_password);         //starting AccessPoint on given credential
      IPAddress myIP = WiFi.softAPIP();        //IP Address of our Esp8266 accesspoint(where we can host webpages, and see data)
      Serial.print("Access Point IP address: ");
      Serial.println(myIP);
      server.on("/credentials",setCredentials);
      server.begin();
      delay(5000);
      creFirstTime = false;
    }
   server.handleClient(); 
  }
  else {
    if(!creFirstTime) {
      creFirstTime = true;
      String litr = readData(r[2]);
      if(litr.toFloat() >= 1000) {
        digitalWrite(VALVE, LOW);
        display.clear();
        displaychar("PAYMENT", 10,10,3);
        displaychar("REQUIRED", 10,70,3);

      }
      digitalWrite(VALVE, HIGH);
      WiFi.mode(WIFI_STA);
      String ssid = readData(r[0]), password = readData(r[1]);
      WiFi.begin(ssid, password);
      Serial.println("SSID: "+ssid+"Pass: "+password);
    }
    if (initialBeginVar && testWifi()){
      firebaseBeginStream();    
    }
    if(!initialBeginVar) {
      if (!Firebase.readStream(firebaseData))
      {
        Serial.println("Can't read stream data...");
        Serial.println("REASON: " + firebaseData.errorReason());
      }
  
      if (firebaseData.streamTimeout())
      {
        Serial.println("Stream timeout, resume streaming...");
      }
  
      if (firebaseData.streamAvailable())
      {
        Serial.println("Stream Data available... Printing");
        Serial.println("STREAM PATH: " + firebaseData.streamPath());
        Serial.println("EVENT PATH: " + firebaseData.dataPath());
        Serial.println("DATA TYPE: " + firebaseData.dataType());
        Serial.println("EVENT TYPE: " + firebaseData.eventType());
        Serial.print("VALUE: ");
        handleStreamResult(firebaseData);
      }
    }
    if (rstVar){
      eeprom_write_page(DEVADDR, w[0], dataS(_SSID), 32);
      eeprom_write_page(DEVADDR, w[1], dataS(_pass), 32);
      eeprom_write_page(DEVADDR, w[2], dataS("0.00"), 32);
      clearLitre();
      //displaychar("Smart Water Meter", 20,0,1);
      String data_Litre = readData(r[2]);
      displaychar("V: "+data_Litre+" L", 0,10,2);
      displaychar("U: "+String(data_Litre.toFloat()/1000,4), 0,30,2);
      displayTestWifi();
      rstVar = false;
    }
  
    
  
  ////////////////////////////////////////////////////////////////////////////////////////
  
    currentMillis2 = millis();
    if (currentMillis2 - previousMillis2 > interval2 && testWifi()){
      Serial.println("10 minute");
      String data_Litre = readData(r[2]);
      String deviceId = mac_address();
      postData(POST_LITRE_URL, "{\"deviceId\":\"" + deviceId + "\",\"litre\":\"" + data_Litre + "\"}");
      previousMillis2 = millis();
    }
  
  //////////////////////////////////////////////////////////////////////////////////////////
    currentMillis = millis();
    if (currentMillis - previousMillis > interval){
      pulse1Sec = pulseCount;
      pulseCount = 0;
    
      flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) ;
      if (flowRate < calibrationFactor * 0.6){
        flowRate = flowRate / (calibrationFactor * .55);
      }
      else if (flowRate < calibrationFactor){
        flowRate = flowRate / (calibrationFactor * .65);
      }
      else if (flowRate < calibrationFactor * 2.2){
        flowRate = flowRate / (calibrationFactor * .80);
      }
      else{
        flowRate = flowRate / calibrationFactor;
      }
      previousMillis = millis();
    
      if(flowRate != 0.00){
        flowLitres = (flowRate / 60);
      
        totalLitres = readData(r[2]).toFloat()+flowLitres;
        eeprom_write_page(DEVADDR, w[2], dataS(String(totalLitres)),32);
        
        Serial.print("Flow rate: ");
        Serial.print(float(flowRate));  // Print the integer part of the variable
        Serial.print("L/min");
        Serial.print("\t");       // Print tab space
  
        Serial.print(totalLitres);
        Serial.println("L");
  
        clearLitre();
  
        String data_Litre = readData(r[2]);
        displaychar("V: "+data_Litre+" L", 0,10,2);
        displaychar("U: "+String(data_Litre.toFloat()/1000,4), 0,30,2);
        displayTestWifi();
      }
    }
  }
}
