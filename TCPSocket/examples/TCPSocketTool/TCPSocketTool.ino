/*
 * Example of a minimal TCPSocket client
 *
 * Author: Adam Phelps
 * License: MIT
 * Copyright: 2018
 */

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_HIGH
#endif
#include "Debug.h"

#include <TCPSocket.h>
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

#ifndef ADDRESS
  #define ADDRESS 128
#endif
#ifndef PORT
  #define PORT TCPSOCKET_PORT
#endif

#define DATA_SIZE 64
#define SEND_BUFFER_SIZE TCP_BUFFER_TOTAL(DATA_SIZE)
byte databuffer[SEND_BUFFER_SIZE];
byte *send_buffer;

WiFiBase *wfb;
TCPSocket tcpSocket;

void setup() {
  Serial.begin(115200);

  /* Use WiFiBase to connect to a network */
  wfb = new WiFiBase(true);
#ifdef USE_SSID
  wfb->addKnownNetwork(USE_SSID, USE_PASSWD);
#endif
  wfb->configureAccessPoint(CONFIG_SSID, CONFIG_PASSWD);
#ifdef CONFIG_PORTAL
  /* The the WiFiBase to generate an access point hosting a config portal */
  wfb->useConfigPortal(true);
#endif
  while (!wfb->startup()) {
    delay(100);
  }

  DEBUG1_VALUELN("Listening on port ", PORT);
  tcpSocket.init(ADDRESS, PORT);
  send_buffer = tcpSocket.initBuffer(databuffer, SEND_BUFFER_SIZE);
  tcpSocket.setup();

  DEBUG1_PRINTLN("*** TCPSocketTool initialized ***")
}

#define SEND_PERIOD 1000
unsigned long last_send_ms = 0;
byte count = 0;
boolean waiting;

void loop() {
  unsigned long now = millis();

  /* Wait until connected */
  if (!tcpSocket.connected()) {
    if (!waiting) {
      DEBUG1_PRINTLN("Waiting for connection")
    }
    waiting = true;
    delay(100);
    return;
  }
  waiting = false;

  if (now - SEND_PERIOD >= last_send_ms) {
    send_buffer[0] = 'T';
    send_buffer[1] = count++;
    DEBUG1_VALUELN("* Sending ", count);

    tcpSocket.sendMsgTo(SOCKET_ADDR_ANY, send_buffer, 2);

    last_send_ms = now;
  }

  unsigned int retlen;
  const byte *data = tcpSocket.getMsg(&retlen);
  if (data != NULL) {
    DEBUG1_VALUE("* Received data ", retlen);
    DEBUG1_PRINT(": ");
    print_hex_buffer((char *)data, retlen);
    DEBUG_PRINT_END();
  }

  delay(10);
}