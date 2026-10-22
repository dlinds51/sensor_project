# MKR WiFi 1010 / MAX31865 / OpenC3 COSMOS

- Arduino sketch: `MKR_MAX31865_COSMOS/MKR_MAX31865_COSMOS.ino`
- COSMOS plugin source: `openc3-cosmos-mkr-rtd/`
- Ready-to-upload plugin gem: `openc3-cosmos-mkr-rtd-0.1.0.gem`

## Pin mapping
| MAX31865 | MKR WiFi 1010 |
| --- | --- |
| VIN | 3.3 V |
| GND | GND |
| CLK / SCK | D9 |
| SDI / MOSI | D8 |
| SDO / MISO | D10 |
| CS | D7 |

RTD sensor: paired red wires F+ and RTD+; remaining silver wire RTD-;
F- empty. For 3-wire board mode, the 2/3 Wire jumper is closed, original
2-wire trace above Rref cut, AND the right-side jumper marked 3 closed.

Confirm PT100/PT1000 and which Rref resistor your Adafruit board carries.
Default sketch constants assume PT100 / 430 ohms.

See plugin README for bridge and COSMOS instructions.
