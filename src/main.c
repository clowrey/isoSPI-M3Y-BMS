/**
 * @file main.c
 * @brief Tesla Model 3 BMS Interface - RP2350A Main Application
 * 
 * Main application for RP2350A-based Tesla BMS interface with INA228
 * current sensing and internal ADC pack voltage monitoring.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#include "pin_config.h"
#include "batman.h"
#include "param.h"
#include "ina228.h"
#include "adc_monitor.h"
#include "coulomb_counter.h"

// Blinking LED configuration
#define BLINK_LED_PIN 39  // GP39 for RP2350
#define BLINK_LED_INTERVAL_MS 500  // Blink every 500ms

// System state
typedef struct {
    bool pack_contactors_enabled;
    bool precharge_relay_enabled;
    uint32_t pack_contactors_start_time;
    uint32_t precharge_relay_start_time;
    bool initial_pulse_complete;
    bool precharge_initial_pulse_complete;
} system_state_t;

static system_state_t system_state = {0};

// Device instances
static ina228_t ina228_dev;
static coulomb_counter_t coulomb_counter;

// Command buffer
#define CMD_BUFFER_SIZE 256
static char cmd_buffer[CMD_BUFFER_SIZE];
static uint16_t cmd_index = 0;

// Function prototypes
void system_init(void);
void gpio_init_all(void);
void uart_init_esphome(void);
void i2c_init_ina228(void);
void pwm_init_contactors(void);
void process_command(const char* cmd);
void set_contactor_pwm(uint8_t duty_percent);
void set_precharge_pwm(uint8_t duty_percent);
void update_system_state(void);
void update_parameters(void);

/**
 * @brief Main entry point
 */
int main() {
    // Initialize system
    system_init();
    
    printf("\n");
    printf("========================================\n");
    printf("  Tesla Model 3 BMS Interface\n");
    printf("  RP2350A + INA228 + Internal ADC\n");
    printf("========================================\n");
    printf("\n");
    
    // Main loop timing
    uint32_t last_main_loop = 0;
    uint32_t last_esphome_update = 0;
    uint32_t last_display_update = 0;
    uint32_t last_blink = 0;
    bool blink_led_state = false;
    const uint32_t MAIN_LOOP_INTERVAL = 50;      // 50ms
    const uint32_t ESPHOME_UPDATE_INTERVAL = 1000;  // 1 second
    const uint32_t DISPLAY_UPDATE_INTERVAL = 5000;  // 5 seconds
    
    printf("System ready - entering main loop\n");
    printf("Type 'help' for available commands\n\n");
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Toggle blinking LED
        if (now - last_blink >= BLINK_LED_INTERVAL_MS) {
            last_blink = now;
            blink_led_state = !blink_led_state;
            gpio_put(BLINK_LED_PIN, blink_led_state);
        }
        
        // Main loop timing control
        if (now - last_main_loop >= MAIN_LOOP_INTERVAL) {
            last_main_loop = now;
            
            // Run BATMan state machine
            batman_loop();
            
            // Read pack voltages from internal ADC
            pack_voltage_t pack_voltages;
            adc_read_all_voltages(&pack_voltages);
            
            // Update coulomb counter
            coulomb_counter_update(&coulomb_counter, pack_voltages.pack_sum);
            
            // Update all system parameters
            update_parameters();
            
            // Handle contactor PWM timing
            update_system_state();
        }
        
        // Send data to ESPHome display
        if (now - last_esphome_update >= ESPHOME_UPDATE_INTERVAL) {
            last_esphome_update = now;
            param_send_to_esphome();
        }
        
        // Display status on USB serial
        if (now - last_display_update >= DISPLAY_UPDATE_INTERVAL) {
            last_display_update = now;
            
            printf("=== System Status ===\n");
            printf("Uptime: %lu ms\n", now);
            printf("Cells: %d\n", param_get_int(PARAM_CELLS_PRESENT));
            printf("Pack Voltage: %.1f V\n", param_get_float(PARAM_UDC));
            printf("Current: %.3f A\n", param_get_float(PARAM_CURRENT));
            printf("Power: %.1f W\n", param_get_float(PARAM_POWER_WATTS));
            printf("SOC: %.1f%%\n", param_get_float(PARAM_STATE_OF_CHARGE));
            printf("====================\n\n");
        }
        
        // Process USB serial commands
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\n' || c == '\r') {
                if (cmd_index > 0) {
                    cmd_buffer[cmd_index] = '\0';
                    process_command(cmd_buffer);
                    cmd_index = 0;
                }
            } else if (cmd_index < CMD_BUFFER_SIZE - 1) {
                cmd_buffer[cmd_index++] = (char)c;
            }
        }
        
        // Small sleep to prevent tight looping
        sleep_ms(1);
    }
    
    return 0;
}

/**
 * @brief Initialize all system components
 */
void system_init(void) {
    // Initialize standard I/O over USB
    stdio_init_all();
    sleep_ms(1000);  // Wait for USB to enumerate
    
    printf("Initializing system...\n");
    
    // Initialize GPIO
    gpio_init_all();
    
    // Initialize UART for ESPHome
    uart_init_esphome();
    
    // Initialize I2C for INA228
    i2c_init_ina228();
    
    // Initialize PWM for contactors
    pwm_init_contactors();
    
    // Initialize parameters
    param_init();
    
    // Initialize BATMan BMS interface
    batman_init();
    
    // Initialize ADC for pack voltage monitoring
    adc_monitor_init();
    
    // Initialize INA228 current sensor
    printf("Initializing INA228 current sensor...\n");
    if (ina228_init(&ina228_dev, INA228_I2C_INST, INA228_I2C_ADDR, 0.000025296f)) {
        printf("INA228 initialized successfully\n");
    } else {
        printf("WARNING: INA228 initialization failed\n");
    }
    
    // Initialize coulomb counter
    printf("Initializing coulomb counter...\n");
    coulomb_counter_init(&coulomb_counter, &ina228_dev);
    
    printf("System initialization complete!\n");
}

/**
 * @brief Initialize GPIO
 */
void gpio_init_all(void) {
    // Initialize LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);  // Turn on LED
    
    // Initialize blinking LED on GPIO 39
    gpio_init(BLINK_LED_PIN);
    gpio_set_dir(BLINK_LED_PIN, GPIO_OUT);
    gpio_put(BLINK_LED_PIN, 0);  // Start with LED off
    
    printf("GPIO: Initialized (LED on GP%d, Blink LED on GP%d)\n", LED_PIN, BLINK_LED_PIN);
}

/**
 * @brief Initialize UART for ESPHome display
 */
void uart_init_esphome(void) {
    // Initialize UART
    uart_init(ESPHOME_UART_INST, ESPHOME_UART_BAUD);
    
    // Set up UART pins
    gpio_set_function(ESPHOME_PIN_TX, GPIO_FUNC_UART);
    gpio_set_function(ESPHOME_PIN_RX, GPIO_FUNC_UART);
    
    printf("UART0: Initialized at %d baud (TX=GP%d, RX=GP%d)\n", 
           ESPHOME_UART_BAUD, ESPHOME_PIN_TX, ESPHOME_PIN_RX);
}

/**
 * @brief Initialize I2C for INA228
 */
void i2c_init_ina228(void) {
    // Initialize I2C
    i2c_init(INA228_I2C_INST, INA228_I2C_FREQ);
    
    // Set up I2C pins
    gpio_set_function(INA228_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(INA228_PIN_SCL, GPIO_FUNC_I2C);
    
    // Enable pull-ups (required for I2C)
    gpio_pull_up(INA228_PIN_SDA);
    gpio_pull_up(INA228_PIN_SCL);
    
    printf("I2C0: Initialized at %d Hz (SDA=GP%d, SCL=GP%d)\n", 
           INA228_I2C_FREQ, INA228_PIN_SDA, INA228_PIN_SCL);
}

/**
 * @brief Initialize PWM for contactor control
 */
void pwm_init_contactors(void) {
    // Configure pack contactors PWM
    gpio_set_function(PWM_PIN_PACK_CONTACTORS, GPIO_FUNC_PWM);
    uint slice_num_pack = pwm_gpio_to_slice_num(PWM_PIN_PACK_CONTACTORS);
    
    // Set PWM frequency
    float clk_div = (float)clock_get_hz(clk_sys) / (PWM_FREQUENCY * 256);
    pwm_set_clkdiv(slice_num_pack, clk_div);
    pwm_set_wrap(slice_num_pack, 255);
    pwm_set_gpio_level(PWM_PIN_PACK_CONTACTORS, 0);
    pwm_set_enabled(slice_num_pack, true);
    
    // Configure precharge relay PWM
    gpio_set_function(PWM_PIN_PRECHARGE_RELAY, GPIO_FUNC_PWM);
    uint slice_num_precharge = pwm_gpio_to_slice_num(PWM_PIN_PRECHARGE_RELAY);
    
    pwm_set_clkdiv(slice_num_precharge, clk_div);
    pwm_set_wrap(slice_num_precharge, 255);
    pwm_set_gpio_level(PWM_PIN_PRECHARGE_RELAY, 0);
    pwm_set_enabled(slice_num_precharge, true);
    
    printf("PWM: Initialized at %d Hz (Pack=GP%d, Precharge=GP%d)\n", 
           PWM_FREQUENCY, PWM_PIN_PACK_CONTACTORS, PWM_PIN_PRECHARGE_RELAY);
}

/**
 * @brief Set pack contactor PWM duty cycle
 */
void set_contactor_pwm(uint8_t duty_percent) {
    if (duty_percent > 100) duty_percent = 100;
    uint16_t level = (duty_percent * 255) / 100;
    pwm_set_gpio_level(PWM_PIN_PACK_CONTACTORS, level);
    printf("Pack Contactors: %d%% duty cycle\n", duty_percent);
}

/**
 * @brief Set precharge relay PWM duty cycle
 */
void set_precharge_pwm(uint8_t duty_percent) {
    if (duty_percent > 100) duty_percent = 100;
    uint16_t level = (duty_percent * 255) / 100;
    pwm_set_gpio_level(PWM_PIN_PRECHARGE_RELAY, level);
    printf("Precharge Relay: %d%% duty cycle\n", duty_percent);
}

/**
 * @brief Update system state (handle contactor timing)
 */
void update_system_state(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Handle pack contactors initial pulse timing
    if (system_state.pack_contactors_enabled && !system_state.initial_pulse_complete) {
        if (now - system_state.pack_contactors_start_time >= PWM_INITIAL_PULSE_MS) {
            set_contactor_pwm(PWM_DUTY_NORMAL);
            system_state.initial_pulse_complete = true;
        }
    }
    
    // Handle precharge relay initial pulse timing
    if (system_state.precharge_relay_enabled && !system_state.precharge_initial_pulse_complete) {
        if (now - system_state.precharge_relay_start_time >= PWM_INITIAL_PULSE_MS) {
            set_precharge_pwm(PWM_DUTY_NORMAL);
            system_state.precharge_initial_pulse_complete = true;
        }
    }
}

/**
 * @brief Update all parameters from sensors
 */
void update_parameters(void) {
    // Update pack voltages from ADC
    pack_voltage_t pack_voltages;
    adc_read_all_voltages(&pack_voltages);
    param_set_float(PARAM_BATT_CONTACTOR_POS, pack_voltages.pack_pos);
    param_set_float(PARAM_BATT_CONTACTOR_NEG, pack_voltages.pack_neg);
    param_set_float(PARAM_UDC, pack_voltages.pack_sum);
    param_set_float(PARAM_BATT_LINK_POS, pack_voltages.link_pos);
    param_set_float(PARAM_BATT_LINK_NEG, pack_voltages.link_neg);
    
    // Update current sensor data
    param_set_float(PARAM_CURRENT, coulomb_counter_get_current(&coulomb_counter));
    param_set_float(PARAM_POWER_WATTS, coulomb_counter_get_power(&coulomb_counter));
    param_set_float(PARAM_ENERGY_WH, coulomb_counter_get_energy_wh(&coulomb_counter));
    param_set_float(PARAM_ENERGY_KWH, coulomb_counter_get_energy_kwh(&coulomb_counter));
    param_set_float(PARAM_STATE_OF_CHARGE, coulomb_counter_get_soc(&coulomb_counter));
    param_set_float(PARAM_REMAINING_CAPACITY_AH, coulomb_counter_get_remaining_capacity(&coulomb_counter));
    
    // Update INA228 temperature
    param_set_float(PARAM_INA228_TEMP, ina228_read_temperature(&ina228_dev));
}

/**
 * @brief Process serial command
 */
void process_command(const char* cmd) {
    char lower_cmd[CMD_BUFFER_SIZE];
    strncpy(lower_cmd, cmd, CMD_BUFFER_SIZE - 1);
    
    // Convert to lowercase for comparison
    for (int i = 0; lower_cmd[i]; i++) {
        if (lower_cmd[i] >= 'A' && lower_cmd[i] <= 'Z') {
            lower_cmd[i] = lower_cmd[i] + 32;
        }
    }
    
    // Process commands
    if (strcmp(lower_cmd, "help") == 0) {
        printf("\nAvailable commands:\n");
        printf("  help                - Show this help message\n");
        printf("  status              - Show system status\n");
        printf("  balance on/off      - Enable/disable cell balancing\n");
        printf("  contactors on/off   - Enable/disable pack contactors\n");
        printf("  precharge on/off    - Enable/disable precharge relay\n");
        printf("  params              - List all parameters\n");
        printf("  ina228              - Show INA228 status\n");
        printf("  adc                 - Show ADC voltages\n");
        printf("  coulomb             - Show coulomb counter status\n");
        printf("  batman              - Show BATMan status\n\n");
    }
    else if (strcmp(lower_cmd, "status") == 0) {
        printf("\n=== System Status ===\n");
        printf("Uptime: %lu ms\n", to_ms_since_boot(get_absolute_time()));
        printf("Cells: %d\n", param_get_int(PARAM_CELLS_PRESENT));
        printf("Pack Voltage: %.1f V\n", param_get_float(PARAM_UDC));
        printf("Current: %.3f A\n", param_get_float(PARAM_CURRENT));
        printf("Power: %.1f W\n", param_get_float(PARAM_POWER_WATTS));
        printf("SOC: %.1f%%\n", param_get_float(PARAM_STATE_OF_CHARGE));
        printf("Contactors: %s\n", system_state.pack_contactors_enabled ? "ON" : "OFF");
        printf("Precharge: %s\n", system_state.precharge_relay_enabled ? "ON" : "OFF");
        printf("====================\n\n");
    }
    else if (strcmp(lower_cmd, "balance on") == 0) {
        batman_set_balance_enabled(true);
        param_set_int(PARAM_BALANCE, 1);
        printf("Balance ENABLED\n");
    }
    else if (strcmp(lower_cmd, "balance off") == 0) {
        batman_set_balance_enabled(false);
        param_set_int(PARAM_BALANCE, 0);
        printf("Balance DISABLED\n");
    }
    else if (strcmp(lower_cmd, "contactors on") == 0) {
        system_state.pack_contactors_enabled = true;
        system_state.pack_contactors_start_time = to_ms_since_boot(get_absolute_time());
        system_state.initial_pulse_complete = false;
        set_contactor_pwm(PWM_DUTY_INITIAL);
        printf("Pack Contactors ENABLED\n");
    }
    else if (strcmp(lower_cmd, "contactors off") == 0) {
        system_state.pack_contactors_enabled = false;
        system_state.initial_pulse_complete = false;
        set_contactor_pwm(0);
        printf("Pack Contactors DISABLED\n");
    }
    else if (strcmp(lower_cmd, "precharge on") == 0) {
        system_state.precharge_relay_enabled = true;
        system_state.precharge_relay_start_time = to_ms_since_boot(get_absolute_time());
        system_state.precharge_initial_pulse_complete = false;
        set_precharge_pwm(PWM_DUTY_INITIAL);
        printf("Precharge Relay ENABLED\n");
    }
    else if (strcmp(lower_cmd, "precharge off") == 0) {
        system_state.precharge_relay_enabled = false;
        system_state.precharge_initial_pulse_complete = false;
        set_precharge_pwm(0);
        printf("Precharge Relay DISABLED\n");
    }
    else if (strcmp(lower_cmd, "params") == 0) {
        param_print_all();
    }
    else if (strcmp(lower_cmd, "ina228") == 0) {
        ina228_print_status(&ina228_dev);
    }
    else if (strcmp(lower_cmd, "adc") == 0) {
        pack_voltage_t voltages;
        adc_read_all_voltages(&voltages);
        printf("\n=== ADC Pack Voltages ===\n");
        printf("Pack Neg: %.3f V\n", voltages.pack_neg);
        printf("Pack Pos: %.3f V\n", voltages.pack_pos);
        printf("Pack Sum: %.3f V\n", voltages.pack_sum);
        printf("Link Neg: %.3f V\n", voltages.link_neg);
        printf("Link Pos: %.3f V\n", voltages.link_pos);
        printf("========================\n\n");
    }
    else if (strcmp(lower_cmd, "coulomb") == 0) {
        coulomb_counter_print_status(&coulomb_counter);
    }
    else if (strcmp(lower_cmd, "batman") == 0) {
        batman_print_hardware_mapping();
    }
    else {
        printf("Unknown command: '%s'\n", cmd);
        printf("Type 'help' for available commands\n");
    }
}

