#include  <Wire.h>
#include  <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Ticker.h>
#include "ThingSpeak.h"

//____________________IFTTT webhook init_____________
const String IFTTT_API_KEY = "cHtnx6MJkEHgk9V_0Z9vKG";
const String IFTTT_EVENT_1 = "pump_on";
const String IFTTT_EVENT_2 = "pump_off";

//______________Email IFTTT_________
const String EMAIL_API_KEY = "cUelwmOCMc7vDnRWkeA49G";
const String EMAIL_EVENT_1 = "email_on";
const String EMAIL_EVENT_2 = "email_off";

Ticker ticker;
WiFiClient  tk_client;
#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // ESP32 DOES NOT DEFINE LED_BUILTIN
#endif
int LED = LED_BUILTIN;
unsigned long myChannelNumber = 1265388;
const char * myWriteAPIKey = "899B2S4PWEL28ZRE";

#define RELAY_ON LOW
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display


const long utcOffsetInSeconds = 25200; // UTC +7.00 -> 7*60*60

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

//___________________Sensor and Pumb____________________
const int relay_pin = D8;
const int sensor_pin = A0;
float trigger_humidity = 10;
bool relay_status = (!RELAY_ON);
bool prev_relay_status = (!RELAY_ON);
//___________Webhook trigger________
void trigger_webhook(String event, String value){
  HTTPClient http;
  http.begin("http://maker.ifttt.com/trigger/" + event + "/with/key/" + IFTTT_API_KEY + "?value1=" + value);
  http.GET();
  http.end();
  Serial.println(event + " notification triggered!");
}

void email_webhook(String event, String value){
  HTTPClient http;
  http.begin("http://maker.ifttt.com/trigger/" + event + "/with/key/" + EMAIL_API_KEY + "?value1=" + value);
  http.GET();
  http.end();
  Serial.println(event + " email webhooks triggered!");
}
void tick()
{
  //toggle state
  digitalWrite(LED, !digitalRead(LED));     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}
//______________MQTT______________

const char* mqtt_server = "broker.mqtt-dashboard.com"; // default

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("iot12/lcd_backlight");
      client.subscribe("iot12/trigger_value");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  if (String(topic) == "iot12/lcd_backlight") {
    Serial.print("LCD Backlight: ");
    if(messageTemp == "on"){
      Serial.println("on");
      lcd.backlight();
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      lcd.noBacklight();
    }
  }
  else if (String(topic) == "iot12/trigger_value"){
    float val = messageTemp.toFloat();
    if (val >=0 && val <=100){
      trigger_humidity = val;
      Serial.print("Trigger limit set to: ");
      Serial.println(val);
    }
    else Serial.println("Invalid setting!!!");
  }
}
void send_message(String msg){
  if (!client.connected()) {
    reconnect();
  }
  char buf[10];
  msg.toCharArray(buf, msg.length()+1);
  Serial.println("Sending: iot/action -> " + msg);
  client.publish("iot12/action", buf);  
}

void send_num(float value){
  if (!client.connected()) {
    reconnect();
  }
  char buf[10];
  String pubString = String(value);
  pubString.toCharArray(buf, pubString.length()+1);
  Serial.println("Sending: iot/humidity -> " + pubString + "%");
  client.publish("iot12/humidity", buf);  
}


void displaytime(){
  lcd.clear(); 
  timeClient.update();
  String formattedDate, dayStamp, timeStamp;
  
  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  lcd.setCursor(0,0);
  lcd.print("Date: ");
  lcd.print(dayStamp);
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  lcd.setCursor(0,1);
  lcd.print("Time: ");
  lcd.print(timeStamp);
  Serial.println(timeStamp);
}
void setup(){
  //___________Pin setup_______________
  Serial.begin(115200);
  pinMode(LED, OUTPUT);  
  pinMode(relay_pin, OUTPUT);
  digitalWrite(relay_pin, (!RELAY_ON)); // turn off relay by default
  pinMode(sensor_pin, INPUT);
  //_________________________________________________
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  WiFiManager wm;

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wm.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  Serial.println("connected...yeey :)");
  ticker.detach();
  //keep LED on
  digitalWrite(LED, LOW);
  //_________________LCD Plashscreen_________________________________
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("HCMUS_IOTProject");
  lcd.setCursor(0,1);
  lcd.print("by Team 12");
  //________________________________________________
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  timeClient.begin();
  ThingSpeak.begin(tk_client);  // Initialize ThingSpeak
  
  //delay(1000);
}
int count_mode = 0;
void loop()
{  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if (count_mode < 10 && relay_status != RELAY_ON){
    displaytime();// dont display time when the pump is running
  }
  else{
    lcd.clear(); 
    lcd.setCursor(0,0);
    lcd.print("Soil Humidity");
    lcd.setCursor(0,1);
    // read the input on analog pin 0:
    int sensorValue = 0;
    for (int i = 0; i < 16; ++i)
      sensorValue += analogRead(sensor_pin);
    sensorValue /= 16;
    Serial.println(sensorValue);
    float humidity = (100 - ((sensorValue / 1024.00) * 100));
    //---------------HUMIDITY------------
    int x = ThingSpeak.writeField(myChannelNumber, 1, humidity, myWriteAPIKey);
    if(x == 200){
      Serial.println("[Channel update successful.]");
    }
    else{
      Serial.println("[Problem updating channel. HTTP error code " + String(x));
    }
    lcd.print(humidity);
    send_num(humidity);
    lcd.print("%");
    if (humidity < trigger_humidity)
      relay_status = (RELAY_ON);
    else
      relay_status = (!RELAY_ON);  
    if (prev_relay_status != relay_status){
      digitalWrite(relay_pin, relay_status);
      lcd.init();                           // initialize the lcd 
      lcd.backlight();
      lcd.clear();  
      if (relay_status == RELAY_ON){       
        lcd.setCursor(0,0);
        lcd.print("Start pump");
        send_message("Start pump");
        trigger_webhook(IFTTT_EVENT_1, String(humidity));
        email_webhook(EMAIL_EVENT_1, String(humidity));
      }
      else{
        lcd.setCursor(0,0);
        lcd.print("Stop pump");     
        send_message("Stop pump"); 
        trigger_webhook(IFTTT_EVENT_2, String(humidity));
        email_webhook(EMAIL_EVENT_2, String(humidity));
      }
      //delay(1000);
    }
    
    count_mode = 0;
  }
  prev_relay_status = relay_status;

  count_mode++;
  delay(2000);
}
