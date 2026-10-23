# Start Here - PCA9685 servo driver update v0.3.1

This is a **firmware-only patch** for the already-created v0.3.0 RTD / UART /
servo project. Do not install a new COSMOS plugin. Existing MKR_RTD screens,
commands, and 5 Hz servo telemetry remain the same.

Changed files:
- `MKR_MAX31865_COSMOS/MKR_MAX31865_COSMOS.ino` (REPLACE existing sketch)
- `openc3-cosmos-mkr-rtd/README.md` (UPDATED project documentation)
- `START_HERE.md` (UPDATED instructions)

Before uploading:
1. Install `Adafruit PWM Servo Driver Library` in Arduino IDE Library Manager
   (accept dependency prompts). Retain `Adafruit MAX31865`.
2. Wire MKR 3.3V->PCA VCC, GND->PCA GND, D11->SDA, D12->SCL.
3. Supply the PCA servo V+ with **external regulated 5V high-current power**;
   connect its negative to shared GND (including MKR).
4. Plug MG996R into PCA channel 0 (orange signal, red V+, brown GND).
5. Disconnect the servo's original direct Arduino D5 control wire;
   leave existing RTD SPI D7-D10 and scope UART TX D14 intact.
6. Initially remove the servo horn/load to avoid end-stop damage.
7. Upload updated sketch. Avoid leaving a second `.ino` in the sketch folder.

Arduino sketch uses PCA9685 at I2C address 0x40, channel 0, 50 Hz.
On startup it requests center (1500 us). Standard +/-90 COSMOS commands map
1000..2000 us as before. Tune limits for your particular MG996R if needed.

Continue using installed v0.3.0 COSMOS gem, your original serial bridge,
and existing RTD, UART_DEMO, SERVO_CONTROL screens. See updated README.

Limitations: three-wire servo has no real position sensor; display shows
commanded angle. A0 is reserved for a future feedback-capable servo.
