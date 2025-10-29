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

# Attempt automatic flash with picotool (using same method as VS Code Pico extension)
echo "Attempting automatic flash..."
if command -v picotool &> /dev/null; then
    if [ -f "tesla_bms_rp2350.elf" ]; then
        echo "Flashing with picotool..."
        # The -x flag automatically:
        #   1. Tracks device and reboots to BOOTSEL mode if needed
        #   2. Loads the firmware into flash
        #   3. Reboots device to start application
        if picotool load -x tesla_bms_rp2350.elf 2>&1; then
            echo ""
            echo "✓ Flash successful! Device rebooted and running."
        else
            echo ""
            echo "✗ Flash failed or no device detected"
            echo ""
            echo "Manual flash instructions:"
            echo "  1. Hold BOOTSEL button while connecting USB"
            echo "  2. Copy build/tesla_bms_rp2350.uf2 to the mounted drive"
            echo "  3. Device will reboot automatically"
        fi
    else
        echo "✗ tesla_bms_rp2350.elf not found!"
    fi
else
    echo "picotool not found - skipping automatic flash"
    echo ""
    echo "Manual flash instructions:"
    echo "  1. Hold BOOTSEL button while connecting USB"
    echo "  2. Copy build/tesla_bms_rp2350.uf2 to the mounted drive"
    echo "  3. Device will reboot automatically"
fi
echo ""

