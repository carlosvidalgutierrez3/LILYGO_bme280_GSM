This example code uses the [LILYGO T-SIM7000G](https://github.com/Xinyuan-LilyGO/LilyGO-T-SIM7000G) to send temperature, humidity and pressure data, using the [BME280](https://learn.adafruit.com/adafruit-bme280-humidity-barometric-pressure-temperature-sensor-breakout/overview) sensor. The data is sent over HTTP using the GSM module combining the esp32 wi-fi libraries and the own GSM libraries of the LILYGO.


# Set-up with Arduino IDE
In the Arduino IDE, go to **Sketch > Include Library > Manage Libraries** and install the following libraries:
### Lilygo libraries
- StreamDebugger by Volodymyr Shymanskyy
- TinyGSM by Volodymyr Shymanskyy
- PubSubClient by Nick O'Leary

### BME280 libraries
- Adafruit BME280 Library by Adafruit
- Adafruit Unified Sensor by Adafruit

Select this board on the Arduino IDE:

Go to **Tools > Board > ESP32 Arduino > ESP32 Dev Module**.

For a more detailed set-up, you can follow [this tutorial](https://randomnerdtutorials.com/lilygo-t-sim7000g-esp32-lte-gprs-gps/).

## Code set-up
In the example code, a sensor ID is used and it's defined in `config/LILYGO_bme280_GSM`. Change it to your desired value. In the same file, you can also change the APN.

In `include/http_temp/http_temp_config.h` set up the credentials for your HTTP server.
