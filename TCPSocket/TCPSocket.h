/*
 * Author: Adam Phelps
 * License: MIT
 * Copyright: 2018
 *
 * This class provides a Socket API wrapper for a TCP port.  (See
 * https://github.com/AMPWorks/ArduinoLibs/blob/master/Socket/Socket.h)
 *
 * This is for compatibility with existing code that uses the Socket API.
 */

#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <WiFiServer.h>
#include <WiFiClient.h>

#include "Socket.h"

#define TCPSOCKET_START (uint32_t)0x54435053 // "TCPS"
#define TCPSOCKET_VERSION 1
typedef struct __attribute__((__packed__)) {
  uint32_t      start;       // 4B
  byte          version;     // 1B
  byte          ID;          // 1B
  byte          length;      // 1B
  byte          flags;       // 1B
  socket_addr_t source;      // 2B
  socket_addr_t address;     // 2B
} tcp_socket_hdr_t;  // Total: 12B

typedef struct {
  tcp_socket_hdr_t hdr;
  byte             data[];
} tcp_socket_msg_t;

/* Calculate the total buffer size with a useable buffer of size x */
#define TCP_BUFFER_TOTAL(x) (uint8_t)(x + sizeof (tcp_socket_hdr_t))
#define TCP_DATA_LENGTH(x) (uint8_t)(x - sizeof (tcp_socket_hdr_t))

class TCPSocket : public Socket {

public:
  /* TCP specific functions */
  TCPSocket();
  ~TCPSocket();
  TCPSocket(socket_addr_t _address,
            uint16_t _port,
            byte _recvBufferSize = DEFAULT_RECEIVE_BUFFER);
  void init(socket_addr_t _address,
            uint16_t _port,
            byte _recvBufferSize = DEFAULT_RECEIVE_BUFFER);

  /*
   * Implement functions from Socket.h
   */
  void setup();
  boolean initialized();
  byte * initBuffer(byte * data, uint16_t data_size);

  void sendMsgTo(uint16_t address, const byte * data, const byte length);

  const byte *getMsg(unsigned int *retlen);
  const byte *getMsg(uint16_t address, unsigned int *retlen);

  byte getLength();
  void *headerFromData(const void *data);
  socket_addr_t sourceFromData(void *data);
  socket_addr_t destFromData(void *data);

  bool connected();

private:
  WiFiServer *tcpServer;
  WiFiClient tcpClient;
  byte currentMsgID;

  static const byte DEFAULT_RECEIVE_BUFFER = TCP_BUFFER_TOTAL(64);
  byte recvBufferSize;
  uint8_t *recvBuffer;
  byte lastRecvSize;

  bool checkClient();
  bool validateHeader(tcp_socket_hdr_t *hdr);
};

#endif // TCPSOCKET_H