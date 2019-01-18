/*
 * Author: Adam Phelps
 * License: MIT
 * Copyright: 2018
 *
 * Implementation of WiFiBase's WebServer and REST implementation
 */

#include <Arduino.h>
#include <WiFi.h>


#ifdef DEBUG_LEVEL_WIFIBASEHANDLERS
  #define DEBUG_LEVEL DEBUG_LEVEL_WIFIBASEHANDLERS
#endif
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_HIGH
#endif
#include <Debug.h>

#include "WiFiBase.h"

/**
 * Startup the management server
 * @return
 */
bool WiFiBase::_createServer() {
  if (!_server) {
    _server = new WebServer(_serverPort);
    if (!_server) {
      DEBUG_ERR("WFB: alloc failure");
      return false;
    }

    DEBUG4_VALUELN("WFB: server on ", _serverPort);

    _server->on("/documentation", std::bind(&WiFiBase::_handleDocumentation, this));
    _server->on("/info", std::bind(&WiFiBase::_handleInfo, this));
    _server->on("/network", std::bind(&WiFiBase::_handleNetwork, this));
    _server->on("/scan", std::bind(&WiFiBase::_handleScan, this));
    _server->onNotFound(std::bind(&WiFiBase::_handleNotFound, this));
    _server->begin();

    return true;
  }

  return true;
}

/**
 * Endpoint handler to get documentation for the server
 */
void WiFiBase::_handleDocumentation() {
  DEBUG4_PRINTLN("WFB: /documentation");

  String response = "{\"/documentation\":{},";
  response += "\"/info\":{},";
  response += "\"/network\":{\"description\":\"connect to network\",\"args\":[\"ssid\",\"passwd\"]},";
  response += "\"/scan\":{}";
  response += "}";

  _server->send(200, "application/json", response);
}

void WiFiBase::_handleInfo() {
  DEBUG4_PRINTLN("WFB: /info");
  String response = "{\"response\":\"This is info\"}";
  _server->send(200, "application/json", response);
}

void WiFiBase::_handleNotFound() {
  DEBUG4_VALUELN("WFB: notFound:", _server->uri());

  String response = "Endpoint " + _server->uri() + " not defined";
  _server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  _server->sendHeader("Pragma", "no-cache");
  _server->sendHeader("Expires", "-1");
  _server->sendHeader("Content-Length", String(response.length()));
  _server->send ( 404, "text/plain", response );
}

/**
 * Attempt to connect to a network specified by the arguments, adding it to
 * the known networks on successful connect.
 */
void WiFiBase::_handleNetwork() {
  int result = 200;
  String ssid = _server->arg("ssid");
  String passwd = _server->arg("passwd");

  DEBUG4_VALUELN("WFB: /network ", ssid);

  String response = "{\"connected\":";
  unsigned long elapsed = millis();
  if (connectAddKnownNetwork(ssid.c_str(), passwd.c_str())) {
    response += "true";
  } else {
    result = 400;
    response += "false";
  }
  elapsed = millis() - elapsed;
  response += ",\"ssid\":\"";
  response += ssid;
  response += "\",\"elapsed\":";
  response += elapsed;

  response += "}";

  _server->send(result, "application/json", response);
}

/**
 * Scan for networks and return the networks
 */
void WiFiBase::_handleScan() {

  long start = millis();
  int networks = WiFi.scanNetworks();

  DEBUG4_VALUELN("WFB: /scan elapsed ", millis() - start);

  String response = "{\"count\":";
  response += networks;
  response += ",\"elapsed\":";
  response += millis() - start;
  response += ",\"networks\":[";

  if (networks) {
    for (int i = 0; i < networks; i++) {
      response += "[\"" + WiFi.SSID(i) + "\",";
      response += WiFi.RSSI(i);
      response += ",";
      response += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "\"\"":"\"*\"";
      response += "]";
      if (i != networks - 1) {
        response += ",";
      }
    }
  }

  WiFi.scanDelete();

  response += "]}";

  _server->send(200, "application/json", response);
}

/**
 * Perform repetitive tasks
 * TODO: This should be done via ticker
 */
void WiFiBase::checkServer() {

  /* Check for HTTP requests */
  _server->handleClient();
}

