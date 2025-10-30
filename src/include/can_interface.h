/**
 * @file can_interface.h
 * @brief CAN Bus Interface using can2040 library
 * 
 * Provides CAN bus communication for BMS data broadcasting.
 */

#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// CAN message structure for easy access
typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} can_message_t;

/**
 * @brief Initialize the CAN bus interface
 * @return true if successful, false otherwise
 */
bool can_interface_init(void);

/**
 * @brief Send a CAN message
 * @param msg Pointer to CAN message to send
 * @return true if message was queued successfully, false otherwise
 */
bool can_interface_send(const can_message_t *msg);

/**
 * @brief Check if a message is available to receive
 * @return true if message available, false otherwise
 */
bool can_interface_has_message(void);

/**
 * @brief Receive a CAN message from the queue
 * @param msg Pointer to store received message
 * @return true if message received, false if queue empty
 */
bool can_interface_receive(can_message_t *msg);

/**
 * @brief Get CAN bus statistics
 * @param rx_count Pointer to store RX count
 * @param tx_count Pointer to store TX count
 * @param error_count Pointer to store error count
 */
void can_interface_get_stats(uint32_t *rx_count, uint32_t *tx_count, uint32_t *error_count);

/**
 * @brief Broadcast BMS pack summary on CAN bus
 * 
 * Sends pack voltage, current, SOC, and temperature over CAN
 */
void can_interface_broadcast_pack_status(void);

#endif // CAN_INTERFACE_H

