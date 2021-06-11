#!/usr/bin/python3
import socket
import struct
import json
import time
import argparse
import threading
import hashlib
from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA
from Crypto.Cipher import PKCS1_v1_5 as PKCS
from Crypto.Signature import PKCS1_v1_5 as SIGN_PKCS


def receive_all(s):
    '''Receive all data from server until it closes. '''

    result = b''
    while True:
        received_data = s.recv(10000)
        if len(received_data) == 0:
            break
        else:
            result += received_data

    return result


def parse_packet(data_packet):
    '''Parse the packet. '''

    packet_type = struct.unpack_from('<i', data_packet)[0]
    if packet_type != 4:
        raise 'malformed response'

    name_length = struct.unpack_from('<i', data_packet, offset=4)[0]
    (name, content_length) = struct.unpack_from('<%dsi' % name_length, data_packet, offset=8)
    name = name.decode('ascii')

    (content, sha256sum, signature_length) = struct.unpack_from('<%ds32si' % content_length, data_packet, offset=8 + name_length + 4)
    (signature, timestamp, freshness, tag_length) = struct.unpack_from('<%dsqqi' % signature_length, data_packet, offset = 8 + name_length + 4 + content_length + 36)
    tag = struct.unpack_from('<%ds' % tag_length, data_packet, offset=8 + name_length + 4 + content_length + 36 + signature_length + 20)[0].decode('ascii')

    if verify_content:
        sha256sum_local = hashlib.sha256(content).digest()
        # print(sha256sum)
        # print(sha256sum_local)

        if sha256sum != sha256sum_local:
            print('content verification failed')
        else:
            print('content verification succeed')

    if verify_signature:
        if SIGN_PKCS.new(public_key).verify(SHA256.new(content), signature):
            print('signature verification succeed')
        else:
            print("signature verification failed")

    return (name, content, freshness, tag)


def make_request(request_path, request_tag):
    ''' Make a request by sending an interest packet to the router and get response.
        Once the responce is got, parse it and return the fields.'''

    interst_packet = struct.pack('<ii%dsqqi%ds' % (len(request_path), len(request_tag)), 3, len(request_path), bytes(request_path, 'ascii'), int(time.time() * 1000), 60000, len(request_tag), bytes(request_tag, 'ascii'))
    # print(interst_packet)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((router_ip, 8888))
    s.send(interst_packet)
    # tells the node that the client is done sending
    s.shutdown(1)
    data_packet = receive_all(s)
    s.close()

    return parse_packet(data_packet)


content_list = 0
thread_list = []

def data_handler(request_path, request_tag, index):
    '''A handler to handle data from the router. '''
    global content_list

    name, content, freshness, tag = make_request(request_path, request_tag)
    content_list[index] = content



def get_file(server_name, file_name, use_parallel):
    '''Get a file from the server. '''

    global content_list, thread_list

    request_path = '/%s/%s/chunks_count' % (server_name, file_name)
    request_tag = 'request from %s\n' % node_name

    name, content, freshness, tag = make_request(request_path, request_tag)

    if len(content) != 4:
        raise 'malformed response'

    chunks_count = struct.unpack('<i', content)[0]
    
    print('chunks count: %d' % chunks_count)
    print('tags: \n%s' % tag)

    if not use_parallel:
        total_content = b''
        for i in range(chunks_count):
            request_path = '/%s/%s/chunk/%d' % (server_name, file_name, i)
            request_tag = 'request from %s\n' % node_name

            name, content, freshness, tag = make_request(request_path, request_tag)
            total_content += content
        
        return total_content

    else:
        content_list = [0] * chunks_count
        for i in range(chunks_count):
            t = threading.Thread(target=data_handler, args=('/%s/%s/chunk/%d' %(server_name, file_name, i), request_tag, i))
            thread_list.append(t)
            t.start()

        for t in thread_list:
            t.join()

        total_content = b''
        for i in range(chunks_count):
            total_content += content_list[i]

        return total_content


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('server', help='The server which has the requested file, it will be the root name in the path.')
    parser.add_argument('file', help='The file on the server, it will be the second part in the path')
    parser.add_argument('--save', dest='local_file', help='The local path to save the file')
    parser.add_argument('--print', action='store_true', help='Do not save file, just print it to the console, this works only for text file.')
    parser.add_argument('--parallel', action='store_true', help='Request chunks in parallel.')
    parser.add_argument('--verify-content', action='store_true', help='verify the sha256 sum of the content')
    parser.add_argument('--verify-signature', action='store_true', help='verify the signature of the data packet')

    args = parser.parse_args()

    f = open('./topo.json')
    topo = json.load(f)
    node_name = topo['name']
    router_name = topo['neighbors'][0]['name']
    router_ip = topo['neighbors'][0]['ip']
    f.close()

    print('get /%s/%s' % (args.server, args.file))

    if args.verify_content:
        verify_content = True
    else:
        verify_content = False

    if args.verify_signature:
        verify_signature = True
        f = open('%s-public.pem' % args.server, mode='rb')
        content = f.read()
        f.close()
        public_key = RSA.importKey(content)
    else:
        verify_signature = False

    start = time.time()
    content = get_file(args.server, args.file, args.parallel)
    end = time.time()

    print("time used: %s ms" % format((end-start) * 1000, '.2f'))

    if args.print:
        print("file content:")
        print(content.decode('ascii'))
    else:
        f = open(args.local_file, 'wb')
        f.write(content)
        f.close()
        print("file successfully write to %s" % args.local_file)
    