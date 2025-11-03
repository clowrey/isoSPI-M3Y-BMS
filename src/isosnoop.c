#include "isosnoop.h"
#include "isosnoop.pio.h"

#include "hardware/dma.h"
#include "hardware/pio.h"

#include <stdio.h>

#define ISOSNOOP_MASTER_PIO pio1
#define ISOSNOOP_MASTER_SM 0

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
    
    printf("[Snooper initialized - GP%d/GP%d, invert=%d]\n", rx_pin_base, rx_pin_base + 1, invert);
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
    while(last_write_addr != write_addr) {
        uint8_t b = *((uint8_t *)last_write_addr);
        last_write_addr = (last_write_addr + 1) & ~0x100; // wrap within buffer
        for(int i=0;i<2;i++) {
            uint8_t chunk = b & 0xf0;
            b <<= 4;
            //printf("%x ", chunk); 
            //continue;
            
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
}
