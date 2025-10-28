/**
 * @file adc_monitor.c
 * @brief Internal ADC Pack Voltage Monitor Implementation
 */

#include "adc_monitor.h"
#include "pin_config.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <math.h>

// Calibration data for each channel
static adc_calibration_t calibration[4] = {
    {0.0f, 1.0f, false},  // Channel 0
    {0.0f, 1.0f, false},  // Channel 1
    {0.0f, 1.0f, false},  // Channel 2
    {0.0f, 1.0f, false},  // Channel 3
};

static bool initialized = false;

/**
 * @brief Initialize ADC for pack voltage monitoring
 */
bool adc_monitor_init(void) {
    if (initialized) {
        return true;
    }
    
    printf("ADC Monitor: Initializing...\n");
    
    // Initialize ADC hardware
    adc_init();
    
    // Configure GPIO pins for ADC input
    adc_gpio_init(ADC_PIN_PACK_NEG);  // GP26 - ADC0
    adc_gpio_init(ADC_PIN_PACK_POS);  // GP27 - ADC1
    adc_gpio_init(ADC_PIN_LINK_NEG);  // GP28 - ADC2
    adc_gpio_init(ADC_PIN_LINK_POS);  // GP29 - ADC3
    
    printf("ADC Monitor: Configured pins GP26-GP29 for ADC input\n");
    printf("ADC Monitor: Voltage divider ratio: 1:%.2f (400V → 3.0V)\n", VOLTAGE_SCALE_FACTOR);
    printf("ADC Monitor: Resolution: 12-bit (%.3fV per LSB at 400V)\n", 
           (400.0f / ADC_RESOLUTION));
    
    // Try to load calibration from flash
    adc_load_calibration();
    
    initialized = true;
    printf("ADC Monitor: Initialized successfully\n");
    return true;
}

/**
 * @brief Read voltage from specific ADC channel
 */
float adc_read_voltage(adc_channel_t channel) {
    if (!initialized) {
        return 0.0f;
    }
    
    // Select ADC input
    adc_select_input(channel);
    
    // Read raw ADC value (0-4095)
    uint16_t raw = adc_read();
    
    // Convert to voltage at ADC pin (0-3.3V)
    float adc_voltage = ((float)raw * ADC_VREF) / ADC_RESOLUTION;
    
    // Scale to actual pack voltage (0-400V)
    float pack_voltage = adc_voltage * VOLTAGE_SCALE_FACTOR;
    
    // Apply calibration
    adc_calibration_t *cal = &calibration[channel];
    if (cal->calibrated) {
        pack_voltage = (pack_voltage * cal->gain) + cal->offset;
    }
    
    return pack_voltage;
}

/**
 * @brief Read voltage with averaging to reduce noise
 */
float adc_read_voltage_averaged(adc_channel_t channel, uint16_t num_samples) {
    if (!initialized) {
        return 0.0f;
    }
    
    uint32_t sum = 0;
    
    // Select ADC input
    adc_select_input(channel);
    
    // Take multiple samples
    for (uint16_t i = 0; i < num_samples; i++) {
        sum += adc_read();
        sleep_us(10);  // Small delay between samples
    }
    
    // Calculate average
    float avg_raw = (float)sum / (float)num_samples;
    
    // Convert to voltage at ADC pin
    float adc_voltage = (avg_raw * ADC_VREF) / ADC_RESOLUTION;
    
    // Scale to actual pack voltage
    float pack_voltage = adc_voltage * VOLTAGE_SCALE_FACTOR;
    
    // Apply calibration
    adc_calibration_t *cal = &calibration[channel];
    if (cal->calibrated) {
        pack_voltage = (pack_voltage * cal->gain) + cal->offset;
    }
    
    return pack_voltage;
}

/**
 * @brief Read all pack voltages
 */
void adc_read_all_voltages(pack_voltage_t *voltages) {
    if (!voltages || !initialized) {
        return;
    }
    
    // Read all 4 channels with averaging
    voltages->pack_neg = adc_read_voltage_averaged(ADC_CH_PACK_NEG, ADC_NUM_SAMPLES);
    voltages->pack_pos = adc_read_voltage_averaged(ADC_CH_PACK_POS, ADC_NUM_SAMPLES);
    voltages->link_neg = adc_read_voltage_averaged(ADC_CH_LINK_NEG, ADC_NUM_SAMPLES);
    voltages->link_pos = adc_read_voltage_averaged(ADC_CH_LINK_POS, ADC_NUM_SAMPLES);
    
    // Calculate pack sum (total voltage)
    voltages->pack_sum = fabsf(voltages->pack_pos) + fabsf(voltages->pack_neg);
    
    // Store timestamp
    voltages->timestamp = to_ms_since_boot(get_absolute_time());
}

/**
 * @brief Read raw ADC value
 */
uint16_t adc_read_raw(adc_channel_t channel) {
    if (!initialized) {
        return 0;
    }
    
    adc_select_input(channel);
    return adc_read();
}

/**
 * @brief Set calibration for a channel
 */
void adc_set_calibration(adc_channel_t channel, float offset, float gain) {
    if (channel >= 4) {
        return;
    }
    
    calibration[channel].offset = offset;
    calibration[channel].gain = gain;
    calibration[channel].calibrated = true;
    
    printf("ADC Ch%d: Calibration set - Offset=%.3fV, Gain=%.6f\n", 
           channel, offset, gain);
}

/**
 * @brief Get calibration for a channel
 */
void adc_get_calibration(adc_channel_t channel, adc_calibration_t *cal) {
    if (channel >= 4 || !cal) {
        return;
    }
    
    *cal = calibration[channel];
}

/**
 * @brief Auto-calibrate a channel with known voltage
 */
void adc_auto_calibrate(adc_channel_t channel, float known_voltage) {
    if (channel >= 4 || !initialized) {
        return;
    }
    
    printf("ADC Ch%d: Auto-calibrating with known voltage %.3fV...\n", 
           channel, known_voltage);
    
    // Read current (uncalibrated) voltage
    adc_select_input(channel);
    uint32_t sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += adc_read();
        sleep_us(10);
    }
    float avg_raw = (float)sum / 100.0f;
    float adc_voltage = (avg_raw * ADC_VREF) / ADC_RESOLUTION;
    float measured_voltage = adc_voltage * VOLTAGE_SCALE_FACTOR;
    
    // Calculate gain correction factor
    float gain = known_voltage / measured_voltage;
    
    // Set calibration (zero offset, calculated gain)
    adc_set_calibration(channel, 0.0f, gain);
    
    printf("ADC Ch%d: Calibration complete - Measured=%.3fV, Gain=%.6f\n", 
           channel, measured_voltage, gain);
}

/**
 * @brief Save calibration to flash (stub - implement with flash API)
 */
void adc_save_calibration(void) {
    // TODO: Implement flash storage for calibration data
    printf("ADC Monitor: Calibration save to flash not yet implemented\n");
}

/**
 * @brief Load calibration from flash (stub - implement with flash API)
 */
void adc_load_calibration(void) {
    // TODO: Implement flash loading for calibration data
    // For now, use default calibration (no offset, unity gain)
    printf("ADC Monitor: Using default calibration (no flash data)\n");
}

/**
 * @brief Check if ADC is initialized
 */
bool adc_is_initialized(void) {
    return initialized;
}

