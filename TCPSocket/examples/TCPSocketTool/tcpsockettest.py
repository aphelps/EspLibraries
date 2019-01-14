#!/usr/bin/python
#
# Send and receive data, can be used with TCPSocketTool
#
# Author: Adam Phelps
# License: MIT
# Copyright: 2018

import socket
import struct
import binascii

HEADER_FORMAT = "<IBBBHHB"
HEADER_LEN = 12

sock = socket.socket()

host = "192.168.1.37"  # ESP32 IP in local network
port = 80              # ESP32 Server Port

sock.connect((host, port))

id = 0
while True:
    message = struct.pack(HEADER_FORMAT + "BBBB",
                          0x54435053,  # START
                          1,           # VERSION
                          id,          # ID
                          4,           # Data Length
                          0x12,        # Source addr
                          128,         # Dest addr
                          0x56,        # Flags
                          0xde, 0xad, 0xbe, 0xef)
    print("Sending: %d:'%s'" %
          (len(message), binascii.hexlify(message)))
    sock.send(message)

    data = ""

    while len(data) < HEADER_LEN:
        data += sock.recv(1)

    print("Received %dB: '%s'" % (len(data), binascii.hexlify(data)))

    (start, version, id, datalen, sourceaddr, destaddr, flags) = \
        struct.unpack(HEADER_FORMAT, data)

    print("Header: start:0x%x version:%d id:%d datalen:%d source:%d dest:%d flags:%d" %
          (start, version, id, datalen, sourceaddr, destaddr, flags))

    data = ""
    while len(data) < datalen:
        data += sock.recv(1)

    print("Data %dB: '%s'" % (len(data), binascii.hexlify(data)))


sock.close()
