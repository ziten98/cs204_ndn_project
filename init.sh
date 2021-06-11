#!/bin/bash

# This script will initiate the environment for the project.

sudo apt update
sudo apt install build-essential cmake ninja-build redis-server
pushd $PWD
mkdir libs
cd libs


# build spdlog
pushd $PWD
git clone https://github.com/gabime/spdlog.git
cd spdlog
mkdir build
cd build
cmake -G Ninja ../
make
popd


# build libuv
pushd $PWD
git clone https://github.com/libuv/libuv.git
cd libuv
mkdir build
cd build
cmake -G Ninja ../
make
sudo make install
popd

# build rapidjson
pushd $PWD
git clone https://github.com/Tencent/rapidjson.git
popd


# install lorem
pushd $PWD
git clone https://github.com/per9000/lorem.git
popd


# install python libraies
pip3 install PyCryptodome
sudo pip3 install PyCryptodome


# install hiredis
pushd $PWD
git clone https://github.com/redis/hiredis.git
cd hiredis
make
sudo make install
popd


# install redis-plus-plus
pushd $PWD
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build 
cd build
cmake -G Ninja -DREDIS_PLUS_PLUS_CXX_STANDARD=2a ../
ninja
sudo ninja install
popd


popd
./build.sh

sudo ./topo/folder_init.py


