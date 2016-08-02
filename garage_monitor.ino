/*
 * Garage door controller in arduino
 * Board ESP12-E NodeMCU 1.0
 */
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//ADC_MODE(ADC_VCC);   /* Setup ADC pin to read VCC voltage (broken in current CC */
/* Configure GPIO */
const int led     = 5;      /* GPIO5 connect to LED */
const int garage1 = 4;      /* GPIO4  connect to garage door1 sense switch */
const int garage2 = 13;     /* GPIO13 connect to garage door2 sense switch */
const int button1 = 14;     /* GPIO14 connect to garage door1 openner button */
const int button2 = 12;     /* GPIO12 connect to garage door2 openner button */

volatile int state1 = 0;
volatile int state2 = 0;

/* WIFI setup */
const char* ssid     = "<ssid>";           /* WIFI SSID */
const char* password = "<password>";       /* WIFI password */

/* MQTT setup */
const char* mqtt_server ="<mqtt_broker_IP>";             /* Set to the IP of your MQTT broker */
const char* topic_garage1 = "OpenHab/garage/status1";    /* Garage door1 status topic */
const char* topic_garage2 = "OpenHab/garage/status2";    /* Garage door2 status topic */
const char* topic_button = "OpenHab/garage/button";      /* Door button topic */
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
  Serial.print("Voltage: ");
  Serial.println(vdd);
  state1 = digitalRead(garage1);
  state2 = digitalRead(garage2);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
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
  Serial.print("Message arrived: [");
  Serial.print(topic);
  Serial.print(" ]");
  char *payload_string = (char *) payload;
  payload_string[length] ='\0';
  Serial.println(payload_string);
  
  // Open door1 if payload says "open1"
  if (strcmp(payload_string,"open1") == 0) {
    digitalWrite(button1, LOW);   // Toggle Relay for 2 seconds
    delay(1100);
    digitalWrite(button1, HIGH);  // Relay is normaly open on HIGH
  }
  // Open door2 if payload says "open2"
  if (strcmp(payload_string,"open2") == 0) {
    digitalWrite(button2, LOW);   // Toggle Relay for 2 seconds
    delay(1100);
    digitalWrite(button2, HIGH);  // Relay is normaly open on HIGH
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      client.subscribe(topic_button);
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
  if (!client.connected()) {
     delay(10);
    /* 
     *  If we lose connection to the Broker then there is a good chance it lost state.
     *  Reset the state of the doors.
     */
     state1 = digitalRead(garage1);
     state2 = digitalRead(garage2);
     reconnect();
  }
  client.loop();

  // Status LED
  if ((digitalRead(garage1) == HIGH) || (digitalRead(garage2) == HIGH)) {
    digitalWrite(led, HIGH);
  } else {
    digitalWrite(led, LOW);
  }
  if ((digitalRead(garage2) == HIGH) && (state2 == 1)) {
    client.publish(topic_garage2, "open");
    Serial.println("Door2 is open");
    state2 = 0;
  }
  if ((digitalRead(garage2) == LOW) && (state2 == 0)) {
    client.publish(topic_garage2, "closed");
    Serial.println("Door2 is closed");
    state2 = 1;
  }
  if ((digitalRead(garage1) == HIGH) && (state1 == 1)) {
    client.publish(topic_garage1, "open");
    Serial.println("Door1 is open");
    state1 = 0;
  } 
  if ((digitalRead(garage1) == LOW) && (state1 == 0)) {
    client.publish(topic_garage1, "closed");
    Serial.println("Door1 is closed");    
    state1 = 1;
  }
  delay(20);
}
