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

char read_buffer[256] __attribute__((aligned(2048)));

void isosnoop_dma_setup(PIO pio, uint sm);

void isosnoop_setup(uint rx_pin_base, uint sampling_pin) {
    uint offset = pio_add_program(ISOSNOOP_MASTER_PIO, &isosnoop_program);
    isosnoop_pio_setup(ISOSNOOP_MASTER_PIO, ISOSNOOP_MASTER_SM, offset, rx_pin_base, sampling_pin);
    isosnoop_dma_setup(ISOSNOOP_MASTER_PIO, ISOSNOOP_MASTER_SM);

    dma_channel_hw_t *dma_chan = dma_channel_hw_addr(dma_channel_rx);
    uint32_t last_write_addr = dma_chan->write_addr;
    uint32_t buf_end = (uint32_t)(read_buffer + sizeof(read_buffer));
}

void isosnoop_dma_setup(PIO pio, uint sm) {
    dma_channel_rx = dma_claim_unused_channel(false);
    if (dma_channel_rx < 0) {
        panic("No free dma channels");
    }

    // irq_add_shared_handler(pio_get_irq_num(pio, PIO_IRQ_TO_USE), pio_irq_handler, PIO_IRQ_PRIORITY);
    // pio_set_irqn_source_enabled(pio, PIO_IRQ_TO_USE, pio_get_rx_fifo_not_empty_interrupt_source(sm), true);
    // irq_set_enabled(pio_get_irq_num(pio, PIO_IRQ_TO_USE), true);

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
    dma_channel_hw_t *dma_chan = dma_channel_hw_addr(dma_channel_rx);
    static uint32_t last_write_addr = dma_chan->write_addr;
    uint32_t buf_end = (uint32_t)(read_buffer + sizeof(read_buffer));

    uint32_t write_addr = dma_chan->write_addr;
    while(last_write_addr != write_addr) {
        uint8_t b = *((uint8_t *)last_write_addr);
        last_write_addr = (last_write_addr + 1) & ~0x100; // wrap within buffer
        for(int i=0;i<2;i++) {
            uint8_t chunk = b & 0xf0;
            b <<= 4;
            if(chunk==0xa0) {
                printf("CS1 ");
            } else if(chunk==0x50) {
                printf("CS0 ");
            } else if(chunk==0x90) {
                printf("1 ");
            } else if(chunk==0x60) {
                printf("0 ");
            } else if(chunk==0x00) {
                printf("_ ");
            } else {
                printf("? ");
            }
        }
    }
    printf("\n");
}
