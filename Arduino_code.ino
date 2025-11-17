#include <Wire.h>
#include <MPU6050.h>
#include <Servo.h>

// ------------------- MPU6050 SETUP -------------------
MPU6050 mpu;

const float FALL_THRESHOLD = 2.5;

String getTimeString() {
  unsigned long ms = millis();
  unsigned long sec = ms / 1000;
  int hours = (sec / 3600) % 24;
  int minutes = (sec / 60) % 60;
  int seconds = sec % 60;

  char buffer[12];
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buffer);
}

// ------------------- EMG FILTER CONSTANTS -------------------
#define BAUD_RATE 115200
#define INPUT_PIN A0

#define BUFFER_SIZE 64
int emgBuffer[BUFFER_SIZE];
int bufferIndex = 0;

float filter1 = 0.0;
float filter2 = 0.0;

const float ALPHA = 0.1;
const float BETA = 0.01;

int envelope = 0;

// ------------------- LED + SERVO PINS -------------------
Servo clawServo;

#define SERVO_PIN 9

#define LED1 2
#define LED2 3
#define LED3 4
#define LED4 5
#define LED5 6
#define LED6 7

// ------------------- EMG THRESHOLDS -------------------
#define EMG_THRESHOLD 200
#define MAX_EMG 600

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(BAUD_RATE);

  // MPU setup
  Wire.begin();
  mpu.initialize();

  // EMG pins
  pinMode(INPUT_PIN, INPUT);

  // LED pins
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(LED5, OUTPUT);
  pinMode(LED6, OUTPUT);

  // Servo
  clawServo.attach(SERVO_PIN);
  clawServo.write(0);  
}

// ------------------- MOVING AVERAGE -------------------
int movingAverage(int val) {
  emgBuffer[bufferIndex] = val;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

  long sum = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    sum += emgBuffer[i];
  }
  return sum / BUFFER_SIZE;
}

// ------------------- EMG ENVELOPE FILTER -------------------
int processEMG(int rawValue) {
  float absValue = abs(rawValue - 512);

  filter1 = (ALPHA * absValue) + ((1 - ALPHA) * filter1);
  filter2 = (BETA * filter1) + ((1 - BETA) * filter2);

  return (int)filter2;
}

// ------------------- UPDATE LED BAR -------------------
void updateLEDs(int value) {
  digitalWrite(LED1, value > (MAX_EMG * 0.10));
  digitalWrite(LED2, value > (MAX_EMG * 0.25));
  digitalWrite(LED3, value > (MAX_EMG * 0.40));
  digitalWrite(LED4, value > (MAX_EMG * 0.55));
  digitalWrite(LED5, value > (MAX_EMG * 0.70));
  digitalWrite(LED6, value > (MAX_EMG * 0.85));
}

// ------------------- MAIN LOOP -------------------
void loop() {
  // -------- MPU FALL DETECTION --------
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float ax_g = ax / 16384.0;
  float ay_g = ay / 16384.0;
  float az_g = az / 16384.0;

  float magnitude = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
  bool fallDetected = magnitude > FALL_THRESHOLD;

  String timeNow = getTimeString();

  // -------- EMG PROCESSING --------
  int rawEMG = analogRead(INPUT_PIN);
  int avgEMG = movingAverage(rawEMG);
  envelope = processEMG(avgEMG);

  // -------- LED BAR OUTPUT --------
  updateLEDs(envelope);

  // -------- SERVO CONTROL --------
  if (envelope > EMG_THRESHOLD) {
    clawServo.write(90);  
  } else {
    clawServo.write(0);   
  }

  // -------- JSON OUTPUT --------
  Serial.print("{");
  Serial.print("\"time\":\"" + timeNow + "\",");
  Serial.print("\"ax\":" + String(ax_g, 3) + ",");
  Serial.print("\"ay\":" + String(ay_g, 3) + ",");
  Serial.print("\"az\":" + String(az_g, 3) + ",");
  Serial.print("\"A\":" + String(magnitude, 3) + ",");
  Serial.print("\"fall\":" + String(fallDetected ? "true" : "false") + ",");
  Serial.print("\"rawEMG\":" + String(rawEMG) + ",");
  Serial.print("\"env\":" + String(envelope));
  Serial.println("}");
}
