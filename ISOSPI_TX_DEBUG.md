# isoSPI TX Pin Debug Procedure

## Issue Fixed
- **Bug**: `sm_config_set_in_pin_count()` was getting pin base (9) instead of count (2)
- **Status**: Fixed in commit

## Hardware Setup
- **TX Pins**: GP7 (enable), GP8 (data)
- **RX Pins**: GP9 (high), GP10 (low)
- **Sampling**: GP11 (debug)

## Test Procedure

### 1. Flash Updated Firmware
```bash
# Hold BOOTSEL and connect USB, then:
cp build/tesla_bms_rp2350.uf2 /path/to/RPI-RP2/
```

### 2. Connect to Serial Console
```bash
# Use your preferred serial terminal at 115200 baud
# You should see initialization messages
```

### 3. Initialize isoSPI (First Time Only)
```
isospi init
```

**Expected Output:**
```
Initializing isoSPI interface...
isoSPI Master: Loading PIO program...
isoSPI Master: PIO program loaded at offset X
isoSPI Master: PIO state machine configured
isoSPI Master: State machine enabled
isoSPI Master: Initialized on PIO2 SM0 (TX: GP7-GP8, RX: GP9-GP10)
isoSPI Snooper: Initialized on PIO1 SM1 (RX: GP9-GP10, invert=0, sampling: GP11)
isoSPI interface initialized successfully
```

### 4. Enable isoSPI Master
```
isospi enable
```

**Expected Output:**
```
Switching to isoSPI interface (disabling Batman)
```

### 5. Run Test Pattern
```
isospi test
```

**Expected Output:**
```
=== isoSPI Test Pattern ===
[isoSPI TX] Starting transmission of 5 bytes
TX: 0xAA 0xFF 0x00 0xCC 0x33 
RX: 0xXX 0xXX 0xXX 0xXX 0xXX 
Valid: YES/NO
=========================
```

### 6. Run BMB Test
```
bmb test
```

**Expected Output:**
```
=== Running BMB Test (isoSPI PIO) ===
Sending command sequence via isoSPI...

1. WAKEUP command:     0x2AD4
[isoSPI TX] Starting transmission of 2 bytes
   ...

2. IDLE_WAKE command:  0x21F2
[isoSPI TX] Starting transmission of 2 bytes
   ...
```

## Scope/Logic Analyzer Check

### What to Look For on GP7 (Enable):
1. During chip select: HIGH-LOW-IDLE pattern
2. During bits: Should stay in appropriate state
3. Differential signaling should be visible

### What to Look For on GP8 (Data):
1. Manchester encoding: transitions mid-bit
2. HIGH = 0b11, LOW = 0b10, IDLE = 0b00
3. Bit timing should match PIO delays

## Troubleshooting

### No Output at All
- Check if `[isoSPI TX] Starting transmission` appears in serial
- If no print: Function not being called
- If print appears: PIO2 initialization issue

### Pins Show Wrong Values
- Verify GPIO mux isn't overridden by another peripheral
- Check that Batman is disabled (`batman enable` to verify)

### Intermittent Output
- Check PIO clock divider settings
- Verify FIFO isn't stalling

## Debug Commands

```bash
# Show current interface status
isospi status

# Check pin states manually
snoop diag

# Re-enable Batman to compare
batman enable
```

## Expected GPIO States

| Command | GP7 (Enable) | GP8 (Data) | Notes |
|---------|--------------|------------|-------|
| Idle | LOW | LOW | Waiting for data |
| CS Pattern | HIGH→LOW→IDLE | HIGH→LOW→IDLE | Start of frame |
| TX Bit 1 | HIGH | HIGH→LOW | Manchester 1 |
| TX Bit 0 | LOW | LOW→HIGH | Manchester 0 |

## Next Steps

If TX pins now show activity:
1. ✅ PIO2 is working correctly
2. ✅ Pin mux is correct
3. → Next: Debug RX path if needed
4. → Next: Verify Manchester encoding timing

If still no activity:
1. Verify PIO2 exists on RP2350 (should have 3 PIOs)
2. Check for PIO resource conflicts
3. Try different pins (test if GP7/GP8 have issues)

