#!/usr/bin/python3
import hashlib
import argparse
from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA
from Crypto.Cipher import PKCS1_v1_5 as PKCS
from Crypto.Signature import PKCS1_v1_5 as SIGN_PKCS

# test the functionalities of python Crypto on rsa sign and verify

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('private_key_path', help='The path to the file of private key')
    parser.add_argument('public_key_path', help='The path to the file of public key')
    parser.add_argument('message', help='The message to test')

    args = parser.parse_args()

    f = open(args.private_key_path, mode='rb')
    content = f.read()
    f.close()
    private_key = RSA.importKey(content)

    f = open(args.public_key_path, mode='rb')
    content = f.read()
    f.close()
    public_key = RSA.importKey(content)

    message = bytes(args.message, 'ascii')
    print('original text', message)
    print('signing')
    signature = SIGN_PKCS.new(private_key).sign(SHA256.new(message))
    print(signature.hex())

    print('verifying')

    if SIGN_PKCS.new(public_key).verify(SHA256.new(message), signature):
        print('verify succeed')
    else:
        print("verify failed")

