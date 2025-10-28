/**
 * @file param.h
 * @brief Parameter Management System
 * 
 * Central parameter storage and management for all BMS parameters.
 * Ported from ESP32/Arduino to RP2350/Pico SDK.
 */

#ifndef PARAM_H
#define PARAM_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum string parameter length
#define PARAM_STRING_MAX_LEN 128

// Parameter enumeration
typedef enum {
    // System parameters
    PARAM_NUMBMBS = 0,
    PARAM_LOOP_CNT,
    PARAM_LOOP_STATE,
    PARAM_CELLS_PRESENT,
    PARAM_CELLS_BALANCING,
    PARAM_BALANCE,
    PARAM_BALANCE_CELL_LIST,
    
    // BMB Connectivity
    PARAM_ACTUAL_BMB_COUNT,
    PARAM_EXPECTED_BMB_COUNT,
    PARAM_BMB_CONNECTED_MASK,
    
    // Voltage statistics
    PARAM_CELL_MAX,
    PARAM_CELL_MIN,
    PARAM_UMAX,
    PARAM_UMIN,
    PARAM_DELTA_V,
    PARAM_UAVG,
    PARAM_UDC,
    PARAM_CELL_VOLTAGE_SUM,
    PARAM_CELL_VMAX,
    PARAM_CELL_VMIN,
    
    // Individual cell voltages (u1-u108)
    PARAM_U1, PARAM_U2, PARAM_U3, PARAM_U4, PARAM_U5, PARAM_U6, PARAM_U7, PARAM_U8,
    PARAM_U9, PARAM_U10, PARAM_U11, PARAM_U12, PARAM_U13, PARAM_U14, PARAM_U15, PARAM_U16,
    PARAM_U17, PARAM_U18, PARAM_U19, PARAM_U20, PARAM_U21, PARAM_U22, PARAM_U23, PARAM_U24,
    PARAM_U25, PARAM_U26, PARAM_U27, PARAM_U28, PARAM_U29, PARAM_U30, PARAM_U31, PARAM_U32,
    PARAM_U33, PARAM_U34, PARAM_U35, PARAM_U36, PARAM_U37, PARAM_U38, PARAM_U39, PARAM_U40,
    PARAM_U41, PARAM_U42, PARAM_U43, PARAM_U44, PARAM_U45, PARAM_U46, PARAM_U47, PARAM_U48,
    PARAM_U49, PARAM_U50, PARAM_U51, PARAM_U52, PARAM_U53, PARAM_U54, PARAM_U55, PARAM_U56,
    PARAM_U57, PARAM_U58, PARAM_U59, PARAM_U60, PARAM_U61, PARAM_U62, PARAM_U63, PARAM_U64,
    PARAM_U65, PARAM_U66, PARAM_U67, PARAM_U68, PARAM_U69, PARAM_U70, PARAM_U71, PARAM_U72,
    PARAM_U73, PARAM_U74, PARAM_U75, PARAM_U76, PARAM_U77, PARAM_U78, PARAM_U79, PARAM_U80,
    PARAM_U81, PARAM_U82, PARAM_U83, PARAM_U84, PARAM_U85, PARAM_U86, PARAM_U87, PARAM_U88,
    PARAM_U89, PARAM_U90, PARAM_U91, PARAM_U92, PARAM_U93, PARAM_U94, PARAM_U95, PARAM_U96,
    PARAM_U97, PARAM_U98, PARAM_U99, PARAM_U100, PARAM_U101, PARAM_U102, PARAM_U103, PARAM_U104,
    PARAM_U105, PARAM_U106, PARAM_U107, PARAM_U108,
    
    // Temperature parameters
    PARAM_CHIPT0,
    PARAM_CELLT0_0,
    PARAM_CELLT0_1,
    PARAM_TEMP_MAX,
    PARAM_TEMP_MIN,
    
    // Pack voltage measurements (from ADC)
    PARAM_BATT_CONTACTOR_POS,
    PARAM_BATT_CONTACTOR_NEG,
    PARAM_BATT_LINK_POS,
    PARAM_BATT_LINK_NEG,
    
    // Current sensor (INA228)
    PARAM_CURRENT,
    PARAM_INA228_TEMP,
    
    // Power and energy (INA228 hardware calculation)
    PARAM_POWER_WATTS,
    PARAM_ENERGY_WH,
    PARAM_ENERGY_KWH,
    
    // Coulomb counting
    PARAM_STATE_OF_CHARGE,
    PARAM_REMAINING_CAPACITY_AH,
    PARAM_BATTERY_CAPACITY_AH,
    PARAM_FULLY_CHARGED_VOLTAGE,
    PARAM_CURRENT_EFFICIENCY,
    
    // Total parameter count
    PARAM_COUNT
} param_num_t;

// Parameter type
typedef enum {
    PARAM_TYPE_INT,
    PARAM_TYPE_FLOAT,
    PARAM_TYPE_STRING
} param_type_t;

// Parameter functions
void param_init(void);

// Setters
void param_set_int(param_num_t param, int32_t value);
void param_set_float(param_num_t param, float value);
void param_set_string(param_num_t param, const char* value);

// Getters
int32_t param_get_int(param_num_t param);
float param_get_float(param_num_t param);
const char* param_get_string(param_num_t param);

// Parameter information
const char* param_get_name(param_num_t param);
param_type_t param_get_type(param_num_t param);
param_num_t param_get_from_name(const char* name);

// Serialization for ESPHome interface
void param_print_all(void);  // Print to USB serial
void param_send_to_esphome(void);  // Send to UART

#ifdef __cplusplus
}
#endif

#endif // PARAM_H

