#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "DHT.h"

#define Relay1_Pin D8
#define Relay2_Pin D2
#define Door1Closed_Pin D0
#define Door2Closed_Pin D6
#define Motion_PIN D1  
#define DHTPIN D4     
#define TRIGGER D3
#define ECHO1 D5
#define ECHO2 D7

//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);


// Update these with values suitable for your network.
const char* ssid = "WifiSSID";				//Wifi SSID
const char* password = "WifiPassword";		//Wifi Password
const char* mqtt_server = "192.168.1.1";	//MQTT Server IP Address

const char* mqtt_user = "MQTTUserName";		//MQTT UserName
const char* mqtt_pass = "MQTTPassword";		//MQTT Password

#define pubTopicStatus "/garage/Status/"
#define subTopic "/garage/#"                //subscribe Topic
#define door_topic1_cmd "/garage/Door1/cmd/"
#define door_topic2_cmd "/garage/Door2/cmd/"
#define LightThresholdTopic "/garage/Light/Threshold/"

//-----For OTA-------//
#define SENSORNAME "OTASensorName"
#define OTApassword "OTAPassword" 
int OTAport = 8266;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char T_msg[50];
char H_msg[50];
int ReportDelay = 300000;
float f;        //Temp in F
char str_temp[6];
float h;        //% Humidity
char str_humid[6];
long lightmills = 0;
int LightThreshold = 400;
int LightValue = 0;
int FailDHTcount = 0;
long distance101, distance102, duration1, distance1, duration2, distance2, duration3, distance3, duration4, distance4, duration5, distance5;
const int BUFFER_SIZE = 300;

char* door_state1 = "UNDEFINED";
char* door_state2 = "UNDEFINED";
char* lastDoor_state1 = "";
char* lastDoor_state2 = "";
char* light_state = "UNDEFINED";
char* lastLight_state = "";
char* motion_state = "UNDEFINED";
char* lastMotion_state = "";
String strPayload;
String strTopic;
long rssi;



void setup() {
  pinMode(Door1Closed_Pin, INPUT);
  pinMode(Door2Closed_Pin, INPUT);
  pinMode(Motion_PIN, INPUT);
  pinMode(Relay1_Pin, OUTPUT);
  pinMode(Relay2_Pin, OUTPUT);
  pinMode(A0, INPUT);
  digitalWrite(Relay1_Pin, LOW);
  digitalWrite(Relay2_Pin, LOW);
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(ECHO2, INPUT);

  Serial.begin(115200);
  ArduinoOTA.setPort(OTAport);
  ArduinoOTA.setHostname(SENSORNAME);
  ArduinoOTA.setPassword((const char *)OTApassword);
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();
  readDHT();

  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP adress: ");
  Serial.println(WiFi.localIP());
  reconnect();
}

void setup_wifi() {

  delay(10);
  // Connect to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  strPayload = "";
  for (int i = 0; i < length; i++) strPayload += (char)payload[i];
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(strPayload);
  
  strTopic = String((char*)topic);
  if (strTopic == door_topic1_cmd) ButtonDoor1();
  else if (strTopic == door_topic2_cmd) ButtonDoor2();
  else if (strTopic == LightThresholdTopic) SetLightThreshold();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client";
    clientId += String(random(0xffff), HEX);
    clientId += String(millis());
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish(pubTopicTemp, "hello world");
      // ... and resubscribe
      client.subscribe(subTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
  ArduinoOTA.handle();
  if (!client.connected()) reconnect();
  
  client.loop();
  checkDoors();
  checkLight();
  checkMotion();
  
  long now = millis();
  if (now - lastMsg > ReportDelay) {
    readDHT();
    PingEcho();

//Uncomment the following code for debug help
/* Serial.println("Publishing Temp");
    client.publish(pubTopicTemp, T_msg);
    client.loop();
    Serial.println("Publishing Humid");
    client.publish(pubTopicHumid, H_msg);
    client.loop();
 
    Serial.println("Publishing Door1");
    client.publish(door_topic1, door_state1);  //pub every minute, regardless of a change.
    //client.loop();
    Serial.println("Publishing Door2");
    client.publish(door_topic2, door_state2);  //pub every minute, regardless of a change.
    //client.loop();
    Serial.println("Publishing Light");
    client.publish(pubTopicLight, light_state);
    //client.loop();
    Serial.println("Publishing Motion");
    client.publish(pubMotion, motion_state);
    //client.loop();
    Serial.println("Publishing DONE");
*/
    Serial.print("Door 1 is: ");
    Serial.println(door_state1);
    Serial.print("Door 2 is: ");
    Serial.println(door_state2);
    Serial.print("Lights are ");
    Serial.print(light_state);
    Serial.println(LightValue);
    Serial.print("Motion is ");
    Serial.println(motion_state);
    rssi   = WiFi.RSSI();
    Serial.print("RSSI:");
    Serial.println(rssi);
    lastMsg = now;
    sendState();
  }
}

/****reset***/
void software_Reset(){ // Restarts program from beginning but does not reset the peripherals and registers
  Serial.print("resetting");
  ESP.reset(); 
}


void readDHT() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  h = dht.readHumidity();
  dtostrf(h, 4, 2, str_humid);
  f = dht.readTemperature(true);        // Read temperature as Fahrenheit (isFahrenheit = true)
  dtostrf(f, 4, 2, str_temp);
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    FailDHTcount = FailDHTcount + 1;
    if (FailDHTcount > 10) software_Reset();
    return;
  }
  FailDHTcount = 0;
  float hif = dht.computeHeatIndex(f, h);     // Compute heat index in Fahrenheit (the default)
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hif);
  Serial.println(" *F");

  snprintf (T_msg, 75, "%s", str_temp);
  snprintf (H_msg, 75, "%s", str_humid);
  Serial.print("Publish message: Temp ");
  Serial.print(T_msg);
  Serial.print(", Humid ");
  Serial.println(H_msg);
}


void checkDoors() {
  //Checks if the door state has changed, and MQTT pub the change
  if (digitalRead(Door1Closed_Pin)) door_state1 = "off";
  else door_state1 = "on";

  if (digitalRead(Door2Closed_Pin)) door_state2 = "off";
  else door_state2 = "on";

  if (door_state1 != lastDoor_state1) {
    lastDoor_state1 = door_state1;
    sendState();
    Serial.print("Door 1 is: ");
    Serial.println(door_state1);
  }
  if (door_state2 != lastDoor_state2) {
    lastDoor_state2 = door_state2;
    sendState();
    Serial.print("Door 2 is: ");
    Serial.println(door_state2);
  }
}

void ButtonDoor1(){
  if (strPayload == "OPEN"){
    digitalWrite(Relay1_Pin, HIGH);
    Serial.println("Relay1 ON");
    delay(500);
    digitalWrite(Relay1_Pin, LOW);
    Serial.println("Relay1 OFF");
  }
  else if (strPayload == "LIGHT"){
    digitalWrite(Relay2_Pin, HIGH);
    Serial.println("Relay2 ON");
    delay(500);
    digitalWrite(Relay2_Pin, LOW);
    Serial.println("Relay2 OFF");
  }
}

void ButtonDoor2(){
  if (strPayload == "OPEN"){
    digitalWrite(Relay2_Pin, HIGH);
    Serial.println("Relay2 ON");
    delay(500);
    digitalWrite(Relay2_Pin, LOW);
    Serial.println("Relay2 OFF");
  }
  else if (strPayload == "LIGHT"){
    digitalWrite(Relay2_Pin, HIGH);
    Serial.println("Relay2 ON");
    delay(50);
    digitalWrite(Relay2_Pin, LOW);
    Serial.println("Relay2 OFF");
  }
}


void checkLight(){
  //Serial.println("Checking Light");
  int LightDelay = 5;
  if (millis() - lightmills > LightDelay) {
    lightmills = millis();
    LightValue = analogRead(A0);
    if (LightValue > LightThreshold) light_state = "ON";
    else light_state = "OFF";
    //Serial.println(analogRead(A0));
  }
  if (light_state != lastLight_state) {
    lastLight_state = light_state;
    //client.publish(pubTopicLight, light_state, true);
    sendState();
    Serial.print("Lights are ");
    Serial.println(light_state);
  }
}

void SetLightThreshold(){
  Serial.print("Set Light ON/OFF Threshold to: ");
  Serial.println(strPayload.toInt());
  LightThreshold = strPayload.toInt();
}


void checkMotion(){
  //Serial.println("Checking Motion");
  if (digitalRead(Motion_PIN)) motion_state = "ON";
  else motion_state = "OFF";

  if (motion_state != lastMotion_state){
    lastMotion_state = motion_state;
    if (motion_state == "ON") PingEcho();
    //client.publish(pubMotion, motion_state, true);
    sendState();
    Serial.print("Motion is ");
    Serial.println(motion_state);
  }
}


void PingEcho(){

//This is really ugly and I now know a better way to do this with a loop,
//but it works now and I didn't bother changing it. 

  //////Ping ECHO1//////
  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration1 = pulseIn(ECHO1, HIGH);
  distance1 = (duration1/2) / 29.1;
  delay(500);

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration2 = pulseIn(ECHO1, HIGH);
  distance2 = (duration2/2) / 29.1;
  delay(500);

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration3 = pulseIn(ECHO1, HIGH);
  distance3 = (duration3/2) / 29.1;
  delay(500);

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration4 = pulseIn(ECHO1, HIGH);
  distance4 = (duration4/2) / 29.1;
  delay(500);

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration5 = pulseIn(ECHO1, HIGH);
  distance5 = (duration5/2) / 29.1;
  delay(500);

  distance101 = ((distance1 + distance2 + distance3 + distance4 + distance5)/5);
 
  Serial.print("Echo Centimeter:");
  Serial.println(distance101);


  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration1 = pulseIn(ECHO2, HIGH);
  distance1 = (duration1/2) / 29.1;
  delay(500);

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration2 = pulseIn(ECHO2, HIGH);
  distance2 = (duration2/2) / 29.1;
  delay(500);

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration3 = pulseIn(ECHO2, HIGH);
  distance3 = (duration3/2) / 29.1;
  delay(500);

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration4 = pulseIn(ECHO2, HIGH);
  distance4 = (duration4/2) / 29.1;
  delay(500);

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIGGER, LOW);
  duration5 = pulseIn(ECHO2, HIGH);
  distance5 = (duration5/2) / 29.1;
  delay(500);

  distance102 = ((distance1 + distance2 + distance3 + distance4 + distance5)/5);
 
  Serial.print("Echo Centimeter:");
  Serial.println(distance102);

}


void sendState(){
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["door1state"] = (String)door_state1;
  root["door2state"] = (String)door_state2;
  root["temperature"] = (String)T_msg;
  root["humidity"] = (String)H_msg;
  root["motion"] = (String)motion_state;
  root["ldr"] = (String)LightValue;
  root["lights"] = (String)light_state;
  root["Echo1cm"] = (String)distance101;
  root["Echo2cm"] = (String)distance102;
  root["rssi"] = (String)rssi;
  
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  Serial.println(buffer);
  client.publish(pubTopicStatus, buffer, true);
}
