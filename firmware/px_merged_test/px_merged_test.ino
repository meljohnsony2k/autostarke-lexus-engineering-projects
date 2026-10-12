// Project X (PX) merged test sketch
// Luxury lounge conversion kit prototype, Autostarke R&D, 2022 (original).
// Fixed and restructured 2026.
//
// Combines the individually tested PX functions into one program:
//  - reading, flood and ambient lights through relays
//  - two DC motors through an L298 H-bridge (privacy screen / blinds test cycle)
//  - NeoPixel ambient strip: rainbow fade by default, or a custom colour
//
// Bluetooth / Serial commands (9600 baud):
//   #R,G,B#      set a custom ambient colour, e.g. #255,120,0#
//   #RAINBOW#    go back to the rainbow fade
//
// Board: Arduino Mega 2560 (also runs on Uno).
//
// What changed from the 2022 version (see README):
//  1. No blocking delay(): lights, motors, LEDs and serial all run together
//     using millis() timers, so commands are handled straight away.
//     (The old loop was stuck for ~20 s per cycle and could miss Bluetooth input.)
//  2. The colour parser is now actually called, and it returns true/false
//     instead of "return R, G, B" (which only returned B).
//  3. Parsed colours are range checked (0-255) and stay on the strip.

#include <Adafruit_NeoPixel.h>

#define LED_PIN 2      // NeoPixel data pin
#define NUMPIXELS 8    // number of LEDs on the strip

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Relay trigger pins
const uint8_t RLIGHT = 7;  // reading light
const uint8_t SLIGHT = 6;  // flood light
const uint8_t ALIGHT = 5;  // ambient light

// Motor A and B (L298, enable pins tied high)
const uint8_t IN1 = 8;
const uint8_t IN2 = 9;
const uint8_t IN3 = 10;
const uint8_t IN4 = 11;

// ---------------- LED state ----------------
bool rainbowMode = true;
uint8_t customR = 255, customG = 255, customB = 255;
long firstPixelHue = 0;
unsigned long lastLedUpdate = 0;
const unsigned long LED_STEP_MS = 10;

// ---------------- Motor test cycle ----------------
// CCW 2 s -> stop 2 s -> CW 2 s -> stop 2 s -> repeat
enum MotorStep { MOTOR_CCW, MOTOR_STOP1, MOTOR_CW, MOTOR_STOP2 };
MotorStep motorStep = MOTOR_CCW;
unsigned long motorStepStart = 0;
const unsigned long MOTOR_STEP_MS = 2000;

// ---------------- Serial frame parser ----------------
char frame[24];
uint8_t frameLen = 0;
bool inFrame = false;

void setMotors(int a1, int a2, int b1, int b2) {
  digitalWrite(IN1, a1);
  digitalWrite(IN2, a2);
  digitalWrite(IN3, b1);
  digitalWrite(IN4, b2);
}

void applyMotorStep() {
  switch (motorStep) {
    case MOTOR_CCW:   setMotors(HIGH, LOW, HIGH, LOW); break;
    case MOTOR_CW:    setMotors(LOW, HIGH, LOW, HIGH); break;
    default:          setMotors(LOW, LOW, LOW, LOW);   break;
  }
}

void updateMotors(unsigned long now) {
  if (now - motorStepStart >= MOTOR_STEP_MS) {
    motorStepStart = now;
    motorStep = (MotorStep) ((motorStep + 1) % 4);
    applyMotorStep();
  }
}

void showColour(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = pixels.Color(r, g, b);
  for (int i = 0; i < NUMPIXELS; i++) pixels.setPixelColor(i, c);
  pixels.show();
}

void updateLeds(unsigned long now) {
  if (!rainbowMode) return;  // custom colour stays as set
  if (now - lastLedUpdate < LED_STEP_MS) return;
  lastLedUpdate = now;
  for (int i = 0; i < NUMPIXELS; i++) {
    long pixelHue = firstPixelHue + (i * 65536L / NUMPIXELS);
    pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV((uint16_t) pixelHue)));
  }
  pixels.show();
  firstPixelHue += 256;
  if (firstPixelHue >= 5L * 65536) firstPixelHue = 0;
}

// Parses "R,G,B" into three bytes. Returns false if it is not a valid colour.
bool parseColour(const char *s, uint8_t &r, uint8_t &g, uint8_t &b) {
  int vals[3];
  int count = 0;
  const char *p = s;
  while (count < 3) {
    char *end;
    long v = strtol(p, &end, 10);
    if (end == p || v < 0 || v > 255) return false;
    vals[count++] = (int) v;
    p = end;
    if (count < 3) {
      if (*p != ',') return false;
      p++;
    }
  }
  if (*p != '\0') return false;
  r = vals[0];
  g = vals[1];
  b = vals[2];
  return true;
}

void handleFrame(const char *f) {
  if (strcmp(f, "RAINBOW") == 0) {
    rainbowMode = true;
    Serial.println(F("Mode: rainbow"));
    return;
  }
  uint8_t r, g, b;
  if (parseColour(f, r, g, b)) {
    customR = r; customG = g; customB = b;
    rainbowMode = false;
    showColour(r, g, b);
    Serial.print(F("Colour set R:")); Serial.print(r);
    Serial.print(F(" G:")); Serial.print(g);
    Serial.print(F(" B:")); Serial.println(b);
  } else {
    Serial.print(F("NOT RGB Code: "));
    Serial.println(f);
  }
}

// Reads "#...#" frames from Serial without blocking.
void readSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '#') {
      if (inFrame && frameLen > 0) {
        frame[frameLen] = '\0';
        handleFrame(frame);
        inFrame = false;
      } else {
        inFrame = true;  // start of a new frame
      }
      frameLen = 0;
    } else if (inFrame) {
      if (c == '\n' || c == '\r') continue;
      if (frameLen < sizeof(frame) - 1) {
        frame[frameLen++] = c;
      } else {
        inFrame = false;  // too long, drop it
        frameLen = 0;
      }
    }
  }
}

void setup() {
  Serial.begin(9600);

  pixels.begin();
  pixels.show();

  pinMode(RLIGHT, OUTPUT);
  pinMode(SLIGHT, OUTPUT);
  pinMode(ALIGHT, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Lights on (relays), motors start the test cycle
  digitalWrite(RLIGHT, HIGH);
  digitalWrite(SLIGHT, HIGH);
  digitalWrite(ALIGHT, HIGH);
  motorStepStart = millis();
  applyMotorStep();

  Serial.println(F("INITIALIZED"));
}

void loop() {
  unsigned long now = millis();
  readSerial();
  updateMotors(now);
  updateLeds(now);
}
