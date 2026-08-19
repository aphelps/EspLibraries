/*
 * Author: Adam Phelps
 * License: MIT
 * Copyright: 2018
 *
 * This library provides a base from which to handle connecting to a Wifi
 * network.
 *
 * Design:
 *   An instance of the class is created with appropriate options, the default
 * once startup() is called will launch a "background" process which searches
 * for any known WiFi network from its list to connect to.  On a failure to
 * find a network, it will launch an access point.  The access point can provide
 * a config portal to allow manual configuration as well as setting up a hub
 * for a mesh network.
 *
 *   NOTE: this class provides NO over-the-air update support.  This comment
 * used to claim that it "will also provide a port for receiving over-the-air
 * firmware updates, and optionally redistribute those updates when acting as a
 * hub" — that was a description of an intention, not of the code, and the class
 * has never had an OTA method of any kind.  A caller that needs OTA builds it
 * on getServer(), as HMTL_Fire_Control does with a guarded /update endpoint.
 *
 * Notes:
 *   - OTA is deliberately left to the caller: the guards that make an update
 *     safe are application knowledge (is anything running? is it safe to
 *     reboot?), and a generic library cannot answer those questions.
 *   - Wifi configuration from WiFiManager (TODO: link)
 *   - Actually do back-grounding of blocking processes
 */

#ifndef WIFIBASE_H
#define WIFIBASE_H

#include <Ticker.h>

#include <WiFiManager.h>

struct network {
  char *ssid;
  char *passwd;
};

/*
 * WiFiBase owns heap state (_knownNetworks, _server) and its destructor frees it and
 * disconnects WiFi, but the class has no copy constructor or copy assignment: assigning a
 * temporary (`wfb = WiFiBase()`) memberwise-copies the pointers and then the temporary's
 * destructor frees them, leaving the assigned object dangling. Construct one instance in
 * place and configure it. (Copy operations become `= delete` once existing callers that
 * copy-assign are migrated.)
 */
class WiFiBase {
  public:
    WiFiBase(boolean useStored = true);
    ~WiFiBase();

    bool configBackground(bool background);
    bool configureAccessPoint(const char *ssid, const char *passwd);
    bool useConfigPortal(bool configPortal);
    bool disableAccessPoint();

    /*
     * Endpoints that change WiFi state or disclose stored networks (/network, /scan,
     * /known) require HTTP Basic auth (user "admin") once a password is set. With no
     * password set and no explicit opt-out they respond 403 — secure by default; a
     * caller that wants the old open behaviour must say so.
     */
    bool setAuthPassword(const char *passwd);
    bool allowUnauthenticatedConfig(bool allow);
    /* Run the same authorization for a caller-added endpoint; sends the 401/403
     * response itself when refusing, so the handler just returns. */
    bool authorizeConfigRequest();

    static const uint8_t INDEX_DISCONNECTED = (uint8_t)-1;
    static const uint8_t MAX_KNOWN_NETWORKS = 255;
    uint8_t addKnownNetwork(const char *ssid, const char *passwd);
    int numKnownNetworks();
    uint8_t lookupKnownNetwork(const char *ssid);
    bool hasKnownNetwork(const char *ssid);
    bool connectAddKnownNetwork(const char *ssid, const char *passwd);

    bool setConnectTimeoutMs(unsigned long ms);
    bool setServerPort(int port);
    WebServer *getServer();

    /* Start WiFiBase */
    bool startup();
    bool connected();

    /* Check the web server for traffic */
    void checkServer();

    /* REST API configuration */
    void addRESTEndpoint(const String &endPoint,
                         WebServer::THandlerFunction handler,
                         const String &docString);

  protected:
    bool _running;
    bool _background;

    WiFiManager *_wifiManager; // TODO: Should this be a temporary object?

    bool _startupConnect();

    /* Config portal and network hub */
    bool _configPortal;
    bool _accessPointEnabled;
    bool _accessPointActive;
    const char *_APSsid;
    const char *_APPasswd;
    bool _startupConfigPortal();
    bool _startupAccessPoint();
    bool _shutdownAccessPoint();

    /* Known networks */
    uint8_t _numKnownNetworks;
    uint16_t _allocatedKnownNetworks;
    struct network *_knownNetworks;


    static const unsigned long DEFAULT_CONNECT_TIMEOUT = 10 * 1000;
    unsigned long _connectionTimeoutMs = 5*1000;
    uint8_t _connectedIndex;
    bool _connectToNetwork();
    bool _connectToNetwork(const char *ssid, const char *passwd);
    bool _connectWait();
    void _setConnected(uint8_t index);
    void _setDisconnected();

    int _serverPort = 80;
    WebServer *_server;
    bool _createServer();

    /* Auth for state-changing/disclosing endpoints */
    const char *_authPasswd;
    bool _allowUnauthConfig;
    bool _authorizedConfig();

    /*
     * Server endpoints
     */
    void _handleDocumentation();
    void _handleInfo();
    void _handleNetwork();
    void _handleNotFound();
    void _handleScan();
    void _handleListKnownNetworks();

    String documentation; // REST Endpoint documentation json

    /* TODO: Over-the-air updates */
    //uint16_t _updatePort;
    //bool _distributeUpdates;
};


#endif // WIFIBASE_H