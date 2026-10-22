// Arduino MKR WiFi 1010 + Adafruit MAX31865 3-wire RTD
// Binary COSMOS protocol adapted from the OpenC3 Arduino Sensor Kit demo.
// https://github.com/OpenC3/openc3-cosmos-arduino-sensor-kit
//
// In binary mode, DO NOT use Serial.print() on this serial port.
// For local Serial Monitor verification, change COSMOS_BINARY_MODE to 0,
// upload, then use Serial Monitor at 9600 baud.

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>
#include <math.h>

#define COSMOS_BINARY_MODE 1

// MKR WiFi 1010: SCK D9, MOSI D8, MISO D10, CS D7.
static constexpr uint8_t MAX31865_CS = 7;

// IMPORTANT: PT100/430-ohm assumptions. Check the RTD AND the Rref
// resistor fitted on the breakout before taking valid measurements.
// For a PT1000 with 4300-ohm Rref, use 1000.0f and 4300.0f.
static constexpr float RTD_NOMINAL = 100.0f;
static constexpr float RTD_RREF = 430.0f;

static constexpr uint32_t SYNC_PATTERN = 0xABCD1234UL;
static constexpr uint8_t RTD_PACKET_ID = 6;  // IDs 1-5 reserved by demo
static constexpr uint8_t CMD_START = 0x11;
static constexpr uint8_t CMD_SET_LED = 0x01;
static constexpr uint8_t CMD_SET_PERIOD = 0x02;

static constexpr uint32_t SERIAL_BAUD = 9600;
Adafruit_MAX31865 max31865(MAX31865_CS); // hardware SPI

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
    default:
      // Unknown command: ignore. Future: servo / relays / motors.
      break;
  }
}

void processCommands() {
  // Decode original demo's 3-byte commands: [0x11][command ID][argument]
  static uint8_t parserState = 0;
  static uint8_t commandId = 0;
  while (Serial.available() > 0) {
    uint8_t incoming = static_cast<uint8_t>(Serial.read());
    switch (parserState) {
      case 0:
        if (incoming == CMD_START) parserState = 1;
        break;
      case 1:
        commandId = incoming;
        parserState = 2;
        break;
      case 2:
        executeCommand(commandId, incoming);
        parserState = 0;
        break;
      default:
        parserState = 0;
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(SERIAL_BAUD);
  // MKR board native USB: bounded wait for monitoring, not an infinite wait.
  unsigned long startMs = millis();
  while (!Serial && millis() - startMs < 2000UL) { }

  max31865.begin(MAX31865_3WIRE);
#if !COSMOS_BINARY_MODE
  Serial.println("TEMP_C,RES_OHMS,RAW_RTD,FAULT,LED,RATE_S");
#endif
  // First sample will be sent after the configured one-second interval.
  lastSampleMs = millis();
}

void loop() {
  processCommands();
  unsigned long now = millis();
  if (now - lastSampleMs >= static_cast<unsigned long>(periodSeconds) * 1000UL) {
    lastSampleMs = now;
    sendTelemetry();
  }
}
