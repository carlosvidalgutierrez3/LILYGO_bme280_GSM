/*
 * Arduino IDE settings:
 *  - Board: ESP32 Arduino -> ESP32 Dev Module
 * 
 * Code based on MqttClient.ino example code: https://github.com/Xinyuan-LilyGO/LilyGO-T-SIM7000G/tree/master/examples/MqttClient
 * 
 */

// esp32 libraries
#include <esp_wifi.h>
#include <esp_bt.h>

// config file
#include "config/LILYGO_bme280_GSM_config.h"

// http and gsm modem libraries
#include "src/modem.h"
#include "src/http_temp/http_temp.h"
#include "src/protobuf_temp.h"

// bme280 sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Your GPRS credentials, if any
const char apn[]      = APN_;
const char gprsUser[] = "";
const char gprsPass[] = "";

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C

void goToSleep(bool deep_sleep, uint64_t sleep_time) {
  //cam sleep
  btStop();
  //adc_power_off();
  esp_wifi_stop();
  esp_bt_controller_disable();
  Serial.print("Sleep time in us: ");
  Serial.println(sleep_time * MINUTES_TO_uS_);
  esp_sleep_enable_timer_wakeup(sleep_time * MINUTES_TO_uS_);
  if (deep_sleep) esp_deep_sleep_start();
  esp_light_sleep_start();
}

void setup()
{
    // Set console baud rate
    Serial.begin(115200);
    delay(10);

    unsigned status = bme.begin();
    int i=0;
    float temp{0.0};
    float pres{0.0};
    float hum{0.0};

    Serial.print("temp1: ");
    Serial.println(temp);
      
    while(!status && i<10){
      Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
      Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
      delay(100);
      i++;
    }

    if(status){
      Serial.println("BME280 begun correctly!");
      delay(1000);
      
      // We need to take the measurement now, because modem initializations will close the i2c connection with the sensor
      temp = bme.readTemperature();
      pres = bme.readPressure() / 10000.0F;
      hum = bme.readHumidity();
    }

    Serial.print("temp2: ");
    Serial.println(temp);
    
    // We need to do WiFi.begin() even though we don't use wifi
    // Esp32's internal clock, tcp, other components and features are only initiated when wifi is initiated. 
    WiFi.begin();
      
    uint8_t msgBuffer[MAX_MSG_SIZE_];
    HTTPSClient http;
    http_init(http);
    
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    delay(300);
    digitalWrite(PWR_PIN, LOW);

    Serial.println("\nWait...");

    delay(1000);

    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    Serial.println("Initializing modem...");
    if (!modem.restart()) {
        Serial.println("Failed to restart modem, attempting to continue without restarting");
    }

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork()) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");

    if (modem.isNetworkConnected()) {
        SerialMon.println("Network connected");
    }

#if TINY_GSM_USE_GPRS
    // GPRS connection parameters are usually set after network registration
    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");

    if (modem.isGprsConnected()) {
        SerialMon.println("GPRS connected");
    }
#endif
    
    measurments[0].sensor_id = SENSOR_ID_;
    measurments[0].x = temp;
    measurments[0].y = pres;
    measurments[0].z = hum;
    
    msg.generic3D.funcs.encode = &generic3D_callback;
    msg.generic3D.arg = NULL;
    
    if (protobuf_converter(msgBuffer)) {
      Serial.print("temp3: ");
      Serial.println(temp);
      http_put(http, "application/x-protobuf", msgBuffer);
    }
    
    SerialMon.println("Delay 10s ...");

    delay(10000);

    // Implement go to sleep. End "Wifi.begin()" if needed. Disconnect all modules before going to sleep
    // shut down modem
    shutdownModule();
    // esp_deep_sleep_start();
    goToSleep(true, TIME_TO_SLEEP_);
}

void loop(){}
