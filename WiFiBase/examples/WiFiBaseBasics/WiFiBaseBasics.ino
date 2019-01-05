
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

#ifndef USE_SSID
#define USE_SSID "unknown"
#endif
#ifndef USE_PASSWD
#define USE_PASSWD "unknown"
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
  wfb->addKnownNetwork(USE_SSID, USE_PASSWD);
  wfb->startup();
}

bool value = 1;
void loop() {
  if (wfb->connected()) {
    // put your main code here, to run repeatedly:
    digitalWrite(STATUS_LED, value);
    value = !value;
    delay(500);
  }
}