#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <MQTT.h>

#define STOP_PIN        2
#define BACKLIGHT_PIN   4
#define SPEAKER_PIN     5

#define ALARM_TIMEOUT   20000L

#define SUMMON_MESSAGE  0
#define SUMMON_COFFEE   1
#define SUMMON_LUNCH    2
#define SUMMON_TEA      3
#define SUMMON_DINNER   4

#define MQTT_SERVER     "geoffs-mac-mini.local"
#define MQTT_CLIENT     "summoner-gameboy"
#define SUMMON_TOPIC    "summon/gameboy"

// External display methods
void beginDisplay();
void drawText(const char *ptext);
void drawBmp(const char *filename, int16_t x, int16_t y);

int note[] = {440, 466, 493, 523, -1};
int duration[] = {200, 200, 200, 600};
bool tuneSounding = false;
int tuneIndex;
unsigned long noteStart;

WiFiClient wifi;
MQTTClient mqtt;

bool alarm = false;
long alarmStart;
char message[64];

void startTune() {
  tuneIndex = 0;
  tone(SPEAKER_PIN, note[0]);
  noteStart = millis();
  tuneSounding = true;
}

void stopTune() {
  noTone(SPEAKER_PIN);
  tuneSounding = false;
}

void loopTune() {
  if (!tuneSounding || millis() - noteStart < duration[tuneIndex]) {
    return;
  }

  tuneIndex++;

  if (note[tuneIndex] == -1) {
    noTone(SPEAKER_PIN);
    tuneSounding = false;
  } else {
    tone (SPEAKER_PIN, note[tuneIndex]);
    noteStart = millis();
  }
}

void startAlarm() {
  alarm = true;
  alarmStart = millis();

  // Show picture if message begins with /, show text otherwise.
  if (message[0] == '/') {
    drawBmp(message, 0, 0);
  }
  digitalWrite(BACKLIGHT_PIN, HIGH);
  startTune();
}

void stopAlarm() {
  digitalWrite(BACKLIGHT_PIN, LOW);
  stopTune();
  alarm = false;
}

void onMessage(String &topic, String &payload) {
  StaticJsonDocument<100> json;

  if (topic != SUMMON_TOPIC) {
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
  Serial.println(s);

  switch (code) {
    /*case SUMMON_MESSAGE:
      strcpy(message, json["message"] | "");
      sprintf (s, "Received mesage: %s", message);
      Serial.println(s);
      startAlarm();
      break;*/
    case SUMMON_LUNCH:
      strcpy(message, "/lunch.bmp");
      startAlarm();
      break;
    case SUMMON_DINNER:
      strcpy(message, "/dinner.bmp");
      startAlarm();
      break;
    case SUMMON_TEA:
      strcpy(message, "/tea.bmp");
      startAlarm();
      break;
    /*default:
      sprintf(message, "%04ld", code);
      startAlarm();
      break;*/
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected");
    return;
  }

  digitalWrite(BACKLIGHT_PIN, HIGH);
  drawText("Scanning for networks...");

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
    drawText("Connecting to Wario...");
    Serial.println("Connecting to Wario ");
    WiFi.begin("Wario", "mansion1");
  } else {
    drawText("Connecting to HobbyHouse...");
    Serial.println("Connecting to HobbyHouse ");
    WiFi.begin("HobbyHouse", "mansion1");
  }

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  drawText("Connected");
  delay(1000);
  digitalWrite(BACKLIGHT_PIN, LOW);
}

void connectMQTT() {
  if (mqtt.connect(MQTT_CLIENT)) {
    mqtt.subscribe(SUMMON_TOPIC);
    Serial.println("MQTT connected and subscribed");
  } else {
    Serial.println("Error connecting to MQTT");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting");

  pinMode(STOP_PIN, INPUT_PULLUP);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW);
  pinMode(SPEAKER_PIN, OUTPUT);

  delay(500);

  // Initialise display
  beginDisplay();
  Serial.println("Display started");

  // Initialise WiFi
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  delay(100);
  connectWiFi();

  // Initialise SPIFFS
  if (SPIFFS.begin()) {
    Serial.println("SPIFFS started");
  } else {
    Serial.println("Error starting SPIFFS");
  }

    // Initialise MQTT
  mqtt.begin(MQTT_SERVER, wifi);
  mqtt.onMessage(onMessage);
  mqtt.setOptions(60, true, 5000);
  Serial.println("MQTT started");
  connectMQTT();
}

void loop() {
  loopTune();

  mqtt.loop();

  if (!mqtt.connected()) {
    Serial.println("MQTT disconnected");
    mqtt.disconnect();
    delay(3000);
    connectMQTT();
  }

  if (alarm) {
    if ((digitalRead(STOP_PIN) == LOW) /*|| (millis() - alarmStart > ALARM_TIMEOUT)*/) {
      stopAlarm();
    }
    return;
  }
}
