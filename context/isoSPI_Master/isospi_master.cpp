#include "isospi_master.pio.h"

#define ISOSPI_MASTER_PIO pio0
#define ISOSPI_MASTER_SM 0

void isospi_master_setup(uint tx_pin_base, uint rx_pin_base) {
    // tx_pin_base      is the driver enable pin (active high)
    // tx_pin_base + 1  is the tx data pin

    // rx_pin_base      is the high rx data pin
    // rx_pin_base + 1  is the low rx data pin

    uint offset = pio_add_program(ISOSPI_MASTER_PIO, &isospi_master_program);
    isospi_master_program_init(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, offset, tx_pin_base, rx_pin_base);
    pio_sm_set_enabled(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, true);
}

bool isospi_write_read_blocking(char* out_buf, char* in_buf, size_t len) {
    const uint8_t cs_front_porch = 150; // wait after asserting CS
    pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, cs_front_porch << 24);

    bool valid = true;
    for(size_t i=0; i<len; i++) {
        // We write 8 bits at a time
        pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, out_buf[i] << 24);

        // Each response bit is encoded as a nibble in a 32 bit word
        uint32_t v = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);
        for(int r=0; r<8; r++) {
            uint8_t nibble = (v >> 28) & 0xf0;
            v <<= 4;
            if(nibble==0b1001) {
                // bit 1
                in_buf[i] = (in_buf[i] << 1) | 0x1;
            } else if(nibble==0b0110) {
                // bit 0
                in_buf[i] = (in_buf[i] << 1) | 0x0;
            } else {
                // invalid
                valid = false;
                in_buf[i] = (in_buf[i] << 1) | 0x0;
            }
        }
    }

    // jump to third-from-last instruction
    pio_sm_exec(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, pio_encode_jmp(29)); 

    return valid;
}