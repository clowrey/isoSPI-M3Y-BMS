void isospi_master_setup(uint tx_pin_base, uint rx_pin_base);
bool isospi_write_read_blocking(char* out_buf, char* in_buf, size_t len);
