#!/bin/bash
# Build script for Tesla BMS RP2350 project

set -e  # Exit on error

echo "========================================="
echo " Tesla BMS RP2350 Build Script"
echo "========================================="
echo ""

# Check if PICO_SDK_PATH is set
if [ -z "$PICO_SDK_PATH" ]; then
    echo "ERROR: PICO_SDK_PATH environment variable not set"
    echo ""
    echo "Please set PICO_SDK_PATH to your Pico SDK installation:"
    echo "  export PICO_SDK_PATH=/path/to/pico-sdk"
    echo ""
    echo "Or install Pico SDK:"
    echo "  git clone https://github.com/raspberrypi/pico-sdk.git"
    echo "  cd pico-sdk"
    echo "  git submodule update --init"
    echo "  export PICO_SDK_PATH=\$(pwd)"
    echo ""
    exit 1
fi

echo "Pico SDK Path: $PICO_SDK_PATH"
echo ""

# Create build directory
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Run CMake
echo "Running CMake configuration..."
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building project..."
ninja

echo ""
echo "========================================="
echo " Build Complete!"
echo "========================================="
echo ""
echo "Output files:"
ls -lh tesla_bms_rp2350.uf2 tesla_bms_rp2350.elf 2>/dev/null || true
echo ""
echo "To flash:"
echo "  1. Hold BOOTSEL button while connecting USB"
echo "  2. Copy build/tesla_bms_rp2350.uf2 to the mounted drive"
echo "  3. Device will reboot automatically"
echo ""

