# MKR WiFi 1010: RTD + UART Scope + Servo Control v0.3.1 (PCA9685)

**Firmware-only update to v0.3.0.** Keep the existing `openc3-cosmos-mkr-rtd-0.3.0.gem` installed. No COSMOS target, command, telemetry, or screen files change.

## Important: present 3-wire servo versus future 4-wire feedback
Current servo positions are **commands**, NOT measurements. Status fields:
- `MKR_RTD SERVO CMD_DEG`: desired angle `-90..+90` relative to center
- `MKR_RTD SERVO DRIVE_ABS_DEG`: absolute commanded angle used for PCA9685 pulse mapping (0..180)
- `MKR_RTD SERVO CMD_SEQ`: incremented on every accepted servo command
- `MKR_RTD SERVO FEEDBACK_VALID`: 0 without separate feedback wire
- `MKR_RTD SERVO FEEDBACK_DEG`: -32768 sentinel if unavailable
- `MKR_RTD SERVO FEEDBACK_RAW`: 65535 sentinel if unavailable
Servo status reports 5 Hz, with immediate status on valid commands.

## Wiring: MKR WiFi 1010 -> PCA9685 -> MG996R

| MKR WiFi 1010 | PCA9685 | Notes |
|---|---|---|
| 3.3V | VCC | Logic power; do not confuse with servo V+ |
| GND | GND | Common ground shared with external servo supply |
| D11 / SDA | SDA | I2C data |
| D12 / SCL | SCL | I2C clock |

| PCA9685 / Supply | MG996R servo | Notes |
|---|---|---|
| Channel 0 PWM pin | Orange/yellow control wire | Signal is driven by PCA9685, NOT MKR D5 |
| Channel 0 V+ | Red servo power wire | Externally supplied servo voltage |
| Channel 0 GND | Brown/black wire | Common ground |
| External regulated +5V | PCA9685 V+ / terminal block positive | Supply capable of high MG996R startup/stall current; use ~3 A or greater for one servo |
| External supply negative | PCA9685 GND / terminal block negative | Also connected to MKR GND |

**Do NOT** power the MG996R from the MKR 5V or 3.3V pin. PCA9685
logic **VCC is 3.3V**; servo rail **V+ is external 5V**. The servo
outputs require both rails and a common ground. Default PCA9685 I2C
address is `0x40` and the sketch uses channel 0. If you have a different
board variant or address jumpers set, adjust the source.

Keep existing peripherals:
- MAX31865: D7 CS, D8 MOSI, D9 SCK, D10 MISO, 3.3V, GND.
- UART oscilloscope: D14 / TX1 and Arduino GND.
- A0: reserved for a future *analog* servo position feedback wire.
- D5: now **unused** (disconnect the old direct servo signal wire).

Start with the servo horn removed and no mechanical load; the code sends
center pulse at startup. The pre-existing 1000..2000 us range is maintained,
but may exceed your specific servo's safe travel. Tighten
`SERVO_MIN_PULSE_US` / `SERVO_MAX_PULSE_US` if binding occurs.

## Change summary vs v0.3.0

- Replaced `Servo.h` / D5 direct servo drive with `Wire.h` and
  `Adafruit_PWMServoDriver.h` using the PCA9685 over I2C.
- New PCA9685 constants: address `0x40`, output channel `0`, frequency `50 Hz`.
- Startup initializes the PCA9685 and requests a center position.
- The original `SET_SERVO` 3-byte command, servo telemetry packet, RTD,
  UART, temperature calibration, and COSMOS screens remain unchanged.
- If no PCA9685 is detected at startup, servo commands are ignored;
  RTD/UART and periodic command-status telemetry remain available.

## Install (firmware-only update)

1. Back up your working v0.3.0 Arduino sketch.
2. In Arduino IDE Library Manager, install **Adafruit PWM Servo Driver Library**
   and its prompted dependencies. Retain the existing **Adafruit MAX31865**
   library. `Wire` is built into the board core; `Servo` is no longer needed.
3. Replace the code in the Arduino sketch folder
   `MKR_MAX31865_COSMOS/MKR_MAX31865_COSMOS.ino` with the updated `.ino`.
   Keep **one** `.ino` in that sketch folder. Select **MKR WiFi 1010**.
4. Wire the PCA9685 and external servo supply as above. Power the servo
   circuit safely; the servo may center itself when the sketch starts.
5. Upload. Close Arduino Serial Monitor and start your **unchanged** Windows
   serial bridge (9600 baud, TCP 2950).
6. Continue using the previously installed `openc3-cosmos-mkr-rtd-0.3.0.gem`.
   Do **not** reinstall/upgrade COSMOS just for the PCA9685.
7. Use the existing `MKR_RTD > SERVO_CONTROL` and `UART_DEMO` screens.
   Check RTD first, UART next, servo last.

## Servo command protocol
USB command `[0x11,0x04,ANGLE_ABS]`, fixed 3 bytes.
`ANGLE_ABS=0` -> -90, 90 -> zero/center, 180 -> +90.
The PCA9685 generates 50 Hz PWM on channel 0; the nominal pulse range
remains 1000..2000 us with 1500 us at center.
This is a *servo position request*, not a calibrated mechanical degree.
Do not hold the horn against its end stops. Confirm travel for your model.

## Servo telemetry packet
Original sync and length framing retained. Length=11, ID=7:
`ID:u8, CMD_DEG:i16, FEEDBACK_DEG:i16, FEEDBACK_VALID:u8,
DRIVE_ABS_DEG:u8, CMD_SEQ:u16, FEEDBACK_RAW:u16`, little-endian.

## Adding real feedback in the future
Firmware already allocates A0 as future feedback input and reserves feedback
fields in the telemetry packet. Install the 4-wire feedback servo, verify the
feedback output **never exceeds 3.3V** at A0; if feedback is 0..5V use e.g.
a divider Rtop=12kohm, Rbottom=20kohm to scale 5V to about 3.13V; verify
with a multimeter, including power-supply tolerance, before connection. Then measure ADC values at
known -90/+90 using a calibration sketch, edit `FEEDBACK_RAW_NEG90` and
`FEEDBACK_RAW_POS90`, set `SERVO_FEEDBACK_ENABLED 1`, re-upload. Firmware
then reports measured angle and FEEDBACK_VALID=1 without changing the
command or telemetry packet definitions. Calibration values in source are
*placeholders*, not valid for actual hardware.

## Slider control status
OpenC3 has no standard slider widget in its screen language. The install-ready
screen uses a centered -90..+90 text entry, the required 3 quick buttons,
and live telemetry. A true slider **prototype source** is included under
`src/ServoSliderWidget.vue` but not built or installed. It will require a
custom Vue widget build/validation before use; see `SLIDER_WIDGET_DEVELOPMENT.md`.

## Verification notes
Firmware code is structurally checked against the unchanged v0.3.0 plugin
protocol; it has not been compiled for the MKR board or tested with physical
Arduino/PCA9685 hardware. The previously built v0.3.0 gem is unchanged. Test RTD first, then UART, finally servo. Servo power
problems can cause resets and apparent loss of RTD telemetry.
