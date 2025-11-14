#include "isosnoop.h"
#include "isosnoop.pio.h"

#include "hardware/dma.h"
#include "hardware/pio.h"

#include <stdio.h>
#include <stdbool.h>

// BATMan Protocol Commands
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

// Use PIO1 SM1 (PIO0=CAN, PIO1=snooper, PIO2=isoSPI master)
#define ISOSNOOP_MASTER_PIO pio1
#define ISOSNOOP_MASTER_SM 1

#define PIO_IRQ_TO_USE 0
#define DMA_IRQ_TO_USE 0
#define DMA_IRQ_PRIORITY PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY
#define PIO_IRQ_PRIORITY PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY
static uint dma_channel_rx;
dma_channel_hw_t *dma_chan;

char read_buffer[256] __attribute__((aligned(2048)));
uint32_t last_write_addr;

void isosnoop_dma_setup(PIO pio, uint sm);

void isosnoop_setup(uint rx_pin_base, int invert, uint sampling_pin) {
    uint offset = pio_add_program(ISOSNOOP_MASTER_PIO, &isosnoop_program);
    isosnoop_pio_setup(ISOSNOOP_MASTER_PIO, ISOSNOOP_MASTER_SM, offset, rx_pin_base, invert, sampling_pin);
    isosnoop_dma_setup(ISOSNOOP_MASTER_PIO, ISOSNOOP_MASTER_SM);

    dma_chan = dma_channel_hw_addr(dma_channel_rx);
    last_write_addr = dma_chan->write_addr;
    
    printf("isoSPI Snooper: Initialized on PIO1 SM1 (RX: GP%d-GP%d, invert=%d, sampling: GP%d)\n", 
           rx_pin_base, rx_pin_base + 1, invert, sampling_pin);
}

void isosnoop_dma_setup(PIO pio, uint sm) {
    dma_channel_rx = dma_claim_unused_channel(false);
    if (dma_channel_rx < 0) {
        panic("No free dma channels");
    }

    for(int i=0; i<sizeof(read_buffer); i++) {
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
    dma_channel_configure(dma_channel_rx, &config_rx, read_buffer, (io_rw_8*)&pio->rxf[sm] + 0, dma_encode_endless_transfer_count(), true); // dma started
}

void isosnoop_print_buffer() {
    uint32_t write_addr = dma_chan->write_addr;
    uint32_t temp_addr = last_write_addr;
    
    // CL: Don't print if there's no new data
    if (temp_addr == write_addr) {
        return;
    }
    
    // First pass: print symbols
    while(temp_addr != write_addr) {
        uint8_t b = *((uint8_t *)temp_addr);
        temp_addr = (temp_addr + 1) & ~0x100; // wrap within buffer
        for(int i=0;i<2;i++) {
            uint8_t chunk = b & 0xf0;
            b <<= 4;
            
            switch(chunk>>4) {
            case 0xa:
                printf("CS1 ");
                break;
            case 0x5:
                printf("CS0 ");
                break;
            case 0x9:
                printf("1 ");
                break;
            case 0x6:
                printf("0 ");
                break;
            case 0x0:
                printf("_ ");
                break;
            default:
                printf("? ");
                break;
            }
        }
    }
    printf("\n");
    
    // Second pass: decode hex
    temp_addr = last_write_addr;
    printf("isoSPI HEX: ");
    uint8_t bit_accumulator = 0;
    int bit_count = 0;
    bool in_frame = false;
    
    while(temp_addr != write_addr) {
        uint8_t b = *((uint8_t *)temp_addr);
        temp_addr = (temp_addr + 1) & ~0x100;
        for(int i=0;i<2;i++) {
            uint8_t chunk = b & 0xf0;
            b <<= 4;
            
            uint8_t symbol = chunk >> 4;
            
            // CS marker detection (either 0xa or 0x5)
            if(symbol == 0xa || symbol == 0x5) {
                if(bit_count > 0) {
                    // Flush any partial nibble
                    if(bit_count < 4) {
                        bit_accumulator <<= (4 - bit_count);
                    }
                    printf("%X", bit_accumulator);
                    bit_count = 0;
                    bit_accumulator = 0;
                }
                if(in_frame) {
                    printf(" | ");
                }
                in_frame = true;
                continue;
            }
            
            // Only process data bits when in a frame
            if(in_frame) {
                if(symbol == 0x9) { // '1'
                    bit_accumulator = (bit_accumulator << 1) | 1;
                    bit_count++;
                } else if(symbol == 0x6) { // '0'
                    bit_accumulator = (bit_accumulator << 1) | 0;
                    bit_count++;
                }
                
                // When we have 4 bits, print the hex nibble
                if(bit_count == 4) {
                    printf("%X", bit_accumulator);
                    bit_accumulator = 0;
                    bit_count = 0;
                }
            }
        }
    }
    
    // Flush any remaining bits
    if(bit_count > 0) {
        bit_accumulator <<= (4 - bit_count);
        printf("%X", bit_accumulator);
    }
    printf("\n");
    
    // Third pass: decode Batman protocol with MOSI/MISO separation
    temp_addr = last_write_addr;
    printf("BATMan: ");
    
    uint8_t mosi_buffer[128];  // Master out (even bits)
    uint8_t miso_buffer[128];  // Slave in (odd bits)
    int mosi_idx = 0;
    int miso_idx = 0;
    uint8_t mosi_accumulator = 0;
    uint8_t miso_accumulator = 0;
    int mosi_bit_count = 0;
    int miso_bit_count = 0;
    in_frame = false;
    int frame_count = 0;
    bool is_mosi = true;  // Alternate between MOSI and MISO
    int bit_debug_count = 0;  // CL: Debug counter
    
    // Collect separated MOSI and MISO bytes
    while(temp_addr != write_addr && mosi_idx < 128 && miso_idx < 128) {
        uint8_t b = *((uint8_t *)temp_addr);
        temp_addr = (temp_addr + 1) & ~0x100;
        for(int i=0;i<2;i++) {
            uint8_t chunk = b & 0xf0;
            b <<= 4;
            uint8_t symbol = chunk >> 4;
            
            if(symbol == 0xa || symbol == 0x5) {
                // Flush any partial bytes
                if(mosi_bit_count > 0) {
                    if(mosi_bit_count < 8) {
                        mosi_accumulator <<= (8 - mosi_bit_count);
                    }
                    mosi_buffer[mosi_idx++] = mosi_accumulator;
                    mosi_bit_count = 0;
                    mosi_accumulator = 0;
                }
                if(miso_bit_count > 0) {
                    if(miso_bit_count < 8) {
                        miso_accumulator <<= (8 - miso_bit_count);
                    }
                    miso_buffer[miso_idx++] = miso_accumulator;
                    miso_bit_count = 0;
                    miso_accumulator = 0;
                }
                in_frame = true;
                frame_count++;
                is_mosi = true;  // First real data bit (after _ idle) is MOSI
                continue;
            }
            
            if(in_frame) {
                // Pattern: _ 0 _ 0 _ 1 _ 0 ...
                // Where _ = MISO (idle), next bit = MOSI, next _ = MISO (idle), next bit = MOSI
                // After the last underscore, bits alternate: MOSI, MISO, MOSI, MISO
                
                if(symbol == 0x0) {
                    // Underscore - this position is MISO idle
                    // Next symbol will be MOSI
                    is_mosi = true;
                    continue;  // Skip the underscore, don't add to buffers
                }
                
                uint8_t bit_val = 0;
                if(symbol == 0x9) {
                    bit_val = 1;
                } else if(symbol == 0x6) {
                    bit_val = 0;
                } else {
                    // Unknown symbol, skip
                    continue;
                }
                
                // Add bit to the appropriate buffer
                // Note: Accumulate bits MSB-first (bit 7 arrives first)
                if(is_mosi) {
                    mosi_accumulator = (mosi_accumulator << 1) | bit_val;
                    mosi_bit_count++;
                    if(mosi_bit_count == 8) {
                        mosi_buffer[mosi_idx++] = mosi_accumulator;
                        mosi_accumulator = 0;
                        mosi_bit_count = 0;
                    }
                } else {
                    miso_accumulator = (miso_accumulator << 1) | bit_val;
                    miso_bit_count++;
                    if(miso_bit_count == 8) {
                        miso_buffer[miso_idx++] = miso_accumulator;
                        miso_accumulator = 0;
                        miso_bit_count = 0;
                    }
                }
                
                // Toggle for next bit
                is_mosi = !is_mosi;
            }
        }
    }
    
    // Show separated MOSI and MISO hex data
    printf("\nMOSI[%d]: ", mosi_idx);
    for(int i = 0; i < mosi_idx && i < 30; i++) {
        printf("%02X ", mosi_buffer[i]);
    }
    printf("\nMISO[%d]: ", miso_idx);
    for(int i = 0; i < miso_idx && i < 30; i++) {
        printf("%02X ", miso_buffer[i]);
    }
    printf("\n");
    
    // Now decode the commands from MOSI
    int pos = 0;
    while(pos < mosi_idx) {
        if(pos + 1 < mosi_idx) {
            uint16_t cmd = (mosi_buffer[pos] << 8) | mosi_buffer[pos + 1];
            
            // Check for 16-bit commands
            if(cmd == CMD_WAKEUP) {
                printf("[WAKEUP] ");
                pos += 2;
            } else if(cmd == CMD_IDLE_WAKE) {
                printf("[UNMUTE] ");
                pos += 2;
            } else if((cmd & 0xFFF0) == (CMD_SNAPSHOT & 0xFFF0)) {
                printf("[SNAPSHOT] ");
                pos += 2;
            } else if(mosi_buffer[pos] == CMD_READ_A && mosi_buffer[pos + 1] == 0x00) {
                // Read command: 0x47 0x00
                printf("[READ_A cells 0-2] ");
                pos += 2; // Skip cmd bytes (0x47 0x00)
                
                // Skip CRC (0x70 0x01 or similar)
                if(pos + 1 < mosi_idx) {
                    pos += 2;
                }
                
                // Skip padding bytes sent while waiting for response
                if(pos + 1 < mosi_idx) {
                    pos += 2;
                }
                
                // Decode cell voltages from MISO buffer
                // Cell data starts at MISO[1] - Cell0 is at bytes [1-2]
                printf("BMB0: ");
                for(int cell = 0; cell < 3; cell++) {
                    int idx = 1 + (cell * 2);  // Cell data starts at MISO[1] (ODD offset!)
                    if(idx + 1 < miso_idx) {
                        // Use LOW,HIGH byte order to match batman.c
                        uint16_t raw = (miso_buffer[idx + 1] << 8) | miso_buffer[idx];
                        if(raw > 1000 && raw != 0xFFFF && raw != 0x7FFF) {
                            // Match batman.c: convert to integer mV first, then to float V
                            uint16_t voltage_mv = raw / 12.5;
                            float voltage = voltage_mv / 1000.0f;
                            printf("C%d=0x%04X(%.3fV) ", cell, raw, voltage);
                        }
                    }
                }
            } else if(mosi_buffer[pos] == CMD_READ_B && mosi_buffer[pos + 1] == 0x00) {
                printf("[READ_B cells 3-5] ");
                pos += 4; // Skip cmd + CRC
                if(pos + 1 < miso_idx) {
                    pos += 2; // Skip padding
                }
                if(pos + 5 < mosi_idx) {
                    printf("BMB0: ");
                    for(int cell = 0; cell < 3 && pos + 1 < mosi_idx; cell++) {
                        uint16_t raw = (mosi_buffer[pos] << 8) | mosi_buffer[pos + 1];
                        if(raw != 0xFFFF && raw != 0x0000 && raw != 0x7F7F && raw > 0x1000) {
                            float voltage = raw / 12.5f / 1000.0f;
                            printf("C%d=%.3fV ", cell + 3, voltage);
                        }
                        pos += 2;
                    }
                }
            } else {
                // Unknown, just skip
                pos++;
            }
        } else {
            pos++;
        }
    }
    printf("\n");
    
    last_write_addr = write_addr;
}
