# RP2040/RP2350 PIO (Programmable I/O) Architecture

## Overview

The Programmable Input/Output (PIO) system is a unique hardware interface in RP2040/RP2350 microcontrollers that handles custom I/O operations with precise timing and determinism. This document explains how PIO works, particularly in the context of our isoSPI implementation.

## Hardware Architecture

### PIO Blocks
- **RP2040**: 2 PIO blocks (PIO0, PIO1)
- **RP2350**: 3 PIO blocks (PIO0, PIO1, PIO2) 
- Each PIO block contains:
  - 4 independent state machines (SM0-SM3)
  - 32 instructions of shared program memory
  - Shared IRQ flags (8 flags per PIO block)
  - Shared GPIO access

### State Machine Components

Each state machine (SM) has:

1. **Registers**
   - 2× 32-bit shift registers (ISR/OSR) - for data input/output
   - 2× 32-bit scratch registers (X, Y) - for temporary storage/counters
   - Program counter (PC) - 5 bits (addresses 0-31)

2. **FIFO Buffers**
   - 4× 32-bit TX FIFO (CPU writes here, SM reads via PULL/OUT)
   - 4× 32-bit RX FIFO (SM writes via PUSH/IN, CPU reads here)
   - Can be joined: 8× 32-bit in one direction (TX-only or RX-only mode)

3. **Clock Divider**
   - 16-bit integer part, 8-bit fractional part
   - Allows each SM to run at different speeds
   - System clock / divider = SM clock frequency
   - Example: 150MHz / 1.0 = 150MHz = 6.67ns per instruction

4. **Pin Mapping**
   - SET pins: pins controlled by `set` instruction
   - OUT pins: pins controlled by `out` instruction (via MOV or direct)
   - IN pins: pins read by `in` instruction
   - SIDESET pins: pins controlled by sideset (optional additional output)
   - JMP pin: single pin used by `jmp pin` instruction
   - WAIT pins: pins monitored by `wait` instruction

## Instruction Set

PIO has only 9 instructions, each 16 bits wide:

### 1. JMP - Jump
```
jmp <condition> <address>
```
**Conditions:**
- (none) - always jump
- `!x` - jump if X is zero
- `x--` - jump if X is non-zero (post-decrement)
- `!y` - jump if Y is zero  
- `y--` - jump if Y is non-zero (post-decrement)
- `x!=y` - jump if X ≠ Y
- `pin` - jump if JMP pin is high
- `!osre` - jump if OSR is not empty

**Use in isoSPI:**
```pio
jmp !x zero_bit        ; Jump to zero_bit if X is 0
jmp x-- received_something  ; Jump if X was non-zero (decrements X)
```

### 2. WAIT - Wait for Condition
```
wait <polarity> <source> <index>
```
**Sources:**
- `gpio` - wait for GPIO level
- `pin` - wait for input-mapped pin level
- `irq` - wait for IRQ flag to be set/cleared

**Polarity:** 0 = wait for low, 1 = wait for high

**Use in isoSPI:**
```pio
wait 1 pin 0           ; Wait for input pin 0 to go high (edge detection)
wait 1 irq 0           ; Wait for IRQ 0 flag to be set
wait 0 pin 0           ; Wait for input pin 0 to go low
```

### 3. IN - Shift Data In (to ISR)
```
in <source>, <bit count>
```
**Sources:** pins, x, y, null, isr, osr, status

Shifts data from source into Input Shift Register (ISR).
When ISR reaches autopush threshold, automatically pushes to RX FIFO.

**Use in isoSPI:**
```pio
in x, 2                ; Shift 2 bits from X into ISR
in y, 2                ; Shift 2 bits from Y into ISR
                      ; When 32 bits accumulated (8 samples), autopush to FIFO
```

### 4. OUT - Shift Data Out (from OSR)
```
out <destination>, <bit count>
```
**Destinations:** pins, x, y, null, pindirs, pc, isr, exec

Shifts data from Output Shift Register (OSR) to destination.
When OSR is empty, automatically pulls from TX FIFO (autopull).

**Use in isoSPI:**
```pio
out x, 1               ; Shift 1 bit from OSR into X register
out y, 2               ; Shift 2 bits from OSR into Y register
```

### 5. PUSH - Push ISR to RX FIFO
```
push [iffull] [block]
```
Explicitly push ISR contents to RX FIFO.
- `iffull`: only push if ISR shift count matches threshold
- `block`: stall if FIFO is full (default: stall)

**Use in isoSPI:**
Most of our code uses autopush (configured in C code), so explicit PUSH is rare.

### 6. PULL - Pull from TX FIFO to OSR
```
pull [ifempty] [block]
```
Explicitly pull from TX FIFO to OSR.
- `ifempty`: only pull if OSR is empty
- `block`: stall if FIFO is empty (default: stall)

**Use in isoSPI:**
Most of our code uses autopull (configured in C code), so explicit PULL is rare.

### 7. MOV - Move/Copy Between Registers
```
mov <destination>, <source>
```
**Sources/Destinations:** pins, x, y, null, status, isr, osr, pc, exec

**Operations (optional):**
- `~` - bitwise invert
- `::` - bit reverse

**Use in isoSPI:**
```pio
mov x, pins            ; Copy pin states into X register
mov y, pins            ; Copy pin states into Y register
mov pins, x            ; Output X register value to pins
```

### 8. IRQ - Set/Clear IRQ Flags
```
irq <mode> <index>
```
**Modes:**
- `set` - set IRQ flag
- `clear` - clear IRQ flag
- `wait` - wait until IRQ flag is clear, then set it (atomic test-and-set)

**Index:** 0-7 (8 IRQ flags per PIO block)

**Use in isoSPI:**
```pio
irq set 0              ; Set IRQ 0 (signal edge detected)
irq clear 0            ; Clear IRQ 0 (reset for next edge)
irq set 1              ; Set IRQ 1 (start timer)
irq wait 3             ; Wait for IRQ 3 to be clear, then set it
```

### 9. SET - Set Pins/Pins Directions
```
set <destination>, <value>
```
**Destinations:**
- `pins` - set pin output levels (up to 5 pins)
- `pindirs` - set pin directions (1=output, 0=input)
- `x`, `y` - set scratch register to immediate value (0-31)

**Use in isoSPI:**
```pio
set pins, TX_HIGH      ; Set pins to TX_HIGH pattern (0b11)
set pins, TX_LOW       ; Set pins to TX_LOW pattern (0b10)
set pins, TX_IDLE      ; Set pins to TX_IDLE pattern (0b00)
```

## Instruction Delays and Sideset

### Delay Modifier `[N]`

Every instruction can have a delay modifier that adds extra cycles:
```pio
set pins, 1 [15]       ; Set pins high, then wait 15 additional cycles
mov x, pins [5]        ; Move pins to X, then wait 5 additional cycles
```

**Total execution time = 1 cycle (instruction) + N cycles (delay)**

At 150MHz: Each cycle = 6.67ns
- `[0]` = 6.67ns total
- `[15]` = 106.7ns total (16 cycles)
- `[31]` = 213.3ns total (32 cycles, maximum delay)

### Sideset

Sideset allows an instruction to simultaneously control additional pins:
```pio
.side_set 1            ; Declare 1 sideset pin

mov x, pins side 1     ; Move pins to X, set sideset pin HIGH
in x, 2 side 0         ; Shift X into ISR, set sideset pin LOW
```

**Use in isoSPI Snooper:**
The sideset pin is used as a diagnostic output that goes HIGH during sampling,
allowing us to see on an oscilloscope exactly when samples occur.

## IRQ System

### IRQ Flags

Each PIO block has 8 IRQ flags (0-7) that can be used for:
1. **Inter-SM communication** - coordinate multiple state machines
2. **CPU interrupts** - notify CPU of events

**Key properties:**
- Flags are shared across all 4 SMs in a PIO block
- Multiple SMs can read/write the same flag
- Flags have no inherent meaning - programmer defines behavior
- `irq set/clear` are instantaneous (don't stall)
- `irq wait` stalls until flag is clear, then atomically sets it

### IRQ Usage in isoSPI Master

```
IRQ 0: Edge detected (set by SM1/SM2, cleared by SM0, waited by SM0)
IRQ 1: Timer start (set by SM0, waited by SM3)
IRQ 2: Timer done (set by SM3, waited by SM0)
IRQ 3: Edge detector gate (clear=active, set=paused)
```

**Flow:**
1. SM0 (main): Transmits bit, then clears IRQ 0 and IRQ 3
2. SM1/SM2 (edge detectors): Wait for IRQ 3 clear, then watch for edges
3. SM1/SM2: When edge detected, set IRQ 0
4. SM0: Wait for IRQ 0, then sample
5. SM0: Set IRQ 1 to start timer
6. SM3 (timer): Wait for IRQ 1, delay, set IRQ 2
7. SM0: Wait for IRQ 2 before next bit

### IRQ Usage in isoSPI Snooper

```
IRQ 0: Start sampling (set by SM+1/SM+2/SM+3, cleared by SM, waited by SM)
IRQ 1: Data received (set by SM when data detected)
```

**Flow:**
1. SM+1/SM+2 (edge detectors): Continuously watch for edges
2. SM+1/SM+2: When edge detected, set IRQ 0
3. SM+3 (timeout): Free-running ~400ns timer, periodically sets IRQ 0
4. SM (main): Wait for IRQ 0, sample pins, push to FIFO
5. SM: If data detected, set IRQ 1 (currently not used, but available)

## FIFO and Data Flow

### Autopull (TX Direction: CPU → SM)

When enabled:
1. CPU writes bytes to TX FIFO using `pio_sm_put()`
2. When OSR is empty and SM executes `out`, automatically pulls from TX FIFO
3. Threshold determines when to pull (typically 8, 16, or 32 bits)

**Configuration in C:**
```c
sm_config_set_out_shift(&config, 
    false,  // shift_right: false=shift left, true=shift right
    true,   // autopull: enable
    8);     // threshold: pull when OSR has < 8 bits
```

**In isoSPI Master:**
- Threshold = 8 bits
- Each byte from CPU becomes 8 bits to transmit
- `out x, 1` automatically pulls next byte when OSR empty

### Autopush (RX Direction: SM → CPU)

When enabled:
1. SM shifts data into ISR using `in`
2. When ISR reaches threshold, automatically pushes to RX FIFO
3. CPU reads from RX FIFO using `pio_sm_get()`

**Configuration in C:**
```c
sm_config_set_in_shift(&config,
    false,  // shift_right: false=shift left, true=shift right  
    true,   // autopush: enable
    32);    // threshold: push when ISR has 32 bits
```

**In isoSPI Master:**
- Threshold = 32 bits
- Each bit sampled twice (2×2 bits = 4 bits per bit)
- 8 bits = 32 bits of samples → auto pushes to FIFO

**In isoSPI Snooper:**
- Threshold = 8 bits
- Each sample is 2 bits (differential pair)
- 4 samples = 8 bits → auto pushes to FIFO

### FIFO Join Mode

Can join TX and RX FIFOs for deeper buffer in one direction:
```c
sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);  // 8-deep RX, no TX
```

**In isoSPI Snooper:**
We use `PIO_FIFO_JOIN_RX` to get 8×32-bit RX FIFO depth instead of 4×32,
reducing chance of FIFO overflow when snooping high-speed traffic.

## Clock Divider

Each SM can run at a different speed using a fractional divider:

```c
sm_config_set_clkdiv_int_frac8(&config, 
    1,   // integer: 1 means no division
    0);  // fractional: 0 means no fractional part
```

**Formula:** `SM_freq = System_freq / (int + frac/256)`

**Examples at 150MHz system clock:**
- `(1, 0)` = 150MHz / 1.0 = 150MHz (6.67ns per cycle)
- `(2, 0)` = 150MHz / 2.0 = 75MHz (13.33ns per cycle)
- `(1, 128)` = 150MHz / 1.5 = 100MHz (10ns per cycle)

**In isoSPI:**
- Clock divider = 1.0 (no division)
- SM runs at full 150MHz = 6.67ns per cycle
- `set pins, 1 [14]` = 15 cycles = 100ns (isoSPI half-bit time)

## Timing Precision

### Why PIO is Perfect for isoSPI

isoSPI requires:
- 1 Mbps data rate
- 100ns pulse widths (differential Manchester encoding)
- Precise sampling windows

**With 150MHz PIO clock:**
- 1 cycle = 6.67ns
- 15 cycles = 100ns (perfect for isoSPI half-bit)
- Deterministic: every instruction takes exactly 1 + delay cycles
- No interrupts or context switches can affect timing

**Contrast with CPU:**
- CPU code has variable execution time
- Interrupts can cause jitter
- Cache misses add unpredictable delays
- Difficult to maintain 100ns precision

### Example Timing Calculation

isoSPI master transmit bit '1':
```pio
set pins, TX_HIGH [14]   ; 15 cycles = 100ns HIGH
set pins, TX_LOW [14]    ; 15 cycles = 100ns LOW
set pins, TX_IDLE [19]   ; 20 cycles = 133ns IDLE
```

Total: 100ns + 100ns + 133ns = 333ns per bit ≈ 3 Mbps symbol rate

But we also receive during this time, and the protocol has gaps,
resulting in effective 1 Mbps data rate.

## Common Patterns

### Edge Detection
```pio
.wrap_target
    wait 1 pin 0        ; Wait for rising edge
    irq set 0           ; Signal that edge was detected
    wait 0 pin 0        ; Wait for falling edge
.wrap
```

### Timeout/Timer
```pio
.wrap_target
    wait 1 irq 1        ; Wait for trigger
    nop [31]            ; Delay
    nop [31]            ; More delay
    irq set 0           ; Signal timeout
.wrap
```

### Sampling with Diagnostic
```pio
.side_set 1

.wrap_target
    wait 1 irq 0 side 0      ; Wait for trigger, diagnostic LOW
    mov x, pins side 1 [10]  ; Sample, diagnostic HIGH, delay
    in x, 2 side 0           ; Push to FIFO, diagnostic LOW
.wrap
```

## Best Practices

### Synchronizing Multiple State Machines

Use IRQ flags for coordination:
1. **Producer-Consumer:** SM1 sets IRQ when data ready, SM2 waits for IRQ
2. **Mutual Exclusion:** Use `irq wait` for atomic test-and-set
3. **Broadcast:** One SM sets IRQ, multiple SMs wait on it

### Debugging PIO Programs

1. **Sideset for Diagnostics:** Use sideset pin to visualize timing on oscilloscope
2. **Forced Jumps:** Jump to specific labels to test code paths
3. **Public Labels:** Export labels to C for inspection: `.PUBLIC label_name`
4. **FIFO Monitoring:** Check FIFO depth to detect overruns/underruns

### Avoiding Common Pitfalls

1. **Stall Detection:** If SM stalls (waiting for FIFO), it stops executing
   - Use `pio_sm_is_exec_stalled()` to detect
   - Ensure CPU reads/writes FIFO fast enough

2. **IRQ Flag Management:** Clear flags when done, or they stay set forever

3. **Pin Conflicts:** Multiple SMs writing same pin causes conflicts
   - Use SET pins for one purpose, OUT pins for another
   - Coordinate pin usage between SMs

4. **Program Memory:** Only 32 instructions per PIO block
   - Share programs between SMs when possible
   - Use `.wrap` to save jump instruction

## Summary

PIO is a powerful tool for precise I/O operations:
- **Deterministic timing:** Immune to interrupts and cache effects
- **Parallel execution:** Multiple SMs work simultaneously  
- **Efficient:** Offloads time-critical tasks from CPU
- **Flexible:** Can implement almost any protocol

For isoSPI, PIO provides:
- Exact 100ns pulse generation
- Precise sampling windows
- Edge detection without polling
- Concurrent TX/RX operation
- Passive bus monitoring (snooping)

Understanding PIO's instruction set, timing model, and synchronization
mechanisms is key to implementing reliable high-speed communication protocols.

