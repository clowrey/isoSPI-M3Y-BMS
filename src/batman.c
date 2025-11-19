/**
 * @file batman.c
 * @brief BATMan BMS Interface Implementation (Stub)
 * 
 * This is a stub implementation that provides the basic framework.
 * The full BATMan protocol implementation from the original ESP32
 * code will be ported in a later phase.
 */

#include "batman.h"
#include "param.h"
#include "pin_config.h"
#include "isospi_master.h"  // CL: For alternating isoSPI master tests
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// BATMan Protocol Commands (from original BatMan.cpp)
#define CMD_WAKEUP      0x2AD4
#define CMD_MUTE        0x20DD
#define CMD_IDLE_WAKE   0x21F2  // Also called UNMUTE - makes IC responsive
#define CMD_SNAPSHOT    0x2BFB
#define CMD_READ_A      0x47    // Cells 0-2
#define CMD_READ_B      0x48    // Cells 3-5
#define CMD_READ_C      0x49    // Cells 6-8
#define CMD_READ_D      0x4A    // Cells 9-11
#define CMD_READ_E      0x4B    // Cells 12-14
#define CMD_READ_F      0x4C    // Chip total voltage
#define CMD_READ_AUX_A  0x4D    // Aux register A
#define CMD_READ_AUX_B  0x4E    // Aux register B
#define CMD_READ_STATUS 0x4F    // Status register
#define CMD_READ_CONFIG 0x50    // Config register
#define CMD_WRITE_CONFIG 0x11   // Write config

// BATMan state
static batman_state_t current_state = BATMAN_STATE_INIT;
static bool balance_enabled = false;
static bool register_debug_enabled = false;
static uint16_t loop_state = 0;
static uint16_t idle_count = 0;
static bool batman_enabled = true;  // CL: Control whether batman loop runs

// Cell voltage storage (8 BMBs x 15 cells max)
static uint16_t cell_voltages[MAX_CELLS];
static uint16_t voltage_array[8][15];  // Raw voltage data per BMB
static uint8_t cells_present = 0;

// Voltage statistics
static uint16_t min_voltage = 0;
static uint16_t max_voltage = 0;
static uint8_t min_cell = 0;
static uint8_t max_cell = 0;

// BMB connectivity
static uint8_t actual_bmb_count = 0;
static uint8_t expected_bmb_count = MAX_BMBS;
static uint16_t bmb_connected_mask = 0;

// Balancing information
static balancing_info_t balancing_info = {0};

// Response buffer
static uint8_t response_buffer[72];

/**
 * @brief Initialize BATMan interface
 */
bool batman_init(void) {
    printf("BATMan: Initializing...\n");
    
    // Initialize SPI for Tesla BMS
    spi_init(TESLA_BMS_SPI_INST, TESLA_BMS_SPI_FREQ);
    
    // Set SPI format: 8 bits, Mode 0 (CPOL=0, CPHA=0), MSB first
    // NOTE: Original STM32 code uses Mode 3, but we got better results with Mode 0
    // The RP2350 SPI may handle clock differently than STM32
    printf("BATMan: Using SPI Mode 0 (CPOL=0, CPHA=0)\n");
    spi_set_format(TESLA_BMS_SPI_INST, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // Initialize SPI pins
    gpio_set_function(TESLA_BMS_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(TESLA_BMS_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(TESLA_BMS_PIN_SCK, GPIO_FUNC_SPI);
    
    // CS pin as GPIO output
    gpio_init(TESLA_BMS_PIN_CS);
    gpio_set_dir(TESLA_BMS_PIN_CS, GPIO_OUT);
    gpio_put(TESLA_BMS_PIN_CS, 1);  // CS high (inactive)
    
    // BMB ENABLE pin as GPIO output (CRITICAL - BMS won't respond without this!)
    // NOTE: ESP32 reference sets this LOW (0), not HIGH
    gpio_init(TESLA_BMS_PIN_ENABLE);
    gpio_set_dir(TESLA_BMS_PIN_ENABLE, GPIO_OUT);
    gpio_put(TESLA_BMS_PIN_ENABLE, 0);  // Enable LOW (matching ESP32 implementation)
    printf("BATMan: BMB_ENABLE pin set LOW on GP%d (matching ESP32)\n", TESLA_BMS_PIN_ENABLE);
    
    printf("BATMan: SPI initialized - MISO=%d, MOSI=%d, SCK=%d, CS=%d, ENABLE=%d\n",
           TESLA_BMS_PIN_MISO, TESLA_BMS_PIN_MOSI, TESLA_BMS_PIN_SCK, TESLA_BMS_PIN_CS, TESLA_BMS_PIN_ENABLE);
    
    // Initialize cell voltages to zero
    memset(cell_voltages, 0, sizeof(cell_voltages));
    
    current_state = BATMAN_STATE_IDLE;
    
    printf("BATMan: Initialized successfully\n");
    printf("BATMan: NOTE - This is a stub implementation\n");
    printf("BATMan: Full BMS protocol will be ported in Phase 3\n");
    
    return true;
}

// CRC8 lookup table for command PEC (from BatMan.cpp)
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
static uint8_t batman_calc_crc(uint8_t *data, uint8_t length) {
    uint8_t crc = 0x10;  // Initial value
    for (uint8_t i = 0; i < length; i++) {
        uint8_t temp = crc ^ data[i];
        crc = crc_table[temp];
    }
    return crc;
}

/**
 * @brief Send wakeup command to BMBs (from BatMan.cpp)
 */
static void batman_wakeup(void) {
    uint16_t wakeup_cmd = CMD_WAKEUP;
    
    // Send wakeup once (testing single wakeup) - NO PRINTF for precise timing
    gpio_put(TESLA_BMS_PIN_CS, 0);
    sleep_us(5);
    
    uint8_t cmd_bytes[2];
    cmd_bytes[0] = (wakeup_cmd >> 8) & 0xFF;
    cmd_bytes[1] = wakeup_cmd & 0xFF;
    spi_write_blocking(TESLA_BMS_SPI_INST, cmd_bytes, 2);
    
    gpio_put(TESLA_BMS_PIN_CS, 1);
    sleep_us(10);
}

/**
 * @brief Send a command once (from BatMan.cpp Generic_Send_Once)
 * Commands like SNAPSHOT (0x2BFB) and UNMUTE (0x21F2) are 16-bit
 */
static void batman_send_command(uint16_t cmd_word) {
    // NO PRINTF for precise timing
    
    gpio_put(TESLA_BMS_PIN_CS, 0);
    sleep_us(5); //CL - 5us needed to wakeup the master - or it will miss the first clock cycles
    // CL - first SCK to CS timing will sync with the clock - so jitter of 1 SCK cycle is expected
    //for(int i=0;i<100;i++) asm("");
    
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
    
    tx_buf[0] = (cmd_word >> 8) & 0xFF;
    tx_buf[1] = cmd_word & 0xFF;
    
    // Send command and read response
    spi_write_read_blocking(TESLA_BMS_SPI_INST, tx_buf, rx_buf, 2);
    
   // sleep_us(10);
    gpio_put(TESLA_BMS_PIN_CS, 1);
}

/**
 * @brief Send idle wake command - makes IC responsive to commands
 * Also known as UNMUTE in some documentation
 */
static void batman_idle_wake(void) {
    batman_send_command(CMD_IDLE_WAKE);
}

/**
 * @brief Send a command once via isoSPI Master (PIO)
 * isoSPI equivalent of batman_send_command()
 * Handles CS timing externally for precise control
 */
static void isospi_send_command(uint16_t cmd_word) {
    // NO PRINTF for precise timing
    
    // Note: CS pulses are generated by the PIO program automatically
    // The cs_front_porch delay in the PIO handles the post-CS timing
    
    char tx[2], rx[2];
    tx[0] = (cmd_word >> 8) & 0xFF;
    tx[1] = cmd_word & 0xFF;
    
    isospi_write_read_blocking(tx, rx, 2);
}

/**
 * @brief Send wakeup command via isoSPI Master (PIO)
 * isoSPI equivalent of batman_wakeup()
 * Handles CS timing and inversion
 */
static void isospi_wakeup(void) {
    // Wakeup requires inverted CS: CS1 CS0
    // The PIO program generates the CS pulses with the inverted pattern
    isospi_invert_first_chip_select(true);
    
    char tx[2], rx[2];
    tx[0] = (CMD_WAKEUP >> 8) & 0xFF;
    tx[1] = CMD_WAKEUP & 0xFF;
    
    isospi_write_read_blocking(tx, rx, 2);
    
    // Restore normal CS pattern for subsequent commands
    isospi_invert_first_chip_select(false);
}

/**
 * @brief Read register with command and CRC via isoSPI Master (PIO)
 * isoSPI equivalent of batman_get_data()
 * Each call generates its own CS pulses via PIO
 */
static void isospi_get_data(uint8_t reg_cmd) {
    uint8_t temp_data[2] = {reg_cmd, 0x00};
    uint8_t crc = batman_calc_crc(temp_data, 2);
    
    // NO PRINTF for precise timing
    
    // Initialize buffer to 0x00
    memset(response_buffer, 0x00, 72);
    
    char tx[2], rx[2];
    
    // Send command (PIO generates CS pulses)
    tx[0] = reg_cmd;
    tx[1] = 0x00;
    isospi_write_read_blocking(tx, rx, 2);
    
    // Send CRC (PIO generates CS pulses)
    tx[0] = crc;
    tx[1] = 0x00;
    isospi_write_read_blocking(tx, rx, 2);
    
    // Read 12 bytes of response in one continuous transaction (PIO generates CS pulses)
    char tx_read[12] = {0};  // Send 12 zero bytes
    char read_buf[12] = {0};  // Receive 12 bytes of data
    isospi_write_read_blocking(tx_read, read_buf, 12);
    
    // Copy to response_buffer (same format as batman_get_data)
    memcpy(response_buffer, read_buf, 12);
}

/**
 * @brief Read register with command and CRC (from BatMan.cpp GetData)
 * 
 * Original uses 16-bit SPI transfers. Data comes back with high byte first!
 * Each 16-bit transfer: [DATA_HIGH][DATA_LOW]
 */
static void batman_get_data(uint8_t reg_cmd) {
    uint8_t temp_data[2] = {reg_cmd, 0x00};
    uint8_t crc = batman_calc_crc(temp_data, 2);
    
    // Build 16-bit words like original: [reg_cmd][0x00] and [crc][0x00]
    uint16_t cmd_word = (reg_cmd << 8);  // 0x4700 for Read A
    uint16_t crc_word = (crc << 8);      // 0x7000 for Read A CRC
    
    // NO PRINTF for precise timing
    
    // Initialize buffer to 0x00 (not 0xFF) to see if data comes in
    memset(response_buffer, 0x00, 72);
    
    // CS low
    gpio_put(TESLA_BMS_PIN_CS, 0);
    //sleep_us(10);  // Small delay after CS
    
    // Send first 16-bit word (command)
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
    
    tx_buf[0] = (cmd_word >> 8) & 0xFF;  // High byte first
    tx_buf[1] = cmd_word & 0xFF;          // Low byte
    spi_write_read_blocking(TESLA_BMS_SPI_INST, tx_buf, rx_buf, 2);
    
    // Send second 16-bit word (CRC)
    tx_buf[0] = (crc_word >> 8) & 0xFF;
    tx_buf[1] = crc_word & 0xFF;
    spi_write_read_blocking(TESLA_BMS_SPI_INST, tx_buf, rx_buf, 2);
    
    // Read only 10 bytes (for 1 BMB + margin) instead of 72 bytes (for 8 BMBs)
    // This makes the transaction shorter and easier to capture with the snooper

    for (int count = 0; count <= 10; count += 2) {
        // Send 16-bit padding (0x0000)
        tx_buf[0] = 0x00;
        tx_buf[1] = 0x00;
        spi_write_read_blocking(TESLA_BMS_SPI_INST, tx_buf, rx_buf, 2);
        
        // Store response
        if (count < 72) {  // Don't overflow buffer
            response_buffer[count] = rx_buf[0];      // High byte
            response_buffer[count + 1] = rx_buf[1];  // Low byte
        }
    }
    
    // CS high
    //sleep_us(10);  // Small delay before CS
    gpio_put(TESLA_BMS_PIN_CS, 1);
}

/**
 * @brief Parse voltage data from register read (from BatMan.cpp GetData)
 */
static void batman_parse_voltages(uint8_t reg_cmd) {
    uint16_t tempvol = 0;
    
    // Parse based on register command (from original GetData switch statement)
    switch (reg_cmd) {
        case CMD_READ_A:  // 0x47 - Cells 0-2
            for (int h = 0; h < 8; h++) {  // 8 BMBs
                for (int g = 0; g <= 2; g++) {  // 3 cells
                    int idx = (h * 9) + (g * 2);
                    tempvol = response_buffer[idx + 1] * 256 + response_buffer[idx];
                    
                    if (tempvol != 0xFFFF && tempvol != 0x0000) {
                        voltage_array[h][g] = tempvol / 12.5;
                    }
                }
            }
            break;
            
        case CMD_READ_B:  // 0x48 - Cells 3-5
            for (int h = 0; h < 8; h++) {
                for (int g = 3; g <= 5; g++) {
                    tempvol = response_buffer[1 + (h * 9) + ((g - 3) * 2)] * 256 + response_buffer[0 + (h * 9) + ((g - 3) * 2)];
                    if (tempvol != 0xFFFF) {
                        voltage_array[h][g] = tempvol / 12.5;
                    }
                }
            }
            break;
            
        case CMD_READ_C:  // 0x49 - Cells 6-8
            for (int h = 0; h < 8; h++) {
                for (int g = 6; g <= 8; g++) {
                    tempvol = response_buffer[1 + (h * 9) + ((g - 6) * 2)] * 256 + response_buffer[0 + (h * 9) + ((g - 6) * 2)];
                    if (tempvol != 0xFFFF) {
                        voltage_array[h][g] = tempvol / 12.5;
                    }
                }
            }
            break;
            
        case CMD_READ_D:  // 0x4A - Cells 9-11
            for (int h = 0; h < 8; h++) {
                for (int g = 9; g <= 11; g++) {
                    tempvol = response_buffer[1 + (h * 9) + ((g - 9) * 2)] * 256 + response_buffer[0 + (h * 9) + ((g - 9) * 2)];
                    if (tempvol != 0xFFFF) {
                        voltage_array[h][g] = tempvol / 12.5;
                    }
                }
            }
            break;
            
        case CMD_READ_E:  // 0x4B - Cells 12-14
            for (int h = 0; h < 8; h++) {
                for (int g = 12; g <= 14; g++) {
                    tempvol = response_buffer[1 + (h * 9) + ((g - 12) * 2)] * 256 + response_buffer[0 + (h * 9) + ((g - 12) * 2)];
                    if (tempvol != 0xFFFF) {
                        voltage_array[h][g] = tempvol / 12.5;
                    }
                }
            }
            break;
    }
}

/**
 * @brief Update cell voltage parameters from voltage_array
 */
static void batman_update_cell_voltages(void) {
    uint8_t chip_num = 2;  // Number of BMBs * 2 (from original code)
    uint8_t xr = 0; // BMB number
    uint8_t yc = 0; // Cell voltage register number
    uint8_t hc = 0; // Cells present per chip
    uint8_t h = 0;  // Spot value index
    
    float cell_v_max = 0;
    float cell_v_min = 5000;
    
    while (h <= 100) {
        if (yc < 14) {  // Check actual measurement present
            if (voltage_array[xr][yc] > 10) {  // Check actual measurement present
                if (cell_v_max < voltage_array[xr][yc]) {
                    cell_v_max = voltage_array[xr][yc];
                    max_cell = h;
                    param_set_int(PARAM_CELL_MAX, h + 1);
                }
                if (cell_v_min > voltage_array[xr][yc]) {
                    cell_v_min = voltage_array[xr][yc];
                    min_cell = h;
                    param_set_int(PARAM_CELL_MIN, h + 1);
                }
                param_set_float((param_num_t)(PARAM_U1 + h), (float)voltage_array[xr][yc]);
                h++;  // next cell spot value along
                hc++; // one more cell present
            }
            yc++;  // next cell along
        } else {
            yc = 0;   // reset Cell column
            hc = 0;   // reset cell count per chip
            xr++;     // next BMB
        }
        
        if (xr == chip_num) {
            max_voltage = cell_v_max;
            min_voltage = cell_v_min;
            cells_present = h;
            
            param_set_float(PARAM_UMAX, cell_v_max);
            param_set_float(PARAM_UMIN, cell_v_min);
            param_set_float(PARAM_DELTA_V, cell_v_max - cell_v_min);
            param_set_int(PARAM_CELLS_PRESENT, h);
            
            printf("BATMan: Found %d cells, min=%d mV, max=%d mV\n", h, (int)cell_v_min, (int)cell_v_max);
            h = 100;
            break;
        }
    }
}

/**
 * @brief Batman SPI test - with precise timing (no printf between commands)
 * Uses traditional SPI interface to communicate with Batman IC
 */
static void batman_simple_test(void) {
    // ============================================================
    // PHASE 1: Send all commands with precise timing (no printing)
    // ============================================================
    
    // Step 1: Wakeup
    batman_wakeup();
    sleep_us(120);
    
    // Step 2: Unmute
    batman_send_command(CMD_IDLE_WAKE);
    sleep_us(70);
    
    // Step 3: Snapshot
    batman_send_command(CMD_SNAPSHOT);
    
    // Step 4: Read voltage register A
    batman_get_data(CMD_READ_A);
    
    // ============================================================
    // PHASE 2: Parse and display results (after all commands sent)
    // ============================================================
    
    printf("\n=== BMS READ COMPLETE ===\n");
    
    // Simplified output - just show BMB 0 cell voltages
    int valid_cells = 0;
    int bmb = 0;
    
    printf("BMB0: ");
    for (int cell = 0; cell <= 2; cell++) {  // Cells 0-2
        int idx = (bmb * 9) + (cell * 2);
        if (idx + 1 < 72) {
            uint16_t tempvol = response_buffer[idx + 1] * 256 + response_buffer[idx];
            
            if (tempvol != 0xFFFF && tempvol != 0x0000) {
                uint16_t voltage_mv = tempvol / 12.5;
                printf("C%d=%.3fV ", cell, voltage_mv / 1000.0f);
                
                // Store in voltage array
                voltage_array[bmb][cell] = voltage_mv;
                valid_cells++;
                
                // Update parameter for first cell
                if (cell == 0) {
                    param_set_float(PARAM_U1, (float)voltage_mv);
                }
            } else {
                printf("C%d=INV ", cell);
            }
        }
    }
    
    if (valid_cells > 0) {
        printf("✓\n");
    } else {
        printf("✗\n");
    }
}

/**
 * @brief isoSPI Master test - with precise timing (no printf between commands)
 * Uses PIO-based isoSPI master instead of Batman SPI
 */
static void isospi_simple_test(void) {
    // ============================================================
    // PHASE 1: Send all commands with precise timing (no printing)
    // ============================================================
    
    // Step 1: Wakeup
    isospi_wakeup();
    sleep_us(140); // CL - User tuned: 140us works
    
    // Step 2: Unmute
    isospi_send_command(CMD_IDLE_WAKE);
    sleep_us(90); // CL - User tuned: 90us works
    
    // Step 3: Snapshot
    isospi_send_command(CMD_SNAPSHOT);
    
    // Step 4: Read voltage register A
    isospi_get_data(CMD_READ_A);
    
    // ============================================================
    // PHASE 2: Parse and display results (after all commands sent)
    // ============================================================
    
    printf("\n=== BMS READ COMPLETE (isoSPI Master) ===\n");
    
    // Simplified output - just show BMB 0 cell voltages
    int valid_cells = 0;
    int bmb = 0;
    
    printf("BMB0: ");
    for (int cell = 0; cell <= 2; cell++) {  // Cells 0-2
        int idx = (bmb * 9) + (cell * 2);
        if (idx + 1 < 72) {
            uint16_t tempvol = response_buffer[idx + 1] * 256 + response_buffer[idx];
            
            if (tempvol != 0xFFFF && tempvol != 0x0000) {
                uint16_t voltage_mv = tempvol / 12.5;
                printf("C%d=%.3fV ", cell, voltage_mv / 1000.0f);
                
                // Store in voltage array
                voltage_array[bmb][cell] = voltage_mv;
                valid_cells++;
                
                // Update parameter for first cell
                if (cell == 0) {
                    param_set_float(PARAM_U1, (float)voltage_mv);
                }
            } else {
                printf("C%d=INV ", cell);
            }
        }
    }
    
    if (valid_cells > 0) {
        printf("✓\n");
    } else {
        printf("✗\n");
    }
}

/**
 * @brief Run isoSPI master test (same commands as Batman but via PIO)
 */


/**
 * @brief State machine - alternates between Batman SPI and isoSPI Master tests
 */
static void batman_state_machine(void) {
    switch (loop_state) {
        case 0:  // Run Batman SPI test
            batman_simple_test();
            loop_state++;
            idle_count = 0;
            break;
            
        case 1:  // Wait 2 seconds after Batman SPI test
            idle_count++;
            if (idle_count >= 20) {  // 2 seconds at 100ms loop
                loop_state = 2;  // Move to isoSPI test
                idle_count = 0;
            }
            break;
            
        case 2:  // Run isoSPI Master test (PIO-based)
            isospi_simple_test();
            loop_state++;
            idle_count = 0;
            break;
            
        case 3:  // Wait 2 seconds after isoSPI Master test
            idle_count++;
            if (idle_count >= 20) {  // 2 seconds at 100ms loop
                loop_state = 0;  // Loop back to Batman SPI test
                idle_count = 0;
            }
            break;
            
        default:
            loop_state = 0;
            break;
    }
}

/**
 * @brief Main BATMan loop (called every 100ms from main)
 */
void batman_loop(void) {
    // CL: Don't run if batman is disabled (e.g., when isoSPI is active)
    if (!batman_enabled) {
        return;
    }
    
    static uint32_t last_update = 0;
    static bool initialized = false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Run state machine every 100ms
    if (now - last_update >= 100) {
        last_update = now;
        
        if (!initialized) {
            printf("\n*** Alternating Test Mode ***\n");
            printf("*** Batman SPI → 2s → isoSPI Master (PIO) → 2s → repeat ***\n");
            printf("*** Both interfaces tested automatically ***\n\n");
            initialized = true;
        }
        
        batman_state_machine();
    }
}

/**
 * @brief Print hardware mapping information
 */
void batman_print_hardware_mapping(void) {
    printf("=== BATMan Hardware Mapping ===\n");
    printf("SPI Bus: SPI0\n");
    printf("  MISO: GP%d\n", TESLA_BMS_PIN_MISO);
    printf("  MOSI: GP%d\n", TESLA_BMS_PIN_MOSI);
    printf("  SCK:  GP%d\n", TESLA_BMS_PIN_SCK);
    printf("  CS:   GP%d\n", TESLA_BMS_PIN_CS);
    printf("  Frequency: %d Hz\n", TESLA_BMS_SPI_FREQ);
    printf("\nCells Present: %d\n", cells_present);
    printf("BMBs Detected: %d / %d\n", actual_bmb_count, expected_bmb_count);
    printf("State: %d\n", current_state);
    printf("==============================\n");
}

/**
 * @brief Get minimum voltage
 */
uint16_t batman_get_min_voltage(void) {
    return min_voltage;
}

/**
 * @brief Get maximum voltage
 */
uint16_t batman_get_max_voltage(void) {
    return max_voltage;
}

/**
 * @brief Get minimum cell number
 */
uint8_t batman_get_min_cell(void) {
    return min_cell;
}

/**
 * @brief Get maximum cell number
 */
uint8_t batman_get_max_cell(void) {
    return max_cell;
}

/**
 * @brief Get specific cell voltage
 */
uint16_t batman_get_cell_voltage(uint8_t cell_num) {
    if (cell_num == 0 || cell_num > MAX_CELLS) {
        return 0;
    }
    return cell_voltages[cell_num - 1];
}

/**
 * @brief Set balance enabled
 */
void batman_set_balance_enabled(bool enabled) {
    balance_enabled = enabled;
    printf("BATMan: Balance %s\n", enabled ? "ENABLED" : "DISABLED");
}

/**
 * @brief Get balance enabled status
 */
bool batman_get_balance_enabled(void) {
    return balance_enabled;
}

/**
 * @brief Get balancing information
 */
balancing_info_t batman_get_balancing_info(void) {
    // TODO: Implement actual balancing logic
    balancing_info.balancing_cells = 0;
    return balancing_info;
}

/**
 * @brief Get actual BMB count
 */
uint8_t batman_get_actual_bmb_count(void) {
    return actual_bmb_count;
}

/**
 * @brief Get expected BMB count
 */
uint8_t batman_get_expected_bmb_count(void) {
    return expected_bmb_count;
}

/**
 * @brief Get BMB connected mask
 */
uint16_t batman_get_bmb_connected_mask(void) {
    return bmb_connected_mask;
}

/**
 * @brief Set register debug
 */
void batman_set_register_debug(bool enabled) {
    register_debug_enabled = enabled;
    printf("BATMan: Register debug %s\n", enabled ? "ENABLED" : "DISABLED");
}

/**
 * @brief Get register debug status
 */
bool batman_get_register_debug(void) {
    return register_debug_enabled;
}

/**
 * @brief Get current state
 */
batman_state_t batman_get_state(void) {
    return current_state;
}

/**
 * @brief Set batman enabled/disabled
 */
void batman_set_enabled(bool enabled) {
    batman_enabled = enabled;
    printf("BATMan: %s\n", enabled ? "ENABLED" : "DISABLED");
}

/**
 * @brief Get batman enabled status
 */
bool batman_is_enabled(void) {
    return batman_enabled;
}

/**
 * @brief Run isoSPI test once (exported for bmb_test)
 * Note: Uses isoSPI Master (PIO) interface
 * To run Batman SPI test, call batman_simple_test() directly
 */
void batman_run_test_once(void) {
    isospi_simple_test();
}

