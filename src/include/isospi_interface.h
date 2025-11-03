/**
 * @file isospi_interface.h
 * @brief High-level isoSPI Interface Management
 * 
 * Manages switching between Batman SPI and isoSPI PIO interfaces
 */

#ifndef ISOSPI_INTERFACE_H
#define ISOSPI_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Interface selection
typedef enum {
    INTERFACE_BATMAN = 0,
    INTERFACE_ISOSPI = 1
} interface_type_t;

/**
 * @brief Initialize isoSPI interface (master + snooper)
 * @return true if initialization successful
 */
bool isospi_interface_init(void);

/**
 * @brief Enable isoSPI master (disables Batman)
 */
void isospi_interface_enable(void);

/**
 * @brief Enable Batman (disables isoSPI master)
 */
void isospi_interface_disable(void);

/**
 * @brief Get current active interface
 * @return Current interface type
 */
interface_type_t isospi_interface_get_active(void);

/**
 * @brief Run test pattern on isoSPI interface
 */
void isospi_interface_test(void);

/**
 * @brief Print captured snooper data
 */
void isospi_interface_print_snoop(void);

/**
 * @brief Print interface status
 */
void isospi_interface_print_status(void);

#ifdef __cplusplus
}
#endif

#endif // ISOSPI_INTERFACE_H

