#!/bin/bash
mkdir -p build
cd build
cmake ..
make -j$(nproc) && cd .. && g++ app_padding.cpp -o app_padding &&\
./app_padding -l build/bootloader.bin -a build/appa.bin -b build/appb.bin -o build/firmware_full.bin && rm app_padding && echo "Build success: build/firmware_full.bin"