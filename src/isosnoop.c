/**
 * @file isosnoop.c
 * @brief isoSPI Bus Snooper using PIO
 * 
 * Passive monitoring of isoSPI bus traffic using DMA ring buffer.
 * Adapted from C++ to C for RP2350, using PIO1 SM1.
 */

#include "isosnoop.h"
#include "isosnoop.pio.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <string.h>

// Use PIO1 SM1 (SM0 is used by isospi_master)
#define ISOSNOOP_MASTER_PIO pio1
#define ISOSNOOP_MASTER_SM 1

// isoSPI Command definitions (from batman.c)
#define CMD_WAKEUP      0x2AD4
#define CMD_MUTE        0x20DD
#define CMD_IDLE_WAKE   0x21F2
#define CMD_SNAPSHOT    0x2BFB
#define CMD_READ_A      0x47
#define CMD_READ_B      0x48
#define CMD_READ_C      0x49
#define CMD_READ_D      0x4A
#define CMD_READ_E      0x4B
#define CMD_READ_F      0x4C

static uint dma_channel_rx;
static char read_buffer[2048] __attribute__((aligned(2048)));  // Increased for continuous sampling
static uint rx_pin_base_saved = 0;
static uint8_t decoded_bytes[256];  // Increased to handle more decoded data
static uint16_t decoded_count = 0;

static void isosnoop_dma_setup(PIO pio, uint sm);
static void isosnoop_decode_manchester(uint8_t *samples, uint16_t sample_count);
static const char* isosnoop_identify_command(uint16_t cmd);

/**
 * @brief Detect 260ns start pulse (frame synchronization)
 * 
 * NOTE: This function is NO LONGER USED - the PIO now handles start pulse detection!
 * The PIO waits for the 260ns start pulse before capturing data.
 * 
 * The start pulse is 260ns wide, which is ~16 samples at our rate
 * Regular data pulses are 75ns (~4-5 samples)
 * 
 * Key characteristics of a real start pulse:
 * - Long run (10+ samples) of a VALID differential state (H0=0x01 or 1H=0x02)
 * - NOT 0x00 (both LOW - error/invalid)
 * - NOT 0x03 (both HIGH - idle)
 * - Followed by changing samples (actual Manchester data)
 * 
 * Returns: index of first sample after start pulse, or 0 if not found
 */
#if 0  // UNUSED - PIO handles triggering now
static uint16_t find_start_pulse(uint8_t *samples, uint16_t sample_count) {
    uint8_t last_sample = 0xFF;
    uint8_t run_length = 0;
    uint16_t run_start_byte = 0;
    uint16_t run_start_sample = 0;
    
    for (uint16_t byte_idx = 0; byte_idx < sample_count - 2; byte_idx++) {  // -2 to check what follows
        uint8_t sample_byte = samples[byte_idx];
        
        for (int sample_idx = 0; sample_idx < 4; sample_idx++) {
            uint8_t sample = (sample_byte >> (6 - sample_idx*2)) & 0x03;
            
            // Reset on idle or invalid samples
            if (sample == 0x03 || sample == 0x00) {
                run_length = 0;
                last_sample = 0xFF;
                continue;
            }
            
            // Only track valid differential states: H0 (0x01) or 1H (0x02)
            if (sample == 0x01 || sample == 0x02) {
                if (sample == last_sample) {
                    run_length++;
                    
                    // 260ns ≈ 16 samples, look for run of 10+ (with margin)
                    if (run_length >= 10) {
                        // Found a long pulse! Check what follows
                        uint16_t check_byte = byte_idx + 1;
                        if (check_byte < sample_count - 1) {
                            // Look at next few bytes for Manchester preamble pattern
                            // isoSPI typically has alternating states (55 AA pattern) after start
                            uint8_t next1 = samples[check_byte];
                            uint8_t next2 = samples[check_byte + 1];
                            
                            // Count valid differential samples (not idle/invalid)
                            int valid_count = 0;
                            for (int i = 0; i < 4; i++) {
                                uint8_t s = (next1 >> (6 - i*2)) & 0x03;
                                if (s == 0x01 || s == 0x02) valid_count++;
                            }
                            for (int i = 0; i < 4; i++) {
                                uint8_t s = (next2 >> (6 - i*2)) & 0x03;
                                if (s == 0x01 || s == 0x02) valid_count++;
                            }
                            
                            // If we have at least 4 valid samples following, it's likely real data
                            if (valid_count >= 4) {
                                uint16_t pos = byte_idx * 4 + sample_idx + 1;
                                printf("[START PULSE: %s×%u at pos %u, %d valid samples follow]\n", 
                                       sample == 0x01 ? "H0" : "1H", run_length, pos, valid_count);
                                return pos / 4;  // Return byte index
                            }
                        }
                        // Continue looking if not followed by enough data
                    }
                } else {
                    // New run starting
                    run_length = 1;
                    last_sample = sample;
                    run_start_byte = byte_idx;
                    run_start_sample = sample_idx;
                }
            } else {
                // Invalid sample
                run_length = 0;
                last_sample = 0xFF;
            }
        }
    }
    
    return 0;  // No start pulse found
}
#endif  // End of unused find_start_pulse function

/**
 * @brief Decode Manchester-encoded samples to bytes
 * 
 * isoSPI uses differential Manchester encoding:
 * - Start pulse: 260ns (16+ samples) for frame sync
 * - Data pulses: 75ns (4-5 samples)
 * - H0 (01) = one differential state
 * - 1H (10) = opposite differential state
 */
static void isosnoop_decode_manchester(uint8_t *samples, uint16_t sample_count) {
    decoded_count = 0;
    
    // PIO has already triggered on the 260ns start pulse
    // Data is synchronized to the beginning of the frame
    printf("[PIO-triggered capture - data starts after 260ns pulse]\n");
    
    uint8_t current_byte = 0;
    uint8_t bit_count = 0;
    
    // Extract all 2-bit samples into a continuous stream
    for (uint16_t byte_idx = 0; byte_idx < sample_count; byte_idx++) {
        uint8_t sample_byte = samples[byte_idx];
        
        // Each byte has 4 samples (2 bits each)
        for (int sample_idx = 0; sample_idx < 4; sample_idx++) {
            uint8_t sample = (sample_byte >> (6 - sample_idx*2)) & 0x03;
            
            // Skip idle samples (both HIGH = 0b11)
            if (sample == 0x03) {
                continue;
            }
            
            // Decode pairs of differential samples
            // Collect every 2 samples (which form a Manchester bit)
            if (bit_count % 2 == 0) {
                // First half of Manchester bit - just store it
                current_byte = (current_byte << 1);
            } else {
                // Second half - compare to first half
                // Same sample repeated = 0, different = 1
                uint8_t prev_sample = (current_byte >> 7) & 0x01;
                if (sample != prev_sample) {
                    current_byte |= 1;  // Transition = 1
                }
                // else no change = 0
                
                // Check if we've completed a byte
                if ((bit_count / 2) % 8 == 7) {
                    if (decoded_count < sizeof(decoded_bytes)) {
                        decoded_bytes[decoded_count++] = current_byte;
                    }
                    current_byte = 0;
                }
            }
            
            bit_count++;
        }
    }
    
    // Store partial byte if any
    if (bit_count > 0 && (bit_count / 2) % 8 != 0) {
        if (decoded_count < sizeof(decoded_bytes)) {
            // Shift remaining bits to MSB
            uint8_t remaining_bits = (bit_count / 2) % 8;
            current_byte <<= (8 - remaining_bits);
            decoded_bytes[decoded_count++] = current_byte;
        }
    }
}

/**
 * @brief Identify isoSPI command from decoded bytes
 */
static const char* isosnoop_identify_command(uint16_t cmd) {
    switch (cmd) {
        case CMD_WAKEUP:    return "WAKEUP";
        case CMD_MUTE:      return "MUTE";
        case CMD_IDLE_WAKE: return "IDLE_WAKE/UNMUTE";
        case CMD_SNAPSHOT:  return "SNAPSHOT";
        default:
            if ((cmd >> 8) == CMD_READ_A) return "READ_A";
            if ((cmd >> 8) == CMD_READ_B) return "READ_B";
            if ((cmd >> 8) == CMD_READ_C) return "READ_C";
            if ((cmd >> 8) == CMD_READ_D) return "READ_D";
            if ((cmd >> 8) == CMD_READ_E) return "READ_E";
            if ((cmd >> 8) == CMD_READ_F) return "READ_F";
            return "UNKNOWN";
    }
}

void isosnoop_setup(uint rx_pin_base, uint sampling_pin) {
    rx_pin_base_saved = rx_pin_base;
    
    uint offset = pio_add_program(ISOSNOOP_MASTER_PIO, &isosnoop_program);
    isosnoop_pio_setup(ISOSNOOP_MASTER_PIO, ISOSNOOP_MASTER_SM, offset, rx_pin_base, sampling_pin);
    isosnoop_dma_setup(ISOSNOOP_MASTER_PIO, ISOSNOOP_MASTER_SM);

    dma_channel_hw_t *dma_chan = dma_channel_hw_addr(dma_channel_rx);
    uint32_t last_write_addr = dma_chan->write_addr;
    uint32_t buf_end = (uint32_t)(read_buffer + sizeof(read_buffer));
    
    printf("isoSPI Snooper: Initialized on PIO1 SM1\n");
    printf("  RX Pins: GP%d (high), GP%d (low)\n", rx_pin_base, rx_pin_base + 1);
    printf("  Debug Pin: GP%d (sampling)\n", sampling_pin);
    printf("  DMA Channel: %d\n", dma_channel_rx);
    printf("  Buffer: 0x%08lx - 0x%08lx (%d bytes)\n", 
           (uint32_t)read_buffer, buf_end, sizeof(read_buffer));
    printf("  DMA Write Addr: 0x%08lx\n", last_write_addr);
}

void isosnoop_get_stats(uint32_t *buffer_addr, uint32_t *dma_addr, bool *pio_running) {
    *buffer_addr = (uint32_t)read_buffer;
    dma_channel_hw_t *dma_chan = dma_channel_hw_addr(dma_channel_rx);
    *dma_addr = dma_chan->write_addr;
    // Check if SM is enabled by reading control register
    *pio_running = (ISOSNOOP_MASTER_PIO->ctrl & (1u << ISOSNOOP_MASTER_SM)) != 0;
}

static void isosnoop_dma_setup(PIO pio, uint sm) {
    dma_channel_rx = dma_claim_unused_channel(false);
    if (dma_channel_rx < 0) {
        panic("No free dma channels");
    }

    // Initialize buffer with pattern for debugging
    for(int i = 0; i < sizeof(read_buffer); i++) {
        read_buffer[i] = i;
    }

    dma_channel_config config_rx = dma_channel_get_default_config(dma_channel_rx);
    channel_config_set_transfer_data_size(&config_rx, DMA_SIZE_8);
    channel_config_set_read_increment(&config_rx, false);
    channel_config_set_write_increment(&config_rx, true);

    channel_config_set_dreq(&config_rx, pio_get_dreq(pio, sm, false));

    // enable dma in ring buffer mode with 8 bit size
    channel_config_set_ring(&config_rx, true, 8); // 2**8 = 256 bytes
    // configure dma to read from pio fifo
    dma_channel_configure(dma_channel_rx, &config_rx, read_buffer, 
                         (io_rw_8*)&pio->rxf[sm] + 0, 
                         dma_encode_endless_transfer_count(), true); // dma started
}

void isosnoop_print_buffer(void) {
    dma_channel_hw_t *dma_chan = dma_channel_hw_addr(dma_channel_rx);
    static uint32_t last_write_addr = 0;
    static uint32_t print_count = 0;
    
    // Initialize on first call
    if (last_write_addr == 0) {
        last_write_addr = dma_chan->write_addr;
        printf("[Snooper initialized at 0x%08lx]\n", last_write_addr);
    }
    
    uint32_t write_addr = dma_chan->write_addr;
    uint32_t bytes_captured = 0;
    bool has_data = false;
    uint32_t max_bytes_to_show = 256;  // Show up to 256 bytes of samples (increased to catch full bursts)
    
    while(last_write_addr != write_addr && bytes_captured < max_bytes_to_show) {
        has_data = true;
        uint8_t b = *((uint8_t *)last_write_addr);
        bytes_captured++;
        
        // Print hex byte and decoded samples
        if (bytes_captured == 1 || (bytes_captured % 8) == 0) {
            printf("\n[%04lu:%03u] ", print_count, bytes_captured);
        }
        
        printf("%02X:", b);
        
        // Wrap within 256-byte ring buffer
        last_write_addr++;
        if (last_write_addr >= (uint32_t)(read_buffer + sizeof(read_buffer))) {
            last_write_addr = (uint32_t)read_buffer;
        }
        
        // Decode 4 samples (2 bits each) in this byte
        for(int i = 0; i < 4; i++) {
            uint8_t sample = (b >> (6 - i*2)) & 0x03;
            // sample: bit1=GP10, bit0=GP9
            if (sample == 0b11) {
                printf("__");  // Idle (both HIGH)
            } else if (sample == 0b10) {
                printf("1H");  // GP9 HIGH, GP10 LOW
            } else if (sample == 0b01) {
                printf("H0");  // GP9 LOW, GP10 HIGH  
            } else {
                printf("00");  // Both LOW
            }
            if (i < 3) printf(" ");
        }
        printf("  ");
    }
    
    if (has_data) {
        printf("\n[Captured %lu bytes of raw samples]\n", bytes_captured);
        
        // Show raw hex dump for analysis - read from where we just read the data!
        printf("\nRaw hex: ");
        uint32_t read_addr = write_addr - bytes_captured;
        // Handle ring buffer wrap
        if (read_addr < (uint32_t)read_buffer) {
            read_addr += sizeof(read_buffer);
        }
        
        // Copy captured bytes to temp buffer for decoding
        uint8_t temp_buffer[256];
        uint32_t temp_addr = read_addr;
        for (uint32_t i = 0; i < bytes_captured && i < 256; i++) {
            temp_buffer[i] = *((uint8_t*)temp_addr);
            if (i % 16 == 0 && i > 0) printf("\n         ");
            printf("%02X ", temp_buffer[i]);
            temp_addr++;
            if (temp_addr >= (uint32_t)(read_buffer + sizeof(read_buffer))) {
                temp_addr = (uint32_t)read_buffer;
            }
        }
        printf("\n");
        
        // Decode Manchester encoding from temp buffer
        isosnoop_decode_manchester(temp_buffer, bytes_captured);
        
        if (decoded_count > 0) {
            printf("\n=== DECODED isoSPI DATA ===\n");
            printf("Decoded %d bytes from Manchester encoding:\n", decoded_count);
            
            // Display decoded bytes
            for (uint16_t i = 0; i < decoded_count; i++) {
                if (i % 16 == 0) {
                    printf("\n%04X: ", i);
                }
                printf("%02X ", decoded_bytes[i]);
            }
            printf("\n");
            
            // Try to identify commands
            if (decoded_count >= 2) {
                printf("\n=== COMMAND IDENTIFICATION ===\n");
                for (uint16_t i = 0; i < decoded_count - 1; i++) {
                    uint16_t word = (decoded_bytes[i] << 8) | decoded_bytes[i+1];
                    const char* cmd_name = isosnoop_identify_command(word);
                    
                    if (strcmp(cmd_name, "UNKNOWN") != 0) {
                        printf("Offset %04X: 0x%04X = %s\n", i, word, cmd_name);
                    }
                }
            }
            printf("===========================\n\n");
        }
        
        print_count++;
    } else {
        // Periodically show that snooper is alive but no data
        static uint32_t no_data_count = 0;
        if (++no_data_count % 10 == 0) {
            printf("[Snooper active, no data - DMA addr: 0x%08lx]\n", write_addr);
        }
    }
}

uint8_t isosnoop_read_pins(void) {
    if (rx_pin_base_saved == 0) {
        return 0xFF; // Not initialized
    }
    
    // Read GP9 and GP10 directly
    uint8_t pin_high = gpio_get(rx_pin_base_saved);
    uint8_t pin_low = gpio_get(rx_pin_base_saved + 1);
    
    return (pin_high << 1) | pin_low;
}


