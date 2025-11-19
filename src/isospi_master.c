/**
 * @file isospi_master.c
 * @brief isoSPI Master Interface using PIO
 * 
 * PIO-based isoSPI master for differential Manchester-like signaling.
 * Based on experimental version with bug fixes for RP2350.
 */

#include "isospi_master.h"
#include "isospi_master.pio.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Use PIO2 SM0 (PIO0=CAN, PIO1=isosnoop, PIO2=isoSPI master)
#define ISOSPI_MASTER_PIO pio2
#define ISOSPI_MASTER_SM 0

// Store the PIO program offset for instruction modification
static uint pio_program_offset = 0;

// CS front porch delay (tunable for different IC requirements)
static uint8_t cs_front_porch_delay = 255;  // Default ~5us

void isospi_master_setup(uint tx_pin_base, uint rx_pin_base) {
    // tx_pin_base      is the driver enable pin (active high)
    // tx_pin_base + 1  is the tx data pin

    // rx_pin_base      is the high rx data pin
    // rx_pin_base + 1  is the low rx data pin

    printf("isoSPI Master: Loading PIO program...\n");
    pio_program_offset = pio_add_program(ISOSPI_MASTER_PIO, &isospi_master_program);
    printf("isoSPI Master: PIO program loaded at offset %d\n", pio_program_offset);
    
    isospi_master_program_init(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, pio_program_offset, tx_pin_base, rx_pin_base);
    printf("isoSPI Master: PIO state machine configured\n");
    
    pio_sm_set_enabled(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, true);
    printf("isoSPI Master: State machine enabled\n");
    
    printf("isoSPI Master: Initialized on PIO2 SM0 (TX: GP%d-GP%d, RX: GP%d-GP%d)\n", 
           tx_pin_base, tx_pin_base + 1, rx_pin_base, rx_pin_base + 1);
}

bool isospi_write_read_blocking(char* out_buf, char* in_buf, size_t len) {
    // CL: Removed printf() - was causing variable delays in critical timing path
    
    // Send cs_front_porch delay value to PIO (wait after asserting CS)
    // This is timing-critical for Batman IC which needs ~5us to wake up SPI interface
    pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, cs_front_porch_delay << 24);

    sleep_us(1);

    bool valid = true;
    for(size_t i = 0; i < len; i++) {
        // We write 8 bits at a time
        pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, out_buf[i] << 24);

        // Each response bit is encoded as a nibble in a 32 bit word
        uint32_t v = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);
        for(int r = 0; r < 8; r++) {
            uint8_t nibble = (v >> 28) & 0x0f;  // Get top 4 bits as nibble (0-15)
            v <<= 4;
            if(nibble == 0b1001) {  // 9 = bit 1
                // bit 1
                in_buf[i] = (in_buf[i] << 1) | 0x1;
            } else if(nibble == 0b0110) {  // 6 = bit 0
                // bit 0
                in_buf[i] = (in_buf[i] << 1) | 0x0;
            } else {
                // invalid
                valid = false;
                in_buf[i] = (in_buf[i] << 1) | 0x0;
            }
        }
        //sleep_us(1); // - creates unnecessary gaps between bytes
    }

    //sleep_us(5);

    // Jump to end chip select sequence (instruction 29 from program start)
    // CRITICAL: Use relative offset, not absolute address!
    pio_sm_exec(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, pio_encode_jmp(pio_program_offset + 29)); 

    sleep_us(10);

    return valid;
}

void isospi_invert_first_chip_select(bool invert) {
    // Invert the first chip select pulse-pair. This is used to match the
    // wake-up behaviour of the Batman IC, which expects CS1 CS0 instead of CS0 CS1.
    
    // The chip select pattern is at instructions 1 and 2 of the PIO program:
    //   set pins TX_HIGH [CS_PULSE_LEN-1]   ; normally 0xe303 (Enable=1, Data=1)
    //   set pins TX_LOW  [CS_PULSE_LEN-1]   ; normally 0xe301 (Enable=1, Data=0)
    
    // CL: No need to disable/restart SM - just modify instructions
    // They will take effect on the next transaction when SM wraps around
    
    // Change the bit pattern of the first two 'set pins' instructions
    // TX_HIGH = 0b11 (value 3), TX_LOW = 0b01 (value 1)
    // CS_PULSE_LEN = 31, so delay = 30 (0x1E)
    // From compiled PIO: set pins, 1 [30] = 0xfe01, set pins, 3 [30] = 0xfe03
    // Normal order: TX_LOW, TX_HIGH = 0xfe01, 0xfe03 (CS0 CS1)
    // Inverted order: TX_HIGH, TX_LOW = 0xfe03, 0xfe01 (CS1 CS0)
    ISOSPI_MASTER_PIO->instr_mem[pio_program_offset + 1] = invert ? 0xfe03 : 0xfe01;
    ISOSPI_MASTER_PIO->instr_mem[pio_program_offset + 2] = invert ? 0xfe01 : 0xfe03;
}

void isospi_set_cs_delay(uint8_t delay_cycles) {
    cs_front_porch_delay = delay_cycles;
}

uint8_t isospi_get_cs_delay(void) {
    return cs_front_porch_delay;
}
