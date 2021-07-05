#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <MQTT.h>

#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>

#define SUMMON_COFFEE   1
#define SUMMON_LUNCH    2
#define SUMMON_DINNER   3
#define SUMMON_TEA      4

#define PUBSUB_SERVER     "geoffs-mac-mini.local"
#define PUBSUB_PORT       18490
#define PUBSUB_CLIENT     "receiver2"
#define PUBSUB_USERNAME   "hsvtkhlx"
#define PUBSUB_PASSWORD   "ZEfZuUx4_UlK"
#define PUBSUB_TOPIC      "summon-test"

#define NUM_LEDS    4
#define LED_PIN     5
#define OFF_PIN     4

WiFiClient wifi;
MQTTClient mqtt;
CRGB leds[NUM_LEDS];

void writeLog(const char *ptext) {
  Serial.println(ptext);
}

void showCode(int code) {
  for (int n = 0; n < NUM_LEDS; n++) {
    if (n + 1 == code) {
      leds[n] = CRGB::White;
    } else {
      leds[n] = CRGB::Black;
    }
  }
  FastLED.show();
}

void clearCode() {
  for (int n = 0; n < NUM_LEDS; n++) {
    leds[n] = CRGB::Black;
  }
  FastLED.show();
}

void showWaiting() {
  for (int n = 0; n < NUM_LEDS; n++) {
    leds[n] = CRGB::Green;
  }
  FastLED.show();
}

void onMessage(String &topic, String &payload) {
  StaticJsonDocument<100> json;

  if (topic != PUBSUB_TOPIC) {
    return;
  }

  DeserializationError error = deserializeJson (json, payload);
  if (error) {
    Serial.println("Error");
    Serial.println(error.c_str());
    return;
  }

  long code = json["code"];

  char s[40];
  sprintf (s, "Received code: %ld", code);
  writeLog(s);

  showCode(code);
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected");
    return;
  }

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

  if (warioStrength > hobbyHouseStrength) {
    WiFi.begin("Wario", "mansion1");
    Serial.println("Connecting to Wario ");
  } else {
    WiFi.begin("HobbyHouse", "mansion1");
    Serial.println("Connecting to HobbyHouse ");
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

void setup() {
  Serial.begin(115200);
  Serial.println();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  showWaiting();
  Serial.println("Leds started");

  pinMode(OFF_PIN, INPUT_PULLUP);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  delay(100);
  connectWiFi();

  // Initialise MQTT
  mqtt.begin(PUBSUB_SERVER, /*PUBSUB_PORT,*/ wifi);
  mqtt.onMessage(onMessage);
  mqtt.setOptions(60, true, 5000);
  Serial.println("MQTT started");

  if (mqtt.connect(PUBSUB_CLIENT/*, PUBSUB_USERNAME, PUBSUB_PASSWORD*/)) {
    mqtt.subscribe(PUBSUB_TOPIC);
    writeLog("MQTT connected and subscribed");
  } else {
    writeLog("Error connecting to MQTT");
  }

  clearCode();
}

void loop() {
  mqtt.loop();

  if (!mqtt.connected()) {
    writeLog("MQTT disconnected");
    mqtt.disconnect();
    delay(3000);
    if (mqtt.connect(PUBSUB_CLIENT/*, PUBSUB_USERNAME, PUBSUB_PASSWORD*/)) {
      mqtt.subscribe(PUBSUB_TOPIC);
      writeLog("MQTT connected and subscribed");
    } else {
      writeLog("Error connecting to MQTT");
    }
  }

  // Check for "lights off" pressed
  if (digitalRead(OFF_PIN) == LOW) {
    clearCode();
  }

  delay(100);
}
