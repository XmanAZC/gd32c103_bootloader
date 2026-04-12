#!/bin/bash
if [[ "$1" == "-d" ]]; then
    export DEBUG=1
else
    unset DEBUG
fi

cmake -S . -B build
cmake --build build

if [[ "$1" == "-d" ]]; then
    exit 0
else
    version=$(git describe --tags --abbrev=0)
    if [[ -z "$version" ]]; then
        version="0.0.0"
    fi
    commit=$(git rev-parse --short HEAD)
    dirty=$(git status --porcelain)
    if [[ -n "$dirty" ]]; then
        commit="${commit}-dirty"
    fi
    ./build/app_padding_tool -l build/bootloader.bin -a build/appa.bin -b build/appb.bin -v "$version" -c "$commit" -o build/gd32c103ab_"$version"_"$commit".bin
fi
