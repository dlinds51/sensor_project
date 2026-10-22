# MKR WiFi 1010 / MAX31865 COSMOS plugin

Uses the same 9600-baud binary wire protocol, LengthProtocol and host serial
bridge as the upstream OpenC3 Arduino Sensor Kit demonstration.

## Install
1. Load `MKR_MAX31865_COSMOS.ino` in Arduino IDE for MKR WiFi 1010.
2. Install the `Adafruit MAX31865` library and its dependencies.
3. Confirm MAX31865 3-wire configuration and RTD type / reference resistor.
4. Upload the sketch with `COSMOS_BINARY_MODE 1`.
5. Close Arduino Serial Monitor (only one process may own the COM port).
6. Start OpenC3 using its Docker setup.
7. On the Windows host, start the serial bridge, replacing COM5 with your COM port:

   `openc3cli bridgegem openc3-cosmos-bridge-serial router_port=2950 write_port_name=COM5 read_port_name=COM5 baud_rate=9600 parity=NONE`

8. Install the packaged `.gem` through COSMOS Admin > Plugins > Install.
9. In Command and Telemetry Server verify MKR_RTD_INT connected.
10. Open Packet Viewer, select MKR_RTD > RTD. Expect TEMP_C, RES_OHMS,
    RAW_RTD, FAULT, LED and RATE_S approximately every second.
11. Try commands MKR_RTD SET_LED (STATE ON / OFF) and MKR_RTD SET_PERIOD.

## Bench-only Serial Monitor
Define `COSMOS_BINARY_MODE 0`, re-upload, and use Arduino Serial Monitor at
9600 baud. This outputs CSV. Set it back to 1 and re-upload for COSMOS.
Text output must **not** be mixed with binary telemetry on the same port.

## Binary packet format
On wire: sync bytes `34 12 CD AB` (little-endian 0xABCD1234),
payload size byte `0E` (14), followed by:

- Offset 0: ID = 6 (uint8)
- Offset 1: TEMP_C (float32 little-endian)
- Offset 5: RES_OHMS (float32 little-endian)
- Offset 9: RAW_RTD (uint16 little-endian)
- Offset 11: FAULT (uint8)
- Offset 12: LED (uint8)
- Offset 13: RATE_S (uint8)

LengthProtocol parses the sync/length and discards the 5 framing bytes,
leaving the 14-byte telemetry payload for COSMOS.
Commands sent from COSMOS to Arduino are three bytes:
`[0x11, 0x01, 0x00 or 0x01]` for LED and `[0x11, 0x02, 1..60]` for rate.

## Faults and safety
Temperature is NaN if the MAX31865 reports faults; monitor FAULT.
The Arduino logic is 3.3 V. Do not drive relays, motors, or servo power
from a GPIO pin; use properly-rated drivers, separate power and commons,
interlocks as needed, and safe power-on defaults.

This draft is protocol-checked against the upstream demo, but is not
hardware-tested with a physical Arduino or a running COSMOS deployment.
