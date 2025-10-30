/**
 * @file can_interface.c
 * @brief CAN Bus Interface Implementation using can2040
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"

#include "can_interface.h"
#include "pin_config.h"
#include "param.h"

// Include can2040 library
#include "../../can2040/src/can2040.h"

// CL: Message queue for received CAN messages (IRQ safe, single-core)
#define CAN_QUEUE_SIZE 64  // Must be power of 2
static struct {
    uint32_t pull_pos;
    volatile uint32_t push_pos;
    can_message_t queue[CAN_QUEUE_SIZE];
} rx_message_queue = {0};

// CL: Internal storage for can2040 module
static struct can2040 can_bus;
static bool can_initialized = false;
static volatile uint32_t can_error_count = 0;

// CL: Forward declarations
static void can2040_callback(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg);
static void pio_irq_handler(void);

/**
 * @brief Main CAN callback (called from IRQ handler)
 */
static void can2040_callback(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg) {
    if (notify == CAN2040_NOTIFY_RX) {
        // CL: Add received message to queue
        uint32_t push_pos = rx_message_queue.push_pos;
        uint32_t pull_pos = rx_message_queue.pull_pos;
        
        // Check if queue is full
        if ((push_pos - pull_pos) >= CAN_QUEUE_SIZE) {
            // Queue full - drop message
            return;
        }
        
        // Copy message to queue
        can_message_t *qmsg = &rx_message_queue.queue[push_pos % CAN_QUEUE_SIZE];
        qmsg->id = msg->id;
        qmsg->dlc = msg->dlc;
        memcpy(qmsg->data, msg->data, 8);
        
        // Update push position
        rx_message_queue.push_pos = push_pos + 1;
    }
    else if (notify == CAN2040_NOTIFY_ERROR) {
        // CL: Increment error counter
        can_error_count++;
    }
}

/**
 * @brief PIO interrupt handler for can2040
 */
static void pio_irq_handler(void) {
    can2040_pio_irq_handler(&can_bus);
}

/**
 * @brief Initialize the CAN bus interface
 */
bool can_interface_init(void) {
    if (can_initialized) {
        return true;
    }
    
    printf("Initializing CAN interface...\n");
    printf("  PIO: %d\n", CAN_PIO_NUM);
    printf("  RX Pin: GP%d\n", CAN_PIN_RX);
    printf("  TX Pin: GP%d\n", CAN_PIN_TX);
    printf("  Bitrate: %d bps\n", CAN_BITRATE);
    
    // CL: Setup can2040
    can2040_setup(&can_bus, CAN_PIO_NUM);
    can2040_callback_config(&can_bus, can2040_callback);
    
    // CL: Enable IRQ for PIO0
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
    irq_set_priority(PIO0_IRQ_0, 1);
    irq_set_enabled(PIO0_IRQ_0, true);
    
    // CL: Start CAN bus
    uint32_t sys_clock = clock_get_hz(clk_sys);
    can2040_start(&can_bus, sys_clock, CAN_BITRATE, CAN_PIN_RX, CAN_PIN_TX);
    
    can_initialized = true;
    printf("CAN interface initialized successfully (sys_clock=%u Hz)\n", sys_clock);
    
    return true;
}

/**
 * @brief Send a CAN message
 */
bool can_interface_send(const can_message_t *msg) {
    if (!can_initialized) {
        return false;
    }
    
    // CL: Check if transmit buffer is available
    if (!can2040_check_transmit(&can_bus)) {
        return false;  // Transmit buffer full
    }
    
    // CL: Prepare can2040 message
    struct can2040_msg tx_msg;
    tx_msg.id = msg->id;
    tx_msg.dlc = msg->dlc;
    memcpy(tx_msg.data, msg->data, 8);
    
    // CL: Transmit message
    int result = can2040_transmit(&can_bus, &tx_msg);
    return (result >= 0);
}

/**
 * @brief Check if a message is available
 */
bool can_interface_has_message(void) {
    uint32_t push_pos = rx_message_queue.push_pos;
    uint32_t pull_pos = rx_message_queue.pull_pos;
    return (push_pos != pull_pos);
}

/**
 * @brief Receive a CAN message
 */
bool can_interface_receive(can_message_t *msg) {
    uint32_t push_pos = rx_message_queue.push_pos;
    uint32_t pull_pos = rx_message_queue.pull_pos;
    
    // CL: Check if queue is empty
    if (push_pos == pull_pos) {
        return false;
    }
    
    // CL: Pop message from queue
    can_message_t *qmsg = &rx_message_queue.queue[pull_pos % CAN_QUEUE_SIZE];
    *msg = *qmsg;
    
    // CL: Update pull position
    rx_message_queue.pull_pos = pull_pos + 1;
    
    return true;
}

/**
 * @brief Get CAN statistics
 */
void can_interface_get_stats(uint32_t *rx_count, uint32_t *tx_count, uint32_t *error_count) {
    if (!can_initialized) {
        *rx_count = 0;
        *tx_count = 0;
        *error_count = 0;
        return;
    }
    
    struct can2040_stats stats;
    can2040_get_statistics(&can_bus, &stats);
    
    *rx_count = stats.rx_total;
    *tx_count = stats.tx_total;
    *error_count = can_error_count + stats.parse_error;
}

/**
 * @brief Broadcast BMS pack status on CAN bus
 * 
 * CL: Sends multiple CAN messages with pack data:
 * - 0x100: Pack voltage and current
 * - 0x101: SOC and cell count
 * - 0x102: Min/max cell voltages
 * - 0x103: Temperatures
 */
void can_interface_broadcast_pack_status(void) {
    if (!can_initialized) {
        return;
    }
    
    can_message_t msg;
    
    // CL: Message 1 (0x100): Pack voltage and current
    msg.id = 0x100;
    msg.dlc = 8;
    
    // Pack voltage (0.1V resolution)
    float pack_voltage = param_get_float(PARAM_UDC);
    uint16_t pack_v = (uint16_t)(pack_voltage * 10.0f);
    msg.data[0] = pack_v & 0xFF;
    msg.data[1] = (pack_v >> 8) & 0xFF;
    
    // Current (0.1A resolution, signed)
    float current = param_get_float(PARAM_CURRENT);
    int16_t current_val = (int16_t)(current * 10.0f);
    msg.data[2] = current_val & 0xFF;
    msg.data[3] = (current_val >> 8) & 0xFF;
    
    // Power (1W resolution, signed)
    float power = param_get_float(PARAM_POWER_WATTS);
    int16_t power_val = (int16_t)power;
    msg.data[4] = power_val & 0xFF;
    msg.data[5] = (power_val >> 8) & 0xFF;
    
    msg.data[6] = 0;
    msg.data[7] = 0;
    
    can_interface_send(&msg);
    
    // CL: Message 2 (0x101): SOC and cell count
    msg.id = 0x101;
    msg.dlc = 8;
    
    // SOC (0.1% resolution)
    float soc = param_get_float(PARAM_STATE_OF_CHARGE);
    uint16_t soc_val = (uint16_t)(soc * 10.0f);
    msg.data[0] = soc_val & 0xFF;
    msg.data[1] = (soc_val >> 8) & 0xFF;
    
    // Cell count
    uint16_t cell_count = param_get_int(PARAM_CELLS_PRESENT);
    msg.data[2] = cell_count & 0xFF;
    msg.data[3] = (cell_count >> 8) & 0xFF;
    
    // Min cell voltage (1mV resolution)
    float min_cell = param_get_float(PARAM_CELL_VMIN);
    uint16_t min_cell_val = (uint16_t)(min_cell * 1000.0f);
    msg.data[4] = min_cell_val & 0xFF;
    msg.data[5] = (min_cell_val >> 8) & 0xFF;
    
    // Max cell voltage (1mV resolution)
    float max_cell = param_get_float(PARAM_CELL_VMAX);
    uint16_t max_cell_val = (uint16_t)(max_cell * 1000.0f);
    msg.data[6] = max_cell_val & 0xFF;
    msg.data[7] = (max_cell_val >> 8) & 0xFF;
    
    can_interface_send(&msg);
    
    // CL: Message 3 (0x102): Temperatures
    msg.id = 0x102;
    msg.dlc = 8;
    
    // Max temperature (0.1°C resolution, offset by 40°C)
    float max_temp = param_get_float(PARAM_TEMP_MAX);
    uint8_t max_temp_val = (uint8_t)((max_temp + 40.0f) * 10.0f);
    msg.data[0] = max_temp_val;
    
    // Min temperature (0.1°C resolution, offset by 40°C)
    float min_temp = param_get_float(PARAM_TEMP_MIN);
    uint8_t min_temp_val = (uint8_t)((min_temp + 40.0f) * 10.0f);
    msg.data[1] = min_temp_val;
    
    // INA228 temperature (0.1°C resolution, offset by 40°C)
    float ina_temp = param_get_float(PARAM_INA228_TEMP);
    uint8_t ina_temp_val = (uint8_t)((ina_temp + 40.0f) * 10.0f);
    msg.data[2] = ina_temp_val;
    
    msg.data[3] = 0;
    msg.data[4] = 0;
    msg.data[5] = 0;
    msg.data[6] = 0;
    msg.data[7] = 0;
    
    can_interface_send(&msg);
}

