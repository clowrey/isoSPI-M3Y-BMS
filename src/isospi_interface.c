/**
 * @file isospi_interface.c
 * @brief High-level isoSPI Interface Management
 * 
 * CL: Both Batman SPI and isoSPI PIO interfaces run simultaneously.
 * The snooper monitors all traffic on the bus from both sources.
 */

#include "isospi_interface.h"
#include "isospi_master.h"
#include "isosnoop.h"
#include "batman.h"
#include "pin_config.h"
#include <stdio.h>
#include <string.h>

static interface_type_t active_interface = INTERFACE_ISOSPI; // CL: Both run together
static bool isospi_initialized = false;

bool isospi_interface_init(void) {
    if (isospi_initialized) {
        printf("isoSPI interface already initialized\n");
        return true;
    }
    
    printf("Initializing isoSPI interface...\n");
    
    // Initialize master (PIO2 SM0)
    isospi_master_setup(ISOSPI_TX_PIN_BASE, ISOSPI_RX_PIN_BASE);
    
    // Initialize snooper (PIO1 SM1)
    // CL: Use invert=1 since signals are normally high, active low
    isosnoop_setup(ISOSPI_RX_PIN_BASE, 1, ISOSPI_SAMPLING_PIN);  // 1 = inversion
    
    isospi_initialized = true;
    printf("isoSPI interface initialized successfully\n");
    
    return true;
}

void isospi_interface_enable(void) {
    if (!isospi_initialized) {
        printf("Error: isoSPI interface not initialized. Call isospi init first.\n");
        return;
    }
    
    // CL: Both interfaces run simultaneously - no need to disable Batman
    active_interface = INTERFACE_ISOSPI;
    printf("isoSPI interface active (Batman continues running)\n");
}

void isospi_interface_disable(void) {
    if (active_interface == INTERFACE_BATMAN) {
        printf("Batman interface already active\n");
        return;
    }
    
    // CL: Both interfaces run simultaneously - this just changes the active_interface flag
    active_interface = INTERFACE_BATMAN;
    printf("Active interface set to Batman (isoSPI master continues running)\n");
}

interface_type_t isospi_interface_get_active(void) {
    return active_interface;
}

void isospi_interface_test(void) {
    if (!isospi_initialized) {
        printf("Error: isoSPI interface not initialized\n");
        return;
    }
    
    // CL: No need to check active_interface - both run simultaneously
    printf("\n=== isoSPI Test Pattern ===\n");
    
    // Test pattern from original isosnooper
    char tx[] = {0b10101010, 0b11111111, 0b00000000, 0b11001100, 0b00110011};
    char rx[sizeof(tx)] = {0};
    
    printf("TX: ");
    for (size_t i = 0; i < sizeof(tx); i++) {
        printf("0x%02X ", (uint8_t)tx[i]);
    }
    printf("\n");
    
    bool valid = isospi_write_read_blocking(tx, rx, sizeof(tx));
    
    printf("RX: ");
    for (size_t i = 0; i < sizeof(rx); i++) {
        printf("0x%02X ", (uint8_t)rx[i]);
    }
    printf("\n");
    
    printf("Valid: %s\n", valid ? "YES" : "NO");
    printf("=========================\n\n");
}

void isospi_interface_print_snoop(void) {
    if (!isospi_initialized) {
        printf("Error: isoSPI interface not initialized\n");
        return;
    }
    
    printf("isoSPI Bus Traffic: ");
    isosnoop_print_buffer();
}

void isospi_interface_print_status(void) {
    printf("\n=== isoSPI Interface Status ===\n");
    printf("Initialized: %s\n", isospi_initialized ? "YES" : "NO");
    printf("Active Test Interface: %s\n", 
           active_interface == INTERFACE_ISOSPI ? "isoSPI" : "Batman");
    printf("Note: Both Batman and isoSPI master run simultaneously\n");
    
    if (isospi_initialized) {
        printf("TX Pins: GP%d (enable), GP%d (data)\n", 
               ISOSPI_TX_PIN_BASE, ISOSPI_TX_PIN_BASE + 1);
        printf("RX Pins: GP%d (high), GP%d (low)\n", 
               ISOSPI_RX_PIN_BASE, ISOSPI_RX_PIN_BASE + 1);
        printf("Sampling Pin: GP%d\n", ISOSPI_SAMPLING_PIN);
        printf("PIO: PIO0=CAN, PIO1=snooper(SM1), PIO2=master(SM0)\n");
    }
    printf("==============================\n\n");
}

