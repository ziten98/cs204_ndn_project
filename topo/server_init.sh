#!/bin/bash

# This script will setup the running environment on a server node

cd test
mkdir $1
cd $1
openssl genrsa -out private.pem 2048
openssl rsa -in private.pem -outform PEM -pubout -out public.pem
# mkdir ../C1
cp public.pem ../C1/$1-public.pem
mkdir files
cd files
../../../libs/lorem/lorem -c 1000 > 1k.txt
../../../libs/lorem/lorem -c 2000 > 2k.txt
../../../libs/lorem/lorem -c 4000 > 4k.txt
../../../libs/lorem/lorem -c 8000 > 8k.txt
../../../libs/lorem/lorem -c 16000 > 16k.txt
../../../libs/lorem/lorem -c 32000 > 32k.txt
../../../libs/lorem/lorem -c 64000 > 64k.txt
../../../libs/lorem/lorem -c 128000 > 128k.txt
../../../libs/lorem/lorem -c 256000 > 256k.txt
../../../libs/lorem/lorem -c 512000 > 512k.txt
../../../libs/lorem/lorem -c 1000000 > 1m.txt
../../../libs/lorem/lorem -c 2000000 > 2m.txt
../../../libs/lorem/lorem -c 4000000 > 4m.txt
../../../libs/lorem/lorem -c 8000000 > 8m.txt
../../../libs/lorem/lorem -c 16000000 > 16m.txt