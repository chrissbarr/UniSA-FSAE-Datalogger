# UniSA-FSAE-Datalogger
This is an Arduino shield designed to be used for logging data on the UniSA SAE Team's vehicle. It provides interfaces to various components - primarily sensors, but there are also user-interface elements as well.

## Connections
### Sensor Inputs
* GPS
* IR Thermopile
* Thermistors
* Potentiometers
* CAN Bus

### User Interface
* Adafruit OLED Display
* Physical Push-buttons
* NFC Tag Read / Write

### Outputs
* Log Data to microSD Card
* Transmit Data via Wireless Link (UART-based)
* Power and Control WS2812 LEDs for Indication and Illumination

### Power
* Board space / pin header to connect a [Pololu D24V50F5](https://www.pololu.com/product/2851) step-down voltage regulator.
 * 7-38V Input
 * Max. 5A Current
* SMD fuse for short-circuit protection
* Diode for reverse-polarity protection

## Compatibility
Originally designed to be compatible with the Arduino Due, problems with I2C communication between some of the peripherals (primarily the NFC module) and the Due caused a shift to the Arduino Mega2560. Form factor remains unchanged, and the Due may still be useable provided the compatibility issue is overcome or disregarded.

The primary concern is sufficient IO, which the Arduino Uno (and other similar form-factor boards) were lacking. Use of the Arduino Due would allow for the CAN Bus controller inside the AT91SAM3X8E to be used (removing the need for the MCP2515 CAN Bus controller IC).
