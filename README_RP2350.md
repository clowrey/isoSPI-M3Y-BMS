# Tesla Model 3 BMS Interface - RP2350A Port

This is the RP2350A port of the Tesla Model 3 BMS Interface, replacing ESP32 with RP2350A MCU and AS8510 with INA228 for current sensing.

## Hardware Requirements

- **MCU:** RP2350A (custom board)
- **Current Sensor:** Texas Instruments INA228 (I2C)
- **Pack Voltage Sensing:** Internal 12-bit ADC with voltage dividers (400V → 3.0V)
- **BMS Interface:** Tesla Model 3 Battery Management Boards (SPI/isoSPI)
- **Display:** ESP32-S3 with ESPHome (UART communication)
- **CAN Transceiver:** Required for CAN bus functionality (e.g., MCP2551, TJA1050)

## Pin Configuration

See `include/pin_config.h` for complete pin assignments:

- **Tesla BMS SPI:** GP16 (MISO), GP19 (MOSI), GP18 (SCK), GP17 (CS), GP22 (Enable)
- **INA228 I2C:** GP4 (SDA), GP5 (SCL), GP6 (Alert)
- **Pack Voltage ADC:** GP26-29 (4 channels: Pack Neg, Pack Pos, Link Neg, Link Pos)
- **ESPHome UART:** GP0 (TX), GP1 (RX)
- **Contactor PWM:** GP20 (Link Pos), GP21 (Link Neg), GP23 (FC Pos), GP24 (FC Neg)
- **CAN Bus:** GP2 (RX), GP3 (TX)
- **isoSPI PIO:** GP7-11 (TX/RX differential pairs + sampling)

## Building

### Prerequisites

1. Install Pico SDK:
```bash
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=$(pwd)
```

2. Install toolchain:
```bash
# For Ubuntu/Debian:
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# For macOS:
brew install cmake
brew install --cask gcc-arm-embedded
```

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

The output file will be `tesla_bms_rp2350.uf2`.

### Flashing

1. Hold BOOTSEL button while connecting USB
2. Copy `tesla_bms_rp2350.uf2` to the mounted drive
3. Device will reboot automatically

## Project Structure

```
.
├── CMakeLists.txt              # Build configuration
├── pico_sdk_import.cmake       # Pico SDK import
├── can2040/                    # CAN bus library
│   └── src/
│       ├── can2040.c          # can2040 implementation
│       └── can2040.h          # can2040 header
├── src/
│   ├── include/                # Header files
│   │   ├── pin_config.h       # Pin assignments
│   │   ├── batman.h           # BMS interface
│   │   ├── param.h            # Parameter system
│   │   ├── ina228.h           # INA228 driver
│   │   ├── adc_monitor.h      # ADC voltage monitor
│   │   ├── coulomb_counter.h  # Coulomb counting
│   │   ├── can_interface.h    # CAN bus interface
│   │   ├── isospi_interface.h # isoSPI interface manager
│   │   ├── isospi_master.h    # isoSPI master (PIO)
│   │   ├── isosnoop.h         # isoSPI bus snooper
│   │   └── bmb_test.h         # Unified BMB test interface
│   ├── main.c                 # Main application
│   ├── batman.c               # BMS interface (stub)
│   ├── param.c                # Parameter management
│   ├── ina228.c               # INA228 driver
│   ├── adc_monitor.c          # ADC implementation
│   ├── coulomb_counter.c      # Coulomb counter
│   ├── can_interface.c        # CAN bus implementation
│   ├── isospi_interface.c     # isoSPI interface manager
│   ├── isospi_master.c        # isoSPI master implementation
│   ├── isospi_master.pio      # PIO code for isoSPI TX/RX
│   ├── isosnoop.c             # isoSPI snooper implementation
│   ├── isosnoop.pio           # PIO code for passive snooping
│   └── bmb_test.c             # Unified BMB test implementation
└── README_RP2350.md           # This file
```

## Current Implementation Status

### ✅ Completed (Phase 2)
- [x] Pico SDK project structure
- [x] CMakeLists.txt configuration
- [x] Pin configuration header
- [x] INA228 I2C driver (complete)
- [x] Internal ADC driver for pack voltages
- [x] Coulomb counter implementation
- [x] Parameter management system
- [x] Main application loop
- [x] USB serial interface (debug)
- [x] UART interface (ESPHome)
- [x] PWM contactor control (4 channels)
- [x] Serial command processing
- [x] CAN bus interface (can2040)
- [x] isoSPI PIO master implementation
- [x] isoSPI passive bus snooper
- [x] Unified BMB test interface

### 🚧 In Progress (Phase 3)
- [x] BATMan/isoSPI interface switching
- [x] Runtime interface control (automatic disable/enable)
- [ ] Full BMS state machine
- [ ] Cell voltage reading via isoSPI
- [ ] Cell balancing control
- [ ] Temperature monitoring

### 📋 Pending (Phase 4-6)
- [ ] Hardware testing and validation
- [ ] Calibration routines
- [ ] Flash storage for calibration
- [ ] Error handling and recovery
- [ ] Extended testing (24+ hours)
- [ ] Performance optimization
- [ ] Documentation completion

## Usage

### Serial Commands

Connect via USB serial (115200 baud) and use these commands:

```
help                - Show available commands
status              - Show system status
balance on/off      - Enable/disable cell balancing
contactors on/off   - Enable/disable all contactors (sequenced)
link_neg on/off     - Enable/disable Link Negative contactor
link_pos on/off     - Enable/disable Link Positive contactor
fc_pos on/off       - Enable/disable FC Positive (precharge) contactor
fc_neg on/off       - Enable/disable FC Negative contactor
params              - List all parameters
ina228              - Show INA228 current sensor status
adc                 - Show ADC pack voltages
coulomb             - Show coulomb counter status
batman              - Show BATMan BMS interface status
can                 - Show CAN bus statistics
isospi init         - Initialize isoSPI interface
isospi enable       - Switch to isoSPI master (disable Batman)
batman enable       - Switch to Batman (disable isoSPI)
isospi test         - Run isoSPI test pattern
isospi snoop        - Print captured bus traffic
isospi status       - Show isoSPI interface status
bmb test            - Run BMB test once (uses active interface)
bmb continuous on   - Enable continuous BMB testing (every 2 seconds)
bmb continuous off  - Disable continuous BMB testing
```

### ESPHome Integration

The system sends data to ESPHome display via UART0 (GP0/GP1) at 921600 baud:

```
param=value\n
cellID=voltage\n
DATA_COMPLETE\n
```

No changes required to the existing ESPHome configuration.

## New Features

### CAN Bus Interface

The system includes CAN bus support via the `can2040` library:

- **Bitrate:** 500 kbps
- **PIO-based:** Uses PIO0 for CAN protocol implementation
- **Message IDs:** Configurable standard or extended IDs
- **Broadcasting:** Automatic 10Hz broadcast of pack status
- **Statistics:** Track RX/TX message counts and errors

CAN messages broadcast pack voltage, current, power, SOC, and cell voltages.

### isoSPI Interface

The project now includes a PIO-based isoSPI interface:

- **Master Mode:** Transmit and receive differential Manchester-encoded data
- **Snooper Mode:** Passive monitoring of existing isoSPI bus traffic
- **PIO Implementation:** Uses PIO1 for high-speed bit-banging
- **Switchable:** Can switch between Batman SPI and isoSPI PIO at runtime
- **Debug Output:** Capture and decode bus traffic

**isoSPI Commands:**
1. `isospi init` - Initialize the PIO state machines
2. `isospi enable` - Switch to isoSPI master (disables Batman)
3. `batman enable` - Switch back to Batman SPI (disables isoSPI)
4. `isospi test` - Send test pattern and verify response
5. `isospi snoop` - Display captured bus traffic
6. `isospi status` - Show interface configuration

### Unified BMB Testing

The system includes a unified BMB test routine that automatically works with whichever interface (Batman or isoSPI) is currently active:

- **Single Test:** `bmb test` - Run one test cycle immediately
- **Continuous Mode:** `bmb continuous on` - Run tests every 2 seconds automatically
- **Stop Continuous:** `bmb continuous off` - Stop automatic testing

**Test Workflow:**
1. Initialize interface: `isospi init` (if using isoSPI)
2. Select interface: `isospi enable` or `batman enable`
3. Run test: `bmb test` (once) or `bmb continuous on` (repeated)
4. View results in serial output

The test routine automatically detects which interface is active and uses the appropriate protocol.

## Hardware Calibration

### ADC Voltage Divider Calibration

1. Measure actual pack voltage with calibrated meter
2. Use `adc` command to see measured voltage
3. Calculate correction factor
4. Apply calibration (stored in flash)

### INA228 Current Calibration

The INA228 is factory calibrated. Shunt resistance is set to 25.296µΩ (Tesla standard).

## Troubleshooting

### INA228 Not Detected

- Check I2C connections (GP4=SDA, GP5=SCL)
- Verify pull-up resistors (2.2kΩ recommended)
- Check I2C address (default 0x40)
- Run `ina228` command for diagnostics

### ADC Reading Issues

- Verify voltage dividers (400V → 3.0V)
- Check TVS diodes for protection
- Ensure voltages never exceed 3.3V
- Run `adc` command to check readings

### BMS Communication Failed

- Verify SPI connections (GP16-19)
- Check SPI clock speed (1MHz)
- Run `batman` command for status
- Enable debug mode for detailed logs
- Try switching to isoSPI mode: `isospi init` then `isospi enable`

### CAN Bus Issues

- Check CAN transceiver wiring (GP2=RX, GP3=TX)
- Verify bitrate matches other CAN devices (500kbps)
- Run `can` command to check statistics
- Ensure proper termination resistors (120Ω at each end)

### isoSPI Issues

- Check pin connections (GP7-11 for isoSPI PIO)
- Verify differential pair wiring
- Run `isospi status` to check initialization
- Use `isospi snoop` to monitor bus traffic
- Try `isospi test` to verify master functionality

## Safety Warnings

⚠️ **HIGH VOLTAGE WARNING**
- This system interfaces with 400V battery packs
- Proper isolation and safety measures required
- Use appropriate protection equipment
- Follow all electrical safety protocols

⚠️ **DEVELOPMENT STATUS**
- This is a development version
- isoSPI implementation is experimental
- BATMan protocol partially implemented
- Extensive testing required before production use
- Use at your own risk

## License

This project is licensed under the GNU General Public License v3.0.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## References

- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [Pico SDK Documentation](https://www.raspberrypi.com/documentation/pico-sdk/)
- [INA228 Datasheet](https://www.ti.com/product/INA228)
- [Tesla BMS Documentation](../PROJECT_DOCUMENTATION.md)
- [can2040 Library](https://github.com/KevinOConnor/can2040)
- [CAN Integration Report](../CAN_INTEGRATION_REPORT.md)
- [CAN Message Format](../CAN_MESSAGE_FORMAT.md)
- [Dual Serial API](../DUAL_SERIAL_API.md)

## Conversion Plan

See `rp2350-ina228-conversion.plan.md` for the complete conversion roadmap.

## Contact

For questions or issues, please open a GitHub issue.

