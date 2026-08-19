/*
 * WifiManagerBasics — bare WiFiManager captive-portal demo (no WiFiBase).
 *
 * WHAT THIS DEMONSTRATES
 *   The tzapu WiFiManager config portal used *directly*, with nothing from this
 *   repo in the way. It is the reference/baseline for WiFiBase's own portal path
 *   (`WiFiBase::useConfigPortal(true)`, which ends up in
 *   `WiFiBase::_startupConfigPortal()` calling the same
 *   `WiFiManager::startConfigPortal()`). Keep this sketch around to answer
 *   "is this WiFiBase's bug or WiFiManager's?" — if the behaviour reproduces
 *   here, it is not WiFiBase.
 *
 *   It also shows the two entry points side by side. The `#if 0` block selects
 *   between them:
 *     - `startConfigPortal()` (the compiled branch): ALWAYS raises the portal AP,
 *       ignoring any credentials the SDK already has stored. Good for testing the
 *       portal itself.
 *     - `autoConnect()` (the `#if 0` branch): tries the stored credentials first
 *       and only falls back to the portal when they fail. That is what a real
 *       product wants. Flip the `#if 0` to `#if 1` to try it.
 *
 * HARDWARE / SETUP
 *   - Any ESP32 dev board. Developed against `esp32doit-devkit-v1`, where
 *     `BUILTIN_LED` is GPIO 2.
 *   - GPIO 23 is driven as a second "alive" output. An LED there is optional —
 *     with nothing wired, the pin still toggles and the sketch behaves the same.
 *   - Serial monitor at 115200 baud. This is the only real UI: the portal SSID
 *     and the AP's IP address are printed there.
 *   - This example ships no `platformio.ini` of its own. To build it, copy the
 *     sibling `../WiFiBaseBasics/platformio.ini` in (same board and framework;
 *     this sketch needs none of the WiFiBase build flags), or point an existing
 *     ESP32 environment's `src_dir` at this directory. `WIFIMANAGER-ESP32` must
 *     be resolvable on `lib_dir` — see the sibling ini's `lib_dir` setting.
 *   - No credentials are compiled in. Everything is chosen at runtime through
 *     the portal, and the ESP32 SDK persists the winner to flash — which is
 *     exactly the stored-credential state `WiFiBase(useStored = true)` picks up
 *     on a later boot.
 *
 * WHAT YOU SHOULD SEE WHEN YOU RUN IT
 *   1. Serial prints `*** STARTING WIFIMANAGER BASICS ***` then
 *      `Starting autoConnect`, and `BUILTIN_LED` starts toggling once a second
 *      (`Ticker` at 1.0 s).
 *   2. The AP callback fires, printing `Entered config mode`, the soft-AP IP and
 *      the portal SSID (`ESP32`), and speeds the blink up to a 0.2 s toggle —
 *      that faster blink is the "waiting for you" signal. Note WiFiManager
 *      invokes this callback *before* `setupConfigPortal()` calls
 *      `WiFi.softAP()`, so the IP printed is whatever the AP interface already
 *      holds after `WiFi.mode(WIFI_AP_STA)` — normally the 192.168.4.1 default.
 *   3. A WPA2 network named `ESP32`, password `12345678`, appears. Join it from a
 *      phone or laptop. The captive-portal prompt usually opens by itself
 *      (WiFiManager runs a DNS server that answers every lookup with its own
 *      address); if it does not, browse to the IP printed in step 2.
 *   4. Pick "Configure WiFi", choose your network, enter its password, Save.
 *   5. `setup()` is BLOCKED at `startConfigPortal()` for all of the above — this
 *      sketch does nothing else until you finish. Because
 *      `setConfigPortalTimeout()` is never called, WiFiManager's timeout is 0,
 *      which it treats as "no timeout": the portal waits indefinitely and the
 *      `failed to connect and hit timeout` / `ESP.restart()` branch below is
 *      unreachable in this configuration. Add
 *      `wifiManager.setConfigPortalTimeout(seconds)` to exercise it.
 *   6. On a successful join, serial prints `connected...yeey :)`, the ticker
 *      detaches, and `loop()` takes over — GPIO 23 toggles every 500 ms forever.
 *      That steady toggle is the "portal finished, we are on the network" signal.
 *   7. Reboot without erasing flash and, in the compiled `startConfigPortal()`
 *      branch, you get the portal again — it does not reuse what you just saved.
 *      Switch to the `autoConnect()` branch to see it rejoin silently instead.
 *
 * CAVEATS
 *   - The portal AP password `12345678` is hard-coded and is demo-only. Anything
 *     shipped should take it from a build flag; note WiFiManager silently drops a
 *     password shorter than 8 characters and brings the AP up OPEN
 *     (`WiFiManager.cpp`, `setupConfigPortal()`), which is why WiFiBase's
 *     `configureAccessPoint()` refuses short passwords outright.
 *   - `wifiManager.resetSettings()` is left commented out at the top of `setup()`.
 *     Uncomment it for one flash to clear stored credentials, then comment it
 *     back — left in, it wipes them on every boot.
 *   - The closing `digitalWrite(..., LOW)` calls are commented "keep LED on",
 *     which is the active-low ESP8266 convention this sketch was ported from. On
 *     a DOIT devkit-v1 (GPIO 2, active high) LOW turns the LED off. Cosmetic.
 */

#include <Arduino.h>

#include <WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <FS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

//for LED status
#include <Ticker.h>
Ticker ticker;

void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Serial.println("*** STARTING WIFIMANAGER BASICS ***");

  //set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(23, OUTPUT);

  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(1.0, tick);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  Serial.println("Starting autoConnect");

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
#if 0
  if (!wifiManager.autoConnect("ESP32","12345678")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
#else
  if (!wifiManager.startConfigPortal("ESP32","12345678")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
#endif
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, LOW);
  digitalWrite(23, LOW);
}

bool value = 1;
void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(23, value);
  value = !value;
  delay(500);
}
