# Tesla Model 3 BMS Interface - RP2350A Port

This is the RP2350A port of the Tesla Model 3 BMS Interface, replacing ESP32 with RP2350A MCU and AS8510 with INA228 for current sensing.

## Hardware Requirements

- **MCU:** RP2350A (custom board)
- **Current Sensor:** Texas Instruments INA228 (I2C)
- **Pack Voltage Sensing:** Internal 12-bit ADC with voltage dividers (400V → 3.0V)
- **BMS Interface:** Tesla Model 3 Battery Management Boards (SPI)
- **Display:** ESP32-S3 with ESPHome (UART communication)

## Pin Configuration

See `include/pin_config.h` for complete pin assignments:

- **Tesla BMS SPI:** GP16 (MISO), GP19 (MOSI), GP18 (SCK), GP17 (CS)
- **INA228 I2C:** GP4 (SDA), GP5 (SCL)
- **Pack Voltage ADC:** GP26-29 (4 channels)
- **ESPHome UART:** GP0 (TX), GP1 (RX)
- **Contactor PWM:** GP20 (Pack), GP21 (Precharge)

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
├── include/                    # Header files
│   ├── pin_config.h           # Pin assignments
│   ├── batman.h               # BMS interface
│   ├── param.h                # Parameter system
│   ├── ina228.h               # INA228 driver
│   ├── adc_monitor.h          # ADC voltage monitor
│   └── coulomb_counter.h      # Coulomb counting
├── src/                        # Source files
│   ├── main.c                 # Main application
│   ├── batman.c               # BMS interface (stub)
│   ├── param.c                # Parameter management
│   ├── ina228.c               # INA228 driver
│   ├── adc_monitor.c          # ADC implementation
│   └── coulomb_counter.c      # Coulomb counter
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
- [x] PWM contactor control
- [x] Serial command processing

### 🚧 In Progress (Phase 3)
- [ ] BATMan SPI protocol implementation
- [ ] Full BMS state machine
- [ ] Cell voltage reading
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
contactors on/off   - Enable/disable pack contactors
precharge on/off    - Enable/disable precharge relay
params              - List all parameters
ina228              - Show INA228 current sensor status
adc                 - Show ADC pack voltages
coulomb             - Show coulomb counter status
batman              - Show BATMan BMS interface status
```

### ESPHome Integration

The system sends data to ESPHome display via UART0 (GP0/GP1) at 921600 baud:

```
param=value\n
cellID=voltage\n
DATA_COMPLETE\n
```

No changes required to the existing ESPHome configuration.

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

## Safety Warnings

⚠️ **HIGH VOLTAGE WARNING**
- This system interfaces with 400V battery packs
- Proper isolation and safety measures required
- Use appropriate protection equipment
- Follow all electrical safety protocols

⚠️ **DEVELOPMENT STATUS**
- This is a development version
- BATMan protocol not fully implemented
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

## Conversion Plan

See `rp2350-ina228-conversion.plan.md` for the complete conversion roadmap.

## Contact

For questions or issues, please open a GitHub issue.

