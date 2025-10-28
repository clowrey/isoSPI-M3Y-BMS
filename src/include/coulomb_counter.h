/**
 * @file coulomb_counter.h
 * @brief Coulomb Counter for State of Charge Calculation
 * 
 * Simplified implementation using INA228's hardware energy accumulation
 * with software-based state of charge tracking.
 */

#ifndef COULOMB_COUNTER_H
#define COULOMB_COUNTER_H

#include <stdint.h>
#include <stdbool.h>
#include "ina228.h"

#ifdef __cplusplus
extern "C" {
#endif

// Default battery parameters
#define DEFAULT_BATTERY_CAPACITY_AH     75.0f   // Tesla Model 3 Standard Range
#define DEFAULT_FULLY_CHARGED_VOLTAGE   4.2f    // Per cell
#define DEFAULT_CURRENT_EFFICIENCY      0.95f   // 95% charging efficiency

// Coulomb counter structure
typedef struct {
    ina228_t *ina228;                    // Pointer to INA228 device
    
    // Battery parameters
    float total_capacity_ah;             // Total battery capacity in Ah
    float remaining_capacity_ah;         // Remaining capacity in Ah
    float state_of_charge_percent;       // SOC percentage (0-100%)
    float fully_charged_voltage;         // Fully charged cell voltage
    float current_efficiency;            // Charging efficiency factor
    
    // Measurement data
    float current;                       // Current in Amperes
    float voltage;                       // Pack voltage in Volts
    float power;                         // Power in Watts
    float energy_wh;                     // Energy in Wh
    float energy_kwh;                    // Energy in kWh
    
    // Timing
    uint32_t last_update_time;           // Last update timestamp
    
    // Status
    bool initialized;
} coulomb_counter_t;

// Initialization
bool coulomb_counter_init(coulomb_counter_t *cc, ina228_t *ina228);

// Update function (call periodically)
void coulomb_counter_update(coulomb_counter_t *cc, float pack_voltage);

// Configuration
void coulomb_counter_set_capacity(coulomb_counter_t *cc, float capacity_ah);
void coulomb_counter_set_fully_charged_voltage(coulomb_counter_t *cc, float voltage);
void coulomb_counter_set_efficiency(coulomb_counter_t *cc, float efficiency);

// Reset functions
void coulomb_counter_reset(coulomb_counter_t *cc);
void coulomb_counter_set_soc(coulomb_counter_t *cc, float soc_percent);

// Getters
float coulomb_counter_get_current(coulomb_counter_t *cc);
float coulomb_counter_get_power(coulomb_counter_t *cc);
float coulomb_counter_get_energy_wh(coulomb_counter_t *cc);
float coulomb_counter_get_energy_kwh(coulomb_counter_t *cc);
float coulomb_counter_get_soc(coulomb_counter_t *cc);
float coulomb_counter_get_remaining_capacity(coulomb_counter_t *cc);

// Status and diagnostics
void coulomb_counter_print_status(coulomb_counter_t *cc);

#ifdef __cplusplus
}
#endif

#endif // COULOMB_COUNTER_H

