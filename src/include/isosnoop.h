/**
 * @file isosnoop.h
 * @brief isoSPI Bus Snooper with GPIO inversion support
 */

#ifndef ISOSNOOP_H
#define ISOSNOOP_H

#include <stdint.h>

/**
 * @brief Initialize the isoSPI snooper
 * 
 * @param rx_pin_base Base GPIO pin for differential input (e.g., GP9)
 * @param invert Set to 1 to invert GPIO inputs in hardware, 0 for no inversion
 * @param sampling_pin GPIO pin for diagnostic output (shows sampling points on scope)
 */
void isosnoop_setup(unsigned int rx_pin_base, int invert, unsigned int sampling_pin);

/**
 * @brief Print captured isoSPI data from ring buffer
 * 
 * Call this periodically (e.g., in main loop) to display any new captured data.
 * Output format: "CS1", "CS0", "1", "0", "_" for each Manchester symbol
 */
void isosnoop_print_buffer();

#endif // ISOSNOOP_H
