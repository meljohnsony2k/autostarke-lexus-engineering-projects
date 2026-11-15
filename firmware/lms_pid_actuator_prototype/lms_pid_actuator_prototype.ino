// Lexus Mobile Showroom (LMS) - PID position control prototype
// Autostarke R&D.
//
// Bench prototype for closed loop control of the side wall actuators.
// Two encoder motors (M1, M2) stand in for the linear actuators.
// Each motor has its own encoder and its own PID loop. Both follow the same
// setpoint, and a sync term slows whichever one is ahead so they move together.
//
// Usage: open the Serial Monitor (115200 baud, "Newline" line ending)
// and type a target position in degrees, e.g. 100 or 245 or -90.
// Output lines are "setpoint pos1 pos2" so they plot in the Serial Plotter.
//
// Board: Arduino Uno or Mega 2560.
//
// Design:
//  - PID output drives the motors as a signed -255..255 PWM command
//  - Quadrature encoder (A + B channels) counted in the interrupt
//  - Non-blocking serial line input, empty lines ignored
//  - Deadband checked first so the motor stops cleanly at the target
//  - Fixed 20 ms control loop using millis(), real dt in the maths
//  - Anti-windup on the integral
//  - Encoders on interrupt pins, motors on PWM pins, Serial pins left free
//  - Position is not wrapped at 360
//  - Sync term keeps the two actuators level while moving
//
// Gains: Kp/Ki/Kd below are starting values. Tune them on the real actuators.

// ---------------- Pins ----------------
const uint8_t ENC_A[2]     = {2, 3};    // encoder channel A: external interrupt pins on Uno and Mega
const uint8_t ENC_B[2]     = {4, 7};    // encoder channel B: read inside the interrupt for direction
const uint8_t MOTOR_FWD[2] = {5, 10};   // PWM pins
const uint8_t MOTOR_REV[2] = {6, 11};   // PWM pins

// ---------------- Settings ----------------
const float COUNTS_PER_REV = 360.0;     // encoder counts per output revolution (set for your motor)
const unsigned long SAMPLE_MS = 20;     // control loop period (50 Hz)
const unsigned long PRINT_MS  = 100;    // serial print period
const float DT = SAMPLE_MS / 1000.0;    // seconds

const float KP = 4.0;
const float KI = 1.5;
const float KD = 0.25;

const int   PWM_MAX = 255;
const int   PWM_MIN = 40;               // smallest PWM that still moves the motor (stiction)
const float DEADBAND_DEG = 2.0;         // close enough to target: stop
const float KSYNC = 8.0;                // PWM taken off the leading motor per degree it is ahead

// ---------------- State ----------------
volatile long encCount[2] = {0, 0};

struct PidState {
  float integral;
  float prevError;
};
PidState pid[2] = {{0, 0}, {0, 0}};

float setpointDeg = 0;                  // start by holding the current position
char  lineBuf[16];
uint8_t lineLen = 0;

unsigned long lastSample = 0;
unsigned long lastPrint  = 0;

// ---------------- Encoders ----------------
// Quadrature encoder (x1 decoding): on each rising edge of channel A,
// channel B tells us which way the shaft is turning. This stays correct
// even while the motor coasts or reverses.
void encoderIsr0() { encCount[0] += digitalRead(ENC_B[0]) ? 1 : -1; }
void encoderIsr1() { encCount[1] += digitalRead(ENC_B[1]) ? 1 : -1; }

float readPositionDeg(uint8_t i) {
  noInterrupts();
  long c = encCount[i];
  interrupts();
  return c * 360.0 / COUNTS_PER_REV;
}

// ---------------- Motors ----------------
// cmd: -255..255. Positive = forward.
void driveMotor(uint8_t i, int cmd) {
  if (cmd > 0) {
    analogWrite(MOTOR_REV[i], 0);
    analogWrite(MOTOR_FWD[i], cmd);
  } else if (cmd < 0) {
    analogWrite(MOTOR_FWD[i], 0);
    analogWrite(MOTOR_REV[i], -cmd);
  } else {
    analogWrite(MOTOR_FWD[i], 0);
    analogWrite(MOTOR_REV[i], 0);
  }
}

// ---------------- PID ----------------
int pidStep(PidState &p, float setpoint, float measured) {
  float error = setpoint - measured;

  // Close enough: stop and clear the integral so it does not push past the target.
  if (fabs(error) <= DEADBAND_DEG) {
    p.integral = 0;
    p.prevError = error;
    return 0;
  }

  float derivative = (error - p.prevError) / DT;
  p.prevError = error;

  float newIntegral = p.integral + error * DT;
  float out = KP * error + KI * newIntegral + KD * derivative;

  // Anti-windup: only keep the new integral while the output is not saturated.
  if (out > PWM_MAX) {
    out = PWM_MAX;
  } else if (out < -PWM_MAX) {
    out = -PWM_MAX;
  } else {
    p.integral = newIntegral;
  }

  // Lift small commands above the stiction threshold so the motor can finish the move.
  int cmd = (int) out;
  if (cmd > 0 && cmd < PWM_MIN) cmd = PWM_MIN;
  if (cmd < 0 && cmd > -PWM_MIN) cmd = -PWM_MIN;
  return cmd;
}

// ---------------- Serial input ----------------
// Reads a full line without blocking. Empty lines are ignored.
void readSetpoint() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        setpointDeg = atof(lineBuf);
        pid[0].integral = 0;
        pid[1].integral = 0;
        lineLen = 0;
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}

// ---------------- Main ----------------
void setup() {
  for (uint8_t i = 0; i < 2; i++) {
    pinMode(ENC_A[i], INPUT_PULLUP);
    pinMode(ENC_B[i], INPUT_PULLUP);
    pinMode(MOTOR_FWD[i], OUTPUT);
    pinMode(MOTOR_REV[i], OUTPUT);
    driveMotor(i, 0);
  }
  attachInterrupt(digitalPinToInterrupt(ENC_A[0]), encoderIsr0, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_A[1]), encoderIsr1, RISING);
  Serial.begin(115200);
  Serial.println(F("setpoint pos1 pos2"));
}

void loop() {
  readSetpoint();

  unsigned long now = millis();
  if (now - lastSample >= SAMPLE_MS) {
    lastSample += SAMPLE_MS;
    float pos0 = readPositionDeg(0);
    float pos1 = readPositionDeg(1);
    int cmd0 = pidStep(pid[0], setpointDeg, pos0);
    int cmd1 = pidStep(pid[1], setpointDeg, pos1);

    // Keep both actuators level while they move together:
    // slow down whichever one is ahead so the wall does not rack.
    if ((cmd0 > 0 && cmd1 > 0) || (cmd0 < 0 && cmd1 < 0)) {
      float gap = pos0 - pos1;           // + means motor 0 is further along (positive direction)
      int adjust = (int) (KSYNC * gap);
      cmd0 = constrain(cmd0 - adjust, -PWM_MAX, PWM_MAX);
      cmd1 = constrain(cmd1 + adjust, -PWM_MAX, PWM_MAX);
      // Never let the sync term reverse a motor: at most it pauses it.
      if (setpointDeg > pos0 && cmd0 < 0) cmd0 = 0;
      if (setpointDeg < pos0 && cmd0 > 0) cmd0 = 0;
      if (setpointDeg > pos1 && cmd1 < 0) cmd1 = 0;
      if (setpointDeg < pos1 && cmd1 > 0) cmd1 = 0;
    }

    driveMotor(0, cmd0);
    driveMotor(1, cmd1);
  }

  if (now - lastPrint >= PRINT_MS) {
    lastPrint = now;
    Serial.print(setpointDeg);
    Serial.print(' ');
    Serial.print(readPositionDeg(0));
    Serial.print(' ');
    Serial.println(readPositionDeg(1));
  }
}
