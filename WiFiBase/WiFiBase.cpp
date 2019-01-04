/*
 * Author: Adam Phelps
 * License: MIT
 * Copyright: 2018
 */

#include <Arduino.h>
#include <WiFi.h>

#ifndef DEBUG_LEVEL
  #define DEBUG_LEVEL DEBUG_HIGH
#endif
#include <Debug.h>

#include "WiFiBase.h"

/**
 * Create a default WifiBase object
 */
WiFiBase::WiFiBase(boolean useStored) {
  _background = true;
  _APSsid = NULL;
  _APPasswd = NULL;
  _running = false;
  _accessPointEnabled = false;
  _accessPointActive = false;

  _numKnownNetworks = 0;
  _allocatedKnownNetworks = 0;
  _knownNetworks = NULL;

  /*
   * If there was a previously connected WiFi, add it as the default known
   * network.
   */
  if (useStored && WiFi.SSID()) {
    DEBUG4_PRINTLN("WFB: adding default network");
    addKnownNetwork("", "");
  }

  DEBUG5_PRINTLN("WFB: Created");
}

WiFiBase::~WiFiBase() {
  for (int i = 0; i < _numKnownNetworks; i++) {
    free(_knownNetworks[i].ssid);
    free(_knownNetworks[i].passwd);
  }
}

/*******************************************************************************
 * Configuration functions
 */

bool WiFiBase::configBackground(bool background) {
  if (_running) {
    DEBUG_ERR("WFB: already running");
    return false;
  }
  _background = background;
  return true;
}

bool WiFiBase::configureAccessPoint(const char *ssid, const char *passwd) {
  if (_accessPointActive) {
    DEBUG_ERR("WFB: access point is active");
    return false;
  }
  _APSsid = ssid;
  _APPasswd = passwd;
  _accessPointEnabled = true;
  return true;
}

bool WiFiBase::disableAccessPoint() {
  if (_accessPointActive) {
    /* Shutdown the access point */
    if (!_shutdownAccessPoint()) {
      DEBUG_ERR("WFB: Failed to disable AP");
      return false;
    }
  }
  _accessPointEnabled = false;
  return true;
}

/**
 * Add a known network to the known network array, reallocating the array as
 * needed.
 *
 * @param ssid
 * @param passwd
 * @return
 */
bool WiFiBase::addKnownNetwork(const char *ssid, const char *passwd) {
  if (!_allocatedKnownNetworks) {
    _allocatedKnownNetworks = 2;
    _numKnownNetworks = 0;
    DEBUG3_VALUELN("WFB: allocate known ", _allocatedKnownNetworks);
    _knownNetworks = (struct network *)malloc(sizeof(struct network) * _allocatedKnownNetworks);
  } else {
    /* Check if the network is already listed */
    if (hasKnownNetwork(ssid)) {
      DEBUG4_VALUELN("WFB: re-added known ", ssid);
      return true;
    }

    /* Check if reallocation is necessary */
    if (_numKnownNetworks == _allocatedKnownNetworks) {
      if (_numKnownNetworks >= MAX_KNOWN_NETWORKS) {
        DEBUG_ERR("WFB: Hit maximum networks")
        return false;
      }

      /* Increase the size, allocate a new array, and copy from the old array */
      uint16_t newAlloc = _allocatedKnownNetworks * 2 ;
      DEBUG3_VALUELN("WFB: realloc known ", newAlloc);
      struct network *newNetworks = (struct network *)malloc(sizeof(struct network) * newAlloc);
      memcpy(newNetworks, _knownNetworks,
             sizeof(struct network) * _numKnownNetworks);
      free(_knownNetworks);

      /* Swap to the new copied array */
      _knownNetworks = newNetworks;
      _allocatedKnownNetworks = newAlloc;
    }
  }

  DEBUG4_VALUE("WFB: known ", _numKnownNetworks);

  _knownNetworks[_numKnownNetworks].ssid = strdup(ssid);
  _knownNetworks[_numKnownNetworks].passwd = strdup(passwd);

  DEBUG4_VALUE(" ", _knownNetworks[_numKnownNetworks].ssid);
  DEBUG4_VALUELN(" ", _knownNetworks[_numKnownNetworks].passwd);

  _numKnownNetworks++;

  return true;
}

/**
 * @return number of known networks
 */
int WiFiBase::numKnownNetworks() {
  return _numKnownNetworks;
}

/**
 * Check if a given ssid is included in the known networks list
 * @param ssid  Name of network to lookup
 * @return      Whether the network is known
 */
bool WiFiBase::hasKnownNetwork(const char *ssid) {
  for (int i = 0; i < _numKnownNetworks; i++) {
    if (strcmp(_knownNetworks[i].ssid, ssid) == 0) {
      return true;
    }
  }

  return false;
}

/*******************************************************************************
 * Operational functions
 */

/**
 *
 * @return
 */
bool WiFiBase::startup() {
  if (_background) {
    /* TODO: Set this to running in the background */
  }

  if (_connectToNetwork()) {

  }

  if (_accessPointEnabled) {

  }

  return false;
}

bool WiFiBase::_startupAccessPoint() {

  return false;
}

bool WiFiBase::_shutdownAccessPoint() {

  return false;
}

/**
 * Iterate over any known networks and connect to the first one possible.
 *
 * @return Whether this connected to a known network
 */
bool WiFiBase::_connectToNetwork() {

  return false;
}