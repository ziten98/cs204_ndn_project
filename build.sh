#!/bin/bash

# This script will build the project under build/ folder

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja ../src
time ninja