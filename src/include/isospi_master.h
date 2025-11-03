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
 */
bool isospi_write_read_blocking(char* out_buf, char* in_buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // ISOSPI_MASTER_H

