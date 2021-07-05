#include <Arduino.h>
#include <Wire.h>

byte segments[7];
byte segmentCount = 0;

byte alphabet[] = {
  119, 124, 57, 94, 121, 113, 61, 116, 4, 14, 117, 56, 85,
  84, 92, 115, 103, 80, 109, 120, 62, 28, 106, 118, 110, 91
};

byte digits[] = {
  63, 6, 91, 79, 102, 109, 125, 7, 127, 111
};

byte currentDigit = 0;
byte current = '1';
unsigned long previous = 0L;

void show(byte ascii) {
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

void digit(byte digit) {
  byte mask = 0x01 << digit;
  Wire.beginTransmission(0x20);
  Wire.write(0x13);
  Wire.write(~mask);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);

  pinMode(10, OUTPUT);
  digitalWrite(10, LOW);

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

void loop() {
  unsigned long now = millis();
  if (now - previous > 60) {
    if (++current > '4') {
      currentDigit = 3;
      current = '1';
    }
    show(0);
    digit(currentDigit--);
    show(current);

    previous = now;
  }
}
