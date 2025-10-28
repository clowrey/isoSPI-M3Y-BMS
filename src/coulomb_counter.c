/**
 * @file coulomb_counter.c
 * @brief Coulomb Counter Implementation
 */

#include "coulomb_counter.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <math.h>

/**
 * @brief Initialize coulomb counter
 */
bool coulomb_counter_init(coulomb_counter_t *cc, ina228_t *ina228) {
    if (!cc || !ina228) {
        return false;
    }
    
    cc->ina228 = ina228;
    
    // Initialize with default battery parameters
    cc->total_capacity_ah = DEFAULT_BATTERY_CAPACITY_AH;
    cc->remaining_capacity_ah = DEFAULT_BATTERY_CAPACITY_AH;
    cc->state_of_charge_percent = 100.0f;
    cc->fully_charged_voltage = DEFAULT_FULLY_CHARGED_VOLTAGE;
    cc->current_efficiency = DEFAULT_CURRENT_EFFICIENCY;
    
    // Initialize measurements
    cc->current = 0.0f;
    cc->voltage = 0.0f;
    cc->power = 0.0f;
    cc->energy_wh = 0.0f;
    cc->energy_kwh = 0.0f;
    
    // Initialize timing
    cc->last_update_time = to_ms_since_boot(get_absolute_time());
    
    cc->initialized = true;
    
    printf("Coulomb Counter: Initialized\n");
    printf("  Capacity: %.1f Ah\n", cc->total_capacity_ah);
    printf("  Fully Charged Voltage: %.2f V/cell\n", cc->fully_charged_voltage);
    printf("  Efficiency: %.0f%%\n", cc->current_efficiency * 100.0f);
    
    return true;
}

/**
 * @brief Update coulomb counter (call periodically)
 */
void coulomb_counter_update(coulomb_counter_t *cc, float pack_voltage) {
    if (!cc || !cc->initialized) {
        return;
    }
    
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    float time_delta_hours = (float)(current_time - cc->last_update_time) / 3600000.0f;
    
    // Read current measurements from INA228
    cc->current = ina228_read_current(cc->ina228);
    cc->power = ina228_read_power(cc->ina228);
    cc->voltage = pack_voltage;
    
    // Read hardware-accumulated energy
    cc->energy_wh = ina228_read_energy(cc->ina228);
    cc->energy_kwh = cc->energy_wh / 1000.0f;
    
    // Update state of charge based on current integration
    // Positive current = charging, negative = discharging
    float charge_delta_ah = cc->current * time_delta_hours;
    
    // Apply efficiency factor for charging
    if (cc->current > 0) {
        charge_delta_ah *= cc->current_efficiency;
    }
    
    // Update remaining capacity
    cc->remaining_capacity_ah += charge_delta_ah;
    
    // Clamp to valid range
    if (cc->remaining_capacity_ah > cc->total_capacity_ah) {
        cc->remaining_capacity_ah = cc->total_capacity_ah;
    } else if (cc->remaining_capacity_ah < 0.0f) {
        cc->remaining_capacity_ah = 0.0f;
    }
    
    // Calculate SOC percentage
    cc->state_of_charge_percent = (cc->remaining_capacity_ah / cc->total_capacity_ah) * 100.0f;
    
    // Clamp SOC to valid range
    if (cc->state_of_charge_percent > 100.0f) {
        cc->state_of_charge_percent = 100.0f;
    } else if (cc->state_of_charge_percent < 0.0f) {
        cc->state_of_charge_percent = 0.0f;
    }
    
    cc->last_update_time = current_time;
}

/**
 * @brief Set battery capacity
 */
void coulomb_counter_set_capacity(coulomb_counter_t *cc, float capacity_ah) {
    if (!cc || capacity_ah <= 0.0f) {
        return;
    }
    
    cc->total_capacity_ah = capacity_ah;
    printf("Coulomb Counter: Capacity set to %.1f Ah\n", capacity_ah);
}

/**
 * @brief Set fully charged voltage
 */
void coulomb_counter_set_fully_charged_voltage(coulomb_counter_t *cc, float voltage) {
    if (!cc || voltage <= 0.0f) {
        return;
    }
    
    cc->fully_charged_voltage = voltage;
    printf("Coulomb Counter: Fully charged voltage set to %.2f V\n", voltage);
}

/**
 * @brief Set charging efficiency
 */
void coulomb_counter_set_efficiency(coulomb_counter_t *cc, float efficiency) {
    if (!cc || efficiency <= 0.0f || efficiency > 1.0f) {
        return;
    }
    
    cc->current_efficiency = efficiency;
    printf("Coulomb Counter: Efficiency set to %.0f%%\n", efficiency * 100.0f);
}

/**
 * @brief Reset coulomb counter to 100% SOC
 */
void coulomb_counter_reset(coulomb_counter_t *cc) {
    if (!cc) {
        return;
    }
    
    cc->remaining_capacity_ah = cc->total_capacity_ah;
    cc->state_of_charge_percent = 100.0f;
    cc->energy_wh = 0.0f;
    cc->energy_kwh = 0.0f;
    
    printf("Coulomb Counter: Reset to 100%% SOC\n");
}

/**
 * @brief Set SOC to specific percentage
 */
void coulomb_counter_set_soc(coulomb_counter_t *cc, float soc_percent) {
    if (!cc || soc_percent < 0.0f || soc_percent > 100.0f) {
        return;
    }
    
    cc->state_of_charge_percent = soc_percent;
    cc->remaining_capacity_ah = (soc_percent / 100.0f) * cc->total_capacity_ah;
    
    printf("Coulomb Counter: SOC set to %.1f%% (%.1f Ah)\n", 
           soc_percent, cc->remaining_capacity_ah);
}

/**
 * @brief Get current in Amperes
 */
float coulomb_counter_get_current(coulomb_counter_t *cc) {
    return cc ? cc->current : 0.0f;
}

/**
 * @brief Get power in Watts
 */
float coulomb_counter_get_power(coulomb_counter_t *cc) {
    return cc ? cc->power : 0.0f;
}

/**
 * @brief Get energy in Wh
 */
float coulomb_counter_get_energy_wh(coulomb_counter_t *cc) {
    return cc ? cc->energy_wh : 0.0f;
}

/**
 * @brief Get energy in kWh
 */
float coulomb_counter_get_energy_kwh(coulomb_counter_t *cc) {
    return cc ? cc->energy_kwh : 0.0f;
}

/**
 * @brief Get state of charge percentage
 */
float coulomb_counter_get_soc(coulomb_counter_t *cc) {
    return cc ? cc->state_of_charge_percent : 0.0f;
}

/**
 * @brief Get remaining capacity in Ah
 */
float coulomb_counter_get_remaining_capacity(coulomb_counter_t *cc) {
    return cc ? cc->remaining_capacity_ah : 0.0f;
}

/**
 * @brief Print coulomb counter status
 */
void coulomb_counter_print_status(coulomb_counter_t *cc) {
    if (!cc) {
        printf("Coulomb Counter: Invalid pointer\n");
        return;
    }
    
    printf("=== Coulomb Counter Status ===\n");
    printf("Current: %.3f A\n", cc->current);
    printf("Voltage: %.1f V\n", cc->voltage);
    printf("Power: %.3f W\n", cc->power);
    printf("Energy: %.2f Wh (%.3f kWh)\n", cc->energy_wh, cc->energy_kwh);
    printf("SOC: %.1f%% (%.1f / %.1f Ah)\n", 
           cc->state_of_charge_percent, cc->remaining_capacity_ah, cc->total_capacity_ah);
    printf("==============================\n");
}

