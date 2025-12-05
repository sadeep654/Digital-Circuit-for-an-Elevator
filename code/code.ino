{\rtf1\ansi\ansicpg1252\cocoartf2867
\cocoatextscaling0\cocoaplatform0{\fonttbl\f0\fswiss\fcharset0 Helvetica;}
{\colortbl;\red255\green255\blue255;}
{\*\expandedcolortbl;;}
\paperw11900\paperh16840\margl1440\margr1440\vieww11520\viewh8400\viewkind0
\pard\tx720\tx1440\tx2160\tx2880\tx3600\tx4320\tx5040\tx5760\tx6480\tx7200\tx7920\tx8640\pardirnatural\partightenfactor0

\f0\fs24 \cf0 #include <Arduino.h>\
\
// === PIN ASSIGNMENTS (change to match your wiring) ===\
// Sensors\
const uint8_t SENSOR_A_PIN = 2;   // upstream sensor (e.g., inside beam 1)\
const uint8_t SENSOR_B_PIN = 3;   // downstream sensor (e.g., beam 2)\
\
// Output BCD pins -> CD4511 inputs (A, B, C, D : LSB to MSB)\
const uint8_t BCD0 = 4; // bit0 (LSB)\
const uint8_t BCD1 = 5; // bit1\
const uint8_t BCD2 = 6; // bit2\
const uint8_t BCD3 = 7; // bit3 (MSB)\
\
// LEDs & buzzer\
const uint8_t GREEN_LED_PIN = 8;\
const uint8_t RED_LED_PIN   = 9;\
const uint8_t BUZZER_PIN    = 10;\
\
// Configurable parameters\
const uint8_t MAX_COUNT = 9;          // capacity threshold\
const unsigned long MAX_GAP_MS = 700UL; // max time allowed between two sensor triggers to consider a passage (tune)\
const unsigned long DEBOUNCE_MS = 50UL; // debounce time for sensors\
\
// internal state\
volatile int countPeople = 0;\
\
// For direction detection we use a small state machine\
enum DetectState \{ IDLE, WAIT_FOR_SECOND \};\
DetectState detectState = IDLE;\
\
unsigned long firstTriggerTime = 0;\
int firstSensor = -1; // 0 = A, 1 = B\
\
// Last sensor read times for software debounce\
unsigned long lastReadA = 0;\
unsigned long lastReadB = 0;\
bool lastStateA = false;\
bool lastStateB = false;\
\
void setup() \{\
  // Serial for debug (optional)\
  Serial.begin(115200);\
  Serial.println(F("Elevator Counter starting..."));\
\
  // Sensor pins\
  pinMode(SENSOR_A_PIN, INPUT);\
  pinMode(SENSOR_B_PIN, INPUT);\
  // If your sensors are open-collector or need pull-ups:\
  // pinMode(SENSOR_A_PIN, INPUT_PULLUP); etc. Invert logic if using pullups.\
\
  // BCD outputs\
  pinMode(BCD0, OUTPUT);\
  pinMode(BCD1, OUTPUT);\
  pinMode(BCD2, OUTPUT);\
  pinMode(BCD3, OUTPUT);\
\
  // LEDs and buzzer\
  pinMode(GREEN_LED_PIN, OUTPUT);\
  pinMode(RED_LED_PIN, OUTPUT);\
  pinMode(BUZZER_PIN, OUTPUT);\
\
  // initial states\
  writeBCD(countPeople);\
  digitalWrite(GREEN_LED_PIN, HIGH); // ready\
  digitalWrite(RED_LED_PIN, LOW);\
  digitalWrite(BUZZER_PIN, LOW);\
\}\
\
void loop() \{\
  // Read sensors with small debounce\
  bool a = readSensorDebounced(SENSOR_A_PIN, &lastStateA, &lastReadA);\
  bool b = readSensorDebounced(SENSOR_B_PIN, &lastStateB, &lastReadB);\
\
  unsigned long now = millis();\
\
  switch (detectState) \{\
    case IDLE:\
      if (a && !b) \{\
        // A triggered first\
        detectState = WAIT_FOR_SECOND;\
        firstSensor = 0;\
        firstTriggerTime = now;\
      \} else if (b && !a) \{\
        // B triggered first\
        detectState = WAIT_FOR_SECOND;\
        firstSensor = 1;\
        firstTriggerTime = now;\
      \}\
      break;\
\
    case WAIT_FOR_SECOND:\
      // If second sensor triggers within time window, decide direction\
      if ((now - firstTriggerTime) > MAX_GAP_MS) \{\
        // Timeout - no complete passage, reset\
        detectState = IDLE;\
        firstSensor = -1;\
      \} else \{\
        // check if opposite sensor triggered\
        if (firstSensor == 0 && b) \{\
          // A -> B => entry\
          countPeople = constrain(countPeople + 1, 0, MAX_COUNT);\
          onCountChanged();\
          detectState = IDLE;\
        \} else if (firstSensor == 1 && a) \{\
          // B -> A => exit\
          countPeople = constrain(countPeople - 1, 0, MAX_COUNT);\
          onCountChanged();\
          detectState = IDLE;\
        \}\
        // If same sensor retriggers, ignore (still waiting)\
      \}\
      break;\
  \}\
\
  // small delay to free CPU (tweak if needed)\
  delay(10);\
\}\
\
// Called when count changes: update display and indicators\
void onCountChanged() \{\
  writeBCD(countPeople);\
  Serial.print(F("Count: "));\
  Serial.println(countPeople);\
\
  if (countPeople >= MAX_COUNT) \{\
    // over capacity (or at limit)\
    digitalWrite(GREEN_LED_PIN, LOW);\
    digitalWrite(RED_LED_PIN, HIGH);\
    buzzAlert();\
  \} else \{\
    digitalWrite(GREEN_LED_PIN, HIGH);\
    digitalWrite(RED_LED_PIN, LOW);\
    digitalWrite(BUZZER_PIN, LOW);\
  \}\
\}\
\
// Write an integer 0..9 as BCD to 4 pins\
void writeBCD(int value) \{\
  value = constrain(value, 0, 15);\
  digitalWrite(BCD0, (value >> 0) & 1);\
  digitalWrite(BCD1, (value >> 1) & 1);\
  digitalWrite(BCD2, (value >> 2) & 1);\
  digitalWrite(BCD3, (value >> 3) & 1);\
\}\
\
// Simple short beep for alert\
void buzzAlert() \{\
  // beep 3 short pulses\
  for (int i = 0; i < 3; ++i) \{\
    digitalWrite(BUZZER_PIN, HIGH);\
    delay(120);\
    digitalWrite(BUZZER_PIN, LOW);\
    delay(120);\
  \}\
\}\
\
// Debounced digital read helper for simple modules\
bool readSensorDebounced(uint8_t pin, bool *lastState, unsigned long *lastTime) \{\
  bool raw = digitalRead(pin); // assumes HIGH when triggered; invert logic if needed\
  unsigned long now = millis();\
  if (raw != *lastState) \{\
    // changed - debounce window\
    if (now - *lastTime > DEBOUNCE_MS) \{\
      *lastState = raw;\
      *lastTime = now;\
      return raw;\
    \} else \{\
      // still in debounce, do not change state yet\
      return *lastState;\
    \}\
  \} else \{\
    // unchanged\
    return *lastState;\
  \}\
\}}