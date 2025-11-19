#include "isospi_scope.pio.h"

#include "hardware/clocks.h"

#include <stdio.h>

#define ISOSPI_SCOPE_PIO pio2
#define ISOSPI_SCOPE_SM 0

void isospi_scope_setup(uint rx_pin_base, int invert) {
    uint offset = pio_add_program(ISOSPI_SCOPE_PIO, &isospi_scope_program);
    isospi_scope_pio_setup(ISOSPI_SCOPE_PIO, ISOSPI_SCOPE_SM, offset, rx_pin_base, invert);
    //isosnoop_dma_setup(ISOSPI_SCOPE_PIO, ISOSPI_SCOPE_SM);

    //dma_chan = dma_channel_hw_addr(dma_channel_rx);
    //last_write_addr = dma_chan->write_addr;
    //uint32_t buf_end = (uint32_t)(read_buffer + sizeof(read_buffer));
}

void isospi_scope_flush() {
    // flush any remaining data in the PIO RX FIFO
    while(!pio_sm_is_rx_fifo_empty(ISOSPI_SCOPE_PIO, ISOSPI_SCOPE_SM)) {
        pio_sm_get_blocking(ISOSPI_SCOPE_PIO, ISOSPI_SCOPE_SM);
    }

    // empty the ISR
    pio_sm_exec(ISOSPI_SCOPE_PIO, ISOSPI_SCOPE_SM, pio_encode_mov(pio_isr, pio_null));
}

void print_isospi_scope_output() {
    char buf1[10*32];
    char buf2[10*32];
    memset(buf1, 0, sizeof(buf1));
    memset(buf2, 0, sizeof(buf2));

    int ptr = 0;


    while(!pio_sm_is_rx_fifo_empty(ISOSPI_SCOPE_PIO, ISOSPI_SCOPE_SM)) {
        uint32_t v = pio_sm_get_blocking(ISOSPI_SCOPE_PIO, ISOSPI_SCOPE_SM);
        for (int i=0; i<16; i++) {
            if((v & 0x80000000) != 0) {
                buf1[ptr] = '1';
                //printf("1");
            } else {
                buf1[ptr] = '0';
                //printf("0");
            }
            if((v & 0x40000000) != 0) {
                buf2[ptr] = '1';
                //printf("1");
            } else {
                //printf("0");
                buf2[ptr] = '0';
            }
            //printf(" ");
            v <<= 2;
            ptr++;
        }

        //printf("0x%08x ", v);
    }
    printf("%s\n", buf1);
    printf("%s\n", buf2);
}