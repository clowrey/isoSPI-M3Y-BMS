/**
 * @file pin_config.h
 * @brief RP2350A Pin Configuration for Tesla BMS Interface
 * 
 * Pin assignments optimized for PCB routing and peripheral requirements.
 */

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// Tesla BMS SPI Interface (SPI0)
#define TESLA_BMS_SPI_INST      spi0
#define TESLA_BMS_SPI_FREQ      1000000  // 1 MHz
#define TESLA_BMS_PIN_MISO      16       // GP16 - SPI0_MISO
#define TESLA_BMS_PIN_MOSI      19       // GP19 - SPI0_MOSI
#define TESLA_BMS_PIN_SCK       18       // GP18 - SPI0_SCK
#define TESLA_BMS_PIN_CS        17       // GP17 - Chip Select (GPIO)
#define TESLA_BMS_PIN_ENABLE    20       // GP20 - BMB Enable (Active LOW per ESP32)

// INA228 Current Sensor I2C Interface (I2C0)
#define INA228_I2C_INST         i2c0
#define INA228_I2C_FREQ         400000   // 400 kHz
#define INA228_PIN_SDA          4        // GP4 - I2C0_SDA
#define INA228_PIN_SCL          5        // GP5 - I2C0_SCL
#define INA228_PIN_ALERT        6        // GP6 - Alert pin (optional)
#define INA228_I2C_ADDR         0x40     // Default I2C address

// Pack Voltage ADC Inputs (Internal 12-bit ADC)
#define ADC_PIN_PACK_NEG        26       // GP26 - ADC0
#define ADC_PIN_PACK_POS        27       // GP27 - ADC1
#define ADC_PIN_LINK_NEG        28       // GP28 - ADC2
#define ADC_PIN_LINK_POS        29       // GP29 - ADC3

#define ADC_CHANNEL_PACK_NEG    0        // ADC Channel 0
#define ADC_CHANNEL_PACK_POS    1        // ADC Channel 1
#define ADC_CHANNEL_LINK_NEG    2        // ADC Channel 2
#define ADC_CHANNEL_LINK_POS    3        // ADC Channel 3

// ESPHome Display Serial Interface (UART0)
#define ESPHOME_UART_INST       uart0
#define ESPHOME_UART_BAUD       921600   // Match ESP32 configuration
#define ESPHOME_PIN_TX          0        // GP0 - UART0_TX
#define ESPHOME_PIN_RX          1        // GP1 - UART0_RX

// PWM Outputs for Contactor Control
#define PWM_PIN_PACK_CONTACTORS 20       // GP20 - PWM2A
#define PWM_PIN_PRECHARGE_RELAY 21       // GP21 - PWM2B
#define PWM_FREQUENCY           10000    // 10 kHz PWM frequency
#define PWM_DUTY_NORMAL         15       // 15% duty cycle
#define PWM_DUTY_INITIAL        100      // 100% initial pulse
#define PWM_INITIAL_PULSE_MS    200      // 200ms initial pulse duration

// CAN Bus Interface (PIO0)
#define CAN_PIN_RX              2        // GP2 - CAN RX
#define CAN_PIN_TX              3        // GP3 - CAN TX
#define CAN_PIO_NUM             0        // Use PIO0
#define CAN_BITRATE             500000   // 500 kbps

// Status LED (built-in)
#define LED_PIN                 25       // GP25 - Built-in LED

#endif // PIN_CONFIG_H

