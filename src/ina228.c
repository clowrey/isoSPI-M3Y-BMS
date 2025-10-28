/**
 * @file ina228.c
 * @brief INA228 Current/Power Monitor Driver Implementation
 */

#include "ina228.h"
#include "pin_config.h"
#include <stdio.h>
#include <math.h>

// INA228 Constants
#define INA228_MANUFACTURER_ID  0x5449  // "TI" in ASCII
#define INA228_DEVICE_ID        0x228   // Device ID

// LSB calculation constants
#define INA228_CURRENT_LSB_FACTOR   13107.2e6f
#define INA228_SHUNT_VOLTAGE_LSB    312.5e-9f  // 312.5 nV per bit
#define INA228_BUS_VOLTAGE_LSB      195.3125e-6f  // 195.3125 µV per bit
#define INA228_TEMP_LSB             7.8125e-3f  // 7.8125 m°C per bit

/**
 * @brief Initialize INA228 device
 */
bool ina228_init(ina228_t *dev, i2c_inst_t *i2c, uint8_t addr, float shunt_resistance) {
    if (!dev || !i2c) {
        return false;
    }
    
    dev->i2c = i2c;
    dev->addr = addr;
    dev->shunt_resistance = shunt_resistance;
    dev->initialized = false;
    
    // Check device ID
    if (!ina228_check_device_id(dev)) {
        printf("INA228: Device ID check failed\n");
        return false;
    }
    
    // Reset device
    if (!ina228_reset(dev)) {
        printf("INA228: Reset failed\n");
        return false;
    }
    
    sleep_ms(10);
    
    // Configure ADC for high precision
    // 256 samples average, 1.1ms conversion time, continuous shunt+bus+temp mode
    if (!ina228_configure_adc(dev, INA228_ADC_AVG_256, INA228_ADC_TIME_1052us, 
                               INA228_ADC_TIME_1052us, INA228_ADC_MODE_CONT_BUS_SHUNT_TEMP)) {
        printf("INA228: ADC configuration failed\n");
        return false;
    }
    
    // Calculate LSB and calibrate
    if (!ina228_calibrate(dev)) {
        printf("INA228: Calibration failed\n");
        return false;
    }
    
    dev->initialized = true;
    printf("INA228: Initialized successfully at address 0x%02X\n", addr);
    return true;
}

/**
 * @brief Reset INA228 device
 */
bool ina228_reset(ina228_t *dev) {
    // Write reset bit to CONFIG register
    return ina228_write_reg16(dev, INA228_REG_CONFIG, 0x8000);
}

/**
 * @brief Calibrate INA228 for current measurement
 */
bool ina228_calibrate(ina228_t *dev) {
    // Set current LSB to 1mA for good resolution across expected range
    dev->current_lsb = 0.001f;  // 1 mA/bit
    
    // Calculate calibration value
    // CAL = 13107.2 × 10^6 × CURRENT_LSB × RSHUNT
    float cal_value = INA228_CURRENT_LSB_FACTOR * dev->current_lsb * dev->shunt_resistance;
    uint16_t cal = (uint16_t)cal_value;
    
    // Power LSB is automatically 0.2 * CURRENT_LSB
    dev->power_lsb = 0.2f * dev->current_lsb;
    
    printf("INA228: Calibration - LSB=%.6f A, CAL=%u, Rshunt=%.9f Ω\n", 
           dev->current_lsb, cal, dev->shunt_resistance);
    
    // Write calibration value
    if (!ina228_write_reg16(dev, INA228_REG_SHUNT_CAL, cal)) {
        return false;
    }
    
    // Set temperature coefficient to 0 (Manganin shunt)
    return ina228_write_reg16(dev, INA228_REG_SHUNT_TEMPCO, 0);
}

/**
 * @brief Configure ADC settings
 */
bool ina228_configure_adc(ina228_t *dev, uint8_t avg, uint8_t vbusct, uint8_t vshct, uint8_t mode) {
    // ADC_CONFIG format: [15:12] AVG, [11:9] VTCT, [8:6] VSHCT, [5:3] VBUSCT, [2:0] MODE
    uint16_t config = ((uint16_t)avg << 12) | 
                      ((uint16_t)vbusct << 9) | 
                      ((uint16_t)vshct << 6) | 
                      ((uint16_t)mode << 0);
    
    return ina228_write_reg16(dev, INA228_REG_ADC_CONFIG, config);
}

/**
 * @brief Read current in Amperes
 */
float ina228_read_current(ina228_t *dev) {
    if (!dev || !dev->initialized) {
        return 0.0f;
    }
    
    uint32_t raw = 0;
    if (!ina228_read_reg24(dev, INA228_REG_CURRENT, &raw)) {
        return 0.0f;
    }
    
    // Sign extend from 20-bit to 32-bit
    int32_t signed_raw = (int32_t)raw;
    if (signed_raw & 0x80000) {
        signed_raw |= 0xFFF00000;
    }
    
    // Convert to Amperes
    return (float)signed_raw * dev->current_lsb;
}

/**
 * @brief Read bus voltage in Volts
 */
float ina228_read_bus_voltage(ina228_t *dev) {
    if (!dev || !dev->initialized) {
        return 0.0f;
    }
    
    uint32_t raw = 0;
    if (!ina228_read_reg24(dev, INA228_REG_VBUS, &raw)) {
        return 0.0f;
    }
    
    // Sign extend from 20-bit to 32-bit
    int32_t signed_raw = (int32_t)raw;
    if (signed_raw & 0x80000) {
        signed_raw |= 0xFFF00000;
    }
    
    // Convert to Volts
    return (float)signed_raw * INA228_BUS_VOLTAGE_LSB;
}

/**
 * @brief Read shunt voltage in Volts
 */
float ina228_read_shunt_voltage(ina228_t *dev) {
    if (!dev || !dev->initialized) {
        return 0.0f;
    }
    
    uint32_t raw = 0;
    if (!ina228_read_reg24(dev, INA228_REG_VSHUNT, &raw)) {
        return 0.0f;
    }
    
    // Sign extend from 20-bit to 32-bit
    int32_t signed_raw = (int32_t)raw;
    if (signed_raw & 0x80000) {
        signed_raw |= 0xFFF00000;
    }
    
    // Convert to Volts
    return (float)signed_raw * INA228_SHUNT_VOLTAGE_LSB;
}

/**
 * @brief Read power in Watts
 */
float ina228_read_power(ina228_t *dev) {
    if (!dev || !dev->initialized) {
        return 0.0f;
    }
    
    uint32_t raw = 0;
    if (!ina228_read_reg24(dev, INA228_REG_POWER, &raw)) {
        return 0.0f;
    }
    
    // Convert to Watts
    return (float)raw * dev->power_lsb;
}

/**
 * @brief Read energy in Wh
 */
float ina228_read_energy(ina228_t *dev) {
    if (!dev || !dev->initialized) {
        return 0.0f;
    }
    
    uint64_t raw = 0;
    if (!ina228_read_reg40(dev, INA228_REG_ENERGY, &raw)) {
        return 0.0f;
    }
    
    // Energy LSB = 16 * POWER_LSB * conversion_time (in seconds)
    // For 1.1ms conversion: LSB = 16 * power_lsb * 0.0011
    float energy_lsb = 16.0f * dev->power_lsb * 0.0011f;
    
    // Convert to Wh
    return (float)raw * energy_lsb;
}

/**
 * @brief Read charge in Ah
 */
float ina228_read_charge(ina228_t *dev) {
    if (!dev || !dev->initialized) {
        return 0.0f;
    }
    
    uint64_t raw = 0;
    if (!ina228_read_reg40(dev, INA228_REG_CHARGE, &raw)) {
        return 0.0f;
    }
    
    // Charge LSB = CURRENT_LSB (in A/s = C/s = As)
    // Convert to Ah: divide by 3600
    return ((float)raw * dev->current_lsb) / 3600.0f;
}

/**
 * @brief Read die temperature in °C
 */
float ina228_read_temperature(ina228_t *dev) {
    if (!dev || !dev->initialized) {
        return 0.0f;
    }
    
    uint16_t raw = 0;
    if (!ina228_read_reg16(dev, INA228_REG_DIETEMP, &raw)) {
        return 0.0f;
    }
    
    // Sign extend from 16-bit
    int16_t signed_raw = (int16_t)raw;
    
    // Convert to °C
    return (float)signed_raw * INA228_TEMP_LSB;
}

/**
 * @brief Write 16-bit register
 */
bool ina228_write_reg16(ina228_t *dev, uint8_t reg, uint16_t value) {
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (value >> 8) & 0xFF;  // MSB
    buf[2] = value & 0xFF;          // LSB
    
    int result = i2c_write_blocking(dev->i2c, dev->addr, buf, 3, false);
    return (result == 3);
}

/**
 * @brief Read 16-bit register
 */
bool ina228_read_reg16(ina228_t *dev, uint8_t reg, uint16_t *value) {
    uint8_t buf[2];
    
    // Write register address
    if (i2c_write_blocking(dev->i2c, dev->addr, &reg, 1, true) != 1) {
        return false;
    }
    
    // Read 2 bytes
    if (i2c_read_blocking(dev->i2c, dev->addr, buf, 2, false) != 2) {
        return false;
    }
    
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return true;
}

/**
 * @brief Read 24-bit register (20-bit data)
 */
bool ina228_read_reg24(ina228_t *dev, uint8_t reg, uint32_t *value) {
    uint8_t buf[3];
    
    // Write register address
    if (i2c_write_blocking(dev->i2c, dev->addr, &reg, 1, true) != 1) {
        return false;
    }
    
    // Read 3 bytes
    if (i2c_read_blocking(dev->i2c, dev->addr, buf, 3, false) != 3) {
        return false;
    }
    
    *value = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    return true;
}

/**
 * @brief Read 40-bit register
 */
bool ina228_read_reg40(ina228_t *dev, uint8_t reg, uint64_t *value) {
    uint8_t buf[5];
    
    // Write register address
    if (i2c_write_blocking(dev->i2c, dev->addr, &reg, 1, true) != 1) {
        return false;
    }
    
    // Read 5 bytes
    if (i2c_read_blocking(dev->i2c, dev->addr, buf, 5, false) != 5) {
        return false;
    }
    
    *value = ((uint64_t)buf[0] << 32) | ((uint64_t)buf[1] << 24) | 
             ((uint64_t)buf[2] << 16) | ((uint64_t)buf[3] << 8) | buf[4];
    return true;
}

/**
 * @brief Check device ID
 */
bool ina228_check_device_id(ina228_t *dev) {
    uint16_t device_id = 0;
    
    if (!ina228_read_reg16(dev, INA228_REG_DEVICE_ID, &device_id)) {
        return false;
    }
    
    // Device ID should be 0x228
    uint16_t dev_id = (device_id >> 4) & 0xFFF;
    
    printf("INA228: Device ID = 0x%03X (expected 0x228)\n", dev_id);
    return (dev_id == INA228_DEVICE_ID);
}

/**
 * @brief Print status information
 */
void ina228_print_status(ina228_t *dev) {
    if (!dev) {
        printf("INA228: Invalid device pointer\n");
        return;
    }
    
    printf("=== INA228 Status ===\n");
    printf("Initialized: %s\n", dev->initialized ? "YES" : "NO");
    printf("I2C Address: 0x%02X\n", dev->addr);
    printf("Shunt Resistance: %.9f Ω\n", dev->shunt_resistance);
    printf("Current LSB: %.6f A\n", dev->current_lsb);
    printf("Power LSB: %.6f W\n", dev->power_lsb);
    
    if (dev->initialized) {
        printf("\nCurrent Readings:\n");
        printf("Current: %.3f A\n", ina228_read_current(dev));
        printf("Shunt Voltage: %.6f V\n", ina228_read_shunt_voltage(dev));
        printf("Bus Voltage: %.3f V\n", ina228_read_bus_voltage(dev));
        printf("Power: %.3f W\n", ina228_read_power(dev));
        printf("Temperature: %.1f °C\n", ina228_read_temperature(dev));
    }
    
    printf("====================\n");
}

