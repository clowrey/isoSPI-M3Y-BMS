# Quick Start Guide - Tesla BMS RP2350A

## Prerequisites

### 1. Install Pico SDK

```bash
# Clone Pico SDK
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=$(pwd)
cd ..
```

### 2. Install Toolchain

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
```

**macOS:**
```bash
brew install cmake
brew install --cask gcc-arm-embedded
```

**Windows:**
- Install [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- Install [CMake](https://cmake.org/download/)
- Install [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)

## Building the Project

### Option 1: Using Build Script (Linux/macOS)

```bash
chmod +x build.sh
./build.sh
```

### Option 2: Manual Build

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Flashing the Firmware

1. **Enter BOOTSEL Mode:**
   - Hold down the BOOTSEL button on your RP2350 board
   - Connect USB cable
   - Release BOOTSEL button
   - The board should appear as a USB mass storage device

2. **Copy Firmware:**
   ```bash
   cp build/tesla_bms_rp2350.uf2 /path/to/RPI-RP2/
   ```
   
   Or simply drag-and-drop `tesla_bms_rp2350.uf2` to the mounted drive.

3. **Device Reboots:**
   - The device will automatically reboot after flashing
   - LED on GP25 should turn on

## Connecting to Serial Terminal

### Linux/macOS

```bash
# Find the device (usually /dev/ttyACM0)
ls /dev/tty*

# Connect with screen
screen /dev/ttyACM0 115200

# Or use minicom
minicom -D /dev/ttyACM0 -b 115200
```

### Windows

Use [PuTTY](https://www.putty.org/) or Arduino Serial Monitor:
- Port: COMx (check Device Manager)
- Baud Rate: 115200
- Data Bits: 8
- Parity: None
- Stop Bits: 1

## First Commands

Once connected to serial, try these commands:

```
help        # Show all available commands
status      # Display system status
ina228      # Show current sensor info
adc         # Show pack voltages
params      # List all parameters
```

## Expected Output

```
========================================
  Tesla Model 3 BMS Interface
  RP2350A + INA228 + Internal ADC
========================================

Initializing system...
GPIO: Initialized (LED on GP25)
UART0: Initialized at 921600 baud (TX=GP0, RX=GP1)
I2C0: Initialized at 400000 Hz (SDA=GP4, SCL=GP5)
PWM: Initialized at 10000 Hz (Pack=GP20, Precharge=GP21)
Parameters: Initialized (150 parameters)
BATMan: Initialized successfully
ADC Monitor: Initialized successfully
INA228: Initialized successfully at address 0x40
Coulomb Counter: Initialized

System ready - entering main loop
Type 'help' for available commands
```

## Troubleshooting

### Build Errors

**"PICO_SDK_PATH not set"**
```bash
export PICO_SDK_PATH=/path/to/pico-sdk
```

**"arm-none-eabi-gcc not found"**
- Ensure ARM toolchain is installed
- Add to PATH if necessary

### Flashing Issues

**Board not recognized in BOOTSEL mode**
- Try different USB cable (some are charge-only)
- Try different USB port
- Check if BOOTSEL button is working

### Serial Connection Issues

**No output on serial terminal**
- Wait 2-3 seconds after connection
- Try pressing Enter
- Check baud rate (should be 115200)
- Verify USB drivers are installed

### Runtime Issues

**INA228 not detected**
- Check I2C connections (GP4=SDA, GP5=SCL)
- Verify pull-up resistors (2.2kΩ)
- Check I2C address (default 0x40)

**ADC readings incorrect**
- Voltage dividers must be installed
- Check for proper grounding
- Verify no voltages exceed 3.3V

## Hardware Connections

### Minimum Setup (for testing without HV)

Connect only:
- USB cable (power and debug)
- Optional: I2C pull-ups for INA228 (2.2kΩ to 3.3V)

This allows testing of:
- Serial communication
- Command interface
- Parameter system
- PWM outputs

### Full Setup (for BMS operation)

Required connections:
1. **Tesla BMS SPI:** GP16-19 to BMS connector
2. **INA228 I2C:** GP4-5 with pull-ups
3. **Voltage Dividers:** GP26-29 with 400V→3V dividers
4. **ESPHome UART:** GP0-1 to ESP32-S3 display
5. **Contactors:** GP20-21 to contactor drivers

⚠️ **HIGH VOLTAGE WARNING:** Only connect to live battery pack if you are qualified and have proper safety equipment!

## Next Steps

1. **Test Basic Functions:**
   - Verify USB serial works
   - Check command interface
   - Test parameter system

2. **Test with INA228:**
   - Connect INA228 module
   - Run `ina228` command
   - Verify current readings

3. **Test ADC:**
   - Connect test voltages to dividers
   - Run `adc` command
   - Verify voltage scaling

4. **Test ESPHome:**
   - Connect UART to ESP32-S3
   - Monitor data stream
   - Verify parameter updates

5. **Integration Testing:**
   - Connect to Tesla BMS (when ready)
   - Test cell voltage reading
   - Test balancing control

## Support

- GitHub Issues: [Open an issue](https://github.com/your-repo/issues)
- Documentation: See `README_RP2350.md`
- Implementation Status: See `IMPLEMENTATION_STATUS.md`
- Conversion Plan: See `rp2350-ina228-conversion.plan.md`

## Safety Reminders

⚠️ **IMPORTANT:**
- This is development firmware
- Test thoroughly before production use
- High voltage can be lethal
- Use proper isolation and protection
- Follow all electrical safety protocols
- Ensure emergency shutdown procedures
- Never work on live circuits alone

## License

GNU General Public License v3.0

