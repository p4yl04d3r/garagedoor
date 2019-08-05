/*
 * Garage door controller in arduino on ESP8266
 * Compile with Board ESP12-E NodeMCU 1.0
 * 
 * Arduino IDE: 1.8.9
 * Required Libraries:
 *    Time and TimeAlarms 1.5.0
 *    PubSubClient 2.6.0
 * Board manager:
 *    esp8266 2.5,2
 */
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <TimeAlarms.h>

int debug = 0;              /* Set this to 1 for serial debug output */
/* Configure GPIO */
const int led     = 5;      /* GPIO5 connect to LED */
const int garage1 = 4;      /* GPIO4  connect to garage door1 sense switch */
const int garage2 = 13;     /* GPIO13 connect to garage door2 sense switch */
const int button1 = 14;     /* GPIO14 connect to garage door1 openner button */
const int button2 = 12;     /* GPIO12 connect to garage door2 openner button */

String door1="unknown";     /* Initial state of door1 */
String door2="unknown";     /* Initial state of door2 */
volatile int state1 = 0;    /* Initial door1 state */
volatile int state2 = 0;    /* Initial door2 state */

/* WIFI setup */
const char* ssid     = "your_wifi_ssid";         /* WIFI SSID */
const char* password = "your_wifi_password";     /* WIFI password */

/* MQTT setup */
const char* mqtt_server = "xxx.xxx.xxx.xxx";            /* IP of mqtt server */
const char* topic_garage1 = "OpenHab/garage/status1";   /* Topic for door1 status */
const char* topic_garage2 = "OpenHab/garage/status2";   /* Topic for door2 status */
const char* topic_button = "OpenHab/garage/button";     /* Subscribed topic for garage door button */
/***********************************************************************************/
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  pinMode(led, OUTPUT);        /* In house status LED */
  pinMode(button1, OUTPUT);    /* Garage1 door opener button */
  pinMode(button2, OUTPUT);    /* Garage2 door opener button */
  pinMode(garage1, INPUT);     /* Garage1 door status switch */
  pinMode(garage2, INPUT);     /* Garage2 door status switch */
  digitalWrite(led, HIGH);     /* Set LED high as feedback   */
  digitalWrite(button1, HIGH); /* Set value HIGH for relay default off */
  digitalWrite(button2, HIGH); /* Set value HIGH for relay default off */
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  float vdd = ESP.getVcc() / 1000.0;  /* Read VCC voltage */
  debug and Serial.print("Voltage: ");
  debug and Serial.println(vdd);
  state1 = digitalRead(garage1);                    // Initial door1 state
  state2 = digitalRead(garage2);                    // Initial door2 state
  Alarm.timerOnce(10, MQTT_RECONNECT);              // Connect 10 seconds after startup
  Alarm.timerRepeat(300, MQTT_RECONNECT);            // Check connection to MQTT server every 5 minutes.
  Alarm.timerRepeat(900, MQTT_PUBLISH);             // Send door state every 15 minutes.
}

void setup_wifi() {
  delay(10);
  WiFi.disconnect();
  // We start by connecting to a WiFi network
  debug and Serial.println();
  debug and Serial.print("Connecting to ");
  debug and Serial.println(ssid);
  WiFi.mode(WIFI_STA);         /* Client only */
  WiFi.begin(ssid, password);
  int retries = 0;
  int tries = 60;
  //Wait for Wifi connection for 5 minutes before resetting ESP.
  while ((WiFi.status() != WL_CONNECTED) && (retries < tries)) {
    retries++;
    delay(5000);
    Serial.print(".");
  } 
  if (retries >= tries) {
    Serial.print("Reset ESP, No wifi");
    ESP.restart();
  }
  debug and Serial.println("");
  debug and Serial.println("WiFi connected");
  debug and Serial.println("IP address: ");
  debug and Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived: [");
  debug and Serial.print(topic);
  debug and Serial.print(" ]");
  char *payload_string = (char *) payload;
  payload_string[length] ='\0';
  Serial.println(payload_string);
  
  // Open door1 if payload says "open1"
  if (strcmp(payload_string,"open1") == 0) {
    digitalWrite(button1, LOW);   // Toggle Relay for 2 seconds
    Alarm.delay(1100);
    digitalWrite(button1, HIGH);  // Relay is normaly open on HIGH
  }
  // Open door2 if payload says "open2"
  if (strcmp(payload_string,"open2") == 0) {
    digitalWrite(button2, LOW);   // Toggle Relay for 2 seconds
    Alarm.delay(1100);
    digitalWrite(button2, HIGH);  // Relay is normaly open on HIGH
  }
}

void MQTT_PUBLISH() {
  client.publish(topic_garage1, (char *) door1.c_str());
  client.publish(topic_garage2, (char *) door2.c_str());
}

void MQTT_RECONNECT() {
  debug and Serial.println("Check MQTT connection...");
  int retries = 0;
  int tries = 12;
  // Loop every 5 seconds until MQTT broker is connected. After 1 minute check Wifi.
  while ((!client.connected()) && (retries < tries)) { 
    retries++;
    debug and Serial.print("Attempting MQTT connection... ");
    if (client.connect("ESP8266Client")) {
      debug and Serial.println(" Connected");
      client.subscribe(topic_button);   // Attempt to subscribe to topic
      MQTT_PUBLISH();                   // Publish door status if we have to reconnect because server may not maintain state.
    } else {
      debug and Serial.print("failed, rc=");
      debug and Serial.println(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  // Reconnect to wifi if we cannot get to MQTT Broker
  if (retries >= tries) {
    Serial.println("Could not establish MQTT connection. Reconnect WiFi.");
    setup_wifi();
  }
}

void loop() {
  client.loop();
  // Status LED
  if ((digitalRead(garage1) == HIGH) || (digitalRead(garage2) == HIGH)) {
    digitalWrite(led, HIGH);
  } else {
    digitalWrite(led, LOW);
  }
  /* Door 1 */
  if ((digitalRead(garage1) == HIGH) && (state1 == 1)) {
    door1="open";
    client.publish(topic_garage1, (char *) door1.c_str());
    debug and Serial.println("Door1 is open");
    state1 = 0;
  } 
  if ((digitalRead(garage1) == LOW) && (state1 == 0)) {
    door1="closed";
    client.publish(topic_garage1, (char *) door1.c_str());
    debug and Serial.println("Door1 is closed");    
    state1 = 1;
  }
  /* Door 2 */
  if ((digitalRead(garage2) == HIGH) && (state2 == 1)) {
    door2="open";
    client.publish(topic_garage2, (char *) door2.c_str());
    debug and Serial.println("Door2 is open");
    state2 = 0;
  }
  if ((digitalRead(garage2) == LOW) && (state2 == 0)) {
    door2="closed";
    client.publish(topic_garage2, (char *) door2.c_str());
    debug and Serial.println("Door2 is closed");
    state2 = 1;
  }
  Alarm.delay(20);
  ESP.wdtFeed();
}
