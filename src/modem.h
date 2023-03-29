/*
 * modemSetup.h
 * Code based on MqttClient.ino example code: https://github.com/Xinyuan-LilyGO/LilyGO-T-SIM7000G/tree/master/examples/MqttClient
 */

 // GSM modem model name
#define TINY_GSM_MODEM_SIM7000

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// Add a reception delay, if needed.
// This may be needed for a fast processor at a slow baud rate.
// #define TINY_GSM_YIELD() { delay(2); }

// Define how you're planning to connect to the internet
// These defines are only for this example; they are not needed in other code.
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false


// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
/*
const char apn[]      = APN_;
const char gprsUser[] = "";
const char gprsPass[] = "";
*/

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <SPI.h>
#include <SD.h>

// Just in case someone defined the wrong thing..
#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif
TinyGsmClient client(modem);

Ticker tick;

// Modem pins
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

// fancy low power mode - while connected
void modem_sleep() // will have an effect after reboot and will replace normal power down
{
  Serial.println("Going to sleep now with modem in power save mode");
  // needs reboot to activa and takes ~20sec to sleep
  //modem.PSM_mode();    //if network supports will enter a low power sleep PCM (9uA)
  //modem.eDRX_mode14(); // https://github.com/botletics/SIM7000-LTE-Shield/wiki/Current-Consumption#e-drx-mode
  modem.sleepEnable(); //will sleep (1.7mA), needs DTR or PWRKEY to wake
  pinMode(PIN_DTR, OUTPUT);
  digitalWrite(PIN_DTR, HIGH);
}

void modem_off()
{
  //if you turn modem off while activating the fancy sleep modes it takes ~20sec, else its immediate
  Serial.println("Going to sleep now with modem turned off");
  //modem.gprsDisconnect();
  //modem.radioOff();
  modem.sleepEnable(false); // required in case sleep was activated and will apply after reboot
  modem.poweroff();
}

void shutdownModule()
{
  //modem_sleep();
  modem_off();

  //delay(1000);
  //Serial.flush();
  //esp_deep_sleep_start();
  Serial.print("Going to sleep ");
  Serial.print(TIME_TO_SLEEP_);
  Serial.println(" minutes...");
  delay(1000);
  Serial.flush();
  //esp_deep_sleep_start();
  //goToSleep(true, TIME_TO_SLEEP_);
}
