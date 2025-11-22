/**
 * @file isospi_master.c
 * @brief isoSPI Master Interface using PIO
 * 
 * PIO-based isoSPI master for differential Manchester-like signaling.
 * Based on experimental version with multi-SM architecture and advanced CS control.
 */

#include "isospi_master.h"
#include "isospi_master.pio.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Use PIO2 (PIO0=CAN, PIO1=isosnoop, PIO2=isoSPI master)
#define ISOSPI_MASTER_PIO pio2
#define ISOSPI_MASTER_SM 0

void isospi_master_setup(uint tx_pin_base, uint rx_pin_base) {
    // tx_pin_base      is the driver enable pin (active high)
    // tx_pin_base + 1  is the tx data pin

    // rx_pin_base      is the high rx data pin
    // rx_pin_base + 1  is the low rx data pin

    printf("isoSPI Master: Initializing multi-SM architecture...\n");
    isospi_master_program_init(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, tx_pin_base, rx_pin_base);
    printf("isoSPI Master: Initialized on PIO2 (4 SMs) (TX: GP%d-GP%d, RX: GP%d-GP%d, sampling debug: GP13)\n", 
           tx_pin_base, tx_pin_base + 1, rx_pin_base, rx_pin_base + 1);
}

void isospi_master_flush() {
    // flush any remaining data in the PIO RX FIFO
    while(!pio_sm_is_rx_fifo_empty(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM)) {
        pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);
    }

    // empty the ISR
    pio_sm_exec(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, pio_encode_mov(pio_isr, pio_null));
}

void isospi_tune(
    uint32_t prescaler,
    uint8_t cs_pulse_length,
    uint8_t data_pulse_length,
    uint8_t pre_rx_delay,
    uint8_t reply_wait,
    uint8_t sample_pos_1,
    uint8_t sample_pos_2,
    uint8_t post_rx_delay
) {
    pio_sm_set_clkdiv_int_frac8(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, 1 + (prescaler>>8), prescaler & 0xff);

#define SET_DELAY(offset, value) \
    ISOSPI_MASTER_PIO->instr_mem[offset] = \
        (isospi_master_program_instructions[offset] & 0xe0ff) | ((value & 0x1f) << 8);

    // SET_DELAY(isospi_master_offset_cs_high_1, cs_pulse_length - 1);
    // SET_DELAY(isospi_master_offset_cs_high_2, cs_pulse_length - 1);
    // SET_DELAY(isospi_master_offset_cs_low_1, cs_pulse_length - 1);
    // SET_DELAY(isospi_master_offset_cs_low_2, cs_pulse_length - 1);

    SET_DELAY(isospi_master_offset_data_high_1, data_pulse_length - 1);
    SET_DELAY(isospi_master_offset_data_high_2, data_pulse_length - 1);
    SET_DELAY(isospi_master_offset_data_low_1, data_pulse_length - 1);
    SET_DELAY(isospi_master_offset_data_low_2, data_pulse_length - 1);

    SET_DELAY(isospi_master_offset_pre_rx, pre_rx_delay - 1);
    //ISOSPI_MASTER_PIO->instr_mem[isospi_master_offset_set_reply_wait] = 
    //    (isospi_master_program_instructions[isospi_master_offset_set_reply_wait] & 0xffe0) | ((reply_wait & 0x1f));
    //SET_DELAY(isospi_master_offset_sample_1, sample_pos_1 - 3);
    //SET_DELAY(isospi_master_offset_sample_2, sample_pos_2 - sample_pos_1 - 1);
    //SET_DELAY(isospi_master_offset_post_rx, post_rx_delay - 1);
}

// Reverse the bits within a nibble using a lookup table
uint8_t reverse_nibble_lut(uint8_t n) {
    // Only 16 possible values (0-15)
    // Index 0 (0000) -> 0 (0000)
    // Index 1 (0001) -> 8 (1000)
    // ...
    // Index 11 (1011) -> 13 (1101)
    static const uint8_t lookup[16] = {
        0x0, 0x8, 0x4, 0xC, 
        0x2, 0xA, 0x6, 0xE, 
        0x1, 0x9, 0x5, 0xD, 
        0x3, 0xB, 0x7, 0xF
    };
    
    // Mask input with 0x0F to ensure we only look at the lower nibble
    return lookup[n & 0x0F];
}

bool isospi_write_read_blocking(unsigned char* out_buf, unsigned char* in_buf, size_t len) {

    isospi_master_cs(false);  // Normal data CS pattern (CS0 CS1) - NOT wakeup pattern!

    sleep_us(3);  // CL: CS front porch - Batman IC needs ~5µs after CS before data

    bool valid = true;
    int carry_bit = 0;

    for(size_t i=0; i<len; i++) {
        // We write 8 bits at a time
        pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, out_buf[i] << 24);

        // Each response bit is encoded as a nibble in a 32 bit word
        uint32_t v = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);
        
        // CL: Process nibbles MSB-first (matching snooper's byte-endian approach)
        for(int r=0; r<8; r++) {
            uint8_t nibble = (v >> 28) & 0xf;
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

        // CL: Implement 1-bit carry to align data correctly (off-by-one fix)
        int new_carry = in_buf[i] & 0x1;
        in_buf[i] = (in_buf[i] >> 1) | (carry_bit << 7);
        carry_bit = new_carry;

        // CL: Debug - print decoded byte for first few bytes
        if(i < 24) {
            printf("  decoded: 0x%02X\n", in_buf[i]);
        }

        sleep_us(1);
    }

    // perform final ending chip select immediately after data
    isospi_master_cs(true);  // CL: Ending CS uses CS1 CS0 to close the frame

    // flush any remaining data
    isospi_master_flush();

    return valid;
}

bool isospi_write_single_test() {
    // snapshot command (final bit will return something)
    char tx[] = {0x2b, 0xfb};
    char rx[2] = {0};

    const uint8_t cs_front_porch = 150; // wait after asserting CS
    pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, cs_front_porch << 24);

    sleep_us(5);

    // Write first byte
    pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, tx[0] << 24);

    // First reply should be zeros (no reply)
    uint32_t reply1 = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);

    sleep_us(1);

    pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, tx[0] << 24);

    // Second reply should be nonzero
    uint32_t reply2 = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);

    sleep_us(5);

    // perform final ending chip select
    //pio_sm_exec(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, pio_encode_jmp(isospi_master_offset_ending_chip_select)); 

    // flush any remaining data
    isospi_master_flush();
    
    return (reply1 == 0) && (reply2 != 0);
}

int isospi_write_tests(int count) {
    int score = 0;
    for(int i=0; i<count; i++) {
        if(isospi_write_single_test()) {
            score++;
        }
    }
    return score;
}

#define PRESCALER_MIN 75
#define PRESCALER_MAX 125
#define CS_PULSE_LENGTH_MIN 20
#define CS_PULSE_LENGTH_MAX 32
#define DATA_PULSE_LENGTH_MIN 5
#define DATA_PULSE_LENGTH_MAX 15
#define PRE_RX_DELAY_MIN 1
#define PRE_RX_DELAY_MAX 32
#define REPLY_WAIT_MIN 1
#define REPLY_WAIT_MAX 32
#define POST_RX_DELAY_MIN 1
#define POST_RX_DELAY_MAX 32




void isospi_calibrate() {
    int tests = 20;
    int pass_threshold = 10;

    int prescaler = 120;
    int cs_pulse_length = 30;
    int data_pulse_length = 10;
    int pre_rx_delay = 16;
    int reply_wait = 30;
    int sample_pos_1 = 6;
    int sample_pos_2 = 16;
    int post_rx_delay = 32;

    int prescaler_min=PRESCALER_MIN;
    int prescaler_max=PRESCALER_MAX;
    int cs_pulse_length_min=CS_PULSE_LENGTH_MIN;
    int cs_pulse_length_max=CS_PULSE_LENGTH_MAX;
    int data_pulse_length_min=DATA_PULSE_LENGTH_MIN;
    int data_pulse_length_max=DATA_PULSE_LENGTH_MAX;
    int pre_rx_delay_min=PRE_RX_DELAY_MIN;
    int pre_rx_delay_max=PRE_RX_DELAY_MAX;
    int reply_wait_min=REPLY_WAIT_MIN;
    int reply_wait_max=REPLY_WAIT_MAX;
    int post_rx_delay_min=POST_RX_DELAY_MIN;
    int post_rx_delay_max=POST_RX_DELAY_MAX;

    void _calibrate_value(int *value, int *min, int *max) {
        int cur = *value;
        for(*value=cur+1; *value<=*max; (*value)++) {
            isospi_tune(
                prescaler,
                cs_pulse_length,
                data_pulse_length,
                pre_rx_delay,
                reply_wait,
                sample_pos_1,
                sample_pos_2,
                post_rx_delay
            );
            int score = isospi_write_tests(tests);
            if(score < pass_threshold) {
                *max = *min < *value ? *value - 1 : *min;
                break;
            }
        }
        for(*value=cur-1; *value>=*min; (*value)--) {
            isospi_tune(
                prescaler,
                cs_pulse_length,
                data_pulse_length,
                pre_rx_delay,
                reply_wait,
                sample_pos_1,
                sample_pos_2,
                post_rx_delay
            );
            int score = isospi_write_tests(tests);
            if(score < pass_threshold) {
                *min = *max > *value ? *value + 1 : *max;
                break;
            }
        }
        *value = ( *min + *max ) / 2;
    }

    for(int round=0; round<10; round++) {
        printf("Calibration round %d\n", round+1);
        printf("  current range: prescaler %d - %d (%d)\n", prescaler_min, prescaler_max, prescaler);
        _calibrate_value(&prescaler, &prescaler_min, &prescaler_max);
        printf("    new range: prescaler %d - %d (%d)\n", prescaler_min, prescaler_max, prescaler);
        printf("  current range: cs_pulse_length %d - %d (%d)\n", cs_pulse_length_min, cs_pulse_length_max, cs_pulse_length);
        _calibrate_value(&cs_pulse_length, &cs_pulse_length_min, &cs_pulse_length_max);
        printf("    new range: cs_pulse_length %d - %d (%d)\n", cs_pulse_length_min, cs_pulse_length_max, cs_pulse_length);
        printf("  current range: data_pulse_length %d - %d (%d)\n", data_pulse_length_min, data_pulse_length_max, data_pulse_length);
        _calibrate_value(&data_pulse_length, &data_pulse_length_min, &data_pulse_length_max);
        printf("    new range: data_pulse_length %d - %d (%d)\n", data_pulse_length_min, data_pulse_length_max, data_pulse_length);
        printf("  current range: pre_rx_delay %d - %d (%d)\n", pre_rx_delay_min, pre_rx_delay_max, pre_rx_delay);
        _calibrate_value(&pre_rx_delay, &pre_rx_delay_min, &pre_rx_delay_max);
        printf("    new range: pre_rx_delay %d - %d (%d)\n", pre_rx_delay_min, pre_rx_delay_max, pre_rx_delay);
        printf("  current range: reply_wait %d - %d (%d)\n", reply_wait_min, reply_wait_max, reply_wait);
        _calibrate_value(&reply_wait, &reply_wait_min, &reply_wait_max);
        printf("    new range: reply_wait %d - %d (%d)\n", reply_wait_min, reply_wait_max, reply_wait);
        printf("  current range: post_rx_delay %d - %d (%d)\n", post_rx_delay_min, post_rx_delay_max, post_rx_delay);
        _calibrate_value(&post_rx_delay, &post_rx_delay_min, &post_rx_delay_max);
        printf("    new range: post_rx_delay %d - %d (%d)\n", post_rx_delay_min, post_rx_delay_max, post_rx_delay);
    }

    printf("new range: prescaler %d - %d\n", prescaler_min, prescaler_max);
}

void isospi_wakeup(void) {
    // Send wakeup CS pulses (CS1 CS0 pattern) with WAKEUP command
    // This sends CS + actual command data, not just standalone CS
    char tx[2] = {0x2A, 0xD4};  // CMD_WAKEUP
    char rx[2] = {0};
    
    isospi_master_cs(true);  // Wakeup CS pattern (CS1 CS0) - only for initial wakeup!
    sleep_us(5);  // CS front porch
    
    // Send the wake command data
    for(int i = 0; i < 2; i++) {
        pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, tx[i] << 24);
        uint32_t v = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);
        // Decode response if needed
        for(int r = 0; r < 8; r++) {
            uint8_t nibble = (v >> 28) & 0xf;
            v <<= 4;
            if(nibble == 0b1001) {
                rx[i] = (rx[i] << 1) | 0x1;
            } else {
                rx[i] = (rx[i] << 1) | 0x0;
            }
        }
    }
    
    // Send closing CS immediately after data
    isospi_master_cs(true);  // CL: Ending CS uses CS1 CS0 to close the frame
    isospi_master_flush();
}

void isospi_send_command(uint16_t cmd_word) {
    // Send a 16-bit command with normal CS pattern
    unsigned char tx[2];
    unsigned char rx[2] = {0};
    
    // Split 16-bit command into 2 bytes (MSB first)
    tx[0] = (cmd_word >> 8) & 0xFF;
    tx[1] = cmd_word & 0xFF;
    
    isospi_write_read_blocking(tx, rx, 2);
}

void isospi_get_data(uint8_t reg_cmd, unsigned char* response_buffer, size_t response_len) {
    // Read data with command and CRC
    // Sends: [reg_cmd] [0x00] [0x70] [0x00] then reads response_len bytes continuously
    
    unsigned char tx[4 + response_len];
    unsigned char rx[4 + response_len];
    
    // Build command packet
    tx[0] = reg_cmd;
    tx[1] = 0x00;
    tx[2] = 0x70;  // CRC placeholder
    tx[3] = 0x00;
    
    // Fill rest with zeros (will be overwritten by response)
    for(size_t i = 4; i < 4 + response_len; i++) {
        tx[i] = 0x00;
    }
    
    // Send and receive
    isospi_write_read_blocking(tx, rx, 4 + response_len);
    
    // Copy response data (skip the first 4 command bytes)
    for(size_t i = 0; i < response_len; i++) {
        response_buffer[i] = rx[4 + i];
    }
}

