
#include <WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <FS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#ifndef DEBUG_LEVEL
  #define DEBUG_LEVEL DEBUG_HIGH
#endif
#include <Debug.h>

#include <WiFiBase.h>

#ifndef USE_PASSWD
  #define USE_PASSWD ""
#endif

#ifndef CONFIG_SSID
  #define CONFIG_SSID "Esp32_WiFiBase"
#endif
#ifndef CONFIG_PASSWD
  #define CONFIG_PASSWD "12345678"
#endif

#define STATUS_LED 23

WiFiBase *wfb;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Serial.println("*** STARTING WIFIBASE BASICS ***");

  //set led pins as output
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  digitalWrite(STATUS_LED, LOW);

  wfb = new WiFiBase(false);
#ifdef USE_SSID
  wfb->addKnownNetwork(USE_SSID, USE_PASSWD);
#endif
#ifdef CONFIG_PORTAL
  /* The the WiFiBase to generate an access point hosting a config portal */
  wfb->useConfigPortal(true);
#endif
  wfb->configureAccessPoint(CONFIG_SSID, CONFIG_PASSWD);

  wfb->startup();
}

bool value = 1;
unsigned long blink = 0;
void loop() {
  unsigned long now = millis();
  if (wfb->connected() && (now - blink > 500)) {
    // put your main code here, to run repeatedly:
    digitalWrite(STATUS_LED, value);
    value = !value;
    blink = now;
  }

  wfb->handle();
}