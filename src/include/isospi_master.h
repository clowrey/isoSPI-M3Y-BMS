/**
 * @file isospi_master.h
 * @brief isoSPI Master Interface using PIO - Advanced Multi-SM Implementation
 * 
 * PIO-based isoSPI master for differential Manchester-like signaling.
 * Uses 4 state machines with IRQ-based edge detection for robust communication.
 */

#ifndef ISOSPI_MASTER_H
#define ISOSPI_MASTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "pico/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize isoSPI master interface
 * @param tx_pin_base Base pin for TX (driver enable on base, data on base+1)
 * @param rx_pin_base Base pin for RX (high on base, low on base+1)
 * 
 * Initializes 4 state machines:
 *   SM0: Main transmit/receive
 *   SM1: RX pin 0 edge detector
 *   SM2: RX pin 1 edge detector
 *   SM3: Gap/timeout detector
 */
void isospi_master_setup(uint tx_pin_base, uint rx_pin_base);

/**
 * @brief Perform blocking write-read transaction
 * @param out_buf Output buffer to transmit
 * @param in_buf Input buffer to receive
 * @param len Length of transaction in bytes
 * @return true if received data was valid, false if errors detected
 * 
 * Note: Includes automatic CS pulse generation via new CS algorithm.
 */
bool isospi_write_read_blocking(unsigned char* out_buf, unsigned char* in_buf, size_t len);

/**
 * @brief Flush RX FIFO
 * 
 * Clears any remaining data from the PIO RX FIFO and ISR.
 */
void isospi_master_flush(void);

/**
 * @brief Tune isoSPI timing parameters
 * @param prescaler Clock prescaler value
 * @param cs_pulse_length CS pulse length in cycles
 * @param data_pulse_length Data pulse length in cycles
 * @param pre_rx_delay Delay before RX sampling
 * @param reply_wait Reply wait timeout
 * @param sample_pos_1 First sample position
 * @param sample_pos_2 Second sample position
 * @param post_rx_delay Delay after RX sampling
 * 
 * Dynamically adjusts timing parameters for optimal communication.
 */
void isospi_tune(
    uint32_t prescaler,
    uint8_t cs_pulse_length,
    uint8_t data_pulse_length,
    uint8_t pre_rx_delay,
    uint8_t reply_wait,
    uint8_t sample_pos_1,
    uint8_t sample_pos_2,
    uint8_t post_rx_delay
);

/**
 * @brief Run write tests
 * @param count Number of tests to run
 * @return Number of successful tests
 * 
 * Used for calibration and diagnostics.
 */
int isospi_write_tests(int count);

/**
 * @brief Auto-calibrate isoSPI timing
 * 
 * Automatically finds optimal timing parameters through iterative testing.
 */
void isospi_calibrate(void);

/**
 * @brief Send wakeup CS pulses (CS1 CS0 pattern)
 * 
 * Generates Batman IC wakeup sequence using inverted CS pattern.
 */
void isospi_wakeup(void);

/**
 * @brief Send a 16-bit command
 * @param cmd_word Command word to send (e.g., CMD_IDLE_WAKE, CMD_SNAPSHOT)
 * 
 * Sends 2 bytes with normal CS pattern.
 */
void isospi_send_command(uint16_t cmd_word);

/**
 * @brief Read data with command and CRC
 * @param reg_cmd Register read command byte (e.g., 0x47 for READ_A)
 * @param response_buffer Buffer to store response
 * @param response_len Number of bytes to read
 * 
 * Sends: [reg_cmd] [0x00] [0x70] [0x00] then reads response_len bytes continuously.
 */
void isospi_get_data(uint8_t reg_cmd, unsigned char* response_buffer, size_t response_len);

#ifdef __cplusplus
}
#endif

#endif // ISOSPI_MASTER_H
