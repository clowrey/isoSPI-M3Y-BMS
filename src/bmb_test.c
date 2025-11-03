/**
 * @file bmb_test.c
 * @brief Unified BMB Test Implementation
 * 
 * Provides a unified test interface that works with both Batman SPI 
 * and isoSPI PIO interfaces. Can run tests once or continuously.
 */

#include "bmb_test.h"
#include "isospi_interface.h"
#include "batman.h"
#include "isospi_master.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// Test control
static bool continuous_enabled = false;
static uint32_t last_test_time = 0;
static const uint32_t TEST_INTERVAL_MS = 2000;  // 2 seconds

// CL: BATMan Protocol Commands (from batman.c)
#define CMD_WAKEUP      0x2AD4
#define CMD_MUTE        0x20DD
#define CMD_IDLE_WAKE   0x21F2  // Also called UNMUTE - makes IC responsive
#define CMD_SNAPSHOT    0x2BFB
#define CMD_READ_A      0x47    // Cells 0-2

// CL: CRC8 lookup table for command PEC (from batman.c)
static const uint8_t crc_table[256] = {
    0x00, 0x2F, 0x5E, 0x71, 0xBC, 0x93, 0xE2, 0xCD, 0x57, 0x78, 0x09, 0x26, 0xEB, 0xC4, 0xB5, 0x9A,
    0xAE, 0x81, 0xF0, 0xDF, 0x12, 0x3D, 0x4C, 0x63, 0xF9, 0xD6, 0xA7, 0x88, 0x45, 0x6A, 0x1B, 0x34,
    0x73, 0x5C, 0x2D, 0x02, 0xCF, 0xE0, 0x91, 0xBE, 0x24, 0x0B, 0x7A, 0x55, 0x98, 0xB7, 0xC6, 0xE9,
    0xDD, 0xF2, 0x83, 0xAC, 0x61, 0x4E, 0x3F, 0x10, 0x8A, 0xA5, 0xD4, 0xFB, 0x36, 0x19, 0x68, 0x47,
    0xE6, 0xC9, 0xB8, 0x97, 0x5A, 0x75, 0x04, 0x2B, 0xB1, 0x9E, 0xEF, 0xC0, 0x0D, 0x22, 0x53, 0x7C,
    0x48, 0x67, 0x16, 0x39, 0xF4, 0xDB, 0xAA, 0x85, 0x1F, 0x30, 0x41, 0x6E, 0xA3, 0x8C, 0xFD, 0xD2,
    0x95, 0xBA, 0xCB, 0xE4, 0x29, 0x06, 0x77, 0x58, 0xC2, 0xED, 0x9C, 0xB3, 0x7E, 0x51, 0x20, 0x0F,
    0x3B, 0x14, 0x65, 0x4A, 0x87, 0xA8, 0xD9, 0xF6, 0x6C, 0x43, 0x32, 0x1D, 0xD0, 0xFF, 0x8E, 0xA1,
    0xE3, 0xCC, 0xBD, 0x92, 0x5F, 0x70, 0x01, 0x2E, 0xB4, 0x9B, 0xEA, 0xC5, 0x08, 0x27, 0x56, 0x79,
    0x4D, 0x62, 0x13, 0x3C, 0xF1, 0xDE, 0xAF, 0x80, 0x1A, 0x35, 0x44, 0x6B, 0xA6, 0x89, 0xF8, 0xD7,
    0x90, 0xBF, 0xCE, 0xE1, 0x2C, 0x03, 0x72, 0x5D, 0xC7, 0xE8, 0x99, 0xB6, 0x7B, 0x54, 0x25, 0x0A,
    0x3E, 0x11, 0x60, 0x4F, 0x82, 0xAD, 0xDC, 0xF3, 0x69, 0x46, 0x37, 0x18, 0xD5, 0xFA, 0x8B, 0xA4,
    0x05, 0x2A, 0x5B, 0x74, 0xB9, 0x96, 0xE7, 0xC8, 0x52, 0x7D, 0x0C, 0x23, 0xEE, 0xC1, 0xB0, 0x9F,
    0xAB, 0x84, 0xF5, 0xDA, 0x17, 0x38, 0x49, 0x66, 0xFC, 0xD3, 0xA2, 0x8D, 0x40, 0x6F, 0x1E, 0x31,
    0x76, 0x59, 0x28, 0x07, 0xCA, 0xE5, 0x94, 0xBB, 0x21, 0x0E, 0x7F, 0x50, 0x9D, 0xB2, 0xC3, 0xEC,
    0xD8, 0xF7, 0x86, 0xA9, 0x64, 0x4B, 0x3A, 0x15, 0x8F, 0xA0, 0xD1, 0xFE, 0x33, 0x1C, 0x6D, 0x42
};

/**
 * @brief Calculate CRC8 for BatMan commands
 */
static uint8_t bmb_calc_crc(uint8_t *data, uint8_t length) {
    uint8_t crc = 0x10;  // Initial value
    for (uint8_t i = 0; i < length; i++) {
        uint8_t temp = crc ^ data[i];
        crc = crc_table[temp];
    }
    return crc;
}

/**
 * @brief Run BMB test once using active interface
 */
void bmb_test_run_once(void) {
    interface_type_t active = isospi_interface_get_active();
    
    if (active == INTERFACE_BATMAN) {
        printf("\n=== Running BMB Test (Batman SPI) ===\n");
        batman_run_test_once();
    } else {
        printf("\n=== Running BMB Test (isoSPI PIO) ===\n");
        
        // CL: Same command sequence as batman_simple_test()
        // Command sequence for LTC6811 (Tesla BMB chip):
        // 1. WAKEUP (0x2AD4)
        // 2. IDLE_WAKE (0x21F2)
        // 3. SNAPSHOT (0x2BFB)
        // 4. READ_A (0x47) with CRC
        
        bool valid;
        char tx[2], rx[2];
        
        // ============================================================
        // PHASE 1: Send all commands with precise timing
        // ============================================================
        
        printf("Sending command sequence via isoSPI...\n\n");
        
        // Command 1: WAKEUP
        printf("1. WAKEUP command:     0x%04X\n", CMD_WAKEUP);
        printf("   Byte 1: 0x%02X = 0b%08b\n", (CMD_WAKEUP >> 8) & 0xFF, (CMD_WAKEUP >> 8) & 0xFF);
        printf("   Byte 2: 0x%02X = 0b%08b\n", CMD_WAKEUP & 0xFF, CMD_WAKEUP & 0xFF);
        
        tx[0] = (CMD_WAKEUP >> 8) & 0xFF;
        tx[1] = CMD_WAKEUP & 0xFF;
        valid = isospi_write_read_blocking(tx, rx, 2);
        sleep_us(120);  // CL: 120us delay after wakeup
        printf("   → Delay: 120 μs\n\n");
        
        // Command 2: IDLE_WAKE
        printf("2. IDLE_WAKE command:  0x%04X\n", CMD_IDLE_WAKE);
        printf("   Byte 1: 0x%02X = 0b%08b\n", (CMD_IDLE_WAKE >> 8) & 0xFF, (CMD_IDLE_WAKE >> 8) & 0xFF);
        printf("   Byte 2: 0x%02X = 0b%08b\n", CMD_IDLE_WAKE & 0xFF, CMD_IDLE_WAKE & 0xFF);
        
        tx[0] = (CMD_IDLE_WAKE >> 8) & 0xFF;
        tx[1] = CMD_IDLE_WAKE & 0xFF;
        valid = isospi_write_read_blocking(tx, rx, 2);
        sleep_us(70);  // CL: 70us delay after idle_wake
        printf("   → Delay: 70 μs\n\n");
        
        // Command 3: SNAPSHOT
        printf("3. SNAPSHOT command:   0x%04X\n", CMD_SNAPSHOT);
        printf("   Byte 1: 0x%02X = 0b%08b\n", (CMD_SNAPSHOT >> 8) & 0xFF, (CMD_SNAPSHOT >> 8) & 0xFF);
        printf("   Byte 2: 0x%02X = 0b%08b\n", CMD_SNAPSHOT & 0xFF, CMD_SNAPSHOT & 0xFF);
        
        tx[0] = (CMD_SNAPSHOT >> 8) & 0xFF;
        tx[1] = CMD_SNAPSHOT & 0xFF;
        valid = isospi_write_read_blocking(tx, rx, 2);
        printf("   → No delay\n\n");
        
        // Command 4: READ_A with CRC
        uint8_t crc = bmb_calc_crc((uint8_t[]){CMD_READ_A, 0x00}, 2);
        printf("4. READ_A command:     0x%02X00\n", CMD_READ_A);
        printf("   Byte 1: 0x%02X = 0b%08b (command)\n", CMD_READ_A, CMD_READ_A);
        printf("   Byte 2: 0x00 = 0b00000000\n");
        printf("   Byte 3: 0x%02X = 0b%08b (CRC)\n", crc, crc);
        printf("   Byte 4: 0x00 = 0b00000000\n");
        
        // Send command word
        tx[0] = CMD_READ_A;
        tx[1] = 0x00;
        valid = isospi_write_read_blocking(tx, rx, 2);
        
        // Send CRC word
        tx[0] = crc;
        tx[1] = 0x00;
        valid = isospi_write_read_blocking(tx, rx, 2);
        
        // Read 72 bytes of response (would need larger buffer for full implementation)
        printf("   → Reading response data...\n");
        printf("   (Note: Full 72-byte response parsing not implemented yet)\n");
        
        printf("\n======================================\n\n");
    }
}

/**
 * @brief Enable continuous test mode
 */
void bmb_test_set_continuous(bool enabled) {
    continuous_enabled = enabled;
    
    if (enabled) {
        printf("BMB Test: Continuous mode ENABLED (test every %d seconds)\n", 
               TEST_INTERVAL_MS / 1000);
        last_test_time = to_ms_since_boot(get_absolute_time());
    } else {
        printf("BMB Test: Continuous mode DISABLED\n");
    }
}

/**
 * @brief Check if continuous test mode is enabled
 */
bool bmb_test_is_continuous(void) {
    return continuous_enabled;
}

/**
 * @brief Loop function for continuous testing
 */
void bmb_test_loop(void) {
    if (!continuous_enabled) {
        return;
    }
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (now - last_test_time >= TEST_INTERVAL_MS) {
        last_test_time = now;
        bmb_test_run_once();
    }
}

