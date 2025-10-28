/**
 * @file batman.h
 * @brief BATMan BMS Interface for Tesla Model 3
 * 
 * Handles SPI communication with Tesla BMS modules.
 * Ported from ESP32/Arduino to RP2350/Pico SDK.
 */

#ifndef BATMAN_H
#define BATMAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// BATMan configuration
#define MAX_BMBS            9      // Maximum number of Battery Management Boards
#define CELLS_PER_BMB       12     // Cells per BMB
#define MAX_CELLS           108    // Total maximum cells (9 BMBs × 12 cells)

// BATMan state machine states
typedef enum {
    BATMAN_STATE_INIT = 0,
    BATMAN_STATE_IDLE,
    BATMAN_STATE_READING,
    BATMAN_STATE_BALANCING,
    BATMAN_STATE_ERROR
} batman_state_t;

// Balancing information structure
typedef struct {
    uint8_t balancing_cells;                    // Number of cells currently balancing
    uint8_t balancing_cell_numbers[MAX_CELLS];  // Array of cell numbers being balanced
} balancing_info_t;

// BATMan interface functions
bool batman_init(void);
void batman_loop(void);
void batman_print_hardware_mapping(void);

// Voltage reading functions
uint16_t batman_get_min_voltage(void);  // Returns voltage in mV
uint16_t batman_get_max_voltage(void);  // Returns voltage in mV
uint8_t batman_get_min_cell(void);      // Returns cell number (1-108)
uint8_t batman_get_max_cell(void);      // Returns cell number (1-108)
uint16_t batman_get_cell_voltage(uint8_t cell_num);  // Returns voltage in mV for specific cell

// Balancing functions
void batman_set_balance_enabled(bool enabled);
bool batman_get_balance_enabled(void);
balancing_info_t batman_get_balancing_info(void);

// BMB connectivity
uint8_t batman_get_actual_bmb_count(void);
uint8_t batman_get_expected_bmb_count(void);
uint16_t batman_get_bmb_connected_mask(void);

// Debug functions
void batman_set_register_debug(bool enabled);
bool batman_get_register_debug(void);

// State machine
batman_state_t batman_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // BATMAN_H

