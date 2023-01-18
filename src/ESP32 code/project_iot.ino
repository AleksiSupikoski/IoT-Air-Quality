//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
// - Read data from MQ2 air quality sensor & compute AQI according 
// to received treshold values, & reads humidity & temperature from 
// DHT11 sensor.
// - Send data via MQTT or HTTP, switchable with telegram bot.
// - Data send period (freq) managed with elapsedMillis-class timer. 
// can be modified via telegram bot (MQTT server to end-device)
//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
#include <WiFi.h>
#include <PubSubClient.h>
#include <elapsedMillis.h>
#include "HTTPClient.h"
// DHT11
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTPIN 14     // Digital pin connected to the DHT sensor
#define DHTTYPE    DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;
//moving avg
#include <movingAvg.h> 

// json parser 
#include <ArduinoJson.h>

//ID and GPS location: ID, Latitude + Longitude (Hard-Coded)
String ID = "ESP32_001";
String GPS_lat = "44.507393";
String GPS_lon =  "11.356048"; 


// MQ2
int Gas_analog = 35;    // used for ESP32
//int Gas_digital = 17;   // used for ESP32
int MAX_GAS_VALUE = 3500;
int MIN_GAS_VALUE = 3000;
//int rec_vals [5] = {0, 0, 0, 0, 0};


String HttpADDR = "http://192.168.178.75:1880/update-sensors"; //"http://127.0.0.1:1880/update-sensors";

int protocol_switch = 0; // Switches protocol between MQTT and HTTP default = HTTP

// Manage data send period, default = 10000 msc
int sendPeriod = 10000;
elapsedMillis sendTimer;


// WiFi
const char *ssid =  "FRITZ!Box 7530 QA"; // Enter your WiFi name
const char *password = "87050750313171843171";  // Enter WiFi password

// MQTT Broker
const char *mqtt_broker = "192.168.178.75"; //
const char *topic = "send/data";
const char *topic_temp = "device/sensors/temp";
const char *topic_hum = "device/sensors/humidity";
const char *topic_gas = "device/sensors/gas";
const char *topic_aqi = "device/sensors/aqi";

const char *topic_wifi = "device/wifi";
const char *topic_ID= "device/ID";
const char *topic_GPS_lat= "device/GPS_lat";
const char *topic_GPS_lon= "device/GPS_lon";


const char *topic2 = "device/modify/freq";
const char *topic3 = "device/modify/protocolHttp";
const char *topic4 = "device/modify/MinAQI";
const char *topic5 = "device/modify/MaxAQI";
const char *topic6 = "device/modify/protocolMQTT";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

//moving average for AQI
movingAvg avgTemp(5);  
// JSON data buffer 
StaticJsonDocument<250> jsonDocument;


void setup() {

  
  // Set software serial baud to 115200;
  Serial.begin(115200);

  // connecting to a WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.print("Connected to the WiFi network. RSSI: ");
  Serial.println(WiFi.RSSI());


  // MQTT Broker: connecting to a mqtt broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public emqx mqtt broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  // publish and subscribe
  client.publish(topic, "ESP32 connected to MQTT");
  client.subscribe(topic);
  client.subscribe(topic2);
  client.subscribe(topic3);
  client.subscribe(topic4);
  client.subscribe(topic5);
  client.subscribe(topic6);

  client.subscribe(topic_temp);
  client.subscribe(topic_hum);
  client.subscribe(topic_gas);
  client.subscribe(topic_aqi);
  client.subscribe(topic_wifi);
  client.subscribe(topic_ID);
  client.subscribe(topic_GPS_lat);
  client.subscribe(topic_GPS_lon);

  //DHT11
  dht.begin();
  sensor_t sensor;
  delayMS = sensor.min_delay / 1000; //minimal delay in milliseconds FOR THE DHT SENSOR.

  // MQ2
  //pinMode(Gas_digital, INPUT);
  pinMode(Gas_analog, INPUT);
  //moving avg
  avgTemp.begin();

  HTTPClient http;   
   http.begin(HttpADDR);
   http.addHeader("Content-Type", "text/plain");            
   int httpResponseCode = http.POST("Test connection");   
   if(httpResponseCode>0){
    String response = http.getString();   
    Serial.println(httpResponseCode);
    Serial.println(response);         //print response (should be echo)     
   }else{
    Serial.print("Error on sending PUT Request: ");
    Serial.println(httpResponseCode);
   }
   http.end();

}

// MQTT Broker: callback func
void callback(char *topic, byte *payload, unsigned int length) {
 Serial.print("Message arrived in topic: ");
 Serial.println(topic);
 Serial.print("Message:");
 for (int i = 0; i < length; i++) {
     Serial.print((char) payload[i]);
 }
 Serial.println();
 Serial.println("-----------------------");

 //if (String(topic) == String("modify/protocol")) {
 // Serial.println("changing send_data protocol to ");
 // protocol_switch = !protocol_switch;
 // }

 if (String(topic) == String("device/modify/freq")) {
  String recData = "";
  Serial.println("changing send_data frequency.....");
  for (int i = 0; i < length; i++) {
     recData.concat((char) payload[i]);
  }
  Serial.println("Frequency received:");
  Serial.println(recData.toInt());
  sendPeriod = recData.toInt();
  }

  if (String(topic) == String("device/modify/protocolHttp")) {
  Serial.println("changing send_data protocol to HTTP");
  protocol_switch = 1;
  }

  
  if (String(topic) == String("device/modify/protocolMQTT")) {
  Serial.println("changing send_data protocol to MQTT");
  protocol_switch = 0;
  }

  if (String(topic) == String("device/modify/MinAQI")) {
  String recData1 = "";
  Serial.println("changing -<MIN>- AQI GAS VALUE.....");
  for (int i = 0; i < length; i++) {
     recData1.concat((char) payload[i]);
  }
  Serial.println("MIN AQI GAS VALUE received:");
  Serial.println(recData1.toInt());
  MIN_GAS_VALUE = recData1.toInt();
  }

  if (String(topic) == String("device/modify/MaxAQI")) {
  String recData = "";
  Serial.println("changing >-MAX-< AQI GAS VALUE.....");
  for (int i = 0; i < length; i++) {
     recData.concat((char) payload[i]);
  }
  Serial.println("MIN AQI GAS VALUE received:");
  Serial.println(recData.toInt());
  MAX_GAS_VALUE = recData.toInt();
  }
}

// Send data according to set timer and protocol
void send_data() {
  if (sendTimer >= sendPeriod) {
    sendTimer = 0;
    String data = "Connection successful";
    String temp = String(read_temp());
    String hum = String(read_humidity());
    String gas = String(read_mq2());
    String aqi = String(read_AQI());
    String rssi = String(WiFi.RSSI());
    Serial.print("Sending data... Current sending period is:");
    Serial.println(sendPeriod);
    Serial.print("Current MIN AQI GAS VALUE is:");
    Serial.println(MIN_GAS_VALUE);
    Serial.print("Current MAX AQI GAS VALUE is:");
    Serial.println(MAX_GAS_VALUE);
    
    if (protocol_switch == 0) {
        client.publish(topic, data.c_str()); //Sent data converted to C-string forthe method
        client.publish(topic_temp, temp.c_str());
        client.publish(topic_hum, hum.c_str());
        client.publish(topic_gas, gas.c_str());
        client.publish(topic_aqi, aqi.c_str());
        client.publish(topic_wifi, rssi.c_str());
        client.publish(topic_ID, ID.c_str());
        client.publish(topic_GPS_lat, GPS_lat.c_str());
        client.publish(topic_GPS_lon, GPS_lon.c_str());
        
        Serial.println("Data sent via MQTT");
      } else {
        String json;
        jsonDocument["temperature"] = temp;
        jsonDocument["humidity"] = hum;
        jsonDocument["gas"] = gas;
        jsonDocument["aqi"] = aqi;
        jsonDocument["rssi"] = rssi;
        jsonDocument["ID"] = ID;
        jsonDocument["GPS_lat"] = GPS_lat;
        jsonDocument["GPS_lon"] = GPS_lon;
        
        serializeJson(jsonDocument, json);
        serializeJson(jsonDocument, Serial);
        
        //HAVE TO ADD LOCATION AND GPS //added.
        http_send(json);
        Serial.println("Data sent vie HTTP");
      } 
  }
}


// Http send-receive data
void http_send(String data){//String temp, String hum, String gas, String aqi, String rssi){
 if(WiFi.status()== WL_CONNECTED){
   HTTPClient http;   
   http.begin(HttpADDR);
   http.addHeader("Content-Type", "application/json");            
   int httpResponseCode = http.POST(data);   
   if(httpResponseCode>0){
    String response = http.getString();   
    Serial.println(httpResponseCode);
    Serial.println(response);         //print response (should be echo)     
   }else{
    Serial.print("Error on sending PUT Request: ");
    Serial.println(httpResponseCode);
   }
   http.end();
  }else{
    Serial.println("Error in WiFi-http connection");
 }
}

int read_mq2(){
  //READ MQ2
  int gassensorAnalog = analogRead(Gas_analog);
  //int gassensorDigital = digitalRead(Gas_digital);//Broken? Giving 1 or 0 at certain analog value treshold. -> not needed
  Serial.print("\n Gas Sensor (analog): ");
  Serial.print(gassensorAnalog);
  //Serial.print("Gas Sensor (Digital): ");
  //Serial.print(gassensorDigital);
  return gassensorAnalog;
}

int read_AQI(){  
  int gas_analog = read_mq2();
  int avg = avgTemp.reading(gas_analog);
  Serial.println("; AVG = ");
  Serial.println(avg);
  int AQI = 2;
  if (avg >= MAX_GAS_VALUE) {  
    AQI = 0;
  }
  else if ((MIN_GAS_VALUE <= avg) && (avg <= MAX_GAS_VALUE)) {
    AQI = 1;
  }
  return AQI;
}

int read_temp(){
  // READ DHT11
  int temperature = 0;
  // Get temperature event and print its value.
  sensors_event_t event;

  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    temperature = event.temperature;
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
  }
  return temperature;
}

int read_humidity(){
  sensors_event_t event;
  int humidity = 0;
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    humidity = event.relative_humidity;
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
  }
  return humidity;
}

void loop() {
  client.loop();
  send_data();
  //int valread = analogRead(Gas_analog);
  //Serial.println("Gas Sensor (analog): -------------------");
  //Serial.print(valread);
  //delay(1000);
}
