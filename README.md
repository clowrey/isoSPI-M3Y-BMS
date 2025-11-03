# Tesla Model 3 BMS Interface - RP2350A Port

This project provides a comprehensive interface for the Tesla Model 3 Battery Management System (BMS), featuring RP2350A microcontroller with advanced monitoring, CAN bus integration, and ESPHome touchscreen display.

**Batman BMS code** originally created by Damien and Tom: https://github.com/damienmaguire/Tesla-M3-Bms-Software
**Pico PIO Code for isoSPI** plus all the isoSPI logic created by jonny5532

## Project Overview

This is the RP2350A port of the Tesla Model 3 BMS Interface, replacing ESP32 with RP2350A MCU and AS8510 with INA228 for current sensing.

### Main Components

1. **RP2350A Firmware** - Core BMS interface with advanced features
2. **ESPHome Interface** - Modern touchscreen display with Home Assistant integration

## Features

### Core BMS Interface (RP2350A)
- **Parameter API**: Read/write all 108+ BMS parameters via serial commands
- **Dual Serial Interface**: USB debug and UART for ESPHome display
- **Real-time Monitoring**: Cell voltages, temperatures, balance status
- **INA228 Current Sensing**: High-precision current and power monitoring
- **CAN Bus**: 500kbps broadcast of pack status (via can2040)
- **isoSPI Interface**: PIO-based master and passive snooper modes
- **Coulomb Counter**: Accurate SOC tracking with INA228 integration
- **ADC Pack Voltage**: 4-channel monitoring (Pack Neg/Pos, Link Neg/Pos)
- **Contactor Control**: 4-channel PWM control for safe sequencing
- **Unified BMB Testing**: Automatic interface switching between Batman and isoSPI

### ESPHome Display Interface
- **Touchscreen Display**: 480x320 QSPI LCD with LVGL interface
- **Real-time Visualization**: Live BMS data with modern UI
- **Cell Voltage Graph**: Visual bar graph of all 108 cells with color-coded status
- **Home Assistant Integration**: All parameters as HA entities
- **Touch Controls**: Balance on/off buttons directly on display
- **OTA Updates**: Over-the-air firmware updates

## Example Output
![image](https://github.com/user-attachments/assets/57a223b0-6f92-4a42-a34e-47e2fbd58b88)

```cpp
=== Cell Voltage Information ===
Total Cells Present: 23
Max Cell Voltage: 3.792V (Cell 13)
Min Cell Voltage: 3.770V (Cell 4)
Voltage Delta: 0.022V
Cells Balancing: 0

Individual Cell Voltages:
Cell 1: 3.789V    Cell 9: 3.790V     Cell 17: 3.791V
Cell 2: 3.789V    Cell 10: 3.790V    Cell 18: 3.790V
Cell 3: 3.790V    Cell 11: 3.790V    Cell 19: 3.792V
Cell 4: 3.770V    Cell 12: 3.791V    Cell 20: 3.791V
Cell 5: 3.790V    Cell 13: 3.792V    Cell 21: 3.791V
Cell 6: 3.789V    Cell 14: 3.792V    Cell 22: 3.787V
Cell 7: 3.790V    Cell 15: 3.791V    Cell 23: 3.790V
Cell 8: 3.789V    Cell 16: 3.791V
==============================

=== System Information ===
Total Pack Voltage: 87.10V
Average Cell Voltage: 3.787V
Temperature Range: 25.0°C - 29.0°C
Balance Status: OFF
==============================
```

## Hardware Requirements

### For RP2350A Implementation
- **MCU:** RP2350A (custom board)
- **Current Sensor:** Texas Instruments INA228 (I2C, 25.296µΩ shunt)
- **Pack Voltage Sensing:** Internal 12-bit ADC with voltage dividers (400V → 3.0V)
- **BMS Interface:** Tesla Model 3 Battery Management Boards (SPI/isoSPI)
- **CAN Transceiver:** MCP2551, TJA1050, or similar (for CAN bus)
- **Display Interface:** UART connection to ESPHome display

### For ESPHome Display Interface
- **ESP32-S3 Development Board** with PSRAM (16MB flash)
- **JC4832W535 QSPI Display** (480x320 with touch)
- **UART Connection** to RP2350A board (921600 baud)

## Project Structure

```
isoSPI-M3Y-BMS/
├── CMakeLists.txt                 # Pico SDK build configuration
├── pico_sdk_import.cmake          # Pico SDK import
├── can2040/                       # CAN bus library
│   └── src/
│       ├── can2040.c              # can2040 implementation
│       └── can2040.h              # can2040 header
├── src/
│   ├── include/                   # Header files
│   │   ├── pin_config.h           # Pin assignments for RP2350A
│   │   ├── batman.h               # BMS interface
│   │   ├── param.h                # Parameter system
│   │   ├── ina228.h               # INA228 driver
│   │   ├── adc_monitor.h          # ADC voltage monitor
│   │   ├── coulomb_counter.h      # Coulomb counting
│   │   ├── can_interface.h        # CAN bus interface
│   │   ├── isospi_interface.h     # isoSPI interface manager
│   │   ├── isospi_master.h        # isoSPI master (PIO)
│   │   ├── isosnoop.h             # isoSPI bus snooper
│   │   └── bmb_test.h             # Unified BMB test interface
│   ├── main.c                     # Main application loop
│   ├── batman.c                   # BMS interface implementation
│   ├── param.c                    # Parameter management
│   ├── ina228.c                   # INA228 driver
│   ├── adc_monitor.c              # ADC implementation
│   ├── coulomb_counter.c          # Coulomb counter
│   ├── can_interface.c            # CAN bus implementation
│   ├── isospi_interface.c         # isoSPI interface manager
│   ├── isospi_master.c            # isoSPI master implementation
│   ├── isospi_master.pio          # PIO code for isoSPI TX/RX
│   ├── isosnoop.c                 # isoSPI snooper implementation
│   ├── isosnoop.pio               # PIO code for passive snooping
│   └── bmb_test.c                 # Unified BMB test implementation
├── esphome-interface/             # ESPHome touchscreen interface
│   ├── tesla_bms_display.yaml     # Main ESPHome configuration
│   ├── cell_voltage_sensors.yaml  # Individual cell sensors
│   ├── external_components/       # Custom components
│   │   └── tesla_bms_uart/        # BMS UART parser component
│   └── README.md                  # ESPHome setup guide
├── context/                       # Original Arduino reference code
├── PARAMETER_API.md               # Complete parameter API documentation
├── CAN_INTEGRATION_REPORT.md      # CAN bus integration details
├── CAN_MESSAGE_FORMAT.md          # CAN message specifications
└── README_RP2350.md               # Additional RP2350A documentation
```

## Getting Started

### RP2350A Setup

1. **Install Pico SDK**:
   ```bash
   git clone https://github.com/raspberrypi/pico-sdk.git
   cd pico-sdk
   git submodule update --init
   export PICO_SDK_PATH=$(pwd)
   ```

2. **Install toolchain**:
   ```bash
   # For Ubuntu/Debian:
   sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
   
   # For macOS:
   brew install cmake
   brew install --cask gcc-arm-embedded
   ```

3. **Clone and build**:
   ```bash
   git clone <repository-url>
   cd isoSPI-M3Y-BMS
   mkdir build
   cd build
   cmake ..
   make
   ```

4. **Flash to RP2350A**:
   - Hold BOOTSEL button while connecting USB
   - Copy `tesla_bms_rp2350.uf2` to the mounted drive
   - Device will reboot automatically

5. **Monitor output**:
   - Connect via USB serial (115200 baud)
   - Use any serial terminal (screen, minicom, PuTTY)

### ESPHome Display Setup

1. **Install ESPHome**:
   ```bash
   pip install esphome
   ```

2. **Configure and flash**:
   ```bash
   cd esphome-interface
   # Edit secrets.yaml with your WiFi credentials
   esphome run tesla_bms_display.yaml
   ```

See `esphome-interface/README.md` for detailed setup instructions.

## API Usage

### Serial Commands

Connect via USB serial (115200 baud) and use these commands:

```bash
# System Status
help                       # Show available commands
status                     # Show system status
params                     # List all parameters

# Parameter API
param get u1               # Get Cell 1 voltage
param get balance          # Get balance status
param get CellsPresent     # Get number of cells
param set balance 1        # Enable balancing
param set balance 0        # Disable balancing

# Component Status
ina228                     # Show INA228 current sensor status
adc                        # Show ADC pack voltages
coulomb                    # Show coulomb counter status
batman                     # Show BATMan BMS interface status
can                        # Show CAN bus statistics

# Contactor Control
contactors on/off          # Enable/disable all contactors (sequenced)
link_neg on/off            # Enable/disable Link Negative contactor
link_pos on/off            # Enable/disable Link Positive contactor
fc_pos on/off              # Enable/disable FC Positive (precharge) contactor
fc_neg on/off              # Enable/disable FC Negative contactor

# isoSPI Interface
isospi init                # Initialize isoSPI interface
isospi enable              # Switch to isoSPI master (disable Batman)
batman enable              # Switch to Batman (disable isoSPI)
isospi test                # Run isoSPI test pattern
isospi snoop               # Print captured bus traffic
isospi status              # Show isoSPI interface status

# BMB Testing
bmb test                   # Run BMB test once (uses active interface)
bmb continuous on          # Enable continuous BMB testing (every 2 seconds)
bmb continuous off         # Disable continuous BMB testing

# Balance Control
balance on/off             # Enable/disable cell balancing
```

### Dual Serial Interface

- **USB Serial**: Full system logs, debug output, and complete API access
- **UART Serial** (GP0/GP1): ESPHome display communication (921600 baud)

### Available Parameters

#### System Parameters
- `numbmbs`, `LoopCnt`, `LoopState`, `CellsPresent`, `CellsBalancing`
- `BalanceCellList` - Comma-separated list of exact cell numbers being balanced

#### Cell Voltages
- `u1` through `u108` - Individual cell voltages (mV)
- `umax`, `umin`, `deltaV`, `uavg` - Voltage statistics
- `CellMax`, `CellMin` - Cell numbers with max/min voltages

#### Temperature & Control
- `Chipt0`, `TempMax`, `TempMin` - Temperature readings
- `balance` - Balance control (0=off, 1=on)

See `PARAMETER_API.md` for complete parameter documentation.

## Pin Configuration

See `src/include/pin_config.h` for complete pin assignments:

- **Tesla BMS SPI:** GP16 (MISO), GP19 (MOSI), GP18 (SCK), GP17 (CS), GP22 (Enable)
- **INA228 I2C:** GP4 (SDA), GP5 (SCL), GP6 (Alert)
- **Pack Voltage ADC:** GP26-29 (4 channels: Pack Neg, Pack Pos, Link Neg, Link Pos)
- **ESPHome UART:** GP0 (TX), GP1 (RX)
- **Contactor PWM:** GP20 (Link Pos), GP21 (Link Neg), GP23 (FC Pos), GP24 (FC Neg)
- **CAN Bus:** GP2 (RX), GP3 (TX)
- **isoSPI PIO:** GP7-11 (TX/RX differential pairs + sampling)

## New Features

### CAN Bus Interface

The system includes CAN bus support via the `can2040` library:

- **Bitrate:** 500 kbps
- **PIO-based:** Uses PIO0 for CAN protocol implementation
- **Message IDs:** Configurable standard or extended IDs
- **Broadcasting:** Automatic 10Hz broadcast of pack status
- **Statistics:** Track RX/TX message counts and errors

CAN messages broadcast pack voltage, current, power, SOC, and cell voltages.

See `CAN_INTEGRATION_REPORT.md` and `CAN_MESSAGE_FORMAT.md` for details.

### isoSPI Interface

The project includes a PIO-based isoSPI interface:

- **Master Mode:** Transmit and receive differential Manchester-encoded data
- **Snooper Mode:** Passive monitoring of existing isoSPI bus traffic
- **PIO Implementation:** Uses PIO1 for high-speed bit-banging
- **Switchable:** Can switch between Batman SPI and isoSPI PIO at runtime
- **Debug Output:** Capture and decode bus traffic

**isoSPI Workflow:**
1. `isospi init` - Initialize the PIO state machines
2. `isospi enable` - Switch to isoSPI master (disables Batman)
3. `bmb test` - Run test to communicate with BMBs
4. `isospi snoop` - Display captured bus traffic
5. `batman enable` - Switch back to Batman SPI (disables isoSPI)

### Unified BMB Testing

The system includes a unified BMB test routine that automatically works with whichever interface (Batman or isoSPI) is currently active:

- **Single Test:** `bmb test` - Run one test cycle immediately
- **Continuous Mode:** `bmb continuous on` - Run tests every 2 seconds automatically
- **Stop Continuous:** `bmb continuous off` - Stop automatic testing

The test routine automatically detects which interface is active and uses the appropriate protocol.

## Integration Options

### 1. Direct Serial Connection
Connect via USB serial for direct parameter access and system control.

### 2. ESPHome + Home Assistant
Full home automation integration with:
- Real-time dashboards
- Alerting and automation
- Historical data logging
- Remote control capabilities
- Touch screen display interface

### 3. CAN Bus Integration
Broadcast BMS data on CAN bus for integration with:
- Vehicle ECUs
- Chargers
- Inverters
- Data loggers

### 4. Custom Applications
Use the parameter API to build custom monitoring solutions.

## Documentation

- **[Parameter API Guide](PARAMETER_API.md)** - Complete API reference
- **[CAN Integration Report](CAN_INTEGRATION_REPORT.md)** - CAN bus implementation details
- **[CAN Message Format](CAN_MESSAGE_FORMAT.md)** - CAN message specifications
- **[ESPHome Setup](esphome-interface/README.md)** - Display interface guide
- **[RP2350A Details](README_RP2350.md)** - Additional RP2350A-specific documentation

## Development

### Building
```bash
# RP2350A Firmware
mkdir build
cd build
cmake ..
make

# ESPHome Display
cd esphome-interface
esphome compile tesla_bms_display.yaml
```

### Testing
- Use logic analyzer captures in `scope-capture/` directory for debugging
- Monitor USB serial for comprehensive system analysis
- Test individual components using serial commands
- Use `bmb continuous on` for extended testing
- Test with actual Tesla BMS hardware for validation

### Troubleshooting

#### INA228 Not Detected
- Check I2C connections (GP4=SDA, GP5=SCL)
- Verify pull-up resistors (2.2kΩ recommended)
- Run `ina228` command for diagnostics

#### ADC Reading Issues
- Verify voltage dividers (400V → 3.0V)
- Check TVS diodes for protection
- Run `adc` command to check readings

#### BMS Communication Failed
- Verify SPI connections (GP16-19)
- Check SPI clock speed (1MHz)
- Run `batman` command for status
- Try switching to isoSPI: `isospi init` then `isospi enable`

#### CAN Bus Issues
- Check CAN transceiver wiring (GP2=RX, GP3=TX)
- Verify bitrate matches other devices (500kbps)
- Run `can` command to check statistics
- Ensure proper termination resistors (120Ω)

#### isoSPI Issues
- Check pin connections (GP7-11)
- Run `isospi status` to check initialization
- Use `isospi snoop` to monitor bus traffic
- Try `isospi test` to verify master functionality

## Implementation Status

### ✅ Completed (Phases 1-2)
- [x] Pico SDK project structure and CMake configuration
- [x] Pin configuration and hardware interfaces
- [x] INA228 I2C driver (complete with alerts)
- [x] Internal ADC driver for pack voltages (4 channels)
- [x] Coulomb counter implementation
- [x] Parameter management system
- [x] USB serial interface (debug and API)
- [x] UART interface for ESPHome (921600 baud)
- [x] PWM contactor control (4 channels, sequenced)
- [x] Serial command processing (30+ commands)
- [x] CAN bus interface (can2040, 500kbps)
- [x] isoSPI PIO master implementation
- [x] isoSPI passive bus snooper
- [x] Unified BMB test interface
- [x] Runtime interface switching (Batman/isoSPI)

### 🚧 In Progress (Phase 3)
- [x] BATMan/isoSPI interface switching
- [x] Runtime interface control (automatic disable/enable)
- [ ] Full BMS state machine
- [ ] Cell voltage reading via isoSPI
- [ ] Cell balancing control
- [ ] Temperature monitoring

### 📋 Pending (Phases 4-6)
- [ ] Hardware testing and validation
- [ ] Calibration routines for ADC
- [ ] Flash storage for calibration data
- [ ] Error handling and recovery
- [ ] Extended testing (24+ hours)
- [ ] Performance optimization
- [ ] Documentation completion

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly with hardware
5. Submit a pull request

## Hardware Safety

⚠️ **HIGH VOLTAGE WARNING**
- This system interfaces with 400V battery packs
- Proper isolation and safety measures required
- Use appropriate personal protection equipment
- Follow all electrical safety protocols
- EV battery systems can deliver lethal current

⚠️ **DEVELOPMENT STATUS**
- This is a development version (v2.0.0)
- isoSPI implementation is experimental
- BATMan protocol partially implemented
- Extensive testing required before production use
- **Use at your own risk**

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

## Changelog

### Version 2.0.0 (November 2025) - RP2350A Port
**Complete hardware platform migration to RP2350A:**

#### **Hardware Changes**
- **Microcontroller:** Migrated from ESP32 to RP2350A
- **Current Sensing:** Replaced AS8510 with Texas Instruments INA228
- **Pack Voltage:** Added internal 12-bit ADC monitoring (4 channels)
- **CAN Bus:** Integrated hardware CAN transceiver support

#### **New Core Features**
- **CAN Bus Interface:** 500kbps broadcast via can2040 library (PIO-based)
  - Automatic 10Hz pack status broadcasting
  - Configurable message IDs and data formatting
  - Statistics tracking for TX/RX/errors
- **isoSPI PIO Implementation:**
  - Master mode for direct BMB communication
  - Passive snooper mode for bus monitoring
  - Runtime switching between Batman SPI and isoSPI
  - Differential Manchester encoding/decoding
- **Unified BMB Testing:** Single test interface for both Batman and isoSPI modes
- **INA228 Driver:** High-precision current, voltage, and power monitoring
- **Coulomb Counter:** Accurate SOC tracking with INA228 integration
- **ADC Monitor:** 4-channel pack voltage monitoring with voltage dividers
- **Contactor Control:** 4-channel PWM control with sequenced operation

#### **Enhanced Capabilities**
- **Serial Command System:** Comprehensive CLI with 30+ commands
- **Interface Switching:** Runtime enable/disable of Batman/isoSPI modes
- **Continuous Testing:** Automated BMB testing every 2 seconds
- **Hardware Monitoring:** Real-time status for all peripherals
- **Parameter System:** Enhanced with additional monitoring parameters

#### **Development Infrastructure**
- **Build System:** Migrated to Pico SDK with CMake
- **PIO Programming:** Custom PIO code for CAN and isoSPI
- **C Implementation:** Pure C codebase (replaced C++ Arduino)
- **Documentation:** Added CAN_INTEGRATION_REPORT.md and CAN_MESSAGE_FORMAT.md

### Version 1.3.1 (January 2025)
**New Feature: Exact Balance Cell Tracking**
- **NEW**: Added `BalanceCellList` parameter that provides a comma-separated list of exact cell numbers being balanced
- **Enhanced**: Cell voltage graph now shows precise balance indicators using exact BMS data instead of estimation
- **Improved**: Balance indication accuracy now at 100% - no more false positives or missed balancing cells
- **Technical**: Added string parameter support to Param system for text-based data
- **Display**: Extended red bars below graph baseline show exactly which cells are balancing
- **API**: New parameter accessible via serial command `param get BalanceCellList`

### Version 1.3.0 (January 2025)
**Added comprehensive ESPHome display interface enhancements:**

#### **New Third Display Page - Cell Voltage Graph**
- **Visual Bar Graph**: All 108 individual cell voltages displayed as vertical bars
- **Real-time Updates**: Bars dynamically adjust height based on actual voltages (3.0V-4.2V range)
- **Color-Coded Status**: 
  - 🔴 **Red**: >4.1V (overcharged cells)
  - 🟡 **Yellow**: 3.9-4.1V (high voltage)
  - 🟢 **Green**: 3.2-3.9V (normal operating range)  
  - 🔵 **Blue**: <3.2V (low voltage cells)
- **Compact Layout**: 3px wide bars with 4px spacing to fit all 108 cells on 480px display
- **Visual Scale**: Voltage reference lines at 3.0V, 3.4V, 3.8V, and 4.2V

#### **Enhanced Navigation System**
- **Three-Page Interface**: Main → Details → Cell Graph
- **Touch Navigation**: 
  - Main page: "Details >" button
  - Details page: "< Back" and "Cells >" buttons  
  - Cell graph: "< Details" button
- **Seamless Flow**: Intuitive page transitions with consistent UI

#### **Comprehensive Data Integration**
- **Parameter Expansion**: Added 15+ new BMS parameters
  - System: `numbmbs`, `LoopCnt`, `LoopState`, `CellsPresent`, `CellsBalancing`
  - Voltages: `umax`, `umin`, `deltaV`, `uavg`, `udc`, `CellMax`, `CellMin`
  - Temperatures: `Chipt0`, `Cellt0_0`, `Cellt0_1`, `TempMax`, `TempMin`
  - Control: `balance` status
- **Unit Conversion**: All voltage displays now show V instead of mV (3 decimal precision)
- **Individual Cell Monitoring**: All 108 cell voltages (u1-u108) with intelligent batching

#### **Performance Optimizations**
- **Intelligent Request Batching**: 6 batches of cell voltage requests with staggered timing
- **Memory Management**: Efficient bar creation with single initialization flag
- **Update Frequency**: 5s for main parameters, 10s for cell voltages
- **Visual Feedback**: Real-time balance status with color indicators

### Version 1.2.0 - Dual Page Display Interface
**Enhanced ESPHome touchscreen interface:**
- **Two-page Display**: Main overview + detailed battery information
- **Touch Controls**: Balance on/off buttons with immediate feedback
- **Real-time Updates**: Live parameter monitoring every 5-10 seconds
- **Professional UI**: Grid-based layout optimized for 480x320 display

### Version 1.1.0 - ESPHome Integration
**Added comprehensive ESPHome display interface:**
- **QSPI Display Support**: JC4832W535 480x320 touchscreen
- **LVGL Interface**: Modern touch-based UI
- **Home Assistant Integration**: All BMS parameters as HA entities
- **OTA Updates**: Over-the-air firmware updates
- **WiFi Connectivity**: Remote monitoring capabilities

### Version 1.0.0 - Core BMS Interface
**Initial release with Arduino/ESP32 implementation:**
- **Parameter API**: 108+ BMS parameters via serial commands
- **Dual Serial Interface**: USB + hardware UART
- **Balance Control**: Remote cell balancing enable/disable
- **Real-time Monitoring**: Cell voltages, temperatures, system status

## References

- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [Pico SDK Documentation](https://www.raspberrypi.com/documentation/pico-sdk/)
- [INA228 Datasheet](https://www.ti.com/product/INA228)
- [can2040 Library](https://github.com/KevinOConnor/can2040)
- [Original BATMan BMS](https://github.com/damienmaguire/Tesla-M3-Bms-Software)

## Acknowledgments

- **Damien Maguire & Tom de Bree** - Original BATMan BMS software
- **ESPHome Community** - Framework and component support
- **Kevin O'Connor** - can2040 library for RP2040/RP2350
- **Raspberry Pi Foundation** - Pico SDK and excellent documentation
