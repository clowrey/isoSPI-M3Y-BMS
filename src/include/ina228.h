/**
 * @file ina228.h
 * @brief INA228 Current/Power Monitor Driver
 * 
 * I2C driver for Texas Instruments INA228 precision current and power monitor.
 * Supports 20-bit current measurement with hardware power/energy calculation.
 */

#ifndef INA228_H
#define INA228_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// INA228 Register Addresses
#define INA228_REG_CONFIG           0x00  // Configuration register
#define INA228_REG_ADC_CONFIG       0x01  // ADC configuration
#define INA228_REG_SHUNT_CAL        0x02  // Shunt calibration
#define INA228_REG_SHUNT_TEMPCO     0x03  // Shunt temperature coefficient
#define INA228_REG_VSHUNT           0x04  // Shunt voltage measurement
#define INA228_REG_VBUS             0x05  // Bus voltage measurement
#define INA228_REG_DIETEMP          0x06  // Die temperature
#define INA228_REG_CURRENT          0x07  // Current result
#define INA228_REG_POWER            0x08  // Power result
#define INA228_REG_ENERGY           0x09  // Energy result
#define INA228_REG_CHARGE           0x0A  // Charge result
#define INA228_REG_DIAG_ALRT        0x0B  // Diagnostic flags and alert
#define INA228_REG_SOVL             0x0C  // Shunt overvoltage threshold
#define INA228_REG_SUVL             0x0D  // Shunt undervoltage threshold
#define INA228_REG_BOVL             0x0E  // Bus overvoltage threshold
#define INA228_REG_BUVL             0x0F  // Bus undervoltage threshold
#define INA228_REG_TEMP_LIMIT       0x10  // Temperature over-limit threshold
#define INA228_REG_PWR_LIMIT        0x11  // Power over-limit threshold
#define INA228_REG_MANUFACTURER_ID  0x3E  // Manufacturer ID
#define INA228_REG_DEVICE_ID        0x3F  // Device ID

// ADC Configuration values
#define INA228_ADC_MODE_SHUTDOWN            0x0
#define INA228_ADC_MODE_TRIG_BUS            0x1
#define INA228_ADC_MODE_TRIG_SHUNT          0x2
#define INA228_ADC_MODE_TRIG_BUS_SHUNT      0x3
#define INA228_ADC_MODE_TRIG_TEMP           0x4
#define INA228_ADC_MODE_CONT_BUS            0x5
#define INA228_ADC_MODE_CONT_SHUNT          0x6
#define INA228_ADC_MODE_CONT_BUS_SHUNT      0x7
#define INA228_ADC_MODE_CONT_BUS_SHUNT_TEMP 0xF

#define INA228_ADC_AVG_1       0x0
#define INA228_ADC_AVG_4       0x1
#define INA228_ADC_AVG_16      0x2
#define INA228_ADC_AVG_64      0x3
#define INA228_ADC_AVG_128     0x4
#define INA228_ADC_AVG_256     0x5
#define INA228_ADC_AVG_512     0x6
#define INA228_ADC_AVG_1024    0x7

#define INA228_ADC_TIME_50us   0x0
#define INA228_ADC_TIME_84us   0x1
#define INA228_ADC_TIME_150us  0x2
#define INA228_ADC_TIME_280us  0x3
#define INA228_ADC_TIME_540us  0x4
#define INA228_ADC_TIME_1052us 0x5
#define INA228_ADC_TIME_2074us 0x6
#define INA228_ADC_TIME_4120us 0x7

// INA228 configuration structure
typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;
    float shunt_resistance;  // In ohms
    float current_lsb;       // LSB value for current calculation
    float power_lsb;         // LSB value for power calculation
    bool initialized;
} ina228_t;

// Initialization and configuration
bool ina228_init(ina228_t *dev, i2c_inst_t *i2c, uint8_t addr, float shunt_resistance);
bool ina228_reset(ina228_t *dev);
bool ina228_calibrate(ina228_t *dev);
bool ina228_configure_adc(ina228_t *dev, uint8_t avg, uint8_t vbusct, uint8_t vshct, uint8_t mode);

// Reading functions
float ina228_read_current(ina228_t *dev);           // Returns current in Amperes
float ina228_read_bus_voltage(ina228_t *dev);       // Returns bus voltage in Volts
float ina228_read_shunt_voltage(ina228_t *dev);     // Returns shunt voltage in Volts
float ina228_read_power(ina228_t *dev);             // Returns power in Watts
float ina228_read_energy(ina228_t *dev);            // Returns energy in Wh
float ina228_read_charge(ina228_t *dev);            // Returns charge in Ah
float ina228_read_temperature(ina228_t *dev);       // Returns die temperature in °C

// Register access (low-level)
bool ina228_write_reg16(ina228_t *dev, uint8_t reg, uint16_t value);
bool ina228_read_reg16(ina228_t *dev, uint8_t reg, uint16_t *value);
bool ina228_read_reg24(ina228_t *dev, uint8_t reg, uint32_t *value);
bool ina228_read_reg40(ina228_t *dev, uint8_t reg, uint64_t *value);

// Utility functions
bool ina228_check_device_id(ina228_t *dev);
void ina228_print_status(ina228_t *dev);

#ifdef __cplusplus
}
#endif

#endif // INA228_H

