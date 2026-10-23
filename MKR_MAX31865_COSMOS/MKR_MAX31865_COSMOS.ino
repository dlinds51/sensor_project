// Arduino MKR WiFi 1010 + Adafruit MAX31865 3-wire RTD
// v0.3.2: Avoid Adafruit PWM Servo Driver PCA9685_I2C_ADDRESS macro collision.
// Binary COSMOS protocol adapted from the OpenC3 Arduino Sensor Kit demo.
// https://github.com/OpenC3/openc3-cosmos-arduino-sensor-kit
//
// In binary mode, DO NOT use Serial.print() on this serial port.
// For local Serial Monitor verification, change COSMOS_BINARY_MODE to 0,
// upload, then use Serial Monitor at 9600 baud.

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <math.h>

#define COSMOS_BINARY_MODE 1

// MKR WiFi 1010: SCK D9, MOSI D8, MISO D10, CS D7.
static constexpr uint8_t MAX31865_CS = 7;

// IMPORTANT: PT100/430-ohm assumptions. Check the RTD AND the Rref
// resistor fitted on the breakout before taking valid measurements.
// For a PT1000 with 4300-ohm Rref, use 1000.0f and 4300.0f.
// Preserve the user's ice-bath baseline adjustment from the working sketch.
// Recheck with a resistance standard if quantitative accuracy is required.
static constexpr float RTD_NOMINAL = 100.6f;
static constexpr float RTD_RREF = 430.0f;

static constexpr uint32_t SYNC_PATTERN = 0xABCD1234UL;
static constexpr uint8_t RTD_PACKET_ID = 6;  // IDs 1-5 reserved by demo
static constexpr uint8_t CMD_START = 0x11;
static constexpr uint8_t CMD_SET_LED = 0x01;
static constexpr uint8_t CMD_SET_PERIOD = 0x02;
static constexpr uint8_t CMD_SEND_TEXT = 0x03;
static constexpr uint8_t CMD_SET_SERVO = 0x04;  // 1-byte payload 0..180 maps -90..+90
static constexpr uint8_t SERVO_PACKET_ID = 7;
static constexpr uint8_t TEXT_FIELD_BYTES = 32;  // fixed COSMOS command field

static constexpr uint32_t SERIAL_BAUD = 9600;
Adafruit_MAX31865 max31865(MAX31865_CS); // hardware SPI

// D14/TX on MKR WiFi 1010 is Serial1, a separate 3.3 V hardware UART.
// USB Serial stays dedicated to the COSMOS binary command/telemetry link.
static constexpr uint32_t SCOPE_UART_BAUD = 9600;
uint8_t pendingText[TEXT_FIELD_BYTES] = {};
uint8_t pendingTextCount = 0;

// PCA9685 generates servo PWM over I2C; Arduino D5 is no longer used.
// MKR WiFi 1010: SDA=D11 and SCL=D12 (Wire). 3.3V -> PCA9685 VCC.
// PCA9685 servo V+ needs a SEPARATE regulated supply and COMMON GND.
static constexpr uint8_t SERVO_DRIVER_I2C_ADDR = 0x40; // default, unless address jumpers changed
static constexpr uint8_t PCA9685_SERVO_CHANNEL = 0; // first three-pin servo header
static constexpr float SERVO_PWM_FREQUENCY_HZ = 50.0f;
// Preserve the previous firmware's 1000..2000 microsecond command range.
// REMOVE servo horn for first test and shorten limits if travel binds.
static constexpr uint16_t SERVO_MIN_PULSE_US = 1000;
static constexpr uint16_t SERVO_MAX_PULSE_US = 2000;
Adafruit_PWMServoDriver pwm(SERVO_DRIVER_I2C_ADDR);
bool pca9685Ready = false;

uint16_t pulseForAbsoluteAngle(uint8_t absoluteAngle) {
  // Absolute servo command: 0 = -90, 90 = center, 180 = +90.
  // Integer math is deterministic and preserves the prior Servo.attach range.
  return static_cast<uint16_t>(SERVO_MIN_PULSE_US +
      (static_cast<uint32_t>(absoluteAngle) *
       (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / 180UL);
}
// Set to 1 ONLY after wiring and calibrating a feedback-capable servo.
#define SERVO_FEEDBACK_ENABLED 0
static constexpr uint8_t SERVO_FEEDBACK_PIN = A0;
static constexpr float FEEDBACK_RAW_NEG90 = 150.0f; // PLACEHOLDER: calibrate later
static constexpr float FEEDBACK_RAW_POS90 = 850.0f; // PLACEHOLDER: calibrate later
static constexpr uint16_t SERVO_TLM_INTERVAL_MS = 200; // 5 Hz
int16_t servoCommandDeg = 0;             // 0=center, -90 to +90
uint16_t servoCmdSequence = 0;           // increments for each valid command
unsigned long lastServoTlmMs = 0;


uint8_t ledState = 0;
uint8_t periodSeconds = 1;
unsigned long lastSampleMs = 0;

// Packet payload is LE, packed, and independent of compiler alignment:
// ID(1), TEMP_C(4), RES_OHMS(4), RAW_RTD(2), FAULT(1), LED(1), RATE_S(1)
struct __attribute__((packed)) RTDPayload {
  uint8_t id;
  float tempC;
  float resistanceOhms;
  uint16_t rawRtd;
  uint8_t fault;
  uint8_t led;
  uint8_t rateS;
};
static_assert(sizeof(RTDPayload) == 14, "Unexpected RTD payload size");
static_assert(sizeof(float) == 4, "COSMOS packet expects 32-bit floats");

// A second, separate telemetry packet. Never alter the 14-byte RTD packet.
//  ID(1) | CMD_DEG(2, signed) | FEEDBACK_DEG(2, signed)
//  | FEEDBACK_VALID(1) | DRIVE_ABS_DEG(1) | CMD_SEQ(2) | FEEDBACK_RAW(2)
// FEEDBACK_DEG=-32768, RAW=65535 and VALID=0 means no feedback hardware.
struct __attribute__((packed)) ServoPayload {
  uint8_t id;
  int16_t cmdDeg;
  int16_t feedbackDeg;
  uint8_t feedbackValid;
  uint8_t driveAbsDeg;
  uint16_t cmdSeq;
  uint16_t feedbackRaw;
};
static_assert(sizeof(ServoPayload) == 11, "Unexpected servo payload size");

void sendServoTelemetry() {
  int16_t feedbackDeg = -32768;
  uint16_t feedbackRaw = 65535;
  uint8_t valid = 0;
#if SERVO_FEEDBACK_ENABLED
  // MAXIMUM input voltage 3.3 V! Use divider for 5-V feedback sources.
  feedbackRaw = static_cast<uint16_t>(analogRead(SERVO_FEEDBACK_PIN));
  const float delta = FEEDBACK_RAW_POS90 - FEEDBACK_RAW_NEG90;
  if (fabsf(delta) > 50.0f) { // rejects uncalibrated zero-span settings
    float angle = (static_cast<float>(feedbackRaw) - FEEDBACK_RAW_NEG90)
        * 180.0f / delta - 90.0f;
    if (angle < -90.0f) angle = -90.0f;
    if (angle > 90.0f) angle = 90.0f;
    feedbackDeg = static_cast<int16_t>(lroundf(angle));
    valid = 1;
  }
#endif
  ServoPayload data = {
    SERVO_PACKET_ID, servoCommandDeg, feedbackDeg, valid,
    static_cast<uint8_t>(servoCommandDeg + 90), servoCmdSequence, feedbackRaw
  };
#if COSMOS_BINARY_MODE
  Serial.write(reinterpret_cast<const uint8_t*>(&SYNC_PATTERN), 4);
  Serial.write(static_cast<uint8_t>(sizeof(ServoPayload)));
  Serial.write(reinterpret_cast<const uint8_t*>(&data), sizeof(data));
#endif
}

void sendTelemetry() {
  // Read RTD only once so temperature, resistance and RAW_RTD refer
  // to the same sensor sample.
  uint16_t rtdRaw = max31865.readRTD();
  uint8_t fault = max31865.readFault();
  if (fault != 0) max31865.clearFault();

  float resistance = (static_cast<float>(rtdRaw) / 32768.0f) * RTD_RREF;
  float tempC = (fault == 0)
      ? max31865.calculateTemperature(rtdRaw, RTD_NOMINAL, RTD_RREF)
      : NAN;

  RTDPayload data = {
    RTD_PACKET_ID, tempC, resistance, rtdRaw, fault, ledState, periodSeconds
  };

#if COSMOS_BINARY_MODE
  // Same framing as the tutorial:
  // [sync: 4 bytes LE][payload length: 1 byte][payload: length bytes]
  Serial.write(reinterpret_cast<const uint8_t*>(&SYNC_PATTERN), 4);
  Serial.write(static_cast<uint8_t>(sizeof(RTDPayload)));
  Serial.write(reinterpret_cast<const uint8_t*>(&data), sizeof(data));
#else
  Serial.print(data.tempC, 3); Serial.print(',');
  Serial.print(data.resistanceOhms, 4); Serial.print(',');
  Serial.print(data.rawRtd); Serial.print(',');
  Serial.print(data.fault); Serial.print(',');
  Serial.print(data.led); Serial.print(',');
  Serial.println(data.rateS);
#endif
}

void executeCommand(uint8_t id, uint8_t value) {
  switch (id) {
    case CMD_SET_LED:
      if (value <= 1) {
        ledState = value;
        digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
      }
      break;
    case CMD_SET_PERIOD:
      // Set telemetry sample interval in whole seconds (1 .. 60).
      if (value >= 1 && value <= 60) {
        periodSeconds = value;
      }
      break;
    case CMD_SET_SERVO:
      if (value <= 180) {
        // Do not acknowledge a new position if the PCA9685 was not found.
        // RTD and UART continue operating even if the servo board is missing.
        if (pca9685Ready) {
          pwm.writeMicroseconds(PCA9685_SERVO_CHANNEL,
                                pulseForAbsoluteAngle(value));
          servoCommandDeg = static_cast<int16_t>(value) - 90;
          ++servoCmdSequence;
          sendServoTelemetry(); // quick command acknowledgement via status
          lastServoTlmMs = millis();
        }
      }
      break;
    default:
      // Ignore unexpected commands; keep the sensor and UART running.
      break;
  }
}

// Send only the meaningful string bytes to the scope UART (never USB Serial).
// The OpenC3 command transports a fixed-size 32-byte, NUL-padded string.
void transmitScopeText(const uint8_t* field) {
  size_t textLength = 0;
  while (textLength < TEXT_FIELD_BYTES && field[textLength] != 0) {
    ++textLength;
  }
  if (textLength > 0) {
    Serial1.write(field, textLength); // 8N1 UART on TX/D14
  }
}

void processCommands() {
  // Existing commands remain [0x11, ID, VALUE].
  // New SEND_TEXT command is [0x11, 0x03, 32 ASCII/NUL-padded bytes].
  // State is preserved across loop() calls, even if USB delivers partial data.
  static uint8_t parserState = 0;
  static uint8_t commandId = 0;
  while (Serial.available() > 0) {
    uint8_t incoming = static_cast<uint8_t>(Serial.read());
    switch (parserState) {
      case 0: // waiting for command marker
        if (incoming == CMD_START) parserState = 1;
        break;
      case 1: // reading command ID
        commandId = incoming;
        if (commandId == CMD_SEND_TEXT) {
          pendingTextCount = 0;
          parserState = 3;
        } else if (commandId == CMD_SET_LED || commandId == CMD_SET_PERIOD || commandId == CMD_SET_SERVO) {
          parserState = 2;
        } else {
          parserState = 0;
        }
        break;
      case 2: // existing single-byte argument
        executeCommand(commandId, incoming);
        parserState = 0;
        break;
      case 3: // fixed-length ASCII field (NUL padded)
        pendingText[pendingTextCount++] = incoming;
        if (pendingTextCount == TEXT_FIELD_BYTES) {
          transmitScopeText(pendingText);
          pendingTextCount = 0;
          parserState = 0;
        }
        break;
      default:
        parserState = 0;
        break;
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(SERIAL_BAUD);
  Serial1.begin(SCOPE_UART_BAUD, SERIAL_8N1); // TX D14, idle HIGH
  // MKR board native USB: bounded wait for monitoring, not an infinite wait.
  unsigned long startMs = millis();
  while (!Serial && millis() - startMs < 2000UL) { }

  max31865.begin(MAX31865_3WIRE);

  Wire.begin();                  // SDA D11, SCL D12 on MKR WiFi 1010
  pca9685Ready = pwm.begin();   // returns false if controller is absent
  if (pca9685Ready) {
    pwm.setPWMFreq(SERVO_PWM_FREQUENCY_HZ);
    delay(10); // permit PCA9685 oscillator/PWM frequency to settle
    pwm.writeMicroseconds(PCA9685_SERVO_CHANNEL,
                          pulseForAbsoluteAngle(90)); // power-on center
  }
#if SERVO_FEEDBACK_ENABLED
  analogReadResolution(10); // default ADC range 0..1023; used by calibration
#endif
#if !COSMOS_BINARY_MODE
  Serial.println("TEMP_C,RES_OHMS,RAW_RTD,FAULT,LED,RATE_S");
#endif
  // First sample will be sent after the configured one-second interval.
  lastSampleMs = millis();
  lastServoTlmMs = millis();
  sendServoTelemetry();
}

void loop() {
  processCommands();
  unsigned long now = millis();
  if (now - lastServoTlmMs >= SERVO_TLM_INTERVAL_MS) {
    lastServoTlmMs = now;
    sendServoTelemetry();
  }
  if (now - lastSampleMs >= static_cast<unsigned long>(periodSeconds) * 1000UL) {
    lastSampleMs = now;
    sendTelemetry();
  }
}
