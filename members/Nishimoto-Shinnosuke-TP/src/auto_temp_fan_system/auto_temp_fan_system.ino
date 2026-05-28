#include <Arduino.h>
#include <DHT.h>
#include <IRremote.hpp>

// Pin definitions
const uint8_t PIN_DHT_DATA = 2;
const uint8_t PIN_LED_RED = 3;
const uint8_t PIN_LED_GREEN = 4;
const uint8_t PIN_LED_YELLOW = 5;
const uint8_t PIN_FAN_PWM = 9;
const uint8_t PIN_IR_RECV = 11;
const uint8_t PIN_ENV_SENSOR = A0;

// Mode definitions
const uint8_t MODE_AUTO = 0;
const uint8_t MODE_MANUAL = 1;
const uint8_t MODE_POWER_OFF = 2;
const uint8_t MODE_ERROR = 3;

// Fan level definitions
const uint8_t FAN_STOP = 0;
const uint8_t FAN_WEAK = 1;
const uint8_t FAN_MEDIUM = 2;
const uint8_t FAN_STRONG = 3;

// Temperature threshold (Celsius)
const float TEMP_WEAK_MAX = 25.0f;
const float TEMP_MEDIUM_MAX = 30.0f;
const float TEMP_VALID_MIN = 0.0f;
const float TEMP_VALID_MAX = 50.0f;

// Timing constants
const unsigned long SENSOR_INTERVAL_MS = 1000;
const unsigned long LED_BLINK_MS = 500;
const unsigned long IR_DEBOUNCE_MS = 50;

// Sensor recovery constants
const uint8_t SENSOR_ERR_LIMIT = 2;
const uint8_t SENSOR_RECOVER_OK = 3;

// Optional env sensor power control
const bool USE_ENV_SENSOR_POWER_CONTROL = false;
const int ENV_OFF_THRESHOLD = 650;
const int ENV_ON_THRESHOLD = 550;

// Replace these placeholder values with measured values from your remote.
const uint32_t IR_CODE_NONE = 0x00000000;
const uint32_t IR_CODE_POWER = 0xF3EAEB55;
const uint32_t IR_CODE_MODE = 0xFCABFFBF;      // Button 0
const uint32_t IR_CODE_FAN_WEAK = 0xEC7BF6BD;  // Button 1
const uint32_t IR_CODE_FAN_MEDIUM = 0x70B9ED69;// Button 2
const uint32_t IR_CODE_FAN_STRONG = 0x2F58D8A1;// Button 3
const uint8_t IR_MAX_BIT_DIFF = 10;

const uint8_t pwmTable[4] = {0, 60, 170, 255};

// Global states
uint8_t currentMode = MODE_AUTO;
uint8_t fanLevel = FAN_STOP;
uint8_t manualFanLevel = FAN_MEDIUM;
bool isPowerOn = true;

float currentTempC = 0.0f;
int envSensorValue = 0;

unsigned long lastSensorMillis = 0;
unsigned long lastLedMillis = 0;
unsigned long lastIrMillis = 0;
uint32_t lastIrCode = IR_CODE_NONE;

uint8_t sensorErrorCount = 0;
uint8_t sensorRecoverCount = 0;
bool errorBlinkOn = false;

DHT dht(PIN_DHT_DATA, DHT11);

bool readTemperature(unsigned long now);
uint32_t readIrCommand(unsigned long now);
void updateMode(uint32_t irCode, bool sensorOk);
uint8_t decideFanLevel(float tempC, uint8_t mode, uint32_t irCode);
void applyOutputs(uint8_t level, uint8_t mode, bool power, unsigned long now);
uint8_t controlByTemperature(float tempC);
void updateLedByState(uint8_t level, uint8_t mode, unsigned long now);
void handleManualCommand(uint32_t irCode);
void startupLedSequence();
void printDebug(uint32_t irCode);
void controlPowerByEnvSensor();
bool irMatches(uint32_t received, uint32_t target);

void setup() {
  Serial.begin(9600);

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_FAN_PWM, OUTPUT);
  pinMode(PIN_ENV_SENSOR, INPUT);

  analogWrite(PIN_FAN_PWM, 0);

  dht.begin();
  IrReceiver.begin(PIN_IR_RECV, DISABLE_LED_FEEDBACK);

  currentMode = MODE_AUTO;
  isPowerOn = true;
  fanLevel = FAN_STOP;
  manualFanLevel = FAN_MEDIUM;
  sensorErrorCount = 0;
  sensorRecoverCount = 0;
  lastSensorMillis = 0;
  lastLedMillis = 0;
  lastIrMillis = 0;
  lastIrCode = IR_CODE_NONE;
  errorBlinkOn = false;

  startupLedSequence();
  applyOutputs(fanLevel, currentMode, isPowerOn, millis());

  Serial.println(F("auto_temp_fan_system started"));
}

void loop() {
  const unsigned long now = millis();

  const uint32_t irCode = readIrCommand(now);
  const bool sensorOk = readTemperature(now);

  if (USE_ENV_SENSOR_POWER_CONTROL) {
    controlPowerByEnvSensor();
  }

  updateMode(irCode, sensorOk);

  if (currentMode == MODE_POWER_OFF) {
    fanLevel = FAN_STOP;
    applyOutputs(fanLevel, currentMode, isPowerOn, now);
    printDebug(irCode);
    return;
  }

  if (currentMode == MODE_ERROR) {
    fanLevel = isPowerOn ? FAN_MEDIUM : FAN_STOP;
    applyOutputs(fanLevel, currentMode, isPowerOn, now);
    printDebug(irCode);
    return;
  }

  fanLevel = decideFanLevel(currentTempC, currentMode, irCode);
  applyOutputs(fanLevel, currentMode, isPowerOn, now);
  printDebug(irCode);
}

bool readTemperature(unsigned long now) {
  if ((now - lastSensorMillis) < SENSOR_INTERVAL_MS) {
    return true;
  }

  lastSensorMillis = now;

  const float readTemp = dht.readTemperature();
  if (isnan(readTemp) || readTemp < TEMP_VALID_MIN || readTemp > TEMP_VALID_MAX) {
    if (sensorErrorCount < 255) {
      sensorErrorCount++;
    }
    sensorRecoverCount = 0;
    return false;
  }

  currentTempC = readTemp;
  sensorErrorCount = 0;

  if (currentMode == MODE_ERROR && sensorRecoverCount < 255) {
    sensorRecoverCount++;
  } else if (currentMode != MODE_ERROR) {
    sensorRecoverCount = 0;
  }

  return true;
}

uint32_t readIrCommand(unsigned long now) {
  if (!IrReceiver.decode()) {
    return IR_CODE_NONE;
  }

  const IRData &data = IrReceiver.decodedIRData;
  const uint32_t code = static_cast<uint32_t>(data.decodedRawData);
  const bool isRepeat = (data.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
  const bool isOverflow = (data.flags & IRDATA_FLAGS_WAS_OVERFLOW) != 0;
  IrReceiver.resume();

  if (isRepeat || isOverflow) {
    return IR_CODE_NONE;
  }

  if (code == 0xFFFFFFFFUL || (code == 0 && data.command == 0 && data.numberOfBits <= 1)) {
    return IR_CODE_NONE;
  }

  if ((now - lastIrMillis) < IR_DEBOUNCE_MS) {
    return IR_CODE_NONE;
  }

  lastIrMillis = now;
  lastIrCode = code;
  return code;
}

void updateMode(uint32_t irCode, bool sensorOk) {
  if (irMatches(irCode, IR_CODE_POWER)) {
    isPowerOn = !isPowerOn;
    currentMode = isPowerOn ? MODE_AUTO : MODE_POWER_OFF;
    sensorRecoverCount = 0;
  } else if (irMatches(irCode, IR_CODE_MODE) && isPowerOn) {
    if (currentMode == MODE_AUTO) {
      currentMode = MODE_MANUAL;
    } else if (currentMode == MODE_MANUAL) {
      currentMode = MODE_AUTO;
    }
  }

  if (!sensorOk && sensorErrorCount >= SENSOR_ERR_LIMIT) {
    currentMode = MODE_ERROR;
  }

  if (currentMode == MODE_ERROR && sensorOk && sensorRecoverCount >= SENSOR_RECOVER_OK) {
    currentMode = isPowerOn ? MODE_AUTO : MODE_POWER_OFF;
    sensorRecoverCount = 0;
  }

  if (!isPowerOn) {
    fanLevel = FAN_STOP;
  }
}

uint8_t decideFanLevel(float tempC, uint8_t mode, uint32_t irCode) {
  if (!isPowerOn || mode == MODE_POWER_OFF) {
    return FAN_STOP;
  }

  if (mode == MODE_ERROR) {
    return isPowerOn ? FAN_MEDIUM : FAN_STOP;
  }

  if (mode == MODE_MANUAL) {
    handleManualCommand(irCode);
    return manualFanLevel;
  }

  if (mode == MODE_AUTO) {
    return controlByTemperature(tempC);
  }

  return FAN_STOP;
}

void applyOutputs(uint8_t level, uint8_t mode, bool power, unsigned long now) {
  const uint8_t safeLevel = (level <= FAN_STRONG) ? level : FAN_STOP;

  if (!power) {
    analogWrite(PIN_FAN_PWM, 0);
  } else {
    analogWrite(PIN_FAN_PWM, pwmTable[safeLevel]);
  }

  updateLedByState(safeLevel, mode, now);
}

uint8_t controlByTemperature(float tempC) {
  if (tempC < TEMP_VALID_MIN || tempC > TEMP_VALID_MAX) {
    return FAN_MEDIUM;
  }

  if (tempC < TEMP_WEAK_MAX) {
    return FAN_WEAK;
  }

  if (tempC < TEMP_MEDIUM_MAX) {
    return FAN_MEDIUM;
  }

  return FAN_STRONG;
}

void updateLedByState(uint8_t level, uint8_t mode, unsigned long now) {
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);

  if (mode == MODE_ERROR) {
    if ((now - lastLedMillis) >= LED_BLINK_MS) {
      lastLedMillis = now;
      errorBlinkOn = !errorBlinkOn;
    }
    digitalWrite(PIN_LED_RED, errorBlinkOn ? HIGH : LOW);
    return;
  }

  errorBlinkOn = false;

  if (mode == MODE_POWER_OFF) {
    digitalWrite(PIN_LED_RED, HIGH);
    return;
  }

  if (level == FAN_STOP) {
    digitalWrite(PIN_LED_RED, HIGH);
  } else if (level == FAN_WEAK || level == FAN_MEDIUM) {
    digitalWrite(PIN_LED_GREEN, HIGH);
  } else if (level == FAN_STRONG) {
    digitalWrite(PIN_LED_YELLOW, HIGH);
  } else {
    digitalWrite(PIN_LED_RED, HIGH);
  }
}

void handleManualCommand(uint32_t irCode) {
  if (irMatches(irCode, IR_CODE_FAN_WEAK)) {
    manualFanLevel = FAN_WEAK;
  } else if (irMatches(irCode, IR_CODE_FAN_MEDIUM)) {
    manualFanLevel = FAN_MEDIUM;
  } else if (irMatches(irCode, IR_CODE_FAN_STRONG)) {
    manualFanLevel = FAN_STRONG;
  }
}

bool irMatches(uint32_t received, uint32_t target) {
  if (received == IR_CODE_NONE || target == IR_CODE_NONE) {
    return false;
  }

  if (received == target) {
    return true;
  }

  const uint32_t diff = received ^ target;
  return __builtin_popcount(diff) <= IR_MAX_BIT_DIFF;
}

void startupLedSequence() {
  digitalWrite(PIN_LED_RED, HIGH);
  delay(100);
  digitalWrite(PIN_LED_RED, LOW);

  digitalWrite(PIN_LED_GREEN, HIGH);
  delay(100);
  digitalWrite(PIN_LED_GREEN, LOW);

  digitalWrite(PIN_LED_YELLOW, HIGH);
  delay(100);
  digitalWrite(PIN_LED_YELLOW, LOW);
}

void printDebug(uint32_t irCode) {
  static unsigned long lastPrint = 0;
  const unsigned long now = millis();

  if ((now - lastPrint) < 1000) {
    return;
  }

  lastPrint = now;
  Serial.print(F("mode="));
  Serial.print(currentMode);
  Serial.print(F(" power="));
  Serial.print(isPowerOn ? 1 : 0);
  Serial.print(F(" temp="));
  Serial.print(currentTempC);
  Serial.print(F(" fan="));
  Serial.print(fanLevel);
  Serial.print(F(" ir=0x"));
  Serial.println(irCode, HEX);
}

void controlPowerByEnvSensor() {
  envSensorValue = analogRead(PIN_ENV_SENSOR);

  if (isPowerOn && envSensorValue >= ENV_OFF_THRESHOLD) {
    isPowerOn = false;
    currentMode = MODE_POWER_OFF;
  } else if (!isPowerOn && envSensorValue <= ENV_ON_THRESHOLD) {
    isPowerOn = true;
    currentMode = MODE_AUTO;
  }
}
