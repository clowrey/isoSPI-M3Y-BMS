# PIO Allocation for RP2350

## Overview

The RP2350 has **3 PIOs** (PIO0, PIO1, PIO2), each with **4 state machines** (SM0-SM3).

This project uses all three PIOs for different high-speed interfaces:

## Current Allocation

| PIO  | State Machine | Purpose              | Files                          |
|------|---------------|----------------------|--------------------------------|
| PIO0 | SM0-SM3       | CAN Bus (can2040)    | `src/can_interface.c`          |
| PIO1 | SM1           | isoSPI Snooper       | `src/isosnoop.c`, `src/isosnoop.pio` |
| PIO2 | SM0           | isoSPI Master        | `src/isospi_master.c`, `src/isospi_master.pio` |

### Notes

1. **PIO0 (CAN Bus)**: The can2040 library uses all 4 state machines for CAN protocol implementation
   - Handles bit timing, synchronization, arbitration, and error detection
   - Uses GP2 (RX) and GP3 (TX)

2. **PIO1 SM1 (isoSPI Snooper)**: Passive monitoring of isoSPI bus traffic
   - DMA-based capture of differential Manchester encoding
   - Uses GP9 (high) and GP10 (low) for RX
   - Debug sampling output on GP11

3. **PIO2 SM0 (isoSPI Master)**: Active communication with Battery Management Boards
   - Differential Manchester TX/RX
   - Uses GP7 (enable) and GP8 (data) for TX
   - Uses GP9 (high) and GP10 (low) for RX (shared with snooper)

## Pin Configuration

All pin configurations are defined in `src/include/pin_config.h`:

```c
// CAN Bus Interface (PIO0)
#define CAN_PIN_RX              2        // GP2 - CAN RX
#define CAN_PIN_TX              3        // GP3 - CAN TX
#define CAN_PIO_NUM             0        // Use PIO0

// isoSPI PIO Interface (PIO1=snooper, PIO2=master)
#define ISOSPI_TX_PIN_BASE          7    // GP7=enable, GP8=data (GP7+1)
#define ISOSPI_RX_PIN_BASE          9    // GP9=high, GP10=low (GP9+1, shared)
#define ISOSPI_SAMPLING_PIN         11   // GP11 - debug output
#define ISOSPI_SNOOPER_PIO_NUM      1    // Use PIO1 for snooper
#define ISOSPI_MASTER_PIO_NUM       2    // Use PIO2 for master
```

## Benefits of Separate PIOs

1. **Isolation**: Each subsystem runs independently without resource conflicts
2. **Performance**: No shared instruction memory or state machine conflicts
3. **Flexibility**: Each PIO can be reconfigured without affecting others
4. **Debugging**: Easier to isolate and debug individual interfaces

## References

- RP2350 Datasheet: 3 PIOs, 4 state machines each
- can2040 Documentation: Uses all 4 SMs of one PIO block
- isoSPI Context: `context/isoSPI_Master/` and `context/isosnooper/`

