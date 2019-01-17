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
import argparse


HEADER_FORMAT = "<IBBBBHH"
HEADER_LEN = 12

DEFAULT_IP = "192.168.4.1"
DEFAULT_PORT = 4081


def handle_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-a", "--address", dest="address",
                        help="IP address to connect to", default = DEFAULT_IP)

    parser.add_argument("-p", "--port", dest="port", type=int,
                        help="Port to connect to",
                        default=DEFAULT_PORT)

    parser.add_argument("-f", "--fragment", dest="fragment",
                        action="store_true",
                        help="Transmit header and data separately", default=False)

    return parser.parse_args()


options = handle_args()

print("Connecting to %s:%d" % (options.address, options.port))

sock = socket.socket()
sock.connect((options.address, options.port))

id = 0
while True:
    if options.fragment:
        message = struct.pack(HEADER_FORMAT,
                              0x54435053,  # START
                              1,           # VERSION
                              id,          # ID
                              4,           # Data Length
                              0x56,        # Flags
                              0x12,        # Source addr
                              128)         # Dest addr
        print("Sending header: %d:'%s'" %
              (len(message), binascii.hexlify(message)))
        sock.send(message)

        message = struct.pack("BBBB",
                              0xde, 0xad, 0xbe, 0xef)
        print("Sending data: %d:'%s'" %
              (len(message), binascii.hexlify(message)))
        sock.send(message)
    else:
        message = struct.pack(HEADER_FORMAT + "BBBB",
                              0x54435053,  # START
                              1,           # VERSION
                              id,          # ID
                              4,           # Data Length
                              0x56,        # Flags
                              0x12,        # Source addr
                              128,         # Dest addr
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
