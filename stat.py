#!/usr/bin/python3

# get the statistics of a node
# you have to run it in the mininet console

import socket
import struct


def node_query():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 8888))
    s.send(struct.pack('<i', 1))

    # tells the router that the client is done sending
    s.shutdown(1)

    result = b''
    while True:
        received_data = s.recv(10000)
        if len(received_data) == 0:
            break
        else:
            result += received_data

    # print(result)

    packet_type = struct.unpack_from('<i', result)[0]
    if packet_type != 2:
        raise 'malformed response'

    timestamp, tag_length = struct.unpack_from('<qi', result, offset=4)
    tag = struct.unpack_from('<%ds' % tag_length, result, offset=16)[0].decode('ascii')
    print(tag)


if __name__ == '__main__':
    node_query()