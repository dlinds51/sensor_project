# PCA9685 firmware fix — v0.3.2

This is a **firmware-only correction** for the compile error from the Adafruit PWM Servo Driver Library. The library defines `PCA9685_I2C_ADDRESS` as a macro, and v0.3.1 had a constant with the same name. The constant is now `SERVO_DRIVER_I2C_ADDR`.

## Install

1. In your Arduino sketch folder, keep only **one** `.ino` file. Use `MKR_MAX31865_COSMOS/MKR_MAX31865_COSMOS.ino` as the complete replacement sketch (rather than pasting the code into an existing second tab).
2. Verify/compile for **Arduino MKR WiFi 1010**; then upload.
3. Keep the existing COSMOS **v0.3.0 plugin** and serial bridge configuration unchanged.
4. Continue powering the MG996R from an independent suitably rated 5 V source; share ground with the Arduino.

## Unchanged

- MAX31865 SPI telemetry and calibration settings
- UART oscilloscope demo on Serial1/D14
- PCA9685 on I2C address 0x40, servo channel 0, 50 Hz
- Existing COSMOS command IDs and telemetry packets
- Reserved A0 servo feedback fields

The 'Multiple libraries were found' notices are informational and not the syntax error. Arduino toolchain/hardware compilation has not been performed in this environment.
