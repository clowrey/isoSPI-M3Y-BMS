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
#include "can_interface.h"
#include "isospi_interface.h"
#include "isosnoop.h"
#include "bmb_test.h"

// Blinking LED configuration
#define BLINK_LED_PIN 39  // GP39 for RP2350
#define BLINK_LED_INTERVAL_MS 500  // Blink every 500ms

// System state
typedef struct {
    bool link_neg_enabled;
    bool link_pos_enabled;
    bool fc_pos_enabled;  // FC Positive acts as precharge
    bool fc_neg_enabled;
    uint32_t link_neg_start_time;
    uint32_t link_pos_start_time;
    uint32_t fc_pos_start_time;
    uint32_t fc_neg_start_time;
    bool link_neg_initial_pulse_complete;
    bool link_pos_initial_pulse_complete;
    bool fc_pos_initial_pulse_complete;
    bool fc_neg_initial_pulse_complete;
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
void set_link_neg_pwm(uint8_t duty_percent);
void set_link_pos_pwm(uint8_t duty_percent);
void set_fc_pos_pwm(uint8_t duty_percent);
void set_fc_neg_pwm(uint8_t duty_percent);
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
    printf("  Mode: BATMan + isoSPI Master + Snooper\n");
    printf("  All interfaces active simultaneously\n");
    printf("========================================\n");
    printf("\n");
    
    // Main loop timing
    uint32_t last_main_loop = 0;
    uint32_t last_esphome_update = 0;
    uint32_t last_display_update = 0;
    uint32_t last_blink = 0;
    uint32_t last_can_broadcast = 0;
    uint32_t last_snoop_print = 0;
    bool blink_led_state = false;
    const uint32_t MAIN_LOOP_INTERVAL = 50;      // 50ms
    const uint32_t ESPHOME_UPDATE_INTERVAL = 1000;  // 1 second
    const uint32_t DISPLAY_UPDATE_INTERVAL = 5000;  // 5 seconds
    const uint32_t CAN_BROADCAST_INTERVAL = 100;  // 100ms (10Hz)
    const uint32_t SNOOP_PRINT_INTERVAL = 500;   // 500ms for snooper output
    
    printf("System ready - entering main loop\n");
    printf("BATMan running with isoSPI snooper monitoring traffic\n");
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
            
            // CL: Run BATMan to generate isoSPI traffic for snooper to capture
            batman_loop();
            
            // BMB test loop (if continuous mode enabled)
            bmb_test_loop();
            
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
        
        // Print isoSPI snooper data
        if (now - last_snoop_print >= SNOOP_PRINT_INTERVAL) {
            last_snoop_print = now;
            printf("isoSPI: ");
            isosnoop_print_buffer();
        }
        
        // Send data to ESPHome display
        if (now - last_esphome_update >= ESPHOME_UPDATE_INTERVAL) {
            last_esphome_update = now;
            param_send_to_esphome();
        }
        
        // Broadcast pack status on CAN bus
        if (now - last_can_broadcast >= CAN_BROADCAST_INTERVAL) {
            last_can_broadcast = now;
            can_interface_broadcast_pack_status();
            
            // Process any received CAN messages
            can_message_t rx_msg;
            while (can_interface_receive(&rx_msg)) {
                // Log received CAN messages
                // (Could be used for remote control in future)
            }
        }
        
        // Display status on USB serial (disabled for now)
        /*
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
        */
        
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
    
    // CL: Initialize BATMan - it will generate isoSPI traffic
    batman_init();
    
    // CL: Initialize isoSPI master and snooper (both active at same time)
    printf("Initializing isoSPI interface...\n");
    isospi_interface_init();
    printf("isoSPI master and snooper initialized - both active alongside BATMan\n");
    
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
    
    // Initialize CAN interface
    printf("Initializing CAN interface...\n");
    if (can_interface_init()) {
        printf("CAN interface initialized successfully\n");
    } else {
        printf("WARNING: CAN interface initialization failed\n");
    }
    
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
    // Calculate PWM frequency divider
    float clk_div = (float)clock_get_hz(clk_sys) / (PWM_FREQUENCY * 256);
    
    // Configure Link Negative Contactor PWM
    gpio_set_function(PWM_PIN_LINK_NEG_CONTACTOR, GPIO_FUNC_PWM);
    uint slice_num_link_neg = pwm_gpio_to_slice_num(PWM_PIN_LINK_NEG_CONTACTOR);
    pwm_set_clkdiv(slice_num_link_neg, clk_div);
    pwm_set_wrap(slice_num_link_neg, 255);
    pwm_set_gpio_level(PWM_PIN_LINK_NEG_CONTACTOR, 0);
    pwm_set_enabled(slice_num_link_neg, true);
    
    // Configure Link Positive Contactor PWM
    gpio_set_function(PWM_PIN_LINK_POS_CONTACTOR, GPIO_FUNC_PWM);
    uint slice_num_link_pos = pwm_gpio_to_slice_num(PWM_PIN_LINK_POS_CONTACTOR);
    pwm_set_clkdiv(slice_num_link_pos, clk_div);
    pwm_set_wrap(slice_num_link_pos, 255);
    pwm_set_gpio_level(PWM_PIN_LINK_POS_CONTACTOR, 0);
    pwm_set_enabled(slice_num_link_pos, true);
    
    // Configure FC Positive Contactor PWM (Precharge)
    gpio_set_function(PWM_PIN_FC_POS_CONTACTOR, GPIO_FUNC_PWM);
    uint slice_num_fc_pos = pwm_gpio_to_slice_num(PWM_PIN_FC_POS_CONTACTOR);
    pwm_set_clkdiv(slice_num_fc_pos, clk_div);
    pwm_set_wrap(slice_num_fc_pos, 255);
    pwm_set_gpio_level(PWM_PIN_FC_POS_CONTACTOR, 0);
    pwm_set_enabled(slice_num_fc_pos, true);
    
    // Configure FC Negative Contactor PWM
    gpio_set_function(PWM_PIN_FC_NEG_CONTACTOR, GPIO_FUNC_PWM);
    uint slice_num_fc_neg = pwm_gpio_to_slice_num(PWM_PIN_FC_NEG_CONTACTOR);
    pwm_set_clkdiv(slice_num_fc_neg, clk_div);
    pwm_set_wrap(slice_num_fc_neg, 255);
    pwm_set_gpio_level(PWM_PIN_FC_NEG_CONTACTOR, 0);
    pwm_set_enabled(slice_num_fc_neg, true);
    
    printf("PWM: Initialized at %d Hz\n", PWM_FREQUENCY);
    printf("  Link Neg=GP%d, Link Pos=GP%d\n", PWM_PIN_LINK_NEG_CONTACTOR, PWM_PIN_LINK_POS_CONTACTOR);
    printf("  FC Pos=GP%d, FC Neg=GP%d\n", PWM_PIN_FC_POS_CONTACTOR, PWM_PIN_FC_NEG_CONTACTOR);
}

/**
 * @brief Set Link Negative Contactor PWM duty cycle
 */
void set_link_neg_pwm(uint8_t duty_percent) {
    if (duty_percent > 100) duty_percent = 100;
    uint16_t level = (duty_percent * 255) / 100;
    pwm_set_gpio_level(PWM_PIN_LINK_NEG_CONTACTOR, level);
    printf("Link Neg Contactor: %d%% duty cycle\n", duty_percent);
}

/**
 * @brief Set Link Positive Contactor PWM duty cycle
 */
void set_link_pos_pwm(uint8_t duty_percent) {
    if (duty_percent > 100) duty_percent = 100;
    uint16_t level = (duty_percent * 255) / 100;
    pwm_set_gpio_level(PWM_PIN_LINK_POS_CONTACTOR, level);
    printf("Link Pos Contactor: %d%% duty cycle\n", duty_percent);
}

/**
 * @brief Set FC Positive Contactor (Precharge) PWM duty cycle
 */
void set_fc_pos_pwm(uint8_t duty_percent) {
    if (duty_percent > 100) duty_percent = 100;
    uint16_t level = (duty_percent * 255) / 100;
    pwm_set_gpio_level(PWM_PIN_FC_POS_CONTACTOR, level);
    printf("FC Pos Contactor (Precharge): %d%% duty cycle\n", duty_percent);
}

/**
 * @brief Set FC Negative Contactor PWM duty cycle
 */
void set_fc_neg_pwm(uint8_t duty_percent) {
    if (duty_percent > 100) duty_percent = 100;
    uint16_t level = (duty_percent * 255) / 100;
    pwm_set_gpio_level(PWM_PIN_FC_NEG_CONTACTOR, level);
    printf("FC Neg Contactor: %d%% duty cycle\n", duty_percent);
}

/**
 * @brief Update system state (handle contactor timing)
 */
void update_system_state(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Handle Link Negative Contactor initial pulse timing
    if (system_state.link_neg_enabled && !system_state.link_neg_initial_pulse_complete) {
        if (now - system_state.link_neg_start_time >= PWM_INITIAL_PULSE_MS) {
            set_link_neg_pwm(PWM_DUTY_NORMAL);
            system_state.link_neg_initial_pulse_complete = true;
        }
    }
    
    // Handle Link Positive Contactor initial pulse timing
    if (system_state.link_pos_enabled && !system_state.link_pos_initial_pulse_complete) {
        if (now - system_state.link_pos_start_time >= PWM_INITIAL_PULSE_MS) {
            set_link_pos_pwm(PWM_DUTY_NORMAL);
            system_state.link_pos_initial_pulse_complete = true;
        }
    }
    
    // Handle FC Positive Contactor (Precharge) initial pulse timing
    if (system_state.fc_pos_enabled && !system_state.fc_pos_initial_pulse_complete) {
        if (now - system_state.fc_pos_start_time >= PWM_INITIAL_PULSE_MS) {
            set_fc_pos_pwm(PWM_DUTY_NORMAL);
            system_state.fc_pos_initial_pulse_complete = true;
        }
    }
    
    // Handle FC Negative Contactor initial pulse timing
    if (system_state.fc_neg_enabled && !system_state.fc_neg_initial_pulse_complete) {
        if (now - system_state.fc_neg_start_time >= PWM_INITIAL_PULSE_MS) {
            set_fc_neg_pwm(PWM_DUTY_NORMAL);
            system_state.fc_neg_initial_pulse_complete = true;
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
        printf("\n=================================================\n");
        printf("  BATMan + isoSPI Snooper - Traffic Monitoring\n");
        printf("=================================================\n");
        printf("\nAvailable commands:\n");
        printf("  help                - Show this help message\n");
        printf("  status              - Show system status\n");
        printf("  balance on/off      - Enable/disable cell balancing\n");
        printf("  contactors on/off   - Enable/disable all contactors (sequenced)\n");
        printf("  link_neg on/off     - Enable/disable Link Negative contactor\n");
        printf("  link_pos on/off     - Enable/disable Link Positive contactor\n");
        printf("  fc_pos on/off       - Enable/disable FC Positive (precharge) contactor\n");
        printf("  fc_neg on/off       - Enable/disable FC Negative contactor\n");
        printf("  params              - List all parameters\n");
        printf("  ina228              - Show INA228 status\n");
        printf("  adc                 - Show ADC voltages\n");
        printf("  coulomb             - Show coulomb counter status\n");
        printf("  batman              - Show BATMan status\n");
        printf("  can                 - Show CAN bus statistics\n");
        printf("  isospi init         - Initialize isoSPI interface\n");
        printf("  isospi enable       - Switch to isoSPI master (disable Batman)\n");
        printf("  batman enable       - Switch to Batman (disable isoSPI)\n");
        printf("  batman stop         - Stop automatic Batman polling\n");
        printf("  batman start        - Start automatic Batman polling\n");
        printf("  test stop           - Stop ALL automatic testing (batman + bmb)\n");
        printf("  test start          - Start automatic testing\n");
        printf("  isospi test         - Run isoSPI test pattern\n");
        printf("  isospi snoop        - Print captured bus traffic\n");
        printf("  isospi status       - Show isoSPI interface status\n");
        printf("  snoop diag          - Show snooper diagnostics and pin states\n");
        printf("  bmb test            - Run BMB test once (uses active interface)\n");
        printf("  bmb continuous on   - Enable continuous BMB testing\n");
        printf("  bmb continuous off  - Disable continuous BMB testing\n\n");
    }
    else if (strcmp(lower_cmd, "status") == 0) {
        printf("\n=== System Status ===\n");
        printf("Uptime: %lu ms\n", to_ms_since_boot(get_absolute_time()));
        printf("Cells: %d\n", param_get_int(PARAM_CELLS_PRESENT));
        printf("Pack Voltage: %.1f V\n", param_get_float(PARAM_UDC));
        printf("Current: %.3f A\n", param_get_float(PARAM_CURRENT));
        printf("Power: %.1f W\n", param_get_float(PARAM_POWER_WATTS));
        printf("SOC: %.1f%%\n", param_get_float(PARAM_STATE_OF_CHARGE));
        printf("Link Neg: %s\n", system_state.link_neg_enabled ? "ON" : "OFF");
        printf("Link Pos: %s\n", system_state.link_pos_enabled ? "ON" : "OFF");
        printf("FC Pos (Precharge): %s\n", system_state.fc_pos_enabled ? "ON" : "OFF");
        printf("FC Neg: %s\n", system_state.fc_neg_enabled ? "ON" : "OFF");
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
        // CL: Sequenced contactor activation: Link Neg → FC Pos (precharge) → Link Pos
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Enable Link Negative first
        system_state.link_neg_enabled = true;
        system_state.link_neg_start_time = now;
        system_state.link_neg_initial_pulse_complete = false;
        set_link_neg_pwm(PWM_DUTY_INITIAL);
        
        // Enable FC Positive (Precharge) after 50ms delay
        sleep_ms(50);
        now = to_ms_since_boot(get_absolute_time());
        system_state.fc_pos_enabled = true;
        system_state.fc_pos_start_time = now;
        system_state.fc_pos_initial_pulse_complete = false;
        set_fc_pos_pwm(PWM_DUTY_INITIAL);
        
        // Enable Link Positive after additional 500ms precharge delay
        sleep_ms(500);
        now = to_ms_since_boot(get_absolute_time());
        system_state.link_pos_enabled = true;
        system_state.link_pos_start_time = now;
        system_state.link_pos_initial_pulse_complete = false;
        set_link_pos_pwm(PWM_DUTY_INITIAL);
        
        printf("All Contactors ENABLED (Sequenced: Link Neg → FC Pos → Link Pos)\n");
    }
    else if (strcmp(lower_cmd, "contactors off") == 0) {
        // CL: Disable all contactors
        system_state.link_neg_enabled = false;
        system_state.link_pos_enabled = false;
        system_state.fc_pos_enabled = false;
        system_state.fc_neg_enabled = false;
        system_state.link_neg_initial_pulse_complete = false;
        system_state.link_pos_initial_pulse_complete = false;
        system_state.fc_pos_initial_pulse_complete = false;
        system_state.fc_neg_initial_pulse_complete = false;
        set_link_neg_pwm(0);
        set_link_pos_pwm(0);
        set_fc_pos_pwm(0);
        set_fc_neg_pwm(0);
        printf("All Contactors DISABLED\n");
    }
    else if (strcmp(lower_cmd, "link_neg on") == 0) {
        system_state.link_neg_enabled = true;
        system_state.link_neg_start_time = to_ms_since_boot(get_absolute_time());
        system_state.link_neg_initial_pulse_complete = false;
        set_link_neg_pwm(PWM_DUTY_INITIAL);
        printf("Link Neg Contactor ENABLED\n");
    }
    else if (strcmp(lower_cmd, "link_neg off") == 0) {
        system_state.link_neg_enabled = false;
        system_state.link_neg_initial_pulse_complete = false;
        set_link_neg_pwm(0);
        printf("Link Neg Contactor DISABLED\n");
    }
    else if (strcmp(lower_cmd, "link_pos on") == 0) {
        system_state.link_pos_enabled = true;
        system_state.link_pos_start_time = to_ms_since_boot(get_absolute_time());
        system_state.link_pos_initial_pulse_complete = false;
        set_link_pos_pwm(PWM_DUTY_INITIAL);
        printf("Link Pos Contactor ENABLED\n");
    }
    else if (strcmp(lower_cmd, "link_pos off") == 0) {
        system_state.link_pos_enabled = false;
        system_state.link_pos_initial_pulse_complete = false;
        set_link_pos_pwm(0);
        printf("Link Pos Contactor DISABLED\n");
    }
    else if (strcmp(lower_cmd, "fc_pos on") == 0) {
        system_state.fc_pos_enabled = true;
        system_state.fc_pos_start_time = to_ms_since_boot(get_absolute_time());
        system_state.fc_pos_initial_pulse_complete = false;
        set_fc_pos_pwm(PWM_DUTY_INITIAL);
        printf("FC Pos Contactor (Precharge) ENABLED\n");
    }
    else if (strcmp(lower_cmd, "fc_pos off") == 0) {
        system_state.fc_pos_enabled = false;
        system_state.fc_pos_initial_pulse_complete = false;
        set_fc_pos_pwm(0);
        printf("FC Pos Contactor (Precharge) DISABLED\n");
    }
    else if (strcmp(lower_cmd, "fc_neg on") == 0) {
        system_state.fc_neg_enabled = true;
        system_state.fc_neg_start_time = to_ms_since_boot(get_absolute_time());
        system_state.fc_neg_initial_pulse_complete = false;
        set_fc_neg_pwm(PWM_DUTY_INITIAL);
        printf("FC Neg Contactor ENABLED\n");
    }
    else if (strcmp(lower_cmd, "fc_neg off") == 0) {
        system_state.fc_neg_enabled = false;
        system_state.fc_neg_initial_pulse_complete = false;
        set_fc_neg_pwm(0);
        printf("FC Neg Contactor DISABLED\n");
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
    else if (strcmp(lower_cmd, "can") == 0) {
        uint32_t rx_count, tx_count, error_count;
        can_interface_get_stats(&rx_count, &tx_count, &error_count);
        printf("\n=== CAN Bus Statistics ===\n");
        printf("RX Messages: %lu\n", rx_count);
        printf("TX Messages: %lu\n", tx_count);
        printf("Errors: %lu\n", error_count);
        printf("========================\n\n");
    }
    else if (strcmp(lower_cmd, "isospi init") == 0) {
        isospi_interface_init();
    }
    else if (strcmp(lower_cmd, "isospi enable") == 0) {
        isospi_interface_enable();
    }
    else if (strcmp(lower_cmd, "batman enable") == 0) {
        isospi_interface_disable();
    }
    else if (strcmp(lower_cmd, "batman stop") == 0) {
        batman_set_enabled(false);
        printf("Batman automatic polling STOPPED\n");
    }
    else if (strcmp(lower_cmd, "batman start") == 0) {
        batman_set_enabled(true);
        printf("Batman automatic polling STARTED\n");
    }
    else if (strcmp(lower_cmd, "test stop") == 0) {
        batman_set_enabled(false);
        bmb_test_set_continuous(false);
        printf("ALL automatic testing STOPPED (Batman + BMB)\n");
    }
    else if (strcmp(lower_cmd, "test start") == 0) {
        batman_set_enabled(true);
        printf("Automatic testing STARTED (Batman enabled)\n");
    }
    else if (strcmp(lower_cmd, "isospi test") == 0) {
        isospi_interface_test();
    }
    else if (strcmp(lower_cmd, "isospi snoop") == 0) {
        isospi_interface_print_snoop();
    }
    else if (strcmp(lower_cmd, "isospi status") == 0) {
        isospi_interface_print_status();
    }
    else if (strcmp(lower_cmd, "snoop diag") == 0) {
        printf("\n=== isoSPI Snooper Diagnostics ===\n");
        printf("Monitoring GP%d and GP%d\n", ISOSPI_RX_PIN_BASE, ISOSPI_RX_PIN_BASE + 1);
        
        printf("\nPin States (sample 20 times):\n");
        for (int i = 0; i < 20; i++) {
            bool pin0 = gpio_get(ISOSPI_RX_PIN_BASE);
            bool pin1 = gpio_get(ISOSPI_RX_PIN_BASE + 1);
            printf("  Sample %2d: GP%d=%d GP%d=%d (0b%d%d)\n", 
                   i, ISOSPI_RX_PIN_BASE, pin0, ISOSPI_RX_PIN_BASE + 1, pin1, pin0, pin1);
            sleep_ms(10);
        }
        printf("================================\n\n");
    }
    else if (strcmp(lower_cmd, "bmb test") == 0) {
        bmb_test_run_once();
    }
    else if (strcmp(lower_cmd, "bmb continuous on") == 0) {
        // CL: Disable batman automatic loop when using manual BMB test
        batman_set_enabled(false);
        bmb_test_set_continuous(true);
    }
    else if (strcmp(lower_cmd, "bmb continuous off") == 0) {
        bmb_test_set_continuous(false);
        // CL: Re-enable batman loop if using batman interface
        if (isospi_interface_get_active() == INTERFACE_BATMAN) {
            batman_set_enabled(true);
        }
    }
    else {
        printf("Unknown command: '%s'\n", cmd);
        printf("Type 'help' for available commands\n");
    }
}

