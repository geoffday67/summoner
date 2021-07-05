#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <MQTT.h>

#define DIGIT_PIN   4
#define DIAL_PIN    5
#define RED_LED     14
#define GREEN_LED   12

#define MQTT_SERVER     "geoffs-mac-mini.local"
#define MQTT_CLIENT     "summoner-dial"
#define SUMMON_TOPIC    "summon/dial"
#define ACK_TOPIC       "summon-ack/#"

WiFiClient wifi;
MQTTClient mqtt;

volatile int number = 0;
volatile boolean dialled = false;

unsigned long lastDial = 0;
unsigned long lastDigit = 0;
boolean waiting = false;
boolean ack = false;
char pendingId[16];

void getRandomString (char *presult, int length) {
  static char randomCharList[] = "abcdefghijklmnopqrstuvwxyz";
  int n;

  for (n = 0; n < length; n++) {
    presult[n] = randomCharList[random(0, 26)];
  }
  presult[n] = 0;
}

void showRed() {
  digitalWrite (RED_LED, HIGH);
  digitalWrite (GREEN_LED, LOW);
}

void showGreen() {
  digitalWrite (GREEN_LED, HIGH);
  digitalWrite (RED_LED, LOW);
}

ICACHE_RAM_ATTR void digitHandler() {
  if (millis() - lastDigit < 80) {
    return;
  }
  lastDigit = millis();

  if (!dialled) {
    number++;
  }
}

ICACHE_RAM_ATTR void dialHandler() {
  // Ignore dial interrupts before there's a nunber, they're probably falsely generated during the falling edge
  if (number == 0) {
    return;
  }

  if (millis() - lastDial < 80) {
    return;
  }
  lastDial = millis();

  dialled = true;
}

void connectWiFi() {
  showRed();

  if (WiFi.SSID().length() != 0) {
    Serial.print ("Connecting with existing parameters ");
    WiFi.reconnect();

  } else {
    int warioStrength = -999;
    int hobbyHouseStrength = -999;
    Serial.println("Scanning for networks");
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++)
    {
      if (WiFi.SSID(i) == "Wario") {
        warioStrength = WiFi.RSSI(i);
      }

      if (WiFi.SSID(i) == "HobbyHouse") {
        hobbyHouseStrength = WiFi.RSSI(i);
      }
    }

    Serial.printf("Wario strength: %d\n", warioStrength);
    Serial.printf("HobbyHouse strength: %d\n", hobbyHouseStrength);

    if (warioStrength > hobbyHouseStrength) {
      WiFi.begin("Wario", "mansion1");
      Serial.println("Connecting to Wario ");
    } else {
      WiFi.begin("HobbyHouse", "mansion1");
      Serial.println("Connecting to HobbyHouse ");
    }
  }

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  if (mqtt.connect(MQTT_CLIENT)) {
    mqtt.subscribe(ACK_TOPIC);
    Serial.println("MQTT connected and subscribed to ack");
    showGreen();
  } else {
    Serial.println("Error connecting to MQTT");
  }
}

void onMessage(String &topic, String &payload) {
  StaticJsonDocument<100> json;

  DeserializationError error = deserializeJson (json, payload);
  if (error) {
    Serial.println("Error!");
    Serial.println(error.c_str());
    return;
  }

  const char *pid = json["id"];
  Serial.print("Received ack id: ");
  Serial.println(pid);

  if (strcmp (pid, pendingId)) {
    Serial.println("Wrong id received");
    return;
  }

  ack = true;
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Turn off blue on-board LED
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  pinMode(DIAL_PIN, INPUT_PULLUP);
  attachInterrupt(DIAL_PIN, dialHandler, RISING);

  pinMode(DIGIT_PIN, INPUT_PULLUP);
  attachInterrupt(DIGIT_PIN, digitHandler, RISING);

  randomSeed(analogRead(0));

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  connectWiFi();

  // Initialise MQTT
  mqtt.begin(MQTT_SERVER, wifi);
  mqtt.onMessage(onMessage);
  mqtt.setOptions(60, true, 5000);
  Serial.println("MQTT started");
  connectMQTT();
}

void loop() {
  char message [64];

  mqtt.loop();

  if (!mqtt.connected()) {
    Serial.println("MQTT disconnected");
    mqtt.disconnect();
    connectWiFi();
    connectMQTT();
  }

  if (waiting) {
    if (ack) {
      ack = false;
      waiting = false;
      showGreen();
    }
  }

  if (dialled) {
    Serial.print("Dialled: ");
    Serial.println(number);

    showRed();
    getRandomString(pendingId, 10);
    sprintf(message, "{\"code\":%d,\"id\":\"%s\"}", number, pendingId);
    mqtt.publish (SUMMON_TOPIC, message);
    waiting = true;

    number = 0;
    dialled = false;
  }
}