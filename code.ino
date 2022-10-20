#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include <FirebaseESP8266.h>
#include <addons/RTDBHelper.h>
//#include <ArduinoJson.h>
#include <String.h>
#include <Wire.h>
#include "SH1106Wire.h"

#define SENSOR  14
#define RST  10
#define VALVE  13
#define host "aalu-pidalu-default-rtdb.firebaseio.com"
//#define api_key "7468798439c614bcc4ea45fe98c5b05815c04b42"
#define api_key "62pyh8BmfqlXYRTCBopoxtGkFxxYUqHGcTtGbSJk"
FirebaseData fbdo;

SH1106Wire display(0x3c, SDA, SCL);
//IPAddress ip(1,1,1,1);

int r[7] = {16,48,80,112,144,176,208}; //(ssid,pass,data_litre,state_valve,}
unsigned w[7] = {0x010,0x030,0x050,0x070,0x090,0x0B0,0x0D0};
const byte DEVADDR = 0x50;

String _SSID = "B2r";
String _pass = "aaaaaaaa";
String id = "/483fdaa43f4f";
String state_Received;

boolean rstVar = false;
//boolean creVar = false;
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
  pulseCount++;
}

void IRAM_ATTR rstLitre(){
  rstVar = true;
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

bool testWifi(int x = 0){
  IPAddress ip(1,1,1,1);
  if (WiFi.status() == WL_CONNECTED) {
    if ( Ping.ping(ip, 1)) {
      if (x){
        Serial.println("Internet Access");
      }
      return true;
    }
    else {
      if (x){
        Serial.println("No Internet");
      }
      return false;
    }
  }
  else
  {
    if (x){
      Serial.println("No Connection");
    }
    //changeState = 1;
    return false;
  }
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

/*bool strCmp(String x, String y){
  byte xLen = x.length();
  byte yLen = y.length();
  if (xLen != 0 and yLen != 0){
    if (xLen != yLen){
      //Serial.println("Returned coz of different length");
      return 0;
    }
    else{
      for (byte i = 0; i < xLen; i++){
        if ((char)x[i] != (char)y[i]){
          //Serial.print("Returned coz of different character from index: ");
          //Serial.println(i);
          //Serial.println((char)x[i]);
          //Serial.println((char)y[i]);
          return 0;
        }
      }
      return 1;
    }
  }
  else{
    //Serial.println("Returned coz of 0 length");
    return 0;
  }
  return 1;
}

void streamTimeoutCallback(bool timeout){
  if(timeout){
    Serial.println("Stream timeout, resume streaming...");
  }  
}

void begin_stream() {
  if (!Firebase.beginStream(fbdo, id + "/access")) {
    Serial.println(fbdo.errorReason());
  }
  else  {
    Serial.print("Successful: ");
    Serial.println(id + "/access");
    initialBeginVar = 0;
  }
  Firebase.reconnectWiFi(true);
  delay(100);
  Firebase.setReadTimeout(fbdo, 1000 * 5);
  Firebase.setwriteSizeLimit(fbdo, "tiny");
  Firebase.setStreamCallback(fbdo, streamCallback, streamTimeoutCallback);
  if (!Firebase.beginStream(fbdo, id+"/access"))
  {
    Serial.println(fbdo.errorReason());
  }
  else{
    Serial.println("Successful begin stream "+id+"/access");
    initialBeginVar = 0;
  }
}


void streamCallback(StreamData data){
  Serial.println("Stream Data...");
  //FirebaseJson *json = data.to<FirebaseJson *>();
  Serial.println(json->raw());
  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_string){
    state_Received = data.to<String>();
    Serial.println(state_Received);
    get_Stream();
  }
}

void get_Stream(){
  if (strCmp(state_Received, "EMPTY_STATE")){
    Serial.println("Acknowledge complete");
  }
  
  else if (strCmp(state_Received, "TAP_OFF")){
    valv = false;
    digitalWrite(VALVE, LOW);
    FirebaseData fset;
    FirebaseJson updateData;
    updateData.set("tap_state", 0);
    updateData.set("access", "EMPTY_STATE");
    updateData.set("device_state_changed_at/.sv", "timestamp");
    updateData.set("last_updated/.sv", "timestamp");
    
    if (Firebase.RTDB.updateNode(&fset, id, &updateData)) {
      Serial.println("Firebase Acknowledged successfully");
      Serial.println(fset.jsonString());
    } 
    else {
      Serial.println(fset.errorReason());
    }
    Serial.println("Tap Off");
  }
   
  else if (strCmp(state_Received, "WIFI_CHANGE_STATE")){
    touchVar = false;
    set_String();
    FirebaseData fbdo;
    String received = "";
    Serial.println("Credential Change From Firebase");
    if (Firebase.getJSON(fbdo, id+"/credentials")){
      received = fbdo.jsonString();
      Serial.println(received);
      DynamicJsonBuffer JSONBuffer;
      JsonObject& parsed= JSONBuffer.parseObject(received);
      const String wifi_ssid = parsed["wifi_ssid"];
      const String wifi_pass = parsed["wifi_pass"];
      Serial.println(wifi_ssid);
      Serial.println(wifi_pass);
      if (!strCmp(wifi_ssid, readData(r[0])) || !strCmp(wifi_pass, readData(r[1]))){
        eeprom_write_page(DEVADDR, w[0], dataS(wifi_ssid), 32);
        eeprom_write_page(DEVADDR, w[1], dataS(wifi_pass), 32);
        hotVar = true;
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        byte c = 0;
        while (WiFi.status() != WL_CONNECTED) {
          Serial.print(".");
          delay(200);
          c++;
          if (c > 35){
            break;
          }
        }
      }
      else{
        Serial.print("Same Credentials Given");
      }
      current_time = millis();
    }
    else{
      Serial.println(fbdo.errorReason());
    }
    touchVar = true;
  }
}*/

void setup(){
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
  pinMode(VALVE, OUTPUT);
  digitalWrite(VALVE, LOW);

  String ssid = readData(r[0]), password = readData(r[1]);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Smart Water Meter", "password");         //starting AccessPoint on given credential
  IPAddress myIP = WiFi.softAPIP();        //IP Address of our Esp8266 accesspoint(where we can host webpages, and see data)
  Serial.print("Access Point IP address: ");
  Serial.println(myIP);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  Serial.println("SSID: "+ssid+"Pass: "+password);

  Firebase.begin(host, api_key);
  Firebase.reconnectWiFi(true);
  
  Serial.print("Output Liquid Quantity: ");
  Serial.print(totalMilliLitres);
  Serial.print("mL / ");
  Serial.print(totalLitres);
  Serial.println("L");

  delay(2000); // Pause for 2 seconds

  display.clear();
  displaychar("Smart Water Meter", 20,0,1);
  String data_Litre = readData(r[2]);
  displaychar("V: "+data_Litre+" L", 0,10,2);
  displaychar("U: "+String(data_Litre.toFloat()/1000,4), 0,30,2);
  displayTestWifi();
}

void loop() {
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
  if (currentMillis2 - previousMillis2 > interval2){
    Serial.println("10 minute");
    if (testWifi()){
      if (initialBeginVar){
          initialBeginVar = 0;
          //begin_stream();
      }
    }
    String data_Litre = readData(r[2]);
    FirebaseData fset;
    FirebaseJson json;
    json.set("last_updated/.sv","timestamp");
    json.set("litre_consumed",data_Litre);
    Serial.printf("Push json... %s\n", Firebase.RTDB.pushJSON(&fset, id+"/data", &json) ? "ok" : fset.errorReason().c_str());
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
   
      //totalLitres += flowLitres;
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
