#!/bin/bash

set -e

read -p "Cross compile for aarch64 (raspi)? (y/n): " answer

if [[ "$answer" == "y" || "$answer" == "Y" ]]; then
    CROSS_ARGS="--cross-file aarch64-cross.txt"
fi

if [ -d build ]; then
    rm -r build
    fi
meson setup --cross-file aarch64-cross.txt build
meson compile -C build
echo
echo "Demo program under: build/raw_i2c_read"
