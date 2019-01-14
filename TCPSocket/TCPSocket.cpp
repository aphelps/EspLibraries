/*
 * Author: Adam Phelps
 * License: MIT
 * Copyright: 2018
 */

#include <WiFi.h>
#include <WiFiServer.h>

#ifdef DEBUG_LEVEL_TCPSOCKET
  #define DEBUG_LEVEL DEBUG_LEVEL_TCPSOCKET
#endif
#ifndef DEBUG_LEVEL
  #define DEBUG_LEVEL DEBUG_HIGH
#endif
#include <Debug.h>

#include <Socket.h>
#include "TCPSocket.h"

TCPSocket::TCPSocket() {
  tcpServer = nullptr;
  tcpClient = WiFiClient();
}

TCPSocket::~TCPSocket() {
  tcpClient.stop();
  tcpServer->stop();
  free(recvBuffer);
}

TCPSocket::TCPSocket(socket_addr_t _address,
                     uint16_t _port,
                     byte _recvBufferSize) {
  init(_address, _port, _recvBufferSize);
}

/**
 * Allocate the receive buffer and setup the WiFi server
 */
void TCPSocket::init(socket_addr_t _address,
                     uint16_t _port,
                     byte _recvBufferSize) {
  sourceAddress = _address;
  currentMsgID = 0;
  lastRecvSize = 0;

  recvBufferSize = _recvBufferSize;
  recvBuffer = (uint8_t *)malloc(recvBufferSize);
  partialRecv = false;

  DEBUG3_VALUE("TCPS: Listinging on ", WiFi.localIP().toString());
  DEBUG3_VALUELN(":", _port);
  tcpServer = new WiFiServer(_port, 1 /* max clients */);
  tcpClient = WiFiClient();

}

void TCPSocket::setup() {
  tcpServer->begin();
}

boolean TCPSocket::initialized() {
  return (tcpServer != nullptr);
}

/**
 * Setup the send buffer, which takes an input buffer and sets the buffer
 * for data to allow for the initial packet header.
 */
byte *TCPSocket::initBuffer(byte *data, uint16_t data_size) {
  send_data_size = data_size - sizeof (tcp_socket_hdr_t);
  send_buffer = data + sizeof (tcp_socket_hdr_t);
  return send_buffer;
}

/**
 * Check if the current client is connected and if not see if there is a
 * connected one available;
 *
 * @return if a connected client is present
 */
bool TCPSocket::checkClient() {
  if (tcpClient) {
    return true;
  }
  tcpClient = tcpServer->available();
  if (tcpClient) {
    DEBUG3_VALUELN("TCPS: Connection from ", tcpClient.remoteIP().toString());
  }
  return tcpClient;
}

/**
 * Verify that the packet header appears to be valid.
 */
bool TCPSocket::validateHeader(tcp_socket_hdr_t *hdr) {
  if (hdr->start != TCPSOCKET_START) {
    DEBUG3_HEXVALLN("TCPS: bad start ", hdr->start);
    return false;
  }
  if (hdr->version != TCPSOCKET_VERSION) {
    DEBUG3_VALUELN("TCPS: bad version ", hdr->version);
    return false;
  }

  return true;
}

/**
 * Transmit a message
 */
void TCPSocket::sendMsgTo(socket_addr_t address,
                          const byte *data,
                          const byte datalength)
{
  if (!checkClient()) {
    DEBUG3_PRINTLN("TCPS: send without connection");
    return;
  }

  tcp_socket_msg_t *msg = (tcp_socket_msg_t *)headerFromData(data);

  unsigned int msg_len = sizeof (tcp_socket_hdr_t) + datalength;

  msg->hdr.start = TCPSOCKET_START;
  msg->hdr.version = TCPSOCKET_VERSION;
  msg->hdr.ID = currentMsgID++;
  msg->hdr.length = datalength;
  msg->hdr.source = sourceAddress;
  msg->hdr.address = address;
  msg->hdr.flags = 0;

  size_t result = tcpClient.write((uint8_t *)msg, msg_len);
  if (result != msg_len) {
    DEBUG3_VALUE("TCPS: under sent ", result);
    DEBUG3_VALUELN("<", msg_len);
  }
}

const byte *TCPSocket::getMsg(unsigned int *retlen) {
  return getMsg(sourceAddress, retlen);
}

/**
 * Read data from a connected TCP client
 *
 * @param address Socket address (not IP) to accept data for
 * @param retlen  Data size returned
 * @return        Pointer to the data portion of the message
 */
const byte *TCPSocket::getMsg(socket_addr_t address, unsigned int *retlen) {
  int result;
  tcp_socket_msg_t *msg = (tcp_socket_msg_t *)recvBuffer;
  tcp_socket_hdr_t *hdr = &(msg->hdr);

  uint32_t start;
  uint8_t *startbytes;

  if (!checkClient()) {
    /* No currently connected client */
    goto NO_RESULT;
  }

  if (partialRecv) {
    /*
     * If the previous getMsg() call got a header but there was insufficient
     * data for the complete message then restart from the existing header.
     */
    DEBUG5_PRINTLN("TCPS: continuing recv");
    goto HAVE_HEADER;
  }

START_VALUE:
  /* Read available data until we run out or find a start value */
  if (tcpClient.available() < sizeof (tcp_socket_hdr_t)) {
    goto NO_RESULT;
  }

  DEBUG5_VALUELN("TCPS: Have avail: ", tcpClient.available());

  /* Read bytes until the full start value is found */
  start = TCPSOCKET_START;
  startbytes = (uint8_t *)&start;
  for (byte i = 0; i < sizeof (hdr->start); i++) {
    uint8_t *val = (uint8_t *)&(hdr->start) + i;
    *val = (uint8_t)tcpClient.read();
    if (*val != startbytes[i]) {
      /* Didn't get the right start byte, restart */
      DEBUG5_HEXVAL("TCPS: Not start byte ", *val);
      DEBUG5_HEXVALLN("!", startbytes[i]);
      goto START_VALUE;
    }
  }

  if (hdr->start != TCPSOCKET_START) {
    DEBUG_ERR("TCPS: Should not reach");
    goto ERROR_OUT;
  }

  /* A start value has been read, get the remainder of the header */
  result = tcpClient.read((uint8_t *)hdr + sizeof (hdr->start),
                          sizeof (tcp_socket_hdr_t) - sizeof (hdr->start));
  if (result + sizeof (hdr->start) < sizeof (tcp_socket_hdr_t)) {
    DEBUG4_VALUELN("TCPS: recv < hdr sz:", result);
    goto ERROR_OUT;
  }

HAVE_HEADER:
  /*
   * The full header has been received, validate it before receiving the message
   * data.
   */
  DEBUG5_COMMAND(
          printHeader(hdr);
  );

  if (!validateHeader(hdr)) {
    DEBUG4_PRINTLN("TCPS: Recv invalid hdr");
    goto ERROR_OUT;
  }

  if (hdr->length > recvBufferSize - sizeof (tcp_socket_hdr_t)) {
    DEBUG4_VALUELN("TCPS: hdr.len > buf sz ", hdr->length);
    goto ERROR_OUT;
  }

  if (tcpClient.available() < hdr->length) {
    /*
     * The header was received but there is not yet enough data available
     * to read the entire packet.  The header is stored in the buffer and will
     * be reused for the next getMsg() call.
     */
    partialRecv = true;
    DEBUG5_VALUE("TCPS: Incomplete ", tcpClient.available());
    DEBUG5_VALUELN("<", hdr->length);
    goto NO_RESULT;
  }

  partialRecv = false;

  /* Read the message data */
  result = tcpClient.read(msg->data, hdr->length);
  if (result != hdr->length) {
    DEBUG4_VALUE("TCPS: recv < hdr.len", result);
    DEBUG4_VALUELN("<", hdr->length);
    goto ERROR_OUT;
  }

  DEBUG5_VALUE("TCPS: data len=", result);
  DEBUG5_COMMAND(
          print_hex_buffer((const char *)msg->data, hdr->length);
  );
  DEBUG_ENDLN();

  if (SOCKET_ADDRESS_MATCH(address, hdr->address)) {
    DEBUG5_PRINTLN("TCPS: getmsg good");
    *retlen = lastRecvSize = hdr->length;
    return msg->data;
  }

  DEBUG5_VALUE("TCPS: address mismatch: ", address);
  DEBUG5_VALUELN("!=", hdr->address);

ERROR_OUT:

NO_RESULT:
  *retlen = 0;
  return nullptr;
}

byte TCPSocket::getLength() {
  return lastRecvSize;
}

void *TCPSocket::headerFromData(const void *data) {
  return ((tcp_socket_hdr_t *)((uint8_t *)data - sizeof (tcp_socket_hdr_t)));
}

socket_addr_t TCPSocket::sourceFromData(void *data) {
  return ((tcp_socket_hdr_t *)headerFromData(data))->source;
}

socket_addr_t TCPSocket::destFromData(void *data) {
  return ((tcp_socket_hdr_t *)headerFromData(data))->address;
}

bool TCPSocket::connected() {
  return checkClient();
}

void TCPSocket::printHeader(tcp_socket_hdr_t *hdr, bool dump) {
  DEBUG3_HEXVAL("TCPS: hdr start:", hdr->start);
  DEBUG3_VALUE(" ver:", hdr->version);
  DEBUG3_VALUE(" id:", hdr->ID);
  DEBUG3_VALUE(" len:", hdr->length);
  DEBUG3_HEXVAL(" flags:", hdr->flags);
  DEBUG3_VALUE(" source:", hdr->source);
  DEBUG3_VALUELN(" dest:", hdr->address);

  if (dump) {
    DEBUG5_PRINT(" hdr=");
    DEBUG5_COMMAND(
            print_hex_buffer((const char *) hdr, sizeof(tcp_socket_hdr_t))
    );
    DEBUG_ENDLN();
  }

}