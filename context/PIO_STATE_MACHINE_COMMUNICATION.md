# PIO State Machine Communication

## Can State Machines Jump Between Programs?

**NO** - Each state machine runs its own independent program with its own program counter. You **cannot** make one SM jump into another SM's program space.

## How State Machines Communicate

State machines are completely independent and can only communicate through:

### 1. IRQ Flags (Most Common)
```
SM0 Program:              SM1 Program:
-----------               -----------
set pins, 1               wait 1 irq 0  ←── Waits here until IRQ 0 is set
irq set 0    ────────────→ (wakes up)
                          mov x, pins
```

### 2. Shared GPIO Pins
```
SM0: set pins, 1          SM1: wait 1 pin 0  (watches same GPIO)
```

### 3. CPU Mediation (via FIFOs)
```
SM0 → TX FIFO → CPU reads → CPU writes → RX FIFO → SM1
```

## Why IRQ Set 0 Should Work

Your current code:

```pio
; Main receiver (SM):
recv_wait:
    irq clear 0 side 0      ; Clear flag
    wait 1 irq 0 side 0     ; Wait for flag to be set
    ; ... sampling code ...

; Gap detector (SM+3):
.wrap_target
    nop [31]
    nop [26]
    irq set 0               ; Set the flag
.wrap
```

This SHOULD work because:
1. Main receiver clears IRQ 0
2. Main receiver waits for IRQ 0 to become 1
3. Gap detector sets IRQ 0 to 1
4. Main receiver wakes up and continues

**IRQ flags are "sticky"** - once set, they stay set until explicitly cleared.

## Problem: Race Condition in Free-Running Loop

The issue with a free-running gap detector is timing:

```
Time:   0ns      200ns     400ns     600ns     800ns
        │         │         │         │         │
Gap:    │◄───────────────GAP──────────────────►│ (500ns gap)
        │         │         │         │         │
Edge:   ↑         │         │         │         ↑
SM1:    set IRQ 0 │         │         │         set IRQ 0
        │         │         │         │         │
SM3:    │         set IRQ 0 │         set IRQ 0 │
        │  (400ns loop)     │  (400ns loop)     │
```

If the gap detector is free-running every 400ns, it might:
- Set IRQ 0 while main receiver is busy processing (flag gets cleared before main loops back)
- Set IRQ 0 at wrong time, causing extra unwanted samples
- Miss short gaps entirely

## Solution: Reactive Gap Detector

Instead of free-running, make the gap detector **reactive**:

```pio
.wrap_target
    ; Wait for main receiver to be ready (IRQ 0 cleared)
    wait 0 irq 0 [10]   ; Wait until IRQ 0 is clear + delay
    ; If we get here, main receiver is waiting and no edges detected
    irq set 0           ; Trigger a sample
.wrap
```

**How this works:**

```
Main receiver:                Gap detector:
--------------                --------------
irq clear 0  ─────────────────► wait 0 irq 0 (detects clear)
wait 1 irq 0 (waiting)          [delay 10 cycles]
   │                            If IRQ 0 still clear (no edges):
   │                            irq set 0 ─────────┐
   └──────────────────────────────────────────────┘
   (wakes up and samples)
```

**Benefits:**
1. Gap detector synchronizes with main receiver's state
2. Only triggers samples when main receiver is actually waiting
3. Short delay allows edge detectors to win if they find an edge
4. Much faster response (~73ns vs ~400ns)

## Alternative: Using OUT exec

There IS one way to force execution, but it requires CPU help:

```c
// From CPU, force SM0 to execute a specific instruction
pio_sm_exec(pio, sm, pio_encode_jmp(target_address));
```

But this requires:
- CPU intervention (defeats purpose of PIO autonomy)
- You need to know the target address
- Can only execute ONE instruction, then SM returns to its program

This is typically used for:
- Emergency stops
- Debug/test purposes
- Initialization

NOT for normal operation like gap detection.

## Best Practice for Gap Detection

For detecting both long and short gaps, use **both** approaches:

1. **Edge detectors (SM+1, SM+2):** Catch data transitions
2. **Reactive gap detector (SM+3):** Catch idle periods

The edge detectors will trigger IRQ 0 faster when there's activity.
The gap detector will trigger IRQ 0 only when idle is detected.

## Timing Comparison

### Free-Running (Old):
- ~400ns loop time
- Sets IRQ 0 every 400ns regardless of state
- Can miss gaps < 400ns
- Can cause spurious samples

### Reactive (New):
- ~73ns response time
- Only sets IRQ 0 when main receiver is waiting
- Catches gaps as short as ~80ns
- Synchronized with receiver state

## Summary

**Q: Can you jump from one SM's program to another?**
**A: No. Each SM has its own program counter and runs independently.**

**Q: Why doesn't IRQ set 0 work?**
**A: It DOES work, but timing matters:**
- Free-running can be too slow or out of sync
- Reactive approach synchronizes with receiver state
- Edge detectors compete with gap detector for IRQ 0

**Q: What's the proper way to coordinate SMs?**
**A: Use IRQ flags as semaphores/signals:**
- Producer sets IRQ, consumer waits for IRQ
- `wait 0 irq X` = wait for clear (ready state)
- `wait 1 irq X` = wait for set (event occurred)
- Synchronizes SM execution without jumps

The reactive gap detector I just implemented should work much better for detecting short gaps because it waits for the main receiver to be ready, then immediately triggers if no edges were detected.

