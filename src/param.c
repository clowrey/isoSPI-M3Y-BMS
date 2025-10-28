/**
 * @file param.c
 * @brief Parameter Management System Implementation
 */

#include "param.h"
#include "pin_config.h"
#include "hardware/uart.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Parameter storage structure
typedef struct {
    union {
        int32_t int_val;
        float float_val;
        char string_val[PARAM_STRING_MAX_LEN];
    } value;
    param_type_t type;
    const char* name;
} param_entry_t;

// Parameter storage array
static param_entry_t params[PARAM_COUNT];

// Parameter name lookup table
static const char* param_names[] = {
    [PARAM_NUMBMBS] = "numbmbs",
    [PARAM_LOOP_CNT] = "LoopCnt",
    [PARAM_LOOP_STATE] = "LoopState",
    [PARAM_CELLS_PRESENT] = "CellsPresent",
    [PARAM_CELLS_BALANCING] = "CellsBalancing",
    [PARAM_BALANCE] = "balance",
    [PARAM_BALANCE_CELL_LIST] = "BalanceCellList",
    [PARAM_ACTUAL_BMB_COUNT] = "ActualBmbCount",
    [PARAM_EXPECTED_BMB_COUNT] = "ExpectedBmbCount",
    [PARAM_BMB_CONNECTED_MASK] = "BmbConnectedMask",
    [PARAM_CELL_MAX] = "CellMax",
    [PARAM_CELL_MIN] = "CellMin",
    [PARAM_UMAX] = "umax",
    [PARAM_UMIN] = "umin",
    [PARAM_DELTA_V] = "deltaV",
    [PARAM_UAVG] = "uavg",
    [PARAM_UDC] = "udc",
    [PARAM_CELL_VOLTAGE_SUM] = "CellVoltageSum",
    [PARAM_CELL_VMAX] = "CellVmax",
    [PARAM_CELL_VMIN] = "CellVmin",
    // Cell voltages u1-u108
    [PARAM_U1] = "u1", [PARAM_U2] = "u2", [PARAM_U3] = "u3", [PARAM_U4] = "u4",
    [PARAM_U5] = "u5", [PARAM_U6] = "u6", [PARAM_U7] = "u7", [PARAM_U8] = "u8",
    [PARAM_U9] = "u9", [PARAM_U10] = "u10", [PARAM_U11] = "u11", [PARAM_U12] = "u12",
    [PARAM_U13] = "u13", [PARAM_U14] = "u14", [PARAM_U15] = "u15", [PARAM_U16] = "u16",
    [PARAM_U17] = "u17", [PARAM_U18] = "u18", [PARAM_U19] = "u19", [PARAM_U20] = "u20",
    [PARAM_U21] = "u21", [PARAM_U22] = "u22", [PARAM_U23] = "u23", [PARAM_U24] = "u24",
    [PARAM_U25] = "u25", [PARAM_U26] = "u26", [PARAM_U27] = "u27", [PARAM_U28] = "u28",
    [PARAM_U29] = "u29", [PARAM_U30] = "u30", [PARAM_U31] = "u31", [PARAM_U32] = "u32",
    [PARAM_U33] = "u33", [PARAM_U34] = "u34", [PARAM_U35] = "u35", [PARAM_U36] = "u36",
    [PARAM_U37] = "u37", [PARAM_U38] = "u38", [PARAM_U39] = "u39", [PARAM_U40] = "u40",
    [PARAM_U41] = "u41", [PARAM_U42] = "u42", [PARAM_U43] = "u43", [PARAM_U44] = "u44",
    [PARAM_U45] = "u45", [PARAM_U46] = "u46", [PARAM_U47] = "u47", [PARAM_U48] = "u48",
    [PARAM_U49] = "u49", [PARAM_U50] = "u50", [PARAM_U51] = "u51", [PARAM_U52] = "u52",
    [PARAM_U53] = "u53", [PARAM_U54] = "u54", [PARAM_U55] = "u55", [PARAM_U56] = "u56",
    [PARAM_U57] = "u57", [PARAM_U58] = "u58", [PARAM_U59] = "u59", [PARAM_U60] = "u60",
    [PARAM_U61] = "u61", [PARAM_U62] = "u62", [PARAM_U63] = "u63", [PARAM_U64] = "u64",
    [PARAM_U65] = "u65", [PARAM_U66] = "u66", [PARAM_U67] = "u67", [PARAM_U68] = "u68",
    [PARAM_U69] = "u69", [PARAM_U70] = "u70", [PARAM_U71] = "u71", [PARAM_U72] = "u72",
    [PARAM_U73] = "u73", [PARAM_U74] = "u74", [PARAM_U75] = "u75", [PARAM_U76] = "u76",
    [PARAM_U77] = "u77", [PARAM_U78] = "u78", [PARAM_U79] = "u79", [PARAM_U80] = "u80",
    [PARAM_U81] = "u81", [PARAM_U82] = "u82", [PARAM_U83] = "u83", [PARAM_U84] = "u84",
    [PARAM_U85] = "u85", [PARAM_U86] = "u86", [PARAM_U87] = "u87", [PARAM_U88] = "u88",
    [PARAM_U89] = "u89", [PARAM_U90] = "u90", [PARAM_U91] = "u91", [PARAM_U92] = "u92",
    [PARAM_U93] = "u93", [PARAM_U94] = "u94", [PARAM_U95] = "u95", [PARAM_U96] = "u96",
    [PARAM_U97] = "u97", [PARAM_U98] = "u98", [PARAM_U99] = "u99", [PARAM_U100] = "u100",
    [PARAM_U101] = "u101", [PARAM_U102] = "u102", [PARAM_U103] = "u103", [PARAM_U104] = "u104",
    [PARAM_U105] = "u105", [PARAM_U106] = "u106", [PARAM_U107] = "u107", [PARAM_U108] = "u108",
    // Temperature
    [PARAM_CHIPT0] = "Chipt0",
    [PARAM_CELLT0_0] = "Cellt0_0",
    [PARAM_CELLT0_1] = "Cellt0_1",
    [PARAM_TEMP_MAX] = "TempMax",
    [PARAM_TEMP_MIN] = "TempMin",
    // Pack voltages
    [PARAM_BATT_CONTACTOR_POS] = "battContactorPos",
    [PARAM_BATT_CONTACTOR_NEG] = "battContactorNeg",
    [PARAM_BATT_LINK_POS] = "battLinkPos",
    [PARAM_BATT_LINK_NEG] = "battLinkNeg",
    // Current sensor
    [PARAM_CURRENT] = "current",
    [PARAM_INA228_TEMP] = "ina228_temp",
    // Power and energy
    [PARAM_POWER_WATTS] = "PowerWatts",
    [PARAM_ENERGY_WH] = "EnergyWh",
    [PARAM_ENERGY_KWH] = "EnergyKWh",
    // Coulomb counting
    [PARAM_STATE_OF_CHARGE] = "StateOfCharge",
    [PARAM_REMAINING_CAPACITY_AH] = "RemainingCapacityAh",
    [PARAM_BATTERY_CAPACITY_AH] = "BatteryCapacityAh",
    [PARAM_FULLY_CHARGED_VOLTAGE] = "FullyChargedVoltage",
    [PARAM_CURRENT_EFFICIENCY] = "CurrentEfficiency",
};

/**
 * @brief Initialize parameter system
 */
void param_init(void) {
    // Initialize all parameters with default values and types
    for (int i = 0; i < PARAM_COUNT; i++) {
        params[i].name = param_names[i];
        
        // Determine parameter type based on name
        if (i == PARAM_BALANCE_CELL_LIST) {
            params[i].type = PARAM_TYPE_STRING;
            params[i].value.string_val[0] = '\0';
        } else if (i >= PARAM_BATT_CONTACTOR_POS && i <= PARAM_CURRENT_EFFICIENCY) {
            params[i].type = PARAM_TYPE_FLOAT;
            params[i].value.float_val = 0.0f;
        } else if (i >= PARAM_U1 && i <= PARAM_U108) {
            params[i].type = PARAM_TYPE_FLOAT;
            params[i].value.float_val = NAN;  // Use NAN for uninitialized cells
        } else {
            params[i].type = PARAM_TYPE_INT;
            params[i].value.int_val = 0;
        }
    }
    
    // Set default values for certain parameters
    param_set_float(PARAM_BATTERY_CAPACITY_AH, 75.0f);
    param_set_float(PARAM_FULLY_CHARGED_VOLTAGE, 4.2f);
    param_set_float(PARAM_CURRENT_EFFICIENCY, 0.95f);
    
    printf("Parameters: Initialized (%d parameters)\n", PARAM_COUNT);
}

/**
 * @brief Set integer parameter
 */
void param_set_int(param_num_t param, int32_t value) {
    if (param >= PARAM_COUNT) return;
    
    if (params[param].type == PARAM_TYPE_INT) {
        params[param].value.int_val = value;
    }
}

/**
 * @brief Set float parameter
 */
void param_set_float(param_num_t param, float value) {
    if (param >= PARAM_COUNT) return;
    
    if (params[param].type == PARAM_TYPE_FLOAT) {
        params[param].value.float_val = value;
    }
}

/**
 * @brief Set string parameter
 */
void param_set_string(param_num_t param, const char* value) {
    if (param >= PARAM_COUNT || !value) return;
    
    if (params[param].type == PARAM_TYPE_STRING) {
        strncpy(params[param].value.string_val, value, PARAM_STRING_MAX_LEN - 1);
        params[param].value.string_val[PARAM_STRING_MAX_LEN - 1] = '\0';
    }
}

/**
 * @brief Get integer parameter
 */
int32_t param_get_int(param_num_t param) {
    if (param >= PARAM_COUNT) return 0;
    
    if (params[param].type == PARAM_TYPE_INT) {
        return params[param].value.int_val;
    }
    return 0;
}

/**
 * @brief Get float parameter
 */
float param_get_float(param_num_t param) {
    if (param >= PARAM_COUNT) return 0.0f;
    
    if (params[param].type == PARAM_TYPE_FLOAT) {
        return params[param].value.float_val;
    }
    return 0.0f;
}

/**
 * @brief Get string parameter
 */
const char* param_get_string(param_num_t param) {
    if (param >= PARAM_COUNT) return "";
    
    if (params[param].type == PARAM_TYPE_STRING) {
        return params[param].value.string_val;
    }
    return "";
}

/**
 * @brief Get parameter name
 */
const char* param_get_name(param_num_t param) {
    if (param >= PARAM_COUNT) return "unknown";
    return params[param].name;
}

/**
 * @brief Get parameter type
 */
param_type_t param_get_type(param_num_t param) {
    if (param >= PARAM_COUNT) return PARAM_TYPE_INT;
    return params[param].type;
}

/**
 * @brief Get parameter number from name
 */
param_num_t param_get_from_name(const char* name) {
    if (!name) return (param_num_t)-1;
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (strcmp(params[i].name, name) == 0) {
            return (param_num_t)i;
        }
    }
    
    return (param_num_t)-1;
}

/**
 * @brief Print all parameters to USB serial
 */
void param_print_all(void) {
    printf("=== BMS Parameters ===\n");
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        printf("%s=", params[i].name);
        
        switch (params[i].type) {
            case PARAM_TYPE_INT:
                printf("%d\n", params[i].value.int_val);
                break;
            case PARAM_TYPE_FLOAT:
                if (isnanf(params[i].value.float_val)) {
                    printf("nan\n");
                } else {
                    printf("%.3f\n", params[i].value.float_val);
                }
                break;
            case PARAM_TYPE_STRING:
                printf("%s\n", params[i].value.string_val);
                break;
        }
    }
    
    printf("DATA_COMPLETE\n");
}

/**
 * @brief Send all parameters to ESPHome via UART
 */
void param_send_to_esphome(void) {
    char buffer[128];
    
    // Send all non-cell voltage parameters first
    for (int i = 0; i < PARAM_U1; i++) {
        int len = 0;
        
        switch (params[i].type) {
            case PARAM_TYPE_INT:
                len = snprintf(buffer, sizeof(buffer), "%s=%d\n", 
                              params[i].name, params[i].value.int_val);
                break;
            case PARAM_TYPE_FLOAT:
                len = snprintf(buffer, sizeof(buffer), "%s=%.3f\n", 
                              params[i].name, params[i].value.float_val);
                break;
            case PARAM_TYPE_STRING:
                len = snprintf(buffer, sizeof(buffer), "%s=%s\n", 
                              params[i].name, params[i].value.string_val);
                break;
        }
        
        if (len > 0) {
            uart_write_blocking(ESPHOME_UART_INST, (uint8_t*)buffer, len);
        }
    }
    
    // Send cell voltages (u1-u108) in compact format
    int cell_num = 1;
    for (int i = PARAM_U1; i <= PARAM_U108; i++) {
        float voltage = params[i].value.float_val;
        int len = 0;
        
        if (isnanf(voltage)) {
            len = snprintf(buffer, sizeof(buffer), "%d=nan\n", cell_num);
        } else if (voltage > 0.0f) {
            len = snprintf(buffer, sizeof(buffer), "%d=%.0f\n", cell_num, voltage);
        }
        
        if (len > 0) {
            uart_write_blocking(ESPHOME_UART_INST, (uint8_t*)buffer, len);
        }
        cell_num++;
    }
    
    // Send remaining parameters (temperature, pack voltages, current, etc.)
    for (int i = PARAM_U108 + 1; i < PARAM_COUNT; i++) {
        int len = 0;
        
        switch (params[i].type) {
            case PARAM_TYPE_INT:
                len = snprintf(buffer, sizeof(buffer), "%s=%d\n", 
                              params[i].name, params[i].value.int_val);
                break;
            case PARAM_TYPE_FLOAT:
                len = snprintf(buffer, sizeof(buffer), "%s=%.3f\n", 
                              params[i].name, params[i].value.float_val);
                break;
            case PARAM_TYPE_STRING:
                len = snprintf(buffer, sizeof(buffer), "%s=%s\n", 
                              params[i].name, params[i].value.string_val);
                break;
        }
        
        if (len > 0) {
            uart_write_blocking(ESPHOME_UART_INST, (uint8_t*)buffer, len);
        }
    }
    
    // Send completion marker
    const char* complete_msg = "DATA_COMPLETE\n";
    uart_write_blocking(ESPHOME_UART_INST, (uint8_t*)complete_msg, strlen(complete_msg));
}

