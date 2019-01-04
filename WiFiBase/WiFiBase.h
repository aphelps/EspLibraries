/*
 * Author: Adam Phelps
 * License: MIT
 * Copyright: 2018
 *
 * This library provides a base from which to handle connecting to a Wifi
 * network as well as updating firmware via that network.
 *
 * Design:
 *   An instance of the class is created with appropriate options, the default
 * once startup() is called will launch a "background" process which searches
 * for any known WiFi network from its list to connect to.  On a failure to
 * find a network, it will launch an access point.  The access point provides
 * a config portal to allow manual configuration as well as setting up a hub
 * for a mesh network.
 *   By default the class will also provide a port for receiving over-the-air
 * firmware updates, and optionally redistribute those updates when acting as a
 * hub.
 *
 * Notes:
 *   - Use the Update and ArduinoOTA libraries?
 *   - Wifi configuration from WiFiManager (TODO: link)
 */

#ifndef WIFIBASE_H
#define WIFIBASE_H

#include <WiFiManager.h>

struct network {
  char *ssid;
  char *passwd;
};

class WiFiBase {
  public:
    WiFiBase(boolean useStored = true);
    ~WiFiBase();

    bool configBackground(bool background);
    bool configureAccessPoint(const char *ssid, const char *passwd);
    bool disableAccessPoint();

    const uint8_t MAX_KNOWN_NETWORKS = 255;
    bool addKnownNetwork(const char *ssid, const char *passwd);
    int numKnownNetworks();
    bool hasKnownNetwork(const char *ssid);

    /* Start WiFiBase */
    bool startup();

  private:
    bool _running;
    bool _background;

    WiFiManager *_wifiManager; // TODO: Should this be a temporary object?

    /* Config portal and network hub */
    bool _accessPointEnabled;
    bool _accessPointActive;
    const char *_APSsid;
    const char *_APPasswd;
    bool _startupAccessPoint();
    bool _shutdownAccessPoint();

    /* Known networks */
    uint8_t _numKnownNetworks;
    uint16_t _allocatedKnownNetworks;
    struct network *_knownNetworks;
    bool _connectToNetwork();

    /* Over-the-air updates */
    uint16_t _updatePort;
    bool _distributeUpdates;
};


#endif // WIFIBASE_H