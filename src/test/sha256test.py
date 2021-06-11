#!/usr/bin/python3
import hashlib
import argparse

# test the functionalities of hashlib on SHA256 digest

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('file', help='The file to calculate sha256 sum')

    args = parser.parse_args()

    f = open(args.file, mode='rb')
    content = f.read()
    f.close()

    sha256sum = hashlib.sha256(content).digest()
    print(sha256sum.hex())

