#include <Arduino.h>

// Pin mapping (simple build: no LCD)
const uint8_t PIN_PIR = 2;
const uint8_t PIN_BUTTON = 3;
const uint8_t PIN_FAN_EN = 4;
const uint8_t PIN_FAN_PWM = 5;
const uint8_t PIN_LED_STATUS = 6;

const unsigned long DEBOUNCE_DELAY_MS = 20;
const unsigned long SENSOR_INTERVAL_MS = 100;
const unsigned long LED_BLINK_MS = 500;
const unsigned long AUTO_STOP_MS = 10000;
const unsigned long NOISE_MASK_MS = 200;

const uint8_t FAN_PWM_LOW = 120;

enum FanState : uint8_t {
  STATE_IDLE = 0,
  STATE_FAN_ON = 1,
  STATE_STOPPING = 2
};

uint8_t currentState = STATE_IDLE;
bool fanEnabled = false;
bool pirDetected = false;
bool buttonStableState = false; // false: released, true: pressed
bool rawButtonValue = false;
uint8_t fanPwmDuty = FAN_PWM_LOW;
uint8_t sensorErrorCount = 0;

unsigned long lastSensorMillis = 0;
unsigned long lastButtonMillis = 0;
unsigned long lastLedMillis = 0;
unsigned long lastDetectedMillis = 0;
unsigned long fanMaskUntilMillis = 0;

bool readButtonDebounced();
bool readPirSensor();
bool isHandDetected(bool pir);
void startFanWithinOneSecond(bool trigger);
void toggleFanByButton(bool buttonPressed);
uint16_t calculateFanDuration(bool pir);
void updateStateMachine(bool shortPress);
void updateOutputs(uint8_t state);

void setup() {
  Serial.begin(9600);

  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_FAN_EN, OUTPUT);
  pinMode(PIN_FAN_PWM, OUTPUT);
  pinMode(PIN_LED_STATUS, OUTPUT);

  digitalWrite(PIN_FAN_EN, LOW);
  analogWrite(PIN_FAN_PWM, 0);
  digitalWrite(PIN_LED_STATUS, LOW);

  unsigned long now = millis();
  lastSensorMillis = now;
  lastButtonMillis = now;
  lastLedMillis = now;
  lastDetectedMillis = now;

  Serial.println("desk_fan_controller started");
}

void loop() {
  bool shortPress = readButtonDebounced();
  readPirSensor();

  updateStateMachine(shortPress);
  updateOutputs(currentState);
}

bool readButtonDebounced() {
  unsigned long now = millis();
  bool rawPressed = (digitalRead(PIN_BUTTON) == LOW);

  if (rawPressed != rawButtonValue) {
    rawButtonValue = rawPressed;
    lastButtonMillis = now;
  }

  if ((now - lastButtonMillis) < DEBOUNCE_DELAY_MS) {
    return false;
  }

  bool shortPress = false;
  if (buttonStableState != rawButtonValue) {
    buttonStableState = rawButtonValue;
    if (!buttonStableState) {
      shortPress = true;
    }
  }

  return shortPress;
}

bool readPirSensor() {
  unsigned long now = millis();
  static bool lastRawPir = false;
  static unsigned long lastRawPirChangeMillis = 0;

  bool rawPir = (digitalRead(PIN_PIR) == HIGH);
  if (rawPir != lastRawPir) {
    if ((now - lastRawPirChangeMillis) < 50) {
      if (sensorErrorCount < 255) {
        sensorErrorCount++;
      }
    } else {
      sensorErrorCount = 0;
    }
    lastRawPir = rawPir;
    lastRawPirChangeMillis = now;
  }

  if ((now - lastSensorMillis) >= SENSOR_INTERVAL_MS) {
    pirDetected = rawPir;
    if (pirDetected) {
      lastDetectedMillis = now;
    }
    lastSensorMillis = now;
  }

  if (sensorErrorCount >= 3) {
    pirDetected = false;
  }

  return pirDetected;
}

bool isHandDetected(bool pir) {
  unsigned long now = millis();
  if (!pir) {
    return false;
  }
  if (now < fanMaskUntilMillis) {
    return false;
  }
  if (sensorErrorCount >= 3) {
    return false;
  }
  return true;
}

void startFanWithinOneSecond(bool trigger) {
  if (!trigger) {
    return;
  }

  unsigned long now = millis();
  fanEnabled = true;
  currentState = STATE_FAN_ON;
  fanPwmDuty = FAN_PWM_LOW;
  fanMaskUntilMillis = now + NOISE_MASK_MS;
  lastDetectedMillis = now;
}

void toggleFanByButton(bool buttonPressed) {
  if (!buttonPressed) {
    return;
  }

  if (fanEnabled) {
    fanEnabled = false;
    currentState = STATE_STOPPING;
  } else {
    startFanWithinOneSecond(true);
  }
}

uint16_t calculateFanDuration(bool pir) {
  return pir ? AUTO_STOP_MS : 3000;
}

void updateStateMachine(bool shortPress) {
  unsigned long now = millis();

  switch (currentState) {
    case STATE_IDLE:
      if (isHandDetected(pirDetected)) {
        startFanWithinOneSecond(true);
      }
      if (shortPress) {
        toggleFanByButton(true);
      }
      break;

    case STATE_FAN_ON:
      if (shortPress) {
        toggleFanByButton(true);
        break;
      }

      if ((now - lastDetectedMillis) >= calculateFanDuration(true)) {
        fanEnabled = false;
        currentState = STATE_STOPPING;
      }
      break;

    case STATE_STOPPING:
      fanEnabled = false;
      currentState = STATE_IDLE;
      break;

    default:
      fanEnabled = false;
      currentState = STATE_IDLE;
      break;
  }
}

void updateOutputs(uint8_t state) {
  unsigned long now = millis();

  if (state == STATE_IDLE || state == STATE_STOPPING) {
    digitalWrite(PIN_FAN_EN, LOW);
    analogWrite(PIN_FAN_PWM, 0);
    digitalWrite(PIN_LED_STATUS, LOW);
    return;
  }

  digitalWrite(PIN_FAN_EN, HIGH);
  analogWrite(PIN_FAN_PWM, fanPwmDuty);

  if (state == STATE_FAN_ON) {
    if ((now - lastLedMillis) >= LED_BLINK_MS) {
      digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
      lastLedMillis = now;
    }
  } else {
    digitalWrite(PIN_FAN_EN, LOW);
    analogWrite(PIN_FAN_PWM, 0);
    digitalWrite(PIN_LED_STATUS, LOW);
  }
}

