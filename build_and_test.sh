#!/bin/bash
# Build, flash, and capture serial data for analysis

echo "=== Building project ==="
./build.sh 2>&1 | tail -20

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "=== Flashing device ==="
picotool reboot -f -u
sleep 3
picotool load build/tesla_bms_rp2350.elf
picotool reboot

echo ""
echo "=== Waiting for device to boot ==="
sleep 2

echo ""
echo "=== Capturing serial data for 10 seconds ==="
powershell.exe -ExecutionPolicy Bypass -File capture_serial.ps1 -Duration 10

echo ""
echo "=== Analysis ==="
if [ -f serial_capture.txt ]; then
    echo "Captured $(wc -l < serial_capture.txt) lines"
    echo ""
    
    # Check for key indicators
    echo "Looking for key patterns:"
    echo ""
    
    echo "Preamble detections:"
    grep -i "PREAMBLE FOUND" serial_capture.txt | head -5
    
    echo ""
    echo "Frame sync detections:"
    grep -i "FRAME SYNC FOUND\|ALIGNED" serial_capture.txt | head -5
    
    echo ""
    echo "Command identifications:"
    grep -E "WAKEUP|IDLE_WAKE|SNAPSHOT|READ_" serial_capture.txt | head -10
    
    echo ""
    echo "Decoded data samples:"
    grep -A 3 "DECODED isoSPI DATA" serial_capture.txt | head -20
    
    echo ""
    echo "Full capture saved to: serial_capture.txt"
else
    echo "No capture file found!"
fi

