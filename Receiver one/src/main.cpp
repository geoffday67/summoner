#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266FtpServer.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <FS.h>
#include <MQTT.h>

#define MDNS_HOST         "receiver1"

#define FTP_USERNAME      "receiver1"
#define FTP_PASSWORD      "mansion1"

#define NTP_SERVER        "time.google.com"
#define NTP_PORT          123
#define NTP_OFFSET        2208988800UL

#define LOG_FILE          "/log.txt"

#define ALARM_TIMEOUT     20000L

#define SUMMON_MESSAGE  0
#define SUMMON_COFFEE   1
#define SUMMON_LUNCH    2
#define SUMMON_TEA      3
#define SUMMON_DINNER   4

#define SOUNDER_PIN     10
#define LEFT_EYE_PIN    14
#define RIGHT_EYE_PIN   12
#define STOP_PIN        13

#define MQTT_SERVER     "geoffs-mac-mini.local"
#define MQTT_CLIENT     "summoner-robot"
#define SUMMON_TOPIC    "summon/robot"
#define ACK_TOPIC       "summon-ack/robot"

WiFiClient wifi;
MQTTClient mqtt;
FtpServer ftp;
WiFiUDP ntpUDP;
bool isTimeSynced = false;

bool alarm = false;
long alarmStart;
bool left;
char message[5];
byte currentCharacter = 0;
byte currentDigit = 0;
unsigned long previousDigit = 0L;
unsigned long previousAlert = 0L;
char messageId[64];

byte alphabet[] = {
  119, 124, 57, 94, 121, 113, 61, 116, 4, 14, 117, 56, 85,
  84, 92, 115, 103, 80, 109, 120, 62, 28, 106, 118, 110, 91
};

byte digits[] = {
  63, 6, 91, 79, 102, 109, 125, 7, 127, 111
};

void writeLog(const char *ptext) {
  char timestamp[40];
  time_t now;
  time(&now);
  strftime(timestamp, 40, "%d/%m/%Y %H:%M:%S ", gmtime(&now));

  File file = SPIFFS.open(LOG_FILE, "a");
  file.print(timestamp);
  file.println(ptext);
  file.close();

  Serial.println(ptext);
}

void startNTP() {
  byte ntpBuffer[48];
  IPAddress address;

  if (!WiFi.hostByName(NTP_SERVER, address)) {
    Serial.println("Error looking up time server address");
    return;
  }
  Serial.print ("Time server address: ");
  Serial.println(address);

  memset(ntpBuffer, 0, 48);
  ntpBuffer[0] = 0b11100011;
  ntpUDP.beginPacket(address, NTP_PORT);
  ntpUDP.write(ntpBuffer, 48);
  ntpUDP.endPacket();
}

void fetchNTP() {
  byte ntpBuffer[48];

  if (ntpUDP.parsePacket() == 0) {
    return;
  }

  ntpUDP.read(ntpBuffer, 48);
  uint32_t ntpTime = (ntpBuffer[40] << 24) | (ntpBuffer[41] << 16) | (ntpBuffer[42] << 8) | ntpBuffer[43];
  uint32_t unixTime = ntpTime - NTP_OFFSET;

  timeval now;
  now.tv_sec = unixTime;
  now.tv_usec = 0;
  timezone zone;
  zone.tz_minuteswest = 0;
  zone.tz_dsttime = 0;
  settimeofday(&now, &zone);

  Serial.println("Time set via NTP");

  isTimeSynced = true;
}

void setCharacter(byte ascii) {
  byte pattern;
  if (ascii >= 'A' && ascii <= 'Z') {
    pattern = alphabet[ascii - 'A'];
  } else if (ascii >= '0' && ascii <= '9') {
    pattern = digits[ascii - '0'];
  } else {
    pattern = 0;
  }

  Wire.beginTransmission(0x20);
  Wire.write(0x12);
  Wire.write(~pattern);
  Wire.endTransmission();
}

void setDigit(byte digit) {
  byte mask = 0b00001000 >> digit;
  Wire.beginTransmission(0x20);
  Wire.write(0x13);
  Wire.write(~mask);
  Wire.endTransmission();
}

void initDisplay() {
  Wire.begin();
  Wire.setClock(100000);

  // Set port A as output
  Wire.beginTransmission(0x20);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();

  // Set port B as output
  Wire.beginTransmission(0x20);
  Wire.write(0x01);
  Wire.write(0x00);
  Wire.endTransmission();
}

void clearDisplay() {
  Wire.beginTransmission(0x20);
  Wire.write(0x13);
  Wire.write(0xFF);
  Wire.endTransmission();
}

void acknowledge() {
  // Send ACK with the received payload ID, if there is an ID.
  if (strlen(messageId) == 0) {
    return;
  }

  char message [64];

  sprintf(message, "{\"id\":\"%s\"}", messageId);
  mqtt.publish (ACK_TOPIC, message);
  delay(100);

  writeLog("Acknowledge sent");
}

void startAlarm() {
  alarm = true;
  alarmStart = millis();
  left = true;
}

void stopAlarm() {
  alarm = false;
  digitalWrite(LEFT_EYE_PIN, LOW);
  digitalWrite(RIGHT_EYE_PIN, LOW);
  digitalWrite(SOUNDER_PIN, LOW);
  clearDisplay();

  acknowledge();
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
  strcpy(messageId, json["id"] | "");

  char s[40];
  sprintf (s, "Received code: %ld, id: %s", code, messageId);
  writeLog(s);

  switch (code) {
    case SUMMON_MESSAGE:
      strcpy(message, json["message"] | "");
      sprintf (s, "Received mesage: %s", message);
      writeLog(s);
      startAlarm();
      break;
    case SUMMON_COFFEE:
      strcpy(message, "CFFE");
      startAlarm();
      break;
    case SUMMON_LUNCH:
      strcpy(message, "LNCH");
      startAlarm();
      break;
    case SUMMON_DINNER:
      strcpy(message, "DNNR");
      startAlarm();
      break;
    case SUMMON_TEA:
      strcpy(message, "TEA ");
      startAlarm();
      break;
    default:
      sprintf(message, "%04ld", code);
      startAlarm();
      break;
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected");
    return;
  }

  digitalWrite(LEFT_EYE_PIN, HIGH);
  digitalWrite(RIGHT_EYE_PIN, HIGH);

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

  digitalWrite(LEFT_EYE_PIN, LOW);
  digitalWrite(RIGHT_EYE_PIN, LOW);
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


  // Initialise display
  initDisplay();
  clearDisplay();
  Serial.println("Display started");

  pinMode(LEFT_EYE_PIN, OUTPUT);
  pinMode(RIGHT_EYE_PIN, OUTPUT);
  pinMode(SOUNDER_PIN, OUTPUT);
  pinMode(STOP_PIN, INPUT_PULLUP);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  delay(100);
  connectWiFi();

  // Initialise UDP for NTP
  if (ntpUDP.begin(NTP_PORT)) {
    Serial.println("UDP for NTP started");
    startNTP();
  } else {
    Serial.println("Error starting UDP for NTP");
  }

  // Initialise mDNS
  if (MDNS.begin(MDNS_HOST)) {
    Serial.print("mDNS responder started: ");
    Serial.println(MDNS_HOST);
  } else {
    Serial.println("Error starting MDNS responder");
  }

  // Initialise FTP server
  ftp.begin(FTP_USERNAME, FTP_PASSWORD);
  Serial.println("FTP started");

  // Initialise MQTT
  mqtt.begin(MQTT_SERVER, wifi);
  mqtt.onMessage(onMessage);
  mqtt.setOptions(60, true, 5000);
  Serial.println("MQTT started");
  connectMQTT();
}

void loop() {
  MDNS.update();
  ftp.handleFTP();
  mqtt.loop();

  if (!isTimeSynced) {
    fetchNTP();
  }

  if (!mqtt.connected()) {
    writeLog("MQTT disconnected");
    mqtt.disconnect();
    delay(3000);
    connectMQTT();
  }

  if (alarm) {
    if ((digitalRead(STOP_PIN) == LOW) || (millis() - alarmStart > ALARM_TIMEOUT)) {
      stopAlarm();
      return;
    }

    unsigned long now = millis();

    if (now - previousDigit > 6) {
      setCharacter(0);
      setDigit(currentDigit++);
      setCharacter(message[currentCharacter]);

      if (++currentCharacter > 3) {
        currentDigit = 0;
        currentCharacter = 0;
      }

      previousDigit = now;
    }

    if (now - previousAlert > 500) {
      if (left) {
        digitalWrite (LEFT_EYE_PIN, LOW);
        digitalWrite (RIGHT_EYE_PIN, HIGH);
        digitalWrite (SOUNDER_PIN, HIGH);
        left = false;
      } else {
        digitalWrite (LEFT_EYE_PIN, HIGH);
        digitalWrite (RIGHT_EYE_PIN, LOW);
        digitalWrite (SOUNDER_PIN, LOW);
        left = true;
      }

      previousAlert = now;
    }

    return;
  }

  delay(100);
}