/**
 * @file isospi_master.h
 * @brief isoSPI Master Interface using PIO
 * 
 * PIO-based isoSPI master for differential Manchester-like signaling
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
 */
void isospi_master_setup(uint tx_pin_base, uint rx_pin_base);

/**
 * @brief Perform blocking write-read transaction
 * @param out_buf Output buffer to transmit
 * @param in_buf Input buffer to receive
 * @param len Length of transaction in bytes
 * @return true if received data was valid, false if errors detected
 * 
 * Note: Includes automatic CS pulse generation by PIO.
 * Use isospi_set_cs_delay() to adjust post-CS timing.
 */
bool isospi_write_read_blocking(char* out_buf, char* in_buf, size_t len);

/**
 * @brief Invert the first chip select pulse-pair
 * @param invert true to send CS1 CS0 (for Batman wakeup), false for normal CS0 CS1
 * 
 * This modifies the PIO instruction memory to swap the first two chip select pulses.
 * Batman ICs require an inverted CS pattern (CS1 CS0) for the wakeup command.
 */
void isospi_invert_first_chip_select(bool invert);

/**
 * @brief Set the CS front porch delay (wait after CS assertion)
 * @param delay_cycles Number of PIO cycles (~255 for 5us with current clock)
 * 
 * Controls timing-critical delay between CS assertion and data transmission.
 * Batman IC requires ~5us delay after CS to wake up the SPI interface.
 */
void isospi_set_cs_delay(uint8_t delay_cycles);

/**
 * @brief Get the current CS front porch delay value
 * @return Current delay in PIO cycles
 */
uint8_t isospi_get_cs_delay(void);

#ifdef __cplusplus
}
#endif

#endif // ISOSPI_MASTER_H

