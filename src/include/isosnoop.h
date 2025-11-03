/**
 * @file isosnoop.h
 * @brief isoSPI Bus Snooper using PIO
 * 
 * Passive monitoring of isoSPI bus traffic using DMA ring buffer
 */

#ifndef ISOSNOOP_H
#define ISOSNOOP_H

#include <stdint.h>
#include "pico/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize isoSPI snooper interface
 * @param rx_pin_base Base pin for RX (high on base, low on base+1)
 * @param sampling_pin Pin for sampling debug output
 */
void isosnoop_setup(uint rx_pin_base, uint sampling_pin);

/**
 * @brief Print captured bus traffic buffer
 * 
 * Decodes and prints captured isoSPI traffic to console
 */
void isosnoop_print_buffer(void);

/**
 * @brief Get snooper statistics for diagnostics
 * @param buffer_addr Pointer to store buffer address
 * @param dma_addr Pointer to store current DMA write address
 * @param pio_running Pointer to store PIO running status
 */
void isosnoop_get_stats(uint32_t *buffer_addr, uint32_t *dma_addr, bool *pio_running);

/**
 * @brief Read raw pin states for diagnostics
 * @return Current state of GP9 and GP10 as 2-bit value
 */
uint8_t isosnoop_read_pins(void);

#ifdef __cplusplus
}
#endif

#endif // ISOSNOOP_H

