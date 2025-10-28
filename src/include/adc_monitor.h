/**
 * @file adc_monitor.h
 * @brief Internal ADC Pack Voltage Monitor
 * 
 * Uses RP2350A's internal 12-bit ADC to monitor 4 pack voltage channels
 * through precision voltage dividers (400V → 3.0V).
 */

#ifndef ADC_MONITOR_H
#define ADC_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ADC configuration
#define ADC_VREF                3.3f       // ADC reference voltage
#define ADC_RESOLUTION          4096.0f    // 12-bit resolution
#define VOLTAGE_SCALE_FACTOR    133.33f    // 400V / 3.0V divider ratio
#define ADC_NUM_SAMPLES         16         // Number of samples for averaging

// ADC channels
typedef enum {
    ADC_CH_PACK_NEG = 0,  // GP26 - Pack Negative
    ADC_CH_PACK_POS = 1,  // GP27 - Pack Positive
    ADC_CH_LINK_NEG = 2,  // GP28 - Link Negative (post-contactors)
    ADC_CH_LINK_POS = 3,  // GP29 - Link Positive (post-contactors)
} adc_channel_t;

// Calibration structure for each channel
typedef struct {
    float offset;        // Voltage offset in V
    float gain;          // Gain correction factor
    bool calibrated;     // Calibration status
} adc_calibration_t;

// Pack voltage data structure
typedef struct {
    float pack_neg;      // Pack negative voltage
    float pack_pos;      // Pack positive voltage
    float pack_sum;      // Total pack voltage (sum of absolute values)
    float link_neg;      // Link negative voltage (post-contactors)
    float link_pos;      // Link positive voltage (post-contactors)
    uint32_t timestamp;  // Timestamp of last reading
} pack_voltage_t;

// Initialization
bool adc_monitor_init(void);

// Reading functions
float adc_read_voltage(adc_channel_t channel);
float adc_read_voltage_averaged(adc_channel_t channel, uint16_t num_samples);
void adc_read_all_voltages(pack_voltage_t *voltages);
uint16_t adc_read_raw(adc_channel_t channel);

// Calibration functions
void adc_set_calibration(adc_channel_t channel, float offset, float gain);
void adc_get_calibration(adc_channel_t channel, adc_calibration_t *cal);
void adc_auto_calibrate(adc_channel_t channel, float known_voltage);
void adc_save_calibration(void);  // Save to flash
void adc_load_calibration(void);  // Load from flash

// Status
bool adc_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // ADC_MONITOR_H

