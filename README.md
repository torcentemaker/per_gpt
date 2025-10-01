## per_gpt

This sketch targets the Arduino MKR WiFi 1010 with the MKR IoT Carrier. It reads the onboard environment sensor, smooths the telemetry to minimise noise, logs the filtered values over the serial monitor, visualises the current temperature on the carrier's RGB LEDs, and persists the latest baseline readings in flash memory using the `FlashStorage` library.

### Features
- Initializes the MKR IoT Carrier and reports persisted baseline data on boot.
- Samples temperature and humidity every two seconds, applies exponential smoothing, and prints the filtered results to the serial console.
- Maps the smoothed temperature to a blue→red LED gradient so you can see changes at a glance without the LEDs being overly bright.
- Stores new baseline readings in flash whenever the temperature or humidity drifts significantly from the saved values.
- Lets you store a new baseline immediately by tapping touch pad 0 on the carrier, or clear the stored baseline with touch pad 4.

### Requirements
- [Arduino MKR WiFi 1010](https://store.arduino.cc/products/arduino-mkr-wifi-1010)
- [Arduino MKR IoT Carrier](https://store.arduino.cc/products/arduino-mkr-iot-carrier)
- Arduino libraries: `Arduino_MKRIoTCarrier`, `FlashStorage`

### Build
```bash
arduino-cli core install arduino:samd
arduino-cli lib install Arduino_MKRIoTCarrier FlashStorage
arduino-cli compile --fqbn arduino:samd:mkrwifi1010 .
```
