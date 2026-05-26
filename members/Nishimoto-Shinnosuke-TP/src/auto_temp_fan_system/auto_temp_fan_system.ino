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

// Thresholds
const float TEMP_WEAK_MAX = 25.0f;
const float TEMP_MEDIUM_MAX = 30.0f;
const float TEMP_VALID_MIN = 0.0f;
const float TEMP_VALID_MAX = 50.0f;

const uint8_t pwmTable[4] = {0, 85, 170, 255};

// Timers
const unsigned long SENSOR_INTERVAL_MS = 1000UL;
const unsigned long LED_BLINK_MS = 500UL;
const unsigned long IR_DEBOUNCE_MS = 50UL;
const unsigned long RESPONSE_LIMIT_MS = 300UL;
const unsigned long DHT_STARTUP_MS = 2000UL;

// IR code definitions (replace with measured values on your remote)
const uint32_t IR_CODE_NONE = 0x00000000UL;
const uint32_t IR_CODE_POWER = 0xA1A1A1A1UL;
const uint32_t IR_CODE_MODE = 0xB2B2B2B2UL;        // Button "0": AUTO/MANUAL toggle.
const uint32_t IR_CODE_FAN_WEAK = 0xC3C3C3C3UL;    // Button "1": weak.
const uint32_t IR_CODE_FAN_MEDIUM = 0xD4D4D4D4UL;  // Button "2": medium.
const uint32_t IR_CODE_FAN_STRONG = 0xE5E5E5E5UL;  // Button "3": strong.

// Error handling
const uint8_t SENSOR_ERR_LIMIT = 2;
const uint8_t SENSOR_RECOVER_OK = 3;

// Optional feature switch
const bool USE_ENV_SENSOR = false;
const bool DEBUG_IR_LOG = true;
const bool DEBUG_STATUS_LOG = true;

// Global variables
uint8_t currentMode = MODE_AUTO;
uint8_t fanLevel = FAN_STOP;
bool isPowerOn = true;

unsigned long lastSensorMillis = 0;
unsigned long lastLedMillis = 0;
unsigned long lastIrMillis = 0;
unsigned long lastStatusPrintMillis = 0;

float currentTempC = 0.0f;
int envSensorValue = 0;
int envSensorThreshold = 600;
uint32_t lastIrCode = IR_CODE_NONE;

uint8_t sensorErrorCount = 0;
uint8_t sensorRecoverCount = 0;
uint8_t manualFanLevel = FAN_MEDIUM;

const uint8_t DHT_TYPE = DHT11;
DHT dht(PIN_DHT_DATA, DHT_TYPE);

bool readTemperature(void);
uint32_t readIrCommand(void);
void updateMode(uint32_t irCode, bool sensorOk);
uint8_t decideFanLevel(float tempC, uint8_t mode, uint32_t irCode);
void applyOutputs(uint8_t level, uint8_t mode, bool power);
uint8_t controlByTemperature(float tempC);
void updateLedByState(uint8_t level, uint8_t mode);
void handleManualCommand(uint32_t irCode);
bool controlPowerByEnvSensor(int sensorValue, int threshold);

void setup(void) {
  Serial.begin(9600);

  pinMode(PIN_IR_RECV, INPUT);
  pinMode(PIN_ENV_SENSOR, INPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_FAN_PWM, OUTPUT);

  analogWrite(PIN_FAN_PWM, 0);

  dht.begin();
  delay(DHT_STARTUP_MS);
  IrReceiver.begin(PIN_IR_RECV, DISABLE_LED_FEEDBACK);

  currentMode = MODE_AUTO;
  isPowerOn = true;
  fanLevel = FAN_STOP;
  sensorErrorCount = 0;
  sensorRecoverCount = 0;
  lastSensorMillis = 0;
  lastLedMillis = 0;
  lastIrMillis = 0;
  lastStatusPrintMillis = 0;

  digitalWrite(PIN_LED_RED, HIGH);
  delay(120);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, HIGH);
  delay(120);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_YELLOW, HIGH);
  delay(120);
  digitalWrite(PIN_LED_YELLOW, LOW);

  applyOutputs(fanLevel, currentMode, isPowerOn);
}

void loop(void) {
  uint32_t irCode = readIrCommand();
  bool sensorOk = readTemperature();

  if (USE_ENV_SENSOR) {
    envSensorValue = analogRead(PIN_ENV_SENSOR);
    bool keepPowerOn = controlPowerByEnvSensor(envSensorValue, envSensorThreshold);
    if (!keepPowerOn && isPowerOn) {
      isPowerOn = false;
      currentMode = MODE_POWER_OFF;
    } else if (keepPowerOn && !isPowerOn) {
      isPowerOn = true;
      currentMode = MODE_AUTO;
    }
  }

  updateMode(irCode, sensorOk);

  if (currentMode == MODE_POWER_OFF) {
    fanLevel = FAN_STOP;
    applyOutputs(fanLevel, currentMode, isPowerOn);
    return;
  }

  if (currentMode == MODE_ERROR) {
    fanLevel = isPowerOn ? FAN_WEAK : FAN_STOP;
    applyOutputs(fanLevel, currentMode, isPowerOn);
    return;
  }

  fanLevel = decideFanLevel(currentTempC, currentMode, irCode);
  applyOutputs(fanLevel, currentMode, isPowerOn);

  if (DEBUG_STATUS_LOG) {
    unsigned long now = millis();
    if ((now - lastStatusPrintMillis) >= SENSOR_INTERVAL_MS) {
      lastStatusPrintMillis = now;
      Serial.print("mode=");
      Serial.print(currentMode);
      Serial.print(", temp=");
      Serial.print(currentTempC);
      Serial.print(", fan=");
      Serial.println(fanLevel);
    }
  }
}

bool readTemperature(void) {
  unsigned long now = millis();
  if ((now - lastSensorMillis) < SENSOR_INTERVAL_MS) {
    return true;
  }

  lastSensorMillis = now;

  float temp = dht.readTemperature();
  if (isnan(temp) || temp < TEMP_VALID_MIN || temp > TEMP_VALID_MAX) {
    if (sensorErrorCount < 255) {
      sensorErrorCount++;
    }
    sensorRecoverCount = 0;
    return false;
  }

  currentTempC = temp;
  sensorErrorCount = 0;
  if (sensorRecoverCount < 255) {
    sensorRecoverCount++;
  }
  return true;
}

uint32_t readIrCommand(void) {
  if (!IrReceiver.decode()) {
    return IR_CODE_NONE;
  }

  uint32_t code = (uint32_t)IrReceiver.decodedIRData.decodedRawData;
  unsigned long now = millis();
  IrReceiver.resume();

  if ((now - lastIrMillis) < IR_DEBOUNCE_MS) {
    return IR_CODE_NONE;
  }

  lastIrMillis = now;
  lastIrCode = code;

  if (DEBUG_IR_LOG) {
    Serial.print("ir_raw=0x");
    Serial.println(code, HEX);
  }

  return code;
}

void updateMode(uint32_t irCode, bool sensorOk) {
  if (irCode == IR_CODE_POWER) {
    isPowerOn = !isPowerOn;
    currentMode = isPowerOn ? MODE_AUTO : MODE_POWER_OFF;
  } else if (irCode == IR_CODE_MODE && isPowerOn) {
    if (currentMode == MODE_AUTO) {
      currentMode = MODE_MANUAL;
    } else if (currentMode == MODE_MANUAL) {
      currentMode = MODE_AUTO;
    }
  }

  if (!sensorOk && sensorErrorCount >= SENSOR_ERR_LIMIT) {
    currentMode = MODE_ERROR;
  }

  if (sensorOk && currentMode == MODE_ERROR && sensorRecoverCount >= SENSOR_RECOVER_OK) {
    currentMode = isPowerOn ? MODE_AUTO : MODE_POWER_OFF;
  }

  if (!isPowerOn) {
    fanLevel = FAN_STOP;
    currentMode = MODE_POWER_OFF;
  }
}

uint8_t decideFanLevel(float tempC, uint8_t mode, uint32_t irCode) {
  if (!isPowerOn || mode == MODE_POWER_OFF) {
    return FAN_STOP;
  }

  if (mode == MODE_ERROR) {
    return isPowerOn ? FAN_WEAK : FAN_STOP;
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

void applyOutputs(uint8_t level, uint8_t mode, bool power) {
  uint8_t safeLevel = level;
  if (safeLevel > FAN_STRONG) {
    safeLevel = FAN_STOP;
  }

  if (!power) {
    analogWrite(PIN_FAN_PWM, pwmTable[FAN_STOP]);
  } else {
    analogWrite(PIN_FAN_PWM, pwmTable[safeLevel]);
  }

  updateLedByState(safeLevel, mode);
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

void updateLedByState(uint8_t level, uint8_t mode) {
  static bool errorLedOn = false;
  unsigned long now = millis();

  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);

  if (mode == MODE_POWER_OFF) {
    digitalWrite(PIN_LED_RED, HIGH);
    return;
  }

  if (mode == MODE_ERROR) {
    if ((now - lastLedMillis) >= LED_BLINK_MS) {
      lastLedMillis = now;
      errorLedOn = !errorLedOn;
    }
    digitalWrite(PIN_LED_RED, errorLedOn ? HIGH : LOW);
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
  if (irCode == IR_CODE_FAN_WEAK) {
    manualFanLevel = FAN_WEAK;
  } else if (irCode == IR_CODE_FAN_MEDIUM) {
    manualFanLevel = FAN_MEDIUM;
  } else if (irCode == IR_CODE_FAN_STRONG) {
    manualFanLevel = FAN_STRONG;
  }
}

bool controlPowerByEnvSensor(int sensorValue, int threshold) {
  if (sensorValue > threshold) {
    return false;
  }
  return true;
}