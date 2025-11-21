void isospi_master_setup(uint tx_pin_base, uint rx_pin_base);
bool isospi_write_read_blocking(char* out_buf, char* in_buf, size_t len);
void isospi_invert_first_chip_select(bool invert);
void isospi_tune(
    uint32_t prescaler,
    uint8_t cs_pulse_length,
    uint8_t data_pulse_length,
    uint8_t pre_rx_delay,
    uint8_t reply_wait,
    uint8_t sample_pos_1,
    uint8_t sample_pos_2,
    uint8_t post_rx_delay
);
int isospi_write_tests(int count);
void isospi_calibrate();
