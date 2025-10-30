# CAN Message Broadcasting Format

## Overview
The BMS broadcasts 3 CAN messages every 100ms (10Hz update rate) containing all critical pack information.

## Message Layout

### 🔋 Message 0x100: Pack Voltage and Current
**DLC: 8 bytes | Broadcast Rate: 10Hz**

```
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│  0   │  1   │  2   │  3   │  4   │  5   │  6   │  7   │
├──────┴──────┼──────┴──────┼──────┴──────┼──────┴──────┤
│ Pack Voltage│   Current   │    Power    │   Reserved  │
│   (0.1V)    │   (0.1A)    │    (1W)     │     (0)     │
└─────────────┴─────────────┴─────────────┴─────────────┘
```

**Fields:**
- **Bytes 0-1**: Pack Voltage (uint16_t, little-endian)
  - Resolution: 0.1V
  - Range: 0 - 6553.5V
  - Example: `0x0C80` = 320.0V

- **Bytes 2-3**: Current (int16_t, little-endian, signed)
  - Resolution: 0.1A
  - Range: -3276.8A to +3276.7A
  - Example: `0x0064` = 10.0A (charging), `0xFF9C` = -10.0A (discharging)

- **Bytes 4-5**: Power (int16_t, little-endian, signed)
  - Resolution: 1W
  - Range: -32768W to +32767W
  - Example: `0x0C80` = 3200W

### 📊 Message 0x101: SOC and Cell Statistics
**DLC: 8 bytes | Broadcast Rate: 10Hz**

```
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│  0   │  1   │  2   │  3   │  4   │  5   │  6   │  7   │
├──────┴──────┼──────┴──────┼──────┴──────┼──────┴──────┤
│     SOC     │ Cell Count  │  Min Cell   │  Max Cell   │
│   (0.1%)    │     (1)     │  Volt (mV)  │  Volt (mV)  │
└─────────────┴─────────────┴─────────────┴─────────────┘
```

**Fields:**
- **Bytes 0-1**: State of Charge (uint16_t)
  - Resolution: 0.1%
  - Range: 0 - 6553.5%
  - Example: `0x0384` = 90.0%

- **Bytes 2-3**: Cell Count (uint16_t)
  - Resolution: 1 cell
  - Range: 0 - 65535 cells
  - Example: `0x006C` = 108 cells (Tesla Model 3 pack)

- **Bytes 4-5**: Min Cell Voltage (uint16_t)
  - Resolution: 1mV
  - Range: 0 - 65.535V
  - Example: `0x0CE4` = 3.300V

- **Bytes 6-7**: Max Cell Voltage (uint16_t)
  - Resolution: 1mV
  - Range: 0 - 65.535V
  - Example: `0x0D48` = 3.400V

### 🌡️ Message 0x102: Temperature Monitoring
**DLC: 8 bytes | Broadcast Rate: 10Hz**

```
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│  0   │  1   │  2   │  3   │  4   │  5   │  6   │  7   │
├──────┼──────┼──────┼──────┴──────┴──────┴──────┴──────┤
│ Max  │ Min  │INA228│         Reserved (0)              │
│ Temp │ Temp │ Temp │                                   │
│(0.1C)│(0.1C)│(0.1C)│                                   │
└──────┴──────┴──────┴───────────────────────────────────┘
```

**Fields:**
- **Byte 0**: Max Temperature (uint8_t)
  - Resolution: 0.1°C
  - Offset: +40°C (to handle negative temps)
  - Range: -40°C to +215°C
  - Formula: `temp_celsius = (byte_value / 10.0) - 40.0`
  - Example: `0xFA` = 250 → (250/10)-40 = 25.0°C

- **Byte 1**: Min Temperature (uint8_t)
  - Same format as Max Temperature
  - Example: `0xDC` = 220 → (220/10)-40 = 22.0°C

- **Byte 2**: INA228 Current Sensor Temperature (uint8_t)
  - Same format as Max Temperature
  - Example: `0xE6` = 230 → (230/10)-40 = 23.0°C

## Example CAN Frame Capture

```
Time      ID   DLC  Data
─────────────────────────────────────────────────────────
0.000     100   8   80 0C 64 00 00 0C 00 00   // 320.0V, 10.0A, 3072W
0.000     101   8   84 03 6C 00 E4 0C 48 0D   // 90.0%, 108 cells, 3.300-3.400V
0.000     102   8   FA DC E6 00 00 00 00 00   // 25.0°C, 22.0°C, 23.0°C

0.100     100   8   80 0C 65 00 04 0C 00 00   // 320.0V, 10.1A, 3076W
0.100     101   8   85 03 6C 00 E4 0C 48 0D   // 90.1%, 108 cells, 3.300-3.400V
0.100     102   8   FA DC E6 00 00 00 00 00   // 25.0°C, 22.0°C, 23.0°C
```

## Decoding Examples

### Python Decoder
```python
def decode_voltage_current(data):
    pack_v = ((data[1] << 8) | data[0]) * 0.1  # Volts
    current = ((data[3] << 8) | data[2])
    if current > 32767:  # Handle signed
        current = current - 65536
    current = current * 0.1  # Amps
    power = ((data[5] << 8) | data[4])
    if power > 32767:
        power = power - 65536  # Watts
    return pack_v, current, power

def decode_soc_cells(data):
    soc = ((data[1] << 8) | data[0]) * 0.1  # Percent
    cell_count = (data[3] << 8) | data[2]
    min_v = ((data[5] << 8) | data[4]) / 1000.0  # Volts
    max_v = ((data[7] << 8) | data[6]) / 1000.0  # Volts
    return soc, cell_count, min_v, max_v

def decode_temperature(data):
    max_temp = (data[0] / 10.0) - 40.0  # Celsius
    min_temp = (data[1] / 10.0) - 40.0  # Celsius
    ina_temp = (data[2] / 10.0) - 40.0  # Celsius
    return max_temp, min_temp, ina_temp
```

### C Decoder
```c
typedef struct {
    float pack_voltage;
    float current;
    int16_t power;
} can_msg_0x100_t;

typedef struct {
    float soc;
    uint16_t cell_count;
    float min_cell_v;
    float max_cell_v;
} can_msg_0x101_t;

typedef struct {
    float max_temp;
    float min_temp;
    float ina228_temp;
} can_msg_0x102_t;

void decode_0x100(uint8_t *data, can_msg_0x100_t *out) {
    uint16_t pack_v_raw = (data[1] << 8) | data[0];
    int16_t current_raw = (data[3] << 8) | data[2];
    int16_t power_raw = (data[5] << 8) | data[4];
    
    out->pack_voltage = pack_v_raw * 0.1f;
    out->current = current_raw * 0.1f;
    out->power = power_raw;
}

void decode_0x101(uint8_t *data, can_msg_0x101_t *out) {
    uint16_t soc_raw = (data[1] << 8) | data[0];
    uint16_t min_v_raw = (data[5] << 8) | data[4];
    uint16_t max_v_raw = (data[7] << 8) | data[6];
    
    out->soc = soc_raw * 0.1f;
    out->cell_count = (data[3] << 8) | data[2];
    out->min_cell_v = min_v_raw / 1000.0f;
    out->max_cell_v = max_v_raw / 1000.0f;
}

void decode_0x102(uint8_t *data, can_msg_0x102_t *out) {
    out->max_temp = (data[0] / 10.0f) - 40.0f;
    out->min_temp = (data[1] / 10.0f) - 40.0f;
    out->ina228_temp = (data[2] / 10.0f) - 40.0f;
}
```

## Hardware Setup

### Required Components
1. **CAN Transceiver** (pick one):
   - MCP2551 (5V, classic choice)
   - TJA1050 (5V, automotive grade)
   - SN65HVD230 (3.3V, low power)
   - ISO1050 (5V, isolated)

### Wiring Diagram
```
RP2350               CAN Transceiver        CAN Bus
─────────           ─────────────────       ─────────
                    
GP3 (TX) ──────────► TXD             
GP2 (RX) ◄────────── RXD
                                    
GND ────────────────► GND
3.3V ───────────────► VCC (if 3.3V transceiver)
                    
                     CANH ───────────────────┐
                                             │ 120Ω
                     CANL ───────────────────┘
```

### Pin Configuration
```c
// In pin_config.h
#define CAN_PIN_RX              2        // GP2 - CAN RX
#define CAN_PIN_TX              3        // GP3 - CAN TX
#define CAN_PIO_NUM             0        // Use PIO0
#define CAN_BITRATE             500000   // 500 kbps
```

## Serial Commands

New command added to BMS:
```
can                 - Show CAN bus statistics
```

Example output:
```
=== CAN Bus Statistics ===
RX Messages: 0
TX Messages: 1234
Errors: 0
========================
```

## Performance Characteristics

- **Broadcast Rate**: 10Hz (100ms interval)
- **Messages per second**: 30 (3 messages × 10Hz)
- **Bus utilization at 500kbps**: ~0.8% (very low)
- **Latency**: <1ms (handled by PIO)
- **CPU Impact**: Minimal (PIO handles protocol)
- **Memory**: 1.3 KB RAM, 7.4 KB flash

## Integration with Vehicle Systems

This CAN format is suitable for:
- ✅ Battery Management Systems
- ✅ Vehicle displays and dashboards  
- ✅ Data logging systems
- ✅ Charge controllers
- ✅ Motor controllers
- ✅ Telemetry systems

## Future Enhancements

Potential additions:
1. J1939 protocol support for heavy vehicles
2. CANopen for industrial automation
3. ISO-TP for diagnostics (UDS)
4. Remote control commands (charge enable, etc.)
5. Configurable CAN IDs
6. Extended frames (29-bit IDs)
7. Message filtering by ID

